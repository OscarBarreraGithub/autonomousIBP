#!/usr/bin/env python3
"""Print a compact M7 release health summary from committed sidecars."""

from __future__ import annotations

import argparse
import json
import sys
import tempfile
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
)


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


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise HealthError(message)


def require_repo_file(root: Path, raw: Any, field: str) -> str:
    expect(isinstance(raw, str) and raw.strip(), f"{field} must be a non-empty path")
    value = raw.strip()
    candidate = root / value
    try:
        candidate.resolve(strict=False).relative_to(root.resolve(strict=False))
    except ValueError as error:
        raise HealthError(f"{field} must stay within the repository: {value}") from error
    expect(candidate.is_file(), f"{field} does not exist as a file: {value}")
    return value


def require_string_list(raw: Any, field: str) -> list[str]:
    expect(isinstance(raw, list), f"{field} must be a list")
    values: list[str] = []
    for index, item in enumerate(raw):
        expect(isinstance(item, str), f"{field}[{index}] must be a string")
        values.append(item)
    return values


def numeric_value(raw: Any, field: str) -> float:
    expect(type(raw) in {int, float}, f"{field} must be numeric")
    return float(raw)


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

    source_commit = payload.get("source_commit")
    expect(isinstance(source_commit, str) and source_commit.strip(), "source_commit must be non-empty")
    performance_review_path = require_repo_file(
        root,
        payload.get("performance_review_summary_path"),
        "performance_review_summary_path",
    )
    return ReadinessSummary(
        ready=payload.get("release_signoff_ready") is True,
        blocker_count=len(blockers),
        blockers=blockers,
        source_commit=source_commit.strip(),
        prerequisite_count=len(prerequisites),
        satisfied_prerequisite_count=satisfied_prerequisites,
        review_section_count=len(reviews),
        reviewed_section_count=reviewed_sections,
        performance_review_path=performance_review_path,
    )


def verify_readiness_sidecar_entry(
    entries: list[InventoryEntry],
    readiness_sidecar: Path,
) -> None:
    relative = readiness_sidecar.as_posix()
    matching = [entry for entry in entries if entry.path == relative]
    expect(matching, f"readiness sidecar is not present in M7 inventory: {relative}")
    entry = matching[0]
    expect(
        entry.schema == "release-readiness-output",
        "readiness sidecar must have schema release-readiness-output: "
        f"{relative} has {entry.schema}",
    )
    expect(
        entry.status == "accepted",
        f"readiness sidecar is not accepted: {relative} ({entry.basis})",
    )


def summarize_inventory_for_readiness(
    root: Path,
    m7_root: Path,
    readiness_sidecar: Path,
) -> InventorySummary:
    entries = build_inventory(root, m7_root)
    verify_inventory(entries)
    verify_schema_reconciliation(root, m7_root, entries)
    verify_readiness_sidecar_entry(entries, readiness_sidecar)
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
    expect(isinstance(label, str) and label.strip(), f"benchmark[{index}].label missing")
    return f"{benchmark_id.strip()}.{label.strip()}"


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


def render_text(
    readiness: ReadinessSummary,
    inventory: InventorySummary,
    performance: PerformanceSummary,
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
    summarize_inventory_for_readiness(root, M7_ROOT, ACCEPTED_READINESS_SIDECAR)
    readiness = summarize_readiness(root, ACCEPTED_READINESS_SIDECAR)
    summarize_performance(root, readiness.performance_review_path)

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
            "duplicate performance benchmark check",
            lambda: summarize_performance(root, str(duplicate_path.relative_to(root))),
            "duplicate performance benchmark label",
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
        inventory = summarize_inventory_for_readiness(root, Path(args.m7_root), readiness_sidecar)
        performance = summarize_performance(root, readiness.performance_review_path)
    except (HealthError, InventoryError, SchemaError, RuntimeError) as error:
        print(str(error), file=sys.stderr)
        return 1

    if args.format == "json":
        print(render_json(readiness, inventory, performance))
    else:
        print(render_text(readiness, inventory, performance))
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
        if failures:
            print("M7 release health verification failed: " + "; ".join(failures), file=sys.stderr)
            return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
