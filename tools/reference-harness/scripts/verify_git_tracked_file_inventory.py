#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-autoIBP-TBD
"""Verify pinned git-tracked file extension counts for source categories."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Callable, cast


DEFAULT_REGISTRY = Path(
    "tools/reference-harness/specs/release/git-tracked-file-extension-inventory.json"
)
SCOPE = "git-tracked-file-extension-inventory"
TRACKED_SOURCE_EXTENSIONS = (".cpp", ".hpp", ".py", ".md")
NO_EXTENSION_KEY = "<none>"


class GitTrackedFileInventoryError(RuntimeError):
    """Raised when tracked source file extension floors regress."""


@dataclass(frozen=True)
class RegistryConfig:
    total_count: int
    no_extension_count: int
    counts_by_extension: dict[str, int]
    tracked_source_extensions: tuple[str, ...]
    minimum_tracked_source_counts: dict[str, int]


@dataclass(frozen=True)
class InventoryCounts:
    total_count: int
    no_extension_count: int
    counts_by_extension: dict[str, int]


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise GitTrackedFileInventoryError(message)


def read_json(path: Path) -> dict[str, Any]:
    try:
        with path.open("r", encoding="utf-8") as stream:
            payload = json.load(stream)
    except json.JSONDecodeError as exc:
        raise GitTrackedFileInventoryError(f"{path} is not valid JSON: {exc}") from exc
    expect(isinstance(payload, dict), f"{path} must contain a JSON object")
    return payload


def require_string(value: Any, label: str) -> str:
    expect(isinstance(value, str) and bool(value.strip()), f"{label} must be a nonempty string")
    expect(value == value.strip(), f"{label} must not carry surrounding whitespace")
    return value


def require_non_negative_int(value: Any, label: str) -> int:
    expect(type(value) is int and value >= 0, f"{label} must be a nonnegative integer")
    return value


def require_string_list(value: Any, label: str) -> list[str]:
    expect(isinstance(value, list), f"{label} must be a list")
    entries = [require_string(item, f"{label}[{index}]") for index, item in enumerate(value)]
    duplicates = sorted(entry for entry in set(entries) if entries.count(entry) > 1)
    expect(not duplicates, f"{label} duplicates value(s): {', '.join(duplicates)}")
    return entries


def require_extension(value: str, label: str) -> str:
    expect(value.startswith("."), f"{label} must start with '.'")
    expect("/" not in value and "\\" not in value, f"{label} must be a suffix, not a path")
    return value


def require_count_map(value: Any, label: str) -> dict[str, int]:
    expect(isinstance(value, dict), f"{label} must be an object")
    counts: dict[str, int] = {}
    for raw_key, raw_value in value.items():
        key = require_extension(require_string(raw_key, f"{label} key"), f"{label}.{raw_key}")
        counts[key] = require_non_negative_int(raw_value, f"{label}.{key}")
    expect(len(counts) == len(value), f"{label} must not contain duplicate extension keys")
    return dict(sorted(counts.items()))


def load_registry(path: Path) -> RegistryConfig:
    registry = read_json(path)
    expect(registry.get("schema_version") == 1, f"{path} schema_version must be 1")
    expect(registry.get("scope") == SCOPE, f"{path} scope must be {SCOPE}")
    require_string(registry.get("baseline_source_commit"), f"{path}.baseline_source_commit")
    require_string(registry.get("baseline_generated_at_utc"), f"{path}.baseline_generated_at_utc")
    require_string(registry.get("baseline_command"), f"{path}.baseline_command")
    require_string(registry.get("regression_policy"), f"{path}.regression_policy")

    raw_counts = registry.get("tracked_file_counts")
    expect(isinstance(raw_counts, dict), f"{path}.tracked_file_counts must be an object")
    tracked_file_counts = cast(dict[str, Any], raw_counts)
    total_count = require_non_negative_int(
        tracked_file_counts.get("total_count"),
        f"{path}.tracked_file_counts.total_count",
    )
    no_extension_count = require_non_negative_int(
        tracked_file_counts.get("no_extension_count"),
        f"{path}.tracked_file_counts.no_extension_count",
    )
    counts_by_extension = require_count_map(
        tracked_file_counts.get("counts_by_extension"),
        f"{path}.tracked_file_counts.counts_by_extension",
    )
    expect(
        total_count == no_extension_count + sum(counts_by_extension.values()),
        f"{path}.tracked_file_counts.total_count must equal no_extension_count plus extension counts",
    )

    tracked_source_extensions = tuple(
        require_extension(extension, f"{path}.tracked_source_extensions[{index}]")
        for index, extension in enumerate(
            require_string_list(registry.get("tracked_source_extensions"), f"{path}.tracked_source_extensions")
        )
    )
    expect(
        tracked_source_extensions == TRACKED_SOURCE_EXTENSIONS,
        f"{path}.tracked_source_extensions must be {list(TRACKED_SOURCE_EXTENSIONS)}",
    )
    minimum_counts = require_count_map(
        registry.get("minimum_tracked_source_counts"),
        f"{path}.minimum_tracked_source_counts",
    )
    expect(
        set(minimum_counts) == set(tracked_source_extensions),
        f"{path}.minimum_tracked_source_counts keys must match tracked_source_extensions",
    )
    for extension in tracked_source_extensions:
        expect(
            minimum_counts[extension] == counts_by_extension.get(extension),
            f"{path}.minimum_tracked_source_counts.{extension} must match pinned extension count",
        )
    return RegistryConfig(
        total_count=total_count,
        no_extension_count=no_extension_count,
        counts_by_extension=counts_by_extension,
        tracked_source_extensions=tracked_source_extensions,
        minimum_tracked_source_counts=minimum_counts,
    )


def run_git(root: Path, args: list[str]) -> bytes:
    completed = subprocess.run(
        ["git", "-C", str(root), *args],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if completed.returncode != 0:
        raise GitTrackedFileInventoryError(
            f"git {' '.join(args)} failed in {root}: "
            f"{completed.stderr.decode('utf-8', errors='replace').strip()}"
        )
    return completed.stdout


def tracked_files(root: Path) -> list[str]:
    output = run_git(root, ["ls-files", "-z"])
    return sorted(item.decode("utf-8") for item in output.split(b"\0") if item)


def extension_key(path_text: str) -> str:
    base_name = path_text.rsplit("/", 1)[-1]
    if "." not in base_name:
        return NO_EXTENSION_KEY
    return "." + base_name.rsplit(".", 1)[1]


def count_extensions(paths: list[str]) -> InventoryCounts:
    no_extension_count = 0
    counts_by_extension: dict[str, int] = {}
    for path_text in paths:
        key = extension_key(path_text)
        if key == NO_EXTENSION_KEY:
            no_extension_count += 1
        else:
            counts_by_extension[key] = counts_by_extension.get(key, 0) + 1
    return InventoryCounts(
        total_count=len(paths),
        no_extension_count=no_extension_count,
        counts_by_extension=dict(sorted(counts_by_extension.items())),
    )


def verify_inventory(root: Path, registry_path: Path) -> InventoryCounts:
    registry = load_registry(registry_path)
    actual = count_extensions(tracked_files(root))
    errors: list[str] = []
    for extension in registry.tracked_source_extensions:
        minimum = registry.minimum_tracked_source_counts[extension]
        actual_count = actual.counts_by_extension.get(extension, 0)
        if actual_count < minimum:
            errors.append(
                f"tracked source category {extension} count dropped: "
                f"actual={actual_count}, pinned_minimum={minimum}"
            )
    if errors:
        raise GitTrackedFileInventoryError("; ".join(errors))
    return actual


def self_check_registry() -> dict[str, Any]:
    return {
        "schema_version": 1,
        "scope": SCOPE,
        "baseline_source_commit": "0" * 40,
        "baseline_generated_at_utc": "2026-06-14T00:00:00Z",
        "baseline_command": "git ls-files grouped by final filename extension",
        "tracked_file_counts": {
            "total_count": 8,
            "no_extension_count": 1,
            "counts_by_extension": {
                ".cpp": 1,
                ".csv": 1,
                ".gitignore": 1,
                ".hpp": 1,
                ".md": 2,
                ".py": 1,
            },
        },
        "tracked_source_extensions": list(TRACKED_SOURCE_EXTENSIONS),
        "minimum_tracked_source_counts": {
            ".cpp": 1,
            ".hpp": 1,
            ".py": 1,
            ".md": 2,
        },
        "regression_policy": (
            "Fail if any tracked-source extension count (.cpp, .hpp, .py, .md) "
            "drops below its pinned minimum."
        ),
    }


def write_fixture(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def make_fixture(root: Path) -> Path:
    write_fixture(root / "src/core.cpp", "int core() { return 0; }\n")
    write_fixture(root / "include/core.hpp", "#pragma once\n")
    write_fixture(root / "tools/check.py", "print('ok')\n")
    write_fixture(root / "README.md", "# fixture\n")
    write_fixture(root / "docs/note.md", "# note\n")
    write_fixture(root / "data/table.csv", "x,y\n")
    write_fixture(root / ".gitignore", "build/\n")
    write_fixture(root / "LICENSE", "fixture\n")
    registry_path = root / DEFAULT_REGISTRY
    write_fixture(registry_path, json.dumps(self_check_registry(), indent=2, sort_keys=True) + "\n")
    run_git(root, ["init", "-q"])
    run_git(root, ["add", "."])
    return registry_path


def expect_failure(label: str, action: Callable[[], object], expected: str) -> None:
    try:
        action()
    except GitTrackedFileInventoryError as exc:
        expect(expected in str(exc), f"{label} failed for the wrong reason: {exc}")
        return
    raise GitTrackedFileInventoryError(f"{label} self-check fixture unexpectedly passed")


def run_self_check() -> None:
    with tempfile.TemporaryDirectory(prefix="git-tracked-inventory-") as temp_dir:
        fixture = Path(temp_dir)
        registry_path = make_fixture(fixture)
        counts = verify_inventory(fixture, registry_path)
        expect(counts.total_count == 9, "fixture total count mismatch")
        expect(counts.no_extension_count == 1, "fixture no-extension count mismatch")
        expect(counts.counts_by_extension[".md"] == 2, "fixture .md count mismatch")
        expect(counts.counts_by_extension[".json"] == 1, "fixture .json count mismatch")

        run_git(fixture, ["rm", "-q", "--cached", "README.md"])
        expect_failure(
            "source-extension-regression",
            lambda: verify_inventory(fixture, registry_path),
            "tracked source category .md count dropped",
        )

    with tempfile.TemporaryDirectory(prefix="git-tracked-inventory-") as temp_dir:
        fixture = Path(temp_dir)
        registry_path = make_fixture(fixture)
        run_git(fixture, ["rm", "-q", "--cached", "data/table.csv"])
        verify_inventory(fixture, registry_path)

    invalid_registry = self_check_registry()
    invalid_registry["minimum_tracked_source_counts"] = {
        ".cpp": 1,
        ".hpp": 1,
        ".py": 2,
        ".md": 2,
    }
    with tempfile.TemporaryDirectory(prefix="git-tracked-inventory-") as temp_dir:
        fixture = Path(temp_dir)
        registry_path = make_fixture(fixture)
        registry_path.write_text(json.dumps(invalid_registry, indent=2) + "\n", encoding="utf-8")
        expect_failure(
            "invalid-registry",
            lambda: load_registry(registry_path),
            "minimum_tracked_source_counts..py",
        )


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--registry",
        type=Path,
        default=DEFAULT_REGISTRY,
        help="Git-tracked file extension inventory registry JSON.",
    )
    parser.add_argument("--verify", action="store_true", help="Verify the repository against the registry.")
    parser.add_argument("--self-check", action="store_true", help="Run synthetic checks for this verifier.")
    args = parser.parse_args(argv)

    try:
        if args.self_check:
            run_self_check()
            print("git tracked file inventory verifier self-check passed")
            return 0

        root = repo_root()
        registry_path = args.registry if args.registry.is_absolute() else root / args.registry
        counts = verify_inventory(root, registry_path)
        source_counts = ", ".join(
            f"{extension}={counts.counts_by_extension.get(extension, 0)}"
            for extension in TRACKED_SOURCE_EXTENSIONS
        )
        print(
            "git tracked file inventory verified: "
            f"{source_counts}, total={counts.total_count}, no_extension={counts.no_extension_count}"
        )
        return 0
    except (GitTrackedFileInventoryError, OSError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
