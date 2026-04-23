#!/usr/bin/env python3
"""Summarize retained phase-0 packet-set qualification evidence without overclaiming M6 closure."""

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
        entries.append({"id": entry_id, "next_runtime_lane": next_runtime_lane})
    return entries


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

    return {
        **summary,
        "phase0_reference_captured_ids": phase0_reference_captured_ids,
        "phase0_pending_ids": phase0_pending_ids,
        "blocked_phase0_examples": blocked_phase0_examples,
    }


def load_packet_set_comparison_summary(summary_path: Path) -> dict[str, Any]:
    summary = load_json(summary_path)
    expect(
        summary.get("schema_version") == 1,
        "packet-set comparison summary schema_version must be 1",
    )
    required_boolean_fields = [
        "required_packet_present",
        "reference_packet_labels_match_scaffold_reference_captured",
        "compared_phase0_ids_match_scaffold_reference_captured",
        "reference_benchmarks_pass_retained_capture_checks",
        "candidate_packet_benchmark_sets_match_reference",
        "candidate_result_manifests_exist",
        "candidate_primary_run_manifests_exist",
        "candidate_output_names_match_reference",
        "candidate_output_hashes_match_reference",
        "digit_threshold_profiles_reported",
        "required_failure_code_profiles_reported",
        "regression_profiles_reported",
    ]
    for field in required_boolean_fields:
        if not isinstance(summary.get(field), bool):
            raise TypeError(f"packet-set comparison summary {field} must be a bool")
    reference_packet_labels = normalize_string_list(
        summary.get("reference_packet_labels", []),
        "packet-set comparison summary reference_packet_labels",
    )
    expected_packet_labels = normalize_string_list(
        summary.get("expected_reference_packet_labels", []),
        "packet-set comparison summary expected_reference_packet_labels",
    )
    compared_phase0_ids = normalize_string_list(
        summary.get("compared_phase0_ids", []),
        "packet-set comparison summary compared_phase0_ids",
    )
    expected_phase0_ids = normalize_string_list(
        summary.get("expected_reference_captured_phase0_ids", []),
        "packet-set comparison summary expected_reference_captured_phase0_ids",
    )
    expect_unique(reference_packet_labels, "packet-set comparison summary reference_packet_labels")
    expect_unique(expected_packet_labels, "packet-set comparison summary expected_reference_packet_labels")
    expect_unique(compared_phase0_ids, "packet-set comparison summary compared_phase0_ids")
    expect_unique(expected_phase0_ids, "packet-set comparison summary expected_reference_captured_phase0_ids")
    return {
        **summary,
        "reference_packet_labels": reference_packet_labels,
        "expected_reference_packet_labels": expected_packet_labels,
        "compared_phase0_ids": compared_phase0_ids,
        "expected_reference_captured_phase0_ids": expected_phase0_ids,
    }


