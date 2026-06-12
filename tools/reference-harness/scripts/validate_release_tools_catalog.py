#!/usr/bin/env python3
"""Validate the release tooling catalog against CTest-wired scripts."""

from __future__ import annotations

import argparse
import re
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path


MODE_FLAGS = ("--self-check", "--verify")
SCRIPTS_ROOT = Path("tools/reference-harness/scripts")
CATALOG_PATH = Path("docs/release/tools.md")
CMAKE_PATH = Path("CMakeLists.txt")
SCRIPT_REF_RE = re.compile(r"tools/reference-harness/scripts/([A-Za-z0-9_]+\.py)")
ADD_TEST_RE = re.compile(r"add_test\s*\((.*?)\)", re.S)
TEST_NAME_RE = re.compile(r"\bNAME\s+([^\s\)]+)")
TOOL_ROW_RE = re.compile(
    r"^\|\s*\[`([^`]+\.py)`\]\(([^)]+)\)\s*\|\s*(.*?)\s*\|\s*(.*?)\s*\|",
    re.M,
)
TOOL_LINK_RE = re.compile(r"\[`([^`]+\.py)`\]\(([^)]+)\)")
PLACEHOLDER_RE = re.compile(r"\b(?:todo|tbd|placeholder)\b", re.I)


@dataclass(frozen=True)
class ToolRow:
    name: str
    link: str
    primary_use: str
    ci_coverage: str


class CatalogError(RuntimeError):
    """Raised when the release tools catalog is inconsistent."""


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise CatalogError(message)


def strip_fragment(link: str) -> str:
    return link.split("#", 1)[0]


def resolve_catalog_link(root: Path, link: str) -> Path:
    raw_path = Path(strip_fragment(link))
    if raw_path.is_absolute():
        return raw_path
    return (root / CATALOG_PATH.parent / raw_path).resolve()


def script_has_mode(root: Path, script_name: str) -> bool:
    script_path = root / SCRIPTS_ROOT / script_name
    if not script_path.is_file():
        return False
    text = script_path.read_text(encoding="utf-8")
    return any(flag in text for flag in MODE_FLAGS)


def ctest_script_coverage(root: Path) -> dict[str, set[str]]:
    cmake_path = root / CMAKE_PATH
    expect(cmake_path.is_file(), f"missing {CMAKE_PATH}")
    cmake_text = cmake_path.read_text(encoding="utf-8")
    coverage: dict[str, set[str]] = {}
    for match in ADD_TEST_RE.finditer(cmake_text):
        block = match.group(1)
        script_match = SCRIPT_REF_RE.search(block)
        if script_match is None:
            continue
        name_match = TEST_NAME_RE.search(block)
        expect(name_match is not None, f"CTest block for {script_match.group(1)} has no NAME")
        coverage.setdefault(script_match.group(1), set()).add(name_match.group(1))
    return coverage


def required_catalog_scripts(root: Path) -> dict[str, set[str]]:
    required: dict[str, set[str]] = {}
    for script_name, test_names in ctest_script_coverage(root).items():
        script_path = root / SCRIPTS_ROOT / script_name
        expect(script_path.is_file(), f"CTest references missing script {SCRIPTS_ROOT / script_name}")
        if script_has_mode(root, script_name):
            required[script_name] = set(test_names)
    return required


def catalog_rows(root: Path) -> dict[str, ToolRow]:
    catalog = root / CATALOG_PATH
    expect(catalog.is_file(), f"missing {CATALOG_PATH}")
    text = catalog.read_text(encoding="utf-8")
    rows: dict[str, ToolRow] = {}
    duplicates: list[str] = []
    for match in TOOL_ROW_RE.finditer(text):
        name, link, primary_use, ci_coverage = (part.strip() for part in match.groups())
        if name in rows:
            duplicates.append(name)
        rows[name] = ToolRow(name, link, primary_use, ci_coverage)
    expect(not duplicates, "duplicate tool catalog rows: " + ", ".join(sorted(duplicates)))
    return rows


def catalog_tool_links(root: Path) -> list[tuple[str, str]]:
    catalog = root / CATALOG_PATH
    expect(catalog.is_file(), f"missing {CATALOG_PATH}")
    text = catalog.read_text(encoding="utf-8")
    return [(match.group(1), match.group(2)) for match in TOOL_LINK_RE.finditer(text)]


