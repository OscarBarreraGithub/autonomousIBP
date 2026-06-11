#!/usr/bin/env python3
"""CTest gate for the pinned b63n parity status JSON contract."""

from __future__ import annotations

import argparse
import copy
import difflib
import json
import subprocess
import sys
from pathlib import Path
from typing import Any, Callable


EXPECTED_JSON_FIXTURE = Path(
    "tools/reference-harness/specs/release/b63n-parity-status-summary.fixture.json"
)
EXPECTED_BLOCKERS = [
    (
        "D2/D4/D6 weighted-residue evidence remains a skeleton sidecar with "
        "publication blockers"
    ),
    (
        "full automatic_phasespace weighted target phase[1,2,1,1,1,1,1] has no "
        "published high-precision AMFlow coefficient packet"
    ),
    "full eta=0 contour execution remains deferred",
]
EXPECTED_D246_PUBLICATION_BLOCKERS = [
    "precision_requested_digits unset; rerun or recapture the upstream Mathematica surface at reviewed high precision",
    "eps_order_requested unset; publish a contiguous epsilon range matching the runtime scope",
    "run_command, run_log, raw_output, and raw_output_sha256 unset",
    "D2, D4, and D6 coefficient arrays are empty for the full j[phase,1,2,1,1,1,1,1] target",
    "independent comparison artifacts and 50-digit agreement claims are absent",
]
EXPECTED_WITHHELD_CLAIMS = [
    (
        "This summary reads committed b63n evidence and optional runtime audit "
        "fingerprints only."
    ),
    "This summary does not rerun AMFlow numerics.",
    "This summary does not claim Milestone M6 closure.",
    "This summary does not claim Milestone M7 closure.",
    "This summary does not claim release readiness.",
    "This summary does not claim full eta=0 contour execution.",
    "This summary does not publish D2/D4/D6 weighted-residue coefficients.",
    "This summary does not widen runtime or public behavior.",
]
EXPECTED_SELECTED4_TRANSPORTED_INTEGRALS = [
    "phase[1,0,1,0,1,0,0]",
    "phase[1,-1,1,0,1,0,0]",
    "phase[1,1,1,0,1,0,1]",
    "phase[1,1,1,1,1,1,1]",
]
EXPECTED_SELECTED4_COMPARED_COEFFICIENTS = 19
EXPECTED_SELECTED4_MINIMUM_DIGIT_AGREEMENT = 999
EXPECTED_FIRST_INTEGRAL = "phase[1,0,1,0,1,0,0]"
EXPECTED_FIRST_ORDERS = [0, 1, 2, 3]
EXPECTED_SCOPED_GATE_LABELS = [
    "blocked-D2-scoped-weighted-residue",
    "published-D7-scoped-weighted-residue",
]
EXPECTED_EVIDENCE_SOURCES = {
    "d246_sidecar": (
        "tools/reference-harness/specs/m6/lane146/"
        "b63n-d246-weighted-residue-reference-evidence.json"
    ),
    "first_compare": (
        "tools/reference-harness/specs/m6/lane143/"
        "automatic_phasespace.first-cutkosky.compare30.json"
    ),
    "first_cpp_result": (
        "tools/reference-harness/specs/m6/lane143/"
        "automatic_phasespace.first-cutkosky.cpp-result.json"
    ),
    "first_evidence": (
        "tools/reference-harness/specs/m6/lane143/"
        "b63n-first-real-coefficient-evidence.json"
    ),
    "selected4_compare": (
        "tools/reference-harness/specs/m6/lane146/"
        "automatic_phasespace.selected4-cutkosky.compare30.json"
    ),
    "selected4_cpp_result": (
        "tools/reference-harness/specs/m6/lane146/"
        "automatic_phasespace.selected4-cutkosky.cpp-result.json"
    ),
    "selected4_evidence": (
        "tools/reference-harness/specs/m6/lane146/"
        "b63n-selected4-real-coefficients-evidence.json"
    ),
}


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def read_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as stream:
        payload = json.load(stream)
    if not isinstance(payload, dict):
        raise RuntimeError(f"{path} must contain a JSON object")
    return payload


def canonical_json(payload: dict[str, Any]) -> str:
    return json.dumps(payload, indent=2, sort_keys=True) + "\n"


