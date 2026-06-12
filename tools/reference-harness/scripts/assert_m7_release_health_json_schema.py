#!/usr/bin/env python3
"""Schema gate for the M7 release health JSON output."""

from __future__ import annotations

import argparse
import copy
import json
import math
import re
import subprocess
import sys
from pathlib import Path
from typing import Any, Callable


SOURCE_COMMIT_PATTERN = re.compile(r"^[0-9a-f]{40}$")
DATE_PATTERN = re.compile(r"^[0-9]{4}-[0-9]{2}-[0-9]{2}$")
KNOWN_RELEASE_STATUSES = frozenset(("ready", "blocked"))
REQUESTED_PRECISION_LIVE_STATUS = "reproduced-matches-golden-at-requested-precision"
FULL_COMPARED_OUTPUT_STATUSES = frozenset(("reproduced-fully", REQUESTED_PRECISION_LIVE_STATUS))
LIVE_RETAINED_GOLDEN_STATUSES = frozenset(("reproduced-fully-live", REQUESTED_PRECISION_LIVE_STATUS))
KNOWN_AMFLOW_EXAMPLE_STATUSES = frozenset(
    (
        "reproduced-fully",
        "reproduced-fully-live",
        REQUESTED_PRECISION_LIVE_STATUS,
        "reproduced-partial",
        "not-reproduced",
        "upstream-only-no-data",
    )
)
ZERO_EVIDENCE_STATUSES = frozenset(("not-reproduced", "upstream-only-no-data"))

TOP_LEVEL_KEYS = frozenset(
    (
        "amflow_example_coverage",
        "blockers",
        "inventory",
        "performance",
        "readiness",
        "schema_version",
        "source_commit",
        "status",
        "withheld_claims",
    )
)
READINESS_KEYS = frozenset(
    (
        "blocker_count",
        "performance_review_path",
        "prerequisite_count",
        "release_signoff_ready",
        "review_section_count",
        "reviewed_section_count",
        "satisfied_prerequisite_count",
    )
)
INVENTORY_KEYS = frozenset(
    (
        "accepted_count",
        "schema_reconciled",
        "sidecar_count",
        "unaccepted_count",
    )
)
PERFORMANCE_KEYS = frozenset(
    (
        "benchmark_count",
        "benchmarks",
        "max_rss_kb",
        "max_wall_seconds",
        "review_complete",
        "run_count",
    )
)
PERFORMANCE_BENCHMARK_KEYS = frozenset(
    (
        "label",
        "max_rss_kb",
        "max_wall_seconds",
        "run_count",
    )
)
AMFLOW_COVERAGE_KEYS = frozenset(
    (
        "documented_gap_count",
        "full_compared_output_count",
        "inventory_path",
        "known_gaps_path",
        "live_retained_golden_count",
        "live_reverified_count",
        "live_reverified_examples",
        "not_full_runtime_count",
        "not_full_runtime_examples",
        "partial_retained_or_selected_count",
        "status_counts",
        "total_example_count",
        "zero_evidence_count",
    )
)
LIVE_REVERIFIED_KEYS = frozenset(("date", "detail", "example", "path"))


class SchemaGateError(RuntimeError):
    """Raised when the release health JSON schema contract is violated."""


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise SchemaGateError(message)


def path_join(path: str, key: str | int) -> str:
    if isinstance(key, int):
        return f"{path}[{key}]"
    return f"{path}.{key}" if path != "$" else f"$.{key}"


def require_object(value: Any, path: str) -> dict[str, Any]:
    expect(isinstance(value, dict), f"{path} must be an object")
    return value


def require_exact_keys(payload: dict[str, Any], keys: frozenset[str], path: str) -> None:
    actual = set(payload)
    missing = sorted(keys - actual)
    extra = sorted(actual - keys)
    expect(not missing, f"{path} missing required key(s): {', '.join(missing)}")
    expect(not extra, f"{path} has unexpected key(s): {', '.join(extra)}")


def require_bool(payload: dict[str, Any], key: str, path: str) -> bool:
    field_path = path_join(path, key)
    value = payload.get(key)
    expect(type(value) is bool, f"{field_path} must be a boolean")
    return value


def require_int(payload: dict[str, Any], key: str, path: str) -> int:
    field_path = path_join(path, key)
    value = payload.get(key)
    expect(type(value) is int, f"{field_path} must be an integer")
    return value


