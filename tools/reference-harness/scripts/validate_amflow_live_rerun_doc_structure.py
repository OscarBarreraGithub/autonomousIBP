#!/usr/bin/env python3
"""Validate structured evidence blocks in AMFlow live-rerun release docs."""

from __future__ import annotations

import argparse
import json
import re
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path
from urllib.parse import unquote, urlsplit


RELEASE_DOCS_ROOT = Path("docs/release")
EXPECTED_LIVE_RERUN_EXAMPLES = frozenset(
    (
        "automatic_loop",
        "automatic_vs_manual",
        "complex_kinematics",
        "differential_equation_solver",
        "feynman_prescription",
        "linear_propagator",
        "spacetime_dimension",
        "user_defined_amfmode",
        "user_defined_ending",
    )
)
SHA256_PATTERN = r"[0-9a-f]{64}"
INLINE_LINK_PATTERN = re.compile(r"(?<!!)\[[^\]\n]+\]\(([^)\n]+)\)")
INLINE_CODE_PATTERN = re.compile(r"`([^`\n]+)`")
EXIT_LINE_PATTERN = re.compile(r"(?m)^([A-Za-z0-9_-]+-exit):\s*(\S+)\s*$")
LAST_REVERIFIED_LINE_PATTERN = re.compile(
    r"(?m)^last-re-verified:\s*([0-9]{4}-[0-9]{2}-[0-9]{2})(?:\s+\([^)]+\))?\s*$"
)
LAST_REVERIFIED_PREFIX_PATTERN = re.compile(r"(?m)^last-re-verified:\s*(.+)$")
EXTERNAL_RETAINED_EVIDENCE_MARKERS = (
    "/amflow-verification/reference-harness/",
    "/amflow-verification/work/goldens/",
)


@dataclass(frozen=True)
class CodeBlock:
    language: str
    text: str
    start_line: int


@dataclass(frozen=True)
class DocStructure:
    example: str
    path: str
    digest_sha_bullets: int
    exit_lines: list[str]
    last_reverified: str | None
    evidence_references: int


class StructureError(RuntimeError):
    """Raised when the structure validator self-check cannot continue."""


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise StructureError(message)


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def live_doc_path_for(example: str) -> Path:
    return RELEASE_DOCS_ROOT / f"amflow-live-rerun-{example}.md"


def relative_to_root(path: Path, root: Path) -> str:
    return str(path.relative_to(root))


def section_text(text: str, heading: str) -> str | None:
    match = re.search(
        rf"(?ms)^## {re.escape(heading)}\s*$\n+(.*?)(?:\n## |\Z)",
        text,
    )
    return match.group(1) if match is not None else None


def parse_code_blocks(text: str) -> list[CodeBlock]:
    blocks: list[CodeBlock] = []
    in_fence = False
    fence_char = ""
    fence_length = 0
    fence_language = ""
    fence_start_line = 0
    block_lines: list[str] = []

    for line_number, line in enumerate(text.splitlines(), start=1):
        fence_match = re.match(r"^ {0,3}(`{3,}|~{3,})(.*)$", line)
        if not in_fence:
            if fence_match is None:
                continue
            fence = fence_match.group(1)
            fence_char = fence[0]
            fence_length = len(fence)
            info = fence_match.group(2).strip()
            fence_language = info.split()[0] if info else ""
            fence_start_line = line_number
            block_lines = []
            in_fence = True
            continue

        close_pattern = rf"^ {{0,3}}{re.escape(fence_char)}{{{fence_length},}}\s*$"
        if re.match(close_pattern, line) is not None:
            blocks.append(
                CodeBlock(
                    language=fence_language,
                    text="\n".join(block_lines),
                    start_line=fence_start_line,
                )
            )
            in_fence = False
            fence_char = ""
            fence_length = 0
            fence_language = ""
            fence_start_line = 0
            block_lines = []
            continue

        block_lines.append(line)

    return blocks


def link_destination(raw: str) -> str:
    value = raw.strip()
    if value.startswith("<"):
        end = value.find(">")
        if end != -1:
            return value[1:end].strip()
    return value.split()[0] if value.split() else ""


def path_under_root(path: Path, root: Path) -> Path | None:
    try:
        return path.resolve().relative_to(root.resolve())
    except ValueError:
        return None


