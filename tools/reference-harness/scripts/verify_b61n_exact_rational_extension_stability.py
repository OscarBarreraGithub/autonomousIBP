#!/usr/bin/env python3
"""Verify the b61n exact-rational 160-digit extension fixture."""

from __future__ import annotations

import argparse
import copy
import json
import sys
from fractions import Fraction
from pathlib import Path
from typing import Any


DEFAULT_EXTENSION_FIXTURE = Path(
    "tools/reference-harness/specs/b61n/b61n-exact-rational-extension-160.json"
)
DEFAULT_SOURCE_CPP_RESULT = Path(
    "tools/reference-harness/specs/m7/lane2/"
    "complex_kinematics.c267-stripped.eps0.cpp-result.json"
)
DEFAULT_SOURCE_PRECISION_EVIDENCE = Path(
    "tools/reference-harness/specs/m7/lane2/b61n-pipeline-precision-evidence.json"
)
EXPECTED_TARGETS: tuple[tuple[str, int], ...] = (
    ("box[1,0,1,1]", 0),
    ("box[1,1,1,1]", -2),
    ("box[1,1,1,1]", -1),
    ("box[1,1,1,1]", 0),
)
COMPONENTS: tuple[str, ...] = ("real", "imag")
EXPECTED_FRACTIONAL_DIGITS = 160


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


def fractional_digit_count(raw_decimal: str, label: str) -> int:
    decimal = raw_decimal[1:] if raw_decimal.startswith("-") else raw_decimal
    expect("E" not in decimal and "e" not in decimal, f"{label} must be plain decimal")
    expect("." in decimal, f"{label} must contain a decimal point")
    fractional = decimal.split(".", 1)[1]
    expect(fractional.isdigit(), f"{label} fractional part must contain only digits")
    return len(fractional)


def visible_prefix(raw_prefix: str, label: str) -> str:
    prefix = require_string(raw_prefix, label)
    expect(prefix.endswith("..."), f"{label} must be an ellipsis-truncated decimal prefix")
    return prefix[:-3]


def index_cpp_coefficients(cpp_result: dict[str, Any]) -> dict[tuple[str, int], dict[str, Any]]:
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


def index_targets(
    payload: dict[str, Any],
    label: str,
) -> dict[tuple[str, int], dict[str, Any]]:
    indexed: dict[tuple[str, int], dict[str, Any]] = {}
    for target_index, raw_target in enumerate(require_list(payload.get("targets"), f"{label}.targets")):
        target = require_object(raw_target, f"{label}.targets[{target_index}]")
        integral = require_string(
            target.get("integral"),
            f"{label}.targets[{target_index}].integral",
        )
        eps_order = require_int(
            target.get("eps_order"),
            f"{label}.targets[{target_index}].eps_order",
        )
        key = (integral, eps_order)
        expect(key not in indexed, f"{label} repeats {target_key(*key)}")
        indexed[key] = target
    return indexed


def validate_component(
    *,
    fixture_target: dict[str, Any],
    evidence_target: dict[str, Any],
    cpp_coefficient: dict[str, Any],
    component: str,
    fractional_digits: int,
    label: str,
) -> dict[str, Any]:
    raw_exact = require_string(
        cpp_coefficient.get(f"exact_{component}"),
        f"cpp_result.{label}.exact_{component}",
    )
    expected = require_string(
        fixture_target.get(f"{component}_160"),
        f"extension_fixture.{label}.{component}_160",
    )
    expect(
        fractional_digit_count(expected, f"extension_fixture.{label}.{component}_160")
        == fractional_digits,
        f"{label} {component} fixture must contain exactly {fractional_digits} fractional digits",
    )

    recomputed = render_fractional_digits(raw_exact, fractional_digits)
    expect(
        recomputed == expected,
        f"{label} {component} 160-digit exact rational extension drifted",
    )

    evidence_digits = require_int(
        require_object(
            evidence_target.get("uplifted_serialized_fraction_digits"),
            f"precision_evidence.{label}.uplifted_serialized_fraction_digits",
        ).get(component),
        f"precision_evidence.{label}.uplifted_serialized_fraction_digits.{component}",
    )
    expect(
        evidence_digits == fractional_digits,
        f"{label} {component} precision-evidence uplifted digit count drifted",
    )

    standard_digits = require_int(
        require_object(
            evidence_target.get("standard_serialized_fraction_digits"),
            f"precision_evidence.{label}.standard_serialized_fraction_digits",
        ).get(component),
        f"precision_evidence.{label}.standard_serialized_fraction_digits.{component}",
    )
    standard = require_string(
        require_object(
            evidence_target.get("standard_80"),
            f"precision_evidence.{label}.standard_80",
        ).get(component),
        f"precision_evidence.{label}.standard_80.{component}",
    )
    expect(
        render_fractional_digits(raw_exact, standard_digits) == standard,
        f"{label} {component} standard serialization no longer recomputes from exact rational",
    )
    expect(
        expected.startswith(standard),
        f"{label} {component} 160-digit extension no longer extends standard serialization",
    )

    prefix = visible_prefix(
        require_object(
            evidence_target.get("uplifted_160_prefix"),
            f"precision_evidence.{label}.uplifted_160_prefix",
        ).get(component),
        f"precision_evidence.{label}.uplifted_160_prefix.{component}",
    )
    expect(
        expected.startswith(prefix),
        f"{label} {component} 160-digit fixture no longer matches precision-evidence prefix",
    )

    tail = expected[len(standard):]
    expect(
        any(char.isdigit() and char != "0" for char in tail),
        f"{label} {component} 160-digit extension tail after standard digits must be nonzero",
    )
    return {
        "component": component,
        "standard_fractional_digits": standard_digits,
        "extension_fractional_digits": fractional_digits,
    }


