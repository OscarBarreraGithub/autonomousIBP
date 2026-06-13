#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-autoIBP-TBD
"""Validate release-doc metadata fields against the pinned class registry."""

from __future__ import annotations

import argparse
import fnmatch
import json
import re
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Any


REGISTRY_PATH = Path("tools/reference-harness/specs/release/release-doc-metadata-fields.registry.json")
DEFAULT_FLAGS = re.MULTILINE


class MetadataFieldError(RuntimeError):
    """Raised when release-doc metadata validation cannot continue."""


@dataclass(frozen=True)
class RequiredField:
    field_id: str
    pattern: str
    min_count: int
    max_count: int | None
    description: str


@dataclass(frozen=True)
class RowRequirement:
    column: str
    pattern: str
    description: str


@dataclass(frozen=True)
class RequiredTable:
    table_id: str
    section: str | None
    columns: tuple[str, ...]
    min_rows: int
    row_requirements: tuple[RowRequirement, ...]


@dataclass(frozen=True)
class DocClass:
    class_id: str
    description: str
    path_globs: tuple[str, ...]
    required_fields: tuple[RequiredField, ...]
    required_tables: tuple[RequiredTable, ...]


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise MetadataFieldError(message)


def require_string(value: Any, label: str) -> str:
    expect(isinstance(value, str) and value.strip(), f"{label} must be a nonempty string")
    return value


def require_string_list(value: Any, label: str) -> tuple[str, ...]:
    expect(isinstance(value, list) and value, f"{label} must be a nonempty list")
    entries: list[str] = []
    for index, item in enumerate(value):
        entries.append(require_string(item, f"{label}[{index}]").strip())
    return tuple(entries)


def require_int(value: Any, label: str, *, default: int | None = None) -> int:
    if value is None and default is not None:
        return default
    expect(isinstance(value, int) and value >= 0, f"{label} must be a nonnegative integer")
    return value


def parse_required_field(raw: Any, label: str) -> RequiredField:
    expect(isinstance(raw, dict), f"{label} must be an object")
    field_id = require_string(raw.get("id"), f"{label}.id").strip()
    pattern = require_string(raw.get("pattern"), f"{label}.pattern")
    min_count = require_int(raw.get("min_count"), f"{label}.min_count", default=1)
    max_value = raw.get("max_count")
    max_count: int | None = None
    if max_value is not None:
        max_count = require_int(max_value, f"{label}.max_count")
        expect(max_count >= min_count, f"{label}.max_count must be >= min_count")
    description = require_string(raw.get("description"), f"{label}.description")
    try:
        re.compile(pattern, DEFAULT_FLAGS)
    except re.error as exc:
        raise MetadataFieldError(f"{label}.pattern is invalid regex: {exc}") from exc
    return RequiredField(field_id, pattern, min_count, max_count, description)


def parse_row_requirement(raw: Any, label: str) -> RowRequirement:
    expect(isinstance(raw, dict), f"{label} must be an object")
    column = require_string(raw.get("column"), f"{label}.column").strip()
    pattern = require_string(raw.get("pattern"), f"{label}.pattern")
    description = require_string(raw.get("description"), f"{label}.description")
    try:
        re.compile(pattern, DEFAULT_FLAGS)
    except re.error as exc:
        raise MetadataFieldError(f"{label}.pattern is invalid regex: {exc}") from exc
    return RowRequirement(column, pattern, description)


def parse_required_table(raw: Any, label: str) -> RequiredTable:
    expect(isinstance(raw, dict), f"{label} must be an object")
    table_id = require_string(raw.get("id"), f"{label}.id").strip()
    section_value = raw.get("section")
    section = None if section_value is None else require_string(section_value, f"{label}.section").strip()
    columns = require_string_list(raw.get("columns"), f"{label}.columns")
    min_rows = require_int(raw.get("min_rows"), f"{label}.min_rows", default=1)
    row_requirements = tuple(
        parse_row_requirement(item, f"{label}.row_requirements[{index}]")
        for index, item in enumerate(raw.get("row_requirements", []))
    )
    known_columns = set(columns)
    for requirement in row_requirements:
        expect(
            requirement.column in known_columns,
            f"{label}.row_requirements references unknown column {requirement.column!r}",
        )
    return RequiredTable(table_id, section, columns, min_rows, row_requirements)