def append_repo_reference(
    references: list[str],
    path: Path,
    root: Path,
    raw_reference: str,
    errors: list[str],
) -> None:
    reference = raw_reference.strip()
    if "*" in reference:
        return
    if not reference.startswith("tools/reference-harness/"):
        return
    target_path = root / reference
    if reference.endswith("/") or target_path.is_dir() or not Path(reference).suffix:
        return
    if not target_path.is_file():
        errors.append(
            f"{relative_to_root(path, root)} has broken repo-local evidence reference: "
            f"{reference}"
        )
        return
    references.append(reference)


def append_external_reference(references: list[str], raw_reference: str) -> None:
    reference = raw_reference.strip()
    if any(marker in reference for marker in EXTERNAL_RETAINED_EVIDENCE_MARKERS):
        references.append(reference)


def evidence_references(path: Path, root: Path, text: str, errors: list[str]) -> list[str]:
    references: list[str] = []

    for match in INLINE_CODE_PATTERN.finditer(text):
        reference = match.group(1)
        append_repo_reference(references, path, root, reference, errors)
        append_external_reference(references, reference)

    for match in INLINE_LINK_PATTERN.finditer(text):
        destination = link_destination(match.group(1))
        if not destination:
            continue
        parsed = urlsplit(destination)
        if parsed.scheme or parsed.netloc:
            continue
        raw_path = unquote(parsed.path)
        if not raw_path:
            continue
        target = Path(raw_path)
        if target.is_absolute():
            continue
        target_path = (path.parent / target).resolve()
        relative_target = path_under_root(target_path, root)
        if relative_target is None:
            continue
        if not target_path.is_file():
            errors.append(
                f"{relative_to_root(path, root)} has broken repo-local evidence link: "
                f"{destination}"
            )
            continue
        parts = relative_target.parts
        if len(parts) >= 2 and parts[:2] == ("tools", "reference-harness"):
            references.append(str(relative_target))

    return sorted(set(references))


def digest_sha_bullets(digest: str) -> list[str]:
    return re.findall(
        rf"(?m)^- .+?:\s*`({SHA256_PATTERN})`(?:\s*\(empty\))?\s*$",
        digest,
    )


def validate_doc(root: Path, example: str, errors: list[str]) -> DocStructure | None:
    path = root / live_doc_path_for(example)
    relative_path = str(live_doc_path_for(example))
    if not path.is_file():
        errors.append(f"{relative_path} is missing")
        return None

    text = path.read_text(encoding="utf-8")
    title = f"# AMFlow Live Rerun: {example}"
    if not text.startswith(title + "\n"):
        errors.append(f"{relative_path} must start with {title!r}")

    invocation = section_text(text, "Live Invocation")
    if invocation is None:
        errors.append(f"{relative_path} missing ## Live Invocation")
        invocation = ""
    digest = section_text(text, "Live Output Digest")
    if digest is None:
        errors.append(f"{relative_path} missing ## Live Output Digest")
        digest = ""
    if section_text(text, "Comparison") is None:
        errors.append(f"{relative_path} missing ## Comparison")

    invocation_blocks = parse_code_blocks(invocation)
    bash_blocks = [block for block in invocation_blocks if block.language == "bash"]
    if not bash_blocks:
        errors.append(f"{relative_path} missing bash fenced live invocation")
    elif not any("wolframscript -file" in block.text for block in bash_blocks):
        errors.append(f"{relative_path} live invocation must call wolframscript -file")

    text_blocks = [block for block in invocation_blocks if block.language == "text"]
    exit_lines: list[str] = []
    for block in text_blocks:
        for name, value in EXIT_LINE_PATTERN.findall(block.text):
            exit_lines.append(f"{name}: {value}")
            if value != "0":
                errors.append(f"{relative_path} records nonzero {name}: {value}")
    if "live-exit: 0" not in exit_lines:
        errors.append(f"{relative_path} missing live-exit: 0 in a text fenced metadata block")

    sha_bullets = digest_sha_bullets(digest)
    if len(sha_bullets) < 3:
        errors.append(
            f"{relative_path} must publish at least three SHA-256 digest bullets"
        )
    if re.search(rf"(?m)^- Live stdout:\s*`{SHA256_PATTERN}`", digest) is None:
        errors.append(f"{relative_path} missing Live stdout SHA-256 digest bullet")
    if re.search(rf"(?m)^- Live stderr:\s*`{SHA256_PATTERN}`", digest) is None:
        errors.append(f"{relative_path} missing Live stderr SHA-256 digest bullet")

    retained_references = evidence_references(path, root, text, errors)
    if not retained_references:
        errors.append(
            f"{relative_path} must include a structured retained evidence reference"
        )

    last_reverified_lines = LAST_REVERIFIED_PREFIX_PATTERN.findall(text)
    if len(last_reverified_lines) > 1:
        errors.append(f"{relative_path} must contain at most one last-re-verified line")
    last_reverified: str | None = None
    if last_reverified_lines:
        match = LAST_REVERIFIED_LINE_PATTERN.search(text)
        if match is None:
            errors.append(f"{relative_path} has malformed last-re-verified line")
        else:
            last_reverified = match.group(0)

    return DocStructure(
        example=example,
        path=relative_path,
        digest_sha_bullets=len(sha_bullets),
        exit_lines=exit_lines,
        last_reverified=last_reverified,
        evidence_references=len(retained_references),
    )