def load_packet_set_correct_digit_summary(summary_path: Path) -> dict[str, Any]:
    summary = load_json(summary_path)
    expect(
        summary.get("schema_version") == 1,
        "packet-set correct-digit summary schema_version must be 1",
    )
    required_boolean_fields = [
        "required_packet_present",
        "reference_packet_labels_match_scaffold_reference_captured",
        "compared_phase0_ids_match_scaffold_reference_captured",
        "reference_benchmarks_pass_retained_capture_checks",
        "candidate_packet_benchmark_sets_match_reference",
        "candidate_result_manifests_exist",
        "candidate_primary_run_manifests_exist",
        "candidate_output_names_match_reference",
        "candidate_numeric_literal_skeletons_match_reference",
        "digit_threshold_profiles_reported",
        "required_failure_code_profiles_reported",
        "regression_profiles_reported",
        "all_compared_benchmarks_meet_digit_thresholds",
    ]
    for field in required_boolean_fields:
        if not isinstance(summary.get(field), bool):
            raise TypeError(f"packet-set correct-digit summary {field} must be a bool")
    minimum_correct_digits = summary.get("minimum_observed_correct_digits_across_packet_set")
    if not isinstance(minimum_correct_digits, int):
        raise TypeError(
            "packet-set correct-digit summary minimum_observed_correct_digits_across_packet_set "
            "must be an int"
        )
    reference_packet_labels = normalize_string_list(
        summary.get("reference_packet_labels", []),
        "packet-set correct-digit summary reference_packet_labels",
    )
    expected_packet_labels = normalize_string_list(
        summary.get("expected_reference_packet_labels", []),
        "packet-set correct-digit summary expected_reference_packet_labels",
    )
    compared_phase0_ids = normalize_string_list(
        summary.get("compared_phase0_ids", []),
        "packet-set correct-digit summary compared_phase0_ids",
    )
    expected_phase0_ids = normalize_string_list(
        summary.get("expected_reference_captured_phase0_ids", []),
        "packet-set correct-digit summary expected_reference_captured_phase0_ids",
    )
    expect_unique(reference_packet_labels, "packet-set correct-digit summary reference_packet_labels")
    expect_unique(expected_packet_labels, "packet-set correct-digit summary expected_reference_packet_labels")
    expect_unique(compared_phase0_ids, "packet-set correct-digit summary compared_phase0_ids")
    expect_unique(expected_phase0_ids, "packet-set correct-digit summary expected_reference_captured_phase0_ids")
    return {
        **summary,
        "minimum_observed_correct_digits_across_packet_set": minimum_correct_digits,
        "reference_packet_labels": reference_packet_labels,
        "expected_reference_packet_labels": expected_packet_labels,
        "compared_phase0_ids": compared_phase0_ids,
        "expected_reference_captured_phase0_ids": expected_phase0_ids,
    }


def load_packet_set_failure_code_summary(summary_path: Path) -> dict[str, Any]:
    summary = load_json(summary_path)
    expect(
        summary.get("schema_version") == 1,
        "packet-set failure-code summary schema_version must be 1",
    )
    required_boolean_fields = [
        "required_packet_present",
        "candidate_packet_labels_match_scaffold_reference_captured",
        "audited_phase0_ids_match_scaffold_reference_captured",
        "candidate_packet_benchmark_sets_match_packet_summaries",
        "all_compared_benchmarks_publish_failure_code_audits",
        "all_compared_benchmarks_report_required_failure_codes",
        "any_compared_benchmarks_report_unexpected_failure_codes",
        "digit_threshold_profiles_reported",
        "required_failure_code_profiles_reported",
        "regression_profiles_reported",
    ]
    for field in required_boolean_fields:
        if not isinstance(summary.get(field), bool):
            raise TypeError(f"packet-set failure-code summary {field} must be a bool")
    candidate_packet_labels = normalize_string_list(
        summary.get("candidate_packet_labels", []),
        "packet-set failure-code summary candidate_packet_labels",
    )
    expected_packet_labels = normalize_string_list(
        summary.get("expected_reference_captured_packet_labels", []),
        "packet-set failure-code summary expected_reference_captured_packet_labels",
    )
    audited_phase0_ids = normalize_string_list(
        summary.get("audited_phase0_ids", []),
        "packet-set failure-code summary audited_phase0_ids",
    )
    expected_phase0_ids = normalize_string_list(
        summary.get("expected_reference_captured_phase0_ids", []),
        "packet-set failure-code summary expected_reference_captured_phase0_ids",
    )
    missing_required_failure_codes = normalize_string_list(
        summary.get("missing_required_failure_codes_across_packet_set", []),
        "packet-set failure-code summary missing_required_failure_codes_across_packet_set",
    )
    expect_unique(candidate_packet_labels, "packet-set failure-code summary candidate_packet_labels")
    expect_unique(
        expected_packet_labels,
        "packet-set failure-code summary expected_reference_captured_packet_labels",
    )
    expect_unique(audited_phase0_ids, "packet-set failure-code summary audited_phase0_ids")
    expect_unique(
        expected_phase0_ids,
        "packet-set failure-code summary expected_reference_captured_phase0_ids",
    )
    expect_unique(
        missing_required_failure_codes,
        "packet-set failure-code summary missing_required_failure_codes_across_packet_set",
    )
    return {
        **summary,
        "candidate_packet_labels": candidate_packet_labels,
        "expected_reference_captured_packet_labels": expected_packet_labels,
        "audited_phase0_ids": audited_phase0_ids,
        "expected_reference_captured_phase0_ids": expected_phase0_ids,
        "missing_required_failure_codes_across_packet_set": missing_required_failure_codes,
    }


