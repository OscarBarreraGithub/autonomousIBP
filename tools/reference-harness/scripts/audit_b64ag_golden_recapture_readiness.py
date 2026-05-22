#!/usr/bin/env python3
"""Fail-closed b64ag golden-recapture readiness audit."""

from __future__ import annotations

import argparse
import copy
import json
import re
import sys
import tempfile
from pathlib import Path
from typing import Any


EXPECTED_DE_BASIS = [
    "gauge[1,1,1,0,1,0,0,0,0]",
    "gauge[1,1,1,-1,1,0,0,0,0]",
    "gauge[0,1,1,1,1,0,0,0,0]",
    "gauge[0,1,1,1,1,-1,0,0,0]",
    "gauge[1,1,1,1,1,0,0,0,0]",
    "gauge[1,1,1,1,1,-1,0,0,0]",
]

EXPECTED_PACKET_TARGETS = [
    "gauge[0,1,1,1,1,-1,0,0,0]",
    "gauge[0,1,1,1,1,0,0,0,0]",
    "gauge[1,1,1,-1,1,0,0,0,0]",
    "gauge[1,1,1,0,1,0,0,0,0]",
    "gauge[1,1,1,1,1,-1,0,0,0]",
    "gauge[1,1,1,1,1,0,-1,0,0]",
    "gauge[1,1,1,1,1,0,0,-1,0]",
    "gauge[1,1,1,1,1,0,0,0,-1]",
    "gauge[1,1,1,1,1,0,0,0,0]",
]

EXPECTED_ORDERS = {
    "gauge[0,1,1,1,1,-1,0,0,0]": [0, 1, 2, 3, 4],
    "gauge[0,1,1,1,1,0,0,0,0]": [0, 1, 2, 3, 4],
    "gauge[1,1,1,-1,1,0,0,0,0]": [-1, 0, 1, 2, 3, 4],
    "gauge[1,1,1,0,1,0,0,0,0]": [-1, 0, 1, 2, 3, 4],
    "gauge[1,1,1,1,1,-1,0,0,0]": [-2, -1, 0, 1, 2, 3, 4],
    "gauge[1,1,1,1,1,0,-1,0,0]": [-2, -1, 0, 1, 2, 3, 4],
    "gauge[1,1,1,1,1,0,0,-1,0]": [-2, -1, 0, 1, 2, 3, 4],
    "gauge[1,1,1,1,1,0,0,0,-1]": [-2, -1, 0, 1, 2, 3, 4],
    "gauge[1,1,1,1,1,0,0,0,0]": [-2, -1, 0, 1, 2, 3, 4],
}

EXPECTED_AMFLOW_GOLDEN = Path(
    "tools/reference-harness/specs/phase0/linear_propagator.golden-manifest.json"
)
REQUIRED_COEFFICIENT_ROWS = sum(len(orders) for orders in EXPECTED_ORDERS.values())
REQUIRED_DIGITS = 50
REQUIRED_EPSILON_SAMPLES = 31

BAD_TEXT = tuple(
    re.compile(pattern, re.IGNORECASE)
    for pattern in (
        r"selected[-_ ]?endpoint",
        r"\bselected\b",
        r"\bscaffold\b",
        r"retained[-_ ]?cache",
        r"solution[-_ ]?samples?",
        r"\bdeferred\b",
        r"\bblocked\b",
    )
)

UNKNOWN_FAILURE_CODE = "b64ag_unknown_readiness_failure"

FAILURE_CODE_BY_CHECK = {
    "runtime_lane_is_b64ag": "b64ag_runtime_packet_contract_mismatch",
    "benchmark_is_linear_propagator": "b64ag_runtime_packet_contract_mismatch",
    "runtime_provenance_rejects_final_solution_samples": "b64ag_provenance_contract_mismatch",
    "continuation_present": "b64ag_runtime_scope_not_full_contour",
    "continuation_variable_gaugex": "b64ag_runtime_scope_not_full_contour",
    "continuation_start_one_fortieth": "b64ag_runtime_scope_not_full_contour",
    "continuation_target_zero": "b64ag_runtime_scope_not_full_contour",
    "continuation_singular_zero": "b64ag_runtime_scope_not_full_contour",
    "full_eta_zero_contour_applied": "b64ag_runtime_scope_not_full_contour",
    "blocked_reason_absent": "b64ag_runtime_scope_not_full_contour",
    "runtime_text_rejects_fake_scope_words": "b64ag_runtime_scope_not_full_contour",
    "runtime_targets_match_packet": "b64ag_runtime_packet_contract_mismatch",
    "runtime_results_match_packet": "b64ag_runtime_packet_contract_mismatch",
    "runtime_results_have_coefficients": "b64ag_digit_evidence_incomplete",
    "state_gauge_link_present": "b64ag_amflow_state_contract_mismatch",
    "state_exact_six_master_basis": "b64ag_amflow_state_contract_mismatch",
    "state_boundary_one_fortieth": "b64ag_amflow_state_contract_mismatch",
    "state_variable_gaugex": "b64ag_amflow_state_contract_mismatch",
    "full_contour_diagnostics_present": "b64ag_full_contour_diagnostics_missing",
    "diagnostic_contour_present": "b64ag_full_contour_diagnostics_missing",
    "diagnostic_poles_present": "b64ag_full_contour_diagnostics_missing",
    "diagnostic_finite_part_extraction_present": "b64ag_full_contour_diagnostics_missing",
    "diagnostic_target_reduction_present": "b64ag_full_contour_diagnostics_missing",
    "diagnostic_precision_present": "b64ag_full_contour_diagnostics_missing",
    "diagnostic_provenance_present": "b64ag_full_contour_diagnostics_missing",
    "contour_fingerprint_present": "b64ag_contour_pole_diagnostics_incomplete",
    "contour_waypoints_present": "b64ag_contour_pole_diagnostics_incomplete",
    "nonzero_poles_present": "b64ag_contour_pole_diagnostics_incomplete",
    "finite_rule_pick_zero_rule_s": "b64ag_finite_part_diagnostics_incomplete",
    "ir_subtraction_applied": "b64ag_finite_part_diagnostics_incomplete",
    "finite_part_order_zero": "b64ag_finite_part_diagnostics_incomplete",
    "dropped_singular_powers_present": "b64ag_finite_part_diagnostics_incomplete",
    "target_reduction_fingerprint_present": "b64ag_finite_part_diagnostics_incomplete",
    "target_reduction_row_count_nine": "b64ag_finite_part_diagnostics_incomplete",
    "working_digits_at_least_50": "b64ag_precision_floor_not_met",
    "epsilon_samples_at_least_31": "b64ag_precision_floor_not_met",
    "diagnostic_final_solution_samples_false": "b64ag_provenance_contract_mismatch",
    "diagnostic_provenance_fingerprints_present": "b64ag_provenance_contract_mismatch",
    "comparison_kind_cpp_vs_amflow": "b64ag_comparison_contract_mismatch",
    "comparison_benchmark_linear_propagator": "b64ag_comparison_contract_mismatch",
    "comparison_passed": "b64ag_comparison_contract_mismatch",
    "comparison_failures_empty": "b64ag_comparison_contract_mismatch",
    "comparison_cpp_result_matches_input": "b64ag_provenance_contract_mismatch",
    "comparison_state_matches_input": "b64ag_provenance_contract_mismatch",
    "comparison_golden_matches_phase0_linear_propagator": "b64ag_provenance_contract_mismatch",
    "comparison_tolerance_at_least_50": "b64ag_precision_floor_not_met",
    "comparison_integrals_match_packet": "b64ag_comparison_packet_mismatch",
    "comparison_matched_integral_count_consistent": "b64ag_comparison_packet_mismatch",
    "comparison_target_counts_match_packet": "b64ag_comparison_packet_mismatch",
    "comparison_target_orders_match_packet": "b64ag_comparison_packet_mismatch",
    "comparison_has_57_rows": "b64ag_comparison_packet_mismatch",
    "comparison_compared_passed_counts_consistent": "b64ag_comparison_packet_mismatch",
    "comparison_all_coefficients_passed": "b64ag_digit_evidence_incomplete",
    "comparison_cpp_amflow_presence_true": "b64ag_digit_evidence_incomplete",
    "comparison_real_imag_digits_present": "b64ag_digit_evidence_incomplete",
    "comparison_not_all_999": "b64ag_digit_evidence_incomplete",
    "comparison_detailed_digits_meet_50": "b64ag_precision_floor_not_met",
}

