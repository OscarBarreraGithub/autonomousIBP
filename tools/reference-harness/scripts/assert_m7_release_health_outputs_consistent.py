#!/usr/bin/env python3
"""CTest gate for mutual consistency of M7 release health outputs."""

from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path
from typing import Any


class ConsistencyError(RuntimeError):
    """Raised when release health outputs disagree."""


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise ConsistencyError(message)


def run_tool(root: Path, args: list[str], label: str) -> str:
    completed = subprocess.run(
        [sys.executable, *args],
        cwd=root,
        text=True,
        capture_output=True,
        check=False,
    )
    if completed.returncode != 0:
        details = completed.stderr.strip() or completed.stdout.strip()
        raise ConsistencyError(f"{label} failed: {details}")
    return completed.stdout


def parse_json_object(raw: str, label: str) -> dict[str, Any]:
    try:
        payload = json.loads(raw)
    except json.JSONDecodeError as error:
        raise ConsistencyError(f"{label} did not emit valid JSON: {error}") from error
    expect(isinstance(payload, dict), f"{label} must emit a JSON object")
    return payload


def required_line(lines: list[str], prefix: str) -> str:
    for line in lines:
        if line.startswith(prefix):
            return line[len(prefix) :]
    raise ConsistencyError(f"text output missing {prefix!r} line")


def key_values(raw: str, field: str) -> dict[str, str]:
    values: dict[str, str] = {}
    for token in raw.split():
        expect("=" in token, f"{field} contains non key=value token: {token}")
        key, value = token.split("=", 1)
        expect(key not in values, f"{field} repeats key: {key}")
        values[key] = value
    return values


def bool_token(value: bool) -> str:
    return "true" if value else "false"


def fraction_token(numerator: int, denominator: int) -> str:
    return f"{numerator}/{denominator}"


def expect_nonnegative_int(payload: dict[str, Any], field: str) -> int:
    value = payload.get(field)
    expect(type(value) is int and value >= 0, f"{field} must be a nonnegative integer")
    return value


def expect_bool(payload: dict[str, Any], field: str) -> bool:
    value = payload.get(field)
    expect(isinstance(value, bool), f"{field} must be boolean")
    return value


def expect_object(payload: dict[str, Any], field: str) -> dict[str, Any]:
    value = payload.get(field)
    expect(isinstance(value, dict), f"{field} must be an object")
    return value


def expect_string_list(payload: dict[str, Any], field: str) -> list[str]:
    value = payload.get(field)
    expect(isinstance(value, list), f"{field} must be a list")
    expect(all(isinstance(item, str) for item in value), f"{field} must contain strings")
    return value


def assert_amflow_example_coverage_matches_text(lines: list[str], health: dict[str, Any]) -> None:
    coverage = expect_object(health, "amflow_example_coverage")
    coverage_text = key_values(
        required_line(lines, "amflow_example_coverage: "),
        "amflow_example_coverage",
    )
    expected_keys = {
        "total",
        "full_compared_output",
        "live_retained_golden",
        "partial",
        "not_full_runtime",
        "known_gap_rows",
        "zero_evidence",
    }
    expect(
        set(coverage_text) == expected_keys,
        "text AMFlow example coverage keys drifted",
    )
    field_mapping = {
        "total": "total_example_count",
        "full_compared_output": "full_compared_output_count",
        "live_retained_golden": "live_retained_golden_count",
        "partial": "partial_retained_or_selected_count",
        "not_full_runtime": "not_full_runtime_count",
        "known_gap_rows": "documented_gap_count",
        "zero_evidence": "zero_evidence_count",
    }
    for text_key, json_key in field_mapping.items():
        expect(
            coverage_text.get(text_key) == str(expect_nonnegative_int(coverage, json_key)),
            f"text/json AMFlow example coverage {json_key} mismatch",
        )

    not_full_examples = expect_string_list(coverage, "not_full_runtime_examples")
    expect(
        len(not_full_examples) == expect_nonnegative_int(coverage, "not_full_runtime_count"),
        "json AMFlow not_full_runtime_count does not match example list length",
    )
    expect(
        len(not_full_examples) == expect_nonnegative_int(coverage, "documented_gap_count"),
        "json AMFlow documented_gap_count does not match example list length",
    )
    expect(
        required_line(lines, "not_full_runtime_examples: ") == "; ".join(not_full_examples),
        "text/json AMFlow not_full_runtime_examples mismatch",
    )

    status_counts = coverage.get("status_counts")
    expect(isinstance(status_counts, dict), "amflow_example_coverage.status_counts must be an object")
    for status, count in status_counts.items():
        expect(isinstance(status, str) and status, "AMFlow status_counts keys must be non-empty strings")
        expect(type(count) is int and count >= 0, f"AMFlow status count must be nonnegative: {status}")
    expect(
        sum(status_counts.values()) == expect_nonnegative_int(coverage, "total_example_count"),
        "AMFlow status_counts do not sum to total_example_count",
    )
    expect(
        status_counts.get("reproduced-fully", 0)
        == expect_nonnegative_int(coverage, "full_compared_output_count"),
        "AMFlow reproduced-fully count mismatch",
    )
    expect(
        status_counts.get("reproduced-fully-live", 0)
        == expect_nonnegative_int(coverage, "live_retained_golden_count"),
        "AMFlow reproduced-fully-live count mismatch",
    )
    expect(
        status_counts.get("reproduced-partial", 0)
        == expect_nonnegative_int(coverage, "partial_retained_or_selected_count"),
        "AMFlow reproduced-partial count mismatch",
    )


