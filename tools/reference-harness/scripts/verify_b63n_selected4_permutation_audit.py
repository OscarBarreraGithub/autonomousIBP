#!/usr/bin/env python3
"""Cross-check the b63n permutation-invariant audit against selected4 parity."""

from __future__ import annotations

import argparse
import json
import sys
from decimal import Decimal, InvalidOperation
from pathlib import Path
from typing import Any


DEFAULT_EVIDENCE = Path(
    "tools/reference-harness/specs/m6/lane146/"
    "b63n-selected4-real-coefficients-evidence.json"
)
DEFAULT_COMPARE = Path(
    "tools/reference-harness/specs/m6/lane146/"
    "automatic_phasespace.selected4-cutkosky.compare30.json"
)
DEFAULT_CPP_RESULT = Path(
    "tools/reference-harness/specs/m6/lane146/"
    "automatic_phasespace.selected4-cutkosky.cpp-result.json"
)

CANONICAL_PERMUTATION_AUDIT = """\
kind=b63n-weighted-residue-moment-cross-validation-gate
gate=passed
reviewed_surface=true
coefficient_free=true
surface=phase[1,2,1,1,1,1,1]
residue_model_kind=automatic_phasespace::one-mass-three-body-residue
coefficient_policy=non-publishing residue-vs-moment cross-validation; verifies the weighted residue plan against synthetic moment seeds only, with no endpoint Laurent coefficient evaluated or published
live_coefficients_available=false
retained_solution_samples_used=false
full_eta_zero_contour_applied=false
moment_weights=D2,D4,D6,D7
publication_gate=blocked-by-publication-gate: all moment seeds remain synthetic and non-publishing
failure_count=0
summary=b63n automatic_phasespace weighted residue plan is cross-validated against the D2,D4,D6,D7 synthetic moment seed packet; no live coefficient, retained final sample, or full eta=0 contour claim is made
"""

EXPECTED_MOMENT_WEIGHTS = ["D2", "D4", "D6", "D7"]
EXPECTED_SELECTED4_ORDERS = {
    "phase[1,-1,1,0,1,0,0]": [0, 1, 2, 3],
    "phase[1,0,1,0,1,0,0]": [0, 1, 2, 3],
    "phase[1,1,1,0,1,0,1]": [0, 1, 2, 3],
    "phase[1,1,1,1,1,1,1]": [-3, -2, -1, 0, 1, 2, 3],
}
PUBLISHED_D7_INTEGRAL = "phase[1,1,1,0,1,0,1]"
EXPECTED_COMPARED_COEFFICIENTS = sum(
    len(orders) for orders in EXPECTED_SELECTED4_ORDERS.values()
)


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


def path_text(path: Path) -> str:
    return str(path.as_posix())


def parse_bool(raw: Any, label: str) -> bool:
    if raw == "true":
        return True
    if raw == "false":
        return False
    raise RuntimeError(f"{label} must be true or false")


def parse_int(raw: Any, label: str) -> int:
    if isinstance(raw, int) and not isinstance(raw, bool):
        return raw
    if isinstance(raw, str):
        return int(raw.strip())
    raise TypeError(f"{label} must be an integer")


def parse_decimal(raw: Any, label: str) -> Decimal:
    expect(isinstance(raw, str), f"{label} must be a decimal string")
    try:
        value = Decimal(raw)
    except InvalidOperation as error:
        raise ValueError(f"{label} must parse as a decimal") from error
    expect(value.is_finite(), f"{label} must be finite")
    return value


def parse_audit_text(text: str) -> dict[str, str]:
    fields: dict[str, str] = {}
    for line_number, raw_line in enumerate(text.splitlines(), start=1):
        line = raw_line.strip()
        if not line:
            continue
        expect("=" in line, f"audit line {line_number} must be key=value")
        key, value = line.split("=", 1)
        expect(key, f"audit line {line_number} has an empty key")
        expect(key not in fields, f"audit repeats key {key}")
        fields[key] = value
    return fields