def determine_current_state(
    *,
    qualification_evidence_coherent: bool,
    packet_labels_match_across_inputs: bool,
    phase0_ids_match_across_inputs: bool,
    packet_set_reference_comparison_passed: bool,
    packet_set_correct_digits_passed: bool,
    packet_set_failure_code_metadata_coherent: bool,
    packet_set_failure_code_audits_complete: bool,
    packet_set_required_failure_codes_satisfied: bool,
    packet_set_unexpected_failure_codes_absent: bool,
) -> str:
    if not qualification_evidence_coherent:
        return "blocked-on-retained-readiness"
    if not packet_labels_match_across_inputs or not phase0_ids_match_across_inputs:
        return "blocked-on-summary-drift"
    if not packet_set_reference_comparison_passed:
        return "blocked-on-reference-comparison"
    if not packet_set_correct_digits_passed:
        return "blocked-on-correct-digit-thresholds"
    if not packet_set_failure_code_metadata_coherent:
        return "blocked-on-failure-code-metadata"
    if not packet_set_failure_code_audits_complete:
        return "blocked-on-failure-code-audit"
    if not packet_set_required_failure_codes_satisfied:
        return "blocked-on-required-failure-codes"
    if not packet_set_unexpected_failure_codes_absent:
        return "blocked-on-unexpected-failure-codes"
    return "phase0-packet-set-qualified"