def assert_text_matches_json(text: str, health: dict[str, Any]) -> None:
    lines = text.splitlines()
    expect(lines[:1] == ["M7 release health summary"], "text header drifted")

    status = health.get("status")
    expect(status in {"ready", "blocked"}, "health status must be ready or blocked")
    expect(required_line(lines, "status: ") == str(status).upper(), "text/json status mismatch")

    source_commit = health.get("source_commit")
    expect(isinstance(source_commit, str) and source_commit, "source_commit must be non-empty")
    expect(required_line(lines, "source_commit: ") == source_commit, "text/json source_commit mismatch")

    readiness = expect_object(health, "readiness")
    readiness_text = key_values(required_line(lines, "readiness: "), "readiness")
    expect(
        readiness_text.get("release_signoff_ready")
        == bool_token(expect_bool(readiness, "release_signoff_ready")),
        "text/json release_signoff_ready mismatch",
    )
    expect(
        readiness_text.get("blockers") == str(expect_nonnegative_int(readiness, "blocker_count")),
        "text/json blocker_count mismatch",
    )
    expect(
        readiness_text.get("prerequisites")
        == fraction_token(
            expect_nonnegative_int(readiness, "satisfied_prerequisite_count"),
            expect_nonnegative_int(readiness, "prerequisite_count"),
        ),
        "text/json prerequisite count mismatch",
    )
    expect(
        readiness_text.get("review_sections")
        == fraction_token(
            expect_nonnegative_int(readiness, "reviewed_section_count"),
            expect_nonnegative_int(readiness, "review_section_count"),
        ),
        "text/json review section count mismatch",
    )

    inventory = expect_object(health, "inventory")
    inventory_text = key_values(required_line(lines, "inventory: "), "inventory")
    sidecar_count = expect_nonnegative_int(inventory, "sidecar_count")
    accepted_count = expect_nonnegative_int(inventory, "accepted_count")
    unaccepted_count = expect_nonnegative_int(inventory, "unaccepted_count")
    expect(inventory_text.get("total") == str(sidecar_count), "text/json sidecar total mismatch")
    expect(inventory_text.get("accepted") == str(accepted_count), "text/json accepted count mismatch")
    expect(inventory_text.get("unaccepted") == str(unaccepted_count), "text/json unaccepted count mismatch")
    expect(inventory.get("schema_reconciled") is True, "json inventory.schema_reconciled must be true")
    expect(inventory_text.get("schema_reconciled") == "true", "text inventory schema reconciliation mismatch")

    performance = expect_object(health, "performance")
    performance_text = key_values(required_line(lines, "performance: "), "performance")
    expect(
        performance_text.get("review_complete") == bool_token(expect_bool(performance, "review_complete")),
        "text/json performance review_complete mismatch",
    )
    expect(
        performance_text.get("benchmarks") == str(expect_nonnegative_int(performance, "benchmark_count")),
        "text/json performance benchmark count mismatch",
    )
    expect(
        performance_text.get("runs") == str(expect_nonnegative_int(performance, "run_count")),
        "text/json performance run count mismatch",
    )
    max_wall_seconds = performance.get("max_wall_seconds")
    expect(type(max_wall_seconds) in {int, float}, "performance.max_wall_seconds must be numeric")
    expect(
        performance_text.get("max_wall_seconds") == f"{float(max_wall_seconds):.2f}",
        "text/json performance max_wall_seconds mismatch",
    )
    expect(
        performance_text.get("max_rss_kb") == str(expect_nonnegative_int(performance, "max_rss_kb")),
        "text/json performance max_rss_kb mismatch",
    )

    raw_benchmarks = performance.get("benchmarks")
    expect(isinstance(raw_benchmarks, list), "performance.benchmarks must be a list")
    expected_benchmarks: list[str] = []
    for index, benchmark in enumerate(raw_benchmarks):
        expect(isinstance(benchmark, dict), f"performance.benchmarks[{index}] must be an object")
        label = benchmark.get("label")
        max_seconds = benchmark.get("max_wall_seconds")
        expect(isinstance(label, str) and label, f"performance.benchmarks[{index}].label missing")
        expect(type(max_seconds) in {int, float}, f"performance.benchmarks[{index}].max_wall_seconds missing")
        expected_benchmarks.append(f"{label} max={float(max_seconds):.2f}s")
    expect(
        required_line(lines, "performance_benchmarks: ") == "; ".join(expected_benchmarks),
        "text/json performance benchmark list mismatch",
    )

    assert_amflow_example_coverage_matches_text(lines, health)

    blockers = health.get("blockers")
    expect(isinstance(blockers, list), "health blockers must be a list")
    expect(all(isinstance(blocker, str) for blocker in blockers), "health blockers must be strings")
    expected_blockers = "none" if not blockers else "; ".join(blockers)
    expect(required_line(lines, "blockers: ") == expected_blockers, "text/json blockers mismatch")

    withheld_claims = health.get("withheld_claims")
    expect(isinstance(withheld_claims, list), "withheld_claims must be a list")
    expect(all(isinstance(claim, str) for claim in withheld_claims), "withheld_claims must be strings")
    expect(
        required_line(lines, "withheld_claims: ") == " ".join(withheld_claims),
        "text/json withheld_claims mismatch",
    )


