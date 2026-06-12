#!/usr/bin/env python3
"""Print a compact M7 release health summary from committed sidecars."""

from __future__ import annotations

import argparse
import json
import math
import re
import subprocess
import sys
import tempfile
from collections import Counter
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Callable

from assert_m7_release_signoff_ready import (
    ACCEPTED_READINESS_SIDECAR,
    read_json,
    repo_root,
)
from audit_m7_sidecar_inventory import (
    M7_ROOT,
    InventoryEntry,
    InventoryError,
    build_inventory,
    verify_inventory,
    verify_schema_reconciliation,
)
from validate_m7_release_sidecar_schemas import SchemaError


WITHHELD_CLAIMS: tuple[str, ...] = (
    "This summary reads committed sidecars only; it does not create new release evidence.",
    "This summary does not rerun AMFlow numerics or widen release claims.",
    "This summary reports documented AMFlow example reproduction gaps without closing runtime lanes.",
)
SOURCE_COMMIT_PATTERN = re.compile(r"^[0-9a-f]{40}$")
AMFLOW_EXAMPLE_COVERAGE = Path("docs/release/amflow-example-coverage.md")
RELEASE_KNOWN_GAPS = Path("docs/release/known-gaps.md")
KNOWN_EXAMPLE_STATUSES = frozenset(
    (
        "reproduced-fully",
        "reproduced-fully-live",
        "reproduced-partial",
        "not-reproduced",
        "upstream-only-no-data",
    )
)
ZERO_EVIDENCE_STATUSES = frozenset(("not-reproduced", "upstream-only-no-data"))
EXAMPLE_TOKEN_PATTERN = re.compile(r"`([^`]+)`")


class HealthError(RuntimeError):
    """Raised when committed release health inputs cannot be summarized."""


@dataclass(frozen=True)
class ReadinessSummary:
    ready: bool
    blocker_count: int
    blockers: list[str]
    source_commit: str
    prerequisite_count: int
    satisfied_prerequisite_count: int
    review_section_count: int
    reviewed_section_count: int
    performance_review_path: str


@dataclass(frozen=True)
class InventorySummary:
    sidecar_count: int
    accepted_count: int
    unaccepted_count: int


@dataclass(frozen=True)
class BenchmarkSummary:
    label: str
    run_count: int
    max_wall_seconds: float
    max_rss_kb: int


@dataclass(frozen=True)
class PerformanceSummary:
    complete: bool
    benchmark_count: int
    run_count: int
    max_wall_seconds: float
    max_rss_kb: int
    benchmarks: list[BenchmarkSummary]


@dataclass(frozen=True)
class ExampleCoverageSummary:
    inventory_path: str
    known_gaps_path: str
    total_example_count: int
    full_compared_output_count: int
    live_retained_golden_count: int
    partial_retained_or_selected_count: int
    not_full_runtime_count: int
    zero_evidence_count: int
    documented_gap_count: int
    status_counts: dict[str, int]
    not_full_runtime_examples: list[str]


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise HealthError(message)


def require_repo_file(root: Path, raw: Any, field: str) -> str:
    expect(isinstance(raw, str) and raw.strip(), f"{field} must be a non-empty path")
    expect(raw == raw.strip(), f"{field} must not carry surrounding whitespace")
    value = raw
    candidate = Path(value)
    if not candidate.is_absolute():
        candidate = root / candidate
    try:
        relative = candidate.resolve(strict=False).relative_to(root.resolve(strict=True))
    except ValueError as error:
        raise HealthError(f"{field} must stay within the repository: {value}") from error
    expect(candidate.is_file(), f"{field} does not exist as a file: {value}")
    return relative.as_posix()


def require_string_list(raw: Any, field: str) -> list[str]:
    expect(isinstance(raw, list), f"{field} must be a list")
    values: list[str] = []
    for index, item in enumerate(raw):
        expect(isinstance(item, str), f"{field}[{index}] must be a string")
        values.append(item)
    return values


def numeric_value(raw: Any, field: str) -> float:
    expect(type(raw) in {int, float}, f"{field} must be numeric")
    value = float(raw)
    expect(math.isfinite(value), f"{field} must be finite")
    return value


def require_source_commit(root: Path, raw: Any, field: str) -> str:
    expect(isinstance(raw, str) and raw.strip(), f"{field} must be non-empty")
    expect(raw == raw.strip(), f"{field} must not carry surrounding whitespace")
    commit = raw.strip()
    expect(
        SOURCE_COMMIT_PATTERN.fullmatch(commit) is not None,
        f"{field} must be a full lowercase 40-character git SHA",
    )
    completed = subprocess.run(
        ["git", "cat-file", "-e", f"{commit}^{{commit}}"],
        cwd=root,
        text=True,
        capture_output=True,
        check=False,
    )
    expect(completed.returncode == 0, f"{field} is not a known commit")
    return commit