def require_nonnegative_int(payload: dict[str, Any], key: str, path: str) -> int:
    value = require_int(payload, key, path)
    expect(value >= 0, f"{path_join(path, key)} must be nonnegative")
    return value


def require_positive_int(payload: dict[str, Any], key: str, path: str) -> int:
    value = require_int(payload, key, path)
    expect(value > 0, f"{path_join(path, key)} must be positive")
    return value


def require_number(payload: dict[str, Any], key: str, path: str) -> float:
    field_path = path_join(path, key)
    value = payload.get(key)
    expect(type(value) in {int, float}, f"{field_path} must be a number")
    numeric = float(value)
    expect(math.isfinite(numeric), f"{field_path} must be finite")
    return numeric


def require_nonnegative_number(payload: dict[str, Any], key: str, path: str) -> float:
    value = require_number(payload, key, path)
    expect(value >= 0.0, f"{path_join(path, key)} must be nonnegative")
    return value


def require_string(payload: dict[str, Any], key: str, path: str, *, nonempty: bool = True) -> str:
    field_path = path_join(path, key)
    value = payload.get(key)
    expect(type(value) is str, f"{field_path} must be a string")
    if nonempty:
        expect(value != "", f"{field_path} must be non-empty")
    expect(value == value.strip(), f"{field_path} must not carry surrounding whitespace")
    return value


def require_string_list(payload: dict[str, Any], key: str, path: str) -> list[str]:
    field_path = path_join(path, key)
    value = payload.get(key)
    expect(isinstance(value, list), f"{field_path} must be a list")
    strings: list[str] = []
    for index, item in enumerate(value):
        item_path = path_join(field_path, index)
        expect(type(item) is str, f"{item_path} must be a string")
        expect(item != "", f"{item_path} must be non-empty")
        expect(item == item.strip(), f"{item_path} must not carry surrounding whitespace")
        strings.append(item)
    return strings


def require_object_list(payload: dict[str, Any], key: str, path: str) -> list[dict[str, Any]]:
    field_path = path_join(path, key)
    value = payload.get(key)
    expect(isinstance(value, list), f"{field_path} must be a list")
    objects: list[dict[str, Any]] = []
    for index, item in enumerate(value):
        objects.append(require_object(item, path_join(field_path, index)))
    return objects


def require_path_string(payload: dict[str, Any], key: str, path: str) -> str:
    value = require_string(payload, key, path)
    expect(not Path(value).is_absolute(), f"{path_join(path, key)} must be repository-relative")
    expect(".." not in Path(value).parts, f"{path_join(path, key)} must not escape the repository")
    return value


def require_unique(values: list[str], path: str) -> None:
    duplicates = sorted(value for value in set(values) if values.count(value) > 1)
    expect(not duplicates, f"{path} must not contain duplicates: {', '.join(duplicates)}")


def assert_readiness_schema(readiness: dict[str, Any], blockers: list[str], status: str) -> None:
    path = "$.readiness"
    require_exact_keys(readiness, READINESS_KEYS, path)
    ready = require_bool(readiness, "release_signoff_ready", path)
    blocker_count = require_nonnegative_int(readiness, "blocker_count", path)
    prerequisite_count = require_nonnegative_int(readiness, "prerequisite_count", path)
    satisfied_prerequisites = require_nonnegative_int(readiness, "satisfied_prerequisite_count", path)
    review_section_count = require_nonnegative_int(readiness, "review_section_count", path)
    reviewed_sections = require_nonnegative_int(readiness, "reviewed_section_count", path)
    require_path_string(readiness, "performance_review_path", path)

    expect(
        blocker_count == len(blockers),
        "$.readiness.blocker_count must match $.blockers length",
    )
    expect(
        satisfied_prerequisites <= prerequisite_count,
        "$.readiness.satisfied_prerequisite_count must not exceed prerequisite_count",
    )
    expect(
        reviewed_sections <= review_section_count,
        "$.readiness.reviewed_section_count must not exceed review_section_count",
    )
    expect(
        (status == "ready") == (ready and blocker_count == 0),
        "$.status must match readiness.release_signoff_ready and blocker_count",
    )


def assert_inventory_schema(inventory: dict[str, Any]) -> None:
    path = "$.inventory"
    require_exact_keys(inventory, INVENTORY_KEYS, path)
    sidecar_count = require_nonnegative_int(inventory, "sidecar_count", path)
    accepted_count = require_nonnegative_int(inventory, "accepted_count", path)
    unaccepted_count = require_nonnegative_int(inventory, "unaccepted_count", path)
    expect(
        require_bool(inventory, "schema_reconciled", path) is True,
        "$.inventory.schema_reconciled must be true",
    )
    expect(
        accepted_count + unaccepted_count == sidecar_count,
        "$.inventory accepted_count + unaccepted_count must equal sidecar_count",
    )


