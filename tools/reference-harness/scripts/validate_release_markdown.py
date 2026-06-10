#!/usr/bin/env python3
"""Validate release markdown links and fenced code blocks."""

from __future__ import annotations

import json
import re
import subprocess
import sys
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable
from urllib.parse import unquote, urlsplit


RELEASE_DOCS_ROOT = Path("docs/release")
RELEASE_CHECKLIST = Path("docs/release-signoff-checklist.md")
REQUIRED_RELEASE_MARKDOWN = frozenset(
    (
        RELEASE_CHECKLIST,
        RELEASE_DOCS_ROOT / "amflow-example-coverage.md",
        RELEASE_DOCS_ROOT / "known-gaps.md",
        RELEASE_DOCS_ROOT / "m7-closure-evidence.md",
        RELEASE_DOCS_ROOT / "re-run-release-readiness.md",
        RELEASE_DOCS_ROOT / "tools.md",
    )
)
ALLOWED_FENCE_LANGUAGES = frozenset(("bash", "json", "sh", "text"))
SHELL_FENCE_LANGUAGES = frozenset(("bash", "sh"))
INLINE_LINK_PATTERN = re.compile(r"(?<!!)\[[^\]\n]+\]\(([^)\n]+)\)")
REFERENCE_LINK_PATTERN = re.compile(r"^\s{0,3}\[[^\]\n]+\]:\s*(\S+)")
HEADING_PATTERN = re.compile(r"^\s{0,3}#{1,6}\s+(.+?)\s*#*\s*$")


@dataclass(frozen=True)
class CodeBlock:
    path: Path
    start_line: int
    language: str
    text: str


class MarkdownError(RuntimeError):
    """Raised when release markdown validation cannot continue."""


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def release_markdown_paths(root: Path) -> list[Path]:
    release_root = root / RELEASE_DOCS_ROOT
    if not release_root.is_dir():
        raise MarkdownError(f"{RELEASE_DOCS_ROOT} is missing")

    missing_required = sorted(
        str(relative_path)
        for relative_path in REQUIRED_RELEASE_MARKDOWN
        if not (root / relative_path).is_file()
    )
    if missing_required:
        raise MarkdownError("required release markdown is missing: " + ", ".join(missing_required))

    paths = sorted(release_root.glob("*.md"))
    checklist = root / RELEASE_CHECKLIST
    if not checklist.is_file():
        raise MarkdownError(f"{RELEASE_CHECKLIST} is missing")
    paths.append(checklist)
    return sorted(dict.fromkeys(paths))


def format_location(path: Path, root: Path, line: int) -> str:
    return f"{path.relative_to(root)}:{line}"


def parse_markdown(path: Path, root: Path, errors: list[str]) -> tuple[list[str], list[CodeBlock]]:
    lines = path.read_text(encoding="utf-8").splitlines()
    masked_lines: list[str] = []
    blocks: list[CodeBlock] = []
    in_fence = False
    fence_char = ""
    fence_length = 0
    fence_language = ""
    fence_start_line = 0
    block_lines: list[str] = []

    for line_number, line in enumerate(lines, start=1):
        fence_match = re.match(r"^ {0,3}(`{3,}|~{3,})(.*)$", line)
        if not in_fence:
            if fence_match is None:
                masked_lines.append(line)
                continue

            fence = fence_match.group(1)
            fence_char = fence[0]
            fence_length = len(fence)
            info = fence_match.group(2).strip()
            language = info.split()[0] if info else ""
            if not language:
                errors.append(
                    f"{format_location(path, root, line_number)} fenced code block must declare a language"
                )
            elif language not in ALLOWED_FENCE_LANGUAGES:
                errors.append(
                    f"{format_location(path, root, line_number)} unsupported fenced code language: {language}"
                )
            if info and info != language:
                errors.append(
                    f"{format_location(path, root, line_number)} fenced code info string "
                    "must contain only the language tag"
                )
            in_fence = True
            fence_language = language
            fence_start_line = line_number
            block_lines = []
            masked_lines.append("")
            continue

        close_pattern = rf"^ {{0,3}}{re.escape(fence_char)}{{{fence_length},}}\s*$"
        if re.match(close_pattern, line) is not None:
            blocks.append(
                CodeBlock(
                    path=path,
                    start_line=fence_start_line,
                    language=fence_language,
                    text="\n".join(block_lines) + "\n",
                )
            )
            in_fence = False
            fence_char = ""
            fence_length = 0
            fence_language = ""
            fence_start_line = 0
            block_lines = []
            masked_lines.append("")
            continue

        block_lines.append(line)
        masked_lines.append("")

    if in_fence:
        errors.append(f"{format_location(path, root, fence_start_line)} fenced code block is not closed")

    return masked_lines, blocks


def parse_code_block(block: CodeBlock, root: Path, errors: list[str]) -> None:
    location = format_location(block.path, root, block.start_line)
    if not block.language:
        return
    if block.language in SHELL_FENCE_LANGUAGES:
        completed = subprocess.run(
            ["bash", "-n"],
            input=block.text,
            text=True,
            capture_output=True,
            check=False,
        )
        if completed.returncode != 0:
            detail = completed.stderr.strip() or "bash -n failed"
            errors.append(f"{location} shell fenced code does not parse: {detail}")
        return
    if block.language == "json":
        try:
            json.loads(block.text)
        except json.JSONDecodeError as error:
            errors.append(f"{location} json fenced code does not parse: {error}")
        return
    if block.language == "text":
        return


