#!/usr/bin/env python3
"""Summarize the current post-M7 b63n parity status from committed evidence."""

from __future__ import annotations

import argparse
import copy
import json
import sys
from pathlib import Path
from typing import Any

from regenerate_b63n_weighted_residue_fingerprints import (
    run_emitter as run_fingerprint_emitter,
    validate_payload as validate_fingerprint_payload,
)
from verify_b63n_d246_evidence import (
    DEFAULT_SIDECAR as DEFAULT_D246_SIDECAR,
    verify_sidecar as verify_d246_sidecar,
)
from verify_b63n_scoped_gate_audit_trail import (
    EXPECTED_LABELS as EXPECTED_SCOPED_GATE_LABELS,
    resolve_test_binary,
    run_emitter as run_scoped_gate_emitter,
    validate_payload as validate_scoped_gate_payload,
)
from verify_b63n_selected4_permutation_audit import (
    DEFAULT_COMPARE as DEFAULT_SELECTED4_COMPARE,
    DEFAULT_CPP_RESULT as DEFAULT_SELECTED4_CPP_RESULT,
    DEFAULT_EVIDENCE as DEFAULT_SELECTED4_EVIDENCE,
    EXPECTED_COMPARED_COEFFICIENTS as SELECTED4_COMPARED_COEFFICIENTS,
    EXPECTED_SELECTED4_ORDERS,
    PUBLISHED_D7_INTEGRAL,
    verify_paths as verify_selected4_paths,
)


DEFAULT_FIRST_EVIDENCE = Path(
    "tools/reference-harness/specs/m6/lane143/"
    "b63n-first-real-coefficient-evidence.json"
)
DEFAULT_FIRST_COMPARE = Path(
    "tools/reference-harness/specs/m6/lane143/"
    "automatic_phasespace.first-cutkosky.compare30.json"
)
DEFAULT_FIRST_CPP_RESULT = Path(
    "tools/reference-harness/specs/m6/lane143/"
    "automatic_phasespace.first-cutkosky.cpp-result.json"
)
FIRST_INTEGRAL = "phase[1,0,1,0,1,0,0]"
FIRST_ORDERS = [0, 1, 2, 3]
WITHHELD_CLAIMS: tuple[str, ...] = (
    "This summary reads committed b63n evidence and optional runtime audit fingerprints only.",
    "This summary does not rerun AMFlow numerics.",
    "This summary does not claim Milestone M6 closure.",
    "This summary does not claim Milestone M7 closure.",
    "This summary does not claim release readiness.",
    "This summary does not claim full eta=0 contour execution.",
    "This summary does not publish D2/D4/D6 weighted-residue coefficients.",
    "This summary does not widen runtime or public behavior.",
)
EVIDENCE_SOURCE_FIELDS: tuple[str, ...] = (
    "first_evidence",
    "first_compare",
    "first_cpp_result",
    "selected4_evidence",
    "selected4_compare",
    "selected4_cpp_result",
    "d246_sidecar",
)


class StatusSummaryError(RuntimeError):
    """Raised when b63n parity status inputs cannot be summarized."""


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise StatusSummaryError(message)


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def resolve_repo_path(root: Path, path: Path) -> Path:
    return path if path.is_absolute() else root / path


def repo_relative_text(root: Path, path: Path) -> str:
    resolved = resolve_repo_path(root, path)
    try:
        return resolved.relative_to(root).as_posix()
    except ValueError:
        return resolved.as_posix()


def read_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as stream:
        payload = json.load(stream)
    expect(isinstance(payload, dict), f"{path} must contain a JSON object")
    return payload


