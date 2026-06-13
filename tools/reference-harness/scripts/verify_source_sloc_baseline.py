#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-autoIBP-TBD
"""Verify or write the pinned source SLOC baseline."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from datetime import datetime, timezone
from decimal import Decimal, InvalidOperation, ROUND_CEILING, ROUND_FLOOR
from pathlib import Path
from typing import Any, Callable


DEFAULT_BASELINE = Path("tools/reference-harness/specs/release/source-sloc-baseline.json")
SCOPE = "total-source-sloc-baseline"
SCOPED_ROOTS = ("lib", "include", "src", "tools")
LANGUAGE_CATEGORIES = ("cpp", "python", "cmake")
ALL_CATEGORIES = (*LANGUAGE_CATEGORIES, "total")
DEFAULT_TOLERANCE_PERCENT = Decimal("5")
CXX_SOURCE_SUFFIXES = {
    ".c",
    ".cc",
    ".cpp",
    ".cxx",
    ".h",
    ".hh",
    ".hpp",
    ".hxx",
    ".ipp",
    ".ixx",
}


class SourceSlocError(RuntimeError):
    """Raised when source SLOC leaves the pinned baseline window."""


@dataclass(frozen=True)
class SourceCount:
    file_count: int
    source_lines: int


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def utc_now() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise SourceSlocError(message)


def run_git(root: Path, args: list[str]) -> bytes:
    completed = subprocess.run(
        ["git", "-C", str(root), *args],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if completed.returncode != 0:
        raise SourceSlocError(
            f"git {' '.join(args)} failed in {root}: "
            f"{completed.stderr.decode('utf-8', errors='replace').strip()}"
        )
    return completed.stdout


def git_head(root: Path) -> str:
    return run_git(root, ["rev-parse", "HEAD"]).decode("utf-8").strip()


def tracked_files(root: Path) -> list[str]:
    output = run_git(root, ["ls-files", "-z", "--", *SCOPED_ROOTS])
    return sorted(item.decode("utf-8") for item in output.split(b"\0") if item)


def language_for_path(path_text: str) -> str | None:
    path = Path(path_text)
    if not path.parts or path.parts[0] not in SCOPED_ROOTS:
        return None
    suffix = path.suffix.lower()
    if suffix in CXX_SOURCE_SUFFIXES:
        return "cpp"
    if suffix == ".py":
        return "python"
    if path.name == "CMakeLists.txt" or suffix == ".cmake":
        return "cmake"
    return None


def cxx_source_lines(text: str) -> int:
    count = 0
    in_block_comment = False
    for raw_line in text.splitlines():
        index = 0
        has_code = False
        while index < len(raw_line):
            if in_block_comment:
                end = raw_line.find("*/", index)
                if end == -1:
                    index = len(raw_line)
                    continue
                in_block_comment = False
                index = end + 2
                continue

            char = raw_line[index]
            next_char = raw_line[index + 1] if index + 1 < len(raw_line) else ""
            if char.isspace():
                index += 1
                continue
            if char == "/" and next_char == "*":
                in_block_comment = True
                index += 2
                continue
            if char == "/" and next_char == "/":
                break
            has_code = True
            break
        if has_code:
            count += 1
    return count


def hash_comment_source_lines(text: str) -> int:
    count = 0
    for raw_line in text.splitlines():
        stripped = raw_line.strip()
        if stripped and not stripped.startswith("#"):
            count += 1
    return count


def source_lines_for_language(text: str, language: str) -> int:
    if language == "cpp":
        return cxx_source_lines(text)
    if language in {"python", "cmake"}:
        return hash_comment_source_lines(text)
    raise SourceSlocError(f"unsupported language category: {language}")


def scan_sources(root: Path) -> dict[str, SourceCount]:
    files_by_language: dict[str, list[str]] = {language: [] for language in LANGUAGE_CATEGORIES}
    for path_text in tracked_files(root):
        language = language_for_path(path_text)
        if language is not None:
            files_by_language[language].append(path_text)

    counts: dict[str, SourceCount] = {}
    total_files = 0
    total_lines = 0
    for language in LANGUAGE_CATEGORIES:
        source_lines = 0
        for path_text in files_by_language[language]:
            path = root / path_text
            text = path.read_text(encoding="utf-8", errors="replace")
            source_lines += source_lines_for_language(text, language)
        file_count = len(files_by_language[language])
        counts[language] = SourceCount(file_count=file_count, source_lines=source_lines)
        total_files += file_count
        total_lines += source_lines
    counts["total"] = SourceCount(file_count=total_files, source_lines=total_lines)
    return counts


def decimal_field(payload: dict[str, Any], field: str) -> Decimal:
    raw = payload.get(field)
    expect(isinstance(raw, (int, float, str)), f"{field} must be numeric")
    try:
        value = Decimal(str(raw))
    except InvalidOperation as exc:
        raise SourceSlocError(f"{field} must be a valid decimal") from exc
    expect(value >= 0, f"{field} must be nonnegative")
    expect(value < 100, f"{field} must be less than 100")
    return value


def window_for(source_lines: int, tolerance_percent: Decimal) -> tuple[int, int]:
    scale = Decimal(100)
    minimum = (
        Decimal(source_lines) * (scale - tolerance_percent) / scale
    ).to_integral_value(rounding=ROUND_CEILING)
    maximum = (
        Decimal(source_lines) * (scale + tolerance_percent) / scale
    ).to_integral_value(rounding=ROUND_FLOOR)
    return int(minimum), int(maximum)


def category_payload(name: str, count: SourceCount) -> dict[str, Any]:
    minimum, maximum = window_for(count.source_lines, DEFAULT_TOLERANCE_PERCENT)
    return {
        "category": name,
        "file_count": count.file_count,
        "source_lines": count.source_lines,
        "tolerance_percent": int(DEFAULT_TOLERANCE_PERCENT),
        "min_source_lines": minimum,
        "max_source_lines": maximum,
    }


def manifest_payload(root: Path, *, baseline_commit: str | None = None) -> dict[str, Any]:
    counts = scan_sources(root)
    return {
        "schema_version": 1,
        "scope": SCOPE,
        "tool": "source-sloc-baseline",
        "baseline_commit": baseline_commit or git_head(root),
        "baseline_captured_at_utc": utc_now(),
        "source_roots": list(SCOPED_ROOTS),
        "counting_policy": (
            "Tracked files under lib/, include/, src/, and tools/ are grouped as "
            "C++ (.c/.cc/.cpp/.cxx/.h/.hh/.hpp/.hxx/.ipp/.ixx), Python (.py), "
            "or CMake (CMakeLists.txt/.cmake). Blank lines and leading whole-line "
            "comments are excluded; C++ block and line comments are stripped before "
            "counting physical source lines."
        ),
        "categories": [category_payload(name, counts[name]) for name in ALL_CATEGORIES],
    }


def read_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as stream:
        payload = json.load(stream)
    expect(isinstance(payload, dict), f"{path} must contain a JSON object")
    return payload


def require_int(item: dict[str, Any], field: str) -> int:
    value = item.get(field)
    if type(value) is not int or value < 0:
        raise SourceSlocError(
            f"{item.get('category', '<unknown>')}.{field} must be a nonnegative integer"
        )
    return value


def validate_manifest(manifest: dict[str, Any]) -> None:
    expect(manifest.get("schema_version") == 1, "schema_version must be 1")
    expect(manifest.get("scope") == SCOPE, f"scope must be {SCOPE}")
    expect(manifest.get("tool") == "source-sloc-baseline", "tool must be source-sloc-baseline")
    expect(manifest.get("source_roots") == list(SCOPED_ROOTS), "source_roots must match the scoped source roots")
    expect(
        isinstance(manifest.get("counting_policy"), str) and manifest["counting_policy"].strip(),
        "counting_policy must be a non-empty string",
    )
    raw_categories = manifest.get("categories")
    if not isinstance(raw_categories, list):
        raise SourceSlocError("categories must be a list")

    categories: dict[str, dict[str, Any]] = {}
    for index, raw_item in enumerate(raw_categories):
        if not isinstance(raw_item, dict):
            raise SourceSlocError(f"categories[{index}] must be an object")
        item: dict[str, Any] = raw_item
        category = item.get("category")
        if not isinstance(category, str) or category not in ALL_CATEGORIES:
            raise SourceSlocError(f"categories[{index}].category is unsupported")
        expect(category not in categories, f"duplicate category: {category}")
        source_lines = require_int(item, "source_lines")
        require_int(item, "file_count")
        tolerance = decimal_field(item, "tolerance_percent")
        expected_minimum, expected_maximum = window_for(source_lines, tolerance)
        expect(
            require_int(item, "min_source_lines") == expected_minimum,
            f"{category}.min_source_lines does not match tolerance",
        )
        expect(
            require_int(item, "max_source_lines") == expected_maximum,
            f"{category}.max_source_lines does not match tolerance",
        )
        expect(
            item["min_source_lines"] <= source_lines <= item["max_source_lines"],
            f"{category} baseline is outside its own window",
        )
        categories[category] = item

    missing = sorted(set(ALL_CATEGORIES) - set(categories))
    expect(not missing, "missing categories: " + ", ".join(missing))
    total_lines = sum(categories[name]["source_lines"] for name in LANGUAGE_CATEGORIES)
    total_files = sum(categories[name]["file_count"] for name in LANGUAGE_CATEGORIES)
    expect(categories["total"]["source_lines"] == total_lines, "total.source_lines must equal the language sum")
    expect(categories["total"]["file_count"] == total_files, "total.file_count must equal the language sum")


def load_manifest(path: Path) -> dict[str, Any]:
    manifest = read_json(path)
    validate_manifest(manifest)
    return manifest


def write_baseline(root: Path, baseline_path: Path) -> int:
    payload = manifest_payload(root)
    validate_manifest(payload)
    baseline_path.parent.mkdir(parents=True, exist_ok=True)
    baseline_path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    total = next(item for item in payload["categories"] if item["category"] == "total")
    print(
        "wrote source SLOC baseline: "
        f"{total['source_lines']} source lines across {total['file_count']} files"
    )
    return 0


def verify_baseline(root: Path, baseline_path: Path) -> int:
    manifest = load_manifest(baseline_path)
    current = scan_sources(root)
    categories = {item["category"]: item for item in manifest["categories"]}
    problems: list[str] = []
    for category in ALL_CATEGORIES:
        baseline = categories[category]
        actual = current[category].source_lines
        if not baseline["min_source_lines"] <= actual <= baseline["max_source_lines"]:
            problems.append(
                f"{category} source line count outside pinned window: "
                f"actual={actual}, baseline={baseline['source_lines']}, "
                f"window=[{baseline['min_source_lines']}, {baseline['max_source_lines']}], "
                f"tolerance={baseline['tolerance_percent']}%"
            )
    if problems:
        raise SourceSlocError("\n".join(problems))
    print(
        "source SLOC baseline verified: "
        + ", ".join(f"{category}={current[category].source_lines}" for category in ALL_CATEGORIES)
    )
    return 0


def expect_failure(action: Callable[[], object], expected: str) -> None:
    try:
        action()
    except SourceSlocError as exc:
        expect(expected in str(exc), f"unexpected self-check error: {exc}")
        return
    raise SourceSlocError("self-check fixture unexpectedly passed")


def write_fixture(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def make_fixture(root: Path) -> None:
    write_fixture(
        root / "include/amflow/example.hpp",
        """
