#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-autoIBP-TBD
"""Verify pinned docs/release markdown file counts by category."""

from __future__ import annotations

import argparse
import fnmatch
import json
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Any, cast


DEFAULT_REGISTRY = Path("tools/reference-harness/specs/release/release-doc-file-count-registry.json")
SCOPE = "release-doc-file-count-registry"


class ReleaseDocCountError(RuntimeError):
    """Raised when the release markdown file count registry drifts."""


@dataclass(frozen=True)
class Category:
    category_id: str
    description: str
    minimum_count: int
    path_globs: tuple[str, ...]
    catch_all: bool


@dataclass(frozen=True)
class Registry:
    include_glob: str
    categories: tuple[Category, ...]
    total_minimum_count: int


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise ReleaseDocCountError(message)


def read_json(path: Path) -> dict[str, Any]:
    try:
        with path.open("r", encoding="utf-8") as stream:
            payload = json.load(stream)
    except json.JSONDecodeError as exc:
        raise ReleaseDocCountError(f"{path} is not valid JSON: {exc}") from exc
    expect(isinstance(payload, dict), f"{path} must contain a JSON object")
    return payload


def require_string(value: Any, label: str) -> str:
    expect(isinstance(value, str) and bool(value.strip()), f"{label} must be a nonempty string")
    expect(value == value.strip(), f"{label} must not carry surrounding whitespace")
    return value


def require_non_negative_int(value: Any, label: str) -> int:
    expect(type(value) is int and value >= 0, f"{label} must be a nonnegative integer")
    return value


def require_bool(value: Any, label: str, *, default: bool = False) -> bool:
    if value is None:
        return default
    expect(type(value) is bool, f"{label} must be a boolean")
    return value


def require_relative_glob(value: Any, label: str) -> str:
    pattern = require_string(value, label)
    path = Path(pattern)
    expect(not path.is_absolute(), f"{label} must be repository-relative")
    expect("\\" not in pattern, f"{label} must use POSIX path separators")
    expect(
        all(part not in {"", ".", ".."} for part in pattern.split("/")),
        f"{label} must be a safe relative POSIX glob",
    )
    expect(pattern.endswith(".md"), f"{label} must target markdown files")
    return pattern


def require_glob_list(value: Any, label: str) -> tuple[str, ...]:
    expect(isinstance(value, list), f"{label} must be a list")
    patterns = tuple(require_relative_glob(item, f"{label}[{index}]") for index, item in enumerate(value))
    duplicates = sorted(pattern for pattern in set(patterns) if patterns.count(pattern) > 1)
    expect(not duplicates, f"{label} duplicates glob(s): {', '.join(duplicates)}")
    return patterns


def parse_category(raw: Any, label: str) -> Category:
    expect(isinstance(raw, dict), f"{label} must be an object")
    category_id = require_string(raw.get("id"), f"{label}.id")
    description = require_string(raw.get("description"), f"{label}.description")
    minimum_count = require_non_negative_int(raw.get("minimum_count"), f"{label}.minimum_count")
    catch_all = require_bool(raw.get("catch_all"), f"{label}.catch_all")
    path_globs = require_glob_list(raw.get("path_globs", []), f"{label}.path_globs")
    if catch_all:
        expect(not path_globs, f"{label}.path_globs must be empty when catch_all=true")
    else:
        expect(bool(path_globs), f"{label}.path_globs must be nonempty when catch_all=false")
    return Category(
        category_id=category_id,
        description=description,
        minimum_count=minimum_count,
        path_globs=path_globs,
        catch_all=catch_all,
    )


