#!/usr/bin/env python3
"""Verify the b61n row 5/6 reference-floor parity gate."""

from __future__ import annotations

import argparse
import copy
import json
import sys
from pathlib import Path
from typing import Any

from compare_cpp_vs_amflow import compare_cpp_vs_amflow


DEFAULT_CPP_RESULT = Path(
    "tools/reference-harness/specs/m7/lane2/"
    "complex_kinematics.c267-stripped.eps0.cpp-result.json"
)
DEFAULT_AMFLOW_GOLDEN = Path(
    "tools/reference-harness/specs/m7/lane2/"
    "complex_kinematics.b61n-reference-floor-golden-manifest.json"
)
DEFAULT_RETAINED_COMPARISON = Path(
    "tools/reference-harness/specs/m7/lane2/"
    "complex_kinematics.c267-stripped.eps0.compare50.reference-floor.json"
)

EXPECTED_TARGETS: dict[tuple[str, int], dict[str, Any]] = {
    ("box[1,0,1,1]", 0): {
        "reference_floor_id": "b61n-row5-eps0-retained-amflow-floor",
        "reference_floor_real_digits": 11,
        "reference_floor_imag_digits": 11,
        "reference_floor_reason": (
            "post-M7 b61n row 5 eps^0 diagnostic: retained AMFlow reference reaches only "
            "11/11 component digits for this target"
        ),
    },
    ("box[1,1,1,1]", -2): {
        "reference_floor_id": "b61n-row6-eps-2-retained-amflow-floor",
        "reference_floor_real_digits": 46,
        "reference_floor_imag_digits": 46,
        "reference_floor_reason": (
            "post-M7 b61n row 6 eps^-2 diagnostic: retained AMFlow reference reaches only "
            "46/46 component digits for this target"
        ),
    },
    ("box[1,1,1,1]", -1): {
        "reference_floor_id": "b61n-row6-eps-1-retained-amflow-floor",
        "reference_floor_real_digits": 12,
        "reference_floor_imag_digits": 13,
        "reference_floor_reason": (
            "post-M7 b61n row 6 eps^-1 diagnostic: retained AMFlow reference reaches only "
            "12/13 component digits for this target"
        ),
    },
    ("box[1,1,1,1]", 0): {
        "reference_floor_id": "b61n-row6-eps0-retained-amflow-floor",
        "reference_floor_real_digits": 12,
        "reference_floor_imag_digits": 12,
        "reference_floor_reason": (
            "post-M7 b61n row 6 eps^0 diagnostic: retained AMFlow reference reaches only "
            "12/12 component digits for this target"
        ),
    },
}

EXPECTED_COMPARED_COEFFICIENTS = 14
EXPECTED_REFERENCE_FLOOR_MATCHES = len(EXPECTED_TARGETS)
EXPECTED_TOLERANCE_PASSES = EXPECTED_COMPARED_COEFFICIENTS - EXPECTED_REFERENCE_FLOOR_MATCHES


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


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


def require_object(raw: Any, label: str) -> dict[str, Any]:
    if not isinstance(raw, dict):
        raise TypeError(f"{label} must be an object")
    return raw


def require_list(raw: Any, label: str) -> list[Any]:
    if not isinstance(raw, list):
        raise TypeError(f"{label} must be a list")
    return raw


def require_int(raw: Any, label: str) -> int:
    if not isinstance(raw, int) or isinstance(raw, bool):
        raise TypeError(f"{label} must be an int")
    return raw


def require_string(raw: Any, label: str) -> str:
    if not isinstance(raw, str) or not raw:
        raise TypeError(f"{label} must be a non-empty string")
    return raw


def require_bool(raw: Any, label: str) -> bool:
    if not isinstance(raw, bool):
        raise TypeError(f"{label} must be a bool")
    return raw


def coefficient_key(integral: str, order: int) -> str:
    return f"{integral} eps^{order}"


def expected_target_contract() -> list[dict[str, Any]]:
    return [
        {
            "integral": integral,
            "order": order,
            "reference_floor_id": EXPECTED_TARGETS[(integral, order)]["reference_floor_id"],
            "reference_floor_real_digits": EXPECTED_TARGETS[(integral, order)][
                "reference_floor_real_digits"
            ],
            "reference_floor_imag_digits": EXPECTED_TARGETS[(integral, order)][
                "reference_floor_imag_digits"
            ],
        }
        for integral, order in sorted(EXPECTED_TARGETS)
    ]