def summarize_readiness(root: Path, readiness_sidecar: Path) -> ReadinessSummary:
    readiness_path = root / readiness_sidecar
    payload = read_json(readiness_path)
    blockers = require_string_list(payload.get("release_signoff_blockers"), "release_signoff_blockers")
    prerequisites = payload.get("release_prerequisites")
    reviews = payload.get("review_sections")
    expect(isinstance(prerequisites, list), "release_prerequisites must be a list")
    expect(isinstance(reviews, list), "review_sections must be a list")

    satisfied_prerequisites = 0
    for index, prerequisite in enumerate(prerequisites):
        expect(isinstance(prerequisite, dict), f"release_prerequisites[{index}] must be an object")
        if prerequisite.get("satisfied") is True:
            satisfied_prerequisites += 1

    reviewed_sections = 0
    for index, section in enumerate(reviews):
        expect(isinstance(section, dict), f"review_sections[{index}] must be an object")
        section_blockers = section.get("blockers")
        if section.get("status") == "reviewed" and section_blockers == []:
            reviewed_sections += 1

    source_commit = require_source_commit(root, payload.get("source_commit"), "source_commit")
    performance_review_path = require_repo_file(
        root,
        payload.get("performance_review_summary_path"),
        "performance_review_summary_path",
    )
    return ReadinessSummary(
        ready=payload.get("release_signoff_ready") is True,
        blocker_count=len(blockers),
        blockers=blockers,
        source_commit=source_commit,
        prerequisite_count=len(prerequisites),
        satisfied_prerequisite_count=satisfied_prerequisites,
        review_section_count=len(reviews),
        reviewed_section_count=reviewed_sections,
        performance_review_path=performance_review_path,
    )


def verify_sidecar_inventory_entry(
    entries: list[InventoryEntry],
    sidecar_path: str,
    *,
    label: str,
    expected_schema: str,
) -> None:
    matching = [entry for entry in entries if entry.path == sidecar_path]
    expect(matching, f"{label} is not present in M7 inventory: {sidecar_path}")
    entry = matching[0]
    expect(
        entry.schema == expected_schema,
        f"{label} must have schema {expected_schema}: {sidecar_path} has {entry.schema}",
    )
    expect(
        entry.status == "accepted",
        f"{label} is not accepted: {sidecar_path} ({entry.basis})",
    )


def summarize_inventory_for_readiness(
    root: Path,
    m7_root: Path,
    readiness_sidecar: Path,
    performance_review_path: str | None = None,
) -> InventorySummary:
    entries = build_inventory(root, m7_root)
    verify_inventory(entries)
    verify_schema_reconciliation(root, m7_root, entries)
    verify_sidecar_inventory_entry(
        entries,
        readiness_sidecar.as_posix(),
        label="readiness sidecar",
        expected_schema="release-readiness-output",
    )
    if performance_review_path is not None:
        verify_sidecar_inventory_entry(
            entries,
            performance_review_path,
            label="performance review sidecar",
            expected_schema="release-performance-review",
        )
    accepted = sum(1 for entry in entries if entry.status == "accepted")
    return InventorySummary(
        sidecar_count=len(entries),
        accepted_count=accepted,
        unaccepted_count=len(entries) - accepted,
    )


def benchmark_label(entry: dict[str, Any], index: int) -> str:
    benchmark_id = entry.get("benchmark_id")
    label = entry.get("label")
    expect(isinstance(benchmark_id, str) and benchmark_id.strip(), f"benchmark[{index}].benchmark_id missing")
    expect(
        benchmark_id == benchmark_id.strip(),
        f"benchmark[{index}].benchmark_id must not carry surrounding whitespace",
    )
    expect(isinstance(label, str) and label.strip(), f"benchmark[{index}].label missing")
    expect(
        label == label.strip(),
        f"benchmark[{index}].label must not carry surrounding whitespace",
    )
    return f"{benchmark_id}.{label}"