def validate_root(root: Path) -> tuple[dict[str, object], list[str]]:
    errors: list[str] = []
    live_docs = sorted((root / RELEASE_DOCS_ROOT).glob("amflow-live-rerun-*.md"))
    actual_examples = {
        path.stem.removeprefix("amflow-live-rerun-")
        for path in live_docs
    }
    missing = sorted(EXPECTED_LIVE_RERUN_EXAMPLES - actual_examples)
    extra = sorted(actual_examples - EXPECTED_LIVE_RERUN_EXAMPLES)
    if missing:
        errors.append("missing live-rerun structured doc(s): " + ", ".join(missing))
    if extra:
        errors.append("unexpected live-rerun structured doc(s): " + ", ".join(extra))

    structures: list[DocStructure] = []
    for example in sorted(EXPECTED_LIVE_RERUN_EXAMPLES):
        structure = validate_doc(root, example, errors)
        if structure is not None:
            structures.append(structure)

    summary: dict[str, object] = {
        "live_doc_count": len(live_docs),
        "expected_live_doc_count": len(EXPECTED_LIVE_RERUN_EXAMPLES),
        "docs": [
            {
                "example": structure.example,
                "path": structure.path,
                "digest_sha_bullets": structure.digest_sha_bullets,
                "exit_lines": structure.exit_lines,
                "last_reverified": structure.last_reverified,
                "evidence_references": structure.evidence_references,
            }
            for structure in structures
        ],
    }
    return summary, errors


def fixture_doc(example: str, *, exit_value: str = "0") -> str:
    return f"""# AMFlow Live Rerun: {example}

Date: 2026-06-12

## Example

- Example: `{example}`
- Retained committed AMFlow golden:
  `tools/reference-harness/specs/phase0/{example}.golden-manifest.json`

## Version Stack

- Mathematica: `13.3.0`
- AMFlow: `1.1`
- Kira: `3.1`
- Fermat: `5.25`

## Live Invocation

```bash
RUNROOT=/tmp/live-reruns/{example}
timeout 5400s wolframscript -file "$RUNROOT/amflow/examples/{example}/run.wl" > "$RUNROOT/live.stdout" 2> "$RUNROOT/live.stderr"
```

Run metadata:

```text
live-start: 2026-06-12T00:00:00-04:00
live-end: 2026-06-12T00:01:00-04:00
live-exit: {exit_value}
```

## Live Output Digest

- Live stdout: `aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa`
- Live stderr: `bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb`
- Live `sol` raw file: `cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc`

## Comparison

Compared live output against the retained AMFlow golden:

```text
SameQ=True
```

## Status

`reproduced-fully-live`

This confirms live retained-golden reproducibility. It does not claim new C++
runtime coverage.
"""


def write_fixture_root(root: Path) -> None:
    docs_root = root / RELEASE_DOCS_ROOT
    docs_root.mkdir(parents=True, exist_ok=True)
    target_root = root / "tools/reference-harness/specs/phase0"
    target_root.mkdir(parents=True, exist_ok=True)
    for example in EXPECTED_LIVE_RERUN_EXAMPLES:
        (target_root / f"{example}.golden-manifest.json").write_text(
            "{}\n",
            encoding="utf-8",
        )
        (root / live_doc_path_for(example)).write_text(
            fixture_doc(example),
            encoding="utf-8",
        )


def run_fixture_validation(mutator=None) -> tuple[dict[str, object], list[str]]:
    with tempfile.TemporaryDirectory(prefix="amflow-live-rerun-structure-") as temp_dir:
        root = Path(temp_dir)
        write_fixture_root(root)
        if mutator is not None:
            mutator(root)
        return validate_root(root)


def expect_self_check_failure(label: str, mutator, expected: str) -> bool:
    _, errors = run_fixture_validation(mutator)
    expect(errors, f"{label} unexpectedly passed")
    detail = "\n".join(errors)
    expect(expected in detail, f"{label} failed for the wrong reason: {detail}")
    return True