def index_coefficients(summary: dict[str, Any], label: str) -> dict[tuple[str, int], dict[str, Any]]:
    indexed: dict[tuple[str, int], dict[str, Any]] = {}
    for integral_index, raw_integral in enumerate(require_list(summary.get("integrals"), f"{label}.integrals")):
        integral = require_object(raw_integral, f"{label}.integrals[{integral_index}]")
        integral_name = require_string(
            integral.get("integral"),
            f"{label}.integrals[{integral_index}].integral",
        )
        for coefficient_index, raw_coefficient in enumerate(
            require_list(
                integral.get("coefficients"),
                f"{label}.integrals[{integral_index}].coefficients",
            )
        ):
            coefficient = require_object(
                raw_coefficient,
                f"{label}.integrals[{integral_index}].coefficients[{coefficient_index}]",
            )
            order = require_int(
                coefficient.get("order"),
                f"{label}.{integral_name}.coefficients[{coefficient_index}].order",
            )
            key = (integral_name, order)
            expect(key not in indexed, f"{label} repeats {coefficient_key(*key)}")
            indexed[key] = coefficient
    return indexed


def index_reference_floor_matches(
    summary: dict[str, Any],
    label: str,
) -> dict[tuple[str, int], dict[str, Any]]:
    indexed: dict[tuple[str, int], dict[str, Any]] = {}
    for match_index, raw_match in enumerate(
        require_list(summary.get("reference_floor_matches"), f"{label}.reference_floor_matches")
    ):
        match = require_object(raw_match, f"{label}.reference_floor_matches[{match_index}]")
        integral = require_string(
            match.get("integral"),
            f"{label}.reference_floor_matches[{match_index}].integral",
        )
        order = require_int(
            match.get("order"),
            f"{label}.reference_floor_matches[{match_index}].order",
        )
        key = (integral, order)
        expect(key not in indexed, f"{label} repeats reference-floor match {coefficient_key(*key)}")
        indexed[key] = match
    return indexed


def validate_reference_floor_metadata(
    record: dict[str, Any],
    expected: dict[str, Any],
    label: str,
) -> None:
    real_floor = require_int(
        record.get("reference_floor_real_digits"),
        f"{label}.reference_floor_real_digits",
    )
    imag_floor = require_int(
        record.get("reference_floor_imag_digits"),
        f"{label}.reference_floor_imag_digits",
    )
    expect(
        record.get("reference_floor_id") == expected["reference_floor_id"],
        f"{label}.reference_floor_id drifted",
    )
    expect(
        real_floor == expected["reference_floor_real_digits"],
        f"{label}.reference_floor_real_digits drifted",
    )
    expect(
        imag_floor == expected["reference_floor_imag_digits"],
        f"{label}.reference_floor_imag_digits drifted",
    )
    reason = require_string(record.get("reference_floor_reason"), f"{label}.reference_floor_reason")
    expect(
        reason == expected["reference_floor_reason"],
        f"{label}.reference_floor_reason drifted",
    )
    expect(
        "retained AMFlow reference" in reason,
        f"{label}.reference_floor_reason must identify the retained AMFlow floor",
    )


def validate_reference_floor_record(
    record: dict[str, Any],
    expected: dict[str, Any],
    label: str,
) -> None:
    real_digits = require_int(record.get("real_agreement_digits"), f"{label}.real_agreement_digits")
    imag_digits = require_int(record.get("imag_agreement_digits"), f"{label}.imag_agreement_digits")
    real_floor = require_int(
        record.get("reference_floor_real_digits"),
        f"{label}.reference_floor_real_digits",
    )
    imag_floor = require_int(
        record.get("reference_floor_imag_digits"),
        f"{label}.reference_floor_imag_digits",
    )
    validate_reference_floor_metadata(record, expected, label)
    expect(
        real_digits >= real_floor,
        f"{label} real agreement {real_digits} dropped below reference floor {real_floor}",
    )
    expect(
        imag_digits >= imag_floor,
        f"{label} imag agreement {imag_digits} dropped below reference floor {imag_floor}",
    )