def parse_doc_class(raw: Any, label: str) -> DocClass:
    expect(isinstance(raw, dict), f"{label} must be an object")
    class_id = require_string(raw.get("id"), f"{label}.id").strip()
    description = require_string(raw.get("description"), f"{label}.description")
    path_globs = require_string_list(raw.get("path_globs"), f"{label}.path_globs")
    required_fields = tuple(
        parse_required_field(item, f"{label}.required_fields[{index}]")
        for index, item in enumerate(raw.get("required_fields", []))
    )
    required_tables = tuple(
        parse_required_table(item, f"{label}.required_tables[{index}]")
        for index, item in enumerate(raw.get("required_tables", []))
    )
    expect(required_fields or required_tables, f"{label} must require at least one metadata field or table")
    field_ids = [field.field_id for field in required_fields]
    duplicate_fields = sorted(field for field in set(field_ids) if field_ids.count(field) > 1)
    expect(not duplicate_fields, f"{label} duplicates required field ids: {', '.join(duplicate_fields)}")
    table_ids = [table.table_id for table in required_tables]
    duplicate_tables = sorted(table for table in set(table_ids) if table_ids.count(table) > 1)
    expect(not duplicate_tables, f"{label} duplicates required table ids: {', '.join(duplicate_tables)}")
    return DocClass(class_id, description, path_globs, required_fields, required_tables)


def load_registry(path: Path) -> tuple[tuple[str, ...], tuple[DocClass, ...]]:
    expect(path.is_file(), f"{path} is missing")
    try:
        payload = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        raise MetadataFieldError(f"{path} is not valid JSON: {exc}") from exc
    expect(isinstance(payload, dict), f"{path} must contain a JSON object")
    expect(payload.get("schema_version") == 1, f"{path} schema_version must be 1")
    expect(payload.get("scope") == "release-doc-metadata-fields", f"{path} scope drifted")
    docs = payload.get("documents")
    expect(isinstance(docs, dict), f"{path}.documents must be an object")
    include_globs = require_string_list(docs.get("include_globs"), f"{path}.documents.include_globs")
    classes = tuple(
        parse_doc_class(item, f"{path}.classes[{index}]")
        for index, item in enumerate(payload.get("classes", []))
    )
    expect(classes, f"{path}.classes must be a nonempty list")
    class_ids = [doc_class.class_id for doc_class in classes]
    duplicates = sorted(class_id for class_id in set(class_ids) if class_ids.count(class_id) > 1)
    expect(not duplicates, f"{path} duplicates class ids: {', '.join(duplicates)}")
    return include_globs, classes


def included_docs(root: Path, include_globs: tuple[str, ...]) -> list[Path]:
    paths: set[Path] = set()
    for pattern in include_globs:
        paths.update(path for path in root.glob(pattern) if path.is_file())
    return sorted(paths)


def matches_any(relative_path: str, patterns: tuple[str, ...]) -> bool:
    return any(fnmatch.fnmatch(relative_path, pattern) for pattern in patterns)


def matching_classes(relative_path: str, classes: tuple[DocClass, ...]) -> list[DocClass]:
    return [doc_class for doc_class in classes if matches_any(relative_path, doc_class.path_globs)]


def validate_fields(relative_path: str, text: str, doc_class: DocClass, errors: list[str]) -> None:
    for field in doc_class.required_fields:
        matches = re.findall(field.pattern, text, DEFAULT_FLAGS)
        count = len(matches)
        if count < field.min_count:
            errors.append(
                f"{relative_path} ({doc_class.class_id}) missing required metadata field "
                f"`{field.field_id}`: expected at least {field.min_count}, found {count}; "
                f"{field.description}"
            )
        if field.max_count is not None and count > field.max_count:
            errors.append(
                f"{relative_path} ({doc_class.class_id}) has too many metadata field "
                f"`{field.field_id}` matches: expected at most {field.max_count}, found {count}"
            )