def assert_performance_schema(performance: dict[str, Any]) -> None:
    path = "$.performance"
    require_exact_keys(performance, PERFORMANCE_KEYS, path)
    require_bool(performance, "review_complete", path)
    benchmark_count = require_nonnegative_int(performance, "benchmark_count", path)
    run_count = require_nonnegative_int(performance, "run_count", path)
    max_wall_seconds = require_nonnegative_number(performance, "max_wall_seconds", path)
    max_rss_kb = require_nonnegative_int(performance, "max_rss_kb", path)
    benchmarks = require_object_list(performance, "benchmarks", path)
    expect(benchmark_count == len(benchmarks), "$.performance.benchmark_count must match benchmarks length")

    labels: list[str] = []
    benchmark_run_count = 0
    benchmark_max_wall_seconds = 0.0
    benchmark_max_rss_kb = 0
    for index, benchmark in enumerate(benchmarks):
        benchmark_path = path_join("$.performance.benchmarks", index)
        require_exact_keys(benchmark, PERFORMANCE_BENCHMARK_KEYS, benchmark_path)
        labels.append(require_string(benchmark, "label", benchmark_path))
        benchmark_run_count += require_positive_int(benchmark, "run_count", benchmark_path)
        benchmark_max_wall_seconds = max(
            benchmark_max_wall_seconds,
            require_nonnegative_number(benchmark, "max_wall_seconds", benchmark_path),
        )
        benchmark_max_rss_kb = max(
            benchmark_max_rss_kb,
            require_nonnegative_int(benchmark, "max_rss_kb", benchmark_path),
        )
    require_unique(labels, "$.performance.benchmarks[*].label")
    expect(run_count == benchmark_run_count, "$.performance.run_count must equal benchmark run_count sum")
    expect(
        math.isclose(max_wall_seconds, benchmark_max_wall_seconds, rel_tol=0.0, abs_tol=1.0e-12),
        "$.performance.max_wall_seconds must equal the largest benchmark max_wall_seconds",
    )
    expect(
        max_rss_kb == benchmark_max_rss_kb,
        "$.performance.max_rss_kb must equal the largest benchmark max_rss_kb",
    )


def assert_live_reverified_schema(records: list[dict[str, Any]]) -> None:
    examples: list[str] = []
    for index, record in enumerate(records):
        path = path_join("$.amflow_example_coverage.live_reverified_examples", index)
        require_exact_keys(record, LIVE_REVERIFIED_KEYS, path)
        date = require_string(record, "date", path)
        expect(DATE_PATTERN.fullmatch(date) is not None, f"{path}.date must be YYYY-MM-DD")
        require_string(record, "detail", path, nonempty=False)
        examples.append(require_string(record, "example", path))
        require_path_string(record, "path", path)
    require_unique(examples, "$.amflow_example_coverage.live_reverified_examples[*].example")


