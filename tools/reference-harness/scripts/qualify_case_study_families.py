#!/usr/bin/env python3
"""Summarize case-study-family qualification evidence without overclaiming M6 closure."""

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


def normalize_bool(raw: Any, label: str) -> bool:
    if not isinstance(raw, bool):
        raise TypeError(f"{label} must be a bool")
    return raw


def load_case_study_readiness_summary(summary_path: Path) -> dict[str, Any]:
    summary = load_json(summary_path)
    expect(summary.get("schema_version") == 1, "case-study readiness schema_version must be 1")

    required_boolean_fields = [
        "case_study_ids_match_selected_benchmarks",
        "parity_labels_match_parity_matrix",
        "selected_benchmark_refs_match_selected_benchmarks",
        "digit_threshold_profiles_match_verification_strategy",
        "stronger_threshold_assignments_match_selected_benchmark_anchors",
        "failure_code_profile_matches_parity_matrix",
        "regression_profile_matches_parity_matrix",
        "runtime_blocked_case_study_lanes_match_theory_frontier",
        "runtime_lane_predecessors_recorded",
    ]
    normalized_bools = {
        field: normalize_bool(summary.get(field), f"case-study readiness {field}")
        for field in required_boolean_fields
    }

    case_study_ids = normalize_string_list(
        summary.get("case_study_ids", []), "case-study readiness case_study_ids"
    )
    literature_anchor_case_study_ids = normalize_string_list(
        summary.get("literature_anchor_case_study_ids", []),
        "case-study readiness literature_anchor_case_study_ids",
    )
    matrix_only_case_study_ids = normalize_string_list(
        summary.get("matrix_only_case_study_ids", []),
        "case-study readiness matrix_only_case_study_ids",
    )
    runtime_blocked_case_study_ids = normalize_string_list(
        summary.get("runtime_blocked_case_study_ids", []),
        "case-study readiness runtime_blocked_case_study_ids",
    )
    strong_precision_case_study_ids = normalize_string_list(
        summary.get("strong_precision_case_study_ids", []),
        "case-study readiness strong_precision_case_study_ids",
    )
    for label, values in [
        ("case-study readiness case_study_ids", case_study_ids),
        (
            "case-study readiness literature_anchor_case_study_ids",
            literature_anchor_case_study_ids,
        ),
        ("case-study readiness matrix_only_case_study_ids", matrix_only_case_study_ids),
        (
            "case-study readiness runtime_blocked_case_study_ids",
            runtime_blocked_case_study_ids,
        ),
        ("case-study readiness strong_precision_case_study_ids", strong_precision_case_study_ids),
    ]:
        expect_unique(values, label)

    raw_families = summary.get("case_study_families", [])
    if not isinstance(raw_families, list):
        raise TypeError("case-study readiness case_study_families must be a list")
    families: list[dict[str, Any]] = []
    seen_family_ids: set[str] = set()
    for raw in raw_families:
        if not isinstance(raw, dict):
            raise TypeError("case-study readiness family entries must be objects")
        family_id = str(raw.get("id", "")).strip()
        expect(family_id, "case-study readiness family id must not be empty")
        expect(family_id not in seen_family_ids, f"duplicate case-study family id: {family_id}")
        seen_family_ids.add(family_id)
        minimum_correct_digits = raw.get("minimum_correct_digits")
        if not isinstance(minimum_correct_digits, int):
            raise TypeError(
                f"case-study readiness family {family_id} minimum_correct_digits must be an int"
            )
        expect(
            minimum_correct_digits > 0,
            f"case-study readiness family {family_id} minimum_correct_digits must be positive",
        )
        families.append(
            {
                "id": family_id,
                "family_state": str(raw.get("family_state", "")).strip(),
                "parity_matrix_label": str(raw.get("parity_matrix_label", "")).strip(),
                "selected_benchmark_refs": normalize_string_list(
                    raw.get("selected_benchmark_refs", []),
                    f"case-study readiness family {family_id} selected_benchmark_refs",
                ),
                "selected_benchmark_anchor_ref": str(
                    raw.get("selected_benchmark_anchor_ref", "")
                ).strip(),
                "digit_threshold_profile": str(raw.get("digit_threshold_profile", "")).strip(),
                "minimum_correct_digits": minimum_correct_digits,
                "failure_code_profile": str(raw.get("failure_code_profile", "")).strip(),
                "required_failure_codes": normalize_string_list(
                    raw.get("required_failure_codes", []),
                    f"case-study readiness family {family_id} required_failure_codes",
                ),
                "regression_profile": str(raw.get("regression_profile", "")).strip(),
                "known_regression_families": normalize_string_list(
                    raw.get("known_regression_families", []),
                    f"case-study readiness family {family_id} known_regression_families",
                ),
                "next_runtime_lane": str(raw.get("next_runtime_lane", "")).strip(),
                "landed_runtime_predecessor": str(
                    raw.get("landed_runtime_predecessor", "")
                ).strip(),
            }
        )

    expect(set(seen_family_ids) == set(case_study_ids), "case-study family ids must match summary ids")
    return {
        **summary,
        **normalized_bools,
        "case_study_ids": case_study_ids,
        "literature_anchor_case_study_ids": literature_anchor_case_study_ids,
        "matrix_only_case_study_ids": matrix_only_case_study_ids,
        "runtime_blocked_case_study_ids": runtime_blocked_case_study_ids,
        "strong_precision_case_study_ids": strong_precision_case_study_ids,
        "case_study_families": families,
    }