def require_field(fields: dict[str, str], key: str) -> str:
    expect(key in fields, f"audit missing {key}")
    return fields[key]


def validate_permutation_audit(audit_text: str) -> dict[str, Any]:
    fields = parse_audit_text(audit_text)
    required_exact = {
        "kind": "b63n-weighted-residue-moment-cross-validation-gate",
        "gate": "passed",
        "reviewed_surface": "true",
        "coefficient_free": "true",
        "surface": "phase[1,2,1,1,1,1,1]",
        "residue_model_kind": "automatic_phasespace::one-mass-three-body-residue",
        "live_coefficients_available": "false",
        "retained_solution_samples_used": "false",
        "full_eta_zero_contour_applied": "false",
        "failure_count": "0",
    }
    for key, expected in required_exact.items():
        actual = require_field(fields, key)
        expect(actual == expected, f"audit {key} must be {expected!r}, got {actual!r}")

    moment_weights = require_field(fields, "moment_weights").split(",")
    expect(
        moment_weights == EXPECTED_MOMENT_WEIGHTS,
        "audit moment_weights must be canonical D2,D4,D6,D7",
    )
    expect(
        require_field(fields, "publication_gate").startswith("blocked-by-publication-gate"),
        "audit publication_gate must remain blocked by publication gate",
    )
    expect(
        "synthetic" in fields["publication_gate"],
        "audit publication gate must identify synthetic moment seeds",
    )
    summary = require_field(fields, "summary")
    for required_text in ("no live coefficient", "retained final sample", "full eta=0 contour"):
        expect(required_text in summary, f"audit summary must include {required_text!r}")

    return {
        "audit_valid": True,
        "surface": fields["surface"],
        "moment_weights": moment_weights,
        "live_coefficients_available": parse_bool(
            fields["live_coefficients_available"],
            "audit.live_coefficients_available",
        ),
        "retained_solution_samples_used": parse_bool(
            fields["retained_solution_samples_used"],
            "audit.retained_solution_samples_used",
        ),
        "full_eta_zero_contour_applied": parse_bool(
            fields["full_eta_zero_contour_applied"],
            "audit.full_eta_zero_contour_applied",
        ),
        "failure_count": parse_int(fields["failure_count"], "audit.failure_count"),
    }


def require_list(raw: Any, label: str) -> list[Any]:
    expect(isinstance(raw, list), f"{label} must be a list")
    return raw


def require_object(raw: Any, label: str) -> dict[str, Any]:
    expect(isinstance(raw, dict), f"{label} must be an object")
    return raw


def rows_by_label(rows: Any, key: str, label: str) -> dict[str, dict[str, Any]]:
    indexed: dict[str, dict[str, Any]] = {}
    for index, raw_row in enumerate(require_list(rows, label)):
        row = require_object(raw_row, f"{label}[{index}]")
        row_label = row.get(key)
        expect(isinstance(row_label, str) and row_label, f"{label}[{index}] missing {key}")
        expect(row_label not in indexed, f"duplicate {label} row {row_label}")
        indexed[row_label] = row
    return indexed


def coefficients_by_order(raw_coefficients: Any, label: str) -> dict[int, dict[str, Any]]:
    indexed: dict[int, dict[str, Any]] = {}
    for index, raw_coefficient in enumerate(require_list(raw_coefficients, label)):
        coefficient = require_object(raw_coefficient, f"{label}[{index}]")
        order = parse_int(coefficient.get("order"), f"{label}[{index}].order")
        expect(order not in indexed, f"duplicate coefficient order {order} in {label}")
        indexed[order] = coefficient
    return indexed