def section_lines(text: str, section: str | None) -> list[str]:
    lines = text.splitlines()
    if section is None:
        return lines
    heading_re = re.compile(rf"^##\s+{re.escape(section)}\s*$")
    start: int | None = None
    for index, line in enumerate(lines):
        if heading_re.match(line):
            start = index + 1
            break
    if start is None:
        return []
    end = len(lines)
    for index in range(start, len(lines)):
        if lines[index].startswith("## "):
            end = index
            break
    return lines[start:end]


def split_markdown_table_row(line: str) -> list[str] | None:
    stripped = line.strip()
    if not stripped.startswith("|") or not stripped.endswith("|"):
        return None
    return [cell.strip() for cell in stripped.strip("|").split("|")]


def valid_separator(cells: list[str]) -> bool:
    return bool(cells) and all(set(cell) <= {"-", ":"} and "-" in cell for cell in cells)


def find_table(lines: list[str], columns: tuple[str, ...]) -> list[list[str]] | None:
    expected = list(columns)
    for index, line in enumerate(lines):
        header = split_markdown_table_row(line)
        if header != expected:
            continue
        if index + 1 >= len(lines):
            return []
        separator = split_markdown_table_row(lines[index + 1])
        if separator is None or len(separator) != len(expected) or not valid_separator(separator):
            return []
        rows: list[list[str]] = []
        for row_line in lines[index + 2 :]:
            row = split_markdown_table_row(row_line)
            if row is None:
                break
            rows.append(row)
        return rows
    return None


def validate_tables(relative_path: str, text: str, doc_class: DocClass, errors: list[str]) -> None:
    for table in doc_class.required_tables:
        lines = section_lines(text, table.section)
        if table.section is not None and not lines:
            errors.append(
                f"{relative_path} ({doc_class.class_id}) missing section for metadata table "
                f"`{table.table_id}`: ## {table.section}"
            )
            continue
        rows = find_table(lines, table.columns)
        if rows is None:
            errors.append(
                f"{relative_path} ({doc_class.class_id}) missing metadata table `{table.table_id}` "
                f"with columns: {', '.join(table.columns)}"
            )
            continue
        malformed = [index for index, row in enumerate(rows) if len(row) != len(table.columns)]
        if malformed:
            errors.append(
                f"{relative_path} ({doc_class.class_id}) metadata table `{table.table_id}` "
                f"has malformed row(s): {malformed}"
            )
            continue
        if len(rows) < table.min_rows:
            errors.append(
                f"{relative_path} ({doc_class.class_id}) metadata table `{table.table_id}` "
                f"has too few rows: expected at least {table.min_rows}, found {len(rows)}"
            )
        column_index = {column: index for index, column in enumerate(table.columns)}
        for row_index, row in enumerate(rows):
            for requirement in table.row_requirements:
                cell = row[column_index[requirement.column]]
                if re.search(requirement.pattern, cell, DEFAULT_FLAGS) is None:
                    errors.append(
                        f"{relative_path} ({doc_class.class_id}) metadata table `{table.table_id}` "
                        f"row {row_index} column `{requirement.column}` failed `{requirement.description}`: "
                        f"{cell!r}"
                    )


