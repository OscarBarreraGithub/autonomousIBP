#!/usr/bin/env python3
"""Verify the b61n row 5/6 precision-uplift monotonicity sidecar."""

from __future__ import annotations

import argparse
import copy
import json
import sys
from fractions import Fraction
from pathlib import Path
from typing import Any


DEFAULT_PRECISION_EVIDENCE = Path(
    "tools/reference-harness/specs/m7/lane2/b61n-pipeline-precision-evidence.json"
)

EXPECTED_TARGETS: tuple[tuple[str, int], ...] = (
    ("box[1,0,1,1]", 0),
    ("box[1,1,1,1]", -2),
    ("box[1,1,1,1]", -1),
    ("box[1,1,1,1]", 0),
)
COMPONENTS: tuple[str, ...] = ("real", "imag")


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


def require_string(raw: Any, label: str) -> str:
    if not isinstance(raw, str) or not raw:
        raise TypeError(f"{label} must be a non-empty string")
    return raw


def require_bool(raw: Any, label: str) -> bool:
    if not isinstance(raw, bool):
        raise TypeError(f"{label} must be a bool")
    return raw


def require_int(raw: Any, label: str) -> int:
    if not isinstance(raw, int) or isinstance(raw, bool):
        raise TypeError(f"{label} must be an int")
    return raw


def resolve_repo_path(root: Path, raw_path: str, label: str) -> Path:
    path = Path(raw_path)
    if not path.is_absolute():
        path = root / path
    expect(path.exists(), f"{label} does not exist: {path}")
    return path


def target_key(integral: str, eps_order: int) -> str:
    return f"{integral} eps^{eps_order}"


def indexed_cpp_coefficients(cpp_result: dict[str, Any]) -> dict[tuple[str, int], dict[str, Any]]:
    indexed: dict[tuple[str, int], dict[str, Any]] = {}
    for result_index, raw_result in enumerate(
        require_list(cpp_result.get("results"), "cpp_result.results")
    ):
        result = require_object(raw_result, f"cpp_result.results[{result_index}]")
        integral = require_string(
            result.get("integral"),
            f"cpp_result.results[{result_index}].integral",
        )
        for coefficient_index, raw_coefficient in enumerate(
            require_list(
                result.get("epsilon_orders"),
                f"cpp_result.results[{result_index}].epsilon_orders",
            )
        ):
            coefficient = require_object(
                raw_coefficient,
                f"cpp_result.results[{result_index}].epsilon_orders[{coefficient_index}]",
            )
            order = require_int(
                coefficient.get("order"),
                f"cpp_result.{integral}.epsilon_orders[{coefficient_index}].order",
            )
            key = (integral, order)
            expect(key not in indexed, f"cpp_result repeats {target_key(*key)}")
            indexed[key] = coefficient
    return indexed


def indexed_diagnostic_targets(diagnostic: dict[str, Any]) -> dict[tuple[str, int], dict[str, Any]]:
    indexed: dict[tuple[str, int], dict[str, Any]] = {}
    for target_index, raw_target in enumerate(
        require_list(diagnostic.get("target_diagnostics"), "diagnostic.target_diagnostics")
    ):
        target = require_object(raw_target, f"diagnostic.target_diagnostics[{target_index}]")
        integral = require_string(
            target.get("integral"),
            f"diagnostic.target_diagnostics[{target_index}].integral",
        )
        order = require_int(
            target.get("eps_order"),
            f"diagnostic.target_diagnostics[{target_index}].eps_order",
        )
        key = (integral, order)
        expect(key not in indexed, f"diagnostic repeats {target_key(*key)}")
        indexed[key] = target
    return indexed