def load_case_study_numeric_summary(summary_path: Path) -> dict[str, Any]:
    summary = load_json(summary_path)
    expect(summary.get("schema_version") == 1, "case-study numeric summary schema_version must be 1")
    expect(
        summary.get("scope") == "case-study-numerics",
        "case-study numeric summary scope must be case-study-numerics",
    )

    required_boolean_fields = [
        "case_study_numeric_comparison_passed",
        "all_case_studies_meet_digit_thresholds",
        "digit_threshold_profiles_reported",
        "required_failure_code_profiles_reported",
        "regression_profiles_reported",
    ]
    normalized_bools = {
        field: normalize_bool(summary.get(field), f"case-study numeric summary {field}")
        for field in required_boolean_fields
    }
    compared_case_study_ids = normalize_string_list(
        summary.get("compared_case_study_ids", []),
        "case-study numeric summary compared_case_study_ids",
    )
    expect_unique(compared_case_study_ids, "case-study numeric summary compared_case_study_ids")
    blocking_reasons = normalize_string_list(
        summary.get("blocking_reasons", []), "case-study numeric summary blocking_reasons"
    )
    missing_case_study_numeric_ids = normalize_string_list(
        summary.get("missing_case_study_numeric_ids", []),
        "case-study numeric summary missing_case_study_numeric_ids",
    )

    minimum_digits_raw = summary.get("minimum_observed_correct_digits_by_case_study", {})
    if not isinstance(minimum_digits_raw, dict):
        raise TypeError(
            "case-study numeric summary minimum_observed_correct_digits_by_case_study must be an object"
        )
    minimum_digits: dict[str, int] = {}
    for family_id, raw_digits in minimum_digits_raw.items():
        if not isinstance(family_id, str) or not family_id.strip():
            raise ValueError("case-study numeric summary digit-map ids must be non-empty strings")
        if not isinstance(raw_digits, int):
            raise TypeError(
                "case-study numeric summary minimum observed digits must be integers"
            )
        minimum_digits[family_id.strip()] = raw_digits
    expect(
        set(minimum_digits) == set(compared_case_study_ids),
        "case-study numeric summary digit-map ids must match compared_case_study_ids",
    )
    expect(
        set(missing_case_study_numeric_ids).issubset(set(compared_case_study_ids)),
        "case-study numeric summary missing ids must be a subset of compared_case_study_ids",
    )

    return {
        **summary,
        **normalized_bools,
        "compared_case_study_ids": compared_case_study_ids,
        "blocking_reasons": blocking_reasons,
        "missing_case_study_numeric_ids": missing_case_study_numeric_ids,
        "minimum_observed_correct_digits_by_case_study": minimum_digits,
    }


def determine_current_state(
    *,
    readiness_contract_coherent: bool,
    runtime_blocked_case_study_ids: list[str],
    case_study_numeric_evidence_present: bool,
    missing_case_study_numeric_ids: list[str],
    case_study_numeric_comparison_passed: bool,
    all_case_studies_meet_digit_thresholds: bool,
    numeric_metadata_coherent: bool,
) -> str:
    if not readiness_contract_coherent:
        return "blocked-on-case-study-readiness"
    if runtime_blocked_case_study_ids:
        return "blocked-on-runtime-lanes"
    if not case_study_numeric_evidence_present:
        return "blocked-on-case-study-numeric-evidence"
    if missing_case_study_numeric_ids:
        return "blocked-on-case-study-numeric-evidence"
    if not numeric_metadata_coherent:
        return "blocked-on-case-study-metadata"
    if not case_study_numeric_comparison_passed:
        return "blocked-on-case-study-comparison"
    if not all_case_studies_meet_digit_thresholds:
        return "blocked-on-case-study-correct-digits"
    return "case-study-families-qualified"