def validate_evidence_sidecar(evidence: dict[str, Any], compare_path: Path) -> dict[str, Any]:
    expect(evidence.get("schema_version") == 1, "selected4 evidence schema_version must be 1")
    expect(evidence.get("runtime_lane") == "b63n", "selected4 evidence runtime_lane must be b63n")
    expect(
        evidence.get("benchmark_id") == "automatic_phasespace",
        "selected4 evidence benchmark must be automatic_phasespace",
    )
    expect(evidence.get("lane") == "lane146", "selected4 evidence lane must be lane146")
    expect(evidence.get("transport_applied") is True, "selected4 transport must be applied")
    expect(
        evidence.get("transport_scope") == "eta-zero-selected-endpoint-coefficients",
        "selected4 transport scope must remain selected-endpoint scoped",
    )
    expect(
        evidence.get("eta_zero_endpoint_transport_applied") is True,
        "selected4 eta-zero endpoint transport must be true",
    )
    expect(
        evidence.get("final_solution_samples_used_as_input") is False,
        "selected4 evidence must reject final solution samples as input",
    )
    expect(
        evidence.get("full_eta_zero_contour_applied") is False,
        "selected4 evidence must not claim full eta-zero contour",
    )
    transported = require_list(
        evidence.get("eta_zero_endpoint_transported_integrals"),
        "selected4 eta_zero_endpoint_transported_integrals",
    )
    expect(
        PUBLISHED_D7_INTEGRAL in transported,
        "selected4 evidence must transport the published D7 weighted integral",
    )

    comparator = require_object(evidence.get("comparator"), "selected4 comparator")
    expect(comparator.get("path") == path_text(compare_path), "selected4 comparator path drifted")
    expect(comparator.get("passed") is True, "selected4 comparator must pass")
    expect(
        parse_int(comparator.get("tolerance_digits"), "selected4 comparator tolerance") == 30,
        "selected4 comparator must retain compare30 tolerance",
    )
    expect(
        parse_int(
            comparator.get("compared_coefficient_count"),
            "selected4 comparator compared count",
        )
        == EXPECTED_COMPARED_COEFFICIENTS,
        "selected4 comparator compared count must stay at 19",
    )
    expect(
        parse_int(
            comparator.get("passed_coefficient_count"),
            "selected4 comparator passed count",
        )
        == EXPECTED_COMPARED_COEFFICIENTS,
        "selected4 comparator passed count must stay at 19",
    )
    expect(
        parse_int(
            comparator.get("minimum_digit_agreement"),
            "selected4 comparator minimum digits",
        )
        == 999,
        "selected4 comparator minimum digit agreement must remain exact-literal",
    )
    per_integral = require_object(
        comparator.get("per_integral_minimum_digit_agreement"),
        "selected4 comparator per_integral_minimum_digit_agreement",
    )
    expect(
        set(per_integral) == set(EXPECTED_SELECTED4_ORDERS),
        "selected4 comparator integral scope drifted",
    )
    for integral in EXPECTED_SELECTED4_ORDERS:
        expect(per_integral[integral] == 999, f"{integral} must retain 999 digit agreement")

    return {
        "evidence_valid": True,
        "runtime_lane": evidence["runtime_lane"],
        "benchmark_id": evidence["benchmark_id"],
        "transported_integrals": transported,
    }