def print_json_diff(expected: dict[str, Any], actual: dict[str, Any]) -> None:
    diff = difflib.unified_diff(
        canonical_json(expected).splitlines(keepends=True),
        canonical_json(actual).splitlines(keepends=True),
        fromfile=str(EXPECTED_JSON_FIXTURE),
        tofile="b63n_parity_status_summary.py --format json",
        n=3,
    )
    print("b63n parity status JSON fixture drifted", file=sys.stderr)
    print("".join(diff), file=sys.stderr)


def require_object(raw: Any, label: str) -> dict[str, Any]:
    expect(isinstance(raw, dict), f"{label} must be an object")
    return raw


def require_list(raw: Any, label: str) -> list[Any]:
    expect(isinstance(raw, list), f"{label} must be a list")
    return raw


def require_exact(raw: Any, expected: Any, label: str) -> None:
    expect(raw == expected, f"{label} must be {expected!r}, got {raw!r}")


def validate_summary_contract(payload: dict[str, Any], *, label: str) -> None:
    require_exact(payload.get("schema_version"), 1, f"{label}.schema_version")
    require_exact(
        payload.get("summary_id"),
        "b63n-post-m7-parity-status-v1",
        f"{label}.summary_id",
    )
    require_exact(
        payload.get("status"),
        "blocked-full-weighted-residue-surface",
        f"{label}.status",
    )
    require_exact(payload.get("inputs_verified"), True, f"{label}.inputs_verified")
    require_exact(
        require_object(payload.get("evidence_sources"), f"{label}.evidence_sources"),
        EXPECTED_EVIDENCE_SOURCES,
        f"{label}.evidence_sources",
    )

    first = require_object(
        payload.get("first_coefficient"),
        f"{label}.first_coefficient",
    )
    require_exact(first.get("lane"), "lane143", f"{label}.first_coefficient.lane")
    require_exact(
        first.get("integral"),
        EXPECTED_FIRST_INTEGRAL,
        f"{label}.first_coefficient.integral",
    )
    require_exact(
        first.get("orders"),
        EXPECTED_FIRST_ORDERS,
        f"{label}.first_coefficient.orders",
    )
    require_exact(
        first.get("compared_coefficient_count"),
        len(EXPECTED_FIRST_ORDERS),
        f"{label}.first_coefficient.compared_coefficient_count",
    )
    require_exact(
        first.get("minimum_digit_agreement"),
        999,
        f"{label}.first_coefficient.minimum_digit_agreement",
    )
    require_exact(
        first.get("full_eta_zero_contour_applied"),
        False,
        f"{label}.first_coefficient.full_eta_zero_contour_applied",
    )

    selected4 = require_object(
        payload.get("selected4_parity"),
        f"{label}.selected4_parity",
    )
    require_exact(selected4.get("lane"), "lane146", f"{label}.selected4_parity.lane")
    require_exact(
        selected4.get("transported_integrals"),
        EXPECTED_SELECTED4_TRANSPORTED_INTEGRALS,
        f"{label}.selected4_parity.transported_integrals",
    )
    require_exact(
        selected4.get("transported_integral_count"),
        len(EXPECTED_SELECTED4_TRANSPORTED_INTEGRALS),
        f"{label}.selected4_parity.transported_integral_count",
    )
    require_exact(
        selected4.get("compared_coefficient_count"),
        EXPECTED_SELECTED4_COMPARED_COEFFICIENTS,
        f"{label}.selected4_parity.compared_coefficient_count",
    )
    require_exact(
        selected4.get("minimum_digit_agreement"),
        EXPECTED_SELECTED4_MINIMUM_DIGIT_AGREEMENT,
        f"{label}.selected4_parity.minimum_digit_agreement",
    )
    require_exact(
        selected4.get("published_d7_integral"),
        "phase[1,1,1,0,1,0,1]",
        f"{label}.selected4_parity.published_d7_integral",
    )
    require_exact(
        selected4.get("published_d7_orders"),
        [0, 1, 2, 3],
        f"{label}.selected4_parity.published_d7_orders",
    )
    require_exact(
        selected4.get("full_eta_zero_contour_applied"),
        False,
        f"{label}.selected4_parity.full_eta_zero_contour_applied",
    )

    d246 = require_object(
        payload.get("d246_weighted_residue_surface"),
        f"{label}.d246_weighted_residue_surface",
    )
    require_exact(
        d246.get("surface_label"),
        "phase[1,2,1,1,1,1,1]",
        f"{label}.d246_weighted_residue_surface.surface_label",
    )
    require_exact(
        d246.get("weights"),
        ["D2", "D4", "D6"],
        f"{label}.d246_weighted_residue_surface.weights",
    )
    require_exact(
        d246.get("published_evidence"),
        False,
        f"{label}.d246_weighted_residue_surface.published_evidence",
    )
    require_exact(
        d246.get("skeleton_evidence"),
        True,
        f"{label}.d246_weighted_residue_surface.skeleton_evidence",
    )
    d246_blockers = require_list(
        d246.get("publication_blockers"),
        f"{label}.d246_weighted_residue_surface.publication_blockers",
    )
    require_exact(
        d246_blockers,
        EXPECTED_D246_PUBLICATION_BLOCKERS,
        f"{label}.d246_weighted_residue_surface.publication_blockers",
    )

    scoped_gate = require_object(
        payload.get("scoped_gate_audit"),
        f"{label}.scoped_gate_audit",
    )
    require_exact(
        scoped_gate.get("runtime_checked"),
        False,
        f"{label}.scoped_gate_audit.runtime_checked",
    )
    require_exact(
        scoped_gate.get("entry_count"),
        None,
        f"{label}.scoped_gate_audit.entry_count",
    )
    require_exact(
        scoped_gate.get("passed"),
        None,
        f"{label}.scoped_gate_audit.passed",
    )
    require_exact(
        scoped_gate.get("queried_labels"),
        EXPECTED_SCOPED_GATE_LABELS,
        f"{label}.scoped_gate_audit.queried_labels",
    )
    require_exact(
        scoped_gate.get("queried_weights"),
        ["D2", "D7"],
        f"{label}.scoped_gate_audit.queried_weights",
    )

    audit_fingerprints = require_object(
        payload.get("audit_fingerprints"),
        f"{label}.audit_fingerprints",
    )
    require_exact(
        audit_fingerprints.get("runtime_checked"),
        False,
        f"{label}.audit_fingerprints.runtime_checked",
    )
    require_exact(
        audit_fingerprints.get("entry_count"),
        None,
        f"{label}.audit_fingerprints.entry_count",
    )
    require_exact(
        audit_fingerprints.get("pins_match"),
        None,
        f"{label}.audit_fingerprints.pins_match",
    )

    require_exact(
        require_list(payload.get("blockers"), f"{label}.blockers"),
        EXPECTED_BLOCKERS,
        f"{label}.blockers",
    )
    require_exact(
        require_list(payload.get("withheld_claims"), f"{label}.withheld_claims"),
        EXPECTED_WITHHELD_CLAIMS,
        f"{label}.withheld_claims",
    )


