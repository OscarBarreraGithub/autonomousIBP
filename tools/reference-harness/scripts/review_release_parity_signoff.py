#!/usr/bin/env python3
"""Produce the M7 release-parity-signoff sidecar for release readiness."""

from __future__ import annotations

import argparse
import json
import tempfile
from pathlib import Path
from typing import Any

from freeze_phase0_goldens import load_json
from release_signoff_readiness import (
    QUALIFICATION_CORPUS_REQUIRED_WITHHELD_CLAIMS,
    load_diagnostic_review_summary,
    load_docs_completion_summary,
    load_performance_review_summary,
    load_qualification_corpus_review_summary,
)


PARITY_SIGNOFF_REQUIRED_INPUTS: tuple[str, ...] = (
    "qualification closure note",
    "b61n single-row publication hook",
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
B61N_SELECTED5_ENDPOINTS: tuple[str, ...] = (
    "box[0,0,0,1]",
    "box[1,0,1,0]",
    "box[1,0,0,1]",
    "box[0,1,0,1]",
    "box[0,0,1,1]",
)
MINIMUM_PROMOTION_DIGITS = 50


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


def resolve_input_path(path: Path, root: Path, label: str) -> Path:
    resolved = path if path.is_absolute() else root / path
    expect_path_within_root(resolved, root, label)
    return resolved


def load_m6_qualification_summary(summary_path: Path) -> dict[str, Any]:
    summary = load_json(summary_path)
    expect(summary.get("schema_version") == 1, "M6 qualification summary schema_version must be 1")
    expect(
        summary.get("scope") == "milestone-m6-qualification",
        "M6 qualification summary scope must be milestone-m6-qualification",
    )

    current_state = str(summary.get("current_state", "")).strip()
    expect(current_state, "M6 qualification summary current_state must not be empty")
    required_boolean_fields = [
        "phase0_packet_set_qualified",
        "phase0_ready_for_m6",
        "phase0_pending_runtime_lanes_closed",
        "case_study_families_qualified",
        "case_study_ready_for_m6",
        "milestone_m6_ready",
    ]
    for field in required_boolean_fields:
        if not isinstance(summary.get(field), bool):
            raise TypeError(f"M6 qualification summary {field} must be a bool")
    blocking_reasons = normalize_string_list(
        summary.get("blocking_reasons", []),
        "M6 qualification summary blocking_reasons",
    )
    withheld_claims = normalize_string_list(
        summary.get("withheld_claims", []),
        "M6 qualification summary withheld_claims",
    )
    if summary["milestone_m6_ready"]:
        expect(
            current_state == "milestone-m6-qualified",
            "ready M6 qualification summary must use current_state=milestone-m6-qualified",
        )
        expect(
            summary["phase0_packet_set_qualified"]
            and summary["phase0_ready_for_m6"]
            and summary["phase0_pending_runtime_lanes_closed"]
            and summary["case_study_families_qualified"]
            and summary["case_study_ready_for_m6"],
            "ready M6 qualification summary must report both subverdicts as ready",
        )
        expect(not blocking_reasons, "ready M6 qualification summary must not report blockers")
    else:
        expect(blocking_reasons, "blocked M6 qualification summary must report blockers")

    return {
        **summary,
        "current_state": current_state,
        "blocking_reasons": blocking_reasons,
        "withheld_claims": withheld_claims,
    }


def load_optional_summary(
    summary_path: Path | None,
    *,
    root: Path,
    label: str,
    loader: Any,
) -> tuple[dict[str, Any] | None, str]:
    if summary_path is None:
        return None, ""
    resolved = resolve_input_path(summary_path, root, label)
    return loader(resolved), str(resolved)


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


def load_b61n_publication_hook_sidecar(
    checklist: dict[str, Any],
    root: Path,
) -> dict[str, Any] | None:
    relative_path = checklist["sources"].get("b61n_single_row_publication_hook", "")
    if not relative_path:
        return None
    hook_path = root / relative_path
    try:
        expect_path_within_root(hook_path, root, "b61n single-row publication hook")
        return load_json(hook_path)
    except Exception:
        return None


def b61n_selected5_m6_signal_preserved(sidecar: dict[str, Any]) -> bool:
    m7_hook = sidecar.get("m7_parity_signoff_hook")
    if not isinstance(m7_hook, dict):
        return False
    required_m6_signal = m7_hook.get("required_m6_signal")
    if not isinstance(required_m6_signal, dict):
        return False
    reviewed_endpoint_integrals = required_m6_signal.get("reviewed_endpoint_integrals")
    if not isinstance(reviewed_endpoint_integrals, list) or not all(
        isinstance(endpoint, str) and endpoint.strip()
        for endpoint in reviewed_endpoint_integrals
    ):
        return False
    minimum_digit_agreement = required_m6_signal.get("minimum_digit_agreement")
    if not isinstance(minimum_digit_agreement, int) or isinstance(
        minimum_digit_agreement, bool
    ):
        return False
    blocked_release_prerequisites = m7_hook.get("blocked_release_prerequisites")
    if not isinstance(blocked_release_prerequisites, list) or not all(
        isinstance(blocker, str) and blocker.strip() for blocker in blocked_release_prerequisites
    ):
        return False
    return (
        required_m6_signal.get("phase0_id") == "complex_kinematics"
        and required_m6_signal.get("optional_capture_packet")
        == "b61n-complex-eta-zero-single-row"
        and required_m6_signal.get("full_eta_zero_contour_applied") is True
        and minimum_digit_agreement >= MINIMUM_PROMOTION_DIGITS
        and sorted(reviewed_endpoint_integrals) == sorted(B61N_SELECTED5_ENDPOINTS)
        and "m6 qualification closure" in blocked_release_prerequisites
        and "release docs-completion review" in blocked_release_prerequisites
    )


def b61n_publication_hook_base_reviewed(sidecar: dict[str, Any]) -> bool:
    if sidecar.get("benchmark_id") != "complex_kinematics":
        return False
    m7_hook = sidecar.get("m7_parity_signoff_hook")
    if not isinstance(m7_hook, dict):
        return False
    return (
        m7_hook.get("release_checklist_section") == "parity-signoff"
        and m7_hook.get("single_row_path") == "b61n-complex-contour-propagator-harness"
        and m7_hook.get("current_state") == "blocked-on-m6"
    )


def summarize_parity_signoff(
    *,
    checklist_path: Path,
    root: Path,
    m6_qualification_summary_path: Path | None = None,
    qualification_corpus_summary_path: Path | None = None,
    performance_review_summary_path: Path | None = None,
    diagnostic_review_summary_path: Path | None = None,
    docs_completion_summary_path: Path | None = None,
) -> dict[str, Any]:
    root = root.resolve(strict=False)
    checklist_path = resolve_input_path(checklist_path, root, "release checklist path")
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
    b61n_hook_sidecar = load_b61n_publication_hook_sidecar(checklist, root)
    b61n_publication_hook_found = b61n_hook_sidecar is not None
    b61n_hook_base_is_reviewed = (
        b61n_hook_sidecar is not None
        and b61n_publication_hook_base_reviewed(b61n_hook_sidecar)
    )
    b61n_selected5_m6_signal_is_preserved = (
        b61n_hook_sidecar is not None
        and b61n_selected5_m6_signal_preserved(b61n_hook_sidecar)
    )
    b61n_single_row_publication_hook_reviewed = (
        b61n_publication_hook_found
        and b61n_hook_base_is_reviewed
        and b61n_selected5_m6_signal_is_preserved
    )

    m6_qualification_summary, m6_qualification_summary_path_text = load_optional_summary(
        m6_qualification_summary_path,
        root=root,
        label="M6 qualification summary path",
        loader=load_m6_qualification_summary,
    )
    qualification_corpus_summary, qualification_corpus_summary_path_text = load_optional_summary(
        qualification_corpus_summary_path,
        root=root,
        label="qualification-corpus summary path",
        loader=load_qualification_corpus_review_summary,
    )
    performance_review_summary, performance_review_summary_path_text = load_optional_summary(
        performance_review_summary_path,
        root=root,
        label="performance review summary path",
        loader=load_performance_review_summary,
    )
    diagnostic_review_summary, diagnostic_review_summary_path_text = load_optional_summary(
        diagnostic_review_summary_path,
        root=root,
        label="diagnostic review summary path",
        loader=load_diagnostic_review_summary,
    )
    docs_completion_summary, docs_completion_summary_path_text = load_optional_summary(
        docs_completion_summary_path,
        root=root,
        label="docs completion summary path",
        loader=load_docs_completion_summary,
    )

    qualification_closure_reviewed = (
        m6_qualification_summary is not None
        and m6_qualification_summary["milestone_m6_ready"]
    )
    qualification_corpus_reviewed = (
        qualification_corpus_summary is not None
        and qualification_corpus_summary["qualification_corpus_review_complete"]
    )
    performance_review_summary_reviewed = (
        performance_review_summary is not None
        and performance_review_summary["performance_review_complete"]
    )
    diagnostic_review_summary_reviewed = (
        diagnostic_review_summary is not None
        and diagnostic_review_summary["diagnostic_review_complete"]
    )
    docs_completion_note_reviewed = (
        docs_completion_summary is not None
        and docs_completion_summary["docs_completion_review_complete"]
    )

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
    if not b61n_single_row_publication_hook_reviewed:
        missing_or_blocked_parity_paths.append("b61n-single-row-publication-hook")
        blocking_reasons.append("b61n single-row publication hook has not been reviewed")
    if not qualification_closure_reviewed:
        missing_or_blocked_parity_paths.append("qualification-closure-note")
        if m6_qualification_summary is None:
            blocking_reasons.append("qualification closure note has not been reviewed")
        elif m6_qualification_summary["blocking_reasons"]:
            blocking_reasons.extend(
                "M6 qualification: " + reason
                for reason in m6_qualification_summary["blocking_reasons"]
            )
        else:
            blocking_reasons.append("M6 qualification summary is not ready")
    if not qualification_corpus_reviewed:
        missing_or_blocked_parity_paths.append("qualification-corpus-summary")
        if qualification_corpus_summary is None:
            blocking_reasons.append("qualification-corpus review summary has not been provided")
        elif qualification_corpus_summary["blocking_reasons"]:
            blocking_reasons.extend(
                "qualification-corpus: " + reason
                for reason in qualification_corpus_summary["blocking_reasons"]
            )
        else:
            blocking_reasons.append("qualification-corpus review summary is not complete")
    if not performance_review_summary_reviewed:
        missing_or_blocked_parity_paths.append("performance-review-summary")
        if performance_review_summary is None:
            blocking_reasons.append("performance review summary has not been reviewed")
        elif performance_review_summary["blocking_reasons"]:
            blocking_reasons.extend(
                "performance: " + reason
                for reason in performance_review_summary["blocking_reasons"]
            )
        else:
            blocking_reasons.append("performance review summary has not been reviewed")
    if not diagnostic_review_summary_reviewed:
        missing_or_blocked_parity_paths.append("diagnostic-review-summary")
        if diagnostic_review_summary is None:
            blocking_reasons.append("diagnostic review summary has not been reviewed")
        elif diagnostic_review_summary["blocking_reasons"]:
            blocking_reasons.extend(
                "diagnostic: " + reason
                for reason in diagnostic_review_summary["blocking_reasons"]
            )
        else:
            blocking_reasons.append("diagnostic review summary has not been reviewed")
    if not docs_completion_note_reviewed:
        missing_or_blocked_parity_paths.append("docs-completion-note")
        if docs_completion_summary is None:
            blocking_reasons.append("docs completion note has not been reviewed")
        elif docs_completion_summary["blocking_reasons"]:
            blocking_reasons.extend(
                "docs-completion: " + reason
                for reason in docs_completion_summary["blocking_reasons"]
            )
        else:
            blocking_reasons.append("docs completion note has not been reviewed")

    parity_signoff_complete = (
        qualification_closure_reviewed
        and qualification_corpus_reviewed
        and performance_review_summary_reviewed
        and diagnostic_review_summary_reviewed
        and docs_completion_note_reviewed
        and withheld_claims_reviewed
        and b61n_single_row_publication_hook_reviewed
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
        "m6_qualification_summary_path": m6_qualification_summary_path_text,
        "qualification_corpus_summary_path": qualification_corpus_summary_path_text,
        "performance_review_summary_path": performance_review_summary_path_text,
        "diagnostic_review_summary_path": diagnostic_review_summary_path_text,
        "docs_completion_summary_path": docs_completion_summary_path_text,
        "parity_signoff_complete": parity_signoff_complete,
        "qualification_closure_reviewed": qualification_closure_reviewed,
        "qualification_corpus_reviewed": qualification_corpus_reviewed,
        "performance_review_summary_reviewed": performance_review_summary_reviewed,
        "diagnostic_review_summary_reviewed": diagnostic_review_summary_reviewed,
        "docs_completion_note_reviewed": docs_completion_note_reviewed,
        "withheld_claims_reviewed": withheld_claims_reviewed,
        "b61n_single_row_publication_hook_reviewed": (
            b61n_single_row_publication_hook_reviewed
        ),
        "b61n_selected5_m6_signal_preserved": b61n_selected5_m6_signal_is_preserved,
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
        "b61n_single_row_publication_hook": (
            "tools/reference-harness/specs/m6/lane5-next7/"
            "b61n-publication-qualifier-hook.json"
        ),
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
    for source_id, relative_path in sources.items():
        if source_id == "b61n_single_row_publication_hook":
            write_json(
                root / relative_path,
                {
                    "schema_version": 1,
                    "benchmark_id": "complex_kinematics",
                    "m7_parity_signoff_hook": {
                        "release_checklist_section": "parity-signoff",
                        "single_row_path": "b61n-complex-contour-propagator-harness",
                        "current_state": "blocked-on-m6",
                        "required_m6_signal": {
                            "phase0_id": "complex_kinematics",
                            "optional_capture_packet": "b61n-complex-eta-zero-single-row",
                            "full_eta_zero_contour_applied": True,
                            "minimum_digit_agreement": MINIMUM_PROMOTION_DIGITS,
                            "reviewed_endpoint_integrals": list(B61N_SELECTED5_ENDPOINTS),
                        },
                        "blocked_release_prerequisites": [
                            "m6 qualification closure",
                            "release qualification-corpus review",
                            "release performance review",
                            "release diagnostic review",
                            "release docs-completion review",
                        ],
                    },
                },
            )
            continue
        write_text(
            root / relative_path,
            "Synthetic release parity signoff source\n"
            "release_signoff_readiness.py\n"
            "review_release_parity_signoff.py\n"
            "release-parity-signoff\n"
            "parity_signoff_required_inputs_preserved\n",
        )
    return checklist_path


def write_synthetic_m6_qualification_summary(path: Path, *, ready: bool) -> None:
    write_json(
        path,
        {
            "schema_version": 1,
            "scope": "milestone-m6-qualification",
            "current_state": (
                "milestone-m6-qualified" if ready else "blocked-on-phase0-runtime-lanes"
            ),
            "phase0_packet_set_qualified": ready,
            "phase0_ready_for_m6": ready,
            "phase0_pending_runtime_lanes_closed": ready,
            "case_study_families_qualified": ready,
            "case_study_ready_for_m6": ready,
            "milestone_m6_ready": ready,
            "blocking_reasons": [] if ready else ["phase0: b61n runtime lane remains pending"],
            "withheld_claims": (
                [
                    "This summary does not mark Milestone M7 or release readiness.",
                    "This summary does not widen runtime or public behavior.",
                ]
                if ready
                else [
                    "This summary does not claim Milestone M6 closure.",
                    "This summary does not mark Milestone M7 or release readiness.",
                    "This summary does not widen runtime or public behavior.",
                ]
            ),
        },
    )


def write_synthetic_qualification_corpus_summary(path: Path) -> None:
    write_json(
        path,
        {
            "schema_version": 1,
            "scope": "release-qualification-corpus",
            "current_state": "qualification-corpus-reviewed",
            "qualification_corpus_review_complete": True,
            "qualification_corpus_required_inputs_preserved": True,
            "qualification_corpus_required_outputs_preserved": True,
            "qualification_evidence_coherent": True,
            "phase0_packet_set_verdict_present": True,
            "phase0_packet_set_qualified": True,
            "case_study_verdict_present": True,
            "case_study_families_qualified": True,
            "closed_benchmark_family_coverage_statement_reviewed": True,
            "residual_blockers_or_carveouts_preserved": True,
            "reviewed_phase0_ids": ["automatic_loop", "complex_kinematics"],
            "pending_phase0_ids": [],
            "blocked_case_study_ids": [],
            "phase0_failure_code_blockers": [],
            "case_study_qualification_blockers": [],
            "missing_or_blocked_qualification_paths": [],
            "blocking_reasons": [],
            "withheld_claims": list(QUALIFICATION_CORPUS_REQUIRED_WITHHELD_CLAIMS),
        },
    )


def write_synthetic_performance_summary(path: Path) -> None:
    write_json(
        path,
        {
            "schema_version": 1,
            "scope": "release-performance-review",
            "current_state": "performance-review-reviewed",
            "performance_review_complete": True,
            "mandatory_benchmark_timings_reviewed": True,
            "benchmark_family_scope_reviewed": True,
            "clean_rebuild_gate_reviewed": True,
            "unstable_performance_runs_reviewed": True,
            "reviewed_benchmark_families": ["phase0-required-set"],
            "missing_or_unreviewed_performance_paths": [],
            "blocking_reasons": [],
            "withheld_claims": [
                "This summary does not claim Milestone M7 closure.",
                "This summary does not claim release readiness.",
            ],
        },
    )


def write_synthetic_diagnostic_summary(path: Path) -> None:
    write_json(
        path,
        {
            "schema_version": 1,
            "scope": "release-diagnostic-review",
            "current_state": "diagnostic-review-reviewed",
            "diagnostic_review_complete": True,
            "required_failure_code_profiles_reviewed": True,
            "typed_failure_paths_preserved": True,
            "unstable_run_evidence_reviewed": True,
            "known_regression_outcomes_reviewed": True,
            "reviewed_failure_code_profiles": ["default-required-failure-codes"],
            "missing_or_degraded_diagnostic_paths": [],
            "blocking_reasons": [],
            "withheld_claims": [
                "This summary does not claim Milestone M7 closure.",
                "This summary does not claim release readiness.",
            ],
        },
    )


def write_synthetic_docs_completion_summary(path: Path) -> None:
    write_json(
        path,
        {
            "schema_version": 1,
            "scope": "release-docs-completion",
            "current_state": "docs-completion-reviewed",
            "docs_completion_review_complete": True,
            "docs_targets_reviewed": True,
            "public_contract_aligned": True,
            "implementation_ledger_aligned": True,
            "verification_strategy_aligned": True,
            "reference_harness_guide_aligned": True,
            "reference_harness_readme_aligned": True,
            "completion_roadmap_aligned": True,
            "explicit_non_claims_reviewed": True,
            "reviewed_doc_targets": ["docs/public-contract.md"],
            "missing_or_stale_doc_paths": [],
            "blocking_reasons": [],
            "withheld_claims": [
                "This summary does not claim Milestone M7 closure.",
                "This summary does not claim release readiness.",
            ],
        },
    )


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

    with tempfile.TemporaryDirectory(prefix="amflow-release-parity-b61n-hook-self-check-") as tmp:
        bad_hook_root = Path(tmp)
        bad_hook_checklist_path = write_synthetic_release_parity_root(bad_hook_root)
        bad_hook_path = (
            bad_hook_root
            / "tools/reference-harness/specs/m6/lane5-next7/"
            "b61n-publication-qualifier-hook.json"
        )
        write_json(
            bad_hook_path,
            {
                "schema_version": 1,
                "benchmark_id": "complex_kinematics",
                "m7_parity_signoff_hook": {
                    "release_checklist_section": "wrong-section",
                    "single_row_path": "wrong-path",
                    "current_state": "blocked-on-m6",
                },
            },
        )
        bad_hook_summary = summarize_parity_signoff(
            checklist_path=bad_hook_checklist_path,
            root=bad_hook_root,
        )

    with tempfile.TemporaryDirectory(
        prefix="amflow-release-parity-b61n-selected5-self-check-"
    ) as tmp:
        bad_signal_root = Path(tmp)
        bad_signal_checklist_path = write_synthetic_release_parity_root(bad_signal_root)
        bad_signal_path = (
            bad_signal_root
            / "tools/reference-harness/specs/m6/lane5-next7/"
            "b61n-publication-qualifier-hook.json"
        )
        write_json(
            bad_signal_path,
            {
                "schema_version": 1,
                "benchmark_id": "complex_kinematics",
                "m7_parity_signoff_hook": {
                    "release_checklist_section": "parity-signoff",
                    "single_row_path": "b61n-complex-contour-propagator-harness",
                    "current_state": "blocked-on-m6",
                    "required_m6_signal": {
                        "phase0_id": "complex_kinematics",
                        "optional_capture_packet": "b61n-complex-eta-zero-single-row",
                        "full_eta_zero_contour_applied": True,
                        "minimum_digit_agreement": MINIMUM_PROMOTION_DIGITS,
                        "reviewed_endpoint_integrals": [None],
                    },
                    "blocked_release_prerequisites": [
                        "m6 qualification closure",
                        "release docs-completion review",
                    ],
                },
            },
        )
        bad_signal_summary = summarize_parity_signoff(
            checklist_path=bad_signal_checklist_path,
            root=bad_signal_root,
        )

    with tempfile.TemporaryDirectory(prefix="amflow-release-parity-prereq-self-check-") as tmp:
        prereq_root = Path(tmp)
        prereq_checklist_path = write_synthetic_release_parity_root(prereq_root)
        m6_path = prereq_root / "m6-qualified.json"
        blocked_m6_path = prereq_root / "m6-blocked.json"
        qualification_corpus_path = prereq_root / "qualification-corpus-reviewed.json"
        performance_path = prereq_root / "performance-reviewed.json"
        diagnostic_path = prereq_root / "diagnostic-reviewed.json"
        docs_path = prereq_root / "docs-reviewed.json"
        write_synthetic_m6_qualification_summary(m6_path, ready=True)
        write_synthetic_m6_qualification_summary(blocked_m6_path, ready=False)
        write_synthetic_qualification_corpus_summary(qualification_corpus_path)
        write_synthetic_performance_summary(performance_path)
        write_synthetic_diagnostic_summary(diagnostic_path)
        write_synthetic_docs_completion_summary(docs_path)
        complete_prereq_summary = summarize_parity_signoff(
            checklist_path=prereq_checklist_path,
            root=prereq_root,
            m6_qualification_summary_path=m6_path,
            qualification_corpus_summary_path=qualification_corpus_path,
            performance_review_summary_path=performance_path,
            diagnostic_review_summary_path=diagnostic_path,
            docs_completion_summary_path=docs_path,
        )
        blocked_m6_summary = summarize_parity_signoff(
            checklist_path=prereq_checklist_path,
            root=prereq_root,
            m6_qualification_summary_path=blocked_m6_path,
            qualification_corpus_summary_path=qualification_corpus_path,
            performance_review_summary_path=performance_path,
            diagnostic_review_summary_path=diagnostic_path,
            docs_completion_summary_path=docs_path,
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
        "b61n_single_row_publication_hook_reviewed": (
            summary["b61n_single_row_publication_hook_reviewed"]
        ),
        "b61n_selected5_m6_signal_preserved": summary[
            "b61n_selected5_m6_signal_preserved"
        ],
        "malformed_b61n_publication_hook_blocked": (
            not bad_hook_summary["b61n_single_row_publication_hook_reviewed"]
            and "b61n-single-row-publication-hook"
            in bad_hook_summary["missing_or_blocked_parity_paths"]
        ),
        "malformed_selected5_m6_signal_blocked": (
            not bad_signal_summary["b61n_selected5_m6_signal_preserved"]
            and "b61n-single-row-publication-hook"
            in bad_signal_summary["missing_or_blocked_parity_paths"]
        ),
        "complete_prerequisite_summaries_unlock_parity": (
            complete_prereq_summary["parity_signoff_complete"]
            and complete_prereq_summary["qualification_closure_reviewed"]
            and complete_prereq_summary["qualification_corpus_reviewed"]
            and complete_prereq_summary["performance_review_summary_reviewed"]
            and complete_prereq_summary["diagnostic_review_summary_reviewed"]
            and complete_prereq_summary["docs_completion_note_reviewed"]
            and complete_prereq_summary["missing_or_blocked_parity_paths"] == []
        ),
        "blocked_m6_summary_blocks_parity": (
            not blocked_m6_summary["parity_signoff_complete"]
            and not blocked_m6_summary["qualification_closure_reviewed"]
            and "qualification-closure-note"
            in blocked_m6_summary["missing_or_blocked_parity_paths"]
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
        "--m6-qualification-summary",
        type=Path,
        help="Optional path to the M6 qualification closure summary",
    )
    parser.add_argument(
        "--qualification-corpus-summary",
        type=Path,
        help="Optional path to the release qualification-corpus review summary",
    )
    parser.add_argument(
        "--performance-review-summary",
        type=Path,
        help="Optional path to the release performance review summary",
    )
    parser.add_argument(
        "--diagnostic-review-summary",
        type=Path,
        help="Optional path to the release diagnostic review summary",
    )
    parser.add_argument(
        "--docs-completion-summary",
        type=Path,
        help="Optional path to the release docs-completion summary",
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
    summary = summarize_parity_signoff(
        checklist_path=checklist_path,
        root=root,
        m6_qualification_summary_path=args.m6_qualification_summary,
        qualification_corpus_summary_path=args.qualification_corpus_summary,
        performance_review_summary_path=args.performance_review_summary,
        diagnostic_review_summary_path=args.diagnostic_review_summary,
        docs_completion_summary_path=args.docs_completion_summary,
    )
    if args.summary_path is not None:
        write_json(args.summary_path, summary)
    print(json.dumps(summary, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