def validate_compare_and_cpp(
    compare: dict[str, Any],
    cpp_result: dict[str, Any],
    evidence: dict[str, Any],
) -> dict[str, Any]:
    expect(compare.get("schema_version") == 1, "compare schema_version must be 1")
    expect(compare.get("comparison") == "cpp-vs-amflow", "compare must be cpp-vs-amflow")
    expect(compare.get("benchmark_id") == "automatic_phasespace", "compare benchmark drifted")
    expect(compare.get("passed") is True, "compare must pass")
    expect(compare.get("failures") == [], "compare must not retain failures")
    expect(compare.get("amflow_golden") == evidence.get("amflow_golden_slice"), "golden path drifted")
    expect(compare.get("cpp_result") == evidence.get("cpp_result"), "cpp result path drifted")
    tolerance_digits = parse_int(compare.get("tolerance_digits"), "compare tolerance_digits")
    expect(tolerance_digits == 30, "compare tolerance_digits must remain 30")
    expect(
        parse_int(compare.get("matched_integral_count"), "compare matched_integral_count")
        == len(EXPECTED_SELECTED4_ORDERS),
        "compare matched integral count drifted",
    )
    expect(
        parse_int(compare.get("compared_coefficient_count"), "compare compared count")
        == EXPECTED_COMPARED_COEFFICIENTS,
        "compare coefficient count drifted",
    )
    expect(
        parse_int(compare.get("passed_coefficient_count"), "compare passed count")
        == EXPECTED_COMPARED_COEFFICIENTS,
        "compare passed coefficient count drifted",
    )

    expect(cpp_result.get("schema_version") == 1, "cpp result schema_version must be 1")
    expect(cpp_result.get("benchmark_id") == "automatic_phasespace", "cpp benchmark drifted")
    expect(cpp_result.get("family") == "phase", "cpp family drifted")
    continuation = require_object(cpp_result.get("continuation"), "cpp continuation")
    expect(continuation.get("transport_applied") is True, "cpp transport must be applied")
    expect(
        continuation.get("transport_scope") == "eta-zero-selected-endpoint-coefficients",
        "cpp transport scope must remain selected-endpoint scoped",
    )
    expect(
        continuation.get("full_eta_zero_contour_applied") is False,
        "cpp result must not claim full eta-zero contour",
    )

    compare_rows = rows_by_label(compare.get("integrals"), "integral", "compare integrals")
    cpp_rows = rows_by_label(cpp_result.get("results"), "integral", "cpp results")
    expect(set(compare_rows) == set(EXPECTED_SELECTED4_ORDERS), "compare integral set drifted")
    expect(
        set(EXPECTED_SELECTED4_ORDERS).issubset(set(cpp_rows)),
        "cpp result missing one or more selected4 integrals",
    )

    observed_count = 0
    observed_minimum_digits = 999
    d7_orders: list[int] = []
    for integral, expected_orders in EXPECTED_SELECTED4_ORDERS.items():
        compare_coefficients = coefficients_by_order(
            compare_rows[integral].get("coefficients"),
            f"compare coefficients {integral}",
        )
        cpp_coefficients = coefficients_by_order(
            cpp_rows[integral].get("epsilon_orders"),
            f"cpp coefficients {integral}",
        )
        expect(
            sorted(compare_coefficients) == expected_orders,
            f"compare coefficient order scope drifted for {integral}",
        )
        for order in expected_orders:
            coefficient = compare_coefficients[order]
            cpp_coefficient = cpp_coefficients.get(order)
            expect(cpp_coefficient is not None, f"cpp result missing {integral} eps^{order}")
            expect(coefficient.get("amflow_present") is True, f"{integral} eps^{order} missing AMFlow")
            expect(coefficient.get("cpp_present") is True, f"{integral} eps^{order} missing C++")
            expect(coefficient.get("passed") is True, f"{integral} eps^{order} did not pass")
            real_digits = parse_int(
                coefficient.get("real_agreement_digits"),
                f"{integral} eps^{order} real_agreement_digits",
            )
            imag_digits = parse_int(
                coefficient.get("imag_agreement_digits"),
                f"{integral} eps^{order} imag_agreement_digits",
            )
            expect(real_digits >= tolerance_digits, f"{integral} eps^{order} real digits too low")
            expect(imag_digits >= tolerance_digits, f"{integral} eps^{order} imaginary digits too low")
            observed_minimum_digits = min(observed_minimum_digits, real_digits, imag_digits)

            amflow_real = parse_decimal(coefficient.get("amflow_real"), f"{integral} eps^{order} amflow_real")
            amflow_imag = parse_decimal(coefficient.get("amflow_imag"), f"{integral} eps^{order} amflow_imag")
            cpp_real = parse_decimal(coefficient.get("cpp_real"), f"{integral} eps^{order} cpp_real")
            cpp_imag = parse_decimal(coefficient.get("cpp_imag"), f"{integral} eps^{order} cpp_imag")
            expect(cpp_real == amflow_real, f"{integral} eps^{order} real value drifted")
            expect(cpp_imag == amflow_imag, f"{integral} eps^{order} imaginary value drifted")
            expect(
                parse_decimal(cpp_coefficient.get("real_digits"), f"{integral} eps^{order} cpp real_digits")
                == cpp_real,
                f"{integral} eps^{order} cpp real_digits mismatch",
            )
            expect(
                parse_decimal(cpp_coefficient.get("imag_digits"), f"{integral} eps^{order} cpp imag_digits")
                == cpp_imag,
                f"{integral} eps^{order} cpp imag_digits mismatch",
            )
            if integral == PUBLISHED_D7_INTEGRAL:
                d7_orders.append(order)
            observed_count += 1

    expect(observed_count == EXPECTED_COMPARED_COEFFICIENTS, "observed selected4 count drifted")
    expect(
        observed_minimum_digits == parse_int(compare.get("minimum_digit_agreement"), "compare minimum"),
        "top-level compare minimum digit agreement does not match coefficient floor",
    )
    expect(d7_orders == [0, 1, 2, 3], "published D7 selected4 order scope drifted")
    return {
        "compare_valid": True,
        "compared_coefficient_count": observed_count,
        "minimum_digit_agreement": observed_minimum_digits,
        "published_d7_integral": PUBLISHED_D7_INTEGRAL,
        "published_d7_orders": d7_orders,
    }