def validate_reference_floor_manifest(manifest: dict[str, Any], label: str) -> dict[str, Any]:
    expect(manifest.get("schema_version") == 1, f"{label}.schema_version must be 1")
    expect(
        manifest.get("benchmark_id") == "complex_kinematics",
        f"{label}.benchmark_id drifted",
    )
    raw_floors = require_list(
        manifest.get("compare_cpp_vs_amflow_reference_floors"),
        f"{label}.compare_cpp_vs_amflow_reference_floors",
    )
    expect(
        len(raw_floors) == EXPECTED_REFERENCE_FLOOR_MATCHES,
        f"{label} must declare exactly four row 5/6 reference floors",
    )

    indexed: dict[tuple[str, int], dict[str, Any]] = {}
    for floor_index, raw_floor in enumerate(raw_floors):
        floor = require_object(
            raw_floor,
            f"{label}.compare_cpp_vs_amflow_reference_floors[{floor_index}]",
        )
        integral = require_string(
            floor.get("integral"),
            f"{label}.compare_cpp_vs_amflow_reference_floors[{floor_index}].integral",
        )
        order = require_int(
            floor.get("order"),
            f"{label}.compare_cpp_vs_amflow_reference_floors[{floor_index}].order",
        )
        key = (integral, order)
        expect(key not in indexed, f"{label} repeats reference floor {coefficient_key(*key)}")
        expected = EXPECTED_TARGETS.get(key)
        expect(
            expected is not None,
            f"{label} declares non-row56 reference floor {coefficient_key(*key)}",
        )
        component_digits = require_object(
            floor.get("component_digits"),
            f"{label}.compare_cpp_vs_amflow_reference_floors[{floor_index}].component_digits",
        )
        normalized = {
            "reference_floor_id": require_string(
                floor.get("id"),
                f"{label}.compare_cpp_vs_amflow_reference_floors[{floor_index}].id",
            ),
            "reference_floor_real_digits": require_int(
                component_digits.get("real"),
                f"{label}.compare_cpp_vs_amflow_reference_floors[{floor_index}].component_digits.real",
            ),
            "reference_floor_imag_digits": require_int(
                component_digits.get("imag"),
                f"{label}.compare_cpp_vs_amflow_reference_floors[{floor_index}].component_digits.imag",
            ),
            "reference_floor_reason": require_string(
                floor.get("reason"),
                f"{label}.compare_cpp_vs_amflow_reference_floors[{floor_index}].reason",
            ),
        }
        validate_reference_floor_metadata(
            normalized,
            expected,
            f"{label}.{coefficient_key(*key)}",
        )
        indexed[key] = normalized

    expect(
        set(indexed) == set(EXPECTED_TARGETS),
        f"{label}.compare_cpp_vs_amflow_reference_floors no longer names the row 5/6 target set",
    )
    return {
        "manifest_reference_floor_count": len(indexed),
        "row56_reference_floor_targets": expected_target_contract(),
    }