def replace_in_doc(root: Path, example: str, old: str, new: str) -> None:
    path = root / live_doc_path_for(example)
    text = path.read_text(encoding="utf-8")
    expect(old in text, f"self-check fixture missing expected text {old!r}")
    path.write_text(text.replace(old, new, 1), encoding="utf-8")


def run_self_check() -> dict[str, object]:
    summary, errors = run_fixture_validation()
    expect(not errors, "valid structured fixture failed: " + "; ".join(errors))
    expect(summary["live_doc_count"] == 9, "valid fixture doc count drifted")

    checks = {
        "valid_fixture": True,
        "missing_doc_rejected": expect_self_check_failure(
            "missing-doc",
            lambda root: (root / live_doc_path_for("linear_propagator")).unlink(),
            "missing live-rerun structured doc",
        ),
        "missing_reference_rejected": expect_self_check_failure(
            "missing-reference",
            lambda root: replace_in_doc(
                root,
                "complex_kinematics",
                "`tools/reference-harness/specs/phase0/complex_kinematics.golden-manifest.json`",
                "`not-reference-harness-evidence.json`",
            ),
            "must include a structured retained evidence reference",
        ),
        "broken_reference_rejected": expect_self_check_failure(
            "broken-reference",
            lambda root: replace_in_doc(
                root,
                "differential_equation_solver",
                "tools/reference-harness/specs/phase0/differential_equation_solver.golden-manifest.json",
                "tools/reference-harness/specs/phase0/missing.json",
            ),
            "broken repo-local evidence reference",
        ),
        "missing_exit_rejected": expect_self_check_failure(
            "missing-exit",
            lambda root: replace_in_doc(
                root,
                "feynman_prescription",
                "live-exit: 0\n",
                "",
            ),
            "missing live-exit: 0",
        ),
        "nonzero_exit_rejected": expect_self_check_failure(
            "nonzero-exit",
            lambda root: replace_in_doc(
                root,
                "spacetime_dimension",
                "live-exit: 0",
                "live-exit: 124",
            ),
            "records nonzero live-exit",
        ),
        "missing_bash_invocation_rejected": expect_self_check_failure(
            "missing-bash",
            lambda root: replace_in_doc(
                root,
                "user_defined_amfmode",
                "```bash",
                "```text",
            ),
            "missing bash fenced live invocation",
        ),
        "missing_stdout_digest_rejected": expect_self_check_failure(
            "missing-stdout-digest",
            lambda root: replace_in_doc(
                root,
                "user_defined_ending",
                "- Live stdout: `aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa`\n",
                "",
            ),
            "missing Live stdout SHA-256 digest bullet",
        ),
        "malformed_sha_rejected": expect_self_check_failure(
            "malformed-sha",
            lambda root: replace_in_doc(
                root,
                "linear_propagator",
                "- Live `sol` raw file: `cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc`",
                "- Live `sol` raw file: `not-a-sha`",
            ),
            "must publish at least three SHA-256 digest bullets",
        ),
        "malformed_last_reverified_rejected": expect_self_check_failure(
            "malformed-last-reverified",
            lambda root: (root / live_doc_path_for("complex_kinematics")).write_text(
                fixture_doc("complex_kinematics")
                + "\nlast-re-verified: June 12, 2026\n",
                encoding="utf-8",
            ),
            "malformed last-re-verified line",
        ),
    }
    expect(all(checks.values()), "live-rerun doc structure self-check failed")
    return {
        "self_check_passed": True,
        "checks": checks,
        "valid_summary": summary,
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--verify",
        action="store_true",
        help="Validate committed live-rerun document structure. This is the default.",
    )
    parser.add_argument(
        "--self-check",
        action="store_true",
        help="Run synthetic positive and negative checks for this validator.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        if args.self_check:
            print(json.dumps(run_self_check(), indent=2, sort_keys=True))
            return 0
        summary, errors = validate_root(repo_root())
    except StructureError as error:
        print(f"AMFlow live-rerun doc structure validation failed: {error}", file=sys.stderr)
        return 1

    if errors:
        print("AMFlow live-rerun doc structure validation failed:", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        return 1

    print(
        "AMFlow live-rerun doc structure validation passed: "
        f"{summary['live_doc_count']} doc(s), "
        f"{sum(doc['digest_sha_bullets'] for doc in summary['docs'])} digest SHA bullet(s), "
        f"{sum(1 for doc in summary['docs'] if doc['last_reverified'] is not None)} last-reverified line(s), "
        f"{sum(doc['evidence_references'] for doc in summary['docs'])} evidence reference(s)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