def load_registry(path: Path) -> Registry:
    payload = read_json(path)
    expect(payload.get("schema_version") == 1, f"{path} schema_version must be 1")
    expect(payload.get("scope") == SCOPE, f"{path} scope must be {SCOPE}")
    raw_docs = payload.get("documents")
    expect(isinstance(raw_docs, dict), f"{path}.documents must be an object")
    docs = cast(dict[str, Any], raw_docs)
    include_glob = require_relative_glob(docs.get("include_glob"), f"{path}.documents.include_glob")
    expect(include_glob == "docs/release/*.md", f"{path}.documents.include_glob must be docs/release/*.md")
    raw_categories = payload.get("categories", [])
    expect(isinstance(raw_categories, list), f"{path}.categories must be a list")
    categories = tuple(
        parse_category(item, f"{path}.categories[{index}]")
        for index, item in enumerate(raw_categories)
    )
    expect(bool(categories), f"{path}.categories must be nonempty")
    ids = [category.category_id for category in categories]
    duplicate_ids = sorted(category_id for category_id in set(ids) if ids.count(category_id) > 1)
    expect(not duplicate_ids, f"{path} duplicates category id(s): {', '.join(duplicate_ids)}")
    catch_all_categories = [category for category in categories if category.catch_all]
    expect(len(catch_all_categories) == 1, f"{path} must define exactly one catch_all category")
    total_minimum_count = require_non_negative_int(
        payload.get("total_minimum_count"),
        f"{path}.total_minimum_count",
    )
    expected_total = sum(category.minimum_count for category in categories)
    expect(
        total_minimum_count == expected_total,
        f"{path}.total_minimum_count must equal the sum of category minimum_count values",
    )
    return Registry(
        include_glob=include_glob,
        categories=categories,
        total_minimum_count=total_minimum_count,
    )


def release_docs(root: Path, include_glob: str) -> list[Path]:
    return sorted(path for path in root.glob(include_glob) if path.is_file())


def classify(relative_path: str, registry: Registry) -> str:
    matches = [
        category.category_id
        for category in registry.categories
        if not category.catch_all
        and any(fnmatch.fnmatch(relative_path, pattern) for pattern in category.path_globs)
    ]
    if len(matches) > 1:
        raise ReleaseDocCountError(
            f"{relative_path} matches multiple release-doc count categories: {', '.join(matches)}"
        )
    if matches:
        return matches[0]
    catch_all = next(category for category in registry.categories if category.catch_all)
    return catch_all.category_id


def count_release_docs(root: Path, registry: Registry) -> dict[str, int]:
    counts = {category.category_id: 0 for category in registry.categories}
    for path in release_docs(root, registry.include_glob):
        relative_path = path.relative_to(root).as_posix()
        counts[classify(relative_path, registry)] += 1
    return counts


def verify_counts(root: Path, registry_path: Path) -> dict[str, Any]:
    registry = load_registry(registry_path)
    counts = count_release_docs(root, registry)
    total = sum(counts.values())
    errors: list[str] = []
    for category in registry.categories:
        actual = counts[category.category_id]
        if actual < category.minimum_count:
            errors.append(
                f"category {category.category_id} regressed: "
                f"actual={actual}, pinned_minimum={category.minimum_count}"
            )
    if total < registry.total_minimum_count:
        errors.append(
            f"total docs/release/*.md count regressed: "
            f"actual={total}, pinned_minimum={registry.total_minimum_count}"
        )
    if errors:
        raise ReleaseDocCountError("; ".join(errors))
    return {
        "registry": registry_path.relative_to(root).as_posix() if registry_path.is_relative_to(root) else str(registry_path),
        "category_counts": counts,
        "total_count": total,
        "total_minimum_count": registry.total_minimum_count,
    }


def write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def self_check_registry() -> dict[str, Any]:
    return {
        "schema_version": 1,
        "scope": SCOPE,
        "documents": {"include_glob": "docs/release/*.md"},
        "categories": [
            {
                "id": "live-rerun-X",
                "description": "Synthetic live-rerun markdown notes.",
                "path_globs": ["docs/release/amflow-live-rerun-*.md"],
                "minimum_count": 2,
            },
            {
                "id": "audit",
                "description": "Synthetic audit markdown notes.",
                "path_globs": ["docs/release/*audit*.md"],
                "minimum_count": 1,
            },
            {
                "id": "known-gaps",
                "description": "Synthetic known-gaps document.",
                "path_globs": ["docs/release/known-gaps.md"],
                "minimum_count": 1,
            },
            {
                "id": "tools",
                "description": "Synthetic tools catalog.",
                "path_globs": ["docs/release/tools.md"],
                "minimum_count": 1,
            },
            {
                "id": "others",
                "description": "Synthetic catch-all category.",
                "catch_all": True,
                "minimum_count": 1,
            },
        ],
        "total_minimum_count": 6,
    }


