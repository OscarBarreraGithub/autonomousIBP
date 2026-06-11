#!/usr/bin/env python3
"""Render a Shields-compatible M7 release status badge from release health."""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path
from typing import Any


HEALTH_SUMMARY_SCRIPT = Path("tools/reference-harness/scripts/release_health_summary.py")
SOURCE_COMMIT_PATTERN = re.compile(r"^[0-9a-f]{40}$")


class BadgeError(RuntimeError):
    """Raised when release health cannot be rendered as a badge."""


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def read_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as stream:
        payload = json.load(stream)
    if not isinstance(payload, dict):
        raise BadgeError(f"{path} must contain a JSON object")
    return payload


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise BadgeError(message)


def require_repo_file(root: Path, raw: Path, field: str) -> Path:
    candidate = raw
    if not candidate.is_absolute():
        candidate = root / candidate
    try:
        relative = candidate.resolve(strict=False).relative_to(root.resolve(strict=True))
    except ValueError as error:
        raise BadgeError(f"{field} must stay within the repository: {raw}") from error
    path = root / relative
    expect(path.is_file(), f"{field} does not exist as a file: {raw}")
    return path


def require_object(payload: dict[str, Any], field: str) -> dict[str, Any]:
    value = payload.get(field)
    expect(isinstance(value, dict), f"{field} must be an object")
    return value


def require_int(payload: dict[str, Any], field: str) -> int:
    value = payload.get(field)
    expect(type(value) is int and value >= 0, f"{field} must be a nonnegative integer")
    return value


def require_positive_int(payload: dict[str, Any], field: str, label: str | None = None) -> int:
    value = payload.get(field)
    diagnostic = label or field
    expect(type(value) is int and value > 0, f"{diagnostic} must be a positive integer")
    return value


def require_nonnegative_number(payload: dict[str, Any], field: str, label: str | None = None) -> float:
    value = payload.get(field)
    diagnostic = label or field
    expect(type(value) in {int, float} and value >= 0, f"{diagnostic} must be a nonnegative number")
    return float(value)


def require_nonempty_string(payload: dict[str, Any], field: str, label: str | None = None) -> str:
    value = payload.get(field)
    diagnostic = label or field
    expect(isinstance(value, str) and value.strip(), f"{diagnostic} must be a non-empty string")
    return value.strip()


def require_string_list(payload: dict[str, Any], field: str) -> list[str]:
    value = payload.get(field)
    expect(isinstance(value, list), f"{field} must be a list")
    values: list[str] = []
    for index, item in enumerate(value):
        expect(isinstance(item, str), f"{field}[{index}] must be a string")
        values.append(item)
    return values


def require_nonempty_string_list(payload: dict[str, Any], field: str) -> list[str]:
    values = require_string_list(payload, field)
    expect(values, f"{field} must be a non-empty list")
    for index, item in enumerate(values):
        expect(item.strip(), f"{field}[{index}] must be non-empty")
        expect(item == item.strip(), f"{field}[{index}] must not carry surrounding whitespace")
    return values


def require_source_commit(payload: dict[str, Any]) -> str:
    source_commit = require_nonempty_string(payload, "source_commit")
    expect(
        SOURCE_COMMIT_PATTERN.fullmatch(source_commit) is not None,
        "source_commit must be a full lowercase 40-character git SHA",
    )
    return source_commit


