#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-autoIBP-TBD
"""Verify pinned public header file counts under include/."""

from __future__ import annotations

import argparse
import json
import sys
import tempfile
from collections.abc import Callable
from dataclasses import dataclass
from pathlib import Path
from typing import cast


DEFAULT_REGISTRY = Path("tools/reference-harness/specs/abi/amflow-public-header-count-registry.json")
SCOPE = "amflow-public-header-count-registry"
DEFAULT_INCLUDE_ROOT = "include"
DEFAULT_EXTENSIONS = (".h", ".hpp")


class PublicHeaderCountError(RuntimeError):
    """Raised when public header count floors regress."""


@dataclass(frozen=True)
class RegistryConfig:
    include_root: str
    extensions: tuple[str, ...]
    minimum_total_count: int
    minimum_counts_by_extension: dict[str, int]


@dataclass(frozen=True)
class HeaderCounts:
    total_count: int
    counts_by_extension: dict[str, int]
    headers: list[str]


@dataclass(frozen=True)
class VerificationSummary:
    registry: str
    total_count: int
    minimum_total_count: int
    counts_by_extension: dict[str, int]


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise PublicHeaderCountError(message)


def read_json(path: Path) -> dict[str, object]:
    try:
        with path.open("r", encoding="utf-8") as stream:
            payload: object = json.load(stream)
    except json.JSONDecodeError as exc:
        raise PublicHeaderCountError(f"{path} is not valid JSON: {exc}") from exc
    expect(isinstance(payload, dict), f"{path} must contain a JSON object")
    raw_payload = cast(dict[object, object], payload)
    expect(all(isinstance(key, str) for key in raw_payload), f"{path} keys must be strings")
    return cast(dict[str, object], raw_payload)