def parse_summary_json(raw_output: str) -> dict[str, Any]:
    try:
        actual = json.loads(raw_output)
    except json.JSONDecodeError as error:
        raise RuntimeError(f"b63n parity status JSON output is not valid JSON: {error}") from error
    if not isinstance(actual, dict):
        raise RuntimeError("b63n parity status JSON output must be a JSON object")
    return actual


def fixture_matches(expected: dict[str, Any], actual: dict[str, Any]) -> bool:
    validate_summary_contract(expected, label="expected fixture")
    validate_summary_contract(actual, label="actual summary")
    return actual == expected


def run_fixture_gate(root: Path) -> int:
    completed = subprocess.run(
        [
            sys.executable,
            str(root / "tools/reference-harness/scripts/b63n_parity_status_summary.py"),
            "--format",
            "json",
        ],
        cwd=root,
        text=True,
        capture_output=True,
        check=False,
    )
    if completed.returncode != 0:
        print("b63n_parity_status_summary.py JSON mode failed", file=sys.stderr)
        if completed.stdout:
            print("stdout:", file=sys.stderr)
            print(completed.stdout, file=sys.stderr)
        if completed.stderr:
            print("stderr:", file=sys.stderr)
            print(completed.stderr, file=sys.stderr)
        return completed.returncode

    try:
        actual = parse_summary_json(completed.stdout)
    except RuntimeError as error:
        print(error, file=sys.stderr)
        print(completed.stdout, file=sys.stderr)
        return 1

    expected = read_json(root / EXPECTED_JSON_FIXTURE)
    try:
        matches = fixture_matches(expected, actual)
    except RuntimeError as error:
        print(f"b63n parity status JSON fixture contract failed: {error}", file=sys.stderr)
        return 1

    if not matches:
        print_json_diff(expected, actual)
        return 1

    print("b63n parity status JSON fixture gate passed")
    return 0


def rejected(check: Callable[[], Any], expected_message: str) -> bool:
    try:
        check()
    except Exception as error:  # noqa: BLE001 - self-check intentionally probes failures.
        return expected_message in str(error)
    return False