def verify_catalog(root: Path) -> tuple[int, int]:
    root = root.resolve()
    required = required_catalog_scripts(root)
    rows = catalog_rows(root)

    missing = sorted(set(required) - set(rows))
    expect(
        not missing,
        "CTest-wired mode-capable scripts missing from docs/release/tools.md: "
        + ", ".join(missing),
    )

    for label, link in catalog_tool_links(root):
        target = resolve_catalog_link(root, link)
        expect(target.is_file(), f"catalog link for {label} does not exist: {link}")
        expect(target.name == label, f"catalog link label {label} points at {target.name}")
        try:
            target.relative_to((root / SCRIPTS_ROOT).resolve())
        except ValueError as exc:
            raise CatalogError(
                f"catalog tool link for {label} escapes {SCRIPTS_ROOT}: {link}"
            ) from exc

    for script_name, test_names in sorted(required.items()):
        row = rows[script_name]
        expect(row.primary_use, f"{script_name} catalog row has an empty primary-use cell")
        expect(
            PLACEHOLDER_RE.search(row.primary_use) is None,
            f"{script_name} catalog row still contains placeholder primary-use text",
        )
        for test_name in sorted(test_names):
            expect(
                f"`{test_name}`" in row.ci_coverage,
                f"{script_name} catalog row omits CTest coverage `{test_name}`",
            )

    return len(required), len(catalog_tool_links(root))


def write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def make_fixture(root: Path, docs_text: str) -> None:
    write_text(
        root / CMAKE_PATH,
        """
include(CTest)
add_test(
  NAME alpha-self-check
  COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/alpha.py
    --self-check)
add_test(
  NAME beta-regular
  COMMAND ${Python3_EXECUTABLE}
    ${CMAKE_CURRENT_SOURCE_DIR}/tools/reference-harness/scripts/beta.py)
""".lstrip(),
    )
    write_text(root / SCRIPTS_ROOT / "alpha.py", '"""Alpha tool."""\n# --self-check\n')
    write_text(root / SCRIPTS_ROOT / "beta.py", '"""Beta tool without a self mode."""\n')
    write_text(root / CATALOG_PATH, docs_text)


def run_self_check() -> None:
    valid_docs = """
# Release Tooling Catalog

| Tool | Primary use | CI coverage |
| --- | --- | --- |
| [`alpha.py`](../../tools/reference-harness/scripts/alpha.py) | Validate alpha catalog behavior. | `alpha-self-check`. |
""".lstrip()
    with tempfile.TemporaryDirectory(prefix="release-tools-catalog-") as tmp:
        fixture = Path(tmp)
        make_fixture(fixture, valid_docs)
        verify_catalog(fixture)

    missing_docs = """
# Release Tooling Catalog

| Tool | Primary use | CI coverage |
| --- | --- | --- |
""".lstrip()
    with tempfile.TemporaryDirectory(prefix="release-tools-catalog-") as tmp:
        fixture = Path(tmp)
        make_fixture(fixture, missing_docs)
        try:
            verify_catalog(fixture)
        except CatalogError as exc:
            expect("missing from docs/release/tools.md" in str(exc), "unexpected missing-row error")
        else:
            raise CatalogError("self-check fixture failed to reject a missing catalog row")

    missing_coverage = """
# Release Tooling Catalog

| Tool | Primary use | CI coverage |
| --- | --- | --- |
| [`alpha.py`](../../tools/reference-harness/scripts/alpha.py) | Validate alpha catalog behavior. | none. |
""".lstrip()
    with tempfile.TemporaryDirectory(prefix="release-tools-catalog-") as tmp:
        fixture = Path(tmp)
        make_fixture(fixture, missing_coverage)
        try:
            verify_catalog(fixture)
        except CatalogError as exc:
            expect("omits CTest coverage" in str(exc), "unexpected coverage error")
        else:
            raise CatalogError("self-check fixture failed to reject missing CTest coverage")

    broken_link = """
# Release Tooling Catalog

| Tool | Primary use | CI coverage |
| --- | --- | --- |
| [`alpha.py`](../../tools/reference-harness/scripts/missing.py) | Validate alpha catalog behavior. | `alpha-self-check`. |
""".lstrip()
    with tempfile.TemporaryDirectory(prefix="release-tools-catalog-") as tmp:
        fixture = Path(tmp)
        make_fixture(fixture, broken_link)
        try:
            verify_catalog(fixture)
        except CatalogError as exc:
            expect("does not exist" in str(exc), "unexpected broken-link error")
        else:
            raise CatalogError("self-check fixture failed to reject a broken tool link")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--verify",
        action="store_true",
        help="Validate committed docs/release/tools.md against CTest-wired tools. This is the default.",
    )
    parser.add_argument(
        "--self-check",
        action="store_true",
        help="Run synthetic positive and negative checks for this validator.",
    )
    args = parser.parse_args(argv)

    try:
        if args.self_check:
            run_self_check()
            print("release tools catalog validator self-check passed")
            return 0
        required_count, link_count = verify_catalog(repo_root())
        print(
            "release tools catalog verified: "
            f"{required_count} CTest-wired mode-capable scripts cataloged; "
            f"{link_count} tool links resolve"
        )
        return 0
    except CatalogError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