def render_fractional_digits(raw_fraction: str, fractional_digits: int) -> str:
    fraction = Fraction(raw_fraction)
    sign = "-" if fraction < 0 else ""
    numerator = abs(fraction.numerator)
    denominator = fraction.denominator
    scale = 10**fractional_digits
    scaled = numerator * scale // denominator
    integer_part = scaled // scale
    fractional_part = scaled % scale
    return f"{sign}{integer_part}.{fractional_part:0{fractional_digits}d}"


def visible_prefix(raw_prefix: str, label: str) -> str:
    prefix = require_string(raw_prefix, label)
    expect(prefix.endswith("..."), f"{label} must be an ellipsis-truncated decimal prefix")
    return prefix[:-3]


def fractional_digit_count(raw_decimal: str, label: str) -> int:
    decimal = raw_decimal[1:] if raw_decimal.startswith("-") else raw_decimal
    expect("E" not in decimal and "e" not in decimal, f"{label} must be plain decimal")
    expect("." in decimal, f"{label} must contain a decimal point")
    fractional = decimal.split(".", 1)[1]
    expect(fractional.isdigit(), f"{label} fractional part must contain only digits")
    return len(fractional)


def validate_component(
    *,
    evidence_target: dict[str, Any],
    diagnostic_target: dict[str, Any],
    cpp_coefficient: dict[str, Any],
    component: str,
    label: str,
) -> dict[str, int]:
    reference_floor = require_int(
        evidence_target.get(f"reference_floor_{component}_digits"),
        f"{label}.reference_floor_{component}_digits",
    )
    diagnostic_floor = require_int(
        diagnostic_target.get(f"reference_floor_{component}_digits"),
        f"diagnostic.{label}.reference_floor_{component}_digits",
    )
    expect(reference_floor == diagnostic_floor, f"{label} {component} reference floor drifted")

    standard_digits = require_int(
        require_object(
            evidence_target.get("standard_serialized_fraction_digits"),
            f"{label}.standard_serialized_fraction_digits",
        ).get(component),
        f"{label}.standard_serialized_fraction_digits.{component}",
    )
    uplifted_digits = require_int(
        require_object(
            evidence_target.get("uplifted_serialized_fraction_digits"),
            f"{label}.uplifted_serialized_fraction_digits",
        ).get(component),
        f"{label}.uplifted_serialized_fraction_digits.{component}",
    )
    expect(
        reference_floor < standard_digits < uplifted_digits,
        (
            f"{label} {component} precision must strictly increase from reference "
            f"floor to standard serialization to uplifted serialization"
        ),
    )

    standard = require_string(
        require_object(evidence_target.get("standard_80"), f"{label}.standard_80").get(component),
        f"{label}.standard_80.{component}",
    )
    expect(
        fractional_digit_count(standard, f"{label}.standard_80.{component}") == standard_digits,
        f"{label} {component} standard fraction digit count drifted",
    )

    exact_fraction = require_string(
        cpp_coefficient.get(f"exact_{component}"),
        f"cpp_result.{label}.exact_{component}",
    )
    expect(
        render_fractional_digits(exact_fraction, standard_digits) == standard,
        f"{label} {component} standard serialization no longer matches exact rational",
    )

    additional_digits = require_int(
        require_object(
            evidence_target.get("additional_fraction_digits_after_standard_80"),
            f"{label}.additional_fraction_digits_after_standard_80",
        ).get(component),
        f"{label}.additional_fraction_digits_after_standard_80.{component}",
    )
    expect(
        additional_digits == uplifted_digits - standard_digits,
        f"{label} {component} additional digit count must equal uplifted-standard delta",
    )

    beyond_floor = require_int(
        require_object(
            evidence_target.get("digits_beyond_reference_floor_in_uplifted_160"),
            f"{label}.digits_beyond_reference_floor_in_uplifted_160",
        ).get(component),
        f"{label}.digits_beyond_reference_floor_in_uplifted_160.{component}",
    )
    expect(
        beyond_floor == uplifted_digits - reference_floor,
        f"{label} {component} uplifted digits beyond floor must match uplifted-floor delta",
    )

    uplifted_160 = render_fractional_digits(exact_fraction, uplifted_digits)
    expect(
        uplifted_160.startswith(standard),
        f"{label} {component} uplifted exact rendering must extend the standard digits",
    )
    tail = uplifted_160[len(standard):]
    expect(
        any(char.isdigit() and char != "0" for char in tail),
        f"{label} {component} uplifted exact tail after standard digits must be nonzero",
    )
    tail_flags = require_object(
        evidence_target.get("uplifted_tail_after_standard_80_has_nonzero_digit"),
        f"{label}.uplifted_tail_after_standard_80_has_nonzero_digit",
    )
    expect(
        require_bool(
            tail_flags.get(component),
            f"{label}.uplifted_tail_after_standard_80_has_nonzero_digit.{component}",
        ),
        f"{label} {component} sidecar must record a nonzero uplifted tail",
    )

    prefixes = {
        "uplifted_120_prefix": visible_prefix(
            require_object(
                evidence_target.get("uplifted_120_prefix"),
                f"{label}.uplifted_120_prefix",
            ).get(component),
            f"{label}.uplifted_120_prefix.{component}",
        ),
        "uplifted_160_prefix": visible_prefix(
            require_object(
                evidence_target.get("uplifted_160_prefix"),
                f"{label}.uplifted_160_prefix",
            ).get(component),
            f"{label}.uplifted_160_prefix.{component}",
        ),
    }
    for prefix_label, prefix in prefixes.items():
        visible_digits = fractional_digit_count(prefix, f"{label}.{prefix_label}.{component}")
        expect(
            visible_digits > standard_digits,
            f"{label} {component} {prefix_label} must expose digits beyond standard 80",
        )
        expect(
            render_fractional_digits(exact_fraction, visible_digits) == prefix,
            f"{label} {component} {prefix_label} no longer matches exact rational prefix",
        )

    expect(
        require_bool(
            evidence_target.get("standard_serialization_matches_exact_rational_80"),
            f"{label}.standard_serialization_matches_exact_rational_80",
        ),
        f"{label} sidecar must record exact-rational standard serialization",
    )
    expect(
        require_bool(
            evidence_target.get("uplifted_160_extends_standard_80"),
            f"{label}.uplifted_160_extends_standard_80",
        ),
        f"{label} sidecar must record uplifted extension of standard serialization",
    )
    expect(
        require_bool(
            evidence_target.get("uplifted_fraction_digits_exceed_reference_floor"),
            f"{label}.uplifted_fraction_digits_exceed_reference_floor",
        ),
        f"{label} sidecar must record uplifted digits exceeding the reference floor",
    )

    return {
        "reference_floor_digits": reference_floor,
        "standard_fraction_digits": standard_digits,
        "uplifted_fraction_digits": uplifted_digits,
    }


