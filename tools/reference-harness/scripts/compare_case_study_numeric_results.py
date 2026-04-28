#!/usr/bin/env python3
"""Build an M6 case-study numeric comparison summary without running numerics."""

from __future__ import annotations

import argparse
import json
import tempfile
from pathlib import Path
from typing import Any

from freeze_phase0_goldens import load_json
from qualify_case_study_families import (
    expect,
    load_case_study_readiness_summary,
    summarize_case_study_qualification,
    write_json,
    write_synthetic_readiness_summary,
)


def normalize_bool(raw: Any, label: str) -> bool:
    if not isinstance(raw, bool):
        raise TypeError(f"{label} must be a bool")
    return raw


def normalize_positive_int(raw: Any, label: str) -> int:
    if not isinstance(raw, int):
        raise TypeError(f"{label} must be an integer")
    expect(raw >= 0, f"{label} must be nonnegative")
    return raw


def normalize_nonempty_string(raw: Any, label: str) -> str:
    if not isinstance(raw, str):
        raise TypeError(f"{label} must be a string")
    value = raw.strip()
    expect(value, f"{label} must not be empty")
    return value


def load_case_study_numeric_evidence(evidence_path: Path) -> dict[str, Any]:
    evidence = load_json(evidence_path)
    expect(
        evidence.get("schema_version") == 1,
        f"case-study numeric evidence {evidence_path} schema_version must be 1",
    )
    return {
        "path": str(evidence_path),
        "case_study_id": normalize_nonempty_string(
            evidence.get("case_study_id"), f"{evidence_path} case_study_id"
        ),
        "comparison_passed": normalize_bool(
            evidence.get("comparison_passed"), f"{evidence_path} comparison_passed"
        ),
        "minimum_observed_correct_digits": normalize_positive_int(
            evidence.get("minimum_observed_correct_digits"),
            f"{evidence_path} minimum_observed_correct_digits",
        ),
        "digit_threshold_profile": normalize_nonempty_string(
            evidence.get("digit_threshold_profile"), f"{evidence_path} digit_threshold_profile"
        ),
        "failure_code_profile": normalize_nonempty_string(
            evidence.get("failure_code_profile"), f"{evidence_path} failure_code_profile"
        ),
        "regression_profile": normalize_nonempty_string(
            evidence.get("regression_profile"), f"{evidence_path} regression_profile"
        ),
    }