FAILURE_CODE_PRIORITY = [
    "b64ag_runtime_scope_not_full_contour",
    "b64ag_full_contour_diagnostics_missing",
    "b64ag_contour_pole_diagnostics_incomplete",
    "b64ag_finite_part_diagnostics_incomplete",
    "b64ag_amflow_state_contract_mismatch",
    "b64ag_runtime_packet_contract_mismatch",
    "b64ag_comparison_packet_mismatch",
    "b64ag_digit_evidence_incomplete",
    "b64ag_precision_floor_not_met",
    "b64ag_provenance_contract_mismatch",
    "b64ag_comparison_contract_mismatch",
    UNKNOWN_FAILURE_CODE,
]


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def load_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as stream:
        payload = json.load(stream)
    expect(isinstance(payload, dict), f"{path} must contain a JSON object")
    return payload


def write_json(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def print_json(payload: dict[str, Any]) -> None:
    print(json.dumps(payload, indent=2, sort_keys=True))


def is_int(raw: Any) -> bool:
    return isinstance(raw, int) and not isinstance(raw, bool)


def as_int(raw: Any) -> int | None:
    if is_int(raw):
        return raw
    if isinstance(raw, str):
        try:
            return int(raw.strip())
        except ValueError:
            return None
    return None


def nonempty_string(raw: Any) -> bool:
    return isinstance(raw, str) and raw.strip() and raw.strip().lower() not in {
        "placeholder",
        "todo",
        "tbd",
        "unknown",
        "none",
        "null",
    }


def resolved_path(raw: Any, base: Path) -> Path | None:
    if not isinstance(raw, str) or not raw.strip():
        return None
    path = Path(raw.strip())
    if path.is_absolute():
        return path.resolve(strict=False)
    root_candidate = (repo_root() / path).resolve(strict=False)
    base_candidate = (base / path).resolve(strict=False)
    if root_candidate.exists() or not base_candidate.exists():
        return root_candidate
    return base_candidate


def path_matches(raw: Any, expected: Path, base: Path) -> bool:
    actual = resolved_path(raw, base)
    return actual is not None and actual == expected.resolve(strict=False)


def master_label(raw: Any) -> str:
    expect(isinstance(raw, dict), "AMFlow DE basis entries must be objects")
    family = raw.get("family")
    indices = raw.get("indices")
    expect(isinstance(family, str) and family, "AMFlow DE basis family must be nonempty")
    expect(isinstance(indices, list), "AMFlow DE basis indices must be a list")
    expect(all(is_int(index) for index in indices), "AMFlow DE basis indices must be integers")
    return f"{family}[{','.join(str(index) for index in indices)}]"


def add(checks: dict[str, bool], blockers: list[str], key: str, ok: bool, message: str) -> None:
    checks[key] = ok
    if not ok:
        blockers.append(message)


def classify_failed_check(check_name: str) -> str:
    return FAILURE_CODE_BY_CHECK.get(check_name, UNKNOWN_FAILURE_CODE)


def ordered_failure_codes(failed_checks_by_code: dict[str, list[str]]) -> list[str]:
    ordered = [code for code in FAILURE_CODE_PRIORITY if code in failed_checks_by_code]
    seen = set(ordered)
    ordered.extend(sorted(code for code in failed_checks_by_code if code not in seen))
    return ordered


def failure_code_postmortem(
    checks: dict[str, bool],
    blockers: list[str],
) -> dict[str, Any]:
    failed_checks_by_code: dict[str, list[str]] = {}
    for check_name, passed in checks.items():
        if passed:
            continue
        failure_code = classify_failed_check(check_name)
        failed_checks_by_code.setdefault(failure_code, []).append(check_name)

    if blockers and not failed_checks_by_code:
        failed_checks_by_code[UNKNOWN_FAILURE_CODE] = ["blocking_reason_without_failed_check"]

    failure_codes = ordered_failure_codes(failed_checks_by_code)
    return {
        "schema_version": 1,
        "profile": "b64ag-golden-recapture-readiness-postmortem-v1",
        "status": "blocked" if failure_codes else "ready",
        "primary_failure_code": failure_codes[0] if failure_codes else "",
        "failure_codes": failure_codes,
        "failed_checks_by_code": failed_checks_by_code,
        "unknown_failed_checks": failed_checks_by_code.get(UNKNOWN_FAILURE_CODE, []),
        "blocking_reason_count": len(blockers),
        "m6_closure_claimed": False,
        "full_eta_zero_contour_applied_claimed_by_helper": False,
    }


def full_contour_diagnostics(cpp_result: dict[str, Any]) -> dict[str, Any] | None:
    candidates: list[Any] = [cpp_result.get("full_contour_diagnostics")]
    diagnostics = cpp_result.get("diagnostics")
    if isinstance(diagnostics, dict):
        candidates.append(diagnostics.get("full_contour"))
    continuation = cpp_result.get("continuation")
    if isinstance(continuation, dict):
        candidates.append(continuation.get("full_contour_diagnostics"))
    return next((candidate for candidate in candidates if isinstance(candidate, dict)), None)


def runtime_text_fields(cpp_result: dict[str, Any]) -> list[tuple[str, str]]:
    fields: list[tuple[str, str]] = []
    for key in ("status", "current_state", "summary"):
        value = cpp_result.get(key)
        if isinstance(value, str):
            fields.append((key, value))
    for parent_key, keys in (
        ("boundary_state", ("kind", "runtime_boundary_provider", "status", "status_text")),
        (
            "continuation",
            (
                "runtime_application",
                "transport_scope",
                "status",
                "status_text",
                "continuation_status",
                "blocked_reason",
            ),
        ),
    ):
        parent = cpp_result.get(parent_key)
        if isinstance(parent, dict):
            for key in keys:
                value = parent.get(key)
                if isinstance(value, str):
                    fields.append((f"{parent_key}.{key}", value))
    return fields


def audit_runtime(cpp_result: dict[str, Any]) -> tuple[dict[str, bool], list[str]]:
    checks: dict[str, bool] = {}
    blockers: list[str] = []
    add(checks, blockers, "runtime_lane_is_b64ag", cpp_result.get("runtime_lane") == "b64ag", "runtime_lane must be b64ag")
    add(checks, blockers, "benchmark_is_linear_propagator", cpp_result.get("benchmark_id") == "linear_propagator", "benchmark_id must be linear_propagator")
    provenance = cpp_result.get("runtime_provenance")
    add(
        checks,
        blockers,
        "runtime_provenance_rejects_final_solution_samples",
        isinstance(provenance, dict) and provenance.get("final_solution_samples_used_as_input") is False,
        "runtime_provenance.final_solution_samples_used_as_input must be false",
    )
    continuation = cpp_result.get("continuation")
    add(checks, blockers, "continuation_present", isinstance(continuation, dict), "continuation object is required")
    if not isinstance(continuation, dict):
        return checks, blockers
    add(checks, blockers, "continuation_variable_gaugex", continuation.get("variable") == "gaugex", "continuation.variable must be gaugex")
    add(checks, blockers, "continuation_start_one_fortieth", continuation.get("start_location") == "gaugex -> 1/40", "continuation.start_location must be gaugex -> 1/40")
    add(checks, blockers, "continuation_target_zero", continuation.get("target_location") == "gaugex=0", "continuation.target_location must be gaugex=0")
    add(checks, blockers, "continuation_singular_zero", continuation.get("singular_points") == ["gaugex=0"], "continuation.singular_points must be [gaugex=0]")
    add(checks, blockers, "full_eta_zero_contour_applied", continuation.get("full_eta_zero_contour_applied") is True, "full_eta_zero_contour_applied must be true on the candidate")
    add(checks, blockers, "blocked_reason_absent", not str(continuation.get("blocked_reason", "")).strip(), "blocked_reason must be absent or empty")
    bad_fields = [name for name, value in runtime_text_fields(cpp_result) if any(pattern.search(value) for pattern in BAD_TEXT)]
    add(checks, blockers, "runtime_text_rejects_fake_scope_words", not bad_fields, "runtime text contains forbidden fake-scope wording: " + ", ".join(bad_fields))
    add(checks, blockers, "runtime_targets_match_packet", cpp_result.get("targets") == EXPECTED_PACKET_TARGETS, "runtime targets must match the retained nine-target packet")
    results = cpp_result.get("results")
    result_integrals = [entry.get("integral") for entry in results if isinstance(entry, dict)] if isinstance(results, list) else []
    add(checks, blockers, "runtime_results_match_packet", result_integrals == EXPECTED_PACKET_TARGETS, "runtime result integrals must match the retained nine-target packet")
    add(
        checks,
        blockers,
        "runtime_results_have_coefficients",
        isinstance(results, list)
        and all(isinstance(entry, dict) and isinstance(entry.get("epsilon_orders"), list) and entry["epsilon_orders"] for entry in results),
        "each runtime result must carry coefficient rows",
    )
    return checks, blockers


def audit_state(state: dict[str, Any]) -> tuple[dict[str, bool], list[str]]:
    checks: dict[str, bool] = {}
    blockers: list[str] = []
    gauge_link = state.get("gauge_link")
    add(checks, blockers, "state_gauge_link_present", isinstance(gauge_link, dict), "AMFlow state gauge_link object is required")
    if not isinstance(gauge_link, dict):
        return checks, blockers
    masters = gauge_link.get("diffeq_masters")
    labels = [master_label(master) for master in masters] if isinstance(masters, list) else []
    add(checks, blockers, "state_exact_six_master_basis", labels == EXPECTED_DE_BASIS, "AMFlow state must publish the exact ordered six-master DE basis")
    add(checks, blockers, "state_boundary_one_fortieth", gauge_link.get("boundary_point") == "gaugex -> 1/40", "AMFlow state boundary_point must be gaugex -> 1/40")
    add(checks, blockers, "state_variable_gaugex", gauge_link.get("diffeq_variables") == ["gaugex"], "AMFlow state diffeq_variables must be [gaugex]")
    return checks, blockers


def audit_diagnostics(cpp_result: dict[str, Any]) -> tuple[dict[str, bool], list[str]]:
    checks: dict[str, bool] = {}
    blockers: list[str] = []
    diag = full_contour_diagnostics(cpp_result)
    add(checks, blockers, "full_contour_diagnostics_present", isinstance(diag, dict), "full-contour diagnostics are required")
    if not isinstance(diag, dict):
        return checks, blockers
    for bucket in ("contour", "poles", "finite_part_extraction", "target_reduction", "precision", "provenance"):
        add(checks, blockers, f"diagnostic_{bucket}_present", isinstance(diag.get(bucket), dict), f"diagnostic {bucket} bucket is required")
    contour = diag.get("contour") if isinstance(diag.get("contour"), dict) else {}
    poles = diag.get("poles") if isinstance(diag.get("poles"), dict) else {}
    finite = diag.get("finite_part_extraction") if isinstance(diag.get("finite_part_extraction"), dict) else {}
    reduction = diag.get("target_reduction") if isinstance(diag.get("target_reduction"), dict) else {}
    precision = diag.get("precision") if isinstance(diag.get("precision"), dict) else {}
    provenance = diag.get("provenance") if isinstance(diag.get("provenance"), dict) else {}
    add(checks, blockers, "contour_fingerprint_present", nonempty_string(contour.get("fingerprint")), "contour fingerprint is required")
    add(checks, blockers, "contour_waypoints_present", isinstance(contour.get("waypoints"), list) and len(contour["waypoints"]) >= 2, "contour waypoints are required")
    nonzero_poles = poles.get("nonzero_poles")
    add(
        checks,
        blockers,
        "nonzero_poles_present",
        isinstance(nonzero_poles, list)
        and any(nonempty_string(pole) and str(pole).strip().lower() not in {"0", "0.0", "gaugex=0"} for pole in nonzero_poles),
        "nonempty nonzero pole diagnostics are required",
    )
    add(checks, blockers, "finite_rule_pick_zero_rule_s", isinstance(finite.get("rule"), str) and "PickZeroRuleS" in finite["rule"], "finite-part rule must publish PickZeroRuleS")
    add(checks, blockers, "ir_subtraction_applied", finite.get("ir_subtraction_applied") is True, "ir_subtraction_applied must be true")
    add(checks, blockers, "finite_part_order_zero", finite.get("finite_part_order") in (0, "0", "gaugex^0"), "finite-part order must be zero")
    add(checks, blockers, "dropped_singular_powers_present", isinstance(finite.get("dropped_singular_powers"), list) and bool(finite["dropped_singular_powers"]), "dropped singular powers are required")
    add(checks, blockers, "target_reduction_fingerprint_present", nonempty_string(reduction.get("fingerprint")), "target reduction fingerprint is required")
    add(checks, blockers, "target_reduction_row_count_nine", reduction.get("target_row_count") == len(EXPECTED_PACKET_TARGETS), "target reduction row count must be nine")
    add(checks, blockers, "working_digits_at_least_50", (as_int(precision.get("working_digits")) or 0) >= REQUIRED_DIGITS, "working_digits must be at least 50")
    add(checks, blockers, "epsilon_samples_at_least_31", (as_int(precision.get("epsilon_sample_count")) or 0) >= REQUIRED_EPSILON_SAMPLES, "epsilon_sample_count must be at least 31")
    add(checks, blockers, "diagnostic_final_solution_samples_false", provenance.get("final_solution_samples_used_as_input") is False, "diagnostic provenance final_solution_samples_used_as_input must be false")
    add(
        checks,
        blockers,
        "diagnostic_provenance_fingerprints_present",
        all(nonempty_string(provenance.get(key)) for key in ("candidate_result_fingerprint", "amflow_state_fingerprint", "amflow_golden_fingerprint")),
        "diagnostic provenance candidate/state/golden fingerprints are required",
    )
    return checks, blockers


def audit_comparison(
    comparison: dict[str, Any],
    comparison_path: Path,
    cpp_result_path: Path,
    amflow_state_path: Path,
) -> tuple[dict[str, bool], list[str], dict[str, Any]]:
    checks: dict[str, bool] = {}
    blockers: list[str] = []
    details: dict[str, Any] = {}
    add(checks, blockers, "comparison_kind_cpp_vs_amflow", comparison.get("comparison") == "cpp-vs-amflow", "comparison must be cpp-vs-amflow")
    add(checks, blockers, "comparison_benchmark_linear_propagator", comparison.get("benchmark_id") == "linear_propagator", "comparison benchmark must be linear_propagator")
    add(checks, blockers, "comparison_passed", comparison.get("passed") is True, "comparison must pass")
    add(checks, blockers, "comparison_failures_empty", comparison.get("failures") == [], "comparison failures must be empty")
    add(checks, blockers, "comparison_cpp_result_matches_input", path_matches(comparison.get("cpp_result"), cpp_result_path, comparison_path.parent), "comparison cpp_result must match candidate input")
    state_field = next((comparison.get(key) for key in ("amflow_state", "amflow_state_json", "retained_amflow_state") if comparison.get(key) is not None), None)
    add(checks, blockers, "comparison_state_matches_input", path_matches(state_field, amflow_state_path, comparison_path.parent), "comparison AMFlow state must match retained state input")
    add(checks, blockers, "comparison_golden_matches_phase0_linear_propagator", path_matches(comparison.get("amflow_golden"), repo_root() / EXPECTED_AMFLOW_GOLDEN, comparison_path.parent), "comparison golden must be the retained phase0 linear_propagator golden manifest")
    add(checks, blockers, "comparison_tolerance_at_least_50", (as_int(comparison.get("tolerance_digits")) or 0) >= REQUIRED_DIGITS, "comparison tolerance must be at least 50")
    integrals = comparison.get("integrals")
    integral_names = [entry.get("integral") for entry in integrals if isinstance(entry, dict)] if isinstance(integrals, list) else []
    add(checks, blockers, "comparison_integrals_match_packet", integral_names == EXPECTED_PACKET_TARGETS, "comparison integrals must match retained nine-target packet")
    add(checks, blockers, "comparison_matched_integral_count_consistent", comparison.get("matched_integral_count") == len(integral_names), "matched_integral_count must match detailed integrals")

    coefficient_count = 0
    passed_count = 0
    failed_coefficients = 0
    malformed_digits = 0
    missing_presence = 0
    count_match = True
    order_match = True
    digit_values: list[int] = []
    non_sentinel_digits: list[int] = []
    if isinstance(integrals, list):
        for entry in integrals:
            if not isinstance(entry, dict):
                continue
            integral = entry.get("integral")
            coefficients = entry.get("coefficients")
            if not isinstance(integral, str) or not isinstance(coefficients, list) or not coefficients:
                count_match = False
                continue
            expected_orders = EXPECTED_ORDERS.get(integral)
            if expected_orders is None or len(coefficients) != len(expected_orders):
                count_match = False
            observed_orders: list[Any] = []
            for coefficient in coefficients:
                if not isinstance(coefficient, dict):
                    malformed_digits += 1
                    continue
                coefficient_count += 1
                observed_orders.append(coefficient.get("order"))
                if coefficient.get("passed") is True:
                    passed_count += 1
                else:
                    failed_coefficients += 1
                if coefficient.get("cpp_present") is not True or coefficient.get("amflow_present") is not True:
                    missing_presence += 1
                real_digits = as_int(coefficient.get("real_agreement_digits"))
                imag_digits = as_int(coefficient.get("imag_agreement_digits"))
                if real_digits is None or imag_digits is None:
                    malformed_digits += 1
                    continue
                digit_values.extend([real_digits, imag_digits])
                non_sentinel_digits.extend(digit for digit in (real_digits, imag_digits) if digit != 999)
            if observed_orders != expected_orders:
                order_match = False

    detailed_min = min(non_sentinel_digits) if non_sentinel_digits else None
    details.update(
        {
            "comparison_integral_count": len(integral_names),
            "detailed_coefficient_count": coefficient_count,
            "detailed_passed_coefficient_count": passed_count,
            "minimum_detailed_digit_agreement": detailed_min,
            "all_digit_evidence_sentinel_999": bool(digit_values) and not non_sentinel_digits,
        }
    )
    add(checks, blockers, "comparison_target_counts_match_packet", count_match, "per-target coefficient counts must match retained packet")
    add(checks, blockers, "comparison_target_orders_match_packet", order_match, "per-target epsilon orders must match retained packet")
    add(checks, blockers, "comparison_has_57_rows", coefficient_count >= REQUIRED_COEFFICIENT_ROWS, "comparison must publish at least 57 detailed rows")
    add(checks, blockers, "comparison_compared_passed_counts_consistent", comparison.get("compared_coefficient_count") == coefficient_count and comparison.get("passed_coefficient_count") == passed_count, "compared/passed counts must match detailed rows")
    add(checks, blockers, "comparison_all_coefficients_passed", failed_coefficients == 0 and passed_count == coefficient_count and coefficient_count > 0, "every coefficient must pass")
    add(checks, blockers, "comparison_cpp_amflow_presence_true", missing_presence == 0 and coefficient_count > 0, "cpp_present and amflow_present must be true for every coefficient")
    add(checks, blockers, "comparison_real_imag_digits_present", malformed_digits == 0 and coefficient_count > 0, "real and imaginary digit fields must be present on every coefficient")
    add(checks, blockers, "comparison_not_all_999", bool(non_sentinel_digits), "digit evidence must not be all-999 sentinel-only")
    add(checks, blockers, "comparison_detailed_digits_meet_50", detailed_min is not None and detailed_min >= REQUIRED_DIGITS, "detailed digit evidence must meet the 50-digit floor")
    return checks, blockers, details


def audit_b64ag_golden_recapture_readiness(
    cpp_result_path: Path,
    comparison_summary_path: Path,
    amflow_state_path: Path,
) -> dict[str, Any]:
    cpp_result = load_json(cpp_result_path)
    comparison = load_json(comparison_summary_path)
    amflow_state = load_json(amflow_state_path)
    checks: dict[str, bool] = {}
    blockers: list[str] = []
    details: dict[str, Any] = {}
    for section_checks, section_blockers in (
        audit_runtime(cpp_result),
        audit_state(amflow_state),
        audit_diagnostics(cpp_result),
    ):
        checks.update(section_checks)
        blockers.extend(section_blockers)
    comparison_checks, comparison_blockers, comparison_details = audit_comparison(
        comparison,
        comparison_summary_path,
        cpp_result_path,
        amflow_state_path,
    )
    checks.update(comparison_checks)
    blockers.extend(comparison_blockers)
    details.update(comparison_details)
    ready = all(checks.values())
    return {
        "schema_version": 1,
        "audit": "b64ag-golden-recapture-readiness",
        "benchmark_id": "linear_propagator",
        "runtime_lane": "b64ag",
        "status": "ready" if ready else "blocked",
        "golden_recapture_ready": ready,
        "checks": checks,
        "blocking_reasons": blockers,
        "failure_code_postmortem": failure_code_postmortem(checks, blockers),
        "details": details,
        "inputs": {
            "cpp_result": str(cpp_result_path),
            "comparison_summary": str(comparison_summary_path),
            "amflow_state": str(amflow_state_path),
        },
        "m6_closure_claimed": False,
        "full_eta_zero_contour_applied_claimed_by_helper": False,
        "notes": [
            "This helper reports golden-recapture readiness only.",
            "This helper does not promote a phase-0 packet.",
            "This helper does not claim Milestone M6 closure.",
        ],
    }


def synthetic_state() -> dict[str, Any]:
    return {
        "schema_version": 1,
        "benchmark_id": "linear_propagator",
        "kind": "amflow_solve_series_state",
        "gauge_link": {
            "boundary_point": "gaugex -> 1/40",
            "diffeq_variables": ["gaugex"],
            "diffeq_masters": [
                {
                    "family": label.split("[", 1)[0],
                    "indices": [int(item) for item in label.split("[", 1)[1][:-1].split(",")],
                }
                for label in EXPECTED_DE_BASIS
            ],
        },
    }


def synthetic_cpp_result() -> dict[str, Any]:
    return {
        "schema_version": 1,
        "benchmark_id": "linear_propagator",
        "runtime_lane": "b64ag",
        "family": "gauge",
        "status": "success",
        "targets": EXPECTED_PACKET_TARGETS,
        "runtime_provenance": {"final_solution_samples_used_as_input": False},
        "boundary_state": {
            "kind": "amflow_finite_gauge_link_boundary",
            "location": "gaugex -> 1/40",
            "epsilon_sample_count": REQUIRED_EPSILON_SAMPLES,
            "runtime_boundary_provider": "retained-finite-gauge-link-boundary+gaugex-zero-full-contour-transport",
        },
        "continuation": {
            "variable": "gaugex",
            "start_location": "gaugex -> 1/40",
            "target_location": "gaugex=0",
            "singular_points": ["gaugex=0"],
            "transport_applied": True,
            "transport_scope": "gaugex-zero-full-contour-coefficients",
            "full_eta_zero_contour_applied": True,
            "runtime_application": "b64ag-gauge-link-full-contour-coefficients",
            "blocked_reason": "",
        },
        "full_contour_diagnostics": {
            "contour": {
                "fingerprint": "fnv1a64:b64agfull001",
                "waypoints": ["gaugex -> 1/40", "gaugex -> -I/64", "gaugex=0"],
            },
            "poles": {"nonzero_poles": ["gaugex=-1/2", "gaugex=-1/4"]},
            "finite_part_extraction": {
                "rule": "PickZeroRuleS",
                "ir_subtraction_applied": True,
                "finite_part_order": 0,
                "dropped_singular_powers": ["gaugex^-1"],
            },
            "target_reduction": {
                "fingerprint": "fnv1a64:b64agred001",
                "target_row_count": len(EXPECTED_PACKET_TARGETS),
            },
            "precision": {
                "working_digits": 80,
                "epsilon_sample_count": REQUIRED_EPSILON_SAMPLES,
            },
            "provenance": {
                "final_solution_samples_used_as_input": False,
                "candidate_result_fingerprint": "sha256:synthetic-candidate",
                "amflow_state_fingerprint": "sha256:synthetic-state",
                "amflow_golden_fingerprint": "sha256:synthetic-golden",
            },
        },
        "summary": "Applied b64ag gauge-link full contour coefficient transport.",
        "results": [
            {
                "integral": integral,
                "epsilon_orders": [
                    {"order": order, "real_digits": "1.0", "imag_digits": "0.0"}
                    for order in EXPECTED_ORDERS[integral]
                ],
            }
            for integral in EXPECTED_PACKET_TARGETS
        ],
    }


def synthetic_comparison(cpp_result_path: Path, amflow_state_path: Path) -> dict[str, Any]:
    integrals: list[dict[str, Any]] = []
    for integral in EXPECTED_PACKET_TARGETS:
        coefficients = [
            {
                "order": order,
                "cpp_present": True,
                "amflow_present": True,
                "cpp_real": "1.0",
                "cpp_imag": "0.0",
                "amflow_real": "1.0",
                "amflow_imag": "0.0",
                "real_agreement_digits": 60,
                "imag_agreement_digits": 60,
                "passed": True,
            }
            for order in EXPECTED_ORDERS[integral]
        ]
        integrals.append({"integral": integral, "status": "compared", "coefficients": coefficients})
    return {
        "schema_version": 1,
        "comparison": "cpp-vs-amflow",
        "benchmark_id": "linear_propagator",
        "cpp_result": str(cpp_result_path),
        "amflow_state": str(amflow_state_path),
        "amflow_golden": str(EXPECTED_AMFLOW_GOLDEN),
        "tolerance_digits": REQUIRED_DIGITS,
        "matched_integral_count": len(EXPECTED_PACKET_TARGETS),
        "compared_coefficient_count": REQUIRED_COEFFICIENT_ROWS,
        "passed_coefficient_count": REQUIRED_COEFFICIENT_ROWS,
        "minimum_digit_agreement": 60,
        "passed": True,
        "failures": [],
        "integrals": integrals,
    }


def audit_payloads(
    cpp_result: dict[str, Any],
    comparison: dict[str, Any],
    state: dict[str, Any],
    root: Path,
) -> dict[str, Any]:
    cpp_path = root / "candidate.cpp-result.json"
    comparison_path = root / "candidate.compare.json"
    state_path = root / "linear_propagator.amflow-state.json"
    write_json(cpp_path, cpp_result)
    write_json(comparison_path, comparison)
    write_json(state_path, state)
    return audit_b64ag_golden_recapture_readiness(cpp_path, comparison_path, state_path)


def run_self_check() -> dict[str, bool]:
    with tempfile.TemporaryDirectory(prefix="b64ag-golden-recapture-readiness-") as raw_tmp:
        tmp = Path(raw_tmp)
        cpp_path = tmp / "candidate.cpp-result.json"
        state_path = tmp / "linear_propagator.amflow-state.json"
        comparison_path = tmp / "candidate.compare.json"
        write_json(cpp_path, synthetic_cpp_result())
        write_json(state_path, synthetic_state())
        write_json(comparison_path, synthetic_comparison(cpp_path, state_path))
        ready_summary = audit_b64ag_golden_recapture_readiness(cpp_path, comparison_path, state_path)
        base_cpp = load_json(cpp_path)
        base_state = load_json(state_path)
        base_comparison = load_json(comparison_path)

        def rejected(
            label: str,
            cpp_mutation: Any | None = None,
            comparison_mutation: Any | None = None,
            state_mutation: Any | None = None,
        ) -> bool:
            cpp = copy.deepcopy(base_cpp)
            comparison = copy.deepcopy(base_comparison)
            state = copy.deepcopy(base_state)
            if cpp_mutation is not None:
                cpp_mutation(cpp)
            if comparison_mutation is not None:
                comparison_mutation(comparison)
            if state_mutation is not None:
                state_mutation(state)
            return not audit_payloads(cpp, comparison, state, tmp / label)["golden_recapture_ready"]

        def postmortem_primary_code(
            label: str,
            cpp_mutation: Any | None = None,
            comparison_mutation: Any | None = None,
            state_mutation: Any | None = None,
        ) -> str:
            cpp = copy.deepcopy(base_cpp)
            comparison = copy.deepcopy(base_comparison)
            state = copy.deepcopy(base_state)
            if cpp_mutation is not None:
                cpp_mutation(cpp)
            if comparison_mutation is not None:
                comparison_mutation(comparison)
            if state_mutation is not None:
                state_mutation(state)
            postmortem = audit_payloads(cpp, comparison, state, tmp / label)[
                "failure_code_postmortem"
            ]
            return str(postmortem["primary_failure_code"])

        def selected_wording(cpp: dict[str, Any]) -> None:
            cpp["boundary_state"]["runtime_boundary_provider"] = "selected-endpoint-solution-samples"

        def missing_contour(cpp: dict[str, Any]) -> None:
            cpp.pop("full_contour_diagnostics")

        def empty_poles(cpp: dict[str, Any]) -> None:
            cpp["full_contour_diagnostics"]["poles"]["nonzero_poles"] = []

        def bad_runtime_sample(cpp: dict[str, Any]) -> None:
            cpp["runtime_provenance"]["final_solution_samples_used_as_input"] = True

        def bad_diag_sample(cpp: dict[str, Any]) -> None:
            cpp["full_contour_diagnostics"]["provenance"]["final_solution_samples_used_as_input"] = True

        def bad_start(cpp: dict[str, Any]) -> None:
            cpp["continuation"]["start_location"] = "gaugex -> 1/41"

        def bad_lane(cpp: dict[str, Any]) -> None:
            cpp["runtime_lane"] = "b61n"

        def contour_false(cpp: dict[str, Any]) -> None:
            cpp["continuation"]["full_eta_zero_contour_applied"] = False

        def missing_target(cpp: dict[str, Any]) -> None:
            cpp["targets"] = cpp["targets"][:-1]

        def permute_basis(state: dict[str, Any]) -> None:
            basis = state["gauge_link"]["diffeq_masters"]
            basis[0], basis[1] = basis[1], basis[0]

        def bad_state_boundary(state: dict[str, Any]) -> None:
            state["gauge_link"]["boundary_point"] = "gaugex -> 1/41"

        def bad_cpp_binding(comparison: dict[str, Any]) -> None:
            comparison["cpp_result"] = str(tmp / "other.cpp-result.json")

        def bad_state_binding(comparison: dict[str, Any]) -> None:
            comparison["amflow_state"] = str(tmp / "other.amflow-state.json")

        def bad_golden_binding(comparison: dict[str, Any]) -> None:
            comparison["amflow_golden"] = "tools/reference-harness/specs/m6/lane147/linear_propagator.selected4-lightlike.amflow-golden.txt"

        def low_digits(comparison: dict[str, Any]) -> None:
            comparison["minimum_digit_agreement"] = 60
            comparison["integrals"][0]["coefficients"][0]["real_agreement_digits"] = 49

        def missing_imag(comparison: dict[str, Any]) -> None:
            comparison["integrals"][0]["coefficients"][0].pop("imag_agreement_digits")

        def missing_order(comparison: dict[str, Any]) -> None:
            comparison["integrals"][0]["coefficients"][0].pop("order")

        def missing_presence(comparison: dict[str, Any]) -> None:
            comparison["integrals"][0]["coefficients"][0]["cpp_present"] = False

        def bad_integral_count(comparison: dict[str, Any]) -> None:
            comparison["matched_integral_count"] = 0

        def all_999(comparison: dict[str, Any]) -> None:
            for integral in comparison["integrals"]:
                for coefficient in integral["coefficients"]:
                    coefficient["real_agreement_digits"] = 999
                    coefficient["imag_agreement_digits"] = 999

        def sparse(comparison: dict[str, Any]) -> None:
            comparison["integrals"][-1]["coefficients"] = []
            comparison["compared_coefficient_count"] = REQUIRED_COEFFICIENT_ROWS - 7
            comparison["passed_coefficient_count"] = REQUIRED_COEFFICIENT_ROWS - 7

        selected_fixture = audit_b64ag_golden_recapture_readiness(
            repo_root()
            / "tools/reference-harness/specs/m6/lane147/linear_propagator.selected4-lightlike.cpp-result.json",
            repo_root()
            / "tools/reference-harness/specs/m6/lane147/linear_propagator.selected4-lightlike.compare30.json",
            repo_root()
            / "tools/reference-harness/specs/phase0/linear_propagator.amflow-state.json",
        )
        unknown_postmortem = failure_code_postmortem(
            {"future_unmapped_check": False},
            ["synthetic unmapped blocker"],
        )

        summary = {
            "ready_synthetic_passed": ready_summary["golden_recapture_ready"] is True,
            "ready_postmortem_empty": ready_summary["failure_code_postmortem"] == {
                "schema_version": 1,
                "profile": "b64ag-golden-recapture-readiness-postmortem-v1",
                "status": "ready",
                "primary_failure_code": "",
                "failure_codes": [],
                "failed_checks_by_code": {},
                "unknown_failed_checks": [],
                "blocking_reason_count": 0,
                "m6_closure_claimed": False,
                "full_eta_zero_contour_applied_claimed_by_helper": False,
            },
            "selected_lane147_fixture_rejected": selected_fixture["golden_recapture_ready"] is False,
            "selected_lane147_primary_failure_code_reported": (
                selected_fixture["failure_code_postmortem"]["primary_failure_code"]
                == "b64ag_runtime_scope_not_full_contour"
            ),
            "selected_lane147_has_no_unknown_failure_codes": (
                selected_fixture["failure_code_postmortem"]["unknown_failed_checks"] == []
            ),
            "selected_lane147_m6_claims_withheld": (
                selected_fixture["failure_code_postmortem"]["m6_closure_claimed"] is False
                and selected_fixture["failure_code_postmortem"][
                    "full_eta_zero_contour_applied_claimed_by_helper"
                ]
                is False
            ),
            "wrong_runtime_lane_rejected": rejected("bad-lane", cpp_mutation=bad_lane),
            "contour_false_rejected": rejected("contour-false", cpp_mutation=contour_false),
            "selected_solution_sample_wording_rejected": rejected("bad-text", cpp_mutation=selected_wording),
            "wrong_gaugex_start_rejected": rejected("bad-start", cpp_mutation=bad_start),
            "missing_full_contour_rejected": rejected("missing-contour", cpp_mutation=missing_contour),
            "missing_full_contour_primary_failure_code_reported": (
                postmortem_primary_code("missing-contour-code", cpp_mutation=missing_contour)
                == "b64ag_full_contour_diagnostics_missing"
            ),
            "empty_nonzero_poles_rejected": rejected("empty-poles", cpp_mutation=empty_poles),
            "runtime_final_solution_sample_true_rejected": rejected("bad-runtime-sample", cpp_mutation=bad_runtime_sample),
            "diagnostic_final_solution_sample_true_rejected": rejected("bad-diag-sample", cpp_mutation=bad_diag_sample),
            "missing_packet_target_rejected": rejected("missing-target", cpp_mutation=missing_target),
            "permuted_de_basis_rejected": rejected("permute-basis", state_mutation=permute_basis),
            "wrong_state_boundary_rejected": rejected("bad-state-boundary", state_mutation=bad_state_boundary),
            "bad_cpp_provenance_binding_rejected": rejected("bad-cpp-binding", comparison_mutation=bad_cpp_binding),
            "bad_state_provenance_binding_rejected": rejected("bad-state-binding", comparison_mutation=bad_state_binding),
            "bad_golden_provenance_binding_rejected": rejected("bad-golden-binding", comparison_mutation=bad_golden_binding),
            "forged_top_level_digits_rejected": rejected("low-digits", comparison_mutation=low_digits),
            "missing_imag_digit_rejected": rejected("missing-imag", comparison_mutation=missing_imag),
            "missing_order_rejected": rejected("missing-order", comparison_mutation=missing_order),
            "missing_side_presence_rejected": rejected("missing-presence", comparison_mutation=missing_presence),
            "forged_integral_count_rejected": rejected("bad-integral-count", comparison_mutation=bad_integral_count),
            "low_digits_primary_failure_code_reported": (
                postmortem_primary_code("low-digits-code", comparison_mutation=low_digits)
                == "b64ag_precision_floor_not_met"
            ),
            "all_999_sentinel_rejected": rejected("all-999", comparison_mutation=all_999),
            "all_999_primary_failure_code_reported": (
                postmortem_primary_code("all-999-code", comparison_mutation=all_999)
                == "b64ag_digit_evidence_incomplete"
            ),
            "sparse_comparison_rejected": rejected("sparse", comparison_mutation=sparse),
            "unmapped_failure_code_fails_closed": (
                unknown_postmortem["status"] == "blocked"
                and unknown_postmortem["primary_failure_code"] == UNKNOWN_FAILURE_CODE
                and unknown_postmortem["unknown_failed_checks"] == ["future_unmapped_check"]
                and unknown_postmortem["m6_closure_claimed"] is False
            ),
        }
        expect(all(summary.values()), "b64ag golden recapture readiness self-check failed")
        return summary


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--cpp-result", type=Path, help="Candidate C++ result JSON")
    parser.add_argument("--comparison-summary", type=Path, help="C++ vs AMFlow comparison summary")
    parser.add_argument("--amflow-state", type=Path, help="Retained linear_propagator AMFlow state")
    parser.add_argument("--summary-path", type=Path, help="Optional JSON output path")
    parser.add_argument("--self-check", action="store_true", help="Run synthetic self-checks")
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    if args.self_check:
        summary = run_self_check()
        if args.summary_path is not None:
            write_json(args.summary_path, summary)
        print_json(summary)
        return 0
    expect(args.cpp_result is not None, "--cpp-result is required unless --self-check is set")
    expect(args.comparison_summary is not None, "--comparison-summary is required unless --self-check is set")
    expect(args.amflow_state is not None, "--amflow-state is required unless --self-check is set")
    summary = audit_b64ag_golden_recapture_readiness(
        args.cpp_result,
        args.comparison_summary,
        args.amflow_state,
    )
    if args.summary_path is not None:
        write_json(args.summary_path, summary)
    print_json(summary)
    return 0 if summary["golden_recapture_ready"] else 1


if __name__ == "__main__":
    try:
        raise SystemExit(main(sys.argv[1:]))
    except Exception as error:
        print_json(
            {
                "schema_version": 1,
                "audit": "b64ag-golden-recapture-readiness",
                "status": "blocked",
                "golden_recapture_ready": False,
                "blocking_reasons": [str(error)],
                "failure_code_postmortem": failure_code_postmortem(
                    {"audit_exception": False},
                    [str(error)],
                ),
                "m6_closure_claimed": False,
                "full_eta_zero_contour_applied_claimed_by_helper": False,
            }
        )
        raise SystemExit(1)