def verify_payloads(
    precision_evidence: dict[str, Any],
    cpp_result: dict[str, Any],
    diagnostic: dict[str, Any],
) -> dict[str, Any]:
    expect(precision_evidence.get("schema_version") == 1, "precision evidence schema_version must be 1")
    expect(
        precision_evidence.get("evidence_id") == "b61n-pipeline-precision-evidence",
        "precision evidence_id drifted",
    )
    expect(
        precision_evidence.get("scope")
        == "post-M7 b61n row 5/6 reference-floor alternative-path precision evidence",
        "precision evidence scope drifted",
    )
    expect(cpp_result.get("benchmark_id") == "complex_kinematics", "C++ benchmark_id drifted")
    expect(
        diagnostic.get("diagnostic_id") == "b61n-row56-specific-target-diagnostic",
        "diagnostic_id drifted",
    )

    cpp_by_target = indexed_cpp_coefficients(cpp_result)
    diagnostic_by_target = indexed_diagnostic_targets(diagnostic)
    component_records: list[dict[str, Any]] = []
    targets = require_list(precision_evidence.get("targets"), "precision_evidence.targets")
    expect(targets, "precision evidence targets must not be empty")
    precision_target_keys: list[tuple[str, int]] = []

    for target_index, raw_target in enumerate(targets):
        target = require_object(raw_target, f"precision_evidence.targets[{target_index}]")
        integral = require_string(
            target.get("integral"),
            f"precision_evidence.targets[{target_index}].integral",
        )
        eps_order = require_int(
            target.get("eps_order"),
            f"precision_evidence.targets[{target_index}].eps_order",
        )
        key = (integral, eps_order)
        label = target_key(*key)
        expect(
            key not in precision_target_keys,
            f"precision_evidence.targets repeats {label}",
        )
        precision_target_keys.append(key)
        expect(key in cpp_by_target, f"C++ result is missing {label}")
        expect(key in diagnostic_by_target, f"diagnostic is missing {label}")
        for component in COMPONENTS:
            record = validate_component(
                evidence_target=target,
                diagnostic_target=diagnostic_by_target[key],
                cpp_coefficient=cpp_by_target[key],
                component=component,
                label=label,
            )
            component_records.append(
                {
                    "integral": integral,
                    "eps_order": eps_order,
                    "component": component,
                    **record,
                }
            )

    expect(
        set(precision_target_keys) == set(diagnostic_by_target),
        "precision evidence target set must match diagnostic row 5/6 targets",
    )
    expect(
        tuple(precision_target_keys) == EXPECTED_TARGETS,
        "precision evidence row 5/6 target order drifted",
    )

    summary = require_object(precision_evidence.get("summary"), "precision_evidence.summary")
    expect(
        require_int(summary.get("target_count"), "precision_evidence.summary.target_count")
        == len(targets),
        "precision evidence target_count drifted",
    )
    expect(
        require_int(
            summary.get("minimum_reference_floor_digits"),
            "precision_evidence.summary.minimum_reference_floor_digits",
        )
        == min(record["reference_floor_digits"] for record in component_records),
        "precision evidence minimum reference floor drifted",
    )
    expect(
        require_int(
            summary.get("minimum_uplifted_fraction_digits"),
            "precision_evidence.summary.minimum_uplifted_fraction_digits",
        )
        == min(record["uplifted_fraction_digits"] for record in component_records),
        "precision evidence minimum uplifted fraction digits drifted",
    )
    for field in (
        "all_standard_serializations_match_exact_rational_80",
        "all_uplifted_160_fraction_digits_exceed_reference_floor",
        "all_uplifted_160_serializations_extend_standard_80",
        "all_uplifted_tail_after_standard_80_has_nonzero_digit",
    ):
        expect(require_bool(summary.get(field), f"precision_evidence.summary.{field}"), f"{field} must stay true")

    return {
        "schema_version": 1,
        "verifier": "b61n-precision-uplift-monotonicity-v1",
        "cross_check_passed": True,
        "target_count": len(targets),
        "component_count": len(component_records),
        "minimum_reference_floor_digits": min(
            record["reference_floor_digits"] for record in component_records
        ),
        "minimum_standard_fraction_digits": min(
            record["standard_fraction_digits"] for record in component_records
        ),
        "minimum_uplifted_fraction_digits": min(
            record["uplifted_fraction_digits"] for record in component_records
        ),
        "m7_closure_claimed": False,
        "release_readiness_claimed": False,
    }