def summarize_case_study_qualification(
    *,
    case_study_readiness_summary_path: Path,
    case_study_numeric_summary_path: Path | None = None,
) -> dict[str, Any]:
    readiness_summary = load_case_study_readiness_summary(case_study_readiness_summary_path)
    readiness_contract_coherent = all(
        readiness_summary[field]
        for field in [
            "case_study_ids_match_selected_benchmarks",
            "parity_labels_match_parity_matrix",
            "selected_benchmark_refs_match_selected_benchmarks",
            "digit_threshold_profiles_match_verification_strategy",
            "stronger_threshold_assignments_match_selected_benchmark_anchors",
            "failure_code_profile_matches_parity_matrix",
            "regression_profile_matches_parity_matrix",
            "runtime_blocked_case_study_lanes_match_theory_frontier",
            "runtime_lane_predecessors_recorded",
        ]
    )

    case_study_numeric_evidence_present = case_study_numeric_summary_path is not None
    compared_case_study_ids: list[str] = []
    missing_case_study_numeric_ids = list(readiness_summary["case_study_ids"])
    minimum_observed_correct_digits_by_case_study: dict[str, int] = {}
    case_study_numeric_comparison_passed = False
    all_case_studies_meet_digit_thresholds = False
    digit_threshold_profiles_reported = False
    required_failure_code_profiles_reported = False
    regression_profiles_reported = False
    numeric_blocking_reasons: list[str] = []

    if case_study_numeric_summary_path is not None:
        numeric_summary = load_case_study_numeric_summary(case_study_numeric_summary_path)
        compared_case_study_ids = numeric_summary["compared_case_study_ids"]
        expect(
            set(compared_case_study_ids) == set(readiness_summary["case_study_ids"]),
            "case-study ids in the numeric summary must match case-study readiness",
        )
        missing_case_study_numeric_ids = numeric_summary["missing_case_study_numeric_ids"]
        minimum_observed_correct_digits_by_case_study = numeric_summary[
            "minimum_observed_correct_digits_by_case_study"
        ]
        case_study_numeric_comparison_passed = numeric_summary[
            "case_study_numeric_comparison_passed"
        ]
        all_case_studies_meet_digit_thresholds = numeric_summary[
            "all_case_studies_meet_digit_thresholds"
        ]
        digit_threshold_profiles_reported = numeric_summary["digit_threshold_profiles_reported"]
        required_failure_code_profiles_reported = numeric_summary[
            "required_failure_code_profiles_reported"
        ]
        regression_profiles_reported = numeric_summary["regression_profiles_reported"]
        numeric_blocking_reasons = numeric_summary["blocking_reasons"]

    numeric_metadata_coherent = (
        (not case_study_numeric_evidence_present)
        or (
            digit_threshold_profiles_reported
            and required_failure_code_profiles_reported
            and regression_profiles_reported
        )
    )
    current_state = determine_current_state(
        readiness_contract_coherent=readiness_contract_coherent,
        runtime_blocked_case_study_ids=readiness_summary["runtime_blocked_case_study_ids"],
        case_study_numeric_evidence_present=case_study_numeric_evidence_present,
        missing_case_study_numeric_ids=missing_case_study_numeric_ids,
        case_study_numeric_comparison_passed=case_study_numeric_comparison_passed,
        all_case_studies_meet_digit_thresholds=all_case_studies_meet_digit_thresholds,
        numeric_metadata_coherent=numeric_metadata_coherent,
    )
    case_study_families_qualified = (
        readiness_contract_coherent
        and not readiness_summary["runtime_blocked_case_study_ids"]
        and case_study_numeric_evidence_present
        and numeric_metadata_coherent
        and case_study_numeric_comparison_passed
        and all_case_studies_meet_digit_thresholds
        and not missing_case_study_numeric_ids
    )

    blocking_reasons: list[str] = []
    if not readiness_contract_coherent:
        blocking_reasons.append("case-study readiness summary is not coherent")
    for family_id in readiness_summary["runtime_blocked_case_study_ids"]:
        blocking_reasons.append(f"case-study family {family_id} is still blocked on a runtime lane")
    if not case_study_numeric_evidence_present:
        blocking_reasons.append("case-study numerics are not yet compared")
    if case_study_numeric_evidence_present and missing_case_study_numeric_ids:
        blocking_reasons.append("case-study numeric summary is missing selected families")
    if case_study_numeric_evidence_present and not numeric_metadata_coherent:
        blocking_reasons.append("case-study numeric summary did not preserve scaffold metadata")
    if case_study_numeric_evidence_present and not case_study_numeric_comparison_passed:
        blocking_reasons.append("case-study numeric comparison has not fully passed")
    if case_study_numeric_evidence_present and not all_case_studies_meet_digit_thresholds:
        blocking_reasons.append("case-study correct-digit thresholds have not fully passed")
    blocking_reasons.extend(numeric_blocking_reasons)

    blocked_case_study_families = [
        {
            "id": family["id"],
            "next_runtime_lane": family["next_runtime_lane"],
            "landed_runtime_predecessor": family["landed_runtime_predecessor"],
        }
        for family in readiness_summary["case_study_families"]
        if family["id"] in readiness_summary["runtime_blocked_case_study_ids"]
    ]

    return {
        "schema_version": 1,
        "scope": "case-study-families-only",
        "current_state": current_state,
        "case_study_readiness_summary_path": str(case_study_readiness_summary_path),
        "case_study_numeric_summary_path": (
            str(case_study_numeric_summary_path)
            if case_study_numeric_summary_path is not None
            else ""
        ),
        "case_study_ids": readiness_summary["case_study_ids"],
        "literature_anchor_case_study_ids": readiness_summary["literature_anchor_case_study_ids"],
        "matrix_only_case_study_ids": readiness_summary["matrix_only_case_study_ids"],
        "runtime_blocked_case_study_ids": readiness_summary["runtime_blocked_case_study_ids"],
        "strong_precision_case_study_ids": readiness_summary["strong_precision_case_study_ids"],
        "readiness_contract_coherent": readiness_contract_coherent,
        "case_study_numeric_evidence_present": case_study_numeric_evidence_present,
        "compared_case_study_ids": compared_case_study_ids,
        "case_study_numeric_comparison_passed": case_study_numeric_comparison_passed,
        "all_case_studies_meet_digit_thresholds": all_case_studies_meet_digit_thresholds,
        "minimum_observed_correct_digits_by_case_study": minimum_observed_correct_digits_by_case_study,
        "missing_case_study_numeric_ids": missing_case_study_numeric_ids,
        "numeric_metadata_coherent": numeric_metadata_coherent,
        "digit_threshold_profiles_reported": digit_threshold_profiles_reported,
        "required_failure_code_profiles_reported": required_failure_code_profiles_reported,
        "regression_profiles_reported": regression_profiles_reported,
        "case_study_families_qualified": case_study_families_qualified,
        "milestone_m6_ready": False,
        "milestone_m6_requires_phase0_verdict": True,
        "blocked_case_study_families": blocked_case_study_families,
        "case_study_families": readiness_summary["case_study_families"],
        "blocking_reasons": blocking_reasons,
        "withheld_claims": [
            "This summary does not launch the C++ runtime or create new retained captures.",
            "This summary does not compare phase-0 packet-set evidence.",
            "This summary does not by itself claim Milestone M6 closure.",
            "This summary does not mark Milestone M7 or release readiness.",
        ],
    }


