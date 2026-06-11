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
    EXPECTED_TARGETS as EXPECTED_REFERENCE_FLOOR_TARGETS,
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
EVIDENCE_SOURCE_FIELDS: tuple[str, ...] = (
    "reference_floor_cpp_result",
    "reference_floor_amflow_golden",
    "reference_floor_retained_comparison",
    "precision_evidence",
    "precision_source_cpp_result",
    "precision_source_diagnostic",
    "publication_qualifier_sidecar",
    "publication_precision_evidence",
    "publication_precision_source_cpp_result",
)


class StatusSummaryError(RuntimeError):
    """Raised when b61n parity status inputs cannot be summarized."""


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise StatusSummaryError(message)


def resolve_repo_path(root: Path, path: Path) -> Path:
    return path if path.is_absolute() else root / path


def repo_relative_path(root: Path, path: Path | str) -> str:
    resolved = resolve_repo_path(root, Path(path)).resolve()
    try:
        return resolved.relative_to(root.resolve()).as_posix()
    except ValueError:
        return str(resolved)


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


def require_string(payload: dict[str, Any], field: str) -> str:
    value = payload.get(field)
    expect(isinstance(value, str) and value, f"{field} must be a non-empty string")
    return value


def require_string_list(payload: dict[str, Any], field: str) -> list[str]:
    value = payload.get(field)
    expect(isinstance(value, list), f"{field} must be a list")
    strings: list[str] = []
    for index, item in enumerate(value):
        expect(isinstance(item, str) and item, f"{field}[{index}] must be a non-empty string")
        strings.append(item)
    return strings


def build_evidence_sources(
    *,
    root: Path,
    reference_floor: dict[str, Any],
    precision: dict[str, Any],
    publication: dict[str, Any],
) -> dict[str, Any]:
    return {
        "reference_floor_cpp_result": repo_relative_path(
            root,
            require_string(reference_floor, "cpp_result"),
        ),
        "reference_floor_amflow_golden": repo_relative_path(
            root,
            require_string(reference_floor, "amflow_golden"),
        ),
        "reference_floor_retained_comparison": repo_relative_path(
            root,
            require_string(reference_floor, "retained_comparison"),
        ),
        "precision_evidence": repo_relative_path(
            root,
            require_string(precision, "precision_evidence"),
        ),
        "precision_source_cpp_result": repo_relative_path(
            root,
            require_string(precision, "source_cpp_result"),
        ),
        "precision_source_diagnostic": repo_relative_path(
            root,
            require_string(precision, "source_diagnostic"),
        ),
        "publication_qualifier_sidecar": repo_relative_path(
            root,
            require_string(publication, "sidecar_path"),
        ),
        "publication_precision_evidence": repo_relative_path(
            root,
            require_string(publication, "precision_evidence_sidecar_path"),
        ),
        "publication_precision_source_cpp_result": repo_relative_path(
            root,
            require_string(publication, "precision_evidence_source_cpp_result"),
        ),
    }


def default_evidence_sources(root: Path) -> dict[str, Any]:
    return {
        "reference_floor_cpp_result": repo_relative_path(root, DEFAULT_CPP_RESULT),
        "reference_floor_amflow_golden": repo_relative_path(root, DEFAULT_AMFLOW_GOLDEN),
        "reference_floor_retained_comparison": repo_relative_path(root, DEFAULT_RETAINED_COMPARISON),
        "precision_evidence": repo_relative_path(root, DEFAULT_PRECISION_EVIDENCE),
        "precision_source_cpp_result": repo_relative_path(root, DEFAULT_CPP_RESULT),
        "precision_source_diagnostic": repo_relative_path(
            root,
            "tools/reference-harness/specs/m7/lane2/b61n-row56-specific-target-diagnostic.json",
        ),
        "publication_qualifier_sidecar": repo_relative_path(
            root,
            default_publication_qualifier_sidecar_path(),
        ),
        "publication_precision_evidence": repo_relative_path(root, DEFAULT_PRECISION_EVIDENCE),
        "publication_precision_source_cpp_result": repo_relative_path(root, DEFAULT_CPP_RESULT),
    }


