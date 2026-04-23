#!/usr/bin/env python3
"""Produce the M7 release-parity-signoff sidecar for release readiness."""

from __future__ import annotations

import argparse
import json
import tempfile
from pathlib import Path
from typing import Any

from freeze_phase0_goldens import load_json


PARITY_SIGNOFF_REQUIRED_INPUTS: tuple[str, ...] = (
    "qualification closure note",
    "performance review summary",
    "diagnostic review summary",
    "docs completion note",
)

PARITY_SIGNOFF_REQUIRED_OUTPUTS: tuple[str, ...] = (
    "release sign-off statement",
    "explicit withheld-claim list if sign-off is still blocked",
)

REQUIRED_RELEASE_REVIEW_SECTIONS: tuple[str, ...] = (
    "qualification-corpus",
    "performance-review",
    "diagnostic-review",
    "docs-completion",
)

WITHHELD_CLAIMS: tuple[str, ...] = (
    "This summary does not claim final parity sign-off.",
    "This summary does not claim Milestone M6 closure.",
    "This summary does not claim Milestone M7 closure.",
    "This summary does not claim release readiness.",
    "This summary does not widen runtime or public behavior.",
)

REQUIRED_NON_CLAIM_MARKERS: tuple[str, ...] = (
    "Milestone M6",
    "Milestone M7",
    "release readiness",
    "runtime",
)


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def write_json(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def write_text(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def normalize_string_list(raw: Any, label: str) -> list[str]:
    if raw is None:
        return []
    if not isinstance(raw, list):
        raise TypeError(f"{label} must be a list, got {type(raw).__name__}")
    values: list[str] = []
    for item in raw:
        if not isinstance(item, str):
            raise TypeError(f"{label} entries must be strings, got {type(item).__name__}")
        value = item.strip()
        if not value:
            raise ValueError(f"{label} entries must not be empty")
        values.append(value)
    return values


def expect_unique(values: list[str], label: str) -> None:
    expect(len(set(values)) == len(values), f"{label} must not contain duplicates")


def expect_path_within_root(path: Path, root: Path, label: str) -> None:
    resolved_path = path.resolve(strict=False)
    resolved_root = root.resolve(strict=False)
    try:
        resolved_path.relative_to(resolved_root)
    except ValueError as error:
        raise RuntimeError(f"{label} must stay under {root}: {path}") from error


def load_release_checklist(checklist_path: Path) -> dict[str, Any]:
    checklist = load_json(checklist_path)
    expect(checklist.get("schema_version") == 1, "release checklist schema_version must be 1")

    sources = checklist.get("sources")
    if not isinstance(sources, dict):
        raise TypeError("release checklist sources must be an object")
    normalized_sources: dict[str, str] = {}
    for key, value in sources.items():
        if not isinstance(key, str) or not key.strip():
            raise ValueError("release checklist source id must be a non-empty string")
        if not isinstance(value, str) or not value.strip():
            raise ValueError(f"release checklist source {key} must be a non-empty string path")
        normalized_sources[key.strip()] = value.strip()

    review_sections = checklist.get("review_sections")
    if not isinstance(review_sections, list):
        raise TypeError("release checklist review_sections must be a list")
    normalized_sections: list[dict[str, Any]] = []
    section_ids: list[str] = []
    for section in review_sections:
        if not isinstance(section, dict):
            raise TypeError("release checklist review_sections entries must be objects")
        section_id = str(section.get("id", "")).strip()
        if not section_id:
            raise ValueError("release checklist review section id must not be empty")
        section_ids.append(section_id)
        normalized_sections.append(
            {
                **section,
                "id": section_id,
                "required_inputs": normalize_string_list(
                    section.get("required_inputs", []),
                    f"release checklist review section {section_id} required_inputs",
                ),
                "required_outputs": normalize_string_list(
                    section.get("required_outputs", []),
                    f"release checklist review section {section_id} required_outputs",
                ),
            }
        )
    expect_unique(section_ids, "release checklist review section ids")

    explicit_non_claims = normalize_string_list(
        checklist.get("explicit_non_claims", []),
        "release checklist explicit_non_claims",
    )

    return {
        **checklist,
        "sources": normalized_sources,
        "review_sections": normalized_sections,
        "explicit_non_claims": explicit_non_claims,
    }


def find_parity_signoff_section(checklist: dict[str, Any]) -> dict[str, Any] | None:
    for section in checklist["review_sections"]:
        if section["id"] == "parity-signoff":
            return section
    return None


def required_values_present(values: list[str], required_values: tuple[str, ...]) -> bool:
    return all(value in values for value in required_values)


def explicit_non_claims_cover_release_blockers(non_claims: list[str]) -> bool:
    combined = "\n".join(non_claims)
    return all(marker in combined for marker in REQUIRED_NON_CLAIM_MARKERS)


def summarize_parity_signoff(*, checklist_path: Path, root: Path) -> dict[str, Any]:
    root = root.resolve(strict=False)
    checklist_path = checklist_path.resolve(strict=False)
    expect_path_within_root(checklist_path, root, "release checklist path")
    checklist = load_release_checklist(checklist_path)

    missing_sources: list[str] = []
    for source_id, relative_path in checklist["sources"].items():
        source_path = root / relative_path
        expect_path_within_root(source_path, root, f"release checklist source {source_id}")
        if not source_path.exists():
            missing_sources.append(f"{source_id}:{relative_path}")

    section_ids = [section["id"] for section in checklist["review_sections"]]
    prerequisite_sections_present = all(
        section_id in section_ids for section_id in REQUIRED_RELEASE_REVIEW_SECTIONS
    )
    parity_section = find_parity_signoff_section(checklist)
    parity_signoff_required_inputs_preserved = False
    parity_signoff_required_outputs_preserved = False
    if parity_section is not None:
        parity_signoff_required_inputs_preserved = required_values_present(
            parity_section["required_inputs"],
            PARITY_SIGNOFF_REQUIRED_INPUTS,
        )
        parity_signoff_required_outputs_preserved = required_values_present(
            parity_section["required_outputs"],
            PARITY_SIGNOFF_REQUIRED_OUTPUTS,
        )

    withheld_claims_reviewed = explicit_non_claims_cover_release_blockers(
        checklist["explicit_non_claims"]
    )

    qualification_closure_reviewed = False
    performance_review_summary_reviewed = False
    diagnostic_review_summary_reviewed = False
    docs_completion_note_reviewed = False

    missing_or_blocked_parity_paths: list[str] = []
    blocking_reasons: list[str] = []

    if parity_section is None:
        missing_or_blocked_parity_paths.append("release-checklist:parity-signoff")
        blocking_reasons.append("release checklist does not define the parity-signoff section")
    if not parity_signoff_required_inputs_preserved:
        missing_or_blocked_parity_paths.append("release-checklist:parity-signoff-inputs")
        blocking_reasons.append("release checklist parity-signoff required inputs are incomplete")
    if not parity_signoff_required_outputs_preserved:
        missing_or_blocked_parity_paths.append("release-checklist:parity-signoff-outputs")
        blocking_reasons.append("release checklist parity-signoff required outputs are incomplete")
    if missing_sources:
        missing_or_blocked_parity_paths.extend(
            f"release-checklist-source:{source}" for source in missing_sources
        )
        blocking_reasons.extend(
            f"release checklist source path is missing: {source}" for source in missing_sources
        )
    if not prerequisite_sections_present:
        missing_or_blocked_parity_paths.append("release-checklist:prerequisite-review-sections")
        blocking_reasons.append("release checklist does not preserve every prerequisite review section")
    if not withheld_claims_reviewed:
        missing_or_blocked_parity_paths.append("release-checklist:explicit-non-claims")
        blocking_reasons.append("release checklist explicit non-claims are incomplete")
    if not qualification_closure_reviewed:
        missing_or_blocked_parity_paths.append("qualification-closure-note")
        blocking_reasons.append("qualification closure note has not been reviewed")
    if not performance_review_summary_reviewed:
        missing_or_blocked_parity_paths.append("performance-review-summary")
        blocking_reasons.append("performance review summary has not been reviewed")
    if not diagnostic_review_summary_reviewed:
        missing_or_blocked_parity_paths.append("diagnostic-review-summary")
        blocking_reasons.append("diagnostic review summary has not been reviewed")
    if not docs_completion_note_reviewed:
        missing_or_blocked_parity_paths.append("docs-completion-note")
        blocking_reasons.append("docs completion note has not been reviewed")

    parity_signoff_complete = (
        qualification_closure_reviewed
        and performance_review_summary_reviewed
        and diagnostic_review_summary_reviewed
        and docs_completion_note_reviewed
        and withheld_claims_reviewed
        and parity_signoff_required_inputs_preserved
        and parity_signoff_required_outputs_preserved
        and prerequisite_sections_present
        and not missing_or_blocked_parity_paths
        and not blocking_reasons
    )

    return {
        "schema_version": 1,
        "scope": "release-parity-signoff",
        "current_state": (
            "parity-signoff-reviewed"
            if parity_signoff_complete
            else "blocked-on-prerequisite-release-reviews"
        ),
        "checklist_path": str(checklist_path),
        "parity_signoff_complete": parity_signoff_complete,
        "qualification_closure_reviewed": qualification_closure_reviewed,
        "performance_review_summary_reviewed": performance_review_summary_reviewed,
        "diagnostic_review_summary_reviewed": diagnostic_review_summary_reviewed,
        "docs_completion_note_reviewed": docs_completion_note_reviewed,
        "withheld_claims_reviewed": withheld_claims_reviewed,
        "parity_signoff_required_inputs_preserved": parity_signoff_required_inputs_preserved,
        "parity_signoff_required_outputs_preserved": parity_signoff_required_outputs_preserved,
        "prerequisite_review_sections_preserved": prerequisite_sections_present,
        "required_release_review_sections": list(REQUIRED_RELEASE_REVIEW_SECTIONS),
        "missing_or_blocked_parity_paths": sorted(set(missing_or_blocked_parity_paths)),
        "blocking_reasons": blocking_reasons,
        "withheld_claims": list(WITHHELD_CLAIMS),
    }


def write_synthetic_release_parity_root(
    root: Path,
    *,
    complete_checklist_inputs: bool = True,
    complete_non_claims: bool = True,
) -> Path:
    checklist_path = root / "tools/reference-harness/templates/release-signoff-checklist.json"
    sources = {
        "release_signoff_markdown": "docs/release-signoff-checklist.md",
        "qualification_scaffold": "tools/reference-harness/templates/qualification-benchmarks.json",
        "release_readiness_helper": "tools/reference-harness/scripts/release_signoff_readiness.py",
        "performance_review_helper": "tools/reference-harness/scripts/review_release_performance.py",
        "diagnostic_review_helper": "tools/reference-harness/scripts/review_release_diagnostic.py",
        "docs_completion_review_helper": "tools/reference-harness/scripts/review_release_docs_completion.py",
        "parity_signoff_review_helper": "tools/reference-harness/scripts/review_release_parity_signoff.py",
        "parity_matrix": "specs/parity-matrix.yaml",
        "verification_strategy": "docs/verification-strategy.md",
    }
    required_inputs = list(PARITY_SIGNOFF_REQUIRED_INPUTS)
    if not complete_checklist_inputs:
        required_inputs = required_inputs[1:]
    explicit_non_claims = [
        "Editing this scaffold does not claim Milestone M6 closure.",
        "Editing this scaffold does not claim Milestone M7 closure.",
        "Editing this scaffold does not claim release readiness.",
        "Editing this scaffold does not widen the reviewed runtime or public contract.",
    ]
    if not complete_non_claims:
        explicit_non_claims = explicit_non_claims[:2]

    review_sections = [
        {
            "id": section_id,
            "required_inputs": ["synthetic prerequisite input"],
            "required_outputs": ["synthetic prerequisite output"],
            "notes": "Synthetic prerequisite section.",
        }
        for section_id in REQUIRED_RELEASE_REVIEW_SECTIONS
    ]
    review_sections.append(
        {
            "id": "parity-signoff",
            "required_inputs": required_inputs,
            "required_outputs": list(PARITY_SIGNOFF_REQUIRED_OUTPUTS),
            "notes": "Synthetic parity-signoff section.",
        }
    )

    write_json(
        checklist_path,
        {
            "schema_version": 1,
            "current_state": "planning-only",
            "sources": sources,
            "review_sections": review_sections,
            "explicit_non_claims": explicit_non_claims,
        },
    )
    for relative_path in sources.values():
        write_text(
            root / relative_path,
            "Synthetic release parity signoff source\n"
            "release_signoff_readiness.py\n"
            "review_release_parity_signoff.py\n"
            "release-parity-signoff\n"
            "parity_signoff_required_inputs_preserved\n",
        )
    return checklist_path


def run_self_check() -> dict[str, Any]:
    with tempfile.TemporaryDirectory(prefix="amflow-release-parity-signoff-self-check-") as tmp:
        temp_root = Path(tmp)
        checklist_path = write_synthetic_release_parity_root(temp_root)
        summary_path = temp_root / "parity-signoff-summary.json"
        summary = summarize_parity_signoff(checklist_path=checklist_path, root=temp_root)
        write_json(summary_path, summary)
        summary_written = summary_path.exists()

        from release_signoff_readiness import load_parity_signoff_summary

        loaded_summary = load_parity_signoff_summary(summary_path)

    with tempfile.TemporaryDirectory(prefix="amflow-release-parity-inputs-self-check-") as tmp:
        incomplete_root = Path(tmp)
        incomplete_checklist_path = write_synthetic_release_parity_root(
            incomplete_root,
            complete_checklist_inputs=False,
        )
        incomplete_summary = summarize_parity_signoff(
            checklist_path=incomplete_checklist_path,
            root=incomplete_root,
        )

    with tempfile.TemporaryDirectory(prefix="amflow-release-parity-nonclaims-self-check-") as tmp:
        nonclaim_root = Path(tmp)
        nonclaim_checklist_path = write_synthetic_release_parity_root(
            nonclaim_root,
            complete_non_claims=False,
        )
        nonclaim_summary = summarize_parity_signoff(
            checklist_path=nonclaim_checklist_path,
            root=nonclaim_root,
        )

    return {
        "parity_signoff_complete": summary["parity_signoff_complete"],
        "parity_signoff_required_inputs_preserved": (
            summary["parity_signoff_required_inputs_preserved"]
        ),
        "parity_signoff_required_outputs_preserved": (
            summary["parity_signoff_required_outputs_preserved"]
        ),
        "prerequisite_review_sections_preserved": (
            summary["prerequisite_review_sections_preserved"]
        ),
        "release_readiness_schema_compatible": (
            loaded_summary["scope"] == "release-parity-signoff"
            and loaded_summary["current_state"] == "blocked-on-prerequisite-release-reviews"
            and not loaded_summary["parity_signoff_complete"]
        ),
        "prerequisite_reviews_blocked": (
            not summary["parity_signoff_complete"]
            and "qualification-closure-note" in summary["missing_or_blocked_parity_paths"]
            and "docs-completion-note" in summary["missing_or_blocked_parity_paths"]
        ),
        "incomplete_checklist_blocked": (
            not incomplete_summary["parity_signoff_required_inputs_preserved"]
            and "release-checklist:parity-signoff-inputs"
            in incomplete_summary["missing_or_blocked_parity_paths"]
        ),
        "incomplete_non_claims_blocked": (
            not nonclaim_summary["withheld_claims_reviewed"]
            and "release-checklist:explicit-non-claims"
            in nonclaim_summary["missing_or_blocked_parity_paths"]
        ),
        "summary_written": summary_written,
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--checklist-path",
        type=Path,
        help="Release-signoff checklist JSON path",
    )
    parser.add_argument(
        "--summary-path",
        type=Path,
        help="Optional output file for the parity-signoff sidecar summary",
    )
    parser.add_argument(
        "--self-check",
        action="store_true",
        help="Run a synthetic parity-signoff sidecar producer check",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.self_check:
        print(json.dumps(run_self_check(), indent=2, sort_keys=True))
        return 0

    root = repo_root()
    checklist_path = (
        args.checklist_path
        if args.checklist_path is not None
        else root / "tools" / "reference-harness" / "templates" / "release-signoff-checklist.json"
    )
    summary = summarize_parity_signoff(checklist_path=checklist_path, root=root)
    if args.summary_path is not None:
        write_json(args.summary_path, summary)
    print(json.dumps(summary, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