def validate_reference_floor_summary(summary: dict[str, Any], label: str) -> dict[str, Any]:
    expect(summary.get("schema_version") == 1, f"{label}.schema_version must be 1")
    expect(summary.get("comparison") == "cpp-vs-amflow", f"{label}.comparison drifted")
    expect(summary.get("benchmark_id") == "complex_kinematics", f"{label}.benchmark_id drifted")
    expect(summary.get("tolerance_digits") == 50, f"{label}.tolerance_digits must stay compare50")
    expect(summary.get("passed") is True, f"{label} must pass")
    expect(
        summary.get("comparison_verdict") == "matched-to-reference-floor",
        f"{label}.comparison_verdict must stay matched-to-reference-floor",
    )
    expect(summary.get("failures") == [], f"{label}.failures must stay empty")
    compared_count = require_int(
        summary.get("compared_coefficient_count"),
        f"{label}.compared_coefficient_count",
    )
    accepted_count = require_int(
        summary.get("accepted_coefficient_count"),
        f"{label}.accepted_coefficient_count",
    )
    tolerance_pass_count = require_int(
        summary.get("passed_coefficient_count"),
        f"{label}.passed_coefficient_count",
    )
    reference_floor_count = require_int(
        summary.get("reference_floor_matched_coefficient_count"),
        f"{label}.reference_floor_matched_coefficient_count",
    )
    expect(
        compared_count == EXPECTED_COMPARED_COEFFICIENTS,
        f"{label} compared coefficient count drifted",
    )
    expect(accepted_count == compared_count, f"{label} must accept every compared coefficient")
    expect(
        tolerance_pass_count == EXPECTED_TOLERANCE_PASSES,
        f"{label} must keep only non-row56 coefficients at 50-digit tolerance",
    )
    expect(
        reference_floor_count == EXPECTED_REFERENCE_FLOOR_MATCHES,
        f"{label} must keep exactly four row 5/6 reference-floor matches",
    )
    expect(
        require_int(summary.get("minimum_digit_agreement"), f"{label}.minimum_digit_agreement") == 11,
        f"{label}.minimum_digit_agreement must keep the retained AMFlow 11-digit floor visible",
    )

    coefficients = index_coefficients(summary, label)
    reference_floor_matches = index_reference_floor_matches(summary, label)
    expect(
        set(reference_floor_matches) == set(EXPECTED_TARGETS),
        f"{label}.reference_floor_matches no longer names the row 5/6 target set",
    )
    for key, expected in EXPECTED_TARGETS.items():
        target_label = f"{label}.{coefficient_key(*key)}"
        expect(key in coefficients, f"{target_label} is missing from coefficient summary")
        coefficient = coefficients[key]
        expect(require_bool(coefficient.get("passed"), f"{target_label}.passed"), f"{target_label} must pass")
        expect(
            require_bool(
                coefficient.get("matched_to_reference_floor"),
                f"{target_label}.matched_to_reference_floor",
            ),
            f"{target_label} must match to reference floor",
        )
        expect(
            not require_bool(
                coefficient.get("matched_to_tolerance_digits"),
                f"{target_label}.matched_to_tolerance_digits",
            ),
            f"{target_label} must not be reported as a 50-digit match",
        )
        expect(
            coefficient.get("verdict") == "matched-to-reference-floor",
            f"{target_label}.verdict must stay matched-to-reference-floor",
        )
        validate_reference_floor_record(coefficient, expected, target_label)
        validate_reference_floor_record(
            reference_floor_matches[key],
            expected,
            f"{label}.reference_floor_matches[{coefficient_key(*key)}]",
        )

    tolerance_verdict = "matched-to-50-digit"
    for key, coefficient in coefficients.items():
        if key in EXPECTED_TARGETS:
            continue
        coefficient_label = f"{label}.{coefficient_key(*key)}"
        expect(
            require_bool(coefficient.get("passed"), f"{coefficient_label}.passed"),
            f"{coefficient_label} must pass",
        )
        expect(
            require_bool(
                coefficient.get("matched_to_tolerance_digits"),
                f"{coefficient_label}.matched_to_tolerance_digits",
            ),
            f"{coefficient_label} must remain a 50-digit match",
        )
        expect(
            not require_bool(
                coefficient.get("matched_to_reference_floor"),
                f"{coefficient_label}.matched_to_reference_floor",
            ),
            f"{coefficient_label} must not use the reference-floor exception",
        )
        expect(
            coefficient.get("verdict") == tolerance_verdict,
            f"{coefficient_label}.verdict must stay {tolerance_verdict}",
        )

    return {
        "comparison_verdict": summary["comparison_verdict"],
        "compared_coefficient_count": compared_count,
        "passed_coefficient_count": tolerance_pass_count,
        "reference_floor_matched_coefficient_count": reference_floor_count,
        "minimum_digit_agreement": summary["minimum_digit_agreement"],
        "row56_reference_floor_targets": expected_target_contract(),
    }


def verify_paths(
    cpp_result_path: Path,
    amflow_golden_path: Path,
    retained_comparison_path: Path,
) -> dict[str, Any]:
    manifest_contract = validate_reference_floor_manifest(
        load_json(amflow_golden_path),
        "amflow_golden",
    )
    fresh_summary = compare_cpp_vs_amflow(
        cpp_result_path=cpp_result_path,
        amflow_golden_path=amflow_golden_path,
        tolerance_digits=50,
    )
    retained_summary = load_json(retained_comparison_path)
    fresh_contract = validate_reference_floor_summary(fresh_summary, "fresh_comparison")
    retained_contract = validate_reference_floor_summary(retained_summary, "retained_comparison")
    expect(
        fresh_contract == retained_contract,
        "fresh comparison and retained comparison disagree on the b61n reference-floor contract",
    )
    expect(
        manifest_contract["row56_reference_floor_targets"]
        == fresh_contract["row56_reference_floor_targets"],
        "AMFlow reference-floor manifest and comparator summary disagree on row 5/6 floors",
    )
    return {
        "schema_version": 1,
        "verifier": "b61n-row56-reference-floor-parity-gate-v1",
        "cross_check_passed": True,
        "manifest_contract_matched": True,
        "cpp_result": str(cpp_result_path),
        "amflow_golden": str(amflow_golden_path),
        "retained_comparison": str(retained_comparison_path),
        **fresh_contract,
        "m7_closure_claimed": False,
        "release_readiness_claimed": False,
    }