def write_synthetic_readiness_summary(
    path: Path,
    *,
    runtime_blocked: bool,
    coherent: bool = True,
) -> None:
    runtime_blocked_ids = ["one-singular-endpoint-case"] if runtime_blocked else []
    case_study_families = [
        {
            "id": "ttbar-h",
            "family_state": "literature-anchor-selected",
            "parity_matrix_label": "ttbar H",
            "selected_benchmark_refs": ["2024-tth-light-quark-loop-mi"],
            "selected_benchmark_anchor_ref": "2024-tth-light-quark-loop-mi",
            "digit_threshold_profile": "2024-tth-light-quark-loop-mi",
            "minimum_correct_digits": 100,
            "failure_code_profile": "default-required-failure-codes",
            "required_failure_codes": ["insufficient_precision", "master_set_instability"],
            "regression_profile": "current-reviewed-regressions",
            "known_regression_families": ["unexpected master sets in Kira interface"],
            "next_runtime_lane": "",
            "landed_runtime_predecessor": "",
        },
        {
            "id": "package-double-box",
            "family_state": "matrix-only-anchor",
            "parity_matrix_label": "package double box",
            "selected_benchmark_refs": [],
            "selected_benchmark_anchor_ref": "",
            "digit_threshold_profile": "core-package-family-default",
            "minimum_correct_digits": 50,
            "failure_code_profile": "default-required-failure-codes",
            "required_failure_codes": ["insufficient_precision", "master_set_instability"],
            "regression_profile": "current-reviewed-regressions",
            "known_regression_families": ["unexpected master sets in Kira interface"],
            "next_runtime_lane": "",
            "landed_runtime_predecessor": "",
        },
    ]
    if runtime_blocked:
        case_study_families.append(
            {
                "id": "one-singular-endpoint-case",
                "family_state": "runtime-blocked",
                "parity_matrix_label": "one singular-endpoint case",
                "selected_benchmark_refs": [],
                "selected_benchmark_anchor_ref": "",
                "digit_threshold_profile": "core-package-family-default",
                "minimum_correct_digits": 50,
                "failure_code_profile": "default-required-failure-codes",
                "required_failure_codes": ["insufficient_precision", "master_set_instability"],
                "regression_profile": "current-reviewed-regressions",
                "known_regression_families": ["unexpected master sets in Kira interface"],
                "next_runtime_lane": "b62n",
                "landed_runtime_predecessor": "b62m",
            }
        )

    write_json(
        path,
        {
            "schema_version": 1,
            "case_study_ids": [family["id"] for family in case_study_families],
            "literature_anchor_case_study_ids": ["ttbar-h"],
            "matrix_only_case_study_ids": ["package-double-box"],
            "runtime_blocked_case_study_ids": runtime_blocked_ids,
            "strong_precision_case_study_ids": ["ttbar-h"],
            "case_study_ids_match_selected_benchmarks": coherent,
            "parity_labels_match_parity_matrix": True,
            "selected_benchmark_refs_match_selected_benchmarks": True,
            "digit_threshold_profiles_match_verification_strategy": True,
            "stronger_threshold_assignments_match_selected_benchmark_anchors": True,
            "failure_code_profile_matches_parity_matrix": True,
            "regression_profile_matches_parity_matrix": True,
            "runtime_blocked_case_study_lanes_match_theory_frontier": True,
            "runtime_lane_predecessors_recorded": True,
            "case_study_families": case_study_families,
        },
    )