def validate_evidence_sources(sources: dict[str, Any]) -> dict[str, str]:
    normalized: dict[str, str] = {}
    for field in EVIDENCE_SOURCE_FIELDS:
        normalized[field] = require_string(sources, field)
    expect(
        normalized["reference_floor_cpp_result"] == normalized["precision_source_cpp_result"],
        "reference-floor C++ result and precision source C++ result must match",
    )
    expect(
        normalized["precision_source_cpp_result"]
        == normalized["publication_precision_source_cpp_result"],
        "precision source C++ result and publication precision source C++ result must match",
    )
    expect(
        normalized["precision_evidence"] == normalized["publication_precision_evidence"],
        "precision evidence and publication precision evidence must match",
    )
    return normalized


def require_reference_floor_targets(
    payload: dict[str, Any],
    *,
    expected_count: int,
    expected_digit_floor: int,
) -> list[dict[str, Any]]:
    value = payload.get("row56_reference_floor_targets")
    expect(isinstance(value, list), "row56_reference_floor_targets must be a list")
    expect(
        len(value) == expected_count,
        "row56 reference-floor target list length must match reference-floor match count",
    )

    targets: list[dict[str, Any]] = []
    target_keys: set[tuple[str, int]] = set()
    target_ids: set[str] = set()
    digit_floor: int | None = None
    for index, item in enumerate(value):
        label = f"row56_reference_floor_targets[{index}]"
        expect(isinstance(item, dict), f"{label} must be an object")

        integral = item.get("integral")
        expect(
            isinstance(integral, str) and integral,
            f"{label}.integral must be a non-empty string",
        )
        order = item.get("order")
        expect(type(order) is int, f"{label}.order must be an int")
        target_key = (integral, order)
        expect(
            target_key not in target_keys,
            f"{label} duplicates a row/order reference-floor target",
        )
        expected_target = EXPECTED_REFERENCE_FLOOR_TARGETS.get(target_key)
        expect(
            expected_target is not None,
            f"{label} is not a reviewed b61n row 5/6 reference-floor target",
        )
        target_keys.add(target_key)

        reference_floor_id = item.get("reference_floor_id")
        expect(
            isinstance(reference_floor_id, str) and reference_floor_id,
            f"{label}.reference_floor_id must be a non-empty string",
        )
        expect(
            reference_floor_id == expected_target["reference_floor_id"],
            f"{label}.reference_floor_id must be {expected_target['reference_floor_id']!r}",
        )
        expect(
            reference_floor_id not in target_ids,
            f"{label}.reference_floor_id duplicates another reference-floor target",
        )
        target_ids.add(reference_floor_id)

        for field in ("reference_floor_real_digits", "reference_floor_imag_digits"):
            digits = item.get(field)
            expect(
                type(digits) is int and digits >= 0,
                f"{label}.{field} must be a nonnegative int",
            )
            expect(
                digits == expected_target[field],
                f"{label}.{field} must be {expected_target[field]}",
            )
            digit_floor = digits if digit_floor is None else min(digit_floor, digits)

        targets.append(item)

    expect(
        target_keys == set(EXPECTED_REFERENCE_FLOOR_TARGETS),
        "row56 reference-floor targets must match the reviewed b61n row 5/6 target set",
    )
    expect(
        digit_floor == expected_digit_floor,
        "row56 reference-floor target digit floor must match minimum digit agreement",
    )
    return targets


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
    evidence_sources: dict[str, Any],
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
    reference_floor_targets = require_reference_floor_targets(
        reference_floor,
        expected_count=reference_floor_count,
        expected_digit_floor=reference_floor["minimum_digit_agreement"],
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
    expect(
        precision.get("amflow_reference_values_used_for_digit_count") is False,
        "precision uplift must not use AMFlow reference values for digit counting",
    )
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
        "evidence_sources": validate_evidence_sources(evidence_sources),
        "reference_floor": {
            "comparison_verdict": reference_floor["comparison_verdict"],
            "compared_coefficient_count": compared_count,
            "matched_to_tolerance_count": tolerance_pass_count,
            "matched_to_reference_floor_count": reference_floor_count,
            "minimum_digit_agreement": reference_floor["minimum_digit_agreement"],
            "targets": reference_floor_targets,
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
            "amflow_reference_values_used_for_digit_count": require_bool(
                precision,
                "amflow_reference_values_used_for_digit_count",
            ),
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
    sources = summary["evidence_sources"]
    reference = summary["reference_floor"]
    precision = summary["precision_uplift"]
    publication = summary["publication_gate"]
    fingerprints = summary["audit_fingerprints"]
    lines = [
        "b61n parity status summary",
        f"status: {summary['status']}",
        (
            "evidence_sources: "
            f"cpp_result={sources['reference_floor_cpp_result']} "
            f"amflow_golden={sources['reference_floor_amflow_golden']} "
            f"retained_comparison={sources['reference_floor_retained_comparison']} "
            f"precision={sources['precision_evidence']} "
            f"publication={sources['publication_qualifier_sidecar']}"
        ),
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
    evidence_sources: dict[str, Any],
    expected_error: str,
) -> bool:
    try:
        summarize_payloads(
            reference_floor=reference_floor,
            precision=precision,
            publication=publication,
            fingerprints=fingerprints,
            evidence_sources=evidence_sources,
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
        "amflow_reference_values_used_for_digit_count": False,
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
    evidence_sources = default_evidence_sources(repo_root())
    valid = summarize_payloads(
        reference_floor=reference_floor,
        precision=precision,
        publication=publication,
        fingerprints=fingerprints,
        evidence_sources=evidence_sources,
    )

    fake_50_digit = copy.deepcopy(reference_floor)
    fake_50_digit["comparison_verdict"] = "matched-to-50-digit"

    lost_uplift = copy.deepcopy(precision)
    lost_uplift["minimum_uplifted_fraction_digits"] = 80

    amflow_reference_backed_digit_count = copy.deepcopy(precision)
    amflow_reference_backed_digit_count["amflow_reference_values_used_for_digit_count"] = True

    precision_target_count_drift = copy.deepcopy(precision)
    precision_target_count_drift["target_count"] = 3

    reference_floor_target_count_drift = copy.deepcopy(reference_floor)
    reference_floor_target_count_drift["row56_reference_floor_targets"] = (
        reference_floor_target_count_drift["row56_reference_floor_targets"][:-1]
    )

    duplicate_reference_floor_target = copy.deepcopy(reference_floor)
    duplicate_reference_floor_target["row56_reference_floor_targets"][1]["integral"] = (
        duplicate_reference_floor_target["row56_reference_floor_targets"][0]["integral"]
    )
    duplicate_reference_floor_target["row56_reference_floor_targets"][1]["order"] = (
        duplicate_reference_floor_target["row56_reference_floor_targets"][0]["order"]
    )

    unknown_reference_floor_target = copy.deepcopy(reference_floor)
    unknown_reference_floor_target["row56_reference_floor_targets"][0]["integral"] = (
        "box[9,9,9,9]"
    )

    stale_reference_floor_id = copy.deepcopy(reference_floor)
    stale_reference_floor_id["row56_reference_floor_targets"][0]["reference_floor_id"] = (
        "synthetic-stale-b61n-reference-floor"
    )

    target_digit_floor_drift = copy.deepcopy(reference_floor)
    target_digit_floor_drift["row56_reference_floor_targets"][1][
        "reference_floor_real_digits"
    ] = 45
    target_digit_floor_drift["row56_reference_floor_targets"][1][
        "reference_floor_imag_digits"
    ] = 45

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

    precision_source_drift = copy.deepcopy(evidence_sources)
    precision_source_drift["precision_source_cpp_result"] = (
        "tools/reference-harness/specs/m7/lane2/synthetic-b61n-source.cpp-result.json"
    )

    publication_precision_drift = copy.deepcopy(evidence_sources)
    publication_precision_drift["publication_precision_evidence"] = (
        "tools/reference-harness/specs/m7/lane2/synthetic-b61n-precision-evidence.json"
    )

    checks = {
        "synthetic_summary_passes": valid["status"] == "blocked-reference-floor-limited",
        "rejects_fake_50_digit_row56_status": rejected(
            reference_floor=fake_50_digit,
            precision=precision,
            publication=publication,
            fingerprints=fingerprints,
            evidence_sources=evidence_sources,
            expected_error="matched-to-reference-floor",
        ),
        "rejects_precision_uplift_regression": rejected(
            reference_floor=reference_floor,
            precision=lost_uplift,
            publication=publication,
            fingerprints=fingerprints,
            evidence_sources=evidence_sources,
            expected_error="160 fractional digits",
        ),
        "rejects_amflow_reference_backed_digit_count": rejected(
            reference_floor=reference_floor,
            precision=amflow_reference_backed_digit_count,
            publication=publication,
            fingerprints=fingerprints,
            evidence_sources=evidence_sources,
            expected_error="must not use AMFlow reference values",
        ),
        "rejects_precision_target_count_drift": rejected(
            reference_floor=reference_floor,
            precision=precision_target_count_drift,
            publication=publication,
            fingerprints=fingerprints,
            evidence_sources=evidence_sources,
            expected_error="target count must match",
        ),
        "rejects_reference_floor_target_count_drift": rejected(
            reference_floor=reference_floor_target_count_drift,
            precision=precision,
            publication=publication,
            fingerprints=fingerprints,
            evidence_sources=evidence_sources,
            expected_error="target list length must match",
        ),
        "rejects_duplicate_reference_floor_target": rejected(
            reference_floor=duplicate_reference_floor_target,
            precision=precision,
            publication=publication,
            fingerprints=fingerprints,
            evidence_sources=evidence_sources,
            expected_error="duplicates a row/order reference-floor target",
        ),
        "rejects_unknown_reference_floor_target": rejected(
            reference_floor=unknown_reference_floor_target,
            precision=precision,
            publication=publication,
            fingerprints=fingerprints,
            evidence_sources=evidence_sources,
            expected_error="not a reviewed b61n row 5/6 reference-floor target",
        ),
        "rejects_stale_reference_floor_id": rejected(
            reference_floor=stale_reference_floor_id,
            precision=precision,
            publication=publication,
            fingerprints=fingerprints,
            evidence_sources=evidence_sources,
            expected_error="reference_floor_id must be",
        ),
        "rejects_reference_floor_target_digit_floor_drift": rejected(
            reference_floor=target_digit_floor_drift,
            precision=precision,
            publication=publication,
            fingerprints=fingerprints,
            evidence_sources=evidence_sources,
            expected_error="reference_floor_real_digits must be",
        ),
        "rejects_publication_gate_promotion": rejected(
            reference_floor=reference_floor,
            precision=precision,
            publication=promoted_gate,
            fingerprints=fingerprints,
            evidence_sources=evidence_sources,
            expected_error="publication gate must stay blocked",
        ),
        "rejects_amflow_reference_backed_precision_mislabel": rejected(
            reference_floor=reference_floor,
            precision=precision,
            publication=amflow_reference_backed_precision,
            fingerprints=fingerprints,
            evidence_sources=evidence_sources,
            expected_error="explicitly non-AMFlow-reference-backed",
        ),
        "rejects_m6_hook_promotion": rejected(
            reference_floor=reference_floor,
            precision=precision,
            publication=promoted_m6_hook,
            fingerprints=fingerprints,
            evidence_sources=evidence_sources,
            expected_error="M6 qualifier hook must not be promoted",
        ),
        "rejects_missing_withheld_m7_claim": rejected(
            reference_floor=reference_floor,
            precision=precision,
            publication=missing_withheld_claim,
            fingerprints=fingerprints,
            evidence_sources=evidence_sources,
            expected_error="withheld_claims missing",
        ),
        "rejects_runtime_fingerprint_drift": rejected(
            reference_floor=reference_floor,
            precision=precision,
            publication=publication,
            fingerprints=fingerprint_drift,
            evidence_sources=evidence_sources,
            expected_error="fingerprints must match pins",
        ),
        "rejects_fingerprint_count_drift": rejected(
            reference_floor=reference_floor,
            precision=precision,
            publication=publication,
            fingerprints=fingerprint_count_drift,
            evidence_sources=evidence_sources,
            expected_error="fingerprint entry_count must match entries",
        ),
        "rejects_precision_source_path_drift": rejected(
            reference_floor=reference_floor,
            precision=precision,
            publication=publication,
            fingerprints=fingerprints,
            evidence_sources=precision_source_drift,
            expected_error="precision source C++ result",
        ),
        "rejects_publication_precision_path_drift": rejected(
            reference_floor=reference_floor,
            precision=precision,
            publication=publication,
            fingerprints=fingerprints,
            evidence_sources=publication_precision_drift,
            expected_error="publication precision evidence",
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
            evidence_sources = build_evidence_sources(
                root=root,
                reference_floor=reference_floor,
                precision=precision,
                publication=publication,
            )
            summary = summarize_payloads(
                reference_floor=reference_floor,
                precision=precision,
                publication=publication,
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