def rejected(summary: dict[str, Any], expected_error: str) -> bool:
    try:
        validate_reference_floor_summary(summary, "self_check")
    except Exception as error:  # noqa: BLE001 - self-check intentionally probes failures.
        return expected_error in str(error)
    return False


def manifest_rejected(manifest: dict[str, Any], expected_error: str) -> bool:
    try:
        validate_reference_floor_manifest(manifest, "self_check.manifest")
    except Exception as error:  # noqa: BLE001 - self-check intentionally probes failures.
        return expected_error in str(error)
    return False


def run_self_check(retained_comparison_path: Path, amflow_golden_path: Path) -> dict[str, Any]:
    valid_summary = load_json(retained_comparison_path)
    valid_contract = validate_reference_floor_summary(valid_summary, "self_check.valid")
    valid_manifest = load_json(amflow_golden_path)
    manifest_contract = validate_reference_floor_manifest(
        valid_manifest,
        "self_check.valid_manifest",
    )

    masked_row56_regression = copy.deepcopy(valid_summary)
    masked_row56_regression["integrals"][-1]["coefficients"][-1]["imag_agreement_digits"] = 11

    fake_50_digit = copy.deepcopy(valid_summary)
    fake_50_digit["integrals"][-1]["coefficients"][-1]["matched_to_tolerance_digits"] = True

    missing_target = copy.deepcopy(valid_summary)
    missing_target["reference_floor_matches"] = missing_target["reference_floor_matches"][:-1]

    duplicate_target = copy.deepcopy(valid_summary)
    duplicate_target["reference_floor_matches"][1]["integral"] = duplicate_target[
        "reference_floor_matches"
    ][0]["integral"]
    duplicate_target["reference_floor_matches"][1]["order"] = duplicate_target[
        "reference_floor_matches"
    ][0]["order"]

    stale_floor_id = copy.deepcopy(valid_summary)
    stale_floor_id["reference_floor_matches"][0]["reference_floor_id"] = (
        "synthetic-b61n-reference-floor-id"
    )

    floor_digit_metadata_drift = copy.deepcopy(valid_summary)
    floor_digit_metadata_drift["reference_floor_matches"][0]["reference_floor_real_digits"] += 1

    missing_retained_floor_reason = copy.deepcopy(valid_summary)
    missing_retained_floor_reason["reference_floor_matches"][0][
        "reference_floor_reason"
    ] = "synthetic missing retained-reference rationale"

    stale_summary_floor_reason = copy.deepcopy(valid_summary)
    stale_summary_floor_reason["reference_floor_matches"][0][
        "reference_floor_reason"
    ] = "post-M7 b61n row 5 eps^0 diagnostic: retained AMFlow reference floor drifted"

    missing_manifest_target = copy.deepcopy(valid_manifest)
    missing_manifest_target["compare_cpp_vs_amflow_reference_floors"] = missing_manifest_target[
        "compare_cpp_vs_amflow_reference_floors"
    ][:-1]

    duplicate_manifest_target = copy.deepcopy(valid_manifest)
    duplicate_manifest_target["compare_cpp_vs_amflow_reference_floors"][1]["integral"] = (
        duplicate_manifest_target["compare_cpp_vs_amflow_reference_floors"][0]["integral"]
    )
    duplicate_manifest_target["compare_cpp_vs_amflow_reference_floors"][1]["order"] = (
        duplicate_manifest_target["compare_cpp_vs_amflow_reference_floors"][0]["order"]
    )

    stale_manifest_floor_id = copy.deepcopy(valid_manifest)
    stale_manifest_floor_id["compare_cpp_vs_amflow_reference_floors"][0]["id"] = (
        "synthetic-b61n-reference-floor-id"
    )

    manifest_digit_metadata_drift = copy.deepcopy(valid_manifest)
    manifest_digit_metadata_drift["compare_cpp_vs_amflow_reference_floors"][0][
        "component_digits"
    ]["real"] += 1

    stale_manifest_floor_reason = copy.deepcopy(valid_manifest)
    stale_manifest_floor_reason["compare_cpp_vs_amflow_reference_floors"][0]["reason"] = (
        "post-M7 b61n row 5 eps^0 diagnostic: retained AMFlow reference floor drifted"
    )

    checks = {
        "retained_fixture_passes": valid_contract["reference_floor_matched_coefficient_count"] == 4,
        "manifest_fixture_passes": manifest_contract["manifest_reference_floor_count"] == 4,
        "rejects_matched_reference_floor_below_floor_regression": rejected(
            masked_row56_regression,
            "dropped below reference floor",
        ),
        "rejects_fake_50_digit_row56_claim": rejected(fake_50_digit, "must not be reported"),
        "rejects_missing_row56_reference_floor_target": rejected(missing_target, "row 5/6 target set"),
        "rejects_duplicate_row56_reference_floor_target": rejected(
            duplicate_target,
            "repeats reference-floor match",
        ),
        "rejects_stale_reference_floor_id": rejected(
            stale_floor_id,
            "reference_floor_id drifted",
        ),
        "rejects_reference_floor_digit_metadata_drift": rejected(
            floor_digit_metadata_drift,
            "reference_floor_real_digits drifted",
        ),
        "rejects_missing_retained_floor_reason": rejected(
            missing_retained_floor_reason,
            "reference_floor_reason drifted",
        ),
        "rejects_reference_floor_reason_drift": rejected(
            stale_summary_floor_reason,
            "reference_floor_reason drifted",
        ),
        "rejects_missing_manifest_reference_floor_target": manifest_rejected(
            missing_manifest_target,
            "exactly four row 5/6 reference floors",
        ),
        "rejects_duplicate_manifest_reference_floor_target": manifest_rejected(
            duplicate_manifest_target,
            "repeats reference floor",
        ),
        "rejects_stale_manifest_reference_floor_id": manifest_rejected(
            stale_manifest_floor_id,
            "reference_floor_id drifted",
        ),
        "rejects_manifest_reference_floor_digit_metadata_drift": manifest_rejected(
            manifest_digit_metadata_drift,
            "reference_floor_real_digits drifted",
        ),
        "rejects_manifest_reference_floor_reason_drift": manifest_rejected(
            stale_manifest_floor_reason,
            "reference_floor_reason drifted",
        ),
    }
    expect(all(checks.values()), "b61n reference-floor verifier self-check failed")
    return {
        "schema_version": 1,
        "verifier": "b61n-row56-reference-floor-parity-gate-v1",
        "self_check_passed": True,
        "checks": checks,
    }