def first_link_destination(raw: str) -> str:
    value = raw.strip()
    if value.startswith("<"):
        end = value.find(">")
        if end != -1:
            return value[1:end].strip()
    return value.split()[0] if value.split() else ""


def iter_link_destinations(lines: Iterable[str]) -> Iterable[tuple[int, str]]:
    for line_number, line in enumerate(lines, start=1):
        for match in INLINE_LINK_PATTERN.finditer(line):
            yield line_number, first_link_destination(match.group(1))
        reference_match = REFERENCE_LINK_PATTERN.match(line)
        if reference_match is not None:
            yield line_number, first_link_destination(reference_match.group(1))


def slugify_heading(raw: str) -> str:
    text = raw.strip()
    text = re.sub(r"\s+#*$", "", text)
    text = re.sub(r"`([^`]*)`", r"\1", text)
    text = re.sub(r"<[^>]+>", "", text)
    text = text.lower()
    text = re.sub(r"[^a-z0-9 _-]", "", text)
    text = re.sub(r"\s+", "-", text.strip())
    return text


def markdown_anchors(path: Path) -> set[str]:
    anchors: set[str] = set()
    counts: defaultdict[str, int] = defaultdict(int)
    in_fence = False
    fence_char = ""
    fence_length = 0

    for line in path.read_text(encoding="utf-8").splitlines():
        fence_match = re.match(r"^ {0,3}(`{3,}|~{3,})(.*)$", line)
        if not in_fence and fence_match is not None:
            fence = fence_match.group(1)
            fence_char = fence[0]
            fence_length = len(fence)
            in_fence = True
            continue
        if in_fence:
            close_pattern = rf"^ {{0,3}}{re.escape(fence_char)}{{{fence_length},}}\s*$"
            if re.match(close_pattern, line) is not None:
                in_fence = False
            continue

        heading_match = HEADING_PATTERN.match(line)
        if heading_match is None:
            continue
        base_slug = slugify_heading(heading_match.group(1))
        duplicate_index = counts[base_slug]
        counts[base_slug] += 1
        anchors.add(base_slug if duplicate_index == 0 else f"{base_slug}-{duplicate_index}")

    return anchors


def resolve_repo_link(source_path: Path, root: Path, destination: str, line_number: int, errors: list[str]) -> None:
    if not destination:
        errors.append(f"{format_location(source_path, root, line_number)} link destination is empty")
        return

    parsed = urlsplit(destination)
    if parsed.scheme in {"http", "https", "mailto"}:
        return
    if parsed.scheme or parsed.netloc:
        errors.append(
            f"{format_location(source_path, root, line_number)} unsupported link scheme: {destination}"
        )
        return

    raw_path = unquote(parsed.path)
    if raw_path:
        target = Path(raw_path)
        if target.is_absolute():
            target_path = root / target.relative_to("/")
        else:
            target_path = source_path.parent / target
    else:
        target_path = source_path

    resolved_root = root.resolve(strict=True)
    resolved_target = target_path.resolve(strict=False)
    try:
        resolved_target.relative_to(resolved_root)
    except ValueError:
        errors.append(
            f"{format_location(source_path, root, line_number)} link escapes repository: {destination}"
        )
        return

    if not resolved_target.exists():
        errors.append(
            f"{format_location(source_path, root, line_number)} link target does not exist: {destination}"
        )
        return

    if parsed.fragment:
        if resolved_target.suffix.lower() != ".md":
            errors.append(
                f"{format_location(source_path, root, line_number)} fragment links must target markdown: "
                f"{destination}"
            )
            return
        fragment = unquote(parsed.fragment).lower()
        anchors = markdown_anchors(resolved_target)
        if fragment not in anchors:
            errors.append(
                f"{format_location(source_path, root, line_number)} markdown anchor does not exist: "
                f"{destination}"
            )


def validate_file(path: Path, root: Path, errors: list[str]) -> tuple[int, int]:
    masked_lines, blocks = parse_markdown(path, root, errors)
    for block in blocks:
        parse_code_block(block, root, errors)

    link_count = 0
    for line_number, destination in iter_link_destinations(masked_lines):
        link_count += 1
        resolve_repo_link(path, root, destination, line_number, errors)

    return link_count, len(blocks)


def main() -> int:
    root = repo_root()
    errors: list[str] = []
    try:
        paths = release_markdown_paths(root)
    except MarkdownError as error:
        print(f"release markdown validation failed: {error}", file=sys.stderr)
        return 1

    link_count = 0
    code_block_count = 0
    for path in paths:
        file_links, file_blocks = validate_file(path, root, errors)
        link_count += file_links
        code_block_count += file_blocks

    if errors:
        print("release markdown validation failed:", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        return 1

    print(
        "release markdown validation passed: "
        f"{len(paths)} file(s), {link_count} link(s), {code_block_count} code block(s)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