def validate_payloads(
    extension_fixture: dict[str, Any],
    cpp_result: dict[str, Any],
    precision_evidence: dict[str, Any],
) -> dict[str, Any]:
    expect(extension_fixture.get("schema_version") == 1, "extension fixture schema_version must be 1")
    expect(
        extension_fixture.get("fixture_id") == "b61n-exact-rational-extension-160",
        "extension fixture_id drifted",
    )
    fractional_digits = require_int(
        extension_fixture.get("fractional_digits"),
        "extension_fixture.fractional_digits",
    )
    expect(
        fractional_digits == EXPECTED_FRACTIONAL_DIGITS,
        f"extension_fixture.fractional_digits must stay {EXPECTED_FRACTIONAL_DIGITS}",
    )
    expect(cpp_result.get("benchmark_id") == "complex_kinematics", "C++ benchmark_id drifted")
    expect(
        precision_evidence.get("evidence_id") == "b61n-pipeline-precision-evidence",
        "precision evidence_id drifted",
    )
    fixture_cpp_source = require_string(
        extension_fixture.get("source_cpp_result"),
        "extension_fixture.source_cpp_result",
    )
    fixture_precision_source = require_string(
        extension_fixture.get("source_precision_evidence"),
        "extension_fixture.source_precision_evidence",
    )
    precision_cpp_source = require_string(
        precision_evidence.get("source_cpp_result"),
        "precision_evidence.source_cpp_result",
    )
    expect(
        fixture_cpp_source == DEFAULT_SOURCE_CPP_RESULT.as_posix(),
        "extension fixture source_cpp_result drifted from reviewed lane2 C++ result",
    )
    expect(
        fixture_precision_source == DEFAULT_SOURCE_PRECISION_EVIDENCE.as_posix(),
        "extension fixture source_precision_evidence drifted from reviewed lane2 sidecar",
    )
    expect(
        precision_cpp_source == fixture_cpp_source,
        "extension fixture source_cpp_result must match precision evidence source_cpp_result",
    )

    fixture_by_target = index_targets(extension_fixture, "extension_fixture")
    evidence_by_target = index_targets(precision_evidence, "precision_evidence")
    cpp_by_target = index_cpp_coefficients(cpp_result)

    expected_key_set = set(EXPECTED_TARGETS)
    expect(
        set(fixture_by_target) == expected_key_set,
        "extension fixture target set drifted",
    )
    component_records: list[dict[str, Any]] = []
    for key in EXPECTED_TARGETS:
        label = target_key(*key)
        expect(key in cpp_by_target, f"C++ result is missing {label}")
        expect(key in evidence_by_target, f"precision evidence is missing {label}")
        for component in COMPONENTS:
            component_records.append(
                {
                    "integral": key[0],
                    "eps_order": key[1],
                    **validate_component(
                        fixture_target=fixture_by_target[key],
                        evidence_target=evidence_by_target[key],
                        cpp_coefficient=cpp_by_target[key],
                        component=component,
                        fractional_digits=fractional_digits,
                        label=label,
                    ),
                }
            )

    return {
        "schema_version": 1,
        "verifier": "b61n-exact-rational-extension-stability-v1",
        "cross_check_passed": True,
        "target_count": len(EXPECTED_TARGETS),
        "component_count": len(component_records),
        "fractional_digits": fractional_digits,
        "source_cpp_result": fixture_cpp_source,
        "source_precision_evidence": fixture_precision_source,
        "minimum_standard_fractional_digits": min(
            record["standard_fractional_digits"] for record in component_records
        ),
        "m7_closure_claimed": False,
        "release_readiness_claimed": False,
    }