def validate_root(root: Path, registry_path: Path = REGISTRY_PATH) -> tuple[dict[str, Any], list[str]]:
    registry = root / registry_path
    include_globs, classes = load_registry(registry)
    docs = included_docs(root, include_globs)
    errors: list[str] = []
    class_counts: dict[str, int] = {doc_class.class_id: 0 for doc_class in classes}
    for path in docs:
        relative_path = path.relative_to(root).as_posix()
        matched = matching_classes(relative_path, classes)
        if not matched:
            errors.append(f"{relative_path} is an unclassified release markdown doc")
            continue
        if len(matched) > 1:
            errors.append(
                f"{relative_path} matches multiple release-doc metadata classes: "
                + ", ".join(doc_class.class_id for doc_class in matched)
            )
            continue
        doc_class = matched[0]
        class_counts[doc_class.class_id] += 1
        text = path.read_text(encoding="utf-8")
        validate_fields(relative_path, text, doc_class, errors)
        validate_tables(relative_path, text, doc_class, errors)
    summary: dict[str, Any] = {
        "doc_count": len(docs),
        "class_counts": class_counts,
        "registry": registry_path.as_posix(),
    }
    return summary, errors


def write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def self_check_registry() -> dict[str, Any]:
    return {
        "schema_version": 1,
        "scope": "release-doc-metadata-fields",
        "documents": {"include_globs": ["docs/release/*.md"]},
        "classes": [
            {
                "id": "live-rerun",
                "description": "Synthetic live-rerun field surface.",
                "path_globs": ["docs/release/amflow-live-rerun-*.md"],
                "required_fields": [
                    {
                        "id": "upstream-source",
                        "description": "source script field",
                        "pattern": r"^- Upstream Mathematica scripts?:",
                    },
                    {
                        "id": "live-exit-code",
                        "description": "successful exit-code field",
                        "pattern": r"^live-exit:\s*0\s*$",
                    },
                    {
                        "id": "sha256-digest-line",
                        "description": "SHA-256 digest line",
                        "pattern": r"^-\s+.+?:\s*`[0-9a-f]{64}`",
                        "min_count": 3,
                    },
                ],
            },
            {
                "id": "known-gaps",
                "description": "Synthetic known-gaps field surface.",
                "path_globs": ["docs/release/known-gaps.md"],
                "required_tables": [
                    {
                        "id": "gap-rows",
                        "section": "Not Fully Reproduced Rows",
                        "columns": ["AMFlow example", "Status", "Remaining gap", "Blocker/reference"],
                        "min_rows": 1,
                        "row_requirements": [
                            {
                                "column": "Status",
                                "pattern": r"^`[a-z0-9_-]+`$",
                                "description": "backticked status",
                            },
                            {
                                "column": "Blocker/reference",
                                "pattern": r"\[[^\]]+\]\([^)]+\)",
                                "description": "markdown blocker/reference link",
                            },
                        ],
                    }
                ],
            },
        ],
    }


def live_fixture() -> str:
    return """# AMFlow Live Rerun: alpha

## Example

- Example: `alpha`
- Upstream Mathematica script: `/tmp/amflow/examples/alpha/run.wl`

```text
live-exit: 0
```

## Live Output Digest

- Live stdout: `aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa`
- Live stderr: `bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb`
- Live `sol` raw file: `cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc`
"""


def known_gaps_fixture() -> str:
    return """# Release Known Gaps

## Not Fully Reproduced Rows

| AMFlow example | Status | Remaining gap | Blocker/reference |
| --- | --- | --- | --- |
| `alpha` | `blocked-runtime-gap` | Needs runtime work. | [blocker](../milestones/alpha.md) |
"""


def write_self_check_root(root: Path) -> None:
    write_text(root / REGISTRY_PATH, json.dumps(self_check_registry(), indent=2, sort_keys=True) + "\n")
    write_text(root / "docs/release/amflow-live-rerun-alpha.md", live_fixture())
    write_text(root / "docs/release/known-gaps.md", known_gaps_fixture())


def run_fixture_validation(mutator=None) -> tuple[dict[str, Any], list[str]]:
    with tempfile.TemporaryDirectory(prefix="release-doc-metadata-fields-") as tmp:
        root = Path(tmp)
        write_self_check_root(root)
        if mutator is not None:
            mutator(root)
        return validate_root(root)