def verify_payloads(
    audit_text: str,
    evidence: dict[str, Any],
    compare: dict[str, Any],
    cpp_result: dict[str, Any],
    *,
    compare_path: Path,
) -> dict[str, Any]:
    audit_summary = validate_permutation_audit(audit_text)
    evidence_summary = validate_evidence_sidecar(evidence, compare_path)
    compare_summary = validate_compare_and_cpp(compare, cpp_result, evidence)

    expect(
        audit_summary["surface"] == "phase[1,2,1,1,1,1,1]",
        "audit surface must remain the D2/D4/D6/D7 weighted target",
    )
    expect(
        PUBLISHED_D7_INTEGRAL in evidence_summary["transported_integrals"],
        "published D7 parity integral must be present in transported selected4 corpus",
    )
    expect(
        "D7" in audit_summary["moment_weights"],
        "permutation-invariant audit must include D7 before checking D7 parity",
    )
    expect(
        audit_summary["live_coefficients_available"] is False
        and audit_summary["retained_solution_samples_used"] is False
        and audit_summary["full_eta_zero_contour_applied"] is False,
        "permutation audit must remain non-publishing while parity corpus carries published D7",
    )
    return {
        "schema_version": 1,
        "verifier": "b63n-selected4-permutation-audit-cross-check-v1",
        "cross_check_passed": True,
        "audit": audit_summary,
        "selected4_evidence": evidence_summary,
        "selected4_compare": compare_summary,
        "m6_closure_claimed": False,
        "m7_closure_claimed": False,
    }


def verify_paths(
    audit_path: Path | None,
    evidence_path: Path,
    compare_path: Path,
    cpp_result_path: Path,
) -> dict[str, Any]:
    if audit_path is None:
        audit_text = CANONICAL_PERMUTATION_AUDIT
    else:
        audit_text = audit_path.read_text(encoding="utf-8")
    return verify_payloads(
        audit_text,
        load_json(evidence_path),
        load_json(compare_path),
        load_json(cpp_result_path),
        compare_path=compare_path.relative_to(repo_root()) if compare_path.is_absolute() else compare_path,
    )