def summarize_phase0_packet_set_qualification(
    *,
    qualification_summary_path: Path,
    packet_set_comparison_summary_path: Path,
    packet_set_correct_digit_summary_path: Path,
    packet_set_failure_code_summary_path: Path,
) -> dict[str, Any]:
    qualification_summary = load_qualification_summary(qualification_summary_path)
    comparison_summary = load_packet_set_comparison_summary(packet_set_comparison_summary_path)
    correct_digit_summary = load_packet_set_correct_digit_summary(
        packet_set_correct_digit_summary_path
    )
    failure_code_summary = load_packet_set_failure_code_summary(
        packet_set_failure_code_summary_path
    )

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
        ]
    )

    phase0_reference_captured_ids = qualification_summary["phase0_reference_captured_ids"]
    comparison_phase0_ids = comparison_summary["compared_phase0_ids"]
    correct_digit_phase0_ids = correct_digit_summary["compared_phase0_ids"]
    failure_code_phase0_ids = failure_code_summary["audited_phase0_ids"]

    packet_labels = comparison_summary["reference_packet_labels"]
    correct_digit_packet_labels = correct_digit_summary["reference_packet_labels"]
    failure_code_packet_labels = failure_code_summary["candidate_packet_labels"]

    expect(
        set(comparison_phase0_ids) == set(phase0_reference_captured_ids),
        "phase-0 ids in the packet-set comparison summary must match qualification_readiness.py",
    )
    expect(
        set(correct_digit_phase0_ids) == set(phase0_reference_captured_ids),
        "phase-0 ids in the packet-set correct-digit summary must match qualification_readiness.py",
    )
    expect(
        set(failure_code_phase0_ids) == set(phase0_reference_captured_ids),
        "phase-0 ids in the packet-set failure-code summary must match qualification_readiness.py",
    )
    expect(
        set(correct_digit_packet_labels) == set(packet_labels),
        "packet labels in the packet-set correct-digit summary must match the comparison summary",
    )
    expect(
        set(failure_code_packet_labels) == set(packet_labels),
        "packet labels in the packet-set failure-code summary must match the comparison summary",
    )

    packet_labels_match_across_inputs = True
    phase0_ids_match_across_inputs = True

    packet_set_reference_comparison_passed = all(
        comparison_summary[field]
        for field in [
            "required_packet_present",
            "reference_packet_labels_match_scaffold_reference_captured",
            "compared_phase0_ids_match_scaffold_reference_captured",
            "reference_benchmarks_pass_retained_capture_checks",
            "candidate_packet_benchmark_sets_match_reference",
            "candidate_result_manifests_exist",
            "candidate_primary_run_manifests_exist",
            "candidate_output_names_match_reference",
            "candidate_output_hashes_match_reference",
        ]
    )

    packet_set_correct_digits_passed = all(
        correct_digit_summary[field]
        for field in [
            "required_packet_present",
            "reference_packet_labels_match_scaffold_reference_captured",
            "compared_phase0_ids_match_scaffold_reference_captured",
            "reference_benchmarks_pass_retained_capture_checks",
            "candidate_packet_benchmark_sets_match_reference",
            "candidate_result_manifests_exist",
            "candidate_primary_run_manifests_exist",
            "candidate_output_names_match_reference",
            "candidate_numeric_literal_skeletons_match_reference",
            "all_compared_benchmarks_meet_digit_thresholds",
        ]
    )

    packet_set_failure_code_audits_complete = failure_code_summary[
        "all_compared_benchmarks_publish_failure_code_audits"
    ]
    packet_set_required_failure_codes_satisfied = failure_code_summary[
        "all_compared_benchmarks_report_required_failure_codes"
    ]
    packet_set_unexpected_failure_codes_absent = not failure_code_summary[
        "any_compared_benchmarks_report_unexpected_failure_codes"
    ]
    packet_set_failure_code_metadata_coherent = (
        failure_code_summary["required_packet_present"]
        and failure_code_summary["candidate_packet_labels_match_scaffold_reference_captured"]
        and failure_code_summary["audited_phase0_ids_match_scaffold_reference_captured"]
        and failure_code_summary["candidate_packet_benchmark_sets_match_packet_summaries"]
    )
    packet_set_failure_codes_passed = (
        packet_set_failure_code_metadata_coherent
        and packet_set_failure_code_audits_complete
        and packet_set_required_failure_codes_satisfied
        and packet_set_unexpected_failure_codes_absent
    )

    digit_threshold_profiles_reported = all(
        summary["digit_threshold_profiles_reported"]
        for summary in [comparison_summary, correct_digit_summary, failure_code_summary]
    )
    required_failure_code_profiles_reported = all(
        summary["required_failure_code_profiles_reported"]
        for summary in [comparison_summary, correct_digit_summary, failure_code_summary]
    )
    regression_profiles_reported = all(
        summary["regression_profiles_reported"]
        for summary in [comparison_summary, correct_digit_summary, failure_code_summary]
    )

    current_state = determine_current_state(
        qualification_evidence_coherent=qualification_evidence_coherent,
        packet_labels_match_across_inputs=packet_labels_match_across_inputs,
        phase0_ids_match_across_inputs=phase0_ids_match_across_inputs,
        packet_set_reference_comparison_passed=packet_set_reference_comparison_passed,
        packet_set_correct_digits_passed=packet_set_correct_digits_passed,
        packet_set_failure_code_metadata_coherent=packet_set_failure_code_metadata_coherent,
        packet_set_failure_code_audits_complete=packet_set_failure_code_audits_complete,
        packet_set_required_failure_codes_satisfied=packet_set_required_failure_codes_satisfied,
        packet_set_unexpected_failure_codes_absent=packet_set_unexpected_failure_codes_absent,
    )

    blocking_reasons: list[str] = []
    if not qualification_evidence_coherent:
        blocking_reasons.append("retained phase-0 readiness summary is not yet coherent")
    if not packet_labels_match_across_inputs:
        blocking_reasons.append("packet labels drifted across the retained packet-set summaries")
    if not phase0_ids_match_across_inputs:
        blocking_reasons.append("captured phase-0 ids drifted across the retained packet-set summaries")
    if not packet_set_reference_comparison_passed:
        blocking_reasons.append("retained packet-set comparison has not fully passed")
    if not packet_set_correct_digits_passed:
        blocking_reasons.append("retained packet-set correct-digit scoring has not fully passed")
    if not packet_set_failure_code_metadata_coherent:
        blocking_reasons.append("retained packet-set failure-code audit metadata is not coherent")
    if not packet_set_failure_code_audits_complete:
        blocking_reasons.append("retained packet-set is missing published failure-code audits")
    if not packet_set_required_failure_codes_satisfied:
        blocking_reasons.append("retained packet-set is missing required typed failure codes")
    if not packet_set_unexpected_failure_codes_absent:
        blocking_reasons.append("retained packet-set still reports unexpected failure codes")

    phase0_packet_set_qualified = (
        qualification_evidence_coherent
        and packet_labels_match_across_inputs
        and phase0_ids_match_across_inputs
        and packet_set_reference_comparison_passed
        and packet_set_correct_digits_passed
        and packet_set_failure_codes_passed
    )

    return {
        "schema_version": 1,
        "scope": "phase0-packet-set-only",
        "current_state": current_state,
        "qualification_summary_path": str(qualification_summary_path),
        "packet_set_comparison_summary_path": str(packet_set_comparison_summary_path),
        "packet_set_correct_digit_summary_path": str(packet_set_correct_digit_summary_path),
        "packet_set_failure_code_summary_path": str(packet_set_failure_code_summary_path),
        "phase0_reference_captured_ids": phase0_reference_captured_ids,
        "phase0_pending_ids": qualification_summary["phase0_pending_ids"],
        "reference_packet_labels": sorted(packet_labels),
        "qualification_evidence_coherent": qualification_evidence_coherent,
        "packet_labels_match_across_inputs": packet_labels_match_across_inputs,
        "phase0_ids_match_across_inputs": phase0_ids_match_across_inputs,
        "packet_set_reference_comparison_passed": packet_set_reference_comparison_passed,
        "packet_set_correct_digits_passed": packet_set_correct_digits_passed,
        "minimum_observed_correct_digits_across_packet_set": correct_digit_summary[
            "minimum_observed_correct_digits_across_packet_set"
        ],
        "packet_set_failure_code_metadata_coherent": packet_set_failure_code_metadata_coherent,
        "packet_set_failure_code_audits_complete": packet_set_failure_code_audits_complete,
        "packet_set_required_failure_codes_satisfied": packet_set_required_failure_codes_satisfied,
        "packet_set_unexpected_failure_codes_absent": packet_set_unexpected_failure_codes_absent,
        "missing_required_failure_codes_across_packet_set": failure_code_summary[
            "missing_required_failure_codes_across_packet_set"
        ],
        "digit_threshold_profiles_reported": digit_threshold_profiles_reported,
        "required_failure_code_profiles_reported": required_failure_code_profiles_reported,
        "regression_profiles_reported": regression_profiles_reported,
        "phase0_packet_set_qualified": phase0_packet_set_qualified,
        "milestone_m6_ready": False,
        "milestone_m6_requires_case_study_numerics": True,
        "blocked_phase0_examples": qualification_summary["blocked_phase0_examples"],
        "blocking_reasons": blocking_reasons,
        "withheld_claims": [
            "This summary does not compare retained case-study numerics.",
            "This summary does not by itself claim Milestone M6 closure.",
            "This summary does not mark Milestone M7 or release readiness.",
            "This summary does not launch the C++ runtime or create new retained captures.",
        ],
    }