def summarize_performance(root: Path, performance_review_path: str) -> PerformanceSummary:
    payload = read_json(root / performance_review_path)
    anti_fake = payload.get("anti_fake_parity_checks")
    expect(isinstance(anti_fake, dict), "anti_fake_parity_checks must be an object")
    failed_anti_fake = [key for key, value in sorted(anti_fake.items()) if value is not True]
    expect(not failed_anti_fake, "performance anti-fake checks failed: " + ",".join(failed_anti_fake))

    raw_benchmarks = payload.get("benchmark_timing_evidence")
    expect(isinstance(raw_benchmarks, list) and raw_benchmarks, "benchmark_timing_evidence must be non-empty")
    benchmarks: list[BenchmarkSummary] = []
    seen_labels: set[str] = set()
    for index, entry in enumerate(raw_benchmarks):
        expect(isinstance(entry, dict), f"benchmark_timing_evidence[{index}] must be an object")
        expect(entry.get("all_runs_exit_zero") is True, f"benchmark[{index}] did not have all zero exits")
        expect(entry.get("all_runs_status_success") is True, f"benchmark[{index}] did not have all success statuses")
        run_count = entry.get("run_count")
        expect(type(run_count) is int and run_count > 0, f"benchmark[{index}].run_count must be positive")
        max_wall_seconds = numeric_value(entry.get("wall_seconds_max"), f"benchmark[{index}].wall_seconds_max")
        expect(max_wall_seconds >= 0.0, f"benchmark[{index}].wall_seconds_max must be nonnegative")
        max_rss_kb = entry.get("max_rss_kb_max")
        expect(type(max_rss_kb) is int and max_rss_kb >= 0, f"benchmark[{index}].max_rss_kb_max must be nonnegative")
        label = benchmark_label(entry, index)
        expect(label not in seen_labels, f"duplicate performance benchmark label: {label}")
        seen_labels.add(label)
        benchmarks.append(
            BenchmarkSummary(
                label=label,
                run_count=run_count,
                max_wall_seconds=max_wall_seconds,
                max_rss_kb=max_rss_kb,
            )
        )

    return PerformanceSummary(
        complete=payload.get("performance_review_complete") is True
        and payload.get("benchmark_family_scope_reviewed") is True,
        benchmark_count=len(benchmarks),
        run_count=sum(benchmark.run_count for benchmark in benchmarks),
        max_wall_seconds=max(benchmark.max_wall_seconds for benchmark in benchmarks),
        max_rss_kb=max(benchmark.max_rss_kb for benchmark in benchmarks),
        benchmarks=benchmarks,
    )


def strip_markdown_code(raw: str) -> str:
    value = raw.strip()
    if value.startswith("`") and value.endswith("`") and len(value) >= 2:
        return value[1:-1]
    return value


def section_lines(markdown: str, heading: str) -> list[str]:
    lines = markdown.splitlines()
    heading_line = f"## {heading}"
    start: int | None = None
    for index, line in enumerate(lines):
        if line.strip() == heading_line:
            start = index + 1
            break
    expect(start is not None, f"release coverage markdown is missing section: {heading}")
    end = len(lines)
    for index in range(start, len(lines)):
        if lines[index].startswith("## "):
            end = index
            break
    return lines[start:end]


def split_markdown_table_row(line: str) -> list[str]:
    cells = [cell.strip() for cell in line.strip().strip("|").split("|")]
    return cells


def table_rows(section: list[str], expected_header: list[str], label: str) -> list[list[str]]:
    rows = [line for line in section if line.strip().startswith("|")]
    expect(len(rows) >= 2, f"{label} table is missing")
    header = split_markdown_table_row(rows[0])
    expect(header == expected_header, f"{label} table header drifted: {header}")
    separator = split_markdown_table_row(rows[1])
    expect(
        all(set(cell) <= {"-", ":"} and "-" in cell for cell in separator),
        f"{label} table separator is malformed",
    )
    parsed_rows = [split_markdown_table_row(row) for row in rows[2:]]
    for index, row in enumerate(parsed_rows):
        expect(len(row) == len(expected_header), f"{label} table row {index} has wrong column count")
    return parsed_rows


def parse_upstream_inventory(markdown: str) -> dict[str, str]:
    rows = table_rows(
        section_lines(markdown, "Upstream Inventory"),
        [
            "Upstream example",
            "Mathematica entry file(s)",
            "C++ lane / evidence in this repo",
            "Status",
        ],
        "upstream inventory",
    )
    inventory: dict[str, str] = {}
    for index, row in enumerate(rows):
        example = strip_markdown_code(row[0])
        status = strip_markdown_code(row[3])
        expect(example, f"upstream inventory row {index} has an empty example id")
        expect(example not in inventory, f"duplicate upstream inventory example: {example}")
        expect(status in KNOWN_EXAMPLE_STATUSES, f"unknown upstream inventory status for {example}: {status}")
        inventory[example] = status
    return inventory


