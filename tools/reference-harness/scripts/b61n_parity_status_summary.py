#!/usr/bin/env python3
"""Summarize the current post-M7 b61n parity status from committed evidence."""

from __future__ import annotations

import argparse
import copy
import json
import sys
from pathlib import Path
from typing import Any

from audit_b61n_publication_qualifier import (
    audit_sidecar,
    default_publication_qualifier_sidecar_path,
)
from regenerate_b61n_publication_audit_fingerprints import (
    EXPECTED_PINS,
    build_regeneration_payload,
    validate_payload as validate_fingerprint_payload,
)
from verify_b61n_precision_uplift_monotonicity import (
    DEFAULT_PRECISION_EVIDENCE,
    verify_paths as verify_precision_paths,
)
from verify_b61n_publication_audit_trail import resolve_test_binary
from verify_b61n_reference_floor_parity import (
    DEFAULT_AMFLOW_GOLDEN,
    DEFAULT_CPP_RESULT,
    DEFAULT_RETAINED_COMPARISON,
    repo_root,
    verify_paths as verify_reference_floor_paths,
)


WITHHELD_CLAIMS: tuple[str, ...] = (
    "This summary reads committed b61n evidence and optional audit fingerprints only.",
    "This summary does not rerun AMFlow numerics.",
    "This summary does not claim Milestone M6 closure.",
    "This summary does not claim Milestone M7 closure.",
    "This summary does not claim release readiness.",
    "This summary does not claim full eta=0 contour execution.",
    "This summary does not widen runtime or public behavior.",
)


class StatusSummaryError(RuntimeError):
    """Raised when b61n parity status inputs cannot be summarized."""


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise StatusSummaryError(message)


def resolve_repo_path(root: Path, path: Path) -> Path:
    return path if path.is_absolute() else root / path


def print_json(payload: dict[str, Any]) -> None:
    print(json.dumps(payload, indent=2, sort_keys=True))