def summarize_case_study_numeric_comparison(
    *,
    case_study_readiness_summary_path: Path,
    numeric_evidence_paths: list[Path],
) -> dict[str, Any]:
    readiness = load_case_study_readiness_summary(case_study_readiness_summary_path)
    families = readiness["case_study_families"]
    family_ids = [family["id"] for family in families]
    family_by_id = {family["id"]: family for family in families}

    evidence_by_id: dict[str, dict[str, Any]] = {}
    for evidence_path in numeric_evidence_paths:
        evidence = load_case_study_numeric_evidence(evidence_path)
        family_id = evidence["case_study_id"]
        expect(
            family_id in family_by_id,
            f"case-study numeric evidence {evidence_path} has unknown case_study_id {family_id}",
        )
        expect(
            family_id not in evidence_by_id,
            f"duplicate case-study numeric evidence for {family_id}",
        )
        evidence_by_id[family_id] = evidence

    missing_case_study_numeric_ids: list[str] = []
    minimum_digits: dict[str, int] = {}
    family_summaries: list[dict[str, Any]] = []
    blocking_reasons: list[str] = []
    all_comparisons_pass = True
    all_digit_thresholds_pass = True
    digit_threshold_profiles_reported = True
    required_failure_code_profiles_reported = True
    regression_profiles_reported = True

    for family in families:
        family_id = family["id"]
        evidence = evidence_by_id.get(family_id)
        if evidence is None:
            missing_case_study_numeric_ids.append(family_id)
            minimum_digits[family_id] = 0
            all_comparisons_pass = False
            all_digit_thresholds_pass = False
            blocking_reasons.append(f"case-study numeric evidence missing for {family_id}")
            family_summaries.append(
                {
                    "id": family_id,
                    "status": "missing-numeric-evidence",
                    "minimum_observed_correct_digits": 0,
                    "required_minimum_correct_digits": family["minimum_correct_digits"],
                    "digit_threshold_met": False,
                    "comparison_passed": False,
                    "metadata_matches_readiness": False,
                }
            )
            continue

        digit_profile_matches = (
            evidence["digit_threshold_profile"] == family["digit_threshold_profile"]
        )
        failure_profile_matches = evidence["failure_code_profile"] == family["failure_code_profile"]
        regression_profile_matches = evidence["regression_profile"] == family["regression_profile"]
        metadata_matches = (
            digit_profile_matches and failure_profile_matches and regression_profile_matches
        )
        digit_threshold_profiles_reported = (
            digit_threshold_profiles_reported and digit_profile_matches
        )
        required_failure_code_profiles_reported = (
            required_failure_code_profiles_reported and failure_profile_matches
        )
        regression_profiles_reported = regression_profiles_reported and regression_profile_matches

        observed_digits = evidence["minimum_observed_correct_digits"]
        threshold_met = observed_digits >= family["minimum_correct_digits"]
        minimum_digits[family_id] = observed_digits
        all_comparisons_pass = all_comparisons_pass and evidence["comparison_passed"]
        all_digit_thresholds_pass = all_digit_thresholds_pass and threshold_met

        if not evidence["comparison_passed"]:
            blocking_reasons.append(f"case-study numeric comparison failed for {family_id}")
        if not threshold_met:
            blocking_reasons.append(
                "case-study numeric evidence for "
                f"{family_id} observed {observed_digits} correct digits below required "
                f"{family['minimum_correct_digits']}"
            )
        if not metadata_matches:
            blocking_reasons.append(
                f"case-study numeric evidence for {family_id} does not preserve scaffold metadata"
            )

        family_summaries.append(
            {
                "id": family_id,
                "status": "digit-threshold-met" if threshold_met else "digit-threshold-failed",
                "minimum_observed_correct_digits": observed_digits,
                "required_minimum_correct_digits": family["minimum_correct_digits"],
                "digit_threshold_met": threshold_met,
                "comparison_passed": evidence["comparison_passed"],
                "metadata_matches_readiness": metadata_matches,
                "evidence_path": evidence["path"],
            }
        )

    metadata_coherent = (
        digit_threshold_profiles_reported
        and required_failure_code_profiles_reported
        and regression_profiles_reported
    )
    case_study_numeric_comparison_passed = (
        bool(numeric_evidence_paths)
        and not missing_case_study_numeric_ids
        and all_comparisons_pass
    )
    all_case_studies_meet_digit_thresholds = (
        bool(numeric_evidence_paths)
        and not missing_case_study_numeric_ids
        and all_digit_thresholds_pass
    )

    if not metadata_coherent:
        all_case_studies_meet_digit_thresholds = False
        case_study_numeric_comparison_passed = False

    return {
        "schema_version": 1,
        "scope": "case-study-numerics",
        "case_study_readiness_summary_path": str(case_study_readiness_summary_path),
        "numeric_evidence_paths": [str(path) for path in numeric_evidence_paths],
        "compared_case_study_ids": family_ids,
        "case_study_numeric_evidence_count": len(numeric_evidence_paths),
        "case_study_numeric_comparison_passed": case_study_numeric_comparison_passed,
        "all_case_studies_meet_digit_thresholds": all_case_studies_meet_digit_thresholds,
        "minimum_observed_correct_digits_by_case_study": minimum_digits,
        "missing_case_study_numeric_ids": missing_case_study_numeric_ids,
        "digit_threshold_profiles_reported": digit_threshold_profiles_reported,
        "required_failure_code_profiles_reported": required_failure_code_profiles_reported,
        "regression_profiles_reported": regression_profiles_reported,
        "case_study_numeric_family_summaries": family_summaries,
        "blocking_reasons": blocking_reasons,
        "withheld_claims": [
            "This summary does not launch the C++ runtime or produce case-study numerics.",
            "This summary does not compare phase-0 packet-set evidence.",
            "This summary does not by itself claim Milestone M6 closure.",
            "This summary does not mark Milestone M7 or release readiness.",
        ],
    }


def write_synthetic_numeric_evidence(
    path: Path,
    *,
    family_id: str,
    digit_threshold_profile: str,
    failure_code_profile: str,
    regression_profile: str,
    comparison_passed: bool = True,
    minimum_observed_correct_digits: int = 100,
) -> None:
    write_json(
        path,
        {
            "schema_version": 1,
            "case_study_id": family_id,
            "comparison_passed": comparison_passed,
            "minimum_observed_correct_digits": minimum_observed_correct_digits,
            "digit_threshold_profile": digit_threshold_profile,
            "failure_code_profile": failure_code_profile,
            "regression_profile": regression_profile,
        },
    )


