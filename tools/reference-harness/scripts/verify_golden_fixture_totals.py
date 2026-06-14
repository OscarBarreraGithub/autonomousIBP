#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-autoIBP-TBD
"""Verify pinned golden fixture file counts and total bytes."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Callable, cast


DEFAULT_REGISTRY = Path("tools/reference-harness/specs/release/golden-fixture-total-bytes-registry.json")
SCOPE = "golden-fixture-total-bytes-registry"
DEFAULT_SPECS_ROOT = "tools/reference-harness/specs"
DEFAULT_GOLDEN_DIRECTORY_NAME = "goldens"
DEFAULT_ALLOWED_POSITIVE_BYTES = 128


class GoldenFixtureTotalsError(RuntimeError):
    """Raised when committed golden fixture totals leave the pinned window."""


@dataclass(frozen=True)
class ByteWindow:
    baseline_total_bytes: int
    allowed_negative_bytes: int
    allowed_positive_bytes: int
    minimum_total_bytes: int
    maximum_total_bytes: int


@dataclass(frozen=True)
class Registry:
    specs_root: str
    golden_directory_name: str
    baseline_file_count: int
    minimum_file_count: int
    byte_window: ByteWindow


@dataclass(frozen=True)
class GoldenTotals:
    file_count: int
    total_bytes: int
    files: tuple[dict[str, Any], ...]


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def utc_now() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def git_head(root: Path) -> str:
    completed = subprocess.run(
        ["git", "rev-parse", "HEAD"],
        cwd=root,
        check=False,
        capture_output=True,
        text=True,
    )
    if completed.returncode != 0:
        return "unknown"
    return completed.stdout.strip()


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise GoldenFixtureTotalsError(message)


def read_json(path: Path) -> dict[str, Any]:
    try:
        with path.open("r", encoding="utf-8") as stream:
            payload = json.load(stream)
    except json.JSONDecodeError as exc:
        raise GoldenFixtureTotalsError(f"{path} is not valid JSON: {exc}") from exc
    expect(isinstance(payload, dict), f"{path} must contain a JSON object")
    return cast(dict[str, Any], payload)


def write_json(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def require_string(value: Any, label: str) -> str:
    expect(isinstance(value, str) and bool(value.strip()), f"{label} must be a nonempty string")
    expect(value == value.strip(), f"{label} must not carry surrounding whitespace")
    return value


def require_non_negative_int(value: Any, label: str) -> int:
    expect(type(value) is int and value >= 0, f"{label} must be a nonnegative integer")
    return int(value)


def require_relative_path(value: Any, label: str) -> str:
    raw = require_string(value, label)
    path = Path(raw)
    expect(not path.is_absolute(), f"{label} must be repository-relative")
    expect("\\" not in raw, f"{label} must use POSIX path separators")
    expect(all(part not in {"", ".", ".."} for part in raw.split("/")), f"{label} must be a safe relative path")
    return raw


def require_directory_name(value: Any, label: str) -> str:
    name = require_string(value, label)
    expect("/" not in name and "\\" not in name, f"{label} must be one path component")
    expect(name not in {".", ".."}, f"{label} must be a concrete directory name")
    return name


def load_registry(path: Path) -> Registry:
    payload = read_json(path)
    expect(payload.get("schema_version") == 1, f"{path} schema_version must be 1")
    expect(payload.get("scope") == SCOPE, f"{path} scope must be {SCOPE}")
    require_string(payload.get("baseline_source_commit"), f"{path}.baseline_source_commit")
    require_string(payload.get("baseline_generated_at_utc"), f"{path}.baseline_generated_at_utc")
    require_string(payload.get("regression_policy"), f"{path}.regression_policy")

    raw_fixtures = payload.get("fixtures")
    expect(isinstance(raw_fixtures, dict), f"{path}.fixtures must be an object")
    fixtures = cast(dict[str, Any], raw_fixtures)
    specs_root = require_relative_path(fixtures.get("root"), f"{path}.fixtures.root")
    golden_directory_name = require_directory_name(
        fixtures.get("golden_directory_name"),
        f"{path}.fixtures.golden_directory_name",
    )
    expect(specs_root == DEFAULT_SPECS_ROOT, f"{path}.fixtures.root must be {DEFAULT_SPECS_ROOT}")
    expect(
        golden_directory_name == DEFAULT_GOLDEN_DIRECTORY_NAME,
        f"{path}.fixtures.golden_directory_name must be {DEFAULT_GOLDEN_DIRECTORY_NAME}",
    )

    baseline_file_count = require_non_negative_int(payload.get("baseline_file_count"), f"{path}.baseline_file_count")
    minimum_file_count = require_non_negative_int(payload.get("minimum_file_count"), f"{path}.minimum_file_count")
    expect(
        minimum_file_count == baseline_file_count,
        f"{path}.minimum_file_count must equal baseline_file_count",
    )

    raw_window = payload.get("total_bytes_window")
    expect(isinstance(raw_window, dict), f"{path}.total_bytes_window must be an object")
    window = cast(dict[str, Any], raw_window)
    baseline_total_bytes = require_non_negative_int(
        window.get("baseline_total_bytes"),
        f"{path}.total_bytes_window.baseline_total_bytes",
    )
    allowed_negative_bytes = require_non_negative_int(
        window.get("allowed_negative_bytes"),
        f"{path}.total_bytes_window.allowed_negative_bytes",
    )
    allowed_positive_bytes = require_non_negative_int(
        window.get("allowed_positive_bytes"),
        f"{path}.total_bytes_window.allowed_positive_bytes",
    )
    minimum_total_bytes = require_non_negative_int(
        window.get("minimum_total_bytes"),
        f"{path}.total_bytes_window.minimum_total_bytes",
    )
    maximum_total_bytes = require_non_negative_int(
        window.get("maximum_total_bytes"),
        f"{path}.total_bytes_window.maximum_total_bytes",
    )
    expected_minimum = max(0, baseline_total_bytes - allowed_negative_bytes)
    expected_maximum = baseline_total_bytes + allowed_positive_bytes
    expect(
        minimum_total_bytes == expected_minimum,
        f"{path}.total_bytes_window.minimum_total_bytes must equal baseline_total_bytes minus allowed_negative_bytes",
    )
    expect(
        maximum_total_bytes == expected_maximum,
        f"{path}.total_bytes_window.maximum_total_bytes must equal baseline_total_bytes plus allowed_positive_bytes",
    )
    expect(minimum_total_bytes <= maximum_total_bytes, f"{path}.total_bytes_window has an empty byte window")

    return Registry(
        specs_root=specs_root,
        golden_directory_name=golden_directory_name,
        baseline_file_count=baseline_file_count,
        minimum_file_count=minimum_file_count,
        byte_window=ByteWindow(
            baseline_total_bytes=baseline_total_bytes,
            allowed_negative_bytes=allowed_negative_bytes,
            allowed_positive_bytes=allowed_positive_bytes,
            minimum_total_bytes=minimum_total_bytes,
            maximum_total_bytes=maximum_total_bytes,
        ),
    )


def registry_relative(path: Path, root: Path) -> str:
    try:
        return path.resolve(strict=False).relative_to(root.resolve(strict=False)).as_posix()
    except ValueError:
        return str(path)


def golden_files(root: Path, specs_root: str, golden_directory_name: str) -> tuple[Path, ...]:
    search_root = root / specs_root
    expect(search_root.is_dir(), f"golden fixture specs root is missing: {search_root}")
    paths: set[Path] = set()
    for golden_dir in sorted(path for path in search_root.rglob(golden_directory_name) if path.is_dir()):
        for path in golden_dir.rglob("*"):
            if path.is_file():
                paths.add(path)
    return tuple(sorted(paths, key=lambda item: registry_relative(item, root)))


def count_golden_fixtures(root: Path, specs_root: str, golden_directory_name: str) -> GoldenTotals:
    files: list[dict[str, Any]] = []
    total_bytes = 0
    for path in golden_files(root, specs_root, golden_directory_name):
        size = path.stat().st_size
        total_bytes += size
        files.append({"path": registry_relative(path, root), "bytes": size})
    return GoldenTotals(file_count=len(files), total_bytes=total_bytes, files=tuple(files))


def verify_totals(root: Path, registry_path: Path) -> dict[str, Any]:
    registry = load_registry(registry_path)
    actual = count_golden_fixtures(root, registry.specs_root, registry.golden_directory_name)
    window = registry.byte_window
    errors: list[str] = []
    if actual.file_count < registry.minimum_file_count:
        errors.append(
            "golden fixture file count dropped: "
            f"actual={actual.file_count}, pinned_minimum={registry.minimum_file_count}"
        )
    if actual.total_bytes < window.minimum_total_bytes:
        errors.append(
            "golden fixture total bytes below pinned window: "
            f"actual={actual.total_bytes}, minimum={window.minimum_total_bytes}, baseline={window.baseline_total_bytes}"
        )
    if actual.total_bytes > window.maximum_total_bytes:
        errors.append(
            "golden fixture total bytes exceeds pinned window: "
            f"actual={actual.total_bytes}, maximum={window.maximum_total_bytes}, baseline={window.baseline_total_bytes}"
        )
    if errors:
        raise GoldenFixtureTotalsError("; ".join(errors))
    return {
        "registry": registry_relative(registry_path, root),
        "file_count": actual.file_count,
        "minimum_file_count": registry.minimum_file_count,
        "total_bytes": actual.total_bytes,
        "minimum_total_bytes": window.minimum_total_bytes,
        "maximum_total_bytes": window.maximum_total_bytes,
        "files": list(actual.files),
    }


def registry_from_totals(root: Path) -> dict[str, Any]:
    actual = count_golden_fixtures(root, DEFAULT_SPECS_ROOT, DEFAULT_GOLDEN_DIRECTORY_NAME)
    return {
        "schema_version": 1,
        "scope": SCOPE,
        "baseline_source_commit": git_head(root),
        "baseline_generated_at_utc": utc_now(),
        "fixtures": {
            "root": DEFAULT_SPECS_ROOT,
            "golden_directory_name": DEFAULT_GOLDEN_DIRECTORY_NAME,
        },
        "baseline_file_count": actual.file_count,
        "minimum_file_count": actual.file_count,
        "total_bytes_window": {
            "baseline_total_bytes": actual.total_bytes,
            "allowed_negative_bytes": 0,
            "allowed_positive_bytes": DEFAULT_ALLOWED_POSITIVE_BYTES,
            "minimum_total_bytes": actual.total_bytes,
            "maximum_total_bytes": actual.total_bytes + DEFAULT_ALLOWED_POSITIVE_BYTES,
        },
        "counting_policy": [
            "Counts every committed regular file below any directory named goldens under tools/reference-harness/specs.",
            "The file-count pin is a floor so adding new committed golden fixtures is allowed.",
            "The byte-total pin is enforced with an explicit lower and upper window to catch truncation and unreviewed bulk changes.",
        ],
        "regression_policy": (
            "Fail if golden fixture file count drops below the pinned baseline or total committed golden bytes "
            "leave total_bytes_window."
        ),
    }


def write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def self_check_registry() -> dict[str, Any]:
    return {
        "schema_version": 1,
        "scope": SCOPE,
        "baseline_source_commit": "0" * 40,
        "baseline_generated_at_utc": "2026-06-14T00:00:00Z",
        "fixtures": {
            "root": DEFAULT_SPECS_ROOT,
            "golden_directory_name": DEFAULT_GOLDEN_DIRECTORY_NAME,
        },
        "baseline_file_count": 3,
        "minimum_file_count": 3,
        "total_bytes_window": {
            "baseline_total_bytes": 12,
            "allowed_negative_bytes": 0,
            "allowed_positive_bytes": 4,
            "minimum_total_bytes": 12,
            "maximum_total_bytes": 16,
        },
        "counting_policy": ["Synthetic self-check fixture."],
        "regression_policy": "Synthetic self-check regression policy.",
    }


def write_self_check_root(root: Path, registry: dict[str, Any] | None = None) -> Path:
    registry_path = root / DEFAULT_REGISTRY
    write_json(registry_path, registry or self_check_registry())
    write_text(root / "tools/reference-harness/specs/m5/lane1/goldens/a.txt", "aaaa")
    write_text(root / "tools/reference-harness/specs/m5/lane1/goldens/b.txt", "bbbb")
    write_text(root / "tools/reference-harness/specs/phase0/case/goldens/c.txt", "cccc")
    write_text(root / "tools/reference-harness/specs/m5/lane1/not-goldens/ignored.txt", "ignored")
    return registry_path


def expect_self_check_failure(label: str, mutator: Callable[[Path, Path], None], expected: str) -> None:
    with tempfile.TemporaryDirectory(prefix="golden-fixture-totals-") as tmp:
        root = Path(tmp)
        registry_path = write_self_check_root(root)
        mutator(root, registry_path)
        try:
            verify_totals(root, registry_path)
        except GoldenFixtureTotalsError as exc:
            expect(expected in str(exc), f"{label} failed for the wrong reason: {exc}")
            return
        raise GoldenFixtureTotalsError(f"{label} self-check fixture unexpectedly passed")


def run_self_check() -> None:
    with tempfile.TemporaryDirectory(prefix="golden-fixture-totals-") as tmp:
        root = Path(tmp)
        registry_path = write_self_check_root(root)
        summary = verify_totals(root, registry_path)
        expect(summary["file_count"] == 3, "valid fixture file count drifted")
        expect(summary["total_bytes"] == 12, "valid fixture total bytes drifted")

    expect_self_check_failure(
        "file-count-regression",
        lambda root, _registry: (root / "tools/reference-harness/specs/m5/lane1/goldens/a.txt").unlink(),
        "golden fixture file count dropped",
    )
    expect_self_check_failure(
        "byte-truncation",
        lambda root, _registry: write_text(root / "tools/reference-harness/specs/m5/lane1/goldens/a.txt", ""),
        "golden fixture total bytes below pinned window",
    )
    expect_self_check_failure(
        "byte-growth",
        lambda root, _registry: write_text(root / "tools/reference-harness/specs/m5/lane1/goldens/a.txt", "a" * 9),
        "golden fixture total bytes exceeds pinned window",
    )

    invalid_registry = self_check_registry()
    invalid_registry["total_bytes_window"]["minimum_total_bytes"] = 10

    def write_invalid_registry(_root: Path, registry_path: Path) -> None:
        write_json(registry_path, invalid_registry)

    expect_self_check_failure(
        "invalid-window",
        write_invalid_registry,
        "minimum_total_bytes",
    )


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=repo_root(), help="Repository root.")
    parser.add_argument("--registry", type=Path, default=DEFAULT_REGISTRY, help="Golden fixture total registry JSON.")
    parser.add_argument("--verify", action="store_true", help="Verify the repository against the registry.")
    parser.add_argument("--self-check", action="store_true", help="Run synthetic checks for this verifier.")
    parser.add_argument("--write-registry", action="store_true", help="Regenerate the committed registry.")
    parser.add_argument("--json", action="store_true", help="Print summary as JSON.")
    args = parser.parse_args(argv)

    root = args.root.resolve()
    registry_path = args.registry if args.registry.is_absolute() else root / args.registry

    try:
        summary: dict[str, Any]
        if args.self_check:
            run_self_check()
            summary = {"self_check": True}
        elif args.write_registry:
            payload = registry_from_totals(root)
            write_json(registry_path, payload)
            summary = {
                "registry": registry_relative(registry_path, root),
                "file_count": payload["baseline_file_count"],
                "total_bytes": payload["total_bytes_window"]["baseline_total_bytes"],
            }
        else:
            summary = verify_totals(root, registry_path)

        if args.json:
            print(json.dumps(summary, indent=2, sort_keys=True))
        elif args.self_check:
            print("golden fixture totals verifier self-check passed")
        elif args.write_registry:
            print(
                "golden fixture total-bytes registry written: "
                f"{summary['file_count']} files, {summary['total_bytes']} bytes"
            )
        else:
            print(
                "golden fixture totals verified: "
                f"{summary['file_count']} files, {summary['total_bytes']} bytes "
                f"(window {summary['minimum_total_bytes']}..{summary['maximum_total_bytes']})"
            )
        return 0
    except (GoldenFixtureTotalsError, OSError, KeyError, TypeError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