def verify_paths(precision_evidence_path: Path) -> dict[str, Any]:
    root = repo_root()
    evidence = load_json(precision_evidence_path)
    method = require_object(evidence.get("method"), "precision_evidence.method")
    expect(
        method.get("amflow_reference_values_used_for_digit_count") is False,
        "precision evidence must not use AMFlow reference values for digit count uplift",
    )
    cpp_path = resolve_repo_path(
        root,
        require_string(evidence.get("source_cpp_result"), "precision_evidence.source_cpp_result"),
        "precision_evidence.source_cpp_result",
    )
    diagnostic_path = resolve_repo_path(
        root,
        require_string(evidence.get("source_diagnostic"), "precision_evidence.source_diagnostic"),
        "precision_evidence.source_diagnostic",
    )
    summary = verify_payloads(evidence, load_json(cpp_path), load_json(diagnostic_path))
    return {
        **summary,
        "precision_evidence": str(precision_evidence_path),
        "source_cpp_result": str(cpp_path),
        "source_diagnostic": str(diagnostic_path),
    }


def rejected(
    precision_evidence: dict[str, Any],
    cpp_result: dict[str, Any],
    diagnostic: dict[str, Any],
    expected_error: str,
) -> bool:
    try:
        verify_payloads(precision_evidence, cpp_result, diagnostic)
    except Exception as error:  # noqa: BLE001 - self-check intentionally probes failures.
        return expected_error in str(error)
    return False


