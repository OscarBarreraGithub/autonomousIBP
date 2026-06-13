#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-autoIBP-TBD
"""CTest gate for the pinned C++ include-order diagnostic baseline."""

from __future__ import annotations

import argparse
import json
import re
import shutil
import subprocess
import sys
import tempfile
from collections import Counter
from dataclasses import dataclass
from pathlib import Path
from typing import Any


DEFAULT_BASELINE = Path(
    "tools/reference-harness/specs/diagnostics/include-order-baseline.json"
)
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
SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx"}
SCOPED_ROOTS = ("include", "lib", "src")
THIRD_PARTY_PREFIXES = (
    "boost/",
    "ginac/",
    "yaml-cpp/",
)
THIRD_PARTY_HEADERS = {
    "gmp.h",
    "gmpxx.h",
    "mpfr.h",
}
INCLUDE_RE = re.compile(r"^\s*#\s*include\s*(?P<delimiter>[<\"])(?P<target>[^>\"]+)[>\"]")
CATEGORY_ORDER = ("self", "standard_or_system", "third_party", "project")
CATEGORY_RANK = {name: index for index, name in enumerate(CATEGORY_ORDER)}


class IncludeOrderError(RuntimeError):
    """Raised when include-order diagnostics exceed the pinned baseline."""


@dataclass(frozen=True)
class IncludeDirective:
    path: str
    line: int
    delimiter: str
    target: str
    category: str

    @property
    def rank(self) -> int:
        return CATEGORY_RANK[self.category]

    @property
    def spelling(self) -> str:
        if self.delimiter == "<":
            return f"<{self.target}>"
        return f"\"{self.target}\""

    @property
    def sort_key(self) -> str:
        return self.target.lower()


@dataclass(frozen=True)
class IncludeDiagnostic:
    path: str
    kind: str
    include: str
    previous_include: str
    message: str
    line: int

    def signature(self) -> tuple[str, str, str, str, str]:
        return (
            self.path,
            self.kind,
            self.include,
            self.previous_include,
            self.message,
        )

    def location(self) -> dict[str, int]:
        return {"line": self.line}


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise IncludeOrderError(message)


