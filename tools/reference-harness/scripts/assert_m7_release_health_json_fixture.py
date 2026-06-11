#!/usr/bin/env python3
"""CTest gate for the M7 release health JSON output contract."""

from __future__ import annotations

import argparse
import copy
import difflib
import json
import subprocess
import sys
from pathlib import Path
from typing import Any, Callable


EXPECTED_JSON_FIXTURE = Path(
    "tools/reference-harness/specs/release/m7-release-health-summary.fixture.json"
)


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def read_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as stream:
        payload = json.load(stream)
    if not isinstance(payload, dict):
        raise RuntimeError(f"{path} must contain a JSON object")
    return payload


def canonical_json(payload: dict[str, Any]) -> str:
    return json.dumps(payload, indent=2, sort_keys=True) + "\n"


def print_json_diff(expected: dict[str, Any], actual: dict[str, Any]) -> None:
    diff = difflib.unified_diff(
        canonical_json(expected).splitlines(keepends=True),
        canonical_json(actual).splitlines(keepends=True),
        fromfile=str(EXPECTED_JSON_FIXTURE),
        tofile="release_health_summary.py --verify --format json",
        n=3,
    )
    print("M7 release health JSON fixture drifted", file=sys.stderr)
    print("".join(diff), file=sys.stderr)


def parse_summary_json(raw_output: str) -> dict[str, Any]:
    try:
        actual = json.loads(raw_output)
    except json.JSONDecodeError as error:
        raise RuntimeError(f"release health JSON output is not valid JSON: {error}") from error
    if not isinstance(actual, dict):
        raise RuntimeError("release health JSON output must be a JSON object")
    return actual


def fixture_matches(expected: dict[str, Any], actual: dict[str, Any]) -> bool:
    return actual == expected


def run_fixture_gate(root: Path) -> int:
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
        print("release_health_summary.py JSON mode failed", file=sys.stderr)
        if completed.stdout:
            print("stdout:", file=sys.stderr)
            print(completed.stdout, file=sys.stderr)
        if completed.stderr:
            print("stderr:", file=sys.stderr)
            print(completed.stderr, file=sys.stderr)
        return completed.returncode

    try:
        actual = parse_summary_json(completed.stdout)
    except RuntimeError as error:
        print(error, file=sys.stderr)
        print(completed.stdout, file=sys.stderr)
        return 1

    expected = read_json(root / EXPECTED_JSON_FIXTURE)
    if not fixture_matches(expected, actual):
        print_json_diff(expected, actual)
        return 1

    print("M7 release health JSON fixture gate passed")
    return 0


def rejected(check: Callable[[], Any], expected_message: str) -> bool:
    try:
        check()
    except Exception as error:  # noqa: BLE001 - self-check intentionally probes failures.
        return expected_message in str(error)
    return False


def run_self_check(root: Path) -> int:
    expected = read_json(root / EXPECTED_JSON_FIXTURE)
    status_drift = copy.deepcopy(expected)
    status_drift["status"] = "blocked"

    readiness_drift = copy.deepcopy(expected)
    readiness_drift["readiness"]["release_signoff_ready"] = False

    inventory_drift = copy.deepcopy(expected)
    inventory_drift["inventory"]["accepted_count"] -= 1

    performance_drift = copy.deepcopy(expected)
    performance_drift["performance"]["benchmarks"][0]["max_wall_seconds"] += 0.01

    missing_withheld_claims = copy.deepcopy(expected)
    missing_withheld_claims.pop("withheld_claims", None)

    checks = {
        "accepts_current_fixture": fixture_matches(expected, copy.deepcopy(expected)),
        "rejects_status_drift": not fixture_matches(expected, status_drift),
        "rejects_readiness_drift": not fixture_matches(expected, readiness_drift),
        "rejects_inventory_drift": not fixture_matches(expected, inventory_drift),
        "rejects_performance_drift": not fixture_matches(expected, performance_drift),
        "rejects_missing_withheld_claims": not fixture_matches(
            expected,
            missing_withheld_claims,
        ),
        "rejects_invalid_json": rejected(
            lambda: parse_summary_json("{"),
            "not valid JSON",
        ),
        "rejects_non_object_json": rejected(
            lambda: parse_summary_json("[]"),
            "must be a JSON object",
        ),
    }
    if not all(checks.values()):
        print(
            json.dumps(
                {"self_check_passed": False, "checks": checks},
                indent=2,
                sort_keys=True,
            )
        )
        return 1

    print(
        json.dumps(
            {
                "schema_version": 1,
                "fixture": str(EXPECTED_JSON_FIXTURE),
                "self_check_passed": True,
                "checks": checks,
            },
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
        help="Run synthetic fixture-gate checks",
    )
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    root = repo_root()
    if args.self_check:
        return run_self_check(root)
    return run_fixture_gate(root)


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