def validate_performance(health: dict[str, Any]) -> None:
    performance = require_object(health, "performance")
    expect(
        performance.get("review_complete") is True,
        "performance.review_complete must be true",
    )
    benchmark_count = require_positive_int(performance, "benchmark_count")
    run_count = require_positive_int(performance, "run_count")
    max_wall_seconds = require_nonnegative_number(performance, "max_wall_seconds")
    max_rss_kb = require_int(performance, "max_rss_kb")
    benchmarks = performance.get("benchmarks")
    expect(isinstance(benchmarks, list), "performance.benchmarks must be a list")
    expect(
        len(benchmarks) == benchmark_count,
        "performance benchmark count does not match performance.benchmarks length",
    )
    total_runs = 0
    max_benchmark_wall = 0.0
    max_benchmark_rss = 0
    seen_labels: set[str] = set()
    for index, benchmark in enumerate(benchmarks):
        expect(isinstance(benchmark, dict), f"performance.benchmarks[{index}] must be an object")
        label = require_nonempty_string(
            benchmark,
            "label",
            f"performance.benchmarks[{index}].label",
        )
        expect(label not in seen_labels, f"duplicate performance benchmark label: {label}")
        seen_labels.add(label)
        benchmark_runs = require_positive_int(
            benchmark,
            "run_count",
            f"performance.benchmarks[{index}].run_count",
        )
        benchmark_wall = require_nonnegative_number(
            benchmark,
            "max_wall_seconds",
            f"performance.benchmarks[{index}].max_wall_seconds",
        )
        benchmark_rss = require_int(benchmark, "max_rss_kb")
        total_runs += benchmark_runs
        max_benchmark_wall = max(max_benchmark_wall, benchmark_wall)
        max_benchmark_rss = max(max_benchmark_rss, benchmark_rss)
    expect(total_runs == run_count, "performance run_count does not match benchmark run totals")
    expect(
        max_benchmark_wall == max_wall_seconds,
        "performance max_wall_seconds does not match benchmark maximum",
    )
    expect(
        max_benchmark_rss == max_rss_kb,
        "performance max_rss_kb does not match benchmark maximum",
    )


def run_release_health(root: Path, *, verify: bool) -> dict[str, Any]:
    command = [
        sys.executable,
        str(root / HEALTH_SUMMARY_SCRIPT),
        "--format",
        "json",
    ]
    if verify:
        command.append("--verify")
    completed = subprocess.run(
        command,
        cwd=root,
        text=True,
        capture_output=True,
        check=False,
    )
    if completed.returncode != 0:
        details = completed.stderr.strip() or completed.stdout.strip()
        raise BadgeError(f"release health summary failed: {details}")
    try:
        payload = json.loads(completed.stdout)
    except json.JSONDecodeError as error:
        raise BadgeError(f"release health summary did not emit JSON: {error}") from error
    if not isinstance(payload, dict):
        raise BadgeError("release health summary JSON must be an object")
    return payload


def badge_payload(health: dict[str, Any]) -> dict[str, Any]:
    expect(health.get("schema_version") == 1, "release health schema_version must be 1")
    status = health.get("status")
    expect(status in {"ready", "blocked"}, "release health status must be ready or blocked")
    require_source_commit(health)
    require_nonempty_string_list(health, "withheld_claims")

    readiness = require_object(health, "readiness")
    inventory = require_object(health, "inventory")
    ready = readiness.get("release_signoff_ready")
    expect(isinstance(ready, bool), "readiness.release_signoff_ready must be boolean")
    blocker_count = require_int(readiness, "blocker_count")
    blockers = require_string_list(health, "blockers")
    expect(
        len(blockers) == blocker_count,
        "health blockers length does not match readiness.blocker_count",
    )
    expected_status = "ready" if ready and blocker_count == 0 else "blocked"
    expect(
        status == expected_status,
        (
            "release health status is inconsistent with readiness fields: "
            f"status={status} expected={expected_status}"
        ),
    )
    sidecar_count = require_int(inventory, "sidecar_count")
    accepted_count = require_int(inventory, "accepted_count")
    unaccepted_count = require_int(inventory, "unaccepted_count")
    expect(
        inventory.get("schema_reconciled") is True,
        "inventory.schema_reconciled must be true",
    )
    validate_performance(health)
    expect(accepted_count <= sidecar_count, "inventory.accepted_count exceeds sidecar_count")
    expect(
        accepted_count + unaccepted_count == sidecar_count,
        "inventory accepted/unaccepted counts do not sum to sidecar_count",
    )

    if status == "ready" and ready and blocker_count == 0:
        message = f"ready; {accepted_count}/{sidecar_count} accepted"
        color = "brightgreen"
    else:
        message = f"blocked; {blocker_count} blockers; {unaccepted_count} unaccepted"
        color = "red"

    return {
        "schemaVersion": 1,
        "label": "M7 release",
        "message": message,
        "color": color,
    }


def render_shields_json(payload: dict[str, Any]) -> str:
    return json.dumps(payload, indent=2, sort_keys=True)