def run_git(root: Path, args: list[str]) -> bytes:
    completed = subprocess.run(
        ["git", "-C", str(root), *args],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if completed.returncode != 0:
        raise IncludeOrderError(
            f"git {' '.join(args)} failed in {root}: "
            f"{completed.stderr.decode('utf-8', errors='replace').strip()}"
        )
    return completed.stdout


def git_head(root: Path) -> str:
    return run_git(root, ["rev-parse", "HEAD"]).decode("utf-8").strip()


def tracked_files(root: Path) -> list[str]:
    output = run_git(root, ["ls-files", "-z"])
    return sorted(
        item.decode("utf-8")
        for item in output.split(b"\0")
        if item
    )


def is_scoped_cxx(path_text: str) -> bool:
    path = Path(path_text)
    return (
        bool(path.parts)
        and path.parts[0] in SCOPED_ROOTS
        and path.suffix.lower() in CXX_SOURCE_SUFFIXES
    )


def matching_public_header(root: Path, path_text: str) -> str | None:
    path = Path(path_text)
    if not path.parts or path.parts[0] != "src" or path.suffix.lower() not in SOURCE_SUFFIXES:
        return None
    stem = path.relative_to("src").with_suffix("")
    for suffix in (".hpp", ".h", ".hh", ".hxx"):
        candidate = Path("include") / "amflow" / stem.with_suffix(suffix)
        if (root / candidate).exists():
            return candidate.relative_to("include").as_posix()
    return None


def include_category(delimiter: str, target: str, self_header: str | None) -> str:
    if delimiter == "\"" and self_header == target:
        return "self"
    if delimiter == "\"":
        return "project"
    if target in THIRD_PARTY_HEADERS or target.startswith(THIRD_PARTY_PREFIXES):
        return "third_party"
    return "standard_or_system"


def parse_includes(root: Path, path_text: str) -> list[IncludeDirective]:
    path = root / path_text
    self_header = matching_public_header(root, path_text)
    try:
        lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    except OSError as exc:
        raise IncludeOrderError(f"cannot read scoped C++ file {path}: {exc}") from exc

    includes: list[IncludeDirective] = []
    for line_number, line in enumerate(lines, start=1):
        match = INCLUDE_RE.match(line)
        if match is None:
            continue
        delimiter = match.group("delimiter")
        target = match.group("target").strip()
        includes.append(
            IncludeDirective(
                path=path_text,
                line=line_number,
                delimiter=delimiter,
                target=target,
                category=include_category(delimiter, target, self_header),
            )
        )
    return includes


def category_order_diagnostics(includes: list[IncludeDirective]) -> list[IncludeDiagnostic]:
    diagnostics: list[IncludeDiagnostic] = []
    highest: IncludeDirective | None = None
    for include in includes:
        if highest is not None and include.rank < highest.rank:
            diagnostics.append(
                IncludeDiagnostic(
                    path=include.path,
                    kind="category-order",
                    include=include.spelling,
                    previous_include=highest.spelling,
                    message=(
                        f"{include.category} include appears after "
                        f"{highest.category} include; expected order is "
                        + " -> ".join(CATEGORY_ORDER)
                    ),
                    line=include.line,
                )
            )
            continue
        if highest is None or include.rank > highest.rank:
            highest = include
    return diagnostics


def sort_order_diagnostics(includes: list[IncludeDirective]) -> list[IncludeDiagnostic]:
    diagnostics: list[IncludeDiagnostic] = []
    previous: IncludeDirective | None = None
    for include in includes:
        if previous is None or previous.category != include.category:
            previous = include
            continue
        if include.category != "self" and previous.sort_key > include.sort_key:
            diagnostics.append(
                IncludeDiagnostic(
                    path=include.path,
                    kind="category-sort",
                    include=include.spelling,
                    previous_include=previous.spelling,
                    message=f"{include.category} includes are not sorted lexicographically",
                    line=include.line,
                )
            )
        previous = include
    return diagnostics


def scan_paths(root: Path, source_paths: list[str]) -> list[IncludeDiagnostic]:
    diagnostics: list[IncludeDiagnostic] = []
    for path in sorted(path for path in source_paths if is_scoped_cxx(path)):
        includes = parse_includes(root, path)
        diagnostics.extend(category_order_diagnostics(includes))
        diagnostics.extend(sort_order_diagnostics(includes))
    return sorted(
        diagnostics,
        key=lambda item: (
            item.path,
            item.line,
            item.kind,
            item.previous_include,
            item.include,
            item.message,
        ),
    )


def grouped_diagnostics(diagnostics: list[IncludeDiagnostic]) -> list[dict[str, Any]]:
    groups: dict[tuple[str, str, str, str, str], list[IncludeDiagnostic]] = {}
    for diagnostic in diagnostics:
        groups.setdefault(diagnostic.signature(), []).append(diagnostic)

    entries: list[dict[str, Any]] = []
    for signature in sorted(groups):
        path, kind, include, previous_include, message = signature
        occurrences = sorted(
            (diagnostic.location() for diagnostic in groups[signature]),
            key=lambda item: item["line"],
        )
        entries.append(
            {
                "path": path,
                "kind": kind,
                "include": include,
                "previous_include": previous_include,
                "message": message,
                "occurrences": occurrences,
            }
        )
    return entries


def iwyu_probe() -> dict[str, Any]:
    candidates = ("include-what-you-use", "iwyu", "iwyu_tool.py")
    for command in candidates:
        resolved = shutil.which(command)
        if resolved:
            return {
                "status": "available",
                "command": command,
                "resolved_path": resolved,
                "diagnostic_count": None,
                "notes": (
                    "IWYU was available to the baseline environment, but this "
                    "gate tracks include ordering only."
                ),
            }
    return {
        "status": "unavailable",
        "checked_commands": list(candidates),
        "diagnostic_count": None,
        "notes": "No include-what-you-use executable was found in PATH.",
    }


def manifest_payload(
    *,
    baseline_commit: str,
    source_paths: list[str],
    diagnostics: list[IncludeDiagnostic],
) -> dict[str, Any]:
    scoped_paths = sorted(path for path in source_paths if is_scoped_cxx(path))
    return {
        "schema_version": 1,
        "tool": "include-order-baseline",
        "scope_roots": list(SCOPED_ROOTS),
        "baseline_commit": baseline_commit,
        "convention": {
            "source_files": (
                "matching public header first when present, then standard/system "
                "angle includes, then third-party angle includes, then project "
                "quoted includes"
            ),
            "headers": (
                "standard/system angle includes, then third-party angle includes, "
                "then project quoted includes"
            ),
            "within_category": "lexicographic by include target",
        },
        "baseline_posture": (
            "Existing deviations are pinned as diagnostics. The gate allows "
            "diagnostics to be resolved, but fails on new or additional "
            "diagnostics unless the baseline is intentionally updated."
        ),
        "file_count": len(scoped_paths),
        "diagnostic_count": len(diagnostics),
        "comparison_key": [
            "path",
            "kind",
            "include",
            "previous_include",
            "message",
            "occurrence_count",
        ],
        "iwyu_baseline": iwyu_probe(),
        "diagnostics": grouped_diagnostics(diagnostics),
    }


def diagnostics_from_manifest(manifest: dict[str, Any]) -> Counter[tuple[str, str, str, str, str]]:
    raw_diagnostics = manifest.get("diagnostics")
    expect(isinstance(raw_diagnostics, list), "manifest diagnostics must be a list")
    counter: Counter[tuple[str, str, str, str, str]] = Counter()
    for index, item in enumerate(raw_diagnostics):
        expect(isinstance(item, dict), f"diagnostics[{index}] must be an object")
        path = item.get("path")
        kind = item.get("kind")
        include = item.get("include")
        previous_include = item.get("previous_include")
        message = item.get("message")
        occurrences = item.get("occurrences")
        expect(isinstance(path, str) and path.strip(), f"diagnostics[{index}].path must be a string")
        expect(isinstance(kind, str) and kind.strip(), f"diagnostics[{index}].kind must be a string")
        expect(isinstance(include, str) and include.strip(), f"diagnostics[{index}].include must be a string")
        expect(
            isinstance(previous_include, str) and previous_include.strip(),
            f"diagnostics[{index}].previous_include must be a string",
        )
        expect(isinstance(message, str) and message.strip(), f"diagnostics[{index}].message must be a string")
        expect(isinstance(occurrences, list), f"diagnostics[{index}].occurrences must be a list")
        expect(occurrences, f"diagnostics[{index}].occurrences must not be empty")
        for occurrence_index, occurrence in enumerate(occurrences):
            expect(
                isinstance(occurrence, dict),
                f"diagnostics[{index}].occurrences[{occurrence_index}] must be an object",
            )
            expect(isinstance(occurrence.get("line"), int), "include diagnostic line must be an integer")
        counter[
            (
                path.strip(),
                kind.strip(),
                include.strip(),
                previous_include.strip(),
                message.strip(),
            )
        ] += len(occurrences)
    return counter


def load_manifest(path: Path) -> Counter[tuple[str, str, str, str, str]]:
    with path.open("r", encoding="utf-8") as stream:
        manifest = json.load(stream)
    expect(isinstance(manifest, dict), "baseline manifest must contain a JSON object")
    expect(manifest.get("schema_version") == 1, "baseline manifest schema_version must be 1")
    expect(manifest.get("tool") == "include-order-baseline", "baseline manifest tool mismatch")
    scope_roots = manifest.get("scope_roots")
    expect(scope_roots == list(SCOPED_ROOTS), "baseline manifest scope_roots mismatch")
    expected = diagnostics_from_manifest(manifest)
    diagnostic_count = manifest.get("diagnostic_count")
    expect(
        isinstance(diagnostic_count, int) and diagnostic_count == sum(expected.values()),
        "baseline manifest diagnostic_count does not match diagnostics",
    )
    iwyu = manifest.get("iwyu_baseline")
    expect(isinstance(iwyu, dict), "baseline manifest iwyu_baseline must be an object")
    expect(iwyu.get("status") in {"available", "unavailable"}, "baseline IWYU status is invalid")
    return expected


def counter_from_diagnostics(
    diagnostics: list[IncludeDiagnostic],
) -> Counter[tuple[str, str, str, str, str]]:
    return Counter(diagnostic.signature() for diagnostic in diagnostics)


def format_signature(signature: tuple[str, str, str, str, str]) -> str:
    path, kind, include, previous_include, message = signature
    return f"{path}: {kind}: {include} after {previous_include}: {message}"


def regression_message(
    expected: Counter[tuple[str, str, str, str, str]],
    actual: Counter[tuple[str, str, str, str, str]],
) -> str | None:
    regressions: list[str] = []
    for signature in sorted(actual):
        excess = actual[signature] - expected.get(signature, 0)
        if excess > 0:
            regressions.append(f"  + {excess}x {format_signature(signature)}")
    if not regressions:
        return None
    return "\n".join(
        [
            "include-order baseline regression detected; update the baseline only after review",
            *regressions,
        ]
    )


def write_baseline(root: Path, baseline: Path) -> int:
    source_paths = tracked_files(root)
    diagnostics = scan_paths(root, source_paths)
    payload = manifest_payload(
        baseline_commit=git_head(root),
        source_paths=source_paths,
        diagnostics=diagnostics,
    )
    baseline.parent.mkdir(parents=True, exist_ok=True)
    baseline.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    print(
        f"wrote include-order baseline: {len(diagnostics)} diagnostics "
        f"across {payload['file_count']} scoped files"
    )
    print(f"IWYU probe: {payload['iwyu_baseline']['status']}")
    return 0


def verify_baseline(root: Path, baseline: Path) -> int:
    expected = load_manifest(baseline)
    diagnostics = scan_paths(root, tracked_files(root))
    actual = counter_from_diagnostics(diagnostics)
    message = regression_message(expected, actual)
    if message is not None:
        raise IncludeOrderError(message)
    resolved = sum((expected - actual).values())
    print(
        f"include-order baseline verified: {sum(actual.values())} diagnostics "
        f"within pinned {sum(expected.values())}"
    )
    if resolved:
        print(f"include-order baseline note: {resolved} pinned diagnostics are no longer present")
    return 0


def run_self_check() -> None:
    with tempfile.TemporaryDirectory() as tmpdir:
        root = Path(tmpdir)
        (root / "include" / "amflow" / "core").mkdir(parents=True)
        (root / "src" / "core").mkdir(parents=True)
        header = root / "include" / "amflow" / "core" / "sample.hpp"
        header.write_text(
            "#pragma once\n\n"
            "#include \"amflow/core/late.hpp\"\n"
            "#include <vector>\n",
            encoding="utf-8",
        )
        source = root / "src" / "core" / "sample.cpp"
        source.write_text(
            "#include \"amflow/core/sample.hpp\"\n\n"
            "#include <vector>\n"
            "#include <algorithm>\n\n"
            "#include \"amflow/core/late.hpp\"\n",
            encoding="utf-8",
        )
        diagnostics = scan_paths(
            root,
            [
                "include/amflow/core/sample.hpp",
                "src/core/sample.cpp",
            ],
        )
        expect(
            any(diagnostic.kind == "category-order" for diagnostic in diagnostics),
            "self-check should detect project-before-standard category regression",
        )
        expect(
            any(diagnostic.kind == "category-sort" for diagnostic in diagnostics),
            "self-check should detect unsorted standard include regression",
        )
        payload = manifest_payload(
            baseline_commit="0" * 40,
            source_paths=[
                "include/amflow/core/sample.hpp",
                "src/core/sample.cpp",
            ],
            diagnostics=diagnostics,
        )
        expected = diagnostics_from_manifest(payload)
        actual = counter_from_diagnostics(diagnostics)
        expect(regression_message(expected, actual) is None, "matching baseline should pass")
        extra = Counter(actual)
        first_signature = next(iter(actual))
        extra[first_signature] += 1
        expect(regression_message(expected, extra) is not None, "extra diagnostic should fail")


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", type=Path, default=DEFAULT_BASELINE)
    parser.add_argument("--write-baseline", action="store_true")
    parser.add_argument("--self-check", action="store_true")
    args = parser.parse_args(argv)

    try:
        if args.self_check:
            run_self_check()
            print("include-order baseline verifier self-check passed")
            return 0

        root = repo_root()
        baseline = args.baseline if args.baseline.is_absolute() else root / args.baseline
        if args.write_baseline:
            return write_baseline(root, baseline)
        return verify_baseline(root, baseline)
    except (IncludeOrderError, OSError, json.JSONDecodeError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