def assert_amflow_example_coverage_schema(coverage: dict[str, Any]) -> None:
    path = "$.amflow_example_coverage"
    require_exact_keys(coverage, AMFLOW_COVERAGE_KEYS, path)
    require_path_string(coverage, "inventory_path", path)
    require_path_string(coverage, "known_gaps_path", path)
    total_example_count = require_nonnegative_int(coverage, "total_example_count", path)
    full_compared_output_count = require_nonnegative_int(coverage, "full_compared_output_count", path)
    live_retained_golden_count = require_nonnegative_int(coverage, "live_retained_golden_count", path)
    live_reverified_count = require_nonnegative_int(coverage, "live_reverified_count", path)
    partial_count = require_nonnegative_int(coverage, "partial_retained_or_selected_count", path)
    not_full_runtime_count = require_nonnegative_int(coverage, "not_full_runtime_count", path)
    zero_evidence_count = require_nonnegative_int(coverage, "zero_evidence_count", path)
    documented_gap_count = require_nonnegative_int(coverage, "documented_gap_count", path)

    live_reverified = require_object_list(coverage, "live_reverified_examples", path)
    assert_live_reverified_schema(live_reverified)
    expect(
        live_reverified_count == len(live_reverified),
        "$.amflow_example_coverage.live_reverified_count must match live_reverified_examples length",
    )

    not_full_examples = require_string_list(coverage, "not_full_runtime_examples", path)
    require_unique(not_full_examples, "$.amflow_example_coverage.not_full_runtime_examples")
    expect(
        not_full_runtime_count == len(not_full_examples),
        "$.amflow_example_coverage.not_full_runtime_count must match not_full_runtime_examples length",
    )
    expect(
        documented_gap_count == len(not_full_examples),
        "$.amflow_example_coverage.documented_gap_count must match not_full_runtime_examples length",
    )

    status_counts = require_object(coverage.get("status_counts"), "$.amflow_example_coverage.status_counts")
    status_total = 0
    for status, count in status_counts.items():
        expect(type(status) is str and status, "$.amflow_example_coverage.status_counts keys must be strings")
        expect(
            status in KNOWN_AMFLOW_EXAMPLE_STATUSES,
            f"$.amflow_example_coverage.status_counts has unknown status: {status}",
        )
        expect(type(count) is int and count >= 0, f"$.amflow_example_coverage.status_counts.{status} must be nonnegative integer")
        status_total += count
    expect(
        status_total == total_example_count,
        "$.amflow_example_coverage.status_counts must sum to total_example_count",
    )
    expect(
        sum(status_counts.get(status, 0) for status in FULL_COMPARED_OUTPUT_STATUSES)
        == full_compared_output_count,
        "$.amflow_example_coverage.full_compared_output_count must match status_counts",
    )
    expect(
        sum(status_counts.get(status, 0) for status in LIVE_RETAINED_GOLDEN_STATUSES)
        == live_retained_golden_count,
        "$.amflow_example_coverage.live_retained_golden_count must match status_counts",
    )
    expect(
        status_counts.get("reproduced-partial", 0) == partial_count,
        "$.amflow_example_coverage.partial_retained_or_selected_count must match status_counts",
    )
    expect(
        sum(status_counts.get(status, 0) for status in ZERO_EVIDENCE_STATUSES) == zero_evidence_count,
        "$.amflow_example_coverage.zero_evidence_count must match zero-evidence statuses",
    )


def assert_release_health_schema(payload: dict[str, Any]) -> None:
    require_exact_keys(payload, TOP_LEVEL_KEYS, "$")
    schema_version = require_int(payload, "schema_version", "$")
    expect(schema_version == 1, "$.schema_version must be integer 1")
    status = require_string(payload, "status", "$")
    expect(status in KNOWN_RELEASE_STATUSES, "$.status must be one of: blocked, ready")
    source_commit = require_string(payload, "source_commit", "$")
    expect(SOURCE_COMMIT_PATTERN.fullmatch(source_commit) is not None, "$.source_commit must be a full lowercase git SHA")
    blockers = require_string_list(payload, "blockers", "$")
    withheld_claims = require_string_list(payload, "withheld_claims", "$")
    expect(withheld_claims, "$.withheld_claims must be non-empty")

    assert_readiness_schema(require_object(payload.get("readiness"), "$.readiness"), blockers, status)
    assert_inventory_schema(require_object(payload.get("inventory"), "$.inventory"))
    assert_performance_schema(require_object(payload.get("performance"), "$.performance"))
    assert_amflow_example_coverage_schema(
        require_object(payload.get("amflow_example_coverage"), "$.amflow_example_coverage")
    )


def parse_json_object(raw_output: str) -> dict[str, Any]:
    try:
        payload = json.loads(raw_output)
    except json.JSONDecodeError as error:
        raise SchemaGateError(f"release health JSON output is not valid JSON: {error}") from error
    return require_object(payload, "$")


def run_release_health_json(root: Path) -> dict[str, Any]:
    completed = subprocess.run(
        [
            sys.executable,
            str(root / "tools/reference-harness/scripts/release_health_summary.py"),
            "--verify",
            "--format",
            "json",
        ],
        cwd=root,
        text=True,
        capture_output=True,
        check=False,
    )
    if completed.returncode != 0:
        details = completed.stderr.strip() or completed.stdout.strip()
        raise SchemaGateError(f"release_health_summary.py JSON mode failed: {details}")
    return parse_json_object(completed.stdout)


def assert_rejected(label: str, payload: dict[str, Any], expected_message: str) -> bool:
    try:
        assert_release_health_schema(payload)
    except SchemaGateError as error:
        return expected_message in str(error)
    raise SchemaGateError(f"{label} unexpectedly passed")


