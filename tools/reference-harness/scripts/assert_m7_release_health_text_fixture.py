#!/usr/bin/env python3
"""CTest gate for the M7 release health text output contract."""

from __future__ import annotations

import difflib
import subprocess
import sys
from pathlib import Path


EXPECTED_TEXT_FIXTURE = Path(
    "tools/reference-harness/specs/release/m7-release-health-summary.fixture.txt"
)


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def print_text_diff(expected: str, actual: str) -> None:
    diff = difflib.unified_diff(
        expected.splitlines(keepends=True),
        actual.splitlines(keepends=True),
        fromfile=str(EXPECTED_TEXT_FIXTURE),
        tofile="release_health_summary.py --verify --format text",
        n=3,
    )
    print("M7 release health text fixture drifted", file=sys.stderr)
    print("".join(diff), file=sys.stderr)


def main() -> int:
    root = repo_root()
    completed = subprocess.run(
        [
            sys.executable,
            str(root / "tools/reference-harness/scripts/release_health_summary.py"),
            "--verify",
            "--format",
            "text",
        ],
        cwd=root,
        text=True,
        capture_output=True,
        check=False,
    )
    if completed.returncode != 0:
        print("release_health_summary.py text mode failed", file=sys.stderr)
        if completed.stdout:
            print("stdout:", file=sys.stderr)
            print(completed.stdout, file=sys.stderr)
        if completed.stderr:
            print("stderr:", file=sys.stderr)
            print(completed.stderr, file=sys.stderr)
        return completed.returncode

    expected = (root / EXPECTED_TEXT_FIXTURE).read_text(encoding="utf-8")
    if completed.stdout != expected:
        print_text_diff(expected, completed.stdout)
        return 1

    print("M7 release health text fixture gate passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