def write_json(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def require_int(payload: dict[str, Any], field: str) -> int:
    value = payload.get(field)
    expect(type(value) is int, f"{field} must be an int")
    return value


def require_bool(payload: dict[str, Any], field: str) -> bool:
    value = payload.get(field)
    expect(type(value) is bool, f"{field} must be a bool")
    return value


def require_string_list(payload: dict[str, Any], field: str) -> list[str]:
    value = payload.get(field)
    expect(isinstance(value, list), f"{field} must be a list")
    strings: list[str] = []
    for index, item in enumerate(value):
        expect(isinstance(item, str) and item, f"{field}[{index}] must be a non-empty string")
        strings.append(item)
    return strings


def summarize_pinned_fingerprints() -> dict[str, Any]:
    entries = [
        {
            "label": label,
            "category": pin["category"],
            "pinned_fingerprint": pin["pinned_fingerprint"],
        }
        for label, pin in sorted(EXPECTED_PINS.items())
    ]
    return {
        "runtime_checked": False,
        "entry_count": len(entries),
        "pins_match": None,
        "entries": entries,
    }


def summarize_runtime_fingerprints(test_binary: Path, root: Path) -> dict[str, Any]:
    payload = build_regeneration_payload(test_binary, root)
    validation = validate_fingerprint_payload(payload, require_matches=True)
    entries = []
    for entry in payload["entries"]:
        entries.append(
            {
                "label": entry["label"],
                "category": entry["category"],
                "fresh_fingerprint": entry["fresh_fingerprint"],
                "pinned_fingerprint": entry["pinned_fingerprint"],
                "matches_pin": entry["matches_pin"],
            }
        )
    return {
        "runtime_checked": True,
        "entry_count": validation["entry_count"],
        "pins_match": True,
        "entries": entries,
    }


def summarize_payloads(
    *,
    reference_floor: dict[str, Any],
    precision: dict[str, Any],
    publication: dict[str, Any],
    fingerprints: dict[str, Any],
) -> dict[str, Any]:
    expect(reference_floor.get("schema_version") == 1, "reference-floor schema_version must be 1")
    expect(precision.get("schema_version") == 1, "precision schema_version must be 1")
    expect(publication.get("schema_version") == 1, "publication schema_version must be 1")

    expect(reference_floor.get("cross_check_passed") is True, "reference-floor cross-check failed")
    expect(
        reference_floor.get("comparison_verdict") == "matched-to-reference-floor",
        "b61n row 5/6 status must remain matched-to-reference-floor",
    )
    reference_floor_count = require_int(
        reference_floor,
        "reference_floor_matched_coefficient_count",
    )
    tolerance_pass_count = require_int(reference_floor, "passed_coefficient_count")
    compared_count = require_int(reference_floor, "compared_coefficient_count")
    expect(reference_floor_count > 0, "reference-floor target count must be positive")
    expect(
        reference_floor_count + tolerance_pass_count == compared_count,
        "reference-floor and tolerance pass counts must cover compared coefficients",
    )
    expect(
        require_int(reference_floor, "minimum_digit_agreement") == 11,
        "minimum b61n agreement must keep the retained 11-digit floor visible",
    )
    expect(reference_floor.get("m7_closure_claimed") is False, "reference-floor summary claimed M7 closure")
    expect(
        reference_floor.get("release_readiness_claimed") is False,
        "reference-floor summary claimed release readiness",
    )

    expect(precision.get("cross_check_passed") is True, "precision uplift cross-check failed")
    precision_targets = require_int(precision, "target_count")
    expect(
        precision_targets == reference_floor_count,
        "precision target count must match the row 5/6 reference-floor target count",
    )
    min_precision_floor = require_int(precision, "minimum_reference_floor_digits")
    min_uplifted = require_int(precision, "minimum_uplifted_fraction_digits")
    expect(
        min_precision_floor == reference_floor["minimum_digit_agreement"],
        "precision minimum reference floor must match the comparator floor",
    )
    expect(min_uplifted >= 160, "precision uplift must preserve at least 160 fractional digits")
    expect(precision.get("m7_closure_claimed") is False, "precision summary claimed M7 closure")
    expect(precision.get("release_readiness_claimed") is False, "precision summary claimed release readiness")

    expect(publication.get("publication_gate_reviewed") is True, "publication gate must be reviewed")
    expect(
        publication.get("precision_evidence_sidecar_reviewed") is True,
        "publication summary must consume the precision evidence sidecar",
    )
    expect(
        publication.get("precision_evidence_source_cpp_result_bound") is True,
        "publication summary must bind precision evidence to its C++ result",
    )
    expect(
        publication.get("precision_evidence_target_count") == precision_targets,
        "publication precision target count must match precision verifier",
    )
    expect(
        publication.get("precision_evidence_not_amflow_reference_backed") is True,
        "precision uplift must remain explicitly non-AMFlow-reference-backed",
    )
    expect(
        publication.get("amflow_cross_comparator_publication_gate_required") is True,
        "publication AMFlow cross-comparator gate must be required",
    )
    expect(
        publication.get("amflow_cross_comparator_publication_gate_passed") is False,
        "publication gate must stay blocked until b61n reaches the 50-digit AMFlow floor",
    )
    blocked_variants = require_int(publication, "amflow_cross_comparator_blocked_publication_variant_count")
    variant_count = require_int(publication, "variant_count")
    expect(blocked_variants == variant_count, "every b61n publication variant must remain blocked")
    expect(
        require_int(publication, "amflow_cross_comparator_minimum_digit_agreement_required") == 50,
        "publication gate must retain the 50-digit AMFlow requirement",
    )
    expect(
        require_int(publication, "amflow_cross_comparator_minimum_digit_agreement_observed") == 2,
        "publication gate must keep the current 2-digit observed blocker visible",
    )
    expect(
        require_bool(publication, "m6_qualifier_hook_prepositioned"),
        "M6 qualifier hook must remain prepositioned",
    )
    expect(
        not require_bool(publication, "m6_qualifier_hook_currently_promoted"),
        "M6 qualifier hook must not be promoted by this summary",
    )
    expect(
        require_bool(publication, "m7_parity_single_row_hook_prepositioned"),
        "M7 parity single-row hook must remain prepositioned",
    )
    for claim in WITHHELD_CLAIMS[2:]:
        expect(
            claim in require_string_list(publication, "withheld_claims"),
            f"publication withheld_claims missing {claim!r}",
        )

    expect(
        require_int(fingerprints, "entry_count") == len(fingerprints.get("entries", [])),
        "fingerprint entry_count must match entries",
    )
    if fingerprints.get("runtime_checked") is True:
        expect(fingerprints.get("pins_match") is True, "runtime b61n audit fingerprints must match pins")

    return {
        "schema_version": 1,
        "summary_id": "b61n-post-m7-parity-status-v1",
        "status": "blocked-reference-floor-limited",
        "inputs_verified": True,
        "reference_floor": {
            "comparison_verdict": reference_floor["comparison_verdict"],
            "compared_coefficient_count": compared_count,
            "matched_to_tolerance_count": tolerance_pass_count,
            "matched_to_reference_floor_count": reference_floor_count,
            "minimum_digit_agreement": reference_floor["minimum_digit_agreement"],
            "targets": reference_floor["row56_reference_floor_targets"],
        },
        "precision_uplift": {
            "target_count": precision_targets,
            "component_count": require_int(precision, "component_count"),
            "minimum_reference_floor_digits": min_precision_floor,
            "minimum_standard_fraction_digits": require_int(
                precision,
                "minimum_standard_fraction_digits",
            ),
            "minimum_uplifted_fraction_digits": min_uplifted,
            "amflow_reference_values_used_for_digit_count": False,
        },
        "publication_gate": {
            "variant_count": variant_count,
            "blocked_variant_count": blocked_variants,
            "minimum_digit_agreement_required": publication[
                "amflow_cross_comparator_minimum_digit_agreement_required"
            ],
            "minimum_digit_agreement_observed": publication[
                "amflow_cross_comparator_minimum_digit_agreement_observed"
            ],
            "gate_passed": publication["amflow_cross_comparator_publication_gate_passed"],
            "m6_qualifier_hook_prepositioned": publication["m6_qualifier_hook_prepositioned"],
            "m6_qualifier_hook_currently_promoted": publication[
                "m6_qualifier_hook_currently_promoted"
            ],
            "m7_parity_single_row_hook_prepositioned": publication[
                "m7_parity_single_row_hook_prepositioned"
            ],
        },
        "audit_fingerprints": fingerprints,
        "blockers": [
            "row 5/6 comparison is matched to retained AMFlow reference floor, not 50-digit parity",
            "publication gate remains blocked by the b61n AMFlow cross-comparator floor",
        ],
        "withheld_claims": list(WITHHELD_CLAIMS),
    }


def render_text(summary: dict[str, Any]) -> str:
    reference = summary["reference_floor"]
    precision = summary["precision_uplift"]
    publication = summary["publication_gate"]
    fingerprints = summary["audit_fingerprints"]
    lines = [
        "b61n parity status summary",
        f"status: {summary['status']}",
        (
            "reference_floor: "
            f"verdict={reference['comparison_verdict']} "
            f"compared={reference['compared_coefficient_count']} "
            f"matched_to_50_digit={reference['matched_to_tolerance_count']} "
            f"matched_to_reference_floor={reference['matched_to_reference_floor_count']} "
            f"minimum_digits={reference['minimum_digit_agreement']}"
        ),
        (
            "precision_uplift: "
            f"targets={precision['target_count']} "
            f"components={precision['component_count']} "
            f"minimum_reference_floor_digits={precision['minimum_reference_floor_digits']} "
            f"minimum_standard_fraction_digits={precision['minimum_standard_fraction_digits']} "
            f"minimum_uplifted_fraction_digits={precision['minimum_uplifted_fraction_digits']} "
            "amflow_reference_values_used_for_digit_count=false"
        ),
        (
            "publication_gate: "
            f"variants={publication['variant_count']} "
            f"blocked={publication['blocked_variant_count']} "
            f"required_digits={publication['minimum_digit_agreement_required']} "
            f"observed_digits={publication['minimum_digit_agreement_observed']} "
            f"gate_passed={str(publication['gate_passed']).lower()} "
            f"m6_hook_promoted={str(publication['m6_qualifier_hook_currently_promoted']).lower()} "
            f"m7_hook_prepositioned={str(publication['m7_parity_single_row_hook_prepositioned']).lower()}"
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


def rejected(
    *,
    reference_floor: dict[str, Any],
    precision: dict[str, Any],
    publication: dict[str, Any],
    fingerprints: dict[str, Any],
    expected_error: str,
) -> bool:
    try:
        summarize_payloads(
            reference_floor=reference_floor,
            precision=precision,
            publication=publication,
            fingerprints=fingerprints,
        )
    except Exception as error:  # noqa: BLE001 - self-check intentionally probes failures.
        return expected_error in str(error)
    return False


def synthetic_payloads() -> tuple[dict[str, Any], dict[str, Any], dict[str, Any], dict[str, Any]]:
    targets = [
        {
            "integral": "box[1,0,1,1]",
            "order": 0,
            "reference_floor_id": "b61n-row5-eps0-retained-amflow-floor",
            "reference_floor_real_digits": 11,
            "reference_floor_imag_digits": 11,
        },
        {
            "integral": "box[1,1,1,1]",
            "order": -2,
            "reference_floor_id": "b61n-row6-eps-2-retained-amflow-floor",
            "reference_floor_real_digits": 46,
            "reference_floor_imag_digits": 46,
        },
        {
            "integral": "box[1,1,1,1]",
            "order": -1,
            "reference_floor_id": "b61n-row6-eps-1-retained-amflow-floor",
            "reference_floor_real_digits": 12,
            "reference_floor_imag_digits": 13,
        },
        {
            "integral": "box[1,1,1,1]",
            "order": 0,
            "reference_floor_id": "b61n-row6-eps0-retained-amflow-floor",
            "reference_floor_real_digits": 12,
            "reference_floor_imag_digits": 12,
        },
    ]
    reference_floor = {
        "schema_version": 1,
        "cross_check_passed": True,
        "comparison_verdict": "matched-to-reference-floor",
        "compared_coefficient_count": 14,
        "passed_coefficient_count": 10,
        "reference_floor_matched_coefficient_count": 4,
        "minimum_digit_agreement": 11,
        "row56_reference_floor_targets": targets,
        "m7_closure_claimed": False,
        "release_readiness_claimed": False,
    }
    precision = {
        "schema_version": 1,
        "cross_check_passed": True,
        "target_count": 4,
        "component_count": 8,
        "minimum_reference_floor_digits": 11,
        "minimum_standard_fraction_digits": 80,
        "minimum_uplifted_fraction_digits": 160,
        "m7_closure_claimed": False,
        "release_readiness_claimed": False,
    }
    publication = {
        "schema_version": 1,
        "publication_gate_reviewed": True,
        "precision_evidence_sidecar_reviewed": True,
        "precision_evidence_source_cpp_result_bound": True,
        "precision_evidence_target_count": 4,
        "precision_evidence_not_amflow_reference_backed": True,
        "amflow_cross_comparator_publication_gate_required": True,
        "amflow_cross_comparator_publication_gate_passed": False,
        "amflow_cross_comparator_blocked_publication_variant_count": 5,
        "variant_count": 5,
        "amflow_cross_comparator_minimum_digit_agreement_required": 50,
        "amflow_cross_comparator_minimum_digit_agreement_observed": 2,
        "m6_qualifier_hook_prepositioned": True,
        "m6_qualifier_hook_currently_promoted": False,
        "m7_parity_single_row_hook_prepositioned": True,
        "withheld_claims": list(WITHHELD_CLAIMS[2:]),
    }
    fingerprints = summarize_pinned_fingerprints()
    return reference_floor, precision, publication, fingerprints


def run_self_check() -> dict[str, Any]:
    reference_floor, precision, publication, fingerprints = synthetic_payloads()
    valid = summarize_payloads(
        reference_floor=reference_floor,
        precision=precision,
        publication=publication,
        fingerprints=fingerprints,
    )

    fake_50_digit = copy.deepcopy(reference_floor)
    fake_50_digit["comparison_verdict"] = "matched-to-50-digit"

    lost_uplift = copy.deepcopy(precision)
    lost_uplift["minimum_uplifted_fraction_digits"] = 80

    precision_target_count_drift = copy.deepcopy(precision)
    precision_target_count_drift["target_count"] = 3

    promoted_gate = copy.deepcopy(publication)
    promoted_gate["amflow_cross_comparator_publication_gate_passed"] = True

    amflow_reference_backed_precision = copy.deepcopy(publication)
    amflow_reference_backed_precision["precision_evidence_not_amflow_reference_backed"] = False

    promoted_m6_hook = copy.deepcopy(publication)
    promoted_m6_hook["m6_qualifier_hook_currently_promoted"] = True

    missing_withheld_claim = copy.deepcopy(publication)
    missing_withheld_claim["withheld_claims"] = [
        claim
        for claim in missing_withheld_claim["withheld_claims"]
        if claim != "This summary does not claim Milestone M7 closure."
    ]

    fingerprint_drift = copy.deepcopy(fingerprints)
    fingerprint_drift["runtime_checked"] = True
    fingerprint_drift["pins_match"] = False

    fingerprint_count_drift = copy.deepcopy(fingerprints)
    fingerprint_count_drift["entry_count"] += 1

    checks = {
        "synthetic_summary_passes": valid["status"] == "blocked-reference-floor-limited",
        "rejects_fake_50_digit_row56_status": rejected(
            reference_floor=fake_50_digit,
            precision=precision,
            publication=publication,
            fingerprints=fingerprints,
            expected_error="matched-to-reference-floor",
        ),
        "rejects_precision_uplift_regression": rejected(
            reference_floor=reference_floor,
            precision=lost_uplift,
            publication=publication,
            fingerprints=fingerprints,
            expected_error="160 fractional digits",
        ),
        "rejects_precision_target_count_drift": rejected(
            reference_floor=reference_floor,
            precision=precision_target_count_drift,
            publication=publication,
            fingerprints=fingerprints,
            expected_error="target count must match",
        ),
        "rejects_publication_gate_promotion": rejected(
            reference_floor=reference_floor,
            precision=precision,
            publication=promoted_gate,
            fingerprints=fingerprints,
            expected_error="publication gate must stay blocked",
        ),
        "rejects_amflow_reference_backed_precision_mislabel": rejected(
            reference_floor=reference_floor,
            precision=precision,
            publication=amflow_reference_backed_precision,
            fingerprints=fingerprints,
            expected_error="explicitly non-AMFlow-reference-backed",
        ),
        "rejects_m6_hook_promotion": rejected(
            reference_floor=reference_floor,
            precision=precision,
            publication=promoted_m6_hook,
            fingerprints=fingerprints,
            expected_error="M6 qualifier hook must not be promoted",
        ),
        "rejects_missing_withheld_m7_claim": rejected(
            reference_floor=reference_floor,
            precision=precision,
            publication=missing_withheld_claim,
            fingerprints=fingerprints,
            expected_error="withheld_claims missing",
        ),
        "rejects_runtime_fingerprint_drift": rejected(
            reference_floor=reference_floor,
            precision=precision,
            publication=publication,
            fingerprints=fingerprint_drift,
            expected_error="fingerprints must match pins",
        ),
        "rejects_fingerprint_count_drift": rejected(
            reference_floor=reference_floor,
            precision=precision,
            publication=publication,
            fingerprints=fingerprint_count_drift,
            expected_error="fingerprint entry_count must match entries",
        ),
    }
    expect(all(checks.values()), "b61n parity status summary self-check failed")
    return {
        "schema_version": 1,
        "summary_id": "b61n-post-m7-parity-status-v1",
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
    parser.add_argument(
        "--precision-evidence-path",
        type=Path,
        default=root / DEFAULT_PRECISION_EVIDENCE,
        help="b61n precision uplift evidence sidecar path",
    )
    parser.add_argument(
        "--publication-qualifier-sidecar-path",
        type=Path,
        default=default_publication_qualifier_sidecar_path(),
        help="b61n publication qualifier sidecar path",
    )
    parser.add_argument(
        "--test-binary",
        help="Path to the built singular-runtime-lane-tests executable.",
    )
    parser.add_argument(
        "--include-runtime-audit",
        action="store_true",
        help="Regenerate b61n publication audit fingerprints and require pins to match.",
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
            reference_floor = verify_reference_floor_paths(
                resolve_repo_path(root, args.cpp_result_path),
                resolve_repo_path(root, args.amflow_golden_path),
                resolve_repo_path(root, args.retained_comparison_path),
            )
            precision = verify_precision_paths(resolve_repo_path(root, args.precision_evidence_path))
            publication = audit_sidecar(resolve_repo_path(root, args.publication_qualifier_sidecar_path))
            if args.include_runtime_audit:
                fingerprints = summarize_runtime_fingerprints(
                    resolve_test_binary(args.test_binary, root),
                    root,
                )
            else:
                fingerprints = summarize_pinned_fingerprints()
            summary = summarize_payloads(
                reference_floor=reference_floor,
                precision=precision,
                publication=publication,
                fingerprints=fingerprints,
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