def write_synthetic_numeric_summary(
    path: Path,
    *,
    case_study_ids: list[str],
    passes_comparison: bool = True,
    meets_digit_thresholds: bool = True,
) -> None:
    write_json(
        path,
        {
            "schema_version": 1,
            "scope": "case-study-numerics",
            "compared_case_study_ids": case_study_ids,
            "case_study_numeric_comparison_passed": passes_comparison,
            "all_case_studies_meet_digit_thresholds": meets_digit_thresholds,
            "minimum_observed_correct_digits_by_case_study": {
                family_id: 100 for family_id in case_study_ids
            },
            "missing_case_study_numeric_ids": [] if passes_comparison else [case_study_ids[0]],
            "digit_threshold_profiles_reported": True,
            "required_failure_code_profiles_reported": True,
            "regression_profiles_reported": True,
            "blocking_reasons": []
            if passes_comparison and meets_digit_thresholds
            else ["synthetic case-study numeric blocker"],
        },
    )


def run_self_check() -> dict[str, Any]:
    with tempfile.TemporaryDirectory(prefix="amflow-case-study-qualification-self-check-") as tmp:
        temp_root = Path(tmp)
        readiness_path = temp_root / "case-study-readiness.json"
        numeric_path = temp_root / "case-study-numerics.json"
        summary_path = temp_root / "case-study-qualification.json"

        write_synthetic_readiness_summary(readiness_path, runtime_blocked=False)
        write_synthetic_numeric_summary(
            numeric_path,
            case_study_ids=["ttbar-h", "package-double-box"],
        )
        matching_summary = summarize_case_study_qualification(
            case_study_readiness_summary_path=readiness_path,
            case_study_numeric_summary_path=numeric_path,
        )
        write_json(summary_path, matching_summary)

        missing_numeric_summary = summarize_case_study_qualification(
            case_study_readiness_summary_path=readiness_path,
        )

        runtime_blocked_readiness_path = temp_root / "case-study-runtime-blocked-readiness.json"
        runtime_blocked_numeric_path = temp_root / "case-study-runtime-blocked-numerics.json"
        write_synthetic_readiness_summary(runtime_blocked_readiness_path, runtime_blocked=True)
        write_synthetic_numeric_summary(
            runtime_blocked_numeric_path,
            case_study_ids=["ttbar-h", "package-double-box", "one-singular-endpoint-case"],
        )
        runtime_blocked_summary = summarize_case_study_qualification(
            case_study_readiness_summary_path=runtime_blocked_readiness_path,
            case_study_numeric_summary_path=runtime_blocked_numeric_path,
        )

        failing_digits_path = temp_root / "case-study-failing-digits.json"
        write_synthetic_numeric_summary(
            failing_digits_path,
            case_study_ids=["ttbar-h", "package-double-box"],
            meets_digit_thresholds=False,
        )
        failing_digits_summary = summarize_case_study_qualification(
            case_study_readiness_summary_path=readiness_path,
            case_study_numeric_summary_path=failing_digits_path,
        )

        incoherent_readiness_path = temp_root / "case-study-incoherent-readiness.json"
        write_synthetic_readiness_summary(
            incoherent_readiness_path,
            runtime_blocked=False,
            coherent=False,
        )
        incoherent_readiness_summary = summarize_case_study_qualification(
            case_study_readiness_summary_path=incoherent_readiness_path,
            case_study_numeric_summary_path=numeric_path,
        )

        case_study_id_drift_rejected = False
        try:
            drift_numeric_path = temp_root / "case-study-drift-numerics.json"
            write_synthetic_numeric_summary(
                drift_numeric_path,
                case_study_ids=["ttbar-h"],
            )
            summarize_case_study_qualification(
                case_study_readiness_summary_path=readiness_path,
                case_study_numeric_summary_path=drift_numeric_path,
            )
        except RuntimeError as error:
            case_study_id_drift_rejected = (
                "case-study ids in the numeric summary must match case-study readiness" in str(error)
            )

        return {
            "matching_case_studies_qualified": matching_summary[
                "case_study_families_qualified"
            ],
            "milestone_m6_still_withheld_without_phase0_verdict": (
                matching_summary["case_study_families_qualified"]
                and not matching_summary["milestone_m6_ready"]
                and matching_summary["milestone_m6_requires_phase0_verdict"]
            ),
            "missing_numeric_evidence_blocks_case_study_qualification": (
                missing_numeric_summary["current_state"]
                == "blocked-on-case-study-numeric-evidence"
            ),
            "runtime_blocked_lanes_block_case_study_qualification": (
                runtime_blocked_summary["current_state"] == "blocked-on-runtime-lanes"
            ),
            "correct_digit_thresholds_block_case_study_qualification": (
                failing_digits_summary["current_state"] == "blocked-on-case-study-correct-digits"
            ),
            "incoherent_readiness_blocks_case_study_qualification": (
                incoherent_readiness_summary["current_state"] == "blocked-on-case-study-readiness"
            ),
            "case_study_id_drift_rejected": case_study_id_drift_rejected,
            "summary_written": summary_path.exists(),
        }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--case-study-readiness-summary",
        type=Path,
        help="Path to the summary emitted by qualification_case_study_readiness.py",
    )
    parser.add_argument(
        "--case-study-numeric-summary",
        type=Path,
        help="Optional future case-study numeric comparison summary",
    )
    parser.add_argument(
        "--summary-path",
        type=Path,
        help="Optional output file for the case-study qualification verdict",
    )
    parser.add_argument(
        "--self-check",
        action="store_true",
        help="Run a synthetic case-study qualification check",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    if args.self_check:
        print(json.dumps(run_self_check(), indent=2, sort_keys=True))
        return

    expect(
        args.case_study_readiness_summary is not None,
        "--case-study-readiness-summary is required unless --self-check is used",
    )
    summary = summarize_case_study_qualification(
        case_study_readiness_summary_path=args.case_study_readiness_summary,
        case_study_numeric_summary_path=args.case_study_numeric_summary,
    )
    if args.summary_path is not None:
        write_json(args.summary_path, summary)
    print(json.dumps(summary, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