def replace_in_file(path: Path, old: str, new: str) -> None:
    text = path.read_text(encoding="utf-8")
    expect(old in text, f"fixture missing expected text {old!r}")
    path.write_text(text.replace(old, new, 1), encoding="utf-8")


def expect_self_check_failure(label: str, mutator, expected: str) -> bool:
    _, errors = run_fixture_validation(mutator)
    expect(errors, f"{label} unexpectedly passed")
    detail = "\n".join(errors)
    expect(expected in detail, f"{label} failed for the wrong reason: {detail}")
    return True


def run_self_check() -> dict[str, Any]:
    summary, errors = run_fixture_validation()
    expect(not errors, "valid release-doc metadata fixture failed: " + "; ".join(errors))
    expect(summary["doc_count"] == 2, f"valid fixture doc count drifted: {summary['doc_count']}")
    checks = {
        "valid_fixture": True,
        "unclassified_doc_rejected": expect_self_check_failure(
            "unclassified-doc",
            lambda root: write_text(root / "docs/release/new-release-note.md", "# New Release Note\n"),
            "unclassified release markdown doc",
        ),
        "missing_live_source_rejected": expect_self_check_failure(
            "missing-live-source",
            lambda root: replace_in_file(
                root / "docs/release/amflow-live-rerun-alpha.md",
                "- Upstream Mathematica script: `/tmp/amflow/examples/alpha/run.wl`\n",
                "",
            ),
            "`upstream-source`",
        ),
        "missing_live_exit_rejected": expect_self_check_failure(
            "missing-live-exit",
            lambda root: replace_in_file(
                root / "docs/release/amflow-live-rerun-alpha.md",
                "live-exit: 0",
                "run-finished: true",
            ),
            "`live-exit-code`",
        ),
        "missing_sha_line_rejected": expect_self_check_failure(
            "missing-sha-line",
            lambda root: replace_in_file(
                root / "docs/release/amflow-live-rerun-alpha.md",
                "- Live `sol` raw file: `cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc`\n",
                "",
            ),
            "`sha256-digest-line`",
        ),
        "known_gap_status_rejected": expect_self_check_failure(
            "known-gap-status",
            lambda root: replace_in_file(
                root / "docs/release/known-gaps.md",
                "| `alpha` | `blocked-runtime-gap` | Needs runtime work. | [blocker](../milestones/alpha.md) |",
                "| `alpha` |  | Needs runtime work. | [blocker](../milestones/alpha.md) |",
            ),
            "column `Status`",
        ),
        "known_gap_blocker_rejected": expect_self_check_failure(
            "known-gap-blocker",
            lambda root: replace_in_file(
                root / "docs/release/known-gaps.md",
                "[blocker](../milestones/alpha.md)",
                "blocker pending",
            ),
            "column `Blocker/reference`",
        ),
    }
    expect(all(checks.values()), "release-doc metadata field self-check failed")
    return {"self_check_passed": True, "checks": checks, "valid_summary": summary}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--verify",
        action="store_true",
        help="Validate committed release-doc metadata fields. This is the default.",
    )
    parser.add_argument(
        "--self-check",
        action="store_true",
        help="Run synthetic positive and negative checks for this validator.",
    )
    parser.add_argument(
        "--registry",
        default=REGISTRY_PATH.as_posix(),
        help="Repository-relative metadata-field registry path.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        if args.self_check:
            print(json.dumps(run_self_check(), indent=2, sort_keys=True))
            return 0
        summary, errors = validate_root(repo_root(), Path(args.registry))
    except MetadataFieldError as exc:
        print(f"release-doc metadata field validation failed: {exc}", file=sys.stderr)
        return 1

    if errors:
        print("release-doc metadata field validation failed:", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        return 1

    populated = {
        class_id: count
        for class_id, count in summary["class_counts"].items()
        if count
    }
    print(
        "release-doc metadata field validation passed: "
        f"{summary['doc_count']} doc(s), classes={json.dumps(populated, sort_keys=True)}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