def sample_coefficient(order: int, real: str) -> dict[str, Any]:
    return {
        "order": order,
        "amflow_present": True,
        "cpp_present": True,
        "passed": True,
        "amflow_real": real,
        "amflow_imag": "0",
        "cpp_real": real,
        "cpp_imag": "0E-80",
        "real_agreement_digits": 999,
        "imag_agreement_digits": 999,
    }


def sample_payloads() -> tuple[dict[str, Any], dict[str, Any], dict[str, Any]]:
    compare_integrals = []
    cpp_results = []
    for integral_index, (integral, orders) in enumerate(EXPECTED_SELECTED4_ORDERS.items(), start=1):
        compare_coefficients = []
        cpp_orders = []
        for order in orders:
            real = f"{integral_index}.{order + 10}25"
            compare_coefficients.append(sample_coefficient(order, real))
            cpp_orders.append({"order": order, "real_digits": real, "imag_digits": "0"})
        compare_integrals.append({
            "integral": integral,
            "status": "compared",
            "coefficients": compare_coefficients,
        })
        cpp_results.append({"integral": integral, "epsilon_orders": cpp_orders})

    compare = {
        "schema_version": 1,
        "comparison": "cpp-vs-amflow",
        "benchmark_id": "automatic_phasespace",
        "amflow_golden": "tools/reference-harness/specs/m6/lane146/automatic_phasespace.selected4-cutkosky.amflow-golden.txt",
        "cpp_result": path_text(DEFAULT_CPP_RESULT),
        "passed": True,
        "failures": [],
        "tolerance_digits": 30,
        "matched_integral_count": len(EXPECTED_SELECTED4_ORDERS),
        "compared_coefficient_count": EXPECTED_COMPARED_COEFFICIENTS,
        "passed_coefficient_count": EXPECTED_COMPARED_COEFFICIENTS,
        "minimum_digit_agreement": 999,
        "integrals": compare_integrals,
    }
    evidence = {
        "schema_version": 1,
        "runtime_lane": "b63n",
        "benchmark_id": "automatic_phasespace",
        "lane": "lane146",
        "amflow_golden_slice": compare["amflow_golden"],
        "cpp_result": compare["cpp_result"],
        "transport_applied": True,
        "transport_scope": "eta-zero-selected-endpoint-coefficients",
        "eta_zero_endpoint_transport_applied": True,
        "eta_zero_endpoint_transported_integrals": list(EXPECTED_SELECTED4_ORDERS),
        "final_solution_samples_used_as_input": False,
        "full_eta_zero_contour_applied": False,
        "comparator": {
            "path": path_text(DEFAULT_COMPARE),
            "passed": True,
            "tolerance_digits": 30,
            "matched_integral_count": len(EXPECTED_SELECTED4_ORDERS),
            "compared_coefficient_count": EXPECTED_COMPARED_COEFFICIENTS,
            "passed_coefficient_count": EXPECTED_COMPARED_COEFFICIENTS,
            "minimum_digit_agreement": 999,
            "per_integral_minimum_digit_agreement": {
                integral: 999 for integral in EXPECTED_SELECTED4_ORDERS
            },
        },
    }
    cpp_result = {
        "schema_version": 1,
        "benchmark_id": "automatic_phasespace",
        "family": "phase",
        "continuation": {
            "transport_applied": True,
            "transport_scope": "eta-zero-selected-endpoint-coefficients",
            "full_eta_zero_contour_applied": False,
        },
        "results": cpp_results,
    }
    return evidence, compare, cpp_result


def rejected(
    audit_text: str,
    evidence: dict[str, Any],
    compare: dict[str, Any],
    cpp_result: dict[str, Any],
    expected: str,
) -> bool:
    try:
        verify_payloads(
            audit_text,
            evidence,
            compare,
            cpp_result,
            compare_path=DEFAULT_COMPARE,
        )
    except Exception as error:  # noqa: BLE001 - self-check records fail-closed behavior.
        return expected in str(error)
    return False


