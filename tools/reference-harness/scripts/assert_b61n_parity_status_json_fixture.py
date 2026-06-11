#!/usr/bin/env python3
"""CTest gate for the pinned b61n parity status JSON contract."""

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
    "tools/reference-harness/specs/release/b61n-parity-status-summary.fixture.json"
)
EXPECTED_BLOCKERS = [
    "row 5/6 comparison is matched to retained AMFlow reference floor, not 50-digit parity",
    "publication gate remains blocked by the b61n AMFlow cross-comparator floor",
]
EXPECTED_WITHHELD_CLAIMS = [
    "This summary reads committed b61n evidence and optional audit fingerprints only.",
    "This summary does not rerun AMFlow numerics.",
    "This summary does not claim Milestone M6 closure.",
    "This summary does not claim Milestone M7 closure.",
    "This summary does not claim release readiness.",
    "This summary does not claim full eta=0 contour execution.",
    "This summary does not widen runtime or public behavior.",
]
EXPECTED_REFERENCE_FLOOR_TARGETS = [
    {
        "integral": "box[1,0,1,1]",
        "order": 0,
        "reference_floor_id": "b61n-row5-eps0-retained-amflow-floor",
        "reference_floor_imag_digits": 11,
        "reference_floor_real_digits": 11,
    },
    {
        "integral": "box[1,1,1,1]",
        "order": -2,
        "reference_floor_id": "b61n-row6-eps-2-retained-amflow-floor",
        "reference_floor_imag_digits": 46,
        "reference_floor_real_digits": 46,
    },
    {
        "integral": "box[1,1,1,1]",
        "order": -1,
        "reference_floor_id": "b61n-row6-eps-1-retained-amflow-floor",
        "reference_floor_imag_digits": 13,
        "reference_floor_real_digits": 12,
    },
    {
        "integral": "box[1,1,1,1]",
        "order": 0,
        "reference_floor_id": "b61n-row6-eps0-retained-amflow-floor",
        "reference_floor_imag_digits": 12,
        "reference_floor_real_digits": 12,
    },
]
EXPECTED_AUDIT_FINGERPRINT_ENTRIES = [
    {
        "category": "b61n-publication-contour-evaluation",
        "label": "published-lane142-primitive-bubble",
        "pinned_fingerprint": "fnv1a64:92f403f7f2c701d5",
    }
]
EXPECTED_EVIDENCE_SOURCES = {
    "precision_evidence": (
        "tools/reference-harness/specs/m7/lane2/b61n-pipeline-precision-evidence.json"
    ),
    "precision_source_cpp_result": (
        "tools/reference-harness/specs/m7/lane2/"
        "complex_kinematics.c267-stripped.eps0.cpp-result.json"
    ),
    "precision_source_diagnostic": (
        "tools/reference-harness/specs/m7/lane2/b61n-row56-specific-target-diagnostic.json"
    ),
    "publication_precision_evidence": (
        "tools/reference-harness/specs/m7/lane2/b61n-pipeline-precision-evidence.json"
    ),
    "publication_precision_source_cpp_result": (
        "tools/reference-harness/specs/m7/lane2/"
        "complex_kinematics.c267-stripped.eps0.cpp-result.json"
    ),
    "publication_qualifier_sidecar": (
        "tools/reference-harness/specs/m6/lane5-next7/b61n-publication-qualifier-hook.json"
    ),
    "reference_floor_amflow_golden": (
        "tools/reference-harness/specs/m7/lane2/"
        "complex_kinematics.b61n-reference-floor-golden-manifest.json"
    ),
    "reference_floor_cpp_result": (
        "tools/reference-harness/specs/m7/lane2/"
        "complex_kinematics.c267-stripped.eps0.cpp-result.json"
    ),
    "reference_floor_retained_comparison": (
        "tools/reference-harness/specs/m7/lane2/"
        "complex_kinematics.c267-stripped.eps0.compare50.reference-floor.json"
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
        tofile="b61n_parity_status_summary.py --format json",
        n=3,
    )
    print("b61n parity status JSON fixture drifted", file=sys.stderr)
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
        "b61n-post-m7-parity-status-v1",
        f"{label}.summary_id",
    )
    require_exact(
        payload.get("status"),
        "blocked-reference-floor-limited",
        f"{label}.status",
    )
    require_exact(payload.get("inputs_verified"), True, f"{label}.inputs_verified")
    require_exact(
        require_object(payload.get("evidence_sources"), f"{label}.evidence_sources"),
        EXPECTED_EVIDENCE_SOURCES,
        f"{label}.evidence_sources",
    )

    reference_floor = require_object(
        payload.get("reference_floor"),
        f"{label}.reference_floor",
    )
    require_exact(
        reference_floor.get("comparison_verdict"),
        "matched-to-reference-floor",
        f"{label}.reference_floor.comparison_verdict",
    )
    require_exact(
        reference_floor.get("compared_coefficient_count"),
        14,
        f"{label}.reference_floor.compared_coefficient_count",
    )
    require_exact(
        reference_floor.get("matched_to_tolerance_count"),
        10,
        f"{label}.reference_floor.matched_to_tolerance_count",
    )
    require_exact(
        reference_floor.get("matched_to_reference_floor_count"),
        len(EXPECTED_REFERENCE_FLOOR_TARGETS),
        f"{label}.reference_floor.matched_to_reference_floor_count",
    )
    require_exact(
        reference_floor.get("minimum_digit_agreement"),
        11,
        f"{label}.reference_floor.minimum_digit_agreement",
    )
    require_exact(
        require_list(reference_floor.get("targets"), f"{label}.reference_floor.targets"),
        EXPECTED_REFERENCE_FLOOR_TARGETS,
        f"{label}.reference_floor.targets",
    )

    precision = require_object(payload.get("precision_uplift"), f"{label}.precision_uplift")
    require_exact(
        precision.get("target_count"),
        len(EXPECTED_REFERENCE_FLOOR_TARGETS),
        f"{label}.precision_uplift.target_count",
    )
    require_exact(
        precision.get("component_count"),
        8,
        f"{label}.precision_uplift.component_count",
    )
    require_exact(
        precision.get("minimum_reference_floor_digits"),
        11,
        f"{label}.precision_uplift.minimum_reference_floor_digits",
    )
    require_exact(
        precision.get("minimum_standard_fraction_digits"),
        80,
        f"{label}.precision_uplift.minimum_standard_fraction_digits",
    )
    require_exact(
        precision.get("minimum_uplifted_fraction_digits"),
        160,
        f"{label}.precision_uplift.minimum_uplifted_fraction_digits",
    )
    require_exact(
        precision.get("amflow_reference_values_used_for_digit_count"),
        False,
        f"{label}.precision_uplift.amflow_reference_values_used_for_digit_count",
    )

    publication = require_object(payload.get("publication_gate"), f"{label}.publication_gate")
    require_exact(
        publication.get("variant_count"),
        5,
        f"{label}.publication_gate.variant_count",
    )
    require_exact(
        publication.get("blocked_variant_count"),
        5,
        f"{label}.publication_gate.blocked_variant_count",
    )
    require_exact(
        publication.get("minimum_digit_agreement_required"),
        50,
        f"{label}.publication_gate.minimum_digit_agreement_required",
    )
    require_exact(
        publication.get("minimum_digit_agreement_observed"),
        2,
        f"{label}.publication_gate.minimum_digit_agreement_observed",
    )
    require_exact(
        publication.get("gate_passed"),
        False,
        f"{label}.publication_gate.gate_passed",
    )
    require_exact(
        publication.get("m6_qualifier_hook_prepositioned"),
        True,
        f"{label}.publication_gate.m6_qualifier_hook_prepositioned",
    )
    require_exact(
        publication.get("m6_qualifier_hook_currently_promoted"),
        False,
        f"{label}.publication_gate.m6_qualifier_hook_currently_promoted",
    )
    require_exact(
        publication.get("m7_parity_single_row_hook_prepositioned"),
        True,
        f"{label}.publication_gate.m7_parity_single_row_hook_prepositioned",
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
        len(EXPECTED_AUDIT_FINGERPRINT_ENTRIES),
        f"{label}.audit_fingerprints.entry_count",
    )
    require_exact(
        audit_fingerprints.get("pins_match"),
        None,
        f"{label}.audit_fingerprints.pins_match",
    )
    require_exact(
        require_list(audit_fingerprints.get("entries"), f"{label}.audit_fingerprints.entries"),
        EXPECTED_AUDIT_FINGERPRINT_ENTRIES,
        f"{label}.audit_fingerprints.entries",
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
        raise RuntimeError(f"b61n parity status JSON output is not valid JSON: {error}") from error
    if not isinstance(actual, dict):
        raise RuntimeError("b61n parity status JSON output must be a JSON object")
    return actual


def fixture_matches(expected: dict[str, Any], actual: dict[str, Any]) -> bool:
    validate_summary_contract(expected, label="expected fixture")
    validate_summary_contract(actual, label="actual summary")
    return actual == expected


def run_fixture_gate(root: Path) -> int:
    completed = subprocess.run(
        [
            sys.executable,
            str(root / "tools/reference-harness/scripts/b61n_parity_status_summary.py"),
            "--format",
            "json",
        ],
        cwd=root,
        text=True,
        capture_output=True,
        check=False,
    )
    if completed.returncode != 0:
        print("b61n_parity_status_summary.py JSON mode failed", file=sys.stderr)
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
        print(f"b61n parity status JSON fixture contract failed: {error}", file=sys.stderr)
        return 1

    if not matches:
        print_json_diff(expected, actual)
        return 1

    print("b61n parity status JSON fixture gate passed")
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
    status_drift["status"] = "synthetic-b61n-status-drift"

    promoted_publication_gate = copy.deepcopy(expected)
    promoted_publication_gate["publication_gate"]["gate_passed"] = True

    publication_variant_drift = copy.deepcopy(expected)
    publication_variant_drift["publication_gate"]["blocked_variant_count"] -= 1

    publication_digit_floor_drift = copy.deepcopy(expected)
    publication_digit_floor_drift["publication_gate"]["minimum_digit_agreement_required"] = 49

    precision_source_drift = copy.deepcopy(expected)
    precision_source_drift["precision_uplift"][
        "amflow_reference_values_used_for_digit_count"
    ] = True

    row56_target_floor_drift = copy.deepcopy(expected)
    row56_target_floor_drift["reference_floor"]["targets"][0][
        "reference_floor_real_digits"
    ] += 1

    row56_target_identity_drift = copy.deepcopy(expected)
    row56_target_identity_drift["reference_floor"]["targets"][0]["reference_floor_id"] = (
        "synthetic-b61n-reference-floor-id"
    )

    missing_blocker = copy.deepcopy(expected)
    missing_blocker["blockers"] = missing_blocker["blockers"][:-1]

    audit_runtime_check_drift = copy.deepcopy(expected)
    audit_runtime_check_drift["audit_fingerprints"]["runtime_checked"] = True

    audit_entry_count_drift = copy.deepcopy(expected)
    audit_entry_count_drift["audit_fingerprints"]["entry_count"] = 2

    audit_fingerprint_pin_drift = copy.deepcopy(expected)
    audit_fingerprint_pin_drift["audit_fingerprints"]["entries"][0]["pinned_fingerprint"] = (
        "fnv1a64:syntheticb61n"
    )

    evidence_source_drift = copy.deepcopy(expected)
    evidence_source_drift["evidence_sources"]["precision_source_cpp_result"] = (
        "tools/reference-harness/specs/m7/lane2/synthetic-b61n-source.cpp-result.json"
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
        "rejects_publication_gate_promotion": rejected(
            lambda: fixture_matches(expected, promoted_publication_gate),
            "actual summary.publication_gate.gate_passed",
        ),
        "rejects_publication_variant_drift": rejected(
            lambda: fixture_matches(expected, publication_variant_drift),
            "actual summary.publication_gate.blocked_variant_count",
        ),
        "rejects_publication_digit_floor_drift": rejected(
            lambda: fixture_matches(expected, publication_digit_floor_drift),
            "actual summary.publication_gate.minimum_digit_agreement_required",
        ),
        "rejects_precision_source_drift": rejected(
            lambda: fixture_matches(expected, precision_source_drift),
            "actual summary.precision_uplift.amflow_reference_values_used_for_digit_count",
        ),
        "rejects_row56_target_floor_drift": rejected(
            lambda: fixture_matches(expected, row56_target_floor_drift),
            "actual summary.reference_floor.targets",
        ),
        "rejects_row56_target_identity_drift": rejected(
            lambda: fixture_matches(expected, row56_target_identity_drift),
            "actual summary.reference_floor.targets",
        ),
        "rejects_missing_blocker": rejected(
            lambda: fixture_matches(expected, missing_blocker),
            "actual summary.blockers",
        ),
        "rejects_audit_runtime_check_drift": rejected(
            lambda: fixture_matches(expected, audit_runtime_check_drift),
            "actual summary.audit_fingerprints.runtime_checked",
        ),
        "rejects_audit_entry_count_drift": rejected(
            lambda: fixture_matches(expected, audit_entry_count_drift),
            "actual summary.audit_fingerprints.entry_count",
        ),
        "rejects_audit_fingerprint_pin_drift": rejected(
            lambda: fixture_matches(expected, audit_fingerprint_pin_drift),
            "actual summary.audit_fingerprints.entries",
        ),
        "rejects_evidence_source_drift": rejected(
            lambda: fixture_matches(expected, evidence_source_drift),
            "actual summary.evidence_sources",
        ),
        "rejects_missing_evidence_sources": rejected(
            lambda: fixture_matches(expected, missing_evidence_sources),
            "actual summary.evidence_sources",
        ),
        "rejects_missing_withheld_claims": rejected(
            lambda: fixture_matches(expected, missing_withheld_claims),
            "actual summary.withheld_claims",
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