def verify_paths(extension_fixture_path: Path) -> dict[str, Any]:
    root = repo_root()
    extension_fixture = load_json(extension_fixture_path)
    cpp_result_path = resolve_repo_path(
        root,
        require_string(
            extension_fixture.get("source_cpp_result"),
            "extension_fixture.source_cpp_result",
        ),
        "extension_fixture.source_cpp_result",
    )
    precision_evidence_path = resolve_repo_path(
        root,
        require_string(
            extension_fixture.get("source_precision_evidence"),
            "extension_fixture.source_precision_evidence",
        ),
        "extension_fixture.source_precision_evidence",
    )
    summary = validate_payloads(
        extension_fixture,
        load_json(cpp_result_path),
        load_json(precision_evidence_path),
    )
    return {
        **summary,
        "extension_fixture": str(extension_fixture_path),
        "source_cpp_result": str(cpp_result_path),
        "source_precision_evidence": str(precision_evidence_path),
    }


def rejected(
    extension_fixture: dict[str, Any],
    cpp_result: dict[str, Any],
    precision_evidence: dict[str, Any],
    expected_error: str,
) -> bool:
    try:
        validate_payloads(extension_fixture, cpp_result, precision_evidence)
    except Exception as error:  # noqa: BLE001 - self-check intentionally probes failures.
        return expected_error in str(error)
    return False