def with_payload_mutation(
    payload: dict[str, Any],
    mutation: Callable[[dict[str, Any]], None],
) -> dict[str, Any]:
    mutated = copy.deepcopy(payload)
    mutation(mutated)
    return mutated


def run_schema_gate(root: Path) -> int:
    try:
        payload = run_release_health_json(root)
        assert_release_health_schema(payload)
    except SchemaGateError as error:
        print(f"M7 release health JSON schema gate failed: {error}", file=sys.stderr)
        return 1

    print("M7 release health JSON schema gate passed")
    return 0


def run_self_check(root: Path) -> int:
    try:
        payload = run_release_health_json(root)
        assert_release_health_schema(payload)
        checks = {
            "accepts_current_output_schema": True,
            "rejects_missing_required_key": assert_rejected(
                "missing readiness blocker_count",
                with_payload_mutation(
                    payload,
                    lambda item: item["readiness"].pop("blocker_count"),
                ),
                "$.readiness missing required key(s): blocker_count",
            ),
            "rejects_unexpected_key": assert_rejected(
                "unexpected top-level key",
                with_payload_mutation(
                    payload,
                    lambda item: item.__setitem__("synthetic_extra_key", True),
                ),
                "$ has unexpected key(s): synthetic_extra_key",
            ),
            "rejects_wrong_type": assert_rejected(
                "wrong blocker_count type",
                with_payload_mutation(
                    payload,
                    lambda item: item["readiness"].__setitem__("blocker_count", "0"),
                ),
                "$.readiness.blocker_count must be an integer",
            ),
            "rejects_bool_as_integer": assert_rejected(
                "bool schema_version type",
                with_payload_mutation(
                    payload,
                    lambda item: item.__setitem__("schema_version", True),
                ),
                "$.schema_version must be an integer",
            ),
            "rejects_status_enum_drift": assert_rejected(
                "status enum drift",
                with_payload_mutation(
                    payload,
                    lambda item: item.__setitem__("status", "green"),
                ),
                "$.status must be one of: blocked, ready",
            ),
            "rejects_negative_count": assert_rejected(
                "negative benchmark count",
                with_payload_mutation(
                    payload,
                    lambda item: item["performance"].__setitem__("benchmark_count", -1),
                ),
                "$.performance.benchmark_count must be nonnegative",
            ),
            "rejects_count_list_mismatch": assert_rejected(
                "not-full runtime count mismatch",
                with_payload_mutation(
                    payload,
                    lambda item: item["amflow_example_coverage"].__setitem__(
                        "not_full_runtime_count",
                        item["amflow_example_coverage"]["not_full_runtime_count"] + 1,
                    ),
                ),
                "$.amflow_example_coverage.not_full_runtime_count must match not_full_runtime_examples length",
            ),
            "rejects_bad_status_count_key": assert_rejected(
                "bad AMFlow status count key",
                with_payload_mutation(
                    payload,
                    lambda item: item["amflow_example_coverage"]["status_counts"].__setitem__(
                        "synthetic-status",
                        1,
                    ),
                ),
                "$.amflow_example_coverage.status_counts has unknown status: synthetic-status",
            ),
            "rejects_bad_live_reverified_date": assert_rejected(
                "bad live reverification date",
                with_payload_mutation(
                    payload,
                    lambda item: item["amflow_example_coverage"]["live_reverified_examples"][0].__setitem__(
                        "date",
                        "06/12/2026",
                    ),
                ),
                "$.amflow_example_coverage.live_reverified_examples[0].date must be YYYY-MM-DD",
            ),
        }
    except (IndexError, KeyError, SchemaGateError) as error:
        print(f"M7 release health JSON schema self-check failed: {error}", file=sys.stderr)
        return 1

    if not all(checks.values()):
        print(
            json.dumps(
                {"schema_version": 1, "self_check_passed": False, "checks": checks},
                indent=2,
                sort_keys=True,
            ),
            file=sys.stderr,
        )
        return 1

    print(
        json.dumps(
            {"schema_version": 1, "self_check_passed": True, "checks": checks},
            indent=2,
            sort_keys=True,
        )
    )
    return 0


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--self-check",
        action="store_true",
        help="Run synthetic schema-gate checks for missing keys, type drift, enum drift, and range drift.",
    )
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    root = repo_root()
    if args.self_check:
        return run_self_check(root)
    return run_schema_gate(root)


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