def parse_not_full_examples(markdown: str) -> list[str]:
    examples: list[str] = []
    for line in section_lines(markdown, "Not Full Rows"):
        stripped = line.strip()
        if not stripped.startswith("- `"):
            continue
        match = EXAMPLE_TOKEN_PATTERN.search(stripped)
        expect(match is not None, f"not-full row is missing a backtick example id: {stripped}")
        example = match.group(1)
        expect(example not in examples, f"duplicate not-full example: {example}")
        examples.append(example)
    expect(examples, "not-full example list is empty")
    return examples


def parse_known_gap_examples(markdown: str) -> list[str]:
    rows = table_rows(
        section_lines(markdown, "Not Fully Reproduced Rows"),
        ["AMFlow example", "Remaining gap"],
        "known gaps",
    )
    examples: list[str] = []
    for index, row in enumerate(rows):
        example = strip_markdown_code(row[0])
        expect(example, f"known gaps row {index} has an empty example id")
        expect(example not in examples, f"duplicate known gap example: {example}")
        examples.append(example)
    return examples


def summarize_example_coverage_from_paths(
    root: Path,
    inventory_path: Path,
    known_gaps_path: Path,
) -> ExampleCoverageSummary:
    inventory_relative = require_repo_file(root, inventory_path.as_posix(), "amflow example coverage path")
    known_gaps_relative = require_repo_file(root, known_gaps_path.as_posix(), "release known gaps path")
    inventory_markdown = (root / inventory_relative).read_text(encoding="utf-8")
    known_gaps_markdown = (root / known_gaps_relative).read_text(encoding="utf-8")

    inventory = parse_upstream_inventory(inventory_markdown)
    not_full_examples = parse_not_full_examples(inventory_markdown)
    known_gap_examples = parse_known_gap_examples(known_gaps_markdown)
    expect(
        set(not_full_examples) == set(known_gap_examples),
        "not-full example set must match docs/release/known-gaps.md table",
    )
    expect(
        not_full_examples == known_gap_examples,
        "not-full example order must match docs/release/known-gaps.md table",
    )
    missing_from_inventory = [example for example in not_full_examples if example not in inventory]
    expect(
        not missing_from_inventory,
        "not-full examples are missing from upstream inventory: " + ", ".join(missing_from_inventory),
    )

    status_counts = Counter(inventory.values())
    zero_evidence_count = sum(status_counts[status] for status in ZERO_EVIDENCE_STATUSES)
    return ExampleCoverageSummary(
        inventory_path=inventory_relative,
        known_gaps_path=known_gaps_relative,
        total_example_count=len(inventory),
        full_compared_output_count=status_counts["reproduced-fully"],
        live_retained_golden_count=status_counts["reproduced-fully-live"],
        partial_retained_or_selected_count=status_counts["reproduced-partial"],
        not_full_runtime_count=len(known_gap_examples),
        zero_evidence_count=zero_evidence_count,
        documented_gap_count=len(known_gap_examples),
        status_counts=dict(sorted(status_counts.items())),
        not_full_runtime_examples=known_gap_examples,
    )


def summarize_example_coverage(root: Path) -> ExampleCoverageSummary:
    return summarize_example_coverage_from_paths(root, AMFLOW_EXAMPLE_COVERAGE, RELEASE_KNOWN_GAPS)