def write_json(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def print_json(payload: dict[str, Any]) -> None:
    print(json.dumps(payload, indent=2, sort_keys=True))


def require_int(payload: dict[str, Any], field: str) -> int:
    value = payload.get(field)
    expect(isinstance(value, int) and not isinstance(value, bool), f"{field} must be an int")
    return value


def require_bool(payload: dict[str, Any], field: str) -> bool:
    value = payload.get(field)
    expect(isinstance(value, bool), f"{field} must be a bool")
    return value


def require_string(payload: dict[str, Any], field: str) -> str:
    value = payload.get(field)
    expect(isinstance(value, str) and value, f"{field} must be a non-empty string")
    return value


def require_list(raw: Any, label: str) -> list[Any]:
    expect(isinstance(raw, list), f"{label} must be a list")
    return raw


def require_object(raw: Any, label: str) -> dict[str, Any]:
    expect(isinstance(raw, dict), f"{label} must be an object")
    return raw


def build_evidence_sources(
    *,
    root: Path,
    first_evidence_path: Path,
    first_compare_path: Path,
    first_cpp_result_path: Path,
    selected4_evidence_path: Path,
    selected4_compare_path: Path,
    selected4_cpp_result_path: Path,
    d246_sidecar_path: Path,
) -> dict[str, Any]:
    return {
        "first_evidence": repo_relative_text(root, first_evidence_path),
        "first_compare": repo_relative_text(root, first_compare_path),
        "first_cpp_result": repo_relative_text(root, first_cpp_result_path),
        "selected4_evidence": repo_relative_text(root, selected4_evidence_path),
        "selected4_compare": repo_relative_text(root, selected4_compare_path),
        "selected4_cpp_result": repo_relative_text(root, selected4_cpp_result_path),
        "d246_sidecar": repo_relative_text(root, d246_sidecar_path),
    }


def default_evidence_sources(root: Path) -> dict[str, Any]:
    return build_evidence_sources(
        root=root,
        first_evidence_path=DEFAULT_FIRST_EVIDENCE,
        first_compare_path=DEFAULT_FIRST_COMPARE,
        first_cpp_result_path=DEFAULT_FIRST_CPP_RESULT,
        selected4_evidence_path=DEFAULT_SELECTED4_EVIDENCE,
        selected4_compare_path=DEFAULT_SELECTED4_COMPARE,
        selected4_cpp_result_path=DEFAULT_SELECTED4_CPP_RESULT,
        d246_sidecar_path=DEFAULT_D246_SIDECAR,
    )


def validate_evidence_sources(sources: dict[str, Any]) -> dict[str, str]:
    normalized = {field: require_string(sources, field) for field in EVIDENCE_SOURCE_FIELDS}
    first_paths = {
        normalized["first_evidence"],
        normalized["first_compare"],
        normalized["first_cpp_result"],
    }
    selected4_paths = {
        normalized["selected4_evidence"],
        normalized["selected4_compare"],
        normalized["selected4_cpp_result"],
    }
    expect(len(first_paths) == 3, "first coefficient evidence source paths must be distinct")
    expect(len(selected4_paths) == 3, "selected4 evidence source paths must be distinct")
    expect(
        normalized["first_compare"] != normalized["selected4_compare"],
        "first and selected4 compare sources must stay distinct",
    )
    expect(
        normalized["first_cpp_result"] != normalized["selected4_cpp_result"],
        "first and selected4 C++ result sources must stay distinct",
    )
    expect(
        normalized["d246_sidecar"] not in first_paths | selected4_paths,
        "D246 sidecar source must stay distinct from parity evidence sources",
    )
    return normalized


def coefficient_orders(row: dict[str, Any], label: str) -> list[int]:
    orders: list[int] = []
    for index, raw_coefficient in enumerate(require_list(row.get("coefficients"), f"{label}.coefficients")):
        coefficient = require_object(raw_coefficient, f"{label}.coefficients[{index}]")
        order = require_int(coefficient, "order")
        expect(coefficient.get("amflow_present") is True, f"{label} eps^{order} missing AMFlow")
        expect(coefficient.get("cpp_present") is True, f"{label} eps^{order} missing C++")
        expect(coefficient.get("passed") is True, f"{label} eps^{order} did not pass")
        expect(
            require_int(coefficient, "real_agreement_digits") >= 30,
            f"{label} eps^{order} real digit floor regressed",
        )
        expect(
            require_int(coefficient, "imag_agreement_digits") >= 30,
            f"{label} eps^{order} imaginary digit floor regressed",
        )
        orders.append(order)
    return orders


def summarize_first_paths(
    *,
    evidence_path: Path,
    compare_path: Path,
    cpp_result_path: Path,
) -> dict[str, Any]:
    root = repo_root()
    evidence = read_json(resolve_repo_path(root, evidence_path))
    compare = read_json(resolve_repo_path(root, compare_path))
    cpp_result = read_json(resolve_repo_path(root, cpp_result_path))

    expect(evidence.get("schema_version") == 1, "first coefficient evidence schema_version must be 1")
    expect(evidence.get("runtime_lane") == "b63n", "first coefficient runtime_lane must be b63n")
    expect(evidence.get("lane") == "lane143", "first coefficient evidence lane must be lane143")
    expect(evidence.get("benchmark_id") == "automatic_phasespace", "first benchmark drifted")
    expect(evidence.get("master") == FIRST_INTEGRAL, "first coefficient integral drifted")
    expect(evidence.get("transport_applied") is True, "first coefficient transport must be applied")
    expect(
        evidence.get("transport_scope") == "eta-zero-selected-endpoint-coefficients",
        "first coefficient transport scope drifted",
    )
    expect(
        evidence.get("eta_zero_endpoint_transport_applied") is True,
        "first coefficient eta-zero endpoint transport must be true",
    )
    expect(
        evidence.get("final_solution_samples_used_as_input") is False,
        "first coefficient must not use final solution samples as input",
    )
    expect(
        evidence.get("full_eta_zero_contour_applied") is False,
        "first coefficient must not claim full eta-zero contour",
    )

    comparator = require_object(evidence.get("comparator"), "first comparator")
    expect(comparator.get("path") == repo_relative_text(root, compare_path), "first comparator path drifted")
    expect(comparator.get("passed") is True, "first comparator must pass")
    expect(require_int(comparator, "tolerance_digits") == 30, "first comparator tolerance must be 30")
    expect(require_int(comparator, "compared_coefficient_count") == 4, "first compared count must be 4")
    expect(require_int(comparator, "passed_coefficient_count") == 4, "first passed count must be 4")
    expect(require_int(comparator, "minimum_digit_agreement") == 999, "first minimum digits must be exact")

    expect(compare.get("schema_version") == 1, "first compare schema_version must be 1")
    expect(compare.get("comparison") == "cpp-vs-amflow", "first compare kind drifted")
    expect(compare.get("passed") is True, "first compare must pass")
    expect(compare.get("failures") == [], "first compare must not retain failures")
    expect(require_int(compare, "matched_integral_count") == 1, "first matched integral count drifted")
    expect(require_int(compare, "compared_coefficient_count") == 4, "first compare count drifted")
    expect(require_int(compare, "passed_coefficient_count") == 4, "first compare pass count drifted")
    expect(require_int(compare, "minimum_digit_agreement") == 999, "first compare minimum digits drifted")
    compare_rows = require_list(compare.get("integrals"), "first compare integrals")
    expect(len(compare_rows) == 1, "first compare must retain one integral row")
    compare_row = require_object(compare_rows[0], "first compare integrals[0]")
    expect(compare_row.get("integral") == FIRST_INTEGRAL, "first compare integral drifted")
    expect(coefficient_orders(compare_row, "first compare") == FIRST_ORDERS, "first order scope drifted")

    expect(cpp_result.get("schema_version") == 1, "first cpp schema_version must be 1")
    expect(cpp_result.get("benchmark_id") == "automatic_phasespace", "first cpp benchmark drifted")
    continuation = require_object(cpp_result.get("continuation"), "first cpp continuation")
    expect(continuation.get("transport_applied") is True, "first cpp transport must be applied")
    expect(
        continuation.get("full_eta_zero_contour_applied") is False,
        "first cpp result must not claim full eta-zero contour",
    )
    expect(
        continuation.get("eta_zero_endpoint_transported_integrals") == [FIRST_INTEGRAL],
        "first cpp transported integral scope drifted",
    )

    return {
        "evidence_valid": True,
        "lane": "lane143",
        "integral": FIRST_INTEGRAL,
        "orders": FIRST_ORDERS,
        "compared_coefficient_count": 4,
        "minimum_digit_agreement": 999,
        "transport_scope": "eta-zero-selected-endpoint-coefficients",
        "final_solution_samples_used_as_input": False,
        "full_eta_zero_contour_applied": False,
    }


def summarize_selected4_paths(
    *,
    evidence_path: Path,
    compare_path: Path,
    cpp_result_path: Path,
) -> dict[str, Any]:
    selected4 = verify_selected4_paths(
        None,
        resolve_repo_path(repo_root(), evidence_path),
        resolve_repo_path(repo_root(), compare_path),
        resolve_repo_path(repo_root(), cpp_result_path),
    )
    expect(selected4.get("cross_check_passed") is True, "selected4 verifier must pass")
    compare = require_object(selected4.get("selected4_compare"), "selected4 verifier compare")
    evidence = require_object(selected4.get("selected4_evidence"), "selected4 verifier evidence")
    expect(
        compare.get("compared_coefficient_count") == SELECTED4_COMPARED_COEFFICIENTS,
        "selected4 compared coefficient count drifted",
    )
    expect(compare.get("minimum_digit_agreement") == 999, "selected4 digit floor drifted")
    expect(compare.get("published_d7_integral") == PUBLISHED_D7_INTEGRAL, "selected4 D7 integral drifted")
    expect(compare.get("published_d7_orders") == [0, 1, 2, 3], "selected4 D7 order scope drifted")
    return {
        "cross_check_passed": True,
        "lane": "lane146",
        "transported_integrals": evidence["transported_integrals"],
        "transported_integral_count": len(evidence["transported_integrals"]),
        "compared_coefficient_count": compare["compared_coefficient_count"],
        "minimum_digit_agreement": compare["minimum_digit_agreement"],
        "per_integral_orders": {
            integral: list(orders) for integral, orders in EXPECTED_SELECTED4_ORDERS.items()
        },
        "published_d7_integral": compare["published_d7_integral"],
        "published_d7_orders": compare["published_d7_orders"],
        "full_eta_zero_contour_applied": False,
        "m6_closure_claimed": False,
        "m7_closure_claimed": False,
    }


def summarize_d246_path(sidecar_path: Path) -> dict[str, Any]:
    root = repo_root()
    sidecar = read_json(resolve_repo_path(root, sidecar_path))
    summary = verify_d246_sidecar(resolve_repo_path(root, sidecar_path))
    expect(summary.get("schema_valid") is True, "D246 sidecar schema must be valid")
    expect(summary.get("runtime_lane") == "b63n", "D246 runtime_lane must be b63n")
    expect(summary.get("surface_label") == "phase[1,2,1,1,1,1,1]", "D246 surface label drifted")
    expect(summary.get("weights") == ["D2", "D4", "D6"], "D246 weight scope drifted")
    expect(summary.get("published_evidence") is False, "D2/D4/D6 must remain blocked, not published")
    expect(summary.get("skeleton_evidence") is True, "D246 sidecar must remain skeleton evidence")
    blockers = require_list(sidecar.get("publication_blockers"), "D246 publication_blockers")
    expect(blockers, "D246 blockers must remain visible")
    return {
        "schema_valid": True,
        "surface_label": summary["surface_label"],
        "weights": summary["weights"],
        "published_evidence": False,
        "skeleton_evidence": True,
        "minimum_digit_agreement": summary["minimum_digit_agreement"],
        "publication_blockers": blockers,
        "m6_closure_claimed": False,
        "m7_closure_claimed": False,
    }


def summarize_scoped_gate_runtime(test_binary: Path, root: Path) -> dict[str, Any]:
    payload = run_scoped_gate_emitter(test_binary, root)
    scoped = validate_scoped_gate_payload(payload)
    expect(scoped.get("scoped_gate_audit_query_passed") is True, "scoped gate audit must pass")
    expect(scoped.get("entry_count") == 2, "scoped gate entry count drifted")
    expect(scoped.get("queried_labels") == EXPECTED_SCOPED_GATE_LABELS, "scoped gate labels drifted")
    expect(scoped.get("queried_weights") == ["D2", "D7"], "scoped gate queried weights drifted")
    return {
        "runtime_checked": True,
        "entry_count": scoped["entry_count"],
        "queried_labels": scoped["queried_labels"],
        "queried_weights": scoped["queried_weights"],
        "passed": True,
    }


def summarize_scoped_gate_pinned() -> dict[str, Any]:
    return {
        "runtime_checked": False,
        "entry_count": None,
        "queried_labels": list(EXPECTED_SCOPED_GATE_LABELS),
        "queried_weights": ["D2", "D7"],
        "passed": None,
    }


def summarize_fingerprint_runtime(test_binary: Path, root: Path) -> dict[str, Any]:
    payload = run_fingerprint_emitter(test_binary, root)
    fingerprint_summary = validate_fingerprint_payload(payload, require_matches=True)
    return {
        "runtime_checked": True,
        "entry_count": fingerprint_summary["entry_count"],
        "pins_match": True,
    }


def summarize_fingerprint_pinned() -> dict[str, Any]:
    return {
        "runtime_checked": False,
        "entry_count": None,
        "pins_match": None,
    }


def summarize_payloads(
    *,
    first: dict[str, Any],
    selected4: dict[str, Any],
    d246: dict[str, Any],
    scoped_gate: dict[str, Any],
    fingerprints: dict[str, Any],
    evidence_sources: dict[str, Any],
) -> dict[str, Any]:
    expect(first.get("evidence_valid") is True, "first coefficient evidence must be valid")
    expect(first.get("integral") == FIRST_INTEGRAL, "first coefficient integral drifted")
    expect(first.get("orders") == FIRST_ORDERS, "first coefficient order scope drifted")
    expect(first.get("compared_coefficient_count") == 4, "first coefficient count drifted")
    expect(first.get("minimum_digit_agreement") == 999, "first coefficient digit floor drifted")
    expect(
        first.get("final_solution_samples_used_as_input") is False,
        "first coefficient must not use final solution samples",
    )
    expect(
        first.get("full_eta_zero_contour_applied") is False,
        "first coefficient must not claim full eta-zero contour",
    )

    expect(selected4.get("cross_check_passed") is True, "selected4 cross-check must pass")
    expect(
        selected4.get("compared_coefficient_count") == SELECTED4_COMPARED_COEFFICIENTS,
        "selected4 compared coefficient count drifted",
    )
    expect(selected4.get("minimum_digit_agreement") == 999, "selected4 digit floor drifted")
    expect(
        selected4.get("published_d7_integral") == PUBLISHED_D7_INTEGRAL,
        "selected4 published D7 integral drifted",
    )
    expect(
        selected4.get("published_d7_orders") == [0, 1, 2, 3],
        "selected4 D7 order scope drifted",
    )
    selected4_transported_integrals = require_list(
        selected4.get("transported_integrals"),
        "selected4 transported_integrals",
    )
    expect(
        all(isinstance(integral, str) and integral for integral in selected4_transported_integrals),
        "selected4 transported_integrals entries must be non-empty strings",
    )
    expect(
        len(set(selected4_transported_integrals)) == len(selected4_transported_integrals),
        "selected4 transported_integrals must not contain duplicates",
    )
    expect(
        set(selected4_transported_integrals) == set(EXPECTED_SELECTED4_ORDERS),
        "selected4 transported integral scope drifted",
    )
    selected4_transported_count = require_int(selected4, "transported_integral_count")
    expect(
        selected4_transported_count == len(selected4_transported_integrals),
        "selected4 transported integral count must match transported_integrals",
    )
    expect(
        selected4["published_d7_integral"] in selected4_transported_integrals,
        "selected4 published D7 integral must be transported",
    )
    expect(
        selected4.get("full_eta_zero_contour_applied") is False,
        "selected4 must not claim full eta-zero contour",
    )

    expect(d246.get("schema_valid") is True, "D246 sidecar schema must be valid")
    expect(d246.get("weights") == ["D2", "D4", "D6"], "D246 weights drifted")
    expect(
        d246.get("published_evidence") is False and d246.get("skeleton_evidence") is True,
        "D2/D4/D6 must remain blocked as skeleton evidence",
    )
    blockers = require_list(d246.get("publication_blockers"), "D246 publication_blockers")
    expect(blockers, "D246 publication blockers must remain visible")

    if scoped_gate.get("runtime_checked") is True:
        expect(scoped_gate.get("passed") is True, "runtime scoped gate audit must pass")
        expect(scoped_gate.get("entry_count") == 2, "runtime scoped gate entry count drifted")
        expect(
            scoped_gate.get("queried_labels") == list(EXPECTED_SCOPED_GATE_LABELS),
            "runtime scoped gate labels drifted",
        )
        expect(scoped_gate.get("queried_weights") == ["D2", "D7"], "runtime scoped gate weights drifted")

    if fingerprints.get("runtime_checked") is True:
        expect(fingerprints.get("entry_count") == 6, "b63n audit fingerprint entry count drifted")
        expect(fingerprints.get("pins_match") is True, "b63n audit fingerprints must match pins")

    return {
        "schema_version": 1,
        "summary_id": "b63n-post-m7-parity-status-v1",
        "status": "blocked-full-weighted-residue-surface",
        "inputs_verified": True,
        "evidence_sources": validate_evidence_sources(evidence_sources),
        "first_coefficient": {
            "lane": first["lane"],
            "integral": first["integral"],
            "orders": first["orders"],
            "compared_coefficient_count": first["compared_coefficient_count"],
            "minimum_digit_agreement": first["minimum_digit_agreement"],
            "full_eta_zero_contour_applied": first["full_eta_zero_contour_applied"],
        },
        "selected4_parity": {
            "lane": selected4["lane"],
            "transported_integral_count": selected4_transported_count,
            "transported_integrals": selected4_transported_integrals,
            "compared_coefficient_count": selected4["compared_coefficient_count"],
            "minimum_digit_agreement": selected4["minimum_digit_agreement"],
            "published_d7_integral": selected4["published_d7_integral"],
            "published_d7_orders": selected4["published_d7_orders"],
            "full_eta_zero_contour_applied": selected4["full_eta_zero_contour_applied"],
        },
        "d246_weighted_residue_surface": {
            "surface_label": d246["surface_label"],
            "weights": d246["weights"],
            "published_evidence": d246["published_evidence"],
            "skeleton_evidence": d246["skeleton_evidence"],
            "publication_blockers": blockers,
        },
        "scoped_gate_audit": scoped_gate,
        "audit_fingerprints": fingerprints,
        "blockers": [
            "D2/D4/D6 weighted-residue evidence remains a skeleton sidecar with publication blockers",
            "full automatic_phasespace weighted target phase[1,2,1,1,1,1,1] has no published high-precision AMFlow coefficient packet",
            "full eta=0 contour execution remains deferred",
        ],
        "withheld_claims": list(WITHHELD_CLAIMS),
    }


def render_text(summary: dict[str, Any]) -> str:
    sources = summary["evidence_sources"]
    first = summary["first_coefficient"]
    selected4 = summary["selected4_parity"]
    d246 = summary["d246_weighted_residue_surface"]
    scoped = summary["scoped_gate_audit"]
    fingerprints = summary["audit_fingerprints"]
    lines = [
        "b63n parity status summary",
        f"status: {summary['status']}",
        (
            "evidence_sources: "
            f"first={sources['first_evidence']} "
            f"selected4={sources['selected4_evidence']} "
            f"d246={sources['d246_sidecar']}"
        ),
        (
            "first_coefficient: "
            f"lane={first['lane']} integral={first['integral']} "
            f"orders={first['orders']} compared={first['compared_coefficient_count']} "
            f"minimum_digits={first['minimum_digit_agreement']} "
            f"full_eta_zero_contour_applied={str(first['full_eta_zero_contour_applied']).lower()}"
        ),
        (
            "selected4_parity: "
            f"lane={selected4['lane']} transported={selected4['transported_integral_count']} "
            f"compared={selected4['compared_coefficient_count']} "
            f"minimum_digits={selected4['minimum_digit_agreement']} "
            f"published_d7={selected4['published_d7_integral']} "
            f"d7_orders={selected4['published_d7_orders']} "
            f"full_eta_zero_contour_applied={str(selected4['full_eta_zero_contour_applied']).lower()}"
        ),
        (
            "d246_weighted_residue_surface: "
            f"surface={d246['surface_label']} weights={','.join(d246['weights'])} "
            f"published={str(d246['published_evidence']).lower()} "
            f"skeleton={str(d246['skeleton_evidence']).lower()} "
            f"blockers={len(d246['publication_blockers'])}"
        ),
        (
            "scoped_gate_audit: "
            f"runtime_checked={str(scoped['runtime_checked']).lower()} "
            f"entry_count={scoped['entry_count']} "
            f"queried_weights={','.join(scoped['queried_weights'])}"
        ),
        (
            "audit_fingerprints: "
            f"runtime_checked={str(fingerprints['runtime_checked']).lower()} "
            f"entry_count={fingerprints['entry_count']} "
            f"pins_match={str(fingerprints['pins_match']).lower()}"
        ),
        "blockers: " + "; ".join(summary["blockers"]),
        "withheld_claims: " + " ".join(summary["withheld_claims"]),
    ]
    return "\n".join(lines)


def synthetic_payloads() -> tuple[dict[str, Any], dict[str, Any], dict[str, Any], dict[str, Any], dict[str, Any]]:
    first = {
        "evidence_valid": True,
        "lane": "lane143",
        "integral": FIRST_INTEGRAL,
        "orders": FIRST_ORDERS,
        "compared_coefficient_count": 4,
        "minimum_digit_agreement": 999,
        "transport_scope": "eta-zero-selected-endpoint-coefficients",
        "final_solution_samples_used_as_input": False,
        "full_eta_zero_contour_applied": False,
    }
    selected4 = {
        "cross_check_passed": True,
        "lane": "lane146",
        "transported_integrals": list(EXPECTED_SELECTED4_ORDERS),
        "transported_integral_count": len(EXPECTED_SELECTED4_ORDERS),
        "compared_coefficient_count": SELECTED4_COMPARED_COEFFICIENTS,
        "minimum_digit_agreement": 999,
        "per_integral_orders": {
            integral: list(orders) for integral, orders in EXPECTED_SELECTED4_ORDERS.items()
        },
        "published_d7_integral": PUBLISHED_D7_INTEGRAL,
        "published_d7_orders": [0, 1, 2, 3],
        "full_eta_zero_contour_applied": False,
        "m6_closure_claimed": False,
        "m7_closure_claimed": False,
    }
    d246 = {
        "schema_valid": True,
        "surface_label": "phase[1,2,1,1,1,1,1]",
        "weights": ["D2", "D4", "D6"],
        "published_evidence": False,
        "skeleton_evidence": True,
        "minimum_digit_agreement": None,
        "publication_blockers": [
            "D2, D4, and D6 coefficient arrays are empty",
        ],
        "m6_closure_claimed": False,
        "m7_closure_claimed": False,
    }
    scoped_gate = {
        "runtime_checked": True,
        "entry_count": 2,
        "queried_labels": list(EXPECTED_SCOPED_GATE_LABELS),
        "queried_weights": ["D2", "D7"],
        "passed": True,
    }
    fingerprints = {
        "runtime_checked": True,
        "entry_count": 6,
        "pins_match": True,
    }
    return first, selected4, d246, scoped_gate, fingerprints


def rejected(
    *,
    first: dict[str, Any],
    selected4: dict[str, Any],
    d246: dict[str, Any],
    scoped_gate: dict[str, Any],
    fingerprints: dict[str, Any],
    evidence_sources: dict[str, Any],
    expected_error: str,
) -> bool:
    try:
        summarize_payloads(
            first=first,
            selected4=selected4,
            d246=d246,
            scoped_gate=scoped_gate,
            fingerprints=fingerprints,
            evidence_sources=evidence_sources,
        )
    except Exception as error:  # noqa: BLE001 - self-check intentionally probes failures.
        return expected_error in str(error)
    return False


def run_self_check() -> dict[str, Any]:
    first, selected4, d246, scoped_gate, fingerprints = synthetic_payloads()
    evidence_sources = default_evidence_sources(repo_root())
    valid = summarize_payloads(
        first=first,
        selected4=selected4,
        d246=d246,
        scoped_gate=scoped_gate,
        fingerprints=fingerprints,
        evidence_sources=evidence_sources,
    )

    fake_full_contour = copy.deepcopy(selected4)
    fake_full_contour["full_eta_zero_contour_applied"] = True

    low_digit_selected4 = copy.deepcopy(selected4)
    low_digit_selected4["minimum_digit_agreement"] = 29

    missing_d7_orders = copy.deepcopy(selected4)
    missing_d7_orders["published_d7_orders"] = [0, 1, 2]

    wrong_transported_count = copy.deepcopy(selected4)
    wrong_transported_count["transported_integral_count"] -= 1

    promoted_d246 = copy.deepcopy(d246)
    promoted_d246["published_evidence"] = True
    promoted_d246["skeleton_evidence"] = False

    stale_scoped_gate = copy.deepcopy(scoped_gate)
    stale_scoped_gate["queried_weights"] = ["D7", "D2"]

    stale_fingerprints = copy.deepcopy(fingerprints)
    stale_fingerprints["pins_match"] = False

    evidence_source_drift = copy.deepcopy(evidence_sources)
    evidence_source_drift["selected4_compare"] = evidence_source_drift["first_compare"]

    checks = {
        "synthetic_summary_passes": valid["status"] == "blocked-full-weighted-residue-surface",
        "rejects_full_contour_overclaim": rejected(
            first=first,
            selected4=fake_full_contour,
            d246=d246,
            scoped_gate=scoped_gate,
            fingerprints=fingerprints,
            evidence_sources=evidence_sources,
            expected_error="full eta-zero contour",
        ),
        "rejects_selected4_digit_floor_regression": rejected(
            first=first,
            selected4=low_digit_selected4,
            d246=d246,
            scoped_gate=scoped_gate,
            fingerprints=fingerprints,
            evidence_sources=evidence_sources,
            expected_error="selected4 digit floor",
        ),
        "rejects_missing_d7_order_scope": rejected(
            first=first,
            selected4=missing_d7_orders,
            d246=d246,
            scoped_gate=scoped_gate,
            fingerprints=fingerprints,
            evidence_sources=evidence_sources,
            expected_error="D7 order scope",
        ),
        "rejects_selected4_transported_count_drift": rejected(
            first=first,
            selected4=wrong_transported_count,
            d246=d246,
            scoped_gate=scoped_gate,
            fingerprints=fingerprints,
            evidence_sources=evidence_sources,
            expected_error="transported integral count",
        ),
        "rejects_d246_silent_promotion": rejected(
            first=first,
            selected4=selected4,
            d246=promoted_d246,
            scoped_gate=scoped_gate,
            fingerprints=fingerprints,
            evidence_sources=evidence_sources,
            expected_error="D2/D4/D6 must remain blocked",
        ),
        "rejects_scoped_gate_order_drift": rejected(
            first=first,
            selected4=selected4,
            d246=d246,
            scoped_gate=stale_scoped_gate,
            fingerprints=fingerprints,
            evidence_sources=evidence_sources,
            expected_error="runtime scoped gate weights",
        ),
        "rejects_runtime_fingerprint_drift": rejected(
            first=first,
            selected4=selected4,
            d246=d246,
            scoped_gate=scoped_gate,
            fingerprints=stale_fingerprints,
            evidence_sources=evidence_sources,
            expected_error="fingerprints must match pins",
        ),
        "rejects_evidence_source_drift": rejected(
            first=first,
            selected4=selected4,
            d246=d246,
            scoped_gate=scoped_gate,
            fingerprints=fingerprints,
            evidence_sources=evidence_source_drift,
            expected_error="first and selected4 compare sources",
        ),
    }
    expect(all(checks.values()), "b63n parity status summary self-check failed")
    return {
        "schema_version": 1,
        "summary_id": "b63n-post-m7-parity-status-v1",
        "self_check_passed": True,
        "checks": checks,
    }


def parse_args(argv: list[str]) -> argparse.Namespace:
    root = repo_root()
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--first-evidence-path",
        type=Path,
        default=root / DEFAULT_FIRST_EVIDENCE,
        help="b63n first coefficient evidence JSON path",
    )
    parser.add_argument(
        "--first-compare-path",
        type=Path,
        default=root / DEFAULT_FIRST_COMPARE,
        help="b63n first coefficient compare30 JSON path",
    )
    parser.add_argument(
        "--first-cpp-result-path",
        type=Path,
        default=root / DEFAULT_FIRST_CPP_RESULT,
        help="b63n first coefficient C++ result JSON path",
    )
    parser.add_argument(
        "--selected4-evidence-path",
        type=Path,
        default=root / DEFAULT_SELECTED4_EVIDENCE,
        help="b63n selected4 evidence JSON path",
    )
    parser.add_argument(
        "--selected4-compare-path",
        type=Path,
        default=root / DEFAULT_SELECTED4_COMPARE,
        help="b63n selected4 compare30 JSON path",
    )
    parser.add_argument(
        "--selected4-cpp-result-path",
        type=Path,
        default=root / DEFAULT_SELECTED4_CPP_RESULT,
        help="b63n selected4 C++ result JSON path",
    )
    parser.add_argument(
        "--d246-sidecar-path",
        type=Path,
        default=root / DEFAULT_D246_SIDECAR,
        help="b63n D2/D4/D6 weighted-residue sidecar path",
    )
    parser.add_argument(
        "--test-binary",
        help="Path to the built cutkosky-weighted-residue-tests executable.",
    )
    parser.add_argument(
        "--include-runtime-audit",
        action="store_true",
        help="Query b63n runtime audit emitters and require pinned fingerprints to match.",
    )
    parser.add_argument(
        "--format",
        choices=("text", "json"),
        default="text",
        help="Output format.",
    )
    parser.add_argument("--summary-path", type=Path, help="Optional JSON output path")
    parser.add_argument("--self-check", action="store_true", help="Run synthetic summary checks")
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    root = repo_root()
    try:
        if args.self_check:
            summary = run_self_check()
        else:
            first = summarize_first_paths(
                evidence_path=args.first_evidence_path,
                compare_path=args.first_compare_path,
                cpp_result_path=args.first_cpp_result_path,
            )
            selected4 = summarize_selected4_paths(
                evidence_path=args.selected4_evidence_path,
                compare_path=args.selected4_compare_path,
                cpp_result_path=args.selected4_cpp_result_path,
            )
            d246 = summarize_d246_path(args.d246_sidecar_path)
            if args.include_runtime_audit:
                test_binary = resolve_test_binary(args.test_binary, root)
                scoped_gate = summarize_scoped_gate_runtime(test_binary, root)
                fingerprints = summarize_fingerprint_runtime(test_binary, root)
            else:
                scoped_gate = summarize_scoped_gate_pinned()
                fingerprints = summarize_fingerprint_pinned()
            evidence_sources = build_evidence_sources(
                root=root,
                first_evidence_path=args.first_evidence_path,
                first_compare_path=args.first_compare_path,
                first_cpp_result_path=args.first_cpp_result_path,
                selected4_evidence_path=args.selected4_evidence_path,
                selected4_compare_path=args.selected4_compare_path,
                selected4_cpp_result_path=args.selected4_cpp_result_path,
                d246_sidecar_path=args.d246_sidecar_path,
            )
            summary = summarize_payloads(
                first=first,
                selected4=selected4,
                d246=d246,
                scoped_gate=scoped_gate,
                fingerprints=fingerprints,
                evidence_sources=evidence_sources,
            )
        if args.summary_path is not None:
            write_json(resolve_repo_path(root, args.summary_path), summary)
        if args.format == "json" or args.self_check:
            print_json(summary)
        else:
            print(render_text(summary))
        return 0
    except Exception as error:  # noqa: BLE001 - command-line summary should fail closed.
        print(f"error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