// leading comment
#pragma once

int example();
""".lstrip(),
    )
    write_fixture(
        root / "src/example.cpp",
        """
/*
 * block comment
 */
#include "amflow/example.hpp"
int example() { return 1; } // trailing comment
""".lstrip(),
    )
    write_fixture(
        root / "tools/example.py",
        """
# comment
def main() -> int:
    return 0
""".lstrip(),
    )
    write_fixture(
        root / "tools/CMakeLists.txt",
        """
# comment
add_custom_target(example)
""".lstrip(),
    )
    run_git(root, ["init", "-q"])
    run_git(root, ["add", "include", "src", "tools"])


def run_self_check() -> None:
    with tempfile.TemporaryDirectory(prefix="source-sloc-baseline-") as temp_dir:
        fixture = Path(temp_dir)
        make_fixture(fixture)
        counts = scan_sources(fixture)
        expect(counts["cpp"] == SourceCount(file_count=2, source_lines=4), "fixture C++ count mismatch")
        expect(counts["python"] == SourceCount(file_count=1, source_lines=2), "fixture Python count mismatch")
        expect(counts["cmake"] == SourceCount(file_count=1, source_lines=1), "fixture CMake count mismatch")
        expect(counts["total"] == SourceCount(file_count=4, source_lines=7), "fixture total count mismatch")

        baseline_path = fixture / "baseline.json"
        payload = manifest_payload(fixture, baseline_commit="0" * 40)
        baseline_path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
        verify_baseline(fixture, baseline_path)

        bad_window = json.loads(json.dumps(payload))
        bad_window["categories"][0]["max_source_lines"] = 3
        baseline_path.write_text(json.dumps(bad_window, indent=2) + "\n", encoding="utf-8")
        expect_failure(lambda: load_manifest(baseline_path), "max_source_lines")

        bad_total = json.loads(json.dumps(payload))
        bad_total["categories"][-1]["source_lines"] = 6
        bad_total["categories"][-1]["min_source_lines"], bad_total["categories"][-1]["max_source_lines"] = window_for(
            6, Decimal(str(bad_total["categories"][-1]["tolerance_percent"]))
        )
        baseline_path.write_text(json.dumps(bad_total, indent=2) + "\n", encoding="utf-8")
        expect_failure(lambda: load_manifest(baseline_path), "total.source_lines")

        write_fixture(fixture / "src/extra.cpp", "int extra() { return 2; }\n")
        run_git(fixture, ["add", "src/extra.cpp"])
        baseline_path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
        expect_failure(lambda: verify_baseline(fixture, baseline_path), "outside pinned window")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--baseline",
        type=Path,
        default=DEFAULT_BASELINE,
        help="Pinned source SLOC baseline JSON.",
    )
    parser.add_argument(
        "--write-baseline",
        action="store_true",
        help="Rewrite the baseline from the current tracked source tree.",
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
            print("source SLOC baseline verifier self-check passed")
            return 0

        root = repo_root()
        baseline_path = args.baseline if args.baseline.is_absolute() else root / args.baseline
        if args.write_baseline:
            return write_baseline(root, baseline_path)
        return verify_baseline(root, baseline_path)
    except (SourceSlocError, OSError, json.JSONDecodeError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