def render_text(
    readiness: ReadinessSummary,
    inventory: InventorySummary,
    performance: PerformanceSummary,
    example_coverage: ExampleCoverageSummary,
) -> str:
    status = release_status(readiness).upper()
    benchmark_tokens = [
        f"{benchmark.label} max={benchmark.max_wall_seconds:.2f}s"
        for benchmark in performance.benchmarks
    ]
    lines = [
        "M7 release health summary",
        f"status: {status}",
        f"source_commit: {readiness.source_commit}",
        (
            "readiness: "
            f"release_signoff_ready={str(readiness.ready).lower()} "
            f"blockers={readiness.blocker_count} "
            f"prerequisites={readiness.satisfied_prerequisite_count}/{readiness.prerequisite_count} "
            f"review_sections={readiness.reviewed_section_count}/{readiness.review_section_count}"
        ),
        (
            "inventory: "
            f"total={inventory.sidecar_count} "
            f"accepted={inventory.accepted_count} "
            f"unaccepted={inventory.unaccepted_count} "
            "schema_reconciled=true"
        ),
        (
            "performance: "
            f"review_complete={str(performance.complete).lower()} "
            f"benchmarks={performance.benchmark_count} "
            f"runs={performance.run_count} "
            f"max_wall_seconds={performance.max_wall_seconds:.2f} "
            f"max_rss_kb={performance.max_rss_kb}"
        ),
        "performance_benchmarks: " + "; ".join(benchmark_tokens),
        (
            "amflow_example_coverage: "
            f"total={example_coverage.total_example_count} "
            f"full_compared_output={example_coverage.full_compared_output_count} "
            f"live_retained_golden={example_coverage.live_retained_golden_count} "
            f"partial={example_coverage.partial_retained_or_selected_count} "
            f"not_full_runtime={example_coverage.not_full_runtime_count} "
            f"known_gap_rows={example_coverage.documented_gap_count} "
            f"zero_evidence={example_coverage.zero_evidence_count}"
        ),
        "not_full_runtime_examples: " + "; ".join(example_coverage.not_full_runtime_examples),
        "blockers: " + ("none" if not readiness.blockers else "; ".join(readiness.blockers)),
        "withheld_claims: " + " ".join(WITHHELD_CLAIMS),
    ]
    return "\n".join(lines)


def release_status(readiness: ReadinessSummary) -> str:
    return "ready" if readiness.ready and readiness.blocker_count == 0 else "blocked"


def render_json(
    readiness: ReadinessSummary,
    inventory: InventorySummary,
    performance: PerformanceSummary,
    example_coverage: ExampleCoverageSummary,
) -> str:
    payload = {
        "schema_version": 1,
        "status": release_status(readiness),
        "source_commit": readiness.source_commit,
        "readiness": {
            "release_signoff_ready": readiness.ready,
            "blocker_count": readiness.blocker_count,
            "prerequisite_count": readiness.prerequisite_count,
            "satisfied_prerequisite_count": readiness.satisfied_prerequisite_count,
            "review_section_count": readiness.review_section_count,
            "reviewed_section_count": readiness.reviewed_section_count,
            "performance_review_path": readiness.performance_review_path,
        },
        "inventory": {
            "sidecar_count": inventory.sidecar_count,
            "accepted_count": inventory.accepted_count,
            "unaccepted_count": inventory.unaccepted_count,
            "schema_reconciled": True,
        },
        "performance": {
            "review_complete": performance.complete,
            "benchmark_count": performance.benchmark_count,
            "run_count": performance.run_count,
            "max_wall_seconds": performance.max_wall_seconds,
            "max_rss_kb": performance.max_rss_kb,
            "benchmarks": [
                {
                    "label": benchmark.label,
                    "run_count": benchmark.run_count,
                    "max_wall_seconds": benchmark.max_wall_seconds,
                    "max_rss_kb": benchmark.max_rss_kb,
                }
                for benchmark in performance.benchmarks
            ],
        },
        "amflow_example_coverage": {
            "inventory_path": example_coverage.inventory_path,
            "known_gaps_path": example_coverage.known_gaps_path,
            "total_example_count": example_coverage.total_example_count,
            "full_compared_output_count": example_coverage.full_compared_output_count,
            "live_retained_golden_count": example_coverage.live_retained_golden_count,
            "partial_retained_or_selected_count": example_coverage.partial_retained_or_selected_count,
            "not_full_runtime_count": example_coverage.not_full_runtime_count,
            "zero_evidence_count": example_coverage.zero_evidence_count,
            "documented_gap_count": example_coverage.documented_gap_count,
            "status_counts": example_coverage.status_counts,
            "not_full_runtime_examples": example_coverage.not_full_runtime_examples,
        },
        "blockers": readiness.blockers,
        "withheld_claims": list(WITHHELD_CLAIMS),
    }
    return json.dumps(payload, indent=2, sort_keys=True)


def expect_health_error(label: str, action: Callable[[], None], expected: str) -> None:
    try:
        action()
    except HealthError as error:
        expect(expected in str(error), f"{label} failed for the wrong reason: {error}")
        return
    raise HealthError(f"{label} unexpectedly passed")


