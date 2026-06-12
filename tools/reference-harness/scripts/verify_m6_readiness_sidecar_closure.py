#!/usr/bin/env python3
"""Verify the accepted M6 readiness sidecar against its closure evidence."""

from __future__ import annotations

import argparse
import copy
from decimal import Decimal, InvalidOperation
import hashlib
import json
from pathlib import Path
from typing import Any

from audit_b64ag_golden_recapture_readiness import (
    EXPECTED_ORDERS,
    EXPECTED_PACKET_TARGETS,
    REQUIRED_COEFFICIENT_ROWS,
    REQUIRED_DIGITS,
    REQUIRED_EPSILON_SAMPLES,
    audit_b64ag_golden_recapture_readiness,
)
from compare_cpp_vs_amflow import compare_cpp_vs_amflow
from qualify_milestone_m6 import summarize_milestone_m6_qualification


M6_QUALIFICATION = Path("tools/reference-harness/specs/m7/lane133/m6-qualification.json")
PHASE0_QUALIFICATION = Path("tools/reference-harness/specs/m7/lane133/phase0-qualification.json")
CASE_STUDY_QUALIFICATION = Path(
    "tools/reference-harness/specs/m7/lane115/case-study-qualification.json"
)
RELEASE_READINESS_OUTPUTS = (
    Path("tools/reference-harness/specs/m7/lane3/release-readiness.m5-accepted.full-output.json"),
    Path("tools/reference-harness/specs/m7/lane3/release-readiness.post-a1f0e1d.full-output.json"),
)
B64AG_EVIDENCE = Path(
    "tools/reference-harness/specs/m6/lane1-next24/b64ag-50digit-per-coefficient-evidence.json"
)
B64AG_READINESS = Path(
    "tools/reference-harness/specs/m6/lane1-next24/b64ag-golden-recapture-readiness.json"
)
B64AG_CPP_RESULT = Path(
    "tools/reference-harness/specs/m6/lane1-next24/"
    "linear_propagator.b64ag-full-packet-finite-part.cpp-result.json"
)
B64AG_COMPARE50 = Path(
    "tools/reference-harness/specs/m6/lane1-next24/"
    "linear_propagator.b64ag-full-packet-finite-part.compare50.json"
)
B64AG_AMFLOW_STATE = Path("tools/reference-harness/specs/phase0/linear_propagator.amflow-state.json")
B64AG_AMFLOW_GOLDEN = Path(
    "tools/reference-harness/specs/phase0/linear_propagator.golden-manifest.json"
)
B64AG_REVIEWED_FIRST_BLOCK_GOLDEN_SLICE = Path(
    "tools/reference-harness/specs/m6/lane3-next20/"
    "linear_propagator.first-block-cache-fit.amflow-golden.txt"
)
B64AG_SOURCE_COMMIT = "c10724f6b74c2f54353a610023d3b6eac9bd45ca"
B64AG_EVIDENCE_BASELINE_COMMIT = "6976c084c9536cf7867d97c5ab4a4adda3481f43"
M6_CLOSURE_COMMIT = "e69a8c8094fbc6c58622f0d40da75e1a86b4b45a"
M6_SOURCE_PROVENANCE_SHA256 = "840ad1bd00cd8712ae63ad62e1cfbe54530245b8d49d65bd07c271973345413a"
IGNORED_M6_PROVENANCE_FIELDS = {
    "source_commit",
    "source_provenance_sha256",
    "post_closure_runtime_lane_evidence",
}
B64AG_CLOSURE_ARTIFACT_SHA256 = {
    "amflow_golden_manifest": "48dbd7f42ba35152210e6104645829580f3f7fb145c103c5fd9d68732689604e",
    "amflow_state": "630478583cda5c04037e40738a2209df98b09e1fae04b707a3b6ae786178eb00",
    "compare50": "afe6c9150b1aeba162c55b4c6ece412b3a784e8c0916e905f1148a6e8dedaf9a",
    "cpp_result": "50fed21d1c47256c0a29ae5451865804227c453f2ecb1b1a0ae1434c221dfaff",
    "readiness_summary": "f6c66db33a16b86fbefd8cc64ce33d59a553e1099222222fd63290035d4e0573",
    "reviewed_first_block_golden_slice": "65a61b92f264b44b802cc6b41f5906fd71d2a1bbe987c959f98ccc6c2885dcd4",
}


class ClosureAuditError(RuntimeError):
    """Raised when accepted M6 readiness evidence is internally inconsistent."""


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise ClosureAuditError(message)


def load_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as stream:
        payload = json.load(stream)
    expect(isinstance(payload, dict), f"{path} must contain a JSON object")
    return payload


