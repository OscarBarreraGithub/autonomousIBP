#!/usr/bin/env python3
"""Audit the M7 release-signoff scaffold against retained readiness evidence."""

from __future__ import annotations

import argparse
import json
import tempfile
from pathlib import Path
from typing import Any

from freeze_phase0_goldens import load_json


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def write_json(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


PARITY_SIGNOFF_REQUIRED_RELEASE_REVIEW_SECTIONS: tuple[str, ...] = (
    "qualification-corpus",
    "performance-review",
    "diagnostic-review",
    "docs-completion",
)

PARITY_SIGNOFF_REQUIRED_WITHHELD_CLAIMS: tuple[str, ...] = (
    "This summary does not claim final parity sign-off.",
    "This summary does not claim Milestone M6 closure.",
    "This summary does not claim Milestone M7 closure.",
    "This summary does not claim release readiness.",
    "This summary does not widen runtime or public behavior.",
)


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


def parse_runtime_lane_entries(raw: Any, label: str) -> list[dict[str, str]]:
    if not isinstance(raw, list):
        raise TypeError(f"{label} must be a list")
    entries: list[dict[str, str]] = []
    seen_ids: set[str] = set()
    for entry in raw:
        if not isinstance(entry, dict):
            raise TypeError(f"{label} entries must be objects")
        entry_id_raw = entry.get("id")
        next_runtime_lane_raw = entry.get("next_runtime_lane", "")
        if not isinstance(entry_id_raw, str):
            raise TypeError(f"{label} id must be a string")
        if not isinstance(next_runtime_lane_raw, str):
            raise TypeError(f"{label} next_runtime_lane must be a string")
        entry_id = entry_id_raw.strip()
        next_runtime_lane = next_runtime_lane_raw.strip()
        if not entry_id:
            raise ValueError(f"{label} id must not be empty")
        if entry_id in seen_ids:
            raise ValueError(f"duplicate {label} id: {entry_id}")
        if not next_runtime_lane:
            raise ValueError(f"{label} next_runtime_lane must not be empty")
        seen_ids.add(entry_id)
        entries.append(
            {
                "id": entry_id,
                "next_runtime_lane": next_runtime_lane,
            }
        )
    return entries


def load_release_checklist(checklist_path: Path) -> dict[str, Any]:
    checklist = load_json(checklist_path)
    expect(checklist.get("schema_version") == 1, "release checklist schema_version must be 1")

    sources = checklist.get("sources")
    if not isinstance(sources, dict):
        raise TypeError("release checklist sources must be an object")
    for key, value in sources.items():
        if not isinstance(value, str) or not value.strip():
            raise ValueError(f"release checklist source {key} must be a non-empty string path")

    review_sections = checklist.get("review_sections")
    if not isinstance(review_sections, list):
        raise TypeError("release checklist review_sections must be a list")
    review_section_ids: list[str] = []
    for section in review_sections:
        if not isinstance(section, dict):
            raise TypeError("release checklist review_sections entries must be objects")
        section_id = str(section.get("id", "")).strip()
        if not section_id:
            raise ValueError("release checklist review section id must not be empty")
        review_section_ids.append(section_id)
        normalize_string_list(
            section.get("required_inputs", []),
            f"release checklist review section {section_id} required_inputs",
        )
        normalize_string_list(
            section.get("required_outputs", []),
            f"release checklist review section {section_id} required_outputs",
        )
    expect_unique(review_section_ids, "release checklist review section ids")

    release_prerequisites = checklist.get("release_prerequisites")
    if not isinstance(release_prerequisites, list):
        raise TypeError("release checklist release_prerequisites must be a list")
    prerequisite_ids: list[str] = []
    for prerequisite in release_prerequisites:
        if not isinstance(prerequisite, dict):
            raise TypeError("release checklist release_prerequisites entries must be objects")
        prerequisite_id = str(prerequisite.get("id", "")).strip()
        if not prerequisite_id:
            raise ValueError("release checklist prerequisite id must not be empty")
        prerequisite_ids.append(prerequisite_id)
        required_state = str(prerequisite.get("required_state", "")).strip()
        if not required_state:
            raise ValueError(
                f"release checklist prerequisite {prerequisite_id} required_state must not be empty"
            )
    expect_unique(prerequisite_ids, "release checklist prerequisite ids")

    docs_completion_targets = checklist.get("docs_completion_targets")
    if not isinstance(docs_completion_targets, list):
        raise TypeError("release checklist docs_completion_targets must be a list")
    doc_target_paths: list[str] = []
    for target in docs_completion_targets:
        if not isinstance(target, dict):
            raise TypeError("release checklist docs_completion_targets entries must be objects")
        path = str(target.get("path", "")).strip()
        if not path:
            raise ValueError("release checklist docs_completion_targets path must not be empty")
        doc_target_paths.append(path)
        expectation = str(target.get("expectation", "")).strip()
        if not expectation:
            raise ValueError(
                f"release checklist docs_completion_targets expectation must not be empty for {path}"
            )
    expect_unique(doc_target_paths, "release checklist docs_completion_targets paths")

    normalize_string_list(checklist.get("explicit_non_claims", []), "release checklist explicit_non_claims")
    return checklist


def load_qualification_summary(summary_path: Path) -> dict[str, Any]:
    summary = load_json(summary_path)
    expect(summary.get("schema_version") == 1, "qualification summary schema_version must be 1")

    required_boolean_fields = [
        "required_root_reference_captured",
        "required_root_captures_required_set",
        "required_root_only_captures_required_set",
        "optional_packets_preserve_bootstrap_only_state",
        "optional_capture_packets_match_scaffold",
        "captured_examples_publish_reference_artifacts",
        "captured_examples_pass_comparison_checks",
        "observed_reference_captured_matches_scaffold",
        "pending_examples_preserve_runtime_lane_hints",
        "blocked_case_study_families_preserve_runtime_lane_hints",
    ]
    for field in required_boolean_fields:
        if not isinstance(summary.get(field), bool):
            raise TypeError(f"qualification summary {field} must be a bool")

    phase0_reference_captured_ids = normalize_string_list(
        summary.get("phase0_reference_captured_ids", []),
        "qualification summary phase0_reference_captured_ids",
    )
    phase0_pending_ids = normalize_string_list(
        summary.get("phase0_pending_ids", []),
        "qualification summary phase0_pending_ids",
    )
    expect_unique(
        phase0_reference_captured_ids,
        "qualification summary phase0_reference_captured_ids",
    )
    expect_unique(phase0_pending_ids, "qualification summary phase0_pending_ids")

    blocked_phase0_examples = parse_runtime_lane_entries(
        summary.get("blocked_phase0_examples", []),
        "qualification summary blocked_phase0_examples",
    )
    blocked_case_study_families = parse_runtime_lane_entries(
        summary.get("blocked_case_study_families", []),
        "qualification summary blocked_case_study_families",
    )

    return {
        **summary,
        "phase0_reference_captured_ids": phase0_reference_captured_ids,
        "phase0_pending_ids": phase0_pending_ids,
        "blocked_phase0_examples": blocked_phase0_examples,
        "blocked_case_study_families": blocked_case_study_families,
    }


def load_phase0_qualification_summary(summary_path: Path) -> dict[str, Any]:
    summary = load_json(summary_path)
    expect(summary.get("schema_version") == 1, "phase-0 qualification summary schema_version must be 1")
    expect(
        summary.get("scope") == "phase0-packet-set-only",
        "phase-0 qualification summary scope must be phase0-packet-set-only",
    )

    current_state = str(summary.get("current_state", "")).strip()
    expect(current_state, "phase-0 qualification summary current_state must not be empty")

    required_boolean_fields = [
        "qualification_evidence_coherent",
        "packet_set_reference_comparison_passed",
        "packet_set_correct_digits_passed",
        "packet_set_failure_code_metadata_coherent",
        "packet_set_failure_code_audits_complete",
        "packet_set_required_failure_codes_satisfied",
        "packet_set_unexpected_failure_codes_absent",
        "phase0_packet_set_qualified",
        "milestone_m6_ready",
        "milestone_m6_requires_case_study_numerics",
        "digit_threshold_profiles_reported",
        "required_failure_code_profiles_reported",
        "regression_profiles_reported",
    ]
    for field in required_boolean_fields:
        if not isinstance(summary.get(field), bool):
            raise TypeError(f"phase-0 qualification summary {field} must be a bool")

    phase0_reference_captured_ids = normalize_string_list(
        summary.get("phase0_reference_captured_ids", []),
        "phase-0 qualification summary phase0_reference_captured_ids",
    )
    phase0_pending_ids = normalize_string_list(
        summary.get("phase0_pending_ids", []),
        "phase-0 qualification summary phase0_pending_ids",
    )
    reference_packet_labels = normalize_string_list(
        summary.get("reference_packet_labels", []),
        "phase-0 qualification summary reference_packet_labels",
    )
    blocking_reasons = normalize_string_list(
        summary.get("blocking_reasons", []),
        "phase-0 qualification summary blocking_reasons",
    )
    missing_required_failure_codes = normalize_string_list(
        summary.get("missing_required_failure_codes_across_packet_set", []),
        "phase-0 qualification summary missing_required_failure_codes_across_packet_set",
    )
    withheld_claims = normalize_string_list(
        summary.get("withheld_claims", []),
        "phase-0 qualification summary withheld_claims",
    )
    expect_unique(
        phase0_reference_captured_ids,
        "phase-0 qualification summary phase0_reference_captured_ids",
    )
    expect_unique(phase0_pending_ids, "phase-0 qualification summary phase0_pending_ids")
    expect_unique(reference_packet_labels, "phase-0 qualification summary reference_packet_labels")
    expect_unique(
        missing_required_failure_codes,
        "phase-0 qualification summary missing_required_failure_codes_across_packet_set",
    )

    return {
        **summary,
        "current_state": current_state,
        "phase0_reference_captured_ids": phase0_reference_captured_ids,
        "phase0_pending_ids": phase0_pending_ids,
        "reference_packet_labels": reference_packet_labels,
        "blocking_reasons": blocking_reasons,
        "missing_required_failure_codes_across_packet_set": missing_required_failure_codes,
        "withheld_claims": withheld_claims,
    }


def load_diagnostic_review_summary(summary_path: Path) -> dict[str, Any]:
    summary = load_json(summary_path)
    expect(summary.get("schema_version") == 1, "diagnostic review summary schema_version must be 1")
    expect(
        summary.get("scope") == "release-diagnostic-review",
        "diagnostic review summary scope must be release-diagnostic-review",
    )

    current_state = str(summary.get("current_state", "")).strip()
    expect(current_state, "diagnostic review summary current_state must not be empty")

    required_boolean_fields = [
        "diagnostic_review_complete",
        "required_failure_code_profiles_reviewed",
        "typed_failure_paths_preserved",
        "unstable_run_evidence_reviewed",
        "known_regression_outcomes_reviewed",
    ]
    for field in required_boolean_fields:
        if not isinstance(summary.get(field), bool):
            raise TypeError(f"diagnostic review summary {field} must be a bool")

    reviewed_failure_code_profiles = normalize_string_list(
        summary.get("reviewed_failure_code_profiles", []),
        "diagnostic review summary reviewed_failure_code_profiles",
    )
    missing_or_degraded_diagnostic_paths = normalize_string_list(
        summary.get("missing_or_degraded_diagnostic_paths", []),
        "diagnostic review summary missing_or_degraded_diagnostic_paths",
    )
    blocking_reasons = normalize_string_list(
        summary.get("blocking_reasons", []),
        "diagnostic review summary blocking_reasons",
    )
    withheld_claims = normalize_string_list(
        summary.get("withheld_claims", []),
        "diagnostic review summary withheld_claims",
    )
    expect_unique(
        reviewed_failure_code_profiles,
        "diagnostic review summary reviewed_failure_code_profiles",
    )
    expect_unique(
        missing_or_degraded_diagnostic_paths,
        "diagnostic review summary missing_or_degraded_diagnostic_paths",
    )
    if not summary["diagnostic_review_complete"]:
        expect(
            bool(missing_or_degraded_diagnostic_paths) or bool(blocking_reasons),
            "incomplete diagnostic review summary must report a blocker",
        )

    return {
        **summary,
        "current_state": current_state,
        "reviewed_failure_code_profiles": reviewed_failure_code_profiles,
        "missing_or_degraded_diagnostic_paths": missing_or_degraded_diagnostic_paths,
        "blocking_reasons": blocking_reasons,
        "withheld_claims": withheld_claims,
    }


def load_performance_review_summary(summary_path: Path) -> dict[str, Any]:
    summary = load_json(summary_path)
    expect(summary.get("schema_version") == 1, "performance review summary schema_version must be 1")
    expect(
        summary.get("scope") == "release-performance-review",
        "performance review summary scope must be release-performance-review",
    )

    current_state = str(summary.get("current_state", "")).strip()
    expect(current_state, "performance review summary current_state must not be empty")

    required_boolean_fields = [
        "performance_review_complete",
        "mandatory_benchmark_timings_reviewed",
        "benchmark_family_scope_reviewed",
        "clean_rebuild_gate_reviewed",
        "unstable_performance_runs_reviewed",
    ]
    for field in required_boolean_fields:
        if not isinstance(summary.get(field), bool):
            raise TypeError(f"performance review summary {field} must be a bool")

    reviewed_benchmark_families = normalize_string_list(
        summary.get("reviewed_benchmark_families", []),
        "performance review summary reviewed_benchmark_families",
    )
    missing_or_unreviewed_performance_paths = normalize_string_list(
        summary.get("missing_or_unreviewed_performance_paths", []),
        "performance review summary missing_or_unreviewed_performance_paths",
    )
    blocking_reasons = normalize_string_list(
        summary.get("blocking_reasons", []),
        "performance review summary blocking_reasons",
    )
    withheld_claims = normalize_string_list(
        summary.get("withheld_claims", []),
        "performance review summary withheld_claims",
    )
    expect_unique(
        reviewed_benchmark_families,
        "performance review summary reviewed_benchmark_families",
    )
    expect_unique(
        missing_or_unreviewed_performance_paths,
        "performance review summary missing_or_unreviewed_performance_paths",
    )
    if not summary["performance_review_complete"]:
        expect(
            bool(missing_or_unreviewed_performance_paths) or bool(blocking_reasons),
            "incomplete performance review summary must report a blocker",
        )

    return {
        **summary,
        "current_state": current_state,
        "reviewed_benchmark_families": reviewed_benchmark_families,
        "missing_or_unreviewed_performance_paths": missing_or_unreviewed_performance_paths,
        "blocking_reasons": blocking_reasons,
        "withheld_claims": withheld_claims,
    }


def load_docs_completion_summary(summary_path: Path) -> dict[str, Any]:
    summary = load_json(summary_path)
    expect(summary.get("schema_version") == 1, "docs completion summary schema_version must be 1")
    expect(
        summary.get("scope") == "release-docs-completion",
        "docs completion summary scope must be release-docs-completion",
    )

    current_state = str(summary.get("current_state", "")).strip()
    expect(current_state, "docs completion summary current_state must not be empty")

    required_boolean_fields = [
        "docs_completion_review_complete",
        "docs_targets_reviewed",
        "public_contract_aligned",
        "implementation_ledger_aligned",
        "verification_strategy_aligned",
        "reference_harness_guide_aligned",
        "reference_harness_readme_aligned",
        "completion_roadmap_aligned",
        "explicit_non_claims_reviewed",
    ]
    for field in required_boolean_fields:
        if not isinstance(summary.get(field), bool):
            raise TypeError(f"docs completion summary {field} must be a bool")

    reviewed_doc_targets = normalize_string_list(
        summary.get("reviewed_doc_targets", []),
        "docs completion summary reviewed_doc_targets",
    )
    missing_or_stale_doc_paths = normalize_string_list(
        summary.get("missing_or_stale_doc_paths", []),
        "docs completion summary missing_or_stale_doc_paths",
    )
    blocking_reasons = normalize_string_list(
        summary.get("blocking_reasons", []),
        "docs completion summary blocking_reasons",
    )
    withheld_claims = normalize_string_list(
        summary.get("withheld_claims", []),
        "docs completion summary withheld_claims",
    )
    expect_unique(reviewed_doc_targets, "docs completion summary reviewed_doc_targets")
    expect_unique(
        missing_or_stale_doc_paths,
        "docs completion summary missing_or_stale_doc_paths",
    )
    if not summary["docs_completion_review_complete"]:
        expect(
            bool(missing_or_stale_doc_paths) or bool(blocking_reasons),
            "incomplete docs completion summary must report a blocker",
        )

    return {
        **summary,
        "current_state": current_state,
        "reviewed_doc_targets": reviewed_doc_targets,
        "missing_or_stale_doc_paths": missing_or_stale_doc_paths,
        "blocking_reasons": blocking_reasons,
        "withheld_claims": withheld_claims,
    }


def load_parity_signoff_summary(summary_path: Path) -> dict[str, Any]:
    summary = load_json(summary_path)
    expect(summary.get("schema_version") == 1, "parity signoff summary schema_version must be 1")
    expect(
        summary.get("scope") == "release-parity-signoff",
        "parity signoff summary scope must be release-parity-signoff",
    )

    current_state = str(summary.get("current_state", "")).strip()
    expect(current_state, "parity signoff summary current_state must not be empty")

    required_boolean_fields = [
        "parity_signoff_complete",
        "qualification_closure_reviewed",
        "performance_review_summary_reviewed",
        "diagnostic_review_summary_reviewed",
        "docs_completion_note_reviewed",
        "withheld_claims_reviewed",
        "parity_signoff_required_inputs_preserved",
        "parity_signoff_required_outputs_preserved",
        "prerequisite_review_sections_preserved",
    ]
    for field in required_boolean_fields:
        if not isinstance(summary.get(field), bool):
            raise TypeError(f"parity signoff summary {field} must be a bool")

    required_release_review_sections = normalize_string_list(
        summary.get("required_release_review_sections", []),
        "parity signoff summary required_release_review_sections",
    )
    missing_or_blocked_parity_paths = normalize_string_list(
        summary.get("missing_or_blocked_parity_paths", []),
        "parity signoff summary missing_or_blocked_parity_paths",
    )
    blocking_reasons = normalize_string_list(
        summary.get("blocking_reasons", []),
        "parity signoff summary blocking_reasons",
    )
    withheld_claims = normalize_string_list(
        summary.get("withheld_claims", []),
        "parity signoff summary withheld_claims",
    )
    expect_unique(
        required_release_review_sections,
        "parity signoff summary required_release_review_sections",
    )
    expect(
        required_release_review_sections
        == list(PARITY_SIGNOFF_REQUIRED_RELEASE_REVIEW_SECTIONS),
        "parity signoff summary required_release_review_sections must preserve the "
        "qualification/performance/diagnostic/docs prerequisite review set",
    )
    expect_unique(
        missing_or_blocked_parity_paths,
        "parity signoff summary missing_or_blocked_parity_paths",
    )
    expect_unique(withheld_claims, "parity signoff summary withheld_claims")
    expect(
        withheld_claims == list(PARITY_SIGNOFF_REQUIRED_WITHHELD_CLAIMS),
        "parity signoff summary withheld_claims must preserve the exact release non-claims",
    )
    if summary["parity_signoff_complete"]:
        expect(
            summary["qualification_closure_reviewed"]
            and summary["performance_review_summary_reviewed"]
            and summary["diagnostic_review_summary_reviewed"]
            and summary["docs_completion_note_reviewed"]
            and summary["withheld_claims_reviewed"]
            and summary["parity_signoff_required_inputs_preserved"]
            and summary["parity_signoff_required_outputs_preserved"]
            and summary["prerequisite_review_sections_preserved"],
            "complete parity signoff summary must report every prerequisite and guardrail as "
            "reviewed",
        )
        expect(
            not missing_or_blocked_parity_paths and not blocking_reasons,
            "complete parity signoff summary must not report blockers",
        )
    else:
        expect(
            bool(missing_or_blocked_parity_paths) or bool(blocking_reasons),
            "incomplete parity signoff summary must report a blocker",
        )

    return {
        **summary,
        "current_state": current_state,
        "required_release_review_sections": required_release_review_sections,
        "missing_or_blocked_parity_paths": missing_or_blocked_parity_paths,
        "blocking_reasons": blocking_reasons,
        "withheld_claims": withheld_claims,
    }


def phase0_failure_code_blockers(phase0_summary: dict[str, Any] | None) -> list[str]:
    if phase0_summary is None:
        return []
    blockers: list[str] = []
    if not phase0_summary["packet_set_failure_code_metadata_coherent"]:
        blockers.append("phase0-failure-code-metadata")
    if not phase0_summary["packet_set_failure_code_audits_complete"]:
        blockers.append("phase0-failure-code-audit")
    if not phase0_summary["packet_set_required_failure_codes_satisfied"]:
        blockers.append("phase0-required-failure-codes")
    if not phase0_summary["packet_set_unexpected_failure_codes_absent"]:
        blockers.append("phase0-unexpected-failure-codes")
    return blockers


def performance_review_blockers(performance_summary: dict[str, Any] | None) -> list[str]:
    if performance_summary is None:
        return ["performance-review-summary"]

    blockers: list[str] = []
    if not performance_summary["performance_review_complete"]:
        blockers.append("performance-review-incomplete")
    if not performance_summary["mandatory_benchmark_timings_reviewed"]:
        blockers.append("performance-mandatory-benchmark-timings")
    if not performance_summary["benchmark_family_scope_reviewed"]:
        blockers.append("performance-benchmark-family-scope")
    if not performance_summary["clean_rebuild_gate_reviewed"]:
        blockers.append("performance-clean-rebuild-gate")
    if not performance_summary["unstable_performance_runs_reviewed"]:
        blockers.append("performance-unstable-run-evidence")
    blockers.extend(
        f"performance-path:{path}"
        for path in performance_summary["missing_or_unreviewed_performance_paths"]
    )

    deduplicated: list[str] = []
    seen: set[str] = set()
    for blocker in blockers:
        if blocker not in seen:
            deduplicated.append(blocker)
            seen.add(blocker)
    return deduplicated


def diagnostic_review_blockers(diagnostic_summary: dict[str, Any] | None) -> list[str]:
    if diagnostic_summary is None:
        return ["diagnostic-review-summary"]

    blockers: list[str] = []
    if not diagnostic_summary["diagnostic_review_complete"]:
        blockers.append("diagnostic-review-incomplete")
    if not diagnostic_summary["required_failure_code_profiles_reviewed"]:
        blockers.append("diagnostic-required-failure-code-profiles")
    if not diagnostic_summary["typed_failure_paths_preserved"]:
        blockers.append("diagnostic-typed-failure-paths")
    if not diagnostic_summary["unstable_run_evidence_reviewed"]:
        blockers.append("diagnostic-unstable-run-evidence")
    if not diagnostic_summary["known_regression_outcomes_reviewed"]:
        blockers.append("diagnostic-known-regression-outcomes")
    blockers.extend(
        f"diagnostic-path:{path}"
        for path in diagnostic_summary["missing_or_degraded_diagnostic_paths"]
    )

    deduplicated: list[str] = []
    seen: set[str] = set()
    for blocker in blockers:
        if blocker not in seen:
            deduplicated.append(blocker)
            seen.add(blocker)
    return deduplicated


def docs_completion_blockers(docs_summary: dict[str, Any] | None) -> list[str]:
    if docs_summary is None:
        return []

    blockers: list[str] = []
    if not docs_summary["docs_completion_review_complete"]:
        blockers.append("docs-completion-review-incomplete")
    if not docs_summary["docs_targets_reviewed"]:
        blockers.append("docs-targets-not-reviewed")
    if not docs_summary["public_contract_aligned"]:
        blockers.append("docs-public-contract-drift")
    if not docs_summary["implementation_ledger_aligned"]:
        blockers.append("docs-implementation-ledger-drift")
    if not docs_summary["verification_strategy_aligned"]:
        blockers.append("docs-verification-strategy-drift")
    if not docs_summary["reference_harness_guide_aligned"]:
        blockers.append("docs-reference-harness-guide-drift")
    if not docs_summary["reference_harness_readme_aligned"]:
        blockers.append("docs-reference-harness-readme-drift")
    if not docs_summary["completion_roadmap_aligned"]:
        blockers.append("docs-completion-roadmap-drift")
    if not docs_summary["explicit_non_claims_reviewed"]:
        blockers.append("docs-explicit-non-claims")
    blockers.extend(f"docs-path:{path}" for path in docs_summary["missing_or_stale_doc_paths"])

    deduplicated: list[str] = []
    seen: set[str] = set()
    for blocker in blockers:
        if blocker not in seen:
            deduplicated.append(blocker)
            seen.add(blocker)
    return deduplicated


def parity_signoff_blockers(parity_summary: dict[str, Any] | None) -> list[str]:
    if parity_summary is None:
        return ["parity-signoff-summary"]

    blockers: list[str] = []
    if not parity_summary["parity_signoff_complete"]:
        blockers.append("parity-signoff-incomplete")
    if not parity_summary["qualification_closure_reviewed"]:
        blockers.append("parity-qualification-closure")
    if not parity_summary["performance_review_summary_reviewed"]:
        blockers.append("parity-performance-review-summary")
    if not parity_summary["diagnostic_review_summary_reviewed"]:
        blockers.append("parity-diagnostic-review-summary")
    if not parity_summary["docs_completion_note_reviewed"]:
        blockers.append("parity-docs-completion-note")
    if not parity_summary["withheld_claims_reviewed"]:
        blockers.append("parity-withheld-claims")
    if not parity_summary["parity_signoff_required_inputs_preserved"]:
        blockers.append("parity-required-inputs")
    if not parity_summary["parity_signoff_required_outputs_preserved"]:
        blockers.append("parity-required-outputs")
    if not parity_summary["prerequisite_review_sections_preserved"]:
        blockers.append("parity-prerequisite-review-sections")
    blockers.extend(
        f"parity-path:{path}" for path in parity_summary["missing_or_blocked_parity_paths"]
    )

    deduplicated: list[str] = []
    seen: set[str] = set()
    for blocker in blockers:
        if blocker not in seen:
            deduplicated.append(blocker)
            seen.add(blocker)
    return deduplicated


def summarize_release_readiness(
    *,
    checklist_path: Path,
    qualification_summary_path: Path,
    phase0_qualification_summary_path: Path | None = None,
    performance_review_summary_path: Path | None = None,
    diagnostic_review_summary_path: Path | None = None,
    docs_completion_summary_path: Path | None = None,
    parity_signoff_summary_path: Path | None = None,
) -> dict[str, Any]:
    root = repo_root()
    checklist_path = checklist_path.resolve(strict=False)
    expect_path_within_root(checklist_path, root, "release checklist path")
    checklist = load_release_checklist(checklist_path)
    qualification_summary = load_qualification_summary(qualification_summary_path)
    phase0_qualification_summary = (
        load_phase0_qualification_summary(phase0_qualification_summary_path)
        if phase0_qualification_summary_path is not None
        else None
    )
    performance_review_summary = (
        load_performance_review_summary(performance_review_summary_path)
        if performance_review_summary_path is not None
        else None
    )
    diagnostic_review_summary = (
        load_diagnostic_review_summary(diagnostic_review_summary_path)
        if diagnostic_review_summary_path is not None
        else None
    )
    docs_completion_summary = (
        load_docs_completion_summary(docs_completion_summary_path)
        if docs_completion_summary_path is not None
        else None
    )
    parity_signoff_summary = (
        load_parity_signoff_summary(parity_signoff_summary_path)
        if parity_signoff_summary_path is not None
        else None
    )

    checklist_sources: list[dict[str, Any]] = []
    checklist_sources_present = True
    for source_id, relative_path in checklist["sources"].items():
        source_path = root / relative_path
        expect_path_within_root(source_path, root, f"release checklist source {source_id}")
        exists = source_path.exists()
        checklist_sources.append(
            {
                "id": source_id,
                "path": relative_path,
                "exists": exists,
                "within_repo": True,
            }
        )
        checklist_sources_present = checklist_sources_present and exists

    docs_completion_targets: list[dict[str, Any]] = []
    docs_completion_targets_present = True
    for target in checklist["docs_completion_targets"]:
        relative_path = target["path"]
        target_path = root / relative_path
        expect_path_within_root(target_path, root, f"docs completion target {relative_path}")
        exists = target_path.exists()
        docs_completion_targets.append(
            {
                "path": relative_path,
                "expectation": target["expectation"],
                "exists": exists,
                "within_repo": True,
            }
        )
        docs_completion_targets_present = docs_completion_targets_present and exists

    qualification_evidence_coherent = all(
        qualification_summary[field]
        for field in [
            "required_root_reference_captured",
            "required_root_captures_required_set",
            "required_root_only_captures_required_set",
            "optional_packets_preserve_bootstrap_only_state",
            "optional_capture_packets_match_scaffold",
            "captured_examples_publish_reference_artifacts",
            "captured_examples_pass_comparison_checks",
            "observed_reference_captured_matches_scaffold",
            "pending_examples_preserve_runtime_lane_hints",
            "blocked_case_study_families_preserve_runtime_lane_hints",
        ]
    )

    blocked_runtime_lanes = sorted(
        {
            entry["next_runtime_lane"]
            for entry in (
                qualification_summary["blocked_phase0_examples"]
                + qualification_summary["blocked_case_study_families"]
            )
        }
    )
    phase0_pending_ids = qualification_summary["phase0_pending_ids"]
    blocked_case_study_ids = [
        entry["id"] for entry in qualification_summary["blocked_case_study_families"]
    ]
    milestone_m6_blocked = (
        not qualification_evidence_coherent
        or bool(phase0_pending_ids)
        or bool(blocked_case_study_ids)
    )
    phase_f_blocked = bool(blocked_runtime_lanes)
    phase0_failure_blockers = phase0_failure_code_blockers(phase0_qualification_summary)
    performance_blockers = performance_review_blockers(performance_review_summary)
    diagnostic_blockers = diagnostic_review_blockers(diagnostic_review_summary)
    docs_blockers = docs_completion_blockers(docs_completion_summary)
    parity_blockers = parity_signoff_blockers(parity_signoff_summary)
    phase0_packet_set_blockers: list[str] = []
    if phase0_qualification_summary is not None:
        if not phase0_qualification_summary["phase0_packet_set_qualified"]:
            phase0_packet_set_blockers.append(
                "phase0:" + phase0_qualification_summary["current_state"]
            )
        phase0_packet_set_blockers.extend(phase0_failure_blockers)
        if phase0_qualification_summary["milestone_m6_requires_case_study_numerics"]:
            phase0_packet_set_blockers.append("case-study-numerics")

    release_prerequisites: list[dict[str, Any]] = []
    for prerequisite in checklist["release_prerequisites"]:
        prerequisite_id = prerequisite["id"]
        current_state = "blocked"
        blockers: list[str] = []
        satisfied = False

        if prerequisite_id == "milestone-m6":
            blockers = phase0_pending_ids + blocked_case_study_ids + phase0_packet_set_blockers
            current_state = (
                "blocked-on-qualification-closure"
                if milestone_m6_blocked
                or bool(phase0_packet_set_blockers)
                else "awaiting-reviewed-and-accepted-m6-packet"
            )
        elif prerequisite_id == "phase-f-feature-parity":
            blockers = blocked_runtime_lanes
            current_state = (
                "blocked-on-runtime-lanes"
                if phase_f_blocked
                else "awaiting-reviewed-and-accepted-m5-packet"
            )
        elif prerequisite_id == "retained-reference-evidence":
            blockers = phase0_pending_ids + blocked_case_study_ids + phase0_packet_set_blockers
            if not qualification_evidence_coherent:
                current_state = "incoherent-retained-evidence"
            elif phase0_packet_set_blockers:
                current_state = "captured-but-phase0-not-qualified"
            else:
                current_state = "captured-but-not-qualified"
        else:
            blockers = ["unknown-prerequisite"]
            current_state = "unknown-prerequisite"

        release_prerequisites.append(
            {
                "id": prerequisite_id,
                "required_state": prerequisite["required_state"],
                "notes": prerequisite["notes"],
                "current_state": current_state,
                "satisfied": satisfied,
                "blockers": blockers,
            }
        )

    review_sections: list[dict[str, Any]] = []
    for section in checklist["review_sections"]:
        section_id = section["id"]
        status = "blocked"
        blockers: list[str] = []

        if section_id == "qualification-corpus":
            blockers = phase0_pending_ids + blocked_case_study_ids + phase0_packet_set_blockers
            status = "blocked"
        elif section_id == "performance-review":
            blockers = list(performance_blockers)
            if milestone_m6_blocked or phase0_packet_set_blockers:
                blockers.insert(0, "milestone-m6")
            status = (
                "reviewed"
                if performance_review_summary is not None
                and performance_review_summary["performance_review_complete"]
                and not blockers
                else "blocked"
            )
        elif section_id == "diagnostic-review":
            blockers = diagnostic_blockers
            status = (
                "reviewed"
                if diagnostic_review_summary is not None
                and diagnostic_review_summary["diagnostic_review_complete"]
                and not blockers
                else "blocked"
            )
        elif section_id == "docs-completion":
            blockers = list(docs_blockers)
            if not checklist_sources_present:
                blockers.append("missing-checklist-source")
            if not docs_completion_targets_present:
                blockers.append("missing-doc-target")
            if docs_completion_summary is None:
                status = "ready-to-audit" if not blockers else "blocked"
            else:
                status = (
                    "reviewed"
                    if docs_completion_summary["docs_completion_review_complete"]
                    and not blockers
                    else "blocked"
                )
        elif section_id == "parity-signoff":
            if parity_signoff_summary is None:
                blockers = [
                    "qualification-corpus",
                    "performance-review",
                    "diagnostic-review",
                    "docs-completion",
                ]
            else:
                blockers = list(parity_blockers)
            status = (
                "reviewed"
                if parity_signoff_summary is not None
                and parity_signoff_summary["parity_signoff_complete"]
                and not blockers
                else "blocked"
            )
        else:
            blockers = ["unknown-review-section"]
            status = "blocked"

        review_sections.append(
            {
                "id": section_id,
                "required_inputs": section["required_inputs"],
                "required_outputs": section["required_outputs"],
                "notes": section["notes"],
                "status": status,
                "blockers": blockers,
            }
        )

    return {
        "schema_version": 1,
        "checklist_path": str(checklist_path),
        "qualification_summary_path": str(qualification_summary_path),
        "checklist_sources": checklist_sources,
        "checklist_sources_present": checklist_sources_present,
        "checklist_sources_within_repo": True,
        "docs_completion_targets": docs_completion_targets,
        "docs_completion_targets_present": docs_completion_targets_present,
        "docs_completion_targets_within_repo": True,
        "qualification_evidence_coherent": qualification_evidence_coherent,
        "phase0_reference_captured_ids": qualification_summary["phase0_reference_captured_ids"],
        "phase0_pending_ids": phase0_pending_ids,
        "blocked_phase0_examples": qualification_summary["blocked_phase0_examples"],
        "blocked_case_study_families": qualification_summary["blocked_case_study_families"],
        "blocked_runtime_lanes": blocked_runtime_lanes,
        "phase0_qualification_summary_path": (
            str(phase0_qualification_summary_path)
            if phase0_qualification_summary_path is not None
            else ""
        ),
        "phase0_qualification_evidence_present": phase0_qualification_summary is not None,
        "phase0_packet_set_current_state": (
            phase0_qualification_summary["current_state"]
            if phase0_qualification_summary is not None
            else "not-provided"
        ),
        "phase0_packet_set_qualified": (
            phase0_qualification_summary["phase0_packet_set_qualified"]
            if phase0_qualification_summary is not None
            else False
        ),
        "phase0_failure_code_blockers": phase0_failure_blockers,
        "phase0_failure_code_blockers_preserved": bool(phase0_failure_blockers),
        "phase0_packet_set_blocking_reasons": (
            phase0_qualification_summary["blocking_reasons"]
            if phase0_qualification_summary is not None
            else []
        ),
        "phase0_missing_required_failure_codes": (
            phase0_qualification_summary["missing_required_failure_codes_across_packet_set"]
            if phase0_qualification_summary is not None
            else []
        ),
        "phase0_withheld_claims": (
            phase0_qualification_summary["withheld_claims"]
            if phase0_qualification_summary is not None
            else []
        ),
        "performance_review_summary_path": (
            str(performance_review_summary_path)
            if performance_review_summary_path is not None
            else ""
        ),
        "performance_review_evidence_present": performance_review_summary is not None,
        "performance_review_current_state": (
            performance_review_summary["current_state"]
            if performance_review_summary is not None
            else "not-provided"
        ),
        "performance_review_complete": (
            performance_review_summary["performance_review_complete"]
            if performance_review_summary is not None
            else False
        ),
        "performance_review_blockers": performance_blockers,
        "performance_review_blockers_preserved": (
            performance_review_summary is not None and bool(performance_blockers)
        ),
        "performance_missing_or_unreviewed_paths": (
            performance_review_summary["missing_or_unreviewed_performance_paths"]
            if performance_review_summary is not None
            else []
        ),
        "performance_review_blocking_reasons": (
            performance_review_summary["blocking_reasons"]
            if performance_review_summary is not None
            else []
        ),
        "performance_reviewed_benchmark_families": (
            performance_review_summary["reviewed_benchmark_families"]
            if performance_review_summary is not None
            else []
        ),
        "performance_review_withheld_claims": (
            performance_review_summary["withheld_claims"]
            if performance_review_summary is not None
            else []
        ),
        "diagnostic_review_summary_path": (
            str(diagnostic_review_summary_path)
            if diagnostic_review_summary_path is not None
            else ""
        ),
        "diagnostic_review_evidence_present": diagnostic_review_summary is not None,
        "diagnostic_review_current_state": (
            diagnostic_review_summary["current_state"]
            if diagnostic_review_summary is not None
            else "not-provided"
        ),
        "diagnostic_review_complete": (
            diagnostic_review_summary["diagnostic_review_complete"]
            if diagnostic_review_summary is not None
            else False
        ),
        "diagnostic_review_blockers": diagnostic_blockers,
        "diagnostic_review_blockers_preserved": (
            diagnostic_review_summary is not None and bool(diagnostic_blockers)
        ),
        "diagnostic_missing_or_degraded_paths": (
            diagnostic_review_summary["missing_or_degraded_diagnostic_paths"]
            if diagnostic_review_summary is not None
            else []
        ),
        "diagnostic_review_blocking_reasons": (
            diagnostic_review_summary["blocking_reasons"]
            if diagnostic_review_summary is not None
            else []
        ),
        "diagnostic_reviewed_failure_code_profiles": (
            diagnostic_review_summary["reviewed_failure_code_profiles"]
            if diagnostic_review_summary is not None
            else []
        ),
        "diagnostic_review_withheld_claims": (
            diagnostic_review_summary["withheld_claims"]
            if diagnostic_review_summary is not None
            else []
        ),
        "docs_completion_summary_path": (
            str(docs_completion_summary_path)
            if docs_completion_summary_path is not None
            else ""
        ),
        "docs_completion_evidence_present": docs_completion_summary is not None,
        "docs_completion_current_state": (
            docs_completion_summary["current_state"]
            if docs_completion_summary is not None
            else "not-provided"
        ),
        "docs_completion_review_complete": (
            docs_completion_summary["docs_completion_review_complete"]
            if docs_completion_summary is not None
            else False
        ),
        "docs_completion_blockers": docs_blockers,
        "docs_completion_blockers_preserved": (
            docs_completion_summary is not None and bool(docs_blockers)
        ),
        "docs_completion_reviewed_targets": (
            docs_completion_summary["reviewed_doc_targets"]
            if docs_completion_summary is not None
            else []
        ),
        "docs_missing_or_stale_paths": (
            docs_completion_summary["missing_or_stale_doc_paths"]
            if docs_completion_summary is not None
            else []
        ),
        "docs_completion_blocking_reasons": (
            docs_completion_summary["blocking_reasons"]
            if docs_completion_summary is not None
            else []
        ),
        "docs_completion_withheld_claims": (
            docs_completion_summary["withheld_claims"]
            if docs_completion_summary is not None
            else []
        ),
        "parity_signoff_summary_path": (
            str(parity_signoff_summary_path)
            if parity_signoff_summary_path is not None
            else ""
        ),
        "parity_signoff_evidence_present": parity_signoff_summary is not None,
        "parity_signoff_current_state": (
            parity_signoff_summary["current_state"]
            if parity_signoff_summary is not None
            else "not-provided"
        ),
        "parity_signoff_complete": (
            parity_signoff_summary["parity_signoff_complete"]
            if parity_signoff_summary is not None
            else False
        ),
        "parity_signoff_blockers": parity_blockers,
        "parity_signoff_blockers_preserved": (
            parity_signoff_summary is not None and bool(parity_blockers)
        ),
        "parity_missing_or_blocked_paths": (
            parity_signoff_summary["missing_or_blocked_parity_paths"]
            if parity_signoff_summary is not None
            else []
        ),
        "parity_signoff_blocking_reasons": (
            parity_signoff_summary["blocking_reasons"]
            if parity_signoff_summary is not None
            else []
        ),
        "parity_required_release_review_sections": (
            parity_signoff_summary["required_release_review_sections"]
            if parity_signoff_summary is not None
            else []
        ),
        "parity_signoff_withheld_claims": (
            parity_signoff_summary["withheld_claims"]
            if parity_signoff_summary is not None
            else []
        ),
        "release_prerequisites": release_prerequisites,
        "review_sections": review_sections,
        "withheld_claims": checklist["explicit_non_claims"],
        "release_signoff_ready": False,
    }


def write_synthetic_qualification_summary(path: Path) -> None:
    write_json(
        path,
        {
            "schema_version": 1,
            "required_root_reference_captured": True,
            "required_root_captures_required_set": True,
            "required_root_only_captures_required_set": True,
            "optional_packets_preserve_bootstrap_only_state": True,
            "optional_capture_packets_match_scaffold": True,
            "captured_examples_publish_reference_artifacts": True,
            "captured_examples_pass_comparison_checks": True,
            "observed_reference_captured_matches_scaffold": True,
            "pending_examples_preserve_runtime_lane_hints": True,
            "blocked_case_study_families_preserve_runtime_lane_hints": True,
            "phase0_reference_captured_ids": [
                "automatic_loop",
                "automatic_vs_manual",
                "differential_equation_solver",
                "spacetime_dimension",
                "user_defined_amfmode",
                "user_defined_ending",
            ],
            "phase0_pending_ids": [
                "automatic_phasespace",
                "complex_kinematics",
                "feynman_prescription",
                "linear_propagator",
            ],
            "blocked_phase0_examples": [
                {
                    "id": "automatic_phasespace",
                    "next_runtime_lane": "b63k",
                },
                {
                    "id": "complex_kinematics",
                    "next_runtime_lane": "b61n",
                },
                {
                    "id": "feynman_prescription",
                    "next_runtime_lane": "b63k",
                },
                {
                    "id": "linear_propagator",
                    "next_runtime_lane": "b64k",
                },
            ],
            "blocked_case_study_families": [
                {
                    "id": "one-singular-endpoint-case",
                    "next_runtime_lane": "b62n",
                }
            ],
        },
    )


def write_synthetic_phase0_qualification_summary(path: Path) -> None:
    write_json(
        path,
        {
            "schema_version": 1,
            "scope": "phase0-packet-set-only",
            "current_state": "blocked-on-failure-code-audit",
            "phase0_reference_captured_ids": [
                "automatic_loop",
                "automatic_vs_manual",
                "differential_equation_solver",
                "spacetime_dimension",
                "user_defined_amfmode",
                "user_defined_ending",
            ],
            "phase0_pending_ids": [
                "automatic_phasespace",
                "complex_kinematics",
                "feynman_prescription",
                "linear_propagator",
            ],
            "reference_packet_labels": ["de-d0-pair", "required-set", "user-hook-pair"],
            "qualification_evidence_coherent": True,
            "packet_set_reference_comparison_passed": True,
            "packet_set_correct_digits_passed": True,
            "packet_set_failure_code_metadata_coherent": True,
            "packet_set_failure_code_audits_complete": False,
            "packet_set_required_failure_codes_satisfied": False,
            "packet_set_unexpected_failure_codes_absent": True,
            "missing_required_failure_codes_across_packet_set": [
                "boundary_unsolved",
                "continuation_budget_exhausted",
            ],
            "digit_threshold_profiles_reported": True,
            "required_failure_code_profiles_reported": True,
            "regression_profiles_reported": True,
            "phase0_packet_set_qualified": False,
            "milestone_m6_ready": False,
            "milestone_m6_requires_case_study_numerics": True,
            "blocking_reasons": [
                "retained packet-set is missing published failure-code audits",
                "retained packet-set is missing required typed failure codes",
            ],
            "withheld_claims": [
                "This summary does not compare retained case-study numerics.",
                "This summary does not by itself claim Milestone M6 closure.",
            ],
        },
    )


def write_synthetic_diagnostic_review_summary(path: Path) -> None:
    write_json(
        path,
        {
            "schema_version": 1,
            "scope": "release-diagnostic-review",
            "current_state": "blocked-on-missing-typed-failure-review",
            "diagnostic_review_complete": False,
            "required_failure_code_profiles_reviewed": True,
            "typed_failure_paths_preserved": False,
            "unstable_run_evidence_reviewed": False,
            "known_regression_outcomes_reviewed": True,
            "reviewed_failure_code_profiles": [
                "boundary_unsolved",
                "continuation_budget_exhausted",
                "unsupported_solver_path",
            ],
            "missing_or_degraded_diagnostic_paths": [
                "unsupported_solver_path",
                "continuation_budget_exhausted",
            ],
            "blocking_reasons": [
                "typed failure-path review is not complete",
                "unstable-run evidence has not been reviewed",
            ],
            "withheld_claims": [
                "This summary does not claim diagnostic review completion.",
                "This summary does not claim release readiness.",
            ],
        },
    )


def write_synthetic_performance_review_summary(path: Path) -> None:
    write_json(
        path,
        {
            "schema_version": 1,
            "scope": "release-performance-review",
            "current_state": "blocked-on-unreviewed-benchmark-timings",
            "performance_review_complete": False,
            "mandatory_benchmark_timings_reviewed": False,
            "benchmark_family_scope_reviewed": True,
            "clean_rebuild_gate_reviewed": True,
            "unstable_performance_runs_reviewed": False,
            "reviewed_benchmark_families": [
                "phase0-required-set",
                "phase0-d0-user-hook-pairs",
            ],
            "missing_or_unreviewed_performance_paths": [
                "mandatory-case-study-timings",
                "unstable-runtime-lane-timings",
            ],
            "blocking_reasons": [
                "mandatory benchmark timings have not been reviewed",
                "unstable runtime-lane timing evidence has not been reviewed",
            ],
            "withheld_claims": [
                "This summary does not claim performance review completion.",
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
            "current_state": "blocked-on-public-doc-alignment",
            "docs_completion_review_complete": False,
            "docs_targets_reviewed": True,
            "public_contract_aligned": False,
            "implementation_ledger_aligned": True,
            "verification_strategy_aligned": True,
            "reference_harness_guide_aligned": True,
            "reference_harness_readme_aligned": True,
            "completion_roadmap_aligned": False,
            "explicit_non_claims_reviewed": True,
            "reviewed_doc_targets": [
                "docs/public-contract.md",
                "docs/implementation-ledger.md",
                "docs/verification-strategy.md",
                "docs/reference-harness.md",
                "tools/reference-harness/README.md",
                "docs/full-amflow-completion-roadmap.md",
            ],
            "missing_or_stale_doc_paths": [
                "docs/public-contract.md",
                "docs/full-amflow-completion-roadmap.md",
            ],
            "blocking_reasons": [
                "public contract release-readiness caveats need review",
                "completion roadmap M7 blocker language needs review",
            ],
            "withheld_claims": [
                "This summary does not claim docs completion.",
                "This summary does not claim release readiness.",
            ],
        },
    )


def write_synthetic_parity_signoff_summary(path: Path) -> None:
    write_json(
        path,
        {
            "schema_version": 1,
            "scope": "release-parity-signoff",
            "current_state": "blocked-on-prerequisite-release-reviews",
            "parity_signoff_complete": False,
            "qualification_closure_reviewed": False,
            "performance_review_summary_reviewed": False,
            "diagnostic_review_summary_reviewed": False,
            "docs_completion_note_reviewed": False,
            "withheld_claims_reviewed": True,
            "parity_signoff_required_inputs_preserved": True,
            "parity_signoff_required_outputs_preserved": True,
            "prerequisite_review_sections_preserved": True,
            "required_release_review_sections": list(
                PARITY_SIGNOFF_REQUIRED_RELEASE_REVIEW_SECTIONS
            ),
            "missing_or_blocked_parity_paths": [
                "qualification-closure-note",
                "performance-review-summary",
                "diagnostic-review-summary",
                "docs-completion-note",
            ],
            "blocking_reasons": [
                "qualification closure note is not reviewed",
                "performance review summary is not complete",
                "diagnostic review summary is not complete",
                "docs completion note is not complete",
            ],
            "withheld_claims": list(PARITY_SIGNOFF_REQUIRED_WITHHELD_CLAIMS),
        },
    )


def run_self_check(checklist_path: Path) -> dict[str, Any]:
    with tempfile.TemporaryDirectory(prefix="amflow-release-signoff-readiness-self-check-") as tmp:
        temp_root = Path(tmp)
        qualification_summary_path = temp_root / "qualification-summary.json"
        phase0_qualification_summary_path = temp_root / "phase0-qualification-summary.json"
        performance_review_summary_path = temp_root / "performance-review-summary.json"
        diagnostic_review_summary_path = temp_root / "diagnostic-review-summary.json"
        docs_completion_summary_path = temp_root / "docs-completion-summary.json"
        parity_signoff_summary_path = temp_root / "parity-signoff-summary.json"
        summary_path = temp_root / "release-readiness-summary.json"

        write_synthetic_qualification_summary(qualification_summary_path)
        write_synthetic_phase0_qualification_summary(phase0_qualification_summary_path)
        write_synthetic_performance_review_summary(performance_review_summary_path)
        write_synthetic_diagnostic_review_summary(diagnostic_review_summary_path)
        write_synthetic_docs_completion_summary(docs_completion_summary_path)
        write_synthetic_parity_signoff_summary(parity_signoff_summary_path)

        malformed_sections_path = temp_root / "parity-signoff-malformed-sections.json"
        malformed_withheld_claims_path = (
            temp_root / "parity-signoff-malformed-withheld-claims.json"
        )
        malformed_guardrails_path = temp_root / "parity-signoff-malformed-guardrails.json"
        parity_signoff_fixture = load_json(parity_signoff_summary_path)
        write_json(
            malformed_sections_path,
            {
                **parity_signoff_fixture,
                "required_release_review_sections": ["qualification-corpus"],
            },
        )
        write_json(
            malformed_withheld_claims_path,
            {
                **parity_signoff_fixture,
                "withheld_claims": ["This summary does not claim release readiness."],
            },
        )
        write_json(
            malformed_guardrails_path,
            {
                **parity_signoff_fixture,
                "parity_signoff_complete": True,
                "qualification_closure_reviewed": True,
                "performance_review_summary_reviewed": True,
                "diagnostic_review_summary_reviewed": True,
                "docs_completion_note_reviewed": True,
                "withheld_claims_reviewed": True,
                "parity_signoff_required_inputs_preserved": False,
                "missing_or_blocked_parity_paths": [],
                "blocking_reasons": [],
            },
        )
        try:
            load_parity_signoff_summary(malformed_sections_path)
            parity_signoff_required_sections_schema_rejected = False
        except Exception:
            parity_signoff_required_sections_schema_rejected = True
        try:
            load_parity_signoff_summary(malformed_withheld_claims_path)
            parity_signoff_withheld_claims_schema_rejected = False
        except Exception:
            parity_signoff_withheld_claims_schema_rejected = True
        try:
            load_parity_signoff_summary(malformed_guardrails_path)
            parity_signoff_guardrail_schema_rejected = False
        except Exception:
            parity_signoff_guardrail_schema_rejected = True

        summary = summarize_release_readiness(
            checklist_path=checklist_path,
            qualification_summary_path=qualification_summary_path,
            phase0_qualification_summary_path=phase0_qualification_summary_path,
            performance_review_summary_path=performance_review_summary_path,
            diagnostic_review_summary_path=diagnostic_review_summary_path,
            docs_completion_summary_path=docs_completion_summary_path,
            parity_signoff_summary_path=parity_signoff_summary_path,
        )
        write_json(summary_path, summary)

        return {
            "checklist_sources_present": summary["checklist_sources_present"],
            "qualification_evidence_coherent": summary["qualification_evidence_coherent"],
            "milestone_m6_blocked": any(
                prerequisite["id"] == "milestone-m6"
                and prerequisite["current_state"] == "blocked-on-qualification-closure"
                for prerequisite in summary["release_prerequisites"]
            ),
            "phase_f_runtime_blockers_preserved": (
                summary["blocked_runtime_lanes"] == ["b61n", "b62n", "b63k", "b64k"]
            ),
            "retained_reference_evidence_not_overclaimed": any(
                prerequisite["id"] == "retained-reference-evidence"
                and prerequisite["current_state"] in {
                    "captured-but-not-qualified",
                    "captured-but-phase0-not-qualified",
                    "incoherent-retained-evidence",
                }
                for prerequisite in summary["release_prerequisites"]
            ),
            "phase0_verdict_consumed": summary["phase0_qualification_evidence_present"],
            "phase0_failure_code_blockers_preserved": (
                summary["phase0_failure_code_blockers"]
                == ["phase0-failure-code-audit", "phase0-required-failure-codes"]
            ),
            "phase0_withheld_claims_preserved": len(summary["phase0_withheld_claims"]) >= 2,
            "performance_review_evidence_consumed": summary["performance_review_evidence_present"],
            "performance_review_blockers_preserved": summary["performance_review_blockers"]
            == [
                "performance-review-incomplete",
                "performance-mandatory-benchmark-timings",
                "performance-unstable-run-evidence",
                "performance-path:mandatory-case-study-timings",
                "performance-path:unstable-runtime-lane-timings",
            ],
            "performance_review_withheld_claims_preserved": (
                len(summary["performance_review_withheld_claims"]) >= 2
            ),
            "performance_review_section_blocked": any(
                section["id"] == "performance-review"
                and section["status"] == "blocked"
                and "performance-path:mandatory-case-study-timings" in section["blockers"]
                for section in summary["review_sections"]
            ),
            "diagnostic_review_evidence_consumed": summary["diagnostic_review_evidence_present"],
            "diagnostic_review_blockers_preserved": summary["diagnostic_review_blockers"]
            == [
                "diagnostic-review-incomplete",
                "diagnostic-typed-failure-paths",
                "diagnostic-unstable-run-evidence",
                "diagnostic-path:unsupported_solver_path",
                "diagnostic-path:continuation_budget_exhausted",
            ],
            "diagnostic_review_withheld_claims_preserved": (
                len(summary["diagnostic_review_withheld_claims"]) >= 2
            ),
            "diagnostic_review_section_blocked": any(
                section["id"] == "diagnostic-review"
                and section["status"] == "blocked"
                and "diagnostic-path:unsupported_solver_path" in section["blockers"]
                for section in summary["review_sections"]
            ),
            "docs_completion_evidence_consumed": summary["docs_completion_evidence_present"],
            "docs_completion_blockers_preserved": summary["docs_completion_blockers"]
            == [
                "docs-completion-review-incomplete",
                "docs-public-contract-drift",
                "docs-completion-roadmap-drift",
                "docs-path:docs/public-contract.md",
                "docs-path:docs/full-amflow-completion-roadmap.md",
            ],
            "docs_completion_withheld_claims_preserved": (
                len(summary["docs_completion_withheld_claims"]) >= 2
            ),
            "docs_completion_section_blocked": any(
                section["id"] == "docs-completion"
                and section["status"] == "blocked"
                and "docs-path:docs/public-contract.md" in section["blockers"]
                for section in summary["review_sections"]
            ),
            "docs_completion_targets_present": summary["docs_completion_targets_present"],
            "docs_completion_section_ready_to_audit": any(
                section["id"] == "docs-completion" and section["status"] == "ready-to-audit"
                for section in summary["review_sections"]
            ),
            "parity_signoff_evidence_consumed": summary["parity_signoff_evidence_present"],
            "parity_signoff_blockers_preserved": summary["parity_signoff_blockers"]
            == [
                "parity-signoff-incomplete",
                "parity-qualification-closure",
                "parity-performance-review-summary",
                "parity-diagnostic-review-summary",
                "parity-docs-completion-note",
                "parity-path:qualification-closure-note",
                "parity-path:performance-review-summary",
                "parity-path:diagnostic-review-summary",
                "parity-path:docs-completion-note",
            ],
            "parity_signoff_withheld_claims_preserved": (
                summary["parity_signoff_withheld_claims"]
                == list(PARITY_SIGNOFF_REQUIRED_WITHHELD_CLAIMS)
            ),
            "parity_signoff_required_sections_schema_rejected": (
                parity_signoff_required_sections_schema_rejected
            ),
            "parity_signoff_withheld_claims_schema_rejected": (
                parity_signoff_withheld_claims_schema_rejected
            ),
            "parity_signoff_guardrail_schema_rejected": (
                parity_signoff_guardrail_schema_rejected
            ),
            "parity_signoff_section_blocked": any(
                section["id"] == "parity-signoff"
                and section["status"] == "blocked"
                and "parity-path:qualification-closure-note" in section["blockers"]
                for section in summary["review_sections"]
            ),
            "final_parity_signoff_blocked": any(
                section["id"] == "parity-signoff" and section["status"] == "blocked"
                for section in summary["review_sections"]
            ),
            "final_parity_signoff_waits_for_docs_completion": any(
                section["id"] == "parity-signoff"
                and (
                    "docs-completion" in section["blockers"]
                    or "parity-docs-completion-note" in section["blockers"]
                )
                for section in summary["review_sections"]
            ),
            "withheld_claims_preserved": len(summary["withheld_claims"]) >= 5,
            "summary_written": summary_path.exists(),
        }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--qualification-summary",
        type=Path,
        help="Path to the machine-readable summary emitted by qualification_readiness.py",
    )
    parser.add_argument(
        "--checklist-path",
        type=Path,
        help="Release-signoff checklist JSON path",
    )
    parser.add_argument(
        "--phase0-qualification-summary",
        type=Path,
        help="Optional path to the phase-0 packet-set qualification verdict summary",
    )
    parser.add_argument(
        "--performance-review-summary",
        type=Path,
        help="Optional path to the M7 performance-review summary",
    )
    parser.add_argument(
        "--diagnostic-review-summary",
        type=Path,
        help="Optional path to the M7 diagnostic-review summary",
    )
    parser.add_argument(
        "--docs-completion-summary",
        type=Path,
        help="Optional path to the M7 docs-completion summary",
    )
    parser.add_argument(
        "--parity-signoff-summary",
        type=Path,
        help="Optional path to the M7 parity-signoff summary",
    )
    parser.add_argument(
        "--summary-path",
        type=Path,
        help="Optional output file for the release-readiness summary",
    )
    parser.add_argument(
        "--self-check",
        action="store_true",
        help="Run a synthetic release-readiness audit against the live checklist",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    checklist_path = (
        args.checklist_path
        if args.checklist_path is not None
        else repo_root() / "tools" / "reference-harness" / "templates" / "release-signoff-checklist.json"
    )

    if args.self_check:
        print(json.dumps(run_self_check(checklist_path), indent=2, sort_keys=True))
        return 0

    expect(
        args.qualification_summary is not None,
        "--qualification-summary is required unless --self-check is used",
    )
    summary = summarize_release_readiness(
        checklist_path=checklist_path,
        qualification_summary_path=args.qualification_summary,
        phase0_qualification_summary_path=args.phase0_qualification_summary,
        performance_review_summary_path=args.performance_review_summary,
        diagnostic_review_summary_path=args.diagnostic_review_summary,
        docs_completion_summary_path=args.docs_completion_summary,
        parity_signoff_summary_path=args.parity_signoff_summary,
    )
    if args.summary_path is not None:
        write_json(args.summary_path, summary)
    print(json.dumps(summary, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