def assert_badge_matches_json(badge: dict[str, Any], health: dict[str, Any]) -> None:
    readiness = expect_object(health, "readiness")
    inventory = expect_object(health, "inventory")
    status = health.get("status")
    ready = expect_bool(readiness, "release_signoff_ready")
    blocker_count = expect_nonnegative_int(readiness, "blocker_count")
    sidecar_count = expect_nonnegative_int(inventory, "sidecar_count")
    accepted_count = expect_nonnegative_int(inventory, "accepted_count")
    unaccepted_count = expect_nonnegative_int(inventory, "unaccepted_count")
    expect(accepted_count + unaccepted_count == sidecar_count, "json inventory counts do not sum")

    if status == "ready" and ready and blocker_count == 0:
        expected_message = f"ready; {accepted_count}/{sidecar_count} accepted"
        expected_color = "brightgreen"
    else:
        expected_message = f"blocked; {blocker_count} blockers; {unaccepted_count} unaccepted"
        expected_color = "red"

    expect(badge.get("schemaVersion") == 1, "badge schemaVersion mismatch")
    expect(badge.get("label") == "M7 release", "badge label mismatch")
    expect(badge.get("message") == expected_message, "badge/json message mismatch")
    expect(badge.get("color") == expected_color, "badge/json color mismatch")


def main() -> int:
    root = repo_root()
    health_script = "tools/reference-harness/scripts/release_health_summary.py"
    badge_script = "tools/reference-harness/scripts/release_status_badge.py"
    try:
        text = run_tool(root, [health_script, "--verify", "--format", "text"], "release health text")
        health = parse_json_object(
            run_tool(root, [health_script, "--verify", "--format", "json"], "release health JSON"),
            "release health JSON",
        )
        badge = parse_json_object(
            run_tool(root, [badge_script, "--verify"], "release status badge"),
            "release status badge",
        )
        assert_text_matches_json(text, health)
        assert_badge_matches_json(badge, health)
    except ConsistencyError as error:
        print(f"M7 release health output consistency failed: {error}", file=sys.stderr)
        return 1

    print("M7 release health text/json/badge consistency gate passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
