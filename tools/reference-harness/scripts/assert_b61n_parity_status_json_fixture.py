#!/usr/bin/env python3
"""CTest gate for the pinned b61n parity status JSON contract."""

from __future__ import annotations

import difflib
import json
import subprocess
import sys
from pathlib import Path
from typing import Any


EXPECTED_JSON_FIXTURE = Path(
    "tools/reference-harness/specs/release/b61n-parity-status-summary.fixture.json"
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
        tofile="b61n_parity_status_summary.py --format json",
        n=3,
    )
    print("b61n parity status JSON fixture drifted", file=sys.stderr)
    print("".join(diff), file=sys.stderr)


def main() -> int:
    root = repo_root()
    completed = subprocess.run(
        [
            sys.executable,
            str(root / "tools/reference-harness/scripts/b61n_parity_status_summary.py"),
            "--format",
            "json",
        ],
        cwd=root,
        text=True,
        capture_output=True,
        check=False,
    )
    if completed.returncode != 0:
        print("b61n_parity_status_summary.py JSON mode failed", file=sys.stderr)
        if completed.stdout:
            print("stdout:", file=sys.stderr)
            print(completed.stdout, file=sys.stderr)
        if completed.stderr:
            print("stderr:", file=sys.stderr)
            print(completed.stderr, file=sys.stderr)
        return completed.returncode

    try:
        actual = json.loads(completed.stdout)
    except json.JSONDecodeError as error:
        print(f"b61n parity status JSON output is not valid JSON: {error}", file=sys.stderr)
        print(completed.stdout, file=sys.stderr)
        return 1
    if not isinstance(actual, dict):
        print("b61n parity status JSON output must be a JSON object", file=sys.stderr)
        return 1

    expected = read_json(root / EXPECTED_JSON_FIXTURE)
    if actual != expected:
        print_json_diff(expected, actual)
        return 1

    print("b61n parity status JSON fixture gate passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