def run_self_check(root: Path) -> int:
    expected = read_json(root / EXPECTED_JSON_FIXTURE)
    status_drift = copy.deepcopy(expected)
    status_drift["status"] = "synthetic-b63n-status-drift"

    promoted_d246 = copy.deepcopy(expected)
    promoted_d246["d246_weighted_residue_surface"]["published_evidence"] = True
    promoted_d246["d246_weighted_residue_surface"]["skeleton_evidence"] = False

    promoted_full_contour = copy.deepcopy(expected)
    promoted_full_contour["selected4_parity"]["full_eta_zero_contour_applied"] = True

    first_integral_drift = copy.deepcopy(expected)
    first_integral_drift["first_coefficient"]["integral"] = "phase[1,1,1,0,1,0,1]"

    first_full_contour_promotion = copy.deepcopy(expected)
    first_full_contour_promotion["first_coefficient"]["full_eta_zero_contour_applied"] = True

    digit_floor_drift = copy.deepcopy(expected)
    digit_floor_drift["first_coefficient"]["minimum_digit_agreement"] = 998

    missing_blockers = copy.deepcopy(expected)
    missing_blockers["blockers"] = []

    thin_d246_blockers = copy.deepcopy(expected)
    thin_d246_blockers["d246_weighted_residue_surface"]["publication_blockers"] = []

    stale_d246_blockers = copy.deepcopy(expected)
    stale_d246_blockers["d246_weighted_residue_surface"]["publication_blockers"][3] = (
        "D2, D4, and D6 coefficient arrays are empty"
    )

    wrong_transported_count = copy.deepcopy(expected)
    wrong_transported_count["selected4_parity"]["transported_integral_count"] -= 1

    selected4_compared_count_drift = copy.deepcopy(expected)
    selected4_compared_count_drift["selected4_parity"]["compared_coefficient_count"] -= 1

    selected4_digit_floor_drift = copy.deepcopy(expected)
    selected4_digit_floor_drift["selected4_parity"]["minimum_digit_agreement"] = 998

    stale_scoped_gate = copy.deepcopy(expected)
    stale_scoped_gate["scoped_gate_audit"]["queried_weights"] = ["D7", "D2"]

    promoted_scoped_gate_runtime_check = copy.deepcopy(expected)
    promoted_scoped_gate_runtime_check["scoped_gate_audit"]["runtime_checked"] = True

    scoped_gate_entry_count_drift = copy.deepcopy(expected)
    scoped_gate_entry_count_drift["scoped_gate_audit"]["entry_count"] = 2

    promoted_audit_fingerprint_runtime_check = copy.deepcopy(expected)
    promoted_audit_fingerprint_runtime_check["audit_fingerprints"]["runtime_checked"] = True

    audit_fingerprint_entry_count_drift = copy.deepcopy(expected)
    audit_fingerprint_entry_count_drift["audit_fingerprints"]["entry_count"] = 6

    audit_fingerprint_pin_match_drift = copy.deepcopy(expected)
    audit_fingerprint_pin_match_drift["audit_fingerprints"]["pins_match"] = True

    evidence_source_drift = copy.deepcopy(expected)
    evidence_source_drift["evidence_sources"]["selected4_compare"] = (
        "tools/reference-harness/specs/m6/lane146/synthetic-b63n-selected4.compare30.json"
    )

    missing_evidence_sources = copy.deepcopy(expected)
    missing_evidence_sources.pop("evidence_sources", None)

    missing_withheld_claims = copy.deepcopy(expected)
    missing_withheld_claims.pop("withheld_claims", None)

    checks = {
        "accepts_current_fixture": fixture_matches(expected, copy.deepcopy(expected)),
        "rejects_status_drift": rejected(
            lambda: fixture_matches(expected, status_drift),
            "actual summary.status",
        ),
        "rejects_d246_silent_promotion": rejected(
            lambda: fixture_matches(expected, promoted_d246),
            "d246_weighted_residue_surface.published_evidence",
        ),
        "rejects_full_contour_promotion": rejected(
            lambda: fixture_matches(expected, promoted_full_contour),
            "selected4_parity.full_eta_zero_contour_applied",
        ),
        "rejects_first_coefficient_integral_drift": rejected(
            lambda: fixture_matches(expected, first_integral_drift),
            "first_coefficient.integral",
        ),
        "rejects_first_coefficient_full_contour_promotion": rejected(
            lambda: fixture_matches(expected, first_full_contour_promotion),
            "first_coefficient.full_eta_zero_contour_applied",
        ),
        "rejects_first_coefficient_digit_floor_drift": rejected(
            lambda: fixture_matches(expected, digit_floor_drift),
            "first_coefficient.minimum_digit_agreement",
        ),
        "rejects_missing_withheld_claims": rejected(
            lambda: fixture_matches(expected, missing_withheld_claims),
            "actual summary.withheld_claims",
        ),
        "rejects_missing_blockers": rejected(
            lambda: validate_summary_contract(missing_blockers, label="synthetic summary"),
            "synthetic summary.blockers",
        ),
        "rejects_empty_d246_publication_blockers": rejected(
            lambda: validate_summary_contract(thin_d246_blockers, label="synthetic summary"),
            "d246_weighted_residue_surface.publication_blockers",
        ),
        "rejects_stale_d246_publication_blockers": rejected(
            lambda: validate_summary_contract(stale_d246_blockers, label="synthetic summary"),
            "d246_weighted_residue_surface.publication_blockers",
        ),
        "rejects_selected4_transported_count_drift": rejected(
            lambda: validate_summary_contract(wrong_transported_count, label="synthetic summary"),
            "transported_integral_count",
        ),
        "rejects_selected4_compared_count_drift": rejected(
            lambda: validate_summary_contract(
                selected4_compared_count_drift,
                label="synthetic summary",
            ),
            "selected4_parity.compared_coefficient_count",
        ),
        "rejects_selected4_digit_floor_drift": rejected(
            lambda: validate_summary_contract(
                selected4_digit_floor_drift,
                label="synthetic summary",
            ),
            "selected4_parity.minimum_digit_agreement",
        ),
        "rejects_scoped_gate_weight_order_drift": rejected(
            lambda: validate_summary_contract(stale_scoped_gate, label="synthetic summary"),
            "scoped_gate_audit.queried_weights",
        ),
        "rejects_scoped_gate_runtime_check_promotion": rejected(
            lambda: validate_summary_contract(
                promoted_scoped_gate_runtime_check,
                label="synthetic summary",
            ),
            "scoped_gate_audit.runtime_checked",
        ),
        "rejects_scoped_gate_entry_count_drift": rejected(
            lambda: validate_summary_contract(
                scoped_gate_entry_count_drift,
                label="synthetic summary",
            ),
            "scoped_gate_audit.entry_count",
        ),
        "rejects_audit_fingerprint_runtime_check_promotion": rejected(
            lambda: validate_summary_contract(
                promoted_audit_fingerprint_runtime_check,
                label="synthetic summary",
            ),
            "audit_fingerprints.runtime_checked",
        ),
        "rejects_audit_fingerprint_entry_count_drift": rejected(
            lambda: validate_summary_contract(
                audit_fingerprint_entry_count_drift,
                label="synthetic summary",
            ),
            "audit_fingerprints.entry_count",
        ),
        "rejects_audit_fingerprint_pin_match_drift": rejected(
            lambda: validate_summary_contract(
                audit_fingerprint_pin_match_drift,
                label="synthetic summary",
            ),
            "audit_fingerprints.pins_match",
        ),
        "rejects_evidence_source_drift": rejected(
            lambda: fixture_matches(expected, evidence_source_drift),
            "actual summary.evidence_sources",
        ),
        "rejects_missing_evidence_sources": rejected(
            lambda: fixture_matches(expected, missing_evidence_sources),
            "actual summary.evidence_sources",
        ),
        "rejects_invalid_json": rejected(
            lambda: parse_summary_json("{"),
            "not valid JSON",
        ),
        "rejects_non_object_json": rejected(
            lambda: parse_summary_json("[]"),
            "must be a JSON object",
        ),
    }
    if not all(checks.values()):
        print(
            json.dumps(
                {"self_check_passed": False, "checks": checks},
                indent=2,
                sort_keys=True,
            )
        )
        return 1

    print(
        json.dumps(
            {
                "schema_version": 1,
                "fixture": str(EXPECTED_JSON_FIXTURE),
                "self_check_passed": True,
                "checks": checks,
            },
            indent=2,
            sort_keys=True,
        )
    )
    return 0


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--self-check",
        action="store_true",
        help="Run synthetic fixture-gate checks",
    )
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    root = repo_root()
    if args.self_check:
        return run_self_check(root)
    return run_fixture_gate(root)


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