def write_self_check_root(root: Path, registry: dict[str, Any] | None = None) -> Path:
    registry_path = root / DEFAULT_REGISTRY
    write_text(registry_path, json.dumps(registry or self_check_registry(), indent=2, sort_keys=True) + "\n")
    for relative_path in (
        "docs/release/amflow-live-rerun-alpha.md",
        "docs/release/amflow-live-rerun-beta.md",
        "docs/release/release-audit.md",
        "docs/release/known-gaps.md",
        "docs/release/tools.md",
        "docs/release/m7-closure-evidence.md",
    ):
        write_text(root / relative_path, f"# {Path(relative_path).stem}\n")
    return registry_path


def expect_self_check_failure(label: str, mutator: Any, expected: str) -> None:
    with tempfile.TemporaryDirectory(prefix="release-doc-file-counts-") as tmp:
        root = Path(tmp)
        registry_path = write_self_check_root(root)
        mutator(root, registry_path)
        try:
            verify_counts(root, registry_path)
        except ReleaseDocCountError as exc:
            expect(expected in str(exc), f"{label} failed for the wrong reason: {exc}")
            return
        raise ReleaseDocCountError(f"{label} self-check fixture unexpectedly passed")


def run_self_check() -> None:
    with tempfile.TemporaryDirectory(prefix="release-doc-file-counts-") as tmp:
        root = Path(tmp)
        registry_path = write_self_check_root(root)
        summary = verify_counts(root, registry_path)
        expect(summary["total_count"] == 6, "valid fixture total count drifted")
        expect(summary["category_counts"]["live-rerun-X"] == 2, "valid fixture live count drifted")
        expect(summary["category_counts"]["audit"] == 1, "valid fixture audit count drifted")
        expect(summary["category_counts"]["others"] == 1, "valid fixture catch-all count drifted")

    expect_self_check_failure(
        "live-rerun-regression",
        lambda root, _registry: (root / "docs/release/amflow-live-rerun-beta.md").unlink(),
        "category live-rerun-X regressed",
    )
    expect_self_check_failure(
        "known-gaps-regression",
        lambda root, _registry: (root / "docs/release/known-gaps.md").unlink(),
        "category known-gaps regressed",
    )
    expect_self_check_failure(
        "catch-all-regression",
        lambda root, _registry: (root / "docs/release/m7-closure-evidence.md").unlink(),
        "category others regressed",
    )

    overlap_registry = self_check_registry()
    overlap_registry["categories"][1]["path_globs"] = ["docs/release/amflow-live-rerun-alpha.md"]
    expect_self_check_failure(
        "overlapping-categories",
        lambda _root, registry: registry.write_text(
            json.dumps(overlap_registry, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        ),
        "matches multiple release-doc count categories",
    )


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--registry",
        type=Path,
        default=DEFAULT_REGISTRY,
        help="Release doc file-count registry JSON.",
    )
    parser.add_argument(
        "--verify",
        action="store_true",
        help="Verify the repository against the registry.",
    )
    parser.add_argument(
        "--self-check",
        action="store_true",
        help="Run synthetic positive and negative checks for this verifier.",
    )
    args = parser.parse_args(argv)

    try:
        if args.self_check:
            run_self_check()
            print("release doc file-count verifier self-check passed")
            return 0

        root = repo_root()
        registry_path = args.registry if args.registry.is_absolute() else root / args.registry
        summary = verify_counts(root, registry_path)
        print(
            "release doc file counts verified: "
            + ", ".join(
                f"{name}={count}" for name, count in sorted(summary["category_counts"].items())
            )
            + f", total={summary['total_count']}"
        )
        return 0
    except ReleaseDocCountError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