def synthetic_health(
    *,
    status: str,
    ready: bool,
    blocker_count: int,
    sidecar_count: int,
    accepted_count: int,
    unaccepted_count: int,
    schema_reconciled: bool = True,
    performance_complete: bool = True,
) -> dict[str, Any]:
    return {
        "schema_version": 1,
        "source_commit": "a" * 40,
        "status": status,
        "blockers": [f"blocker-{index}" for index in range(blocker_count)],
        "withheld_claims": ["Synthetic badge health does not create release evidence."],
        "readiness": {
            "release_signoff_ready": ready,
            "blocker_count": blocker_count,
        },
        "inventory": {
            "sidecar_count": sidecar_count,
            "accepted_count": accepted_count,
            "unaccepted_count": unaccepted_count,
            "schema_reconciled": schema_reconciled,
        },
        "performance": {
            "review_complete": performance_complete,
            "benchmark_count": 1,
            "run_count": 3,
            "max_wall_seconds": 1.25,
            "max_rss_kb": 2048,
            "benchmarks": [
                {
                    "label": "synthetic.release-badge",
                    "run_count": 3,
                    "max_wall_seconds": 1.25,
                    "max_rss_kb": 2048,
                }
            ],
        },
    }


def expect_badge_error(label: str, health: dict[str, Any], expected: str) -> None:
    try:
        badge_payload(health)
    except BadgeError as error:
        expect(expected in str(error), f"{label} failed for the wrong reason: {error}")
        return
    raise BadgeError(f"{label} unexpectedly passed")


def expect_path_error(label: str, root: Path, raw: Path, expected: str) -> None:
    try:
        require_repo_file(root, raw, "health JSON")
    except BadgeError as error:
        expect(expected in str(error), f"{label} failed for the wrong reason: {error}")
        return
    raise BadgeError(f"{label} unexpectedly passed")