def synthetic_phase0_reference_captured_ids() -> list[str]:
    return [
        "automatic_loop",
        "automatic_vs_manual",
        "differential_equation_solver",
        "spacetime_dimension",
        "user_defined_amfmode",
        "user_defined_ending",
    ]


def synthetic_phase0_pending_ids() -> list[str]:
    return [
        "automatic_phasespace",
        "complex_kinematics",
        "feynman_prescription",
        "linear_propagator",
    ]


def synthetic_packet_labels() -> list[str]:
    return ["de-d0-pair", "required-set", "user-hook-pair"]


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
            "phase0_reference_captured_ids": synthetic_phase0_reference_captured_ids(),
            "phase0_pending_ids": synthetic_phase0_pending_ids(),
            "blocked_phase0_examples": [
                {"id": "complex_kinematics", "next_runtime_lane": "b61n"},
                {"id": "feynman_prescription", "next_runtime_lane": "b63k"},
                {"id": "linear_propagator", "next_runtime_lane": "b64k"},
            ],
        },
    )


def write_synthetic_packet_set_comparison_summary(path: Path) -> None:
    write_json(
        path,
        {
            "schema_version": 1,
            "reference_packet_labels": synthetic_packet_labels(),
            "expected_reference_packet_labels": synthetic_packet_labels(),
            "required_packet_present": True,
            "compared_phase0_ids": synthetic_phase0_reference_captured_ids(),
            "expected_reference_captured_phase0_ids": synthetic_phase0_reference_captured_ids(),
            "reference_packet_labels_match_scaffold_reference_captured": True,
            "compared_phase0_ids_match_scaffold_reference_captured": True,
            "reference_benchmarks_pass_retained_capture_checks": True,
            "candidate_packet_benchmark_sets_match_reference": True,
            "candidate_result_manifests_exist": True,
            "candidate_primary_run_manifests_exist": True,
            "candidate_output_names_match_reference": True,
            "candidate_output_hashes_match_reference": True,
            "digit_threshold_profiles_reported": True,
            "required_failure_code_profiles_reported": True,
            "regression_profiles_reported": True,
        },
    )