def require_string(value: object, label: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise PublicHeaderCountError(f"{label} must be a nonempty string")
    if value != value.strip():
        raise PublicHeaderCountError(f"{label} must not carry surrounding whitespace")
    return value


def require_non_negative_int(value: object, label: str) -> int:
    if not isinstance(value, int) or isinstance(value, bool) or value < 0:
        raise PublicHeaderCountError(f"{label} must be a nonnegative integer")
    return value


def require_string_list(value: object, label: str) -> list[str]:
    if not isinstance(value, list):
        raise PublicHeaderCountError(f"{label} must be a list")
    raw_entries = cast(list[object], value)
    entries = [require_string(item, f"{label}[{index}]") for index, item in enumerate(raw_entries)]
    duplicates = sorted(entry for entry in set(entries) if entries.count(entry) > 1)
    expect(not duplicates, f"{label} duplicates value(s): {', '.join(duplicates)}")
    return entries


def require_extension(value: str, label: str) -> str:
    expect(value.startswith("."), f"{label} must start with '.'")
    expect("/" not in value and "\\" not in value, f"{label} must be a file suffix, not a path")
    expect(value == value.lower(), f"{label} must be lowercase")
    return value


def require_extension_count_map(value: object, label: str, extensions: tuple[str, ...]) -> dict[str, int]:
    if not isinstance(value, dict):
        raise PublicHeaderCountError(f"{label} must be an object")
    raw_counts = cast(dict[str, object], value)
    expected_keys = set(extensions)
    actual_keys = set(raw_counts)
    expect(
        actual_keys == expected_keys,
        f"{label} keys must match configured extensions: {', '.join(extensions)}",
    )
    return {
        extension: require_non_negative_int(raw_counts[extension], f"{label}.{extension}")
        for extension in extensions
    }


def load_registry(path: Path) -> RegistryConfig:
    registry = read_json(path)
    expect(registry.get("schema_version") == 1, f"{path} schema_version must be 1")
    expect(registry.get("scope") == SCOPE, f"{path} scope must be {SCOPE}")
    require_string(registry.get("baseline_source_commit"), f"{path}.baseline_source_commit")
    require_string(registry.get("baseline_generated_at_utc"), f"{path}.baseline_generated_at_utc")
    require_string(registry.get("regression_policy"), f"{path}.regression_policy")

    raw_headers = registry.get("headers")
    expect(isinstance(raw_headers, dict), f"{path}.headers must be an object")
    headers = cast(dict[str, object], raw_headers)
    include_root = require_string(headers.get("root"), f"{path}.headers.root")
    expect(include_root == DEFAULT_INCLUDE_ROOT, f"{path}.headers.root must be {DEFAULT_INCLUDE_ROOT}")
    extensions = tuple(
        require_extension(extension, f"{path}.headers.extensions[{index}]")
        for index, extension in enumerate(require_string_list(headers.get("extensions"), f"{path}.headers.extensions"))
    )
    expect(extensions == DEFAULT_EXTENSIONS, f"{path}.headers.extensions must be ['.h', '.hpp']")

    minimum_total = require_non_negative_int(registry.get("minimum_total_count"), f"{path}.minimum_total_count")
    minimum_counts = require_extension_count_map(
        registry.get("minimum_counts_by_extension"),
        f"{path}.minimum_counts_by_extension",
        extensions,
    )
    expect(
        minimum_total == sum(minimum_counts.values()),
        f"{path}.minimum_total_count must equal sum(minimum_counts_by_extension)",
    )
    return RegistryConfig(
        include_root=include_root,
        extensions=extensions,
        minimum_total_count=minimum_total,
        minimum_counts_by_extension=minimum_counts,
    )


def count_public_headers(root: Path, include_root: str, extensions: tuple[str, ...]) -> HeaderCounts:
    header_root = root / include_root
    expect(header_root.is_dir(), f"public include directory is missing: {header_root}")
    counts = {extension: 0 for extension in extensions}
    header_paths: list[str] = []
    for path in sorted(header_root.rglob("*")):
        if not path.is_file() or path.suffix not in counts:
            continue
        counts[path.suffix] += 1
        header_paths.append(path.relative_to(root).as_posix())
    return HeaderCounts(total_count=len(header_paths), counts_by_extension=counts, headers=header_paths)


def registry_relative(path: Path, root: Path) -> str:
    try:
        return path.resolve(strict=False).relative_to(root.resolve(strict=False)).as_posix()
    except ValueError:
        return str(path)


def verify_counts(root: Path, registry_path: Path) -> VerificationSummary:
    registry = load_registry(registry_path)
    actual = count_public_headers(root, registry.include_root, registry.extensions)
    errors: list[str] = []
    if actual.total_count < registry.minimum_total_count:
        errors.append(
            "total public header count dropped: "
            f"actual={actual.total_count}, pinned_minimum={registry.minimum_total_count}"
        )
    for extension, minimum in registry.minimum_counts_by_extension.items():
        actual_count = actual.counts_by_extension.get(extension, 0)
        if actual_count < minimum:
            errors.append(
                f"extension {extension} public header count dropped: "
                f"actual={actual_count}, pinned_minimum={minimum}"
            )
    if errors:
        raise PublicHeaderCountError("; ".join(errors))
    return VerificationSummary(
        registry=registry_relative(registry_path, root),
        total_count=actual.total_count,
        minimum_total_count=registry.minimum_total_count,
        counts_by_extension=actual.counts_by_extension,
    )


def self_check_registry() -> dict[str, object]:
    return {
        "schema_version": 1,
        "scope": SCOPE,
        "baseline_source_commit": "0" * 40,
        "baseline_generated_at_utc": "2026-06-13T00:00:00Z",
        "headers": {
            "root": DEFAULT_INCLUDE_ROOT,
            "extensions": list(DEFAULT_EXTENSIONS),
        },
        "minimum_total_count": 3,
        "minimum_counts_by_extension": {
            ".h": 1,
            ".hpp": 2,
        },
        "regression_policy": (
            "Fail if the total public header count or any configured extension count "
            "drops below its pinned minimum."
        ),
    }


def write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def write_self_check_root(root: Path, registry: dict[str, object] | None = None) -> Path:
    registry_path = root / DEFAULT_REGISTRY
    write_text(registry_path, json.dumps(registry or self_check_registry(), indent=2, sort_keys=True) + "\n")
    write_text(root / "include/amflow/core/problem_spec.hpp", "// public fixture\n")
    write_text(root / "include/amflow/runtime/eta_mode.hpp", "// public fixture\n")
    write_text(root / "include/amflow/compat.h", "/* public fixture */\n")
    write_text(root / "include/amflow/ignored.hxx", "/* not counted */\n")
    return registry_path


def expect_self_check_failure(label: str, mutator: Callable[[Path, Path], None], expected: str) -> None:
    with tempfile.TemporaryDirectory(prefix="public-header-count-") as tmp:
        root = Path(tmp)
        registry_path = write_self_check_root(root)
        mutator(root, registry_path)
        try:
            verify_counts(root, registry_path)
        except PublicHeaderCountError as exc:
            expect(expected in str(exc), f"{label} failed for the wrong reason: {exc}")
            return
        raise PublicHeaderCountError(f"{label} self-check fixture unexpectedly passed")


def run_self_check() -> None:
    with tempfile.TemporaryDirectory(prefix="public-header-count-") as tmp:
        root = Path(tmp)
        registry_path = write_self_check_root(root)
        summary = verify_counts(root, registry_path)
        expect(summary.total_count == 3, "valid fixture total count drifted")
        expect(summary.counts_by_extension[".h"] == 1, "valid fixture .h count drifted")
        expect(summary.counts_by_extension[".hpp"] == 2, "valid fixture .hpp count drifted")

    expect_self_check_failure(
        "hpp-regression",
        lambda root, _registry: (root / "include/amflow/runtime/eta_mode.hpp").unlink(),
        "extension .hpp public header count dropped",
    )
    expect_self_check_failure(
        "h-regression",
        lambda root, _registry: (root / "include/amflow/compat.h").unlink(),
        "extension .h public header count dropped",
    )

    invalid_registry: dict[str, object] = self_check_registry()
    invalid_registry["minimum_total_count"] = 4

    def write_invalid_total_registry(_root: Path, registry: Path) -> None:
        registry.write_text(
            json.dumps(invalid_registry, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )

    expect_self_check_failure(
        "invalid-total",
        write_invalid_total_registry,
        "minimum_total_count",
    )


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--registry",
        type=Path,
        default=DEFAULT_REGISTRY,
        help="Public header count registry JSON.",
    )
    parser.add_argument("--verify", action="store_true", help="Verify the repository against the registry.")
    parser.add_argument("--self-check", action="store_true", help="Run synthetic checks for this verifier.")
    args = parser.parse_args(argv)

    try:
        if args.self_check:
            run_self_check()
            print("public header count verifier self-check passed")
            return 0

        root = repo_root()
        registry_path = args.registry if args.registry.is_absolute() else root / args.registry
        summary = verify_counts(root, registry_path)
        counts = ", ".join(f"{extension}={count}" for extension, count in sorted(summary.counts_by_extension.items()))
        print(
            "public header counts verified: "
            f"{counts}, total={summary.total_count}, "
            f"minimum={summary.minimum_total_count}"
        )
        return 0
    except (PublicHeaderCountError, OSError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
