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
    },
    ("box[1,1,1,1]", -2): {
        "reference_floor_id": "b61n-row6-eps-2-retained-amflow-floor",
        "reference_floor_real_digits": 46,
        "reference_floor_imag_digits": 46,
    },
    ("box[1,1,1,1]", -1): {
        "reference_floor_id": "b61n-row6-eps-1-retained-amflow-floor",
        "reference_floor_real_digits": 12,
        "reference_floor_imag_digits": 13,
    },
    ("box[1,1,1,1]", 0): {
        "reference_floor_id": "b61n-row6-eps0-retained-amflow-floor",
        "reference_floor_real_digits": 12,
        "reference_floor_imag_digits": 12,
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
    expect(
        real_digits >= real_floor,
        f"{label} real agreement {real_digits} dropped below reference floor {real_floor}",
    )
    expect(
        imag_digits >= imag_floor,
        f"{label} imag agreement {imag_digits} dropped below reference floor {imag_floor}",
    )
    reason = require_string(record.get("reference_floor_reason"), f"{label}.reference_floor_reason")
    expect(
        "retained AMFlow reference" in reason,
        f"{label}.reference_floor_reason must identify the retained AMFlow floor",
    )


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
        "row56_reference_floor_targets": [
            {"integral": integral, "order": order, **EXPECTED_TARGETS[(integral, order)]}
            for integral, order in sorted(EXPECTED_TARGETS)
        ],
    }


def verify_paths(
    cpp_result_path: Path,
    amflow_golden_path: Path,
    retained_comparison_path: Path,
) -> dict[str, Any]:
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
    return {
        "schema_version": 1,
        "verifier": "b61n-row56-reference-floor-parity-gate-v1",
        "cross_check_passed": True,
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


def run_self_check(retained_comparison_path: Path) -> dict[str, Any]:
    valid_summary = load_json(retained_comparison_path)
    valid_contract = validate_reference_floor_summary(valid_summary, "self_check.valid")

    below_floor = copy.deepcopy(valid_summary)
    below_floor["integrals"][-1]["coefficients"][-1]["imag_agreement_digits"] = 11

    fake_50_digit = copy.deepcopy(valid_summary)
    fake_50_digit["integrals"][-1]["coefficients"][-1]["matched_to_tolerance_digits"] = True

    missing_target = copy.deepcopy(valid_summary)
    missing_target["reference_floor_matches"] = missing_target["reference_floor_matches"][:-1]

    checks = {
        "retained_fixture_passes": valid_contract["reference_floor_matched_coefficient_count"] == 4,
        "rejects_below_reference_floor": rejected(below_floor, "dropped below reference floor"),
        "rejects_fake_50_digit_row56_claim": rejected(fake_50_digit, "must not be reported"),
        "rejects_missing_row56_reference_floor_target": rejected(missing_target, "row 5/6 target set"),
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
            summary = run_self_check(args.retained_comparison_path)
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