def run_self_check() -> dict[str, Any]:
    with tempfile.TemporaryDirectory(prefix="amflow-case-study-numeric-self-check-") as tmp:
        temp_root = Path(tmp)
        readiness_path = temp_root / "case-study-readiness.json"
        write_synthetic_readiness_summary(readiness_path, runtime_blocked=False)

        matching_evidence_paths = [
            temp_root / "ttbar-h-numeric.json",
            temp_root / "package-double-box-numeric.json",
        ]
        write_synthetic_numeric_evidence(
            matching_evidence_paths[0],
            family_id="ttbar-h",
            digit_threshold_profile="2024-tth-light-quark-loop-mi",
            failure_code_profile="default-required-failure-codes",
            regression_profile="current-reviewed-regressions",
            minimum_observed_correct_digits=125,
        )
        write_synthetic_numeric_evidence(
            matching_evidence_paths[1],
            family_id="package-double-box",
            digit_threshold_profile="core-package-family-default",
            failure_code_profile="default-required-failure-codes",
            regression_profile="current-reviewed-regressions",
            minimum_observed_correct_digits=70,
        )
        matching_summary = summarize_case_study_numeric_comparison(
            case_study_readiness_summary_path=readiness_path,
            numeric_evidence_paths=matching_evidence_paths,
        )
        summary_path = temp_root / "case-study-numerics.json"
        write_json(summary_path, matching_summary)
        qualified_summary = summarize_case_study_qualification(
            case_study_readiness_summary_path=readiness_path,
            case_study_numeric_summary_path=summary_path,
        )

        missing_summary = summarize_case_study_numeric_comparison(
            case_study_readiness_summary_path=readiness_path,
            numeric_evidence_paths=[matching_evidence_paths[0]],
        )

        failing_digits_path = temp_root / "package-double-box-low-digits.json"
        write_synthetic_numeric_evidence(
            failing_digits_path,
            family_id="package-double-box",
            digit_threshold_profile="core-package-family-default",
            failure_code_profile="default-required-failure-codes",
            regression_profile="current-reviewed-regressions",
            minimum_observed_correct_digits=12,
        )
        failing_digits_summary = summarize_case_study_numeric_comparison(
            case_study_readiness_summary_path=readiness_path,
            numeric_evidence_paths=[matching_evidence_paths[0], failing_digits_path],
        )

        drift_path = temp_root / "ttbar-h-drift.json"
        write_synthetic_numeric_evidence(
            drift_path,
            family_id="ttbar-h",
            digit_threshold_profile="core-package-family-default",
            failure_code_profile="default-required-failure-codes",
            regression_profile="current-reviewed-regressions",
            minimum_observed_correct_digits=125,
        )
        metadata_drift_summary = summarize_case_study_numeric_comparison(
            case_study_readiness_summary_path=readiness_path,
            numeric_evidence_paths=[drift_path, matching_evidence_paths[1]],
        )

        duplicate_evidence_rejected = False
        try:
            summarize_case_study_numeric_comparison(
                case_study_readiness_summary_path=readiness_path,
                numeric_evidence_paths=[matching_evidence_paths[0], matching_evidence_paths[0]],
            )
        except RuntimeError as error:
            duplicate_evidence_rejected = "duplicate case-study numeric evidence" in str(error)

        unknown_family_rejected = False
        try:
            unknown_path = temp_root / "unknown-family.json"
            write_synthetic_numeric_evidence(
                unknown_path,
                family_id="unknown-family",
                digit_threshold_profile="core-package-family-default",
                failure_code_profile="default-required-failure-codes",
                regression_profile="current-reviewed-regressions",
            )
            summarize_case_study_numeric_comparison(
                case_study_readiness_summary_path=readiness_path,
                numeric_evidence_paths=[unknown_path],
            )
        except RuntimeError as error:
            unknown_family_rejected = "unknown case_study_id" in str(error)

        return {
            "matching_case_study_numeric_summary_passes": (
                matching_summary["case_study_numeric_comparison_passed"]
                and matching_summary["all_case_studies_meet_digit_thresholds"]
                and not matching_summary["missing_case_study_numeric_ids"]
            ),
            "case_study_numeric_summary_feeds_case_study_qualification": qualified_summary[
                "case_study_families_qualified"
            ],
            "missing_evidence_blocks_numeric_summary": (
                "package-double-box" in missing_summary["missing_case_study_numeric_ids"]
                and not missing_summary["case_study_numeric_comparison_passed"]
            ),
            "threshold_failures_reported": (
                not failing_digits_summary["all_case_studies_meet_digit_thresholds"]
                and failing_digits_summary["minimum_observed_correct_digits_by_case_study"][
                    "package-double-box"
                ]
                == 12
            ),
            "metadata_drift_reported": (
                not metadata_drift_summary["digit_threshold_profiles_reported"]
                and not metadata_drift_summary["case_study_numeric_comparison_passed"]
            ),
            "duplicate_evidence_rejected": duplicate_evidence_rejected,
            "unknown_family_rejected": unknown_family_rejected,
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
        "--numeric-evidence",
        type=Path,
        action="append",
        default=[],
        help="Case-study numeric evidence sidecar to include; may be repeated",
    )
    parser.add_argument(
        "--summary-path",
        type=Path,
        help="Optional output file for the case-study numeric comparison summary",
    )
    parser.add_argument(
        "--self-check",
        action="store_true",
        help="Run a synthetic case-study numeric comparison check",
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
    summary = summarize_case_study_numeric_comparison(
        case_study_readiness_summary_path=args.case_study_readiness_summary,
        numeric_evidence_paths=args.numeric_evidence,
    )
    if args.summary_path is not None:
        write_json(args.summary_path, summary)
    print(json.dumps(summary, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