def run_self_check() -> dict[str, Any]:
    evidence, compare, cpp_result = sample_payloads()
    good = verify_payloads(
        CANONICAL_PERMUTATION_AUDIT,
        evidence,
        compare,
        cpp_result,
        compare_path=DEFAULT_COMPARE,
    )

    bad_audit = CANONICAL_PERMUTATION_AUDIT.replace(
        "moment_weights=D2,D4,D6,D7",
        "moment_weights=D7,D6,D4,D2",
    )
    bad_evidence = json.loads(json.dumps(evidence))
    bad_evidence["final_solution_samples_used_as_input"] = True
    bad_compare = json.loads(json.dumps(compare))
    bad_compare["integrals"][2]["coefficients"][1]["passed"] = False
    bad_cpp_result = json.loads(json.dumps(cpp_result))
    bad_cpp_result["continuation"]["full_eta_zero_contour_applied"] = True

    checks = {
        "canonical_fixture_passes": good["cross_check_passed"] is True,
        "rejects_noncanonical_moment_order": rejected(
            bad_audit,
            evidence,
            compare,
            cpp_result,
            "canonical D2,D4,D6,D7",
        ),
        "rejects_final_solution_sample_input": rejected(
            CANONICAL_PERMUTATION_AUDIT,
            bad_evidence,
            compare,
            cpp_result,
            "reject final solution samples",
        ),
        "rejects_failed_compare_coefficient": rejected(
            CANONICAL_PERMUTATION_AUDIT,
            evidence,
            bad_compare,
            cpp_result,
            "did not pass",
        ),
        "rejects_full_contour_overclaim": rejected(
            CANONICAL_PERMUTATION_AUDIT,
            evidence,
            compare,
            bad_cpp_result,
            "must not claim full eta-zero contour",
        ),
    }
    expect(all(checks.values()), "b63n selected4 permutation audit verifier self-check failed")
    return {
        "schema_version": 1,
        "verifier": "b63n-selected4-permutation-audit-cross-check-v1",
        "self_check_passed": True,
        "checks": checks,
    }


def parse_args(argv: list[str]) -> argparse.Namespace:
    root = repo_root()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--audit-path",
        type=Path,
        help="Optional text file containing SerializeCutkoskyWeightedResidueMomentCrossValidationGateAudit output",
    )
    parser.add_argument(
        "--selected4-evidence-path",
        type=Path,
        default=root / DEFAULT_EVIDENCE,
        help="b63n selected4 real-coefficients evidence JSON path",
    )
    parser.add_argument(
        "--compare-path",
        type=Path,
        default=root / DEFAULT_COMPARE,
        help="lane146 selected4 compare30 JSON path",
    )
    parser.add_argument(
        "--cpp-result-path",
        type=Path,
        default=root / DEFAULT_CPP_RESULT,
        help="lane146 selected4 C++ result JSON path",
    )
    parser.add_argument("--summary-path", type=Path, help="Optional JSON output path")
    parser.add_argument("--self-check", action="store_true", help="Run synthetic verifier checks")
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    try:
        if args.self_check:
            summary = run_self_check()
        else:
            summary = verify_paths(
                args.audit_path,
                args.selected4_evidence_path,
                args.compare_path,
                args.cpp_result_path,
            )
        if args.summary_path is not None:
            write_json(args.summary_path, summary)
        print_json(summary)
        return 0
    except Exception as error:  # noqa: BLE001 - command-line verifier should fail closed.
        summary = {
            "schema_version": 1,
            "verifier": "b63n-selected4-permutation-audit-cross-check-v1",
            "cross_check_passed": False,
            "blocking_reasons": [str(error)],
            "m6_closure_claimed": False,
            "m7_closure_claimed": False,
        }
        if args.summary_path is not None:
            write_json(args.summary_path, summary)
        print_json(summary)
        return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