def write_synthetic_packet_set_correct_digit_summary(
    path: Path,
    *,
    meets_digit_thresholds: bool,
) -> None:
    write_json(
        path,
        {
            "schema_version": 1,
            "reference_packet_labels": synthetic_packet_labels(),
            "expected_reference_packet_labels": synthetic_packet_labels(),
            "required_packet_present": True,
            "compared_phase0_ids": synthetic_phase0_reference_captured_ids(),
            "expected_reference_captured_phase0_ids": synthetic_phase0_reference_captured_ids(),
            "reference_packet_labels_match_scaffold_reference_captured": True,
            "compared_phase0_ids_match_scaffold_reference_captured": True,
            "reference_benchmarks_pass_retained_capture_checks": True,
            "candidate_packet_benchmark_sets_match_reference": True,
            "candidate_result_manifests_exist": True,
            "candidate_primary_run_manifests_exist": True,
            "candidate_output_names_match_reference": True,
            "candidate_numeric_literal_skeletons_match_reference": True,
            "digit_threshold_profiles_reported": True,
            "required_failure_code_profiles_reported": True,
            "regression_profiles_reported": True,
            "all_compared_benchmarks_meet_digit_thresholds": meets_digit_thresholds,
            "minimum_observed_correct_digits_across_packet_set": (
                50 if meets_digit_thresholds else 18
            ),
        },
    )


def write_synthetic_packet_set_failure_code_summary(
    path: Path,
    *,
    publish_audits: bool,
    report_required_failure_codes: bool,
    unexpected_failure_codes: bool,
    metadata_coherent: bool = True,
) -> None:
    missing_required_failure_codes = []
    if not report_required_failure_codes:
        missing_required_failure_codes = ["boundary_unsolved"]
    write_json(
        path,
        {
            "schema_version": 1,
            "candidate_packet_labels": synthetic_packet_labels(),
            "expected_reference_captured_packet_labels": synthetic_packet_labels(),
            "required_packet_present": True,
            "audited_phase0_ids": synthetic_phase0_reference_captured_ids(),
            "expected_reference_captured_phase0_ids": synthetic_phase0_reference_captured_ids(),
            "candidate_packet_labels_match_scaffold_reference_captured": True,
            "audited_phase0_ids_match_scaffold_reference_captured": True,
            "candidate_packet_benchmark_sets_match_packet_summaries": metadata_coherent,
            "all_compared_benchmarks_publish_failure_code_audits": publish_audits,
            "all_compared_benchmarks_report_required_failure_codes": report_required_failure_codes,
            "any_compared_benchmarks_report_unexpected_failure_codes": unexpected_failure_codes,
            "missing_required_failure_codes_across_packet_set": missing_required_failure_codes,
            "digit_threshold_profiles_reported": True,
            "required_failure_code_profiles_reported": True,
            "regression_profiles_reported": True,
        },
    )


