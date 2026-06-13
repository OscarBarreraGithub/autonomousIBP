#!/usr/bin/env python3
"""Require committed reference-harness JSON sidecars to publish schema_version."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


def repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def validate_sidecars(spec_root: Path) -> list[str]:
    failures: list[str] = []
    for path in sorted(spec_root.rglob("*.json")):
        relpath = path.as_posix()
        try:
            payload = json.loads(path.read_text(encoding="utf-8"))
        except json.JSONDecodeError as exc:
            failures.append(f"{relpath}: invalid JSON: {exc}")
            continue
        if not isinstance(payload, dict):
            failures.append(f"{relpath}: sidecar root must be a JSON object")
            continue
        version = payload.get("schema_version")
        if not isinstance(version, int) or isinstance(version, bool) or version <= 0:
            failures.append(f"{relpath}: missing positive integer schema_version")
    return failures


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--spec-root",
        type=Path,
        default=repo_root() / "tools/reference-harness/specs",
        help="Reference-harness specs root to audit.",
    )
    args = parser.parse_args(argv)

    spec_root = args.spec_root.resolve()
    if not spec_root.is_dir():
        print(f"error: missing specs root: {spec_root}", file=sys.stderr)
        return 1

    failures = validate_sidecars(spec_root)
    if failures:
        print("error: reference-harness JSON sidecar schema_version audit failed", file=sys.stderr)
        for failure in failures:
            print(f"  {failure}", file=sys.stderr)
        return 1

    total = sum(1 for _ in spec_root.rglob("*.json"))
    print(f"Reference sidecar schema-version gate passed: {total} JSON sidecars checked")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