def self_check() -> None:
    ready_badge = badge_payload(
        synthetic_health(
            status="ready",
            ready=True,
            blocker_count=0,
            sidecar_count=77,
            accepted_count=56,
            unaccepted_count=21,
        )
    )
    expect(
        ready_badge
        == {
            "schemaVersion": 1,
            "label": "M7 release",
            "message": "ready; 56/77 accepted",
            "color": "brightgreen",
        },
        "ready badge payload drifted",
    )

    blocked_badge = badge_payload(
        synthetic_health(
            status="blocked",
            ready=False,
            blocker_count=2,
            sidecar_count=11,
            accepted_count=4,
            unaccepted_count=7,
        )
    )
    expect(
        blocked_badge
        == {
            "schemaVersion": 1,
            "label": "M7 release",
            "message": "blocked; 2 blockers; 7 unaccepted",
            "color": "red",
        },
        "blocked badge payload drifted",
    )

    expect_badge_error(
        "inconsistent inventory count check",
        synthetic_health(
            status="ready",
            ready=True,
            blocker_count=0,
            sidecar_count=10,
            accepted_count=8,
            unaccepted_count=1,
        ),
        "inventory accepted/unaccepted counts do not sum",
    )
    expect_badge_error(
        "status/readiness coherence check",
        synthetic_health(
            status="ready",
            ready=False,
            blocker_count=0,
            sidecar_count=10,
            accepted_count=8,
            unaccepted_count=2,
        ),
        "status is inconsistent with readiness fields",
    )
    expect_badge_error(
        "inventory schema reconciliation check",
        synthetic_health(
            status="ready",
            ready=True,
            blocker_count=0,
            sidecar_count=10,
            accepted_count=8,
            unaccepted_count=2,
            schema_reconciled=False,
        ),
        "inventory.schema_reconciled must be true",
    )
    mismatched_blockers = synthetic_health(
        status="blocked",
        ready=False,
        blocker_count=2,
        sidecar_count=10,
        accepted_count=8,
        unaccepted_count=2,
    )
    mismatched_blockers["blockers"] = ["single-blocker"]
    expect_badge_error(
        "blocker list/count coherence check",
        mismatched_blockers,
        "blockers length does not match",
    )
    malformed_blockers = synthetic_health(
        status="blocked",
        ready=False,
        blocker_count=1,
        sidecar_count=10,
        accepted_count=8,
        unaccepted_count=2,
    )
    malformed_blockers["blockers"] = [42]
    expect_badge_error(
        "blocker list type check",
        malformed_blockers,
        "blockers[0] must be a string",
    )
    missing_source_commit = synthetic_health(
        status="ready",
        ready=True,
        blocker_count=0,
        sidecar_count=10,
        accepted_count=8,
        unaccepted_count=2,
    )
    missing_source_commit.pop("source_commit")
    expect_badge_error(
        "missing source commit check",
        missing_source_commit,
        "source_commit must be a non-empty string",
    )
    short_source_commit = synthetic_health(
        status="ready",
        ready=True,
        blocker_count=0,
        sidecar_count=10,
        accepted_count=8,
        unaccepted_count=2,
    )
    short_source_commit["source_commit"] = "5fdba2c"
    expect_badge_error(
        "short source commit check",
        short_source_commit,
        "source_commit must be a full lowercase 40-character git SHA",
    )
    missing_withheld_claims = synthetic_health(
        status="ready",
        ready=True,
        blocker_count=0,
        sidecar_count=10,
        accepted_count=8,
        unaccepted_count=2,
    )
    missing_withheld_claims.pop("withheld_claims")
    expect_badge_error(
        "missing withheld claims check",
        missing_withheld_claims,
        "withheld_claims must be a list",
    )
    empty_withheld_claims = synthetic_health(
        status="ready",
        ready=True,
        blocker_count=0,
        sidecar_count=10,
        accepted_count=8,
        unaccepted_count=2,
    )
    empty_withheld_claims["withheld_claims"] = []
    expect_badge_error(
        "empty withheld claims check",
        empty_withheld_claims,
        "withheld_claims must be a non-empty list",
    )
    missing_performance = synthetic_health(
        status="ready",
        ready=True,
        blocker_count=0,
        sidecar_count=10,
        accepted_count=8,
        unaccepted_count=2,
    )
    missing_performance.pop("performance")
    expect_badge_error(
        "missing performance summary check",
        missing_performance,
        "performance must be an object",
    )
    expect_badge_error(
        "incomplete performance review check",
        synthetic_health(
            status="ready",
            ready=True,
            blocker_count=0,
            sidecar_count=10,
            accepted_count=8,
            unaccepted_count=2,
            performance_complete=False,
        ),
        "performance.review_complete must be true",
    )
    mismatched_performance = synthetic_health(
        status="ready",
        ready=True,
        blocker_count=0,
        sidecar_count=10,
        accepted_count=8,
        unaccepted_count=2,
    )
    mismatched_performance["performance"]["benchmarks"][0]["run_count"] = 2
    expect_badge_error(
        "performance run-count coherence check",
        mismatched_performance,
        "performance run_count does not match benchmark run totals",
    )

    root = repo_root()
    fixture_path = Path("tools/reference-harness/specs/release/m7-release-health-summary.fixture.json")
    fixture_absolute = root / fixture_path
    expect(
        require_repo_file(root, fixture_path, "health JSON").resolve(strict=True)
        == fixture_absolute.resolve(strict=True),
        "relative health JSON path did not normalize to the committed fixture",
    )
    expect(
        require_repo_file(root, fixture_absolute, "health JSON").resolve(strict=True)
        == fixture_absolute.resolve(strict=True),
        "absolute health JSON path did not normalize to the committed fixture",
    )
    expect_path_error(
        "health JSON repository escape guard",
        root,
        root.parent / "m7-release-health-summary.fixture.json",
        "health JSON must stay within the repository",
    )
    expect_path_error(
        "health JSON missing-file guard",
        root,
        Path("tools/reference-harness/specs/release/missing-health-summary.json"),
        "health JSON does not exist as a file",
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--health-json",
        type=Path,
        help="Existing repository-local release_health_summary.py JSON output to render instead of running it.",
    )
    parser.add_argument(
        "--format",
        choices=("shields-json",),
        default="shields-json",
        help="Badge output format.",
    )
    parser.add_argument(
        "--verify",
        action="store_true",
        help="Pass --verify through to release_health_summary.py when reading live health.",
    )
    parser.add_argument(
        "--self-check",
        action="store_true",
        help="Run synthetic badge rendering checks.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        if args.self_check:
            self_check()
            print("M7 release status badge self-check passed")
            return 0

        root = repo_root()
        if args.health_json is None:
            health = run_release_health(root, verify=args.verify)
        else:
            health_path = require_repo_file(root, args.health_json, "health JSON")
            health = read_json(health_path)

        print(render_shields_json(badge_payload(health)))
        return 0
    except BadgeError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