def run_self_check() -> dict[str, Any]:
    with tempfile.TemporaryDirectory(prefix="amflow-phase0-qualification-packet-set-self-check-") as tmp:
        temp_root = Path(tmp)
        qualification_summary_path = temp_root / "qualification-summary.json"
        comparison_summary_path = temp_root / "comparison-summary.json"
        correct_digit_summary_path = temp_root / "correct-digit-summary.json"
        failing_correct_digit_summary_path = temp_root / "correct-digit-summary-failing.json"
        passing_failure_code_summary_path = temp_root / "failure-code-summary-passing.json"
        missing_audit_summary_path = temp_root / "failure-code-summary-missing-audit.json"
        metadata_drift_summary_path = temp_root / "failure-code-summary-metadata-drift.json"
        unexpected_failure_summary_path = temp_root / "failure-code-summary-unexpected.json"
        drift_comparison_summary_path = temp_root / "comparison-summary-drift.json"
        drift_failure_code_summary_path = temp_root / "failure-code-summary-drift.json"
        summary_path = temp_root / "phase0-qualification-summary.json"

        write_synthetic_qualification_summary(qualification_summary_path)
        write_synthetic_packet_set_comparison_summary(comparison_summary_path)
        write_synthetic_packet_set_correct_digit_summary(
            correct_digit_summary_path,
            meets_digit_thresholds=True,
        )
        write_synthetic_packet_set_correct_digit_summary(
            failing_correct_digit_summary_path,
            meets_digit_thresholds=False,
        )
        write_synthetic_packet_set_failure_code_summary(
            passing_failure_code_summary_path,
            publish_audits=True,
            report_required_failure_codes=True,
            unexpected_failure_codes=False,
        )
        write_synthetic_packet_set_failure_code_summary(
            missing_audit_summary_path,
            publish_audits=False,
            report_required_failure_codes=False,
            unexpected_failure_codes=False,
        )
        write_synthetic_packet_set_failure_code_summary(
            metadata_drift_summary_path,
            publish_audits=True,
            report_required_failure_codes=True,
            unexpected_failure_codes=False,
            metadata_coherent=False,
        )
        write_synthetic_packet_set_failure_code_summary(
            unexpected_failure_summary_path,
            publish_audits=True,
            report_required_failure_codes=True,
            unexpected_failure_codes=True,
        )

        passing_summary = summarize_phase0_packet_set_qualification(
            qualification_summary_path=qualification_summary_path,
            packet_set_comparison_summary_path=comparison_summary_path,
            packet_set_correct_digit_summary_path=correct_digit_summary_path,
            packet_set_failure_code_summary_path=passing_failure_code_summary_path,
        )
        write_json(summary_path, passing_summary)

        missing_audit_summary = summarize_phase0_packet_set_qualification(
            qualification_summary_path=qualification_summary_path,
            packet_set_comparison_summary_path=comparison_summary_path,
            packet_set_correct_digit_summary_path=correct_digit_summary_path,
            packet_set_failure_code_summary_path=missing_audit_summary_path,
        )

        failing_correct_digit_summary = summarize_phase0_packet_set_qualification(
            qualification_summary_path=qualification_summary_path,
            packet_set_comparison_summary_path=comparison_summary_path,
            packet_set_correct_digit_summary_path=failing_correct_digit_summary_path,
            packet_set_failure_code_summary_path=passing_failure_code_summary_path,
        )

        metadata_drift_summary = summarize_phase0_packet_set_qualification(
            qualification_summary_path=qualification_summary_path,
            packet_set_comparison_summary_path=comparison_summary_path,
            packet_set_correct_digit_summary_path=correct_digit_summary_path,
            packet_set_failure_code_summary_path=metadata_drift_summary_path,
        )

        unexpected_failure_summary = summarize_phase0_packet_set_qualification(
            qualification_summary_path=qualification_summary_path,
            packet_set_comparison_summary_path=comparison_summary_path,
            packet_set_correct_digit_summary_path=correct_digit_summary_path,
            packet_set_failure_code_summary_path=unexpected_failure_summary_path,
        )

        drift_payload = load_json(comparison_summary_path)
        drift_payload["compared_phase0_ids"] = drift_payload["compared_phase0_ids"] + [
            "automatic_phasespace"
        ]
        write_json(drift_comparison_summary_path, drift_payload)

        packet_label_drift_payload = load_json(passing_failure_code_summary_path)
        packet_label_drift_payload["candidate_packet_labels"] = [
            "de-d0-pair",
            "required-set",
            "user-hook-other",
        ]
        write_json(drift_failure_code_summary_path, packet_label_drift_payload)

        phase0_id_drift_rejected = False
        try:
            summarize_phase0_packet_set_qualification(
                qualification_summary_path=qualification_summary_path,
                packet_set_comparison_summary_path=drift_comparison_summary_path,
                packet_set_correct_digit_summary_path=correct_digit_summary_path,
                packet_set_failure_code_summary_path=passing_failure_code_summary_path,
            )
        except RuntimeError as error:
            phase0_id_drift_rejected = (
                "phase-0 ids in the packet-set comparison summary" in str(error)
            )

        packet_label_drift_rejected = False
        try:
            summarize_phase0_packet_set_qualification(
                qualification_summary_path=qualification_summary_path,
                packet_set_comparison_summary_path=comparison_summary_path,
                packet_set_correct_digit_summary_path=correct_digit_summary_path,
                packet_set_failure_code_summary_path=drift_failure_code_summary_path,
            )
        except RuntimeError as error:
            packet_label_drift_rejected = "packet labels in the packet-set failure-code summary" in str(
                error
            )

        return {
            "matching_phase0_packet_set_qualified": passing_summary["phase0_packet_set_qualified"],
            "milestone_m6_still_withheld_without_case_studies": (
                passing_summary["phase0_packet_set_qualified"]
                and not passing_summary["milestone_m6_ready"]
                and passing_summary["milestone_m6_requires_case_study_numerics"]
            ),
            "missing_failure_code_audits_block_phase0_qualification": (
                missing_audit_summary["current_state"] == "blocked-on-failure-code-audit"
                and not missing_audit_summary["phase0_packet_set_qualified"]
            ),
            "correct_digit_thresholds_block_phase0_qualification": (
                failing_correct_digit_summary["current_state"]
                == "blocked-on-correct-digit-thresholds"
                and not failing_correct_digit_summary["phase0_packet_set_qualified"]
            ),
            "failure_code_metadata_blocks_phase0_qualification": (
                metadata_drift_summary["current_state"] == "blocked-on-failure-code-metadata"
                and not metadata_drift_summary["phase0_packet_set_qualified"]
            ),
            "unexpected_failure_codes_block_phase0_qualification": (
                unexpected_failure_summary["current_state"] == "blocked-on-unexpected-failure-codes"
                and not unexpected_failure_summary["phase0_packet_set_qualified"]
            ),
            "phase0_id_drift_rejected": phase0_id_drift_rejected,
            "packet_label_drift_rejected": packet_label_drift_rejected,
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
        "--packet-set-comparison-summary",
        type=Path,
        help="Path to the machine-readable summary emitted by compare_phase0_packet_set_to_reference.py",
    )
    parser.add_argument(
        "--packet-set-correct-digit-summary",
        type=Path,
        help="Path to the machine-readable summary emitted by score_phase0_packet_set_correct_digits.py",
    )
    parser.add_argument(
        "--packet-set-failure-code-summary",
        type=Path,
        help="Path to the machine-readable summary emitted by audit_phase0_packet_set_failure_codes.py",
    )
    parser.add_argument(
        "--summary-path",
        type=Path,
        help="Optional output file for the aggregated phase-0 qualification summary",
    )
    parser.add_argument(
        "--self-check",
        action="store_true",
        help="Run a synthetic phase-0 qualification aggregation self-check",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.self_check:
        print(json.dumps(run_self_check(), indent=2, sort_keys=True))
        return 0

    expect(
        args.qualification_summary is not None,
        "--qualification-summary is required unless --self-check is used",
    )
    expect(
        args.packet_set_comparison_summary is not None,
        "--packet-set-comparison-summary is required unless --self-check is used",
    )
    expect(
        args.packet_set_correct_digit_summary is not None,
        "--packet-set-correct-digit-summary is required unless --self-check is used",
    )
    expect(
        args.packet_set_failure_code_summary is not None,
        "--packet-set-failure-code-summary is required unless --self-check is used",
    )

    summary = summarize_phase0_packet_set_qualification(
        qualification_summary_path=args.qualification_summary,
        packet_set_comparison_summary_path=args.packet_set_comparison_summary,
        packet_set_correct_digit_summary_path=args.packet_set_correct_digit_summary,
        packet_set_failure_code_summary_path=args.packet_set_failure_code_summary,
    )
    if args.summary_path is not None:
        write_json(args.summary_path, summary)
    print(json.dumps(summary, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