def synthetic_payloads() -> tuple[dict[str, Any], dict[str, Any], dict[str, Any]]:
    denominator = 10**120
    exact_real = f"{'1234567890' * 12}/{denominator}"
    exact_imag = f"-{'7654321098' * 12}/{denominator}"
    real_standard = render_fractional_digits(exact_real, 80)
    imag_standard = render_fractional_digits(exact_imag, 80)
    real_160 = render_fractional_digits(exact_real, EXPECTED_FRACTIONAL_DIGITS)
    imag_160 = render_fractional_digits(exact_imag, EXPECTED_FRACTIONAL_DIGITS)
    precision_evidence = {
        "evidence_id": "b61n-pipeline-precision-evidence",
        "source_cpp_result": DEFAULT_SOURCE_CPP_RESULT.as_posix(),
        "targets": [
            {
                "integral": "box[1,0,1,1]",
                "eps_order": 0,
                "standard_80": {"real": real_standard, "imag": imag_standard},
                "standard_serialized_fraction_digits": {"real": 80, "imag": 80},
                "uplifted_160_prefix": {
                    "real": f"{real_160[:100]}...",
                    "imag": f"{imag_160[:100]}...",
                },
                "uplifted_serialized_fraction_digits": {
                    "real": EXPECTED_FRACTIONAL_DIGITS,
                    "imag": EXPECTED_FRACTIONAL_DIGITS,
                },
            },
            {
                "integral": "box[1,1,1,1]",
                "eps_order": -2,
                "standard_80": {"real": real_standard, "imag": imag_standard},
                "standard_serialized_fraction_digits": {"real": 80, "imag": 80},
                "uplifted_160_prefix": {
                    "real": f"{real_160[:100]}...",
                    "imag": f"{imag_160[:100]}...",
                },
                "uplifted_serialized_fraction_digits": {
                    "real": EXPECTED_FRACTIONAL_DIGITS,
                    "imag": EXPECTED_FRACTIONAL_DIGITS,
                },
            },
            {
                "integral": "box[1,1,1,1]",
                "eps_order": -1,
                "standard_80": {"real": real_standard, "imag": imag_standard},
                "standard_serialized_fraction_digits": {"real": 80, "imag": 80},
                "uplifted_160_prefix": {
                    "real": f"{real_160[:100]}...",
                    "imag": f"{imag_160[:100]}...",
                },
                "uplifted_serialized_fraction_digits": {
                    "real": EXPECTED_FRACTIONAL_DIGITS,
                    "imag": EXPECTED_FRACTIONAL_DIGITS,
                },
            },
            {
                "integral": "box[1,1,1,1]",
                "eps_order": 0,
                "standard_80": {"real": real_standard, "imag": imag_standard},
                "standard_serialized_fraction_digits": {"real": 80, "imag": 80},
                "uplifted_160_prefix": {
                    "real": f"{real_160[:100]}...",
                    "imag": f"{imag_160[:100]}...",
                },
                "uplifted_serialized_fraction_digits": {
                    "real": EXPECTED_FRACTIONAL_DIGITS,
                    "imag": EXPECTED_FRACTIONAL_DIGITS,
                },
            },
        ],
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
    extension_fixture = {
        "schema_version": 1,
        "fixture_id": "b61n-exact-rational-extension-160",
        "source_cpp_result": DEFAULT_SOURCE_CPP_RESULT.as_posix(),
        "source_precision_evidence": DEFAULT_SOURCE_PRECISION_EVIDENCE.as_posix(),
        "fractional_digits": EXPECTED_FRACTIONAL_DIGITS,
        "targets": [
            {
                "integral": integral,
                "eps_order": eps_order,
                "real_160": real_160,
                "imag_160": imag_160,
            }
            for integral, eps_order in EXPECTED_TARGETS
        ],
    }
    return extension_fixture, cpp_result, precision_evidence


def mutate_last_digit(raw_decimal: str) -> str:
    replacement = "8" if raw_decimal[-1] != "8" else "7"
    return raw_decimal[:-1] + replacement


def run_self_check() -> dict[str, Any]:
    extension_fixture, cpp_result, precision_evidence = synthetic_payloads()
    valid_summary = validate_payloads(extension_fixture, cpp_result, precision_evidence)

    bad_extension = copy.deepcopy(extension_fixture)
    bad_extension["targets"][0]["real_160"] = mutate_last_digit(
        bad_extension["targets"][0]["real_160"]
    )

    bad_prefix = copy.deepcopy(precision_evidence)
    bad_prefix["targets"][0]["uplifted_160_prefix"]["imag"] = "9.999..."

    bad_digit_count = copy.deepcopy(extension_fixture)
    bad_digit_count["fractional_digits"] = 159

    bad_fixture_source = copy.deepcopy(extension_fixture)
    bad_fixture_source["source_cpp_result"] = (
        "tools/reference-harness/specs/m7/lane2/stale-c267.cpp-result.json"
    )

    bad_precision_source = copy.deepcopy(precision_evidence)
    bad_precision_source["source_cpp_result"] = (
        "tools/reference-harness/specs/m7/lane2/stale-c267.cpp-result.json"
    )

    bad_precision_sidecar_source = copy.deepcopy(extension_fixture)
    bad_precision_sidecar_source["source_precision_evidence"] = (
        "tools/reference-harness/specs/m7/lane2/stale-precision-evidence.json"
    )

    checks = {
        "synthetic_fixture_passes": valid_summary["component_count"] == 8,
        "rejects_160_digit_drift": rejected(
            bad_extension,
            cpp_result,
            precision_evidence,
            "160-digit exact rational extension drifted",
        ),
        "rejects_precision_evidence_prefix_drift": rejected(
            extension_fixture,
            cpp_result,
            bad_prefix,
            "precision-evidence prefix",
        ),
        "rejects_fractional_digit_count_drift": rejected(
            bad_digit_count,
            cpp_result,
            precision_evidence,
            "fractional_digits must stay 160",
        ),
        "rejects_fixture_cpp_source_drift": rejected(
            bad_fixture_source,
            cpp_result,
            precision_evidence,
            "source_cpp_result drifted from reviewed lane2 C++ result",
        ),
        "rejects_precision_cpp_source_mismatch": rejected(
            extension_fixture,
            cpp_result,
            bad_precision_source,
            "source_cpp_result must match precision evidence source_cpp_result",
        ),
        "rejects_fixture_precision_sidecar_source_drift": rejected(
            bad_precision_sidecar_source,
            cpp_result,
            precision_evidence,
            "source_precision_evidence drifted from reviewed lane2 sidecar",
        ),
    }
    expect(all(checks.values()), "b61n exact-rational extension stability self-check failed")
    return {
        "schema_version": 1,
        "verifier": "b61n-exact-rational-extension-stability-v1",
        "self_check_passed": True,
        "checks": checks,
    }


def parse_args(argv: list[str]) -> argparse.Namespace:
    root = repo_root()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--extension-fixture-path",
        type=Path,
        default=root / DEFAULT_EXTENSION_FIXTURE,
        help="b61n exact-rational 160-digit extension fixture path",
    )
    parser.add_argument("--self-check", action="store_true", help="Run synthetic rejection checks")
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    try:
        if args.self_check:
            summary = run_self_check()
        else:
            summary = verify_paths(args.extension_fixture_path)
        print_json(summary)
        return 0
    except Exception as error:  # noqa: BLE001 - command-line verifier should fail closed.
        print_json(
            {
                "schema_version": 1,
                "verifier": "b61n-exact-rational-extension-stability-v1",
                "cross_check_passed": False,
                "blocking_reasons": [str(error)],
                "m7_closure_claimed": False,
                "release_readiness_claimed": False,
            }
        )
        return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