def self_check(root: Path) -> None:
    readiness = summarize_readiness(root, ACCEPTED_READINESS_SIDECAR)
    summarize_inventory_for_readiness(
        root,
        M7_ROOT,
        ACCEPTED_READINESS_SIDECAR,
        readiness.performance_review_path,
    )
    summarize_performance(root, readiness.performance_review_path)
    summarize_example_coverage(root)

    absolute_readiness_sidecar = Path(
        require_repo_file(
            root,
            (root / ACCEPTED_READINESS_SIDECAR).as_posix(),
            "readiness sidecar",
        )
    )
    expect(
        absolute_readiness_sidecar == ACCEPTED_READINESS_SIDECAR,
        "absolute readiness sidecar path did not normalize to repository-relative form",
    )
    absolute_readiness = summarize_readiness(root, absolute_readiness_sidecar)
    expect(
        absolute_readiness.performance_review_path == readiness.performance_review_path,
        "absolute readiness sidecar path changed the selected performance review",
    )
    expect_health_error(
        "readiness sidecar surrounding whitespace check",
        lambda: require_repo_file(
            root,
            f" {ACCEPTED_READINESS_SIDECAR.as_posix()} ",
            "readiness sidecar",
        ),
        "readiness sidecar must not carry surrounding whitespace",
    )
    summarize_inventory_for_readiness(
        root,
        M7_ROOT,
        absolute_readiness_sidecar,
        absolute_readiness.performance_review_path,
    )

    performance_payload = read_json(root / readiness.performance_review_path)
    duplicate_payload = json.loads(json.dumps(performance_payload))
    benchmark_evidence = duplicate_payload.get("benchmark_timing_evidence")
    expect(
        isinstance(benchmark_evidence, list) and benchmark_evidence,
        "accepted performance sidecar must carry benchmark timing evidence",
    )
    benchmark_evidence.append(dict(benchmark_evidence[0]))
    with tempfile.TemporaryDirectory(prefix="release-health-self-check-", dir=root) as temp_dir:
        duplicate_path = Path(temp_dir) / "duplicate-performance-benchmark.json"
        duplicate_path.write_text(
            json.dumps(duplicate_payload, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        expect_health_error(
            "performance sidecar inventory membership check",
            lambda: summarize_inventory_for_readiness(
                root,
                M7_ROOT,
                ACCEPTED_READINESS_SIDECAR,
                duplicate_path.relative_to(root).as_posix(),
            ),
            "performance review sidecar is not present in M7 inventory",
        )
        expect_health_error(
            "duplicate performance benchmark check",
            lambda: summarize_performance(root, str(duplicate_path.relative_to(root))),
            "duplicate performance benchmark label",
        )
        negative_timing_payload = json.loads(json.dumps(performance_payload))
        negative_timing_payload["benchmark_timing_evidence"][0]["wall_seconds_max"] = -0.01
        negative_timing_path = Path(temp_dir) / "negative-performance-benchmark.json"
        negative_timing_path.write_text(
            json.dumps(negative_timing_payload, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        expect_health_error(
            "negative performance benchmark timing check",
            lambda: summarize_performance(root, str(negative_timing_path.relative_to(root))),
            "wall_seconds_max must be nonnegative",
        )
        infinite_timing_payload = json.loads(json.dumps(performance_payload))
        infinite_timing_payload["benchmark_timing_evidence"][0]["wall_seconds_max"] = float("inf")
        infinite_timing_path = Path(temp_dir) / "infinite-performance-benchmark.json"
        infinite_timing_path.write_text(
            json.dumps(infinite_timing_payload, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        expect_health_error(
            "infinite performance benchmark timing check",
            lambda: summarize_performance(root, str(infinite_timing_path.relative_to(root))),
            "wall_seconds_max must be finite",
        )
        whitespace_label_payload = json.loads(json.dumps(performance_payload))
        whitespace_label_payload["benchmark_timing_evidence"][0]["label"] = (
            f" {whitespace_label_payload['benchmark_timing_evidence'][0]['label']} "
        )
        whitespace_label_path = Path(temp_dir) / "whitespace-performance-benchmark-label.json"
        whitespace_label_path.write_text(
            json.dumps(whitespace_label_payload, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        expect_health_error(
            "performance benchmark label whitespace check",
            lambda: summarize_performance(root, str(whitespace_label_path.relative_to(root))),
            "benchmark[0].label must not carry surrounding whitespace",
        )
        whitespace_benchmark_id_payload = json.loads(json.dumps(performance_payload))
        whitespace_benchmark_id_payload["benchmark_timing_evidence"][0]["benchmark_id"] = (
            f" {whitespace_benchmark_id_payload['benchmark_timing_evidence'][0]['benchmark_id']} "
        )
        whitespace_benchmark_id_path = Path(temp_dir) / "whitespace-performance-benchmark-id.json"
        whitespace_benchmark_id_path.write_text(
            json.dumps(whitespace_benchmark_id_payload, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        expect_health_error(
            "performance benchmark id whitespace check",
            lambda: summarize_performance(root, str(whitespace_benchmark_id_path.relative_to(root))),
            "benchmark[0].benchmark_id must not carry surrounding whitespace",
        )
        short_commit_payload = json.loads(json.dumps(read_json(root / ACCEPTED_READINESS_SIDECAR)))
        short_commit_payload["source_commit"] = "5fdba2c"
        short_commit_path = Path(temp_dir) / "short-source-commit-readiness.json"
        short_commit_path.write_text(
            json.dumps(short_commit_payload, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        expect_health_error(
            "short source commit check",
            lambda: summarize_readiness(root, short_commit_path.relative_to(root)),
            "source_commit must be a full lowercase 40-character git SHA",
        )
        whitespace_commit_payload = json.loads(json.dumps(read_json(root / ACCEPTED_READINESS_SIDECAR)))
        whitespace_commit_payload["source_commit"] = (
            f" {whitespace_commit_payload['source_commit']} "
        )
        whitespace_commit_path = Path(temp_dir) / "whitespace-source-commit-readiness.json"
        whitespace_commit_path.write_text(
            json.dumps(whitespace_commit_payload, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        expect_health_error(
            "source commit whitespace check",
            lambda: summarize_readiness(root, whitespace_commit_path.relative_to(root)),
            "source_commit must not carry surrounding whitespace",
        )
        unknown_commit_payload = json.loads(json.dumps(read_json(root / ACCEPTED_READINESS_SIDECAR)))
        unknown_commit_payload["source_commit"] = "f" * 40
        unknown_commit_path = Path(temp_dir) / "unknown-source-commit-readiness.json"
        unknown_commit_path.write_text(
            json.dumps(unknown_commit_payload, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        expect_health_error(
            "unknown source commit check",
            lambda: summarize_readiness(root, unknown_commit_path.relative_to(root)),
            "source_commit is not a known commit",
        )
        coverage_payload = (root / AMFLOW_EXAMPLE_COVERAGE).read_text(encoding="utf-8")
        known_gaps_payload = (root / RELEASE_KNOWN_GAPS).read_text(encoding="utf-8")
        missing_gap_lines = [
            line
            for line in known_gaps_payload.splitlines()
            if not line.startswith("| `user_defined_ending` |")
        ]
        missing_gap_payload = "\n".join(missing_gap_lines)
        if known_gaps_payload.endswith("\n"):
            missing_gap_payload += "\n"
        expect(
            missing_gap_payload != known_gaps_payload,
            "known gaps self-check fixture must remove user_defined_ending row",
        )
        missing_gap_path = Path(temp_dir) / "known-gaps-missing-user-defined-ending.md"
        missing_gap_path.write_text(missing_gap_payload, encoding="utf-8")
        expect_health_error(
            "known gaps coverage coherence check",
            lambda: summarize_example_coverage_from_paths(
                root,
                AMFLOW_EXAMPLE_COVERAGE,
                missing_gap_path.relative_to(root),
            ),
            "not-full example set must match docs/release/known-gaps.md table",
        )
        reordered_gap_lines = known_gaps_payload.splitlines()
        first_gap_index = next(
            (
                index
                for index, line in enumerate(reordered_gap_lines)
                if line.startswith("| `automatic_phasespace` |")
            ),
            None,
        )
        second_gap_index = next(
            (
                index
                for index, line in enumerate(reordered_gap_lines)
                if line.startswith("| `complex_kinematics` |")
            ),
            None,
        )
        expect(
            first_gap_index is not None and second_gap_index is not None,
            "known gaps self-check fixture must contain the first two not-full rows",
        )
        reordered_gap_lines[first_gap_index], reordered_gap_lines[second_gap_index] = (
            reordered_gap_lines[second_gap_index],
            reordered_gap_lines[first_gap_index],
        )
        reordered_gap_payload = "\n".join(reordered_gap_lines)
        if known_gaps_payload.endswith("\n"):
            reordered_gap_payload += "\n"
        reordered_gap_path = Path(temp_dir) / "known-gaps-reordered-not-full.md"
        reordered_gap_path.write_text(reordered_gap_payload, encoding="utf-8")
        expect_health_error(
            "known gaps coverage order check",
            lambda: summarize_example_coverage_from_paths(
                root,
                AMFLOW_EXAMPLE_COVERAGE,
                reordered_gap_path.relative_to(root),
            ),
            "not-full example order must match docs/release/known-gaps.md table",
        )
        unknown_status_payload = coverage_payload.replace(
            "| `automatic_loop` | `examples/automatic_loop/run.wl` | Core solve-series evidence: M5 lane39/lane45 `automatic_loop.eps8`, 126/126 coefficients, min 41 digits; phase-0 retained state `tools/reference-harness/specs/phase0/automatic_loop.amflow-state.json`. | `reproduced-fully` |",
            "| `automatic_loop` | `examples/automatic_loop/run.wl` | Core solve-series evidence: M5 lane39/lane45 `automatic_loop.eps8`, 126/126 coefficients, min 41 digits; phase-0 retained state `tools/reference-harness/specs/phase0/automatic_loop.amflow-state.json`. | `unknown-status` |",
        )
        unknown_status_path = Path(temp_dir) / "amflow-example-coverage-unknown-status.md"
        unknown_status_path.write_text(unknown_status_payload, encoding="utf-8")
        expect_health_error(
            "upstream inventory status vocabulary check",
            lambda: summarize_example_coverage_from_paths(
                root,
                unknown_status_path.relative_to(root),
                RELEASE_KNOWN_GAPS,
            ),
            "unknown upstream inventory status",
        )

    expect_health_error(
        "unaccepted readiness sidecar check",
        lambda: summarize_inventory_for_readiness(
            root,
            M7_ROOT,
            Path("tools/reference-harness/specs/m7/lane3/release-readiness.post-a1f0e1d.full-output.json"),
        ),
        "readiness sidecar is not accepted",
    )
    expect_health_error(
        "non-readiness sidecar check",
        lambda: summarize_inventory_for_readiness(
            root,
            M7_ROOT,
            Path("tools/reference-harness/specs/m7/lane70/release-performance-review.json"),
        ),
        "readiness sidecar must have schema release-readiness-output",
    )
    expect_health_error(
        "non-performance sidecar check",
        lambda: summarize_inventory_for_readiness(
            root,
            M7_ROOT,
            ACCEPTED_READINESS_SIDECAR,
            "tools/reference-harness/specs/m7/lane133/release-qualification-corpus.json",
        ),
        "performance review sidecar must have schema release-performance-review",
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--readiness-sidecar",
        default=str(ACCEPTED_READINESS_SIDECAR),
        help="Repository-relative accepted release-readiness sidecar to summarize.",
    )
    parser.add_argument(
        "--m7-root",
        default=str(M7_ROOT),
        help="Repository-relative M7 sidecar root to inventory.",
    )
    parser.add_argument(
        "--format",
        choices=("text", "json"),
        default="text",
        help="Output format.",
    )
    parser.add_argument(
        "--verify",
        action="store_true",
        help="Fail unless readiness is ready, blockers are empty, inventory reconciles, and performance is complete.",
    )
    parser.add_argument(
        "--self-check",
        action="store_true",
        help="Run negative checks for stale or non-readiness source sidecars.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    root = repo_root()
    try:
        if args.self_check:
            self_check(root)
            print("M7 release health source-sidecar self-check passed")
            return 0
        readiness_sidecar = Path(require_repo_file(root, args.readiness_sidecar, "readiness sidecar"))
        readiness = summarize_readiness(root, readiness_sidecar)
        inventory = summarize_inventory_for_readiness(
            root,
            Path(args.m7_root),
            readiness_sidecar,
            readiness.performance_review_path,
        )
        performance = summarize_performance(root, readiness.performance_review_path)
        example_coverage = summarize_example_coverage(root)
    except (HealthError, InventoryError, SchemaError, RuntimeError) as error:
        print(str(error), file=sys.stderr)
        return 1

    if args.format == "json":
        print(render_json(readiness, inventory, performance, example_coverage))
    else:
        print(render_text(readiness, inventory, performance, example_coverage))
    if args.verify:
        failures: list[str] = []
        if not readiness.ready:
            failures.append("release_signoff_ready is false")
        if readiness.blockers:
            failures.append("release_signoff_blockers is not empty")
        if readiness.satisfied_prerequisite_count != readiness.prerequisite_count:
            failures.append("not all release prerequisites are satisfied")
        if readiness.reviewed_section_count != readiness.review_section_count:
            failures.append("not all release review sections are reviewed")
        if not performance.complete:
            failures.append("performance review is incomplete")
        if example_coverage.zero_evidence_count != 0:
            failures.append("AMFlow example coverage has zero-evidence rows")
        if failures:
            print("M7 release health verification failed: " + "; ".join(failures), file=sys.stderr)
            return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