def synthetic_payloads() -> tuple[dict[str, Any], dict[str, Any], dict[str, Any]]:
    exact_real = "1234567/1000000"
    exact_imag = "-7654321/1000000"
    targets = [
        {
            "integral": integral,
            "eps_order": eps_order,
            "reference_floor_real_digits": 2,
            "reference_floor_imag_digits": 2,
            "standard_80": {"real": "1.234", "imag": "-7.654"},
            "standard_serialization_matches_exact_rational_80": True,
            "standard_serialized_fraction_digits": {"real": 3, "imag": 3},
            "uplifted_120_prefix": {"real": "1.2345...", "imag": "-7.6543..."},
            "uplifted_160_prefix": {"real": "1.2345...", "imag": "-7.6543..."},
            "uplifted_160_extends_standard_80": True,
            "uplifted_fraction_digits_exceed_reference_floor": True,
            "uplifted_serialized_fraction_digits": {"real": 5, "imag": 5},
            "additional_fraction_digits_after_standard_80": {"real": 2, "imag": 2},
            "digits_beyond_reference_floor_in_uplifted_160": {"real": 3, "imag": 3},
            "uplifted_tail_after_standard_80_has_nonzero_digit": {
                "real": True,
                "imag": True,
            },
        }
        for integral, eps_order in EXPECTED_TARGETS
    ]
    evidence = {
        "schema_version": 1,
        "evidence_id": "b61n-pipeline-precision-evidence",
        "scope": "post-M7 b61n row 5/6 reference-floor alternative-path precision evidence",
        "summary": {
            "all_standard_serializations_match_exact_rational_80": True,
            "all_uplifted_160_fraction_digits_exceed_reference_floor": True,
            "all_uplifted_160_serializations_extend_standard_80": True,
            "all_uplifted_tail_after_standard_80_has_nonzero_digit": True,
            "minimum_reference_floor_digits": 2,
            "minimum_uplifted_fraction_digits": 5,
            "target_count": len(EXPECTED_TARGETS),
        },
        "targets": targets,
    }
    cpp_result = {
        "benchmark_id": "complex_kinematics",
        "results": [
            {
                "integral": integral,
                "epsilon_orders": [
                    {
                        "order": eps_order,
                        "exact_real": exact_real,
                        "exact_imag": exact_imag,
                    }
                ],
            }
            for integral, eps_order in EXPECTED_TARGETS
        ],
    }
    diagnostic = {
        "diagnostic_id": "b61n-row56-specific-target-diagnostic",
        "target_diagnostics": [
            {
                "integral": integral,
                "eps_order": eps_order,
                "reference_floor_real_digits": 2,
                "reference_floor_imag_digits": 2,
            }
            for integral, eps_order in EXPECTED_TARGETS
        ],
    }
    return evidence, cpp_result, diagnostic