def parse_args(argv: list[str]) -> argparse.Namespace:
    root = repo_root()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--cpp-result-path",
        type=Path,
        default=root / DEFAULT_CPP_RESULT,
        help="b61n retained C++ solve-series result JSON path",
    )
    parser.add_argument(
        "--amflow-golden-path",
        type=Path,
        default=root / DEFAULT_AMFLOW_GOLDEN,
        help="b61n reference-floor AMFlow golden manifest path",
    )
    parser.add_argument(
        "--retained-comparison-path",
        type=Path,
        default=root / DEFAULT_RETAINED_COMPARISON,
        help="retained b61n compare50 reference-floor JSON path",
    )
    parser.add_argument("--summary-path", type=Path, help="Optional JSON output path")
    parser.add_argument("--self-check", action="store_true", help="Run synthetic rejection checks")
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    try:
        if args.self_check:
            summary = run_self_check(args.retained_comparison_path, args.amflow_golden_path)
        else:
            summary = verify_paths(
                args.cpp_result_path,
                args.amflow_golden_path,
                args.retained_comparison_path,
            )
        if args.summary_path is not None:
            write_json(args.summary_path, summary)
        print_json(summary)
        return 0
    except Exception as error:  # noqa: BLE001 - command-line verifier should fail closed.
        summary = {
            "schema_version": 1,
            "verifier": "b61n-row56-reference-floor-parity-gate-v1",
            "cross_check_passed": False,
            "blocking_reasons": [str(error)],
            "m7_closure_claimed": False,
            "release_readiness_claimed": False,
        }
        if args.summary_path is not None:
            write_json(args.summary_path, summary)
        print_json(summary)
        return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
