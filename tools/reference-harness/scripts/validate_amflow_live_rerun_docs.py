#!/usr/bin/env python3
"""Validate the AMFlow live-rerun release-doc inventory."""

from __future__ import annotations

import argparse
import json
import re
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path


RELEASE_DOCS_ROOT = Path("docs/release")
AMFLOW_COVERAGE_DOC = RELEASE_DOCS_ROOT / "amflow-example-coverage.md"

EXPECTED_AMFLOW_EXAMPLES = (
    "automatic_loop",
    "automatic_phasespace",
    "automatic_vs_manual",
    "complex_kinematics",
    "differential_equation_solver",
    "feynman_prescription",
    "linear_propagator",
    "spacetime_dimension",
    "user_defined_amfmode",
    "user_defined_ending",
)

EXPECTED_LIVE_RETAINED_GOLDEN_EXAMPLES = frozenset(
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

BLOCKED_LIVE_EXAMPLES = frozenset(("automatic_phasespace",))

ACCEPTED_LIVE_DOC_STATUSES = frozenset(
    (
        "reproduced-fully-live",
        "reproduced-matches-golden",
        "reproduced-matches-golden-at-requested-precision",
    )
)

REQUIRED_LIVE_DOC_SECTIONS = (
    "## Example",
    "## Version Stack",
    "## Live Invocation",
    "## Live Output Digest",
    "## Comparison",
    "## Status",
)


@dataclass(frozen=True)
class CoverageRow:
    example: str
    entry_files: str
    evidence: str
    status: str
    line_number: int


@dataclass(frozen=True)
class LiveDoc:
    path: Path
    example: str
    status: str


class ValidationError(RuntimeError):
    """Raised when the live-rerun doc validator self-check cannot continue."""


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise ValidationError(message)


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def strip_backticks(value: str) -> str:
    text = value.strip()
    if len(text) >= 2 and text.startswith("`") and text.endswith("`"):
        return text[1:-1]
    return text


def split_markdown_table_row(line: str) -> list[str] | None:
    stripped = line.strip()
    if not stripped.startswith("|") or not stripped.endswith("|"):
        return None
    cells = [cell.strip() for cell in stripped.strip("|").split("|")]
    if len(cells) != 4:
        return None
    return cells


def parse_coverage_rows(root: Path, errors: list[str]) -> dict[str, CoverageRow]:
    path = root / AMFLOW_COVERAGE_DOC
    if not path.is_file():
        errors.append(f"{AMFLOW_COVERAGE_DOC} is missing")
        return {}

    rows: dict[str, CoverageRow] = {}
    for line_number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
        cells = split_markdown_table_row(line)
        if cells is None:
            continue
        if cells[0] == "Upstream example" or set(cells[0]) <= {"-", " "}:
            continue
        example = strip_backticks(cells[0])
        if example == cells[0]:
            continue
        if example in rows:
            errors.append(f"{AMFLOW_COVERAGE_DOC}:{line_number} duplicates `{example}`")
            continue
        rows[example] = CoverageRow(
            example=example,
            entry_files=cells[1],
            evidence=cells[2],
            status=strip_backticks(cells[3]),
            line_number=line_number,
        )
    return rows


def live_doc_path_for(example: str) -> Path:
    return RELEASE_DOCS_ROOT / f"amflow-live-rerun-{example}.md"


def parse_status_section(text: str) -> str | None:
    match = re.search(r"(?ms)^## Status\s*$\n+(.*?)(?:\n## |\Z)", text)
    if match is None:
        return None
    for line in match.group(1).splitlines():
        stripped = line.strip()
        status_match = re.fullmatch(r"`([^`]+)`", stripped)
        if status_match is not None:
            return status_match.group(1)
    return None


def parse_example_marker(text: str) -> str | None:
    match = re.search(r"(?m)^- Example:\s*`([^`]+)`\s*$", text)
    return match.group(1) if match is not None else None


def validate_live_doc(path: Path, root: Path, expected_example: str, errors: list[str]) -> LiveDoc | None:
    relative_path = path.relative_to(root)
    text = path.read_text(encoding="utf-8")
    normalized_text = re.sub(r"\s+", " ", text.lower())

    for section in REQUIRED_LIVE_DOC_SECTIONS:
        if section not in text:
            errors.append(f"{relative_path} missing required section {section!r}")

    example = parse_example_marker(text)
    if example is None:
        errors.append(f"{relative_path} missing '- Example: `...`' marker")
        example = ""
    elif example != expected_example:
        errors.append(
            f"{relative_path} declares example `{example}` but filename implies `{expected_example}`"
        )

    status = parse_status_section(text)
    if status is None:
        errors.append(f"{relative_path} missing backticked status under ## Status")
        status = ""
    elif status not in ACCEPTED_LIVE_DOC_STATUSES:
        accepted = ", ".join(f"`{value}`" for value in sorted(ACCEPTED_LIVE_DOC_STATUSES))
        errors.append(f"{relative_path} has unsupported live-rerun status `{status}`; expected one of {accepted}")

    lower_text = text.lower()
    for required_phrase in ("live-exit: 0", "mathematica:", "amflow:", "kira:", "fermat:"):
        if required_phrase not in lower_text:
            errors.append(f"{relative_path} missing required live rerun marker {required_phrase!r}")
    if "retained" not in lower_text:
        errors.append(f"{relative_path} must identify the retained golden or packet")
    has_non_claim_boundary = any(
        phrase in normalized_text
        for phrase in (
            "does not claim",
            "not a claim",
            "not broader",
            "not a broader",
        )
    )
    if not has_non_claim_boundary or "c++" not in normalized_text:
        errors.append(f"{relative_path} must preserve the C++ non-claim boundary")

    return LiveDoc(path=path, example=example, status=status)


def validate_root(root: Path) -> tuple[dict[str, object], list[str]]:
    errors: list[str] = []
    rows = parse_coverage_rows(root, errors)

    expected_examples = set(EXPECTED_AMFLOW_EXAMPLES)
    actual_examples = set(rows)
    missing_examples = sorted(expected_examples - actual_examples)
    extra_examples = sorted(actual_examples - expected_examples)
    if missing_examples:
        errors.append("coverage table is missing AMFlow examples: " + ", ".join(missing_examples))
    if extra_examples:
        errors.append("coverage table has unexpected AMFlow examples: " + ", ".join(extra_examples))

    live_rows = {
        example for example, row in rows.items() if row.status in ACCEPTED_LIVE_DOC_STATUSES
    }
    if live_rows != EXPECTED_LIVE_RETAINED_GOLDEN_EXAMPLES:
        errors.append(
            "coverage live retained-golden set drifted: expected "
            f"{sorted(EXPECTED_LIVE_RETAINED_GOLDEN_EXAMPLES)}, got {sorted(live_rows)}"
        )

    for example in BLOCKED_LIVE_EXAMPLES:
        row = rows.get(example)
        if row is not None and row.status != "reproduced-partial":
            errors.append(f"`{example}` must remain `reproduced-partial` until its live blocker closes")

    for example in sorted(EXPECTED_LIVE_RETAINED_GOLDEN_EXAMPLES):
        row = rows.get(example)
        if row is None:
            continue
        doc = live_doc_path_for(example)
        literal_doc_path = str(doc)
        if literal_doc_path not in row.evidence:
            errors.append(
                f"{AMFLOW_COVERAGE_DOC}:{row.line_number} `{example}` must cite {literal_doc_path}"
            )
        if "Live Mathematica+AMFlow rerun" not in row.evidence:
            errors.append(
                f"{AMFLOW_COVERAGE_DOC}:{row.line_number} `{example}` must identify live Mathematica+AMFlow rerun evidence"
            )
        normalized_evidence = re.sub(r"\s+", " ", row.evidence.lower())
        has_non_broadening_boundary = any(
            phrase in normalized_evidence
            for phrase in (
                "not broader",
                "not a broader",
                "not c++",
                "not a claim",
            )
        )
        if not has_non_broadening_boundary:
            errors.append(
                f"{AMFLOW_COVERAGE_DOC}:{row.line_number} `{example}` must preserve the non-broadening boundary"
            )

    live_doc_paths = sorted((root / RELEASE_DOCS_ROOT).glob("amflow-live-rerun-*.md"))
    live_docs_by_example: dict[str, LiveDoc] = {}
    for path in live_doc_paths:
        example = path.stem.removeprefix("amflow-live-rerun-")
        if example in BLOCKED_LIVE_EXAMPLES:
            errors.append(f"{path.relative_to(root)} is unexpected while `{example}` remains blocked")
        if example not in EXPECTED_LIVE_RETAINED_GOLDEN_EXAMPLES:
            errors.append(f"{path.relative_to(root)} is not part of the pinned live retained-golden surface")
        live_doc = validate_live_doc(path, root, example, errors)
        if live_doc is not None:
            live_docs_by_example[example] = live_doc

    actual_live_doc_examples = set(live_docs_by_example)
    missing_live_docs = sorted(EXPECTED_LIVE_RETAINED_GOLDEN_EXAMPLES - actual_live_doc_examples)
    extra_live_docs = sorted(actual_live_doc_examples - EXPECTED_LIVE_RETAINED_GOLDEN_EXAMPLES)
    if missing_live_docs:
        errors.append("missing live rerun document(s): " + ", ".join(missing_live_docs))
    if extra_live_docs:
        errors.append("unexpected live rerun document(s): " + ", ".join(extra_live_docs))

    summary: dict[str, object] = {
        "coverage_examples": len(rows),
        "expected_examples": len(EXPECTED_AMFLOW_EXAMPLES),
        "live_retained_golden_count": len(live_rows),
        "live_doc_count": len(live_doc_paths),
        "live_examples": sorted(live_rows),
        "blocked_live_examples": sorted(BLOCKED_LIVE_EXAMPLES),
    }
    return summary, errors


def live_doc_fixture(example: str, status: str = "reproduced-fully-live") -> str:
    return f"""# AMFlow Live Rerun: {example}

Date: 2026-06-12

## Example

- Example: `{example}`
- Upstream Mathematica script: `/tmp/amflow/examples/{example}/run.wl`
- Live scratch root: `/tmp/live-reruns/{example}`
- Retained full AMFlow golden: `tools/reference-harness/specs/phase0/{example}.golden-manifest.json`

## Version Stack

- Mathematica: `13.3.0 for Linux x86 (64-bit) (June 3, 2023)`
- AMFlow: `1.1`
- Kira: `3.1`
- Fermat: `5.25`

## Live Invocation

```bash
true
```

Run metadata:

```text
live-exit: 0
```

## Live Output Digest

- Live stdout: `{"0" * 64}`

## Comparison

Compared live output against the retained golden.

## Status

`{status}`

This confirms live retained-golden reproducibility. It does not claim new C++
runtime coverage.
"""


def coverage_fixture(status_overrides: dict[str, str] | None = None, omit_link: str | None = None) -> str:
    overrides = status_overrides or {}
    lines = [
        "# AMFlow Example Coverage Inventory",
        "",
        "## Upstream Inventory",
        "",
        "| Upstream example | Mathematica entry file(s) | C++ lane / evidence in this repo | Status |",
        "| --- | --- | --- | --- |",
    ]
    for example in EXPECTED_AMFLOW_EXAMPLES:
        if example in EXPECTED_LIVE_RETAINED_GOLDEN_EXAMPLES:
            doc_path = live_doc_path_for(example)
            link_text = "" if example == omit_link else f" see `{doc_path}`."
            evidence = (
                f"Live Mathematica+AMFlow rerun matched retained golden for `{example}`;"
                f"{link_text} This is live retained-golden evidence, not broader C++ runtime coverage."
            )
            status = "reproduced-fully-live"
        elif example == "automatic_phasespace":
            evidence = "D246 blocker remains open."
            status = "reproduced-partial"
        else:
            evidence = "Accepted retained-state C++ comparison evidence."
            status = "reproduced-fully"
        status = overrides.get(example, status)
        lines.append(
            f"| `{example}` | `examples/{example}/run.wl` | {evidence} | `{status}` |"
        )
    lines.append("")
    return "\n".join(lines)


def write_fixture_root(root: Path) -> None:
    docs_root = root / RELEASE_DOCS_ROOT
    docs_root.mkdir(parents=True, exist_ok=True)
    (root / AMFLOW_COVERAGE_DOC).write_text(coverage_fixture(), encoding="utf-8")
    for example in EXPECTED_LIVE_RETAINED_GOLDEN_EXAMPLES:
        (root / live_doc_path_for(example)).write_text(live_doc_fixture(example), encoding="utf-8")


def run_fixture_validation(mutator=None) -> tuple[dict[str, object], list[str]]:
    with tempfile.TemporaryDirectory(prefix="amflow-live-rerun-docs-") as temp_dir:
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


def run_self_check() -> dict[str, object]:
    summary, errors = run_fixture_validation()
    expect(not errors, "valid live-rerun doc fixture failed: " + "; ".join(errors))
    expect(summary["live_retained_golden_count"] == 9, "valid fixture live count drifted")
    expect(summary["live_doc_count"] == 9, "valid fixture doc count drifted")

    checks = {
        "valid_fixture": True,
        "missing_live_doc_rejected": expect_self_check_failure(
            "missing-live-doc",
            lambda root: (root / live_doc_path_for("linear_propagator")).unlink(),
            "missing live rerun document",
        ),
        "wrong_doc_example_rejected": expect_self_check_failure(
            "wrong-doc-example",
            lambda root: (root / live_doc_path_for("complex_kinematics")).write_text(
                live_doc_fixture("automatic_loop"), encoding="utf-8"
            ),
            "declares example",
        ),
        "missing_required_section_rejected": expect_self_check_failure(
            "missing-section",
            lambda root: (root / live_doc_path_for("feynman_prescription")).write_text(
                live_doc_fixture("feynman_prescription").replace("## Comparison\n", ""),
                encoding="utf-8",
            ),
            "missing required section",
        ),
        "automatic_phasespace_overclaim_rejected": expect_self_check_failure(
            "automatic-phasespace-overclaim",
            lambda root: (root / AMFLOW_COVERAGE_DOC).write_text(
                coverage_fixture({"automatic_phasespace": "reproduced-fully-live"}),
                encoding="utf-8",
            ),
            "`automatic_phasespace` must remain `reproduced-partial`",
        ),
        "coverage_doc_link_rejected": expect_self_check_failure(
            "coverage-doc-link",
            lambda root: (root / AMFLOW_COVERAGE_DOC).write_text(
                coverage_fixture(omit_link="linear_propagator"),
                encoding="utf-8",
            ),
            "must cite docs/release/amflow-live-rerun-linear_propagator.md",
        ),
        "unexpected_live_doc_rejected": expect_self_check_failure(
            "unexpected-live-doc",
            lambda root: (root / live_doc_path_for("automatic_phasespace")).write_text(
                live_doc_fixture("automatic_phasespace"), encoding="utf-8"
            ),
            "is unexpected while `automatic_phasespace` remains blocked",
        ),
    }
    expect(all(checks.values()), "live-rerun doc validator self-check failed")
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
        help="Validate the committed release docs. This is the default unless --self-check is used.",
    )
    parser.add_argument(
        "--self-check",
        action="store_true",
        help="Run fixture-backed positive and negative checks for this validator.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        if args.self_check:
            print(json.dumps(run_self_check(), indent=2, sort_keys=True))
            return 0
        summary, errors = validate_root(repo_root())
    except ValidationError as error:
        print(f"AMFlow live-rerun doc validation failed: {error}", file=sys.stderr)
        return 1

    if errors:
        print("AMFlow live-rerun doc validation failed:", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        return 1

    print(
        "AMFlow live-rerun doc validation passed: "
        f"{summary['coverage_examples']} coverage row(s), "
        f"{summary['live_retained_golden_count']} live retained-golden row(s), "
        f"{summary['live_doc_count']} live rerun doc(s)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
