#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-autoIBP-TBD
"""CTest fixture gate for immutable test/reference fixture trees."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import stat
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Any


DEFAULT_FIXTURE_PATHS = (
    Path("tests"),
    Path("specs"),
    Path("tools/reference-harness/specs"),
    Path("tools/reference-harness/templates"),
    Path("references/snapshots"),
)
STATE_SCHEMA_VERSION = 1


class FixtureTreeError(RuntimeError):
    """Raised when a protected fixture tree changes during CTest execution."""


@dataclass(frozen=True)
class DiffSummary:
    added: list[str]
    removed: list[str]
    changed: list[str]

    def is_clean(self) -> bool:
        return not self.added and not self.removed and not self.changed


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise FixtureTreeError(message)


def resolve_root(path: Path) -> Path:
    return path if path.is_absolute() else repo_root() / path


def normalize_fixture_paths(root: Path, paths: list[Path]) -> list[Path]:
    normalized: list[Path] = []
    seen: set[str] = set()
    root_resolved = root.resolve(strict=True)
    for raw_path in paths:
        candidate = raw_path if raw_path.is_absolute() else root / raw_path
        resolved = candidate.resolve(strict=True)
        try:
            relative = resolved.relative_to(root_resolved)
        except ValueError as exc:
            raise FixtureTreeError(
                f"fixture path is outside the repository root: {raw_path}"
            ) from exc
        expect(bool(relative.parts), f"fixture path must not be the repository root: {raw_path}")
        relative_path = Path(*relative.parts)
        text = relative_path.as_posix()
        if text not in seen:
            normalized.append(relative_path)
            seen.add(text)
    return normalized


def file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def stat_mode(path: Path) -> str:
    return oct(stat.S_IMODE(path.lstat().st_mode))


def entry_key(root: Path, path: Path) -> str:
    return path.relative_to(root).as_posix()


def snapshot_path(root: Path, path: Path, entries: dict[str, dict[str, Any]]) -> None:
    key = entry_key(root, path)
    if path.is_symlink():
        entries[key] = {
            "kind": "symlink",
            "mode": stat_mode(path),
            "target": os.readlink(path),
        }
        return

    mode = path.lstat().st_mode
    if stat.S_ISDIR(mode):
        entries[key] = {"kind": "directory", "mode": stat_mode(path)}
        for child in sorted(path.iterdir(), key=lambda item: item.name):
            snapshot_path(root, child, entries)
        return

    if stat.S_ISREG(mode):
        entries[key] = {
            "kind": "file",
            "mode": stat_mode(path),
            "size": path.stat().st_size,
            "sha256": file_sha256(path),
        }
        return

    entries[key] = {"kind": "other", "mode": stat_mode(path)}


def build_snapshot(root: Path, fixture_paths: list[Path]) -> dict[str, Any]:
    entries: dict[str, dict[str, Any]] = {}
    for fixture_path in fixture_paths:
        snapshot_path(root, root / fixture_path, entries)
    return {
        "schema_version": STATE_SCHEMA_VERSION,
        "fixture_root": str(root),
        "fixture_paths": [path.as_posix() for path in fixture_paths],
        "entries": dict(sorted(entries.items())),
    }


def write_snapshot(root: Path, fixture_paths: list[Path], state_path: Path) -> None:
    snapshot = build_snapshot(root, fixture_paths)
    state_path.parent.mkdir(parents=True, exist_ok=True)
    state_path.write_text(
        json.dumps(snapshot, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    print(
        "Fixture tree snapshot recorded: "
        f"{len(snapshot['entries'])} entries across {len(fixture_paths)} roots"
    )


def load_state(state_path: Path) -> dict[str, Any]:
    try:
        payload = json.loads(state_path.read_text(encoding="utf-8"))
    except FileNotFoundError as exc:
        raise FixtureTreeError(f"missing fixture snapshot state: {state_path}") from exc
    except json.JSONDecodeError as exc:
        raise FixtureTreeError(f"fixture snapshot state is not valid JSON: {state_path}: {exc}") from exc
    expect(isinstance(payload, dict), "fixture snapshot state must contain a JSON object")
    expect(
        payload.get("schema_version") == STATE_SCHEMA_VERSION,
        f"fixture snapshot schema_version must be {STATE_SCHEMA_VERSION}",
    )
    expect(isinstance(payload.get("entries"), dict), "fixture snapshot entries must be an object")
    fixture_paths = payload.get("fixture_paths")
    expect(isinstance(fixture_paths, list), "fixture snapshot fixture_paths must be a list")
    for item in fixture_paths:
        expect(
            isinstance(item, str) and bool(item.strip()),
            "fixture_paths entries must be strings",
        )
    return payload


def diff_snapshots(expected: dict[str, Any], actual: dict[str, Any]) -> DiffSummary:
    expected_entries = expected["entries"]
    actual_entries = actual["entries"]
    expected_paths = set(expected_entries)
    actual_paths = set(actual_entries)
    common_paths = expected_paths & actual_paths
    return DiffSummary(
        added=sorted(actual_paths - expected_paths),
        removed=sorted(expected_paths - actual_paths),
        changed=sorted(
            path for path in common_paths if expected_entries[path] != actual_entries[path]
        ),
    )


def format_limited(paths: list[str], limit: int = 20) -> str:
    shown = paths[:limit]
    rendered = "\n".join(f"  - {path}" for path in shown)
    if len(paths) > limit:
        rendered += f"\n  ... {len(paths) - limit} more"
    return rendered


def verify_snapshot(root: Path, state_path: Path) -> None:
    expected = load_state(state_path)
    root_in_state = expected.get("fixture_root")
    if not isinstance(root_in_state, str) or not root_in_state:
        raise FixtureTreeError("fixture_root must be a string")
    expect(
        Path(root_in_state).resolve(strict=True) == root.resolve(strict=True),
        f"fixture snapshot root mismatch: state={root_in_state} current={root}",
    )
    fixture_paths = normalize_fixture_paths(
        root,
        [Path(item) for item in expected["fixture_paths"]],
    )
    actual = build_snapshot(root, fixture_paths)
    diff = diff_snapshots(expected, actual)
    if diff.is_clean():
        print(
            "Fixture tree immutability verified: "
            f"{len(actual['entries'])} entries unchanged"
        )
        return

    messages: list[str] = []
    if diff.added:
        messages.append("Added fixture-tree entries:\n" + format_limited(diff.added))
    if diff.removed:
        messages.append("Removed fixture-tree entries:\n" + format_limited(diff.removed))
    if diff.changed:
        messages.append("Changed fixture-tree entries:\n" + format_limited(diff.changed))
    raise FixtureTreeError(
        "fixture tree changed during the CTest run; tests must write outputs outside "
        "protected fixture roots.\n" + "\n\n".join(messages)
    )


def expect_failure(action: Any, expected: str) -> None:
    try:
        action()
    except FixtureTreeError as exc:
        expect(expected in str(exc), f"unexpected self-check error: {exc}")
        return
    raise FixtureTreeError("self-check fixture unexpectedly passed")


def self_check() -> None:
    with tempfile.TemporaryDirectory(prefix="fixture-tree-immutability-") as tmp:
        root = Path(tmp)
        state_path = root / "build" / "fixture-state.json"
        fixture_paths = [Path("tests"), Path("tools/reference-harness/specs")]
        (root / "tests" / "data").mkdir(parents=True)
        (root / "tools" / "reference-harness" / "specs").mkdir(parents=True)
        (root / "tests" / "data" / "input.txt").write_text("fixture\n", encoding="utf-8")
        (root / "tools" / "reference-harness" / "specs" / "case.json").write_text(
            '{"ok": true}\n',
            encoding="utf-8",
        )

        normalized_paths = normalize_fixture_paths(root, fixture_paths)
        write_snapshot(root, normalized_paths, state_path)
        verify_snapshot(root, state_path)

        input_path = root / "tests" / "data" / "input.txt"
        original = input_path.read_text(encoding="utf-8")
        input_path.write_text("mutated\n", encoding="utf-8")
        expect_failure(lambda: verify_snapshot(root, state_path), "Changed fixture-tree entries")
        input_path.write_text(original, encoding="utf-8")

        added_path = root / "tests" / "data" / "output.txt"
        added_path.write_text("new output\n", encoding="utf-8")
        expect_failure(lambda: verify_snapshot(root, state_path), "Added fixture-tree entries")
        added_path.unlink()

        input_path.unlink()
        expect_failure(lambda: verify_snapshot(root, state_path), "Removed fixture-tree entries")
    print("Fixture tree immutability self-check passed")


def parse_args(argv: list[str] | None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--snapshot", action="store_true", help="Record fixture-tree state.")
    mode.add_argument("--verify", action="store_true", help="Verify fixture-tree state.")
    mode.add_argument("--self-check", action="store_true", help="Run synthetic self-checks.")
    parser.add_argument("--root", type=Path, default=repo_root(), help="Repository root.")
    parser.add_argument("--state", type=Path, help="Snapshot state JSON path.")
    parser.add_argument(
        "--path",
        action="append",
        type=Path,
        dest="paths",
        help="Repo-relative fixture root to protect. May be repeated.",
    )
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = parse_args(argv)
    try:
        if args.self_check:
            self_check()
            return 0

        state_arg = args.state
        if state_arg is None:
            raise FixtureTreeError("--state is required with --snapshot or --verify")
        root = resolve_root(args.root).resolve(strict=True)
        fixture_paths = normalize_fixture_paths(root, args.paths or list(DEFAULT_FIXTURE_PATHS))
        state_path = state_arg if state_arg.is_absolute() else root / state_arg
        if args.snapshot:
            write_snapshot(root, fixture_paths, state_path)
        else:
            verify_snapshot(root, state_path)
        return 0
    except FixtureTreeError as exc:
        print(f"Fixture tree immutability gate failed: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