def write_json(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def normalized_m6_summary(payload: dict[str, Any]) -> dict[str, Any]:
    return {
        key: value
        for key, value in payload.items()
        if key not in IGNORED_M6_PROVENANCE_FIELDS
    }


def require_path(raw: Any, expected: Path, label: str) -> None:
    expect(raw == str(expected), f"{label} must be {expected}, got {raw!r}")


def require_object(payload: dict[str, Any], field: str, label: str) -> dict[str, Any]:
    value = payload.get(field)
    expect(isinstance(value, dict), f"{label}.{field} must be an object")
    return value


def require_list(payload: dict[str, Any], field: str, label: str) -> list[Any]:
    value = payload.get(field)
    expect(isinstance(value, list), f"{label}.{field} must be a list")
    return value


def require_string(payload: dict[str, Any], field: str, label: str) -> str:
    value = payload.get(field)
    expect(isinstance(value, str) and value.strip(), f"{label}.{field} must be a non-empty string")
    return value


def require_bool(payload: dict[str, Any], field: str, label: str) -> bool:
    value = payload.get(field)
    expect(type(value) is bool, f"{label}.{field} must be a bool")
    return value


def require_int(payload: dict[str, Any], field: str, label: str) -> int:
    value = payload.get(field)
    expect(type(value) is int, f"{label}.{field} must be an int")
    return value


def require_string_list(payload: dict[str, Any], field: str, label: str) -> list[str]:
    values = require_list(payload, field, label)
    for index, value in enumerate(values):
        expect(
            isinstance(value, str) and value.strip(),
            f"{label}.{field}[{index}] must be a non-empty string",
        )
    return values


def resolve_json_pointer(document: Any, pointer: str, label: str) -> Any:
    expect(isinstance(pointer, str), f"{label} must be a JSON pointer string")
    if pointer.startswith("#/"):
        pointer = pointer[1:]
    expect(pointer.startswith("/"), f"{label} must start with '/'")
    current = document
    if pointer == "/":
        return current
    for raw_token in pointer.split("/")[1:]:
        token = raw_token.replace("~1", "/").replace("~0", "~")
        if isinstance(current, list):
            expect(token.isdigit(), f"{label} list token must be numeric: {token!r}")
            index = int(token)
            expect(index < len(current), f"{label} list token out of range: {index}")
            current = current[index]
            continue
        expect(isinstance(current, dict), f"{label} traversed into non-container")
        expect(token in current, f"{label} missing object key: {token}")
        current = current[token]
    return current


def decimal_strings_equal(left: Any, right: Any) -> bool:
    if not isinstance(left, str) or not isinstance(right, str):
        return False
    try:
        return Decimal(left) == Decimal(right)
    except InvalidOperation:
        return left == right


def require_ready_m6_summary(root: Path, m6_summary: dict[str, Any]) -> None:
    expect(m6_summary.get("scope") == "milestone-m6-qualification", "M6 summary scope drifted")
    expect(m6_summary.get("current_state") == "milestone-m6-qualified", "M6 summary is not qualified")
    expect(m6_summary.get("milestone_m6_ready") is True, "M6 ready flag is not true")
    expect(m6_summary.get("phase0_packet_set_qualified") is True, "phase0 packet set is not qualified")
    expect(m6_summary.get("phase0_ready_for_m6") is True, "phase0_ready_for_m6 is not true")
    expect(
        m6_summary.get("phase0_pending_runtime_lanes_closed") is True,
        "phase0 pending runtime lanes are not closed",
    )
    expect(m6_summary.get("case_study_families_qualified") is True, "case studies are not qualified")
    expect(m6_summary.get("case_study_ready_for_m6") is True, "case_study_ready_for_m6 is not true")
    expect(m6_summary.get("phase0_pending_ids") == [], "ready M6 summary has phase0_pending_ids")
    expect(
        m6_summary.get("blocked_phase0_examples") == [],
        "ready M6 summary has blocked_phase0_examples",
    )
    expect(m6_summary.get("blocked_runtime_lanes") == [], "ready M6 summary has runtime blockers")
    expect(m6_summary.get("blocking_reasons") == [], "ready M6 summary has blocking reasons")
    expect(m6_summary.get("source_commit") == M6_CLOSURE_COMMIT, "M6 source commit drifted")
    expect(
        m6_summary.get("source_provenance_sha256") == M6_SOURCE_PROVENANCE_SHA256,
        "M6 source provenance digest drifted",
    )
    require_path(
        m6_summary.get("phase0_qualification_summary_path"),
        PHASE0_QUALIFICATION,
        "phase0_qualification_summary_path",
    )
    require_path(
        m6_summary.get("case_study_qualification_summary_path"),
        CASE_STUDY_QUALIFICATION,
        "case_study_qualification_summary_path",
    )

    recomputed = summarize_milestone_m6_qualification(
        phase0_qualification_summary_path=root / PHASE0_QUALIFICATION,
        case_study_qualification_summary_path=root / CASE_STUDY_QUALIFICATION,
    )
    recomputed["phase0_qualification_summary_path"] = str(PHASE0_QUALIFICATION)
    recomputed["case_study_qualification_summary_path"] = str(CASE_STUDY_QUALIFICATION)
    expect(
        normalized_m6_summary(m6_summary) == recomputed,
        "M6 qualification summary no longer matches its phase0/case-study inputs",
    )


def require_release_surface_uses_accepted_m6(root: Path) -> None:
    for path in RELEASE_READINESS_OUTPUTS:
        payload = load_json(root / path)
        require_path(payload.get("m6_qualification_summary_path"), M6_QUALIFICATION, f"{path}")
        prereqs = payload.get("release_prerequisites")
        expect(isinstance(prereqs, list), f"{path} release_prerequisites must be a list")
        milestone_entries = [
            entry for entry in prereqs if isinstance(entry, dict) and entry.get("id") == "milestone-m6"
        ]
        expect(len(milestone_entries) == 1, f"{path} must carry one milestone-m6 prerequisite")
        milestone = milestone_entries[0]
        expect(milestone.get("satisfied") is True, f"{path} milestone-m6 prerequisite is not satisfied")
        expect(
            milestone.get("current_state") == "reviewed-and-accepted-m6-packet",
            f"{path} milestone-m6 prerequisite state drifted",
        )
        expect(milestone.get("blockers") == [], f"{path} milestone-m6 prerequisite has blockers")


def require_source_artifact_hashes(
    root: Path,
    evidence: dict[str, Any],
    closure_hashes: dict[str, str] | None = None,
) -> None:
    artifacts = evidence.get("source_artifacts")
    expect(isinstance(artifacts, dict), "b64ag evidence source_artifacts must be an object")
    expected_paths = {
        "amflow_golden_manifest": B64AG_AMFLOW_GOLDEN,
        "amflow_state": B64AG_AMFLOW_STATE,
        "compare50": B64AG_COMPARE50,
        "cpp_result": B64AG_CPP_RESULT,
        "readiness_summary": B64AG_READINESS,
        "reviewed_first_block_golden_slice": B64AG_REVIEWED_FIRST_BLOCK_GOLDEN_SLICE,
    }
    expected_hashes = closure_hashes or B64AG_CLOSURE_ARTIFACT_SHA256
    for field, expected_path in expected_paths.items():
        require_path(artifacts.get(field), expected_path, f"source_artifacts.{field}")
        observed_sha = sha256_file(root / expected_path)
        expect(
            artifacts.get(f"{field}_sha256") == observed_sha,
            f"source_artifacts.{field}_sha256 does not match {expected_path}",
        )
        expect(
            expected_hashes.get(field) == observed_sha,
            f"{expected_path} drifted from b64ag closure packet {B64AG_SOURCE_COMMIT}",
        )


def comparison_rows(summary: dict[str, Any]) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    integrals = summary.get("integrals")
    expect(isinstance(integrals, list), "comparison integrals must be a list")
    for integral in integrals:
        expect(isinstance(integral, dict), "comparison integral entries must be objects")
        integral_name = integral.get("integral")
        expect(isinstance(integral_name, str) and integral_name, "comparison integral name missing")
        coefficients = integral.get("coefficients")
        expect(isinstance(coefficients, list), f"{integral_name} coefficients must be a list")
        for coefficient in coefficients:
            expect(isinstance(coefficient, dict), f"{integral_name} coefficient must be an object")
            rows.append(
                {
                    "integral": integral_name,
                    "order": coefficient.get("order"),
                    "cpp_real": coefficient.get("cpp_real"),
                    "cpp_imag": coefficient.get("cpp_imag"),
                    "amflow_real": coefficient.get("amflow_real"),
                    "amflow_imag": coefficient.get("amflow_imag"),
                    "cpp_present": coefficient.get("cpp_present"),
                    "amflow_present": coefficient.get("amflow_present"),
                    "real_agreement_digits": coefficient.get("real_agreement_digits"),
                    "imag_agreement_digits": coefficient.get("imag_agreement_digits"),
                    "passed": coefficient.get("passed"),
                }
            )
    return rows


def require_semantic_comparison_match(
    committed: dict[str, Any],
    recomputed: dict[str, Any],
) -> None:
    for field in (
        "benchmark_id",
        "comparison",
        "cpp_result",
        "amflow_golden",
        "amflow_state",
        "tolerance_digits",
        "matched_integral_count",
        "compared_coefficient_count",
        "passed_coefficient_count",
        "minimum_digit_agreement",
        "passed",
        "failures",
    ):
        expect(committed.get(field) == recomputed.get(field), f"comparison field drifted: {field}")
    expect(committed.get("passed") is True, "committed b64ag comparison is not passing")
    expect(committed.get("compared_coefficient_count") == 57, "b64ag compared row count drifted")
    expect(committed.get("passed_coefficient_count") == 57, "b64ag passed row count drifted")
    expect((committed.get("minimum_digit_agreement") or 0) >= 50, "b64ag digit floor drifted")
    expect(
        comparison_rows(committed) == comparison_rows(recomputed),
        "b64ag per-coefficient comparison rows drifted from a fresh semantic recompare",
    )


def require_b64ag_runtime_provenance(evidence: dict[str, Any]) -> None:
    provenance = require_object(evidence, "runtime_provenance", "b64ag evidence")
    expect(
        require_bool(provenance, "final_solution_samples_used_as_input", "b64ag runtime_provenance")
        is False,
        "b64ag runtime_provenance.final_solution_samples_used_as_input must be false",
    )
    expect(
        require_bool(provenance, "full_eta_zero_contour_applied", "b64ag runtime_provenance")
        is True,
        "b64ag runtime_provenance.full_eta_zero_contour_applied must be true",
    )
    expect(
        require_bool(provenance, "transport_applied", "b64ag runtime_provenance") is True,
        "b64ag runtime_provenance.transport_applied must be true",
    )
    expect(
        require_int(provenance, "epsilon_order", "b64ag runtime_provenance") == 4,
        "b64ag runtime_provenance.epsilon_order must be 4",
    )
    expect(
        require_int(provenance, "epsilon_sample_count", "b64ag runtime_provenance")
        == REQUIRED_EPSILON_SAMPLES,
        "b64ag runtime_provenance.epsilon_sample_count drifted",
    )
    expect(
        require_int(provenance, "precision_digits", "b64ag runtime_provenance")
        >= REQUIRED_DIGITS,
        "b64ag runtime_provenance.precision_digits is below the floor",
    )
    expect(
        provenance.get("blocked_reason") == "",
        "b64ag runtime_provenance.blocked_reason must be empty",
    )
    expect(
        require_string(provenance, "runtime_application", "b64ag runtime_provenance")
        == "b64ag-gauge-link-full-packet-finite-part-coefficients",
        "b64ag runtime_provenance.runtime_application drifted",
    )
    expect(
        require_string(provenance, "runtime_boundary_provider", "b64ag runtime_provenance")
        == "retained-finite-gauge-link-boundary+gaugex-zero-full-packet-finite-part-transport",
        "b64ag runtime_provenance.runtime_boundary_provider drifted",
    )
    expect(
        require_string(provenance, "transport_scope", "b64ag runtime_provenance")
        == "eta-zero-b64ag-full-packet-finite-part-coefficients",
        "b64ag runtime_provenance.transport_scope drifted",
    )


def require_b64ag_role_verdicts(evidence: dict[str, Any]) -> None:
    role_verdicts = require_object(evidence, "role_verdicts", "b64ag evidence")
    for role in ("role_a", "role_b", "role_c", "role_d"):
        verdict = require_string(role_verdicts, role, "b64ag role_verdicts")
        expect(verdict.startswith("APPROVE "), f"b64ag {role} must approve the packet")
    expect(
        require_string(role_verdicts, "unanimous", "b64ag role_verdicts") == "APPROVE",
        "b64ag role verdicts must be unanimous APPROVE",
    )


def component_floor(row: dict[str, Any]) -> tuple[int, int | None]:
    real_digits = require_int(row, "real_agreement_digits", "b64ag coefficient")
    imag_digits = require_int(row, "imag_agreement_digits", "b64ag coefficient")
    floor = min(real_digits, imag_digits)
    non_sentinel = [value for value in (real_digits, imag_digits) if value != 999]
    return floor, min(non_sentinel) if non_sentinel else None


def require_b64ag_coefficient_witness(
    coefficient: dict[str, Any],
    *,
    integral: str,
    integral_index: int,
    coefficient_index: int,
    committed_compare: dict[str, Any],
    committed_cpp: dict[str, Any],
) -> tuple[int, int | None]:
    label = f"b64ag coefficient {integral}#{coefficient_index}"
    witness = require_object(coefficient, "witness_reference", label)
    require_path(
        witness.get("comparison_artifact"),
        B64AG_COMPARE50,
        f"{label} witness comparison_artifact",
    )
    require_path(
        witness.get("cpp_result_artifact"),
        B64AG_CPP_RESULT,
        f"{label} witness cpp_result_artifact",
    )
    expected_comparison_pointer = f"#/integrals/{integral_index}/coefficients/{coefficient_index}"
    expected_cpp_pointer = f"#/results/{integral_index}/epsilon_orders/{coefficient_index}"
    expect(
        witness.get("comparison_json_pointer") == expected_comparison_pointer,
        f"{label} comparison witness pointer drifted",
    )
    expect(
        witness.get("cpp_result_json_pointer") == expected_cpp_pointer,
        f"{label} C++ witness pointer drifted",
    )
    expect(witness.get("integral") == integral, f"{label} witness integral drifted")
    expect(witness.get("order") == coefficient.get("order"), f"{label} witness order drifted")

    comparison_integral = resolve_json_pointer(
        committed_compare,
        f"/integrals/{integral_index}",
        f"{label} comparison integral witness",
    )
    expect(
        isinstance(comparison_integral, dict) and comparison_integral.get("integral") == integral,
        f"{label} comparison witness integral mismatch",
    )
    comparison_row = resolve_json_pointer(
        committed_compare,
        expected_comparison_pointer,
        f"{label} comparison row witness",
    )
    expect(isinstance(comparison_row, dict), f"{label} comparison row witness must be an object")
    for field in (
        "order",
        "cpp_real",
        "cpp_imag",
        "amflow_real",
        "amflow_imag",
        "cpp_present",
        "amflow_present",
        "real_agreement_digits",
        "imag_agreement_digits",
        "real_relative_error_abs",
        "imag_relative_error_abs",
        "relative_error_abs",
        "passed",
    ):
        expect(
            coefficient.get(field) == comparison_row.get(field),
            f"{label} field drifted from comparison witness: {field}",
        )
    expect(comparison_row.get("cpp_present") is True, f"{label} C++ presence must be true")
    expect(comparison_row.get("amflow_present") is True, f"{label} AMFlow presence must be true")
    expect(comparison_row.get("passed") is True, f"{label} comparison row must pass")

    cpp_integral = resolve_json_pointer(
        committed_cpp,
        f"/results/{integral_index}",
        f"{label} C++ integral witness",
    )
    expect(
        isinstance(cpp_integral, dict) and cpp_integral.get("integral") == integral,
        f"{label} C++ witness integral mismatch",
    )
    cpp_row = resolve_json_pointer(
        committed_cpp,
        expected_cpp_pointer,
        f"{label} C++ coefficient witness",
    )
    expect(isinstance(cpp_row, dict), f"{label} C++ coefficient witness must be an object")
    expect(cpp_row.get("order") == coefficient.get("order"), f"{label} C++ witness order drifted")
    expect(
        decimal_strings_equal(cpp_row.get("real_digits"), coefficient.get("cpp_real")),
        f"{label} C++ real value drifted",
    )
    expect(
        decimal_strings_equal(cpp_row.get("imag_digits"), coefficient.get("cpp_imag")),
        f"{label} C++ imag value drifted",
    )

    floor, non_sentinel_floor = component_floor(coefficient)
    expect(
        coefficient.get("component_digit_floor") == floor,
        f"{label} component_digit_floor drifted",
    )
    expect(
        coefficient.get("non_sentinel_component_digit_floor") == non_sentinel_floor,
        f"{label} non_sentinel_component_digit_floor drifted",
    )
    return floor, non_sentinel_floor


def require_b64ag_per_coefficient_evidence(
    evidence: dict[str, Any],
    committed_compare: dict[str, Any],
    committed_cpp: dict[str, Any],
) -> dict[str, Any]:
    per_integral = require_list(evidence, "per_integral_digit_evidence", "b64ag evidence")
    expect(len(per_integral) == len(EXPECTED_PACKET_TARGETS), "b64ag evidence target count drifted")
    total_coefficients = 0
    total_passed = 0
    direct_count = 0
    reviewed_count = 0
    component_floors: list[int] = []
    non_sentinel_floors: list[int] = []
    direct_non_sentinel_floors: list[int] = []
    reviewed_component_floors: list[int] = []
    for integral_index, raw_integral in enumerate(per_integral):
        expect(isinstance(raw_integral, dict), "b64ag per-integral evidence entries must be objects")
        integral = require_string(raw_integral, "integral", "b64ag per-integral evidence")
        expect(
            integral == EXPECTED_PACKET_TARGETS[integral_index],
            f"b64ag per-integral target order drifted at {integral_index}",
        )
        coefficients = require_list(raw_integral, "coefficients", f"b64ag per-integral {integral}")
        expect(
            len(coefficients) == len(EXPECTED_ORDERS[integral]),
            f"b64ag {integral} coefficient count drifted",
        )
        expect(
            require_int(raw_integral, "coefficient_count", f"b64ag per-integral {integral}")
            == len(coefficients),
            f"b64ag {integral} coefficient_count field drifted",
        )
        expect(
            require_int(raw_integral, "passed_coefficient_count", f"b64ag per-integral {integral}")
            == len(coefficients),
            f"b64ag {integral} passed_coefficient_count field drifted",
        )
        classification = require_string(raw_integral, "classification", f"b64ag per-integral {integral}")
        expect(
            classification
            in {"direct-first-block-reviewed-row", "zero-or-reviewed-table-row"},
            f"b64ag {integral} classification drifted",
        )
        integral_component_floors: list[int] = []
        integral_non_sentinel_floors: list[int] = []
        observed_orders: list[int] = []
        for coefficient_index, raw_coefficient in enumerate(coefficients):
            expect(isinstance(raw_coefficient, dict), f"b64ag {integral} coefficient must be an object")
            floor, non_sentinel_floor = require_b64ag_coefficient_witness(
                raw_coefficient,
                integral=integral,
                integral_index=integral_index,
                coefficient_index=coefficient_index,
                committed_compare=committed_compare,
                committed_cpp=committed_cpp,
            )
            observed_orders.append(require_int(raw_coefficient, "order", f"b64ag {integral} coefficient"))
            integral_component_floors.append(floor)
            component_floors.append(floor)
            if non_sentinel_floor is not None:
                integral_non_sentinel_floors.append(non_sentinel_floor)
                non_sentinel_floors.append(non_sentinel_floor)
        expect(observed_orders == EXPECTED_ORDERS[integral], f"b64ag {integral} order drifted")
        integral_floor = min(integral_component_floors)
        integral_non_sentinel = min(integral_non_sentinel_floors) if integral_non_sentinel_floors else None
        expect(
            raw_integral.get("minimum_component_digit_floor") == integral_floor,
            f"b64ag {integral} minimum_component_digit_floor drifted",
        )
        expect(
            raw_integral.get("minimum_non_sentinel_component_digit_floor") == integral_non_sentinel,
            f"b64ag {integral} minimum_non_sentinel_component_digit_floor drifted",
        )
        total_coefficients += len(coefficients)
        total_passed += len(coefficients)
        if classification == "direct-first-block-reviewed-row":
            direct_count += len(coefficients)
            direct_non_sentinel_floors.extend(integral_non_sentinel_floors)
        else:
            reviewed_count += len(coefficients)
            reviewed_component_floors.extend(integral_component_floors)
    return {
        "total_coefficients": total_coefficients,
        "total_passed": total_passed,
        "minimum_component_floor": min(component_floors),
        "minimum_non_sentinel_floor": min(non_sentinel_floors),
        "direct_count": direct_count,
        "direct_minimum_non_sentinel_floor": min(direct_non_sentinel_floors),
        "reviewed_count": reviewed_count,
        "reviewed_minimum_component_floor": min(reviewed_component_floors),
    }


def require_b64ag_evidence_payload(
    root: Path,
    evidence: dict[str, Any],
    committed_compare: dict[str, Any],
    committed_cpp: dict[str, Any],
) -> None:
    expect(evidence.get("schema_version") == 1, "b64ag evidence schema_version drifted")
    expect(
        evidence.get("baseline_commit") == B64AG_EVIDENCE_BASELINE_COMMIT,
        "b64ag evidence baseline commit drifted",
    )
    expect(evidence.get("benchmark_id") == "linear_propagator", "b64ag benchmark_id drifted")
    expect(evidence.get("lane") == "lane1-next24", "b64ag lane drifted")
    expect(evidence.get("runtime_lane") == "b64ag", "b64ag runtime_lane drifted")
    expect(
        evidence.get("step_chosen") == "formal-b64ag-full-contour-flip",
        "b64ag step_chosen drifted",
    )
    require_string(evidence, "evidence_scope", "b64ag evidence")
    expect(
        evidence.get("status") == "m6_b64ag_qualifier_passed_full_eta_zero_contour_applied",
        "b64ag evidence status drifted",
    )
    expect(evidence.get("m6_flipped") is False, "b64ag evidence must not flip global M6")
    expect(require_string_list(evidence, "withheld_claims", "b64ag evidence"), "b64ag withheld claims missing")
    require_b64ag_runtime_provenance(evidence)
    require_b64ag_role_verdicts(evidence)

    qualifier_pass = require_object(evidence, "qualifier_pass", "b64ag evidence")
    require_path(qualifier_pass.get("summary"), B64AG_READINESS, "b64ag qualifier_pass.summary")
    expect(qualifier_pass.get("status") == "ready", "b64ag qualifier pass is not ready")
    expect(qualifier_pass.get("golden_recapture_ready") is True, "b64ag golden recapture is not ready")
    expect(qualifier_pass.get("blocking_reasons") == [], "b64ag qualifier has blockers")
    expect(qualifier_pass.get("failure_codes") == [], "b64ag qualifier has failure codes")
    expect(qualifier_pass.get("detailed_coefficient_count") == REQUIRED_COEFFICIENT_ROWS, "b64ag qualifier count drifted")
    expect(
        qualifier_pass.get("minimum_detailed_digit_agreement") == 51,
        "b64ag qualifier digit agreement drifted",
    )

    row_summary = require_b64ag_per_coefficient_evidence(evidence, committed_compare, committed_cpp)
    rollup = require_object(evidence, "coefficient_rollup", "b64ag evidence")
    expect(
        rollup.get("all_coefficients_passed_50digit_floor") is True,
        "b64ag coefficient rollup is not passing",
    )
    expect(
        rollup.get("direct_first_block_targets")
        == [
            "gauge[1,1,1,-1,1,0,0,0,0]",
            "gauge[1,1,1,0,1,0,0,0,0]",
        ],
        "b64ag direct first-block targets drifted",
    )
    expect(
        rollup.get("direct_first_block_coefficient_count") == row_summary["direct_count"],
        "b64ag direct count drifted",
    )
    expect(
        rollup.get("direct_first_block_minimum_non_sentinel_component_digit_floor")
        == row_summary["direct_minimum_non_sentinel_floor"],
        "b64ag direct digit floor drifted",
    )
    expect(
        rollup.get("zero_or_reviewed_table_coefficient_count") == row_summary["reviewed_count"],
        "b64ag reviewed-table count drifted",
    )
    expect(
        rollup.get("zero_or_reviewed_table_minimum_component_digit_floor")
        == row_summary["reviewed_minimum_component_floor"],
        "b64ag reviewed-table digit floor drifted",
    )

    comparison_summary = require_object(evidence, "comparison_summary", "b64ag evidence")
    expect(comparison_summary.get("comparison") == "cpp-vs-amflow", "b64ag comparison kind drifted")
    expect(comparison_summary.get("passed") is True, "b64ag comparison summary is not passing")
    expect(comparison_summary.get("failures") == [], "b64ag comparison summary has failures")
    expect(
        comparison_summary.get("matched_integral_count") == len(EXPECTED_PACKET_TARGETS),
        "b64ag matched integral count drifted",
    )
    expect(
        comparison_summary.get("compared_coefficient_count") == row_summary["total_coefficients"],
        "b64ag compared coefficient count drifted",
    )
    expect(
        comparison_summary.get("computed_coefficient_count") == row_summary["total_coefficients"],
        "b64ag computed coefficient count drifted",
    )
    expect(
        comparison_summary.get("passed_coefficient_count") == row_summary["total_passed"],
        "b64ag passed coefficient count drifted",
    )
    expect(
        comparison_summary.get("computed_passed_coefficient_count") == row_summary["total_passed"],
        "b64ag computed passed coefficient count drifted",
    )
    expect(
        comparison_summary.get("computed_minimum_component_digit_floor")
        == row_summary["minimum_non_sentinel_floor"],
        "b64ag computed component digit floor drifted",
    )
    expect(
        comparison_summary.get("computed_minimum_non_sentinel_component_digit_floor")
        == row_summary["minimum_non_sentinel_floor"],
        "b64ag non-sentinel digit floor drifted",
    )
    expect(
        comparison_summary.get("minimum_digit_agreement") == committed_compare.get("minimum_digit_agreement"),
        "b64ag comparison minimum digit agreement drifted",
    )
    expect(
        comparison_summary.get("tolerance_digits") == committed_compare.get("tolerance_digits"),
        "b64ag comparison tolerance drifted",
    )

    require_source_artifact_hashes(root, evidence)


def require_b64ag_closure_evidence(root: Path) -> None:
    evidence = load_json(root / B64AG_EVIDENCE)
    committed_compare = load_json(root / B64AG_COMPARE50)
    committed_cpp = load_json(root / B64AG_CPP_RESULT)
    require_b64ag_evidence_payload(root, evidence, committed_compare, committed_cpp)

    committed_readiness = load_json(root / B64AG_READINESS)
    rerun_readiness = audit_b64ag_golden_recapture_readiness(
        root / B64AG_CPP_RESULT,
        root / B64AG_COMPARE50,
        root / B64AG_AMFLOW_STATE,
    )
    rerun_readiness["inputs"] = {
        "cpp_result": str(B64AG_CPP_RESULT),
        "comparison_summary": str(B64AG_COMPARE50),
        "amflow_state": str(B64AG_AMFLOW_STATE),
    }
    expect(committed_readiness == rerun_readiness, "b64ag readiness sidecar is not reproducible")
    expect(committed_readiness.get("golden_recapture_ready") is True, "b64ag readiness is not ready")
    expect(committed_readiness.get("m6_closure_claimed") is False, "b64ag readiness claims M6 closure")

    recomputed_compare = compare_cpp_vs_amflow(
        cpp_result_path=root / B64AG_CPP_RESULT,
        amflow_golden_path=root / B64AG_AMFLOW_GOLDEN,
        tolerance_digits=50,
        amflow_state_path=root / B64AG_AMFLOW_STATE,
    )
    recomputed_compare["cpp_result"] = str(B64AG_CPP_RESULT)
    recomputed_compare["amflow_golden"] = str(B64AG_AMFLOW_GOLDEN)
    recomputed_compare["amflow_state"] = str(B64AG_AMFLOW_STATE)
    require_semantic_comparison_match(committed_compare, recomputed_compare)


def verify(root: Path) -> dict[str, Any]:
    m6_summary = load_json(root / M6_QUALIFICATION)
    require_ready_m6_summary(root, m6_summary)
    require_b64ag_closure_evidence(root)
    require_release_surface_uses_accepted_m6(root)
    return {
        "schema_version": 1,
        "status": "m6-readiness-sidecar-closure-consistent",
        "accepted_m6_qualification": str(M6_QUALIFICATION),
        "m6_source_commit": M6_CLOSURE_COMMIT,
        "b64ag_source_commit": B64AG_SOURCE_COMMIT,
        "b64ag_qualifier_summary": str(B64AG_READINESS),
        "b64ag_comparison_summary": str(B64AG_COMPARE50),
        "checked_release_readiness_outputs": [str(path) for path in RELEASE_READINESS_OUTPUTS],
    }


def run_self_check() -> dict[str, bool]:
    root = repo_root()
    verify(root)
    committed = load_json(root / B64AG_COMPARE50)
    recomputed = compare_cpp_vs_amflow(
        cpp_result_path=root / B64AG_CPP_RESULT,
        amflow_golden_path=root / B64AG_AMFLOW_GOLDEN,
        tolerance_digits=50,
        amflow_state_path=root / B64AG_AMFLOW_STATE,
    )
    recomputed["cpp_result"] = str(B64AG_CPP_RESULT)
    recomputed["amflow_golden"] = str(B64AG_AMFLOW_GOLDEN)
    recomputed["amflow_state"] = str(B64AG_AMFLOW_STATE)
    tampered = copy.deepcopy(committed)
    tampered["integrals"][2]["coefficients"][0]["real_agreement_digits"] = 49
    tampered["minimum_digit_agreement"] = 49
    coefficient_tamper_rejected = False
    try:
        require_semantic_comparison_match(tampered, recomputed)
    except ClosureAuditError:
        coefficient_tamper_rejected = True

    tampered_evidence = copy.deepcopy(load_json(root / B64AG_EVIDENCE))
    tampered_evidence["source_artifacts"]["compare50_sha256"] = "bad"
    source_hash_tamper_rejected = False
    try:
        require_source_artifact_hashes(root, tampered_evidence)
    except ClosureAuditError:
        source_hash_tamper_rejected = True

    tampered_closure_hashes = dict(B64AG_CLOSURE_ARTIFACT_SHA256)
    tampered_closure_hashes["compare50"] = "bad"
    closure_artifact_hash_tamper_rejected = False
    try:
        require_source_artifact_hashes(
            root,
            load_json(root / B64AG_EVIDENCE),
            closure_hashes=tampered_closure_hashes,
        )
    except ClosureAuditError:
        closure_artifact_hash_tamper_rejected = True

    committed_cpp = load_json(root / B64AG_CPP_RESULT)
    witnessless_evidence = copy.deepcopy(load_json(root / B64AG_EVIDENCE))
    witnessless_evidence["per_integral_digit_evidence"][0]["coefficients"][0].pop(
        "witness_reference"
    )
    missing_witness_rejected = False
    try:
        require_b64ag_evidence_payload(root, witnessless_evidence, committed, committed_cpp)
    except ClosureAuditError:
        missing_witness_rejected = True

    nullable_field_evidence = copy.deepcopy(load_json(root / B64AG_EVIDENCE))
    nullable_field_evidence["runtime_provenance"][
        "final_solution_samples_used_as_input"
    ] = None
    nullable_field_rejected = False
    try:
        require_b64ag_evidence_payload(root, nullable_field_evidence, committed, committed_cpp)
    except ClosureAuditError:
        nullable_field_rejected = True

    summary = {
        "current_surface_verified": True,
        "coefficient_tamper_rejected": coefficient_tamper_rejected,
        "source_hash_tamper_rejected": source_hash_tamper_rejected,
        "closure_artifact_hash_tamper_rejected": closure_artifact_hash_tamper_rejected,
        "missing_witness_rejected": missing_witness_rejected,
        "nullable_field_rejected": nullable_field_rejected,
    }
    expect(all(summary.values()), "M6 readiness sidecar closure self-check failed")
    return summary


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-check", action="store_true", help="Run synthetic tamper checks")
    parser.add_argument(
        "--summary-path",
        type=Path,
        help="Optional path to write the audit summary JSON",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        summary = run_self_check() if args.self_check else verify(repo_root())
        if args.summary_path is not None:
            write_json(args.summary_path, summary)
        print(json.dumps(summary, indent=2, sort_keys=True))
        return 0
    except Exception as error:  # noqa: BLE001 - make CTest failure explicit.
        print(
            json.dumps(
                {
                    "schema_version": 1,
                    "status": "m6-readiness-sidecar-closure-drift",
                    "blocking_reasons": [str(error)],
                },
                indent=2,
                sort_keys=True,
            )
        )
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