def run_self_check() -> dict[str, Any]:
    evidence, cpp_result, diagnostic = synthetic_payloads()
    valid_summary = verify_payloads(evidence, cpp_result, diagnostic)

    nonmonotone = copy.deepcopy(evidence)
    nonmonotone["targets"][0]["uplifted_serialized_fraction_digits"]["real"] = 3

    bad_delta = copy.deepcopy(evidence)
    bad_delta["targets"][0]["additional_fraction_digits_after_standard_80"]["real"] = 1

    bad_standard = copy.deepcopy(evidence)
    bad_standard["targets"][0]["standard_80"]["real"] = "1.235"

    duplicate_target = copy.deepcopy(evidence)
    duplicate_target["targets"].append(copy.deepcopy(duplicate_target["targets"][0]))
    duplicate_target["summary"]["target_count"] = len(EXPECTED_TARGETS) + 1

    missing_diagnostic_target = copy.deepcopy(diagnostic)
    missing_diagnostic_target["target_diagnostics"].append(
        {
            "integral": "box[0,0,0,1]",
            "eps_order": 0,
            "reference_floor_real_digits": 2,
            "reference_floor_imag_digits": 2,
        }
    )

    wrong_target_evidence = copy.deepcopy(evidence)
    wrong_target_cpp_result = copy.deepcopy(cpp_result)
    wrong_target_diagnostic = copy.deepcopy(diagnostic)
    wrong_target_evidence["targets"][0]["integral"] = "box[0,0,0,1]"
    wrong_target_cpp_result["results"][0]["integral"] = "box[0,0,0,1]"
    wrong_target_diagnostic["target_diagnostics"][0]["integral"] = "box[0,0,0,1]"

    checks = {
        "synthetic_fixture_passes": valid_summary["component_count"] == 8,
        "rejects_nonmonotone_uplift": rejected(
            nonmonotone,
            cpp_result,
            diagnostic,
            "precision must strictly increase",
        ),
        "rejects_bad_additional_digit_delta": rejected(
            bad_delta,
            cpp_result,
            diagnostic,
            "additional digit count",
        ),
        "rejects_standard_exact_rational_drift": rejected(
            bad_standard,
            cpp_result,
            diagnostic,
            "standard serialization no longer matches exact rational",
        ),
        "rejects_duplicate_precision_target": rejected(
            duplicate_target,
            cpp_result,
            diagnostic,
            "repeats box[1,0,1,1] eps^0",
        ),
        "rejects_missing_diagnostic_target": rejected(
            evidence,
            cpp_result,
            missing_diagnostic_target,
            "target set must match diagnostic row 5/6 targets",
        ),
        "rejects_coherent_non_row56_retargeting": rejected(
            wrong_target_evidence,
            wrong_target_cpp_result,
            wrong_target_diagnostic,
            "row 5/6 target order drifted",
        ),
    }
    expect(all(checks.values()), "b61n precision-uplift monotonicity self-check failed")
    return {
        "schema_version": 1,
        "verifier": "b61n-precision-uplift-monotonicity-v1",
        "self_check_passed": True,
        "checks": checks,
    }


def parse_args(argv: list[str]) -> argparse.Namespace:
    root = repo_root()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--precision-evidence-path",
        type=Path,
        default=root / DEFAULT_PRECISION_EVIDENCE,
        help="b61n precision-evidence sidecar path",
    )
    parser.add_argument("--self-check", action="store_true", help="Run synthetic rejection checks")
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    try:
        if args.self_check:
            summary = run_self_check()
        else:
            summary = verify_paths(args.precision_evidence_path)
        print_json(summary)
        return 0
    except Exception as error:  # noqa: BLE001 - command-line verifier should fail closed.
        print_json(
            {
                "schema_version": 1,
                "verifier": "b61n-precision-uplift-monotonicity-v1",
                "cross_check_passed": False,
                "blocking_reasons": [str(error)],
                "m7_closure_claimed": False,
                "release_readiness_claimed": False,
            }
        )
        return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
