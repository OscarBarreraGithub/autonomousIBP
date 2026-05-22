#!/usr/bin/env python3
"""Audit the b61n publication qualifier hook sidecar."""

from __future__ import annotations

import argparse
import json
import re
import tempfile
from decimal import Decimal, InvalidOperation
from pathlib import Path
from typing import Any

from freeze_phase0_goldens import load_json


REVIEWED_VARIANT_ENDPOINTS: dict[str, str] = {
    "lane142-primitive-bubble-box1010": "box[1,0,1,0]",
    "lane5-next7-primitive-bubble-box0011": "box[0,0,1,1]",
}

REQUIRED_NUMERIC_SYMBOLS: tuple[str, ...] = ("s", "t", "p3sq", "p4sq", "m3sq")
RETAINED_NUMERIC_CASE_ID = "retained-complex-kinematics"
RETAINED_NUMERIC_EVIDENCE_SCOPE = "source-backed-retained-contour-evidence"
SMOKE_NUMERIC_EVIDENCE_SCOPE = "publication-side-shape-only-regression"
REVIEWED_SOURCE_CONTOUR_FINGERPRINT = "fnv1a64:a8dd3d0427fbf52b"
REVIEWED_SOURCE_CONTOUR_WAYPOINT_SUMMARY = (
    "-988.886458875053403341688*I -> -247.221614718763350835422*I -> "
    "-1*I -> -0.0625*I -> 0"
)
RETAINED_SOURCE_NUMERIC_EVIDENCE = (
    "tools/reference-harness/specs/phase0/complex_kinematics.amflow-state.json"
)
REQUIRED_SOURCE_EVIDENCE: tuple[str, ...] = (
    "tools/reference-harness/specs/m6/lane142/b61n-selected5-real-coefficients-evidence.json",
    "tools/reference-harness/specs/m6/lane142/complex_kinematics.selected5.compare30.json",
    "tools/reference-harness/specs/m6/lane142/complex_kinematics.selected5.stripped-result.json",
    RETAINED_SOURCE_NUMERIC_EVIDENCE,
)
WITHHELD_CLAIMS: tuple[str, ...] = (
    "This summary does not claim Milestone M6 closure.",
    "This summary does not claim Milestone M7 closure.",
    "This summary does not claim release readiness.",
    "This summary does not claim full eta=0 contour execution.",
    "This summary does not widen runtime or public behavior.",
)


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def write_json(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def resolve_sidecar_path(path_text: str, sidecar_path: Path) -> Path:
    path = Path(path_text)
    if path.is_absolute():
        return path
    repo_path = repo_root() / path
    if repo_path.exists():
        return repo_path
    return sidecar_path.parent / path


def parse_decimal(raw: Any, label: str) -> Decimal:
    if isinstance(raw, int):
        value = Decimal(raw)
    elif isinstance(raw, str):
        try:
            value = Decimal(raw.strip())
        except InvalidOperation as error:
            raise ValueError(f"{label} must be a finite decimal") from error
    else:
        raise TypeError(f"{label} must be a string or int, got {type(raw).__name__}")
    expect(value.is_finite(), f"{label} must be finite")
    return value


def parse_rational_decimal(raw: Any, label: str) -> Decimal:
    if isinstance(raw, int):
        return Decimal(raw)
    if not isinstance(raw, str):
        raise TypeError(f"{label} must be a numeric string or int")
    value = raw.strip()
    expect(value, f"{label} must not be empty")
    if "/" in value:
        numerator, separator, denominator = value.partition("/")
        expect(separator == "/" and denominator, f"{label} has malformed rational value")
        result = parse_decimal(numerator, f"{label} numerator") / parse_decimal(
            denominator, f"{label} denominator"
        )
    else:
        result = parse_decimal(value, label)
    expect(result.is_finite(), f"{label} must be finite")
    return result


def split_complex_text(value: str, label: str) -> tuple[str, str]:
    text = value.replace(" ", "").replace("*", "")
    expect(text.endswith("I"), f"{label} must include an explicit imaginary part")
    without_i = text[:-1]
    split_at = -1
    for index in range(1, len(without_i)):
        if without_i[index] in "+-":
            split_at = index
    expect(split_at > 0, f"{label} must include real and imaginary parts")
    real = without_i[:split_at]
    imag = without_i[split_at:]
    if imag in {"+", ""}:
        imag = "+1"
    elif imag == "-":
        imag = "-1"
    return real, imag


def parse_complex_numeric(raw: Any, label: str) -> tuple[Decimal, Decimal]:
    if not isinstance(raw, str):
        raise TypeError(f"{label} must be a complex numeric string")
    real_text, imag_text = split_complex_text(raw, label)
    real = parse_rational_decimal(real_text, f"{label} real")
    imag = parse_rational_decimal(imag_text, f"{label} imag")
    expect(imag != 0, f"{label} must preserve a nonzero imaginary part")
    return real, imag


def normalized_string(raw: Any, label: str) -> str:
    if not isinstance(raw, str):
        raise TypeError(f"{label} must be a string")
    value = raw.strip()
    expect(value, f"{label} must not be empty")
    return value


def normalize_bool(raw: Any, label: str) -> bool:
    if not isinstance(raw, bool):
        raise TypeError(f"{label} must be a bool")
    return raw


def normalize_string_list(raw: Any, label: str) -> list[str]:
    if not isinstance(raw, list):
        raise TypeError(f"{label} must be a list")
    values: list[str] = []
    for item in raw:
        values.append(normalized_string(item, f"{label} entry"))
    expect(len(set(values)) == len(values), f"{label} must not contain duplicates")
    return values


def numeric_case_fingerprint(numeric_substitutions: dict[str, Any]) -> str:
    return json.dumps(numeric_substitutions, sort_keys=True, separators=(",", ":"))


def extract_mathematica_numeric_substitutions(raw_text: str) -> dict[str, str]:
    match = re.search(r'"Numeric"\s*->\s*\{([^}]*)\}', raw_text)
    expect(match is not None, "source AMFlow state is missing Numeric substitutions")
    values: dict[str, str] = {}
    for raw_entry in match.group(1).split(","):
        symbol, separator, value = raw_entry.partition("->")
        expect(separator == "->", "source AMFlow Numeric substitution is malformed")
        values[symbol.strip()] = value.strip()
    for symbol in REQUIRED_NUMERIC_SYMBOLS:
        expect(symbol in values, f"source AMFlow Numeric list is missing {symbol}")
    return {symbol: values[symbol] for symbol in REQUIRED_NUMERIC_SYMBOLS}


def normalize_numeric_substitutions(
    numeric_substitutions: dict[str, Any],
    label: str,
) -> dict[str, Any]:
    values: dict[str, Any] = {}
    for symbol in ("s", "t", "p3sq", "p4sq"):
        values[symbol] = parse_rational_decimal(numeric_substitutions[symbol], f"{label} {symbol}")
    values["m3sq"] = parse_complex_numeric(numeric_substitutions["m3sq"], f"{label} m3sq")
    return values


def parse_summary_contour_waypoints(raw: str, label: str) -> list[tuple[Decimal, Decimal]]:
    waypoints: list[tuple[Decimal, Decimal]] = []
    for index, part in enumerate(raw.split("->")):
        value = part.strip()
        if value == "0":
            waypoints.append((Decimal(0), Decimal(0)))
            continue
        expect(value.endswith("*I"), f"{label} waypoint {index} must be an eta-axis point")
        waypoints.append((Decimal(0), parse_decimal(value[:-2], f"{label} waypoint {index} imag")))
    expect(waypoints, f"{label} must publish at least one waypoint")
    return waypoints


def parse_source_contour_summary(summary: str) -> dict[str, Any]:
    match = re.search(
        r"contour_waypoints=\[([^\]]+)\]; contour_fingerprint=([^;]+);"
        r".*?minimum_pole_distance_to_contour=([^;]+);",
        summary,
    )
    expect(match is not None, "b61n stripped result summary is missing contour evidence")
    waypoint_summary = match.group(1).strip()
    contour_fingerprint = match.group(2).strip()
    minimum_pole_distance = parse_decimal(
        match.group(3).strip(),
        "b61n source minimum_pole_distance_to_contour",
    )
    expect(
        waypoint_summary == REVIEWED_SOURCE_CONTOUR_WAYPOINT_SUMMARY,
        "b61n source contour waypoint summary drifted",
    )
    expect(
        contour_fingerprint == REVIEWED_SOURCE_CONTOUR_FINGERPRINT,
        "b61n source contour fingerprint drifted",
    )
    expect(
        minimum_pole_distance > 0,
        "b61n source contour must retain a positive pole distance",
    )
    return {
        "contour_fingerprint": contour_fingerprint,
        "contour_waypoints": parse_summary_contour_waypoints(
            waypoint_summary,
            "b61n source contour_waypoints",
        ),
        "minimum_pole_distance_to_contour": minimum_pole_distance,
    }


def validate_publication_contour(case: dict[str, Any], label: str) -> list[tuple[Decimal, Decimal]]:
    expect(
        normalized_string(case.get("direction"), f"{label} direction") == "NegIm",
        f"{label} direction must be NegIm",
    )
    expect(
        normalized_string(case.get("target_location"), f"{label} target_location") == "eta=0",
        f"{label} target_location must be eta=0",
    )
    raw_waypoints = case.get("contour_waypoints")
    if not isinstance(raw_waypoints, list):
        raise TypeError(f"{label} contour_waypoints must be a list")
    expect(len(raw_waypoints) >= 2, f"{label} must publish at least two contour waypoints")

    previous_imag: Decimal | None = None
    waypoints: list[tuple[Decimal, Decimal]] = []
    for index, raw_waypoint in enumerate(raw_waypoints):
        if not isinstance(raw_waypoint, dict):
            raise TypeError(f"{label} contour waypoint entries must be objects")
        real = parse_decimal(raw_waypoint.get("real"), f"{label} waypoint {index} real")
        imag = parse_decimal(raw_waypoint.get("imag"), f"{label} waypoint {index} imag")
        expect(real == 0, f"{label} waypoint {index} must stay on the eta imaginary axis")
        final = index + 1 == len(raw_waypoints)
        if final:
            expect(imag == 0, f"{label} final waypoint must end exactly at eta=0")
        else:
            expect(imag < 0, f"{label} non-final waypoint must stay on the NegIm branch")
        if previous_imag is not None:
            expect(
                imag > previous_imag,
                f"{label} contour waypoint imaginary parts must increase monotonically",
            )
        previous_imag = imag
        waypoints.append((real, imag))
    return waypoints


def validate_numeric_case(case: dict[str, Any], label: str) -> dict[str, Any]:
    if not isinstance(case, dict):
        raise TypeError(f"{label} must be an object")
    case_id = normalized_string(case.get("id"), f"{label} id")
    contour_fingerprint = normalized_string(
        case.get("contour_fingerprint"),
        f"{label} contour_fingerprint",
    )
    evidence_scope = normalized_string(case.get("evidence_scope"), f"{label} evidence_scope")
    numeric_substitutions = case.get("numeric_substitutions")
    if not isinstance(numeric_substitutions, dict):
        raise TypeError(f"{label} numeric_substitutions must be an object")
    for symbol in REQUIRED_NUMERIC_SYMBOLS:
        expect(symbol in numeric_substitutions, f"{label} Numeric list is missing {symbol}")
    normalized_numeric = normalize_numeric_substitutions(numeric_substitutions, label)
    contour_waypoints = validate_publication_contour(case, label)
    return {
        "case_id": case_id,
        "contour_fingerprint": contour_fingerprint,
        "contour_waypoints": contour_waypoints,
        "evidence_scope": evidence_scope,
        "numeric_fingerprint": numeric_case_fingerprint(numeric_substitutions),
        "normalized_numeric_substitutions": normalized_numeric,
    }


def validate_variant(
    variant: dict[str, Any],
    index: int,
    source_evidence: dict[str, Any],
) -> tuple[str, set[str]]:
    label = f"publication_variants[{index}]"
    variant_id = normalized_string(variant.get("id"), f"{label} id")
    expected_endpoint = REVIEWED_VARIANT_ENDPOINTS.get(variant_id)
    expect(expected_endpoint is not None, f"{label} is not a reviewed b61n variant")
    endpoint_integral = normalized_string(
        variant.get("endpoint_integral_id"), f"{label} endpoint_integral_id"
    )
    expect(
        endpoint_integral == expected_endpoint,
        f"{label} endpoint integral does not match its reviewed variant id",
    )
    expect(
        normalized_string(variant.get("matrix_fingerprint"), f"{label} matrix_fingerprint")
        == "lane142-b61n-selected5-primitive-bubble-v1",
        f"{label} must preserve the reviewed lane142 matrix fingerprint",
    )
    expect(
        normalized_string(
            variant.get("endpoint_local_model_kind"),
            f"{label} endpoint_local_model_kind",
        )
        == "b61n-primitive-bubble-regular-taylor-r0",
        f"{label} must use the reviewed primitive-bubble local model",
    )
    expect(
        normalized_string(variant.get("transport_scope"), f"{label} transport_scope")
        == "eta-zero-selected-endpoint-coefficients",
        f"{label} must stay scoped to selected endpoint coefficients",
    )
    expect(
        normalize_bool(variant.get("coefficient_publication"), f"{label} coefficient_publication"),
        f"{label} must publish the selected endpoint coefficient",
    )
    expect(
        normalize_bool(
            variant.get("endpoint_extraction_applied"),
            f"{label} endpoint_extraction_applied",
        ),
        f"{label} must mark endpoint extraction",
    )
    expect(
        not normalize_bool(
            variant.get("full_eta_zero_contour_applied"),
            f"{label} full_eta_zero_contour_applied",
        ),
        f"{label} must not claim full eta=0 contour execution",
    )
    expect(
        not normalize_bool(
            variant.get("final_solution_samples_used_as_input"),
            f"{label} final_solution_samples_used_as_input",
        ),
        f"{label} must not use final solution samples as input",
    )

    raw_numeric_cases = variant.get("numeric_substitution_cases")
    if not isinstance(raw_numeric_cases, list):
        raise TypeError(f"{label} numeric_substitution_cases must be a list")
    expect(
        len(raw_numeric_cases) >= 2,
        f"{label} must exercise the NegIm gate at multiple Numeric substitutions",
    )
    case_summaries = [
        validate_numeric_case(case, f"{label} numeric_substitution_cases[{case_index}]")
        for case_index, case in enumerate(raw_numeric_cases)
    ]
    numeric_fingerprints = {case["numeric_fingerprint"] for case in case_summaries}
    expect(
        len(numeric_fingerprints) >= 2,
        f"{label} Numeric substitution cases must be distinct",
    )
    retained_cases = [
        case for case in case_summaries if case["case_id"] == RETAINED_NUMERIC_CASE_ID
    ]
    expect(
        len(retained_cases) == 1,
        f"{label} must include exactly one retained source-backed Numeric case",
    )
    retained = retained_cases[0]
    expect(
        retained["evidence_scope"] == RETAINED_NUMERIC_EVIDENCE_SCOPE,
        f"{label} retained Numeric case must be marked source-backed",
    )
    expect(
        retained["normalized_numeric_substitutions"]
        == source_evidence["retained_numeric_substitutions"],
        f"{label} retained Numeric case must match the retained source Numeric substitutions",
    )
    expect(
        retained["contour_fingerprint"] == source_evidence["contour_fingerprint"],
        f"{label} retained Numeric case must match the reviewed source contour fingerprint",
    )
    expect(
        retained["contour_waypoints"] == source_evidence["contour_waypoints"],
        f"{label} retained Numeric case must match the reviewed source contour waypoints",
    )
    for case in case_summaries:
        if case["case_id"] == RETAINED_NUMERIC_CASE_ID:
            continue
        expect(
            case["evidence_scope"] == SMOKE_NUMERIC_EVIDENCE_SCOPE,
            f"{label} non-retained Numeric cases must be marked shape-only smoke",
        )
        expect(
            str(case["contour_fingerprint"]).startswith("publication-smoke:"),
            f"{label} non-retained Numeric cases must be marked as publication smoke",
        )
    return endpoint_integral, numeric_fingerprints


def validate_m6_hook(hook: dict[str, Any]) -> dict[str, Any]:
    if not isinstance(hook, dict):
        raise TypeError("m6_qualifier_hook must be an object")
    phase0_id = normalized_string(hook.get("phase0_id"), "m6_qualifier_hook phase0_id")
    optional_packet = normalized_string(
        hook.get("optional_capture_packet"),
        "m6_qualifier_hook optional_capture_packet",
    )
    expect(phase0_id == "complex_kinematics", "M6 hook must target complex_kinematics")
    expect(optional_packet == "b61n-complex-eta-zero-single-row",
           "M6 hook must pre-position the b61n optional packet id")
    expect(
        normalize_bool(
            hook.get("requires_full_eta_zero_contour_applied"),
            "m6_qualifier_hook requires_full_eta_zero_contour_applied",
        ),
        "M6 hook must require full eta=0 contour application before promotion",
    )
    full_eta_zero_contour_applied_observed = normalize_bool(
        hook.get("full_eta_zero_contour_applied_observed"),
        "m6_qualifier_hook full_eta_zero_contour_applied_observed",
    )
    currently_promoted = normalize_bool(
        hook.get("currently_promoted"), "m6_qualifier_hook currently_promoted"
    )
    expect(
        not full_eta_zero_contour_applied_observed,
        "M6 hook must remain pre-positioned until retained runtime capture observes full eta=0",
    )
    expect(not currently_promoted, "M6 hook must not self-certify promotion")
    expect(
        hook.get("accepted_runtime_result") in (None, ""),
        "M6 hook must not attach accepted_runtime_result before retained runtime capture",
    )
    return {
        "full_eta_zero_contour_applied_observed": full_eta_zero_contour_applied_observed,
        "optional_capture_packet": optional_packet,
        "phase0_id": phase0_id,
    }


def validate_m7_hook(hook: dict[str, Any]) -> dict[str, Any]:
    if not isinstance(hook, dict):
        raise TypeError("m7_parity_signoff_hook must be an object")
    expect(
        normalized_string(
            hook.get("release_checklist_section"),
            "m7_parity_signoff_hook release_checklist_section",
        )
        == "parity-signoff",
        "M7 hook must attach to the parity-signoff section",
    )
    single_row_path = normalized_string(
        hook.get("single_row_path"),
        "m7_parity_signoff_hook single_row_path",
    )
    expect(
        single_row_path == "b61n-complex-contour-propagator-harness",
        "M7 hook must name the b61n single-row path",
    )
    expect(
        normalized_string(hook.get("current_state"), "m7_parity_signoff_hook current_state")
        == "blocked-on-m6",
        "M7 hook must remain blocked until M6 closes",
    )
    return {"single_row_path": single_row_path}


def validate_reviewed_endpoint_compare_evidence(
    *,
    evidence_compare: dict[str, Any],
    compare: dict[str, Any],
) -> None:
    per_integral_digits = evidence_compare.get("per_integral_minimum_digit_agreement", {})
    if not isinstance(per_integral_digits, dict):
        raise TypeError("b61n evidence per-integral digit summary must be an object")
    raw_integrals = compare.get("integrals")
    if not isinstance(raw_integrals, list):
        raise TypeError("b61n compare integrals must be a list")
    compare_by_integral: dict[str, dict[str, Any]] = {}
    for raw_integral in raw_integrals:
        if not isinstance(raw_integral, dict):
            raise TypeError("b61n compare integral entries must be objects")
        integral_id = normalized_string(raw_integral.get("integral"), "b61n compare integral")
        compare_by_integral[integral_id] = raw_integral

    for endpoint in REVIEWED_VARIANT_ENDPOINTS.values():
        minimum_digits = per_integral_digits.get(endpoint)
        expect(isinstance(minimum_digits, int), f"b61n evidence missing digits for {endpoint}")
        expect(
            minimum_digits >= 30,
            f"b61n evidence endpoint {endpoint} must retain compare30 digit coverage",
        )
        record = compare_by_integral.get(endpoint)
        expect(record is not None, f"b61n compare missing reviewed endpoint {endpoint}")
        expect(record.get("status") == "compared", f"b61n compare endpoint {endpoint} is not compared")
        coefficients = record.get("coefficients")
        if not isinstance(coefficients, list):
            raise TypeError(f"b61n compare endpoint {endpoint} coefficients must be a list")
        expect(coefficients, f"b61n compare endpoint {endpoint} must include coefficients")
        for coefficient in coefficients:
            if not isinstance(coefficient, dict):
                raise TypeError(f"b61n compare endpoint {endpoint} coefficients must be objects")
            expect(coefficient.get("passed") is True,
                   f"b61n compare endpoint {endpoint} coefficient must pass")
            expect(coefficient.get("amflow_present") is True,
                   f"b61n compare endpoint {endpoint} must retain AMFlow coefficients")
            expect(coefficient.get("cpp_present") is True,
                   f"b61n compare endpoint {endpoint} must retain C++ coefficients")
            real_digits = coefficient.get("real_agreement_digits")
            imag_digits = coefficient.get("imag_agreement_digits")
            expect(isinstance(real_digits, int) and real_digits >= 30,
                   f"b61n compare endpoint {endpoint} real agreement must pass compare30")
            expect(isinstance(imag_digits, int) and imag_digits >= 30,
                   f"b61n compare endpoint {endpoint} imag agreement must pass compare30")


def validate_source_evidence(sidecar: dict[str, Any], sidecar_path: Path) -> dict[str, Any]:
    source_evidence = normalize_string_list(sidecar.get("source_evidence"), "source_evidence")
    for required in REQUIRED_SOURCE_EVIDENCE:
        expect(required in source_evidence, f"source_evidence missing {required}")
    evidence_paths = {
        path_text: resolve_sidecar_path(path_text, sidecar_path) for path_text in source_evidence
    }
    for path_text, path in evidence_paths.items():
        expect(path.exists(), f"source_evidence path is missing: {path_text}")

    evidence = load_json(evidence_paths[REQUIRED_SOURCE_EVIDENCE[0]])
    expect(evidence.get("benchmark_id") == "complex_kinematics", "b61n evidence benchmark drifted")
    expect(evidence.get("eta_zero_endpoint_transport_applied") is True,
           "b61n evidence must record endpoint transport")
    expect(evidence.get("full_eta_zero_contour_applied") is False,
           "b61n selected evidence must not claim the full contour")
    compared = evidence.get("compare30", {})
    if not isinstance(compared, dict):
        raise TypeError("b61n evidence compare30 must be an object")
    expect(compared.get("passed") is True, "b61n evidence compare30 must pass")
    transported_integrals = normalize_string_list(
        evidence.get("eta_zero_endpoint_transported_integrals"),
        "b61n evidence eta_zero_endpoint_transported_integrals",
    )
    for endpoint in REVIEWED_VARIANT_ENDPOINTS.values():
        expect(endpoint in transported_integrals, f"b61n evidence missing endpoint {endpoint}")

    compare = load_json(evidence_paths[REQUIRED_SOURCE_EVIDENCE[1]])
    expect(compare.get("comparison") == "cpp-vs-amflow", "b61n compare kind drifted")
    expect(compare.get("failures") == [], "b61n compare must have no failures")
    expect(compare.get("compared_coefficient_count", 0) >= 20,
           "b61n compare must retain selected5 coefficient coverage")
    validate_reviewed_endpoint_compare_evidence(evidence_compare=compared, compare=compare)

    stripped = load_json(evidence_paths[REQUIRED_SOURCE_EVIDENCE[2]])
    continuation = stripped.get("continuation", {})
    if not isinstance(continuation, dict):
        raise TypeError("b61n stripped result continuation must be an object")
    boundary_state = stripped.get("boundary_state", {})
    if not isinstance(boundary_state, dict):
        raise TypeError("b61n stripped result boundary_state must be an object")
    expect(
        continuation.get("direction") == "NegIm" or boundary_state.get("direction") == "NegIm",
        "b61n stripped result must preserve NegIm direction",
    )
    expect(continuation.get("target_location") == "eta=0",
           "b61n stripped result must target eta=0")
    expect(continuation.get("full_eta_zero_contour_applied") is False,
           "b61n stripped result must not claim full eta=0 contour")
    stripped_integrals = normalize_string_list(
        continuation.get("eta_zero_endpoint_transported_integrals"),
        "b61n stripped result eta_zero_endpoint_transported_integrals",
    )
    for endpoint in REVIEWED_VARIANT_ENDPOINTS.values():
        expect(endpoint in stripped_integrals, f"b61n stripped result missing endpoint {endpoint}")
    source_state = load_json(evidence_paths[RETAINED_SOURCE_NUMERIC_EVIDENCE])
    raw_config = source_state.get("amflow_config", {}).get("raw", {}).get("raw")
    source_numeric_text = extract_mathematica_numeric_substitutions(
        normalized_string(raw_config, "b61n source AMFlow config raw")
    )
    source_contour = parse_source_contour_summary(
        normalized_string(stripped.get("summary"), "b61n stripped result summary")
    )
    return {
        **source_contour,
        "retained_numeric_substitutions": normalize_numeric_substitutions(
            source_numeric_text,
            "b61n source Numeric",
        ),
        "reviewed_endpoint_compare_evidence_matched": True,
    }


def audit_sidecar(sidecar_path: Path) -> dict[str, Any]:
    sidecar = load_json(sidecar_path)
    expect(sidecar.get("schema_version") == 1, "sidecar schema_version must be 1")
    expect(
        normalized_string(sidecar.get("benchmark_id"), "benchmark_id") == "complex_kinematics",
        "sidecar benchmark_id must be complex_kinematics",
    )
    expect(
        normalized_string(sidecar.get("lane"), "lane") == "lane5-next7",
        "sidecar lane must be lane5-next7",
    )
    source_evidence = validate_source_evidence(sidecar, sidecar_path)

    raw_variants = sidecar.get("publication_variants")
    if not isinstance(raw_variants, list):
        raise TypeError("publication_variants must be a list")
    expect(len(raw_variants) >= 2, "publication sidecar must cover at least two variants")

    endpoint_integrals: list[str] = []
    variant_ids: list[str] = []
    numeric_fingerprints: set[str] = set()
    for index, variant in enumerate(raw_variants):
        if not isinstance(variant, dict):
            raise TypeError("publication_variants entries must be objects")
        variant_ids.append(normalized_string(variant.get("id"), f"publication_variants[{index}] id"))
        endpoint_integral, variant_numeric = validate_variant(variant, index, source_evidence)
        endpoint_integrals.append(endpoint_integral)
        numeric_fingerprints.update(variant_numeric)
    expect(len(set(variant_ids)) == len(variant_ids), "publication variant ids must be unique")
    expect(
        len(set(endpoint_integrals)) == len(endpoint_integrals),
        "publication variants must not duplicate endpoint integrals",
    )
    expect(
        "box[0,0,1,1]" in endpoint_integrals,
        "publication coverage must include the lane5-next7 reviewed box[0,0,1,1] variant",
    )
    expect(
        len(numeric_fingerprints) >= 2,
        "publication sidecar must exercise multiple distinct Numeric substitutions",
    )

    m6_hook = validate_m6_hook(sidecar.get("m6_qualifier_hook"))
    m7_hook = validate_m7_hook(sidecar.get("m7_parity_signoff_hook"))
    withheld_claims = normalize_string_list(sidecar.get("withheld_claims"), "withheld_claims")
    for claim in WITHHELD_CLAIMS:
        expect(claim in withheld_claims, f"withheld_claims missing {claim!r}")

    return {
        "schema_version": 1,
        "scope": "b61n-publication-qualifier-hook",
        "sidecar_path": str(sidecar_path),
        "benchmark_id": "complex_kinematics",
        "publication_gate_reviewed": True,
        "variant_count": len(raw_variants),
        "reviewed_endpoint_integrals": sorted(endpoint_integrals),
        "new_lane5_endpoint_variant": "box[0,0,1,1]" in endpoint_integrals,
        "multi_numeric_negim_gate_regressed": len(numeric_fingerprints) >= 2,
        "smoke_numeric_cases_shape_only": True,
        "retained_contour_evidence_matched": True,
        "retained_numeric_evidence_matched": True,
        "reviewed_endpoint_compare_evidence_matched": source_evidence[
            "reviewed_endpoint_compare_evidence_matched"
        ],
        "source_contour_fingerprint": source_evidence["contour_fingerprint"],
        "source_minimum_pole_distance_to_contour": str(
            source_evidence["minimum_pole_distance_to_contour"]
        ),
        "m6_qualifier_hook_prepositioned": True,
        "m6_qualifier_hook_currently_promoted": m6_hook[
            "full_eta_zero_contour_applied_observed"
        ],
        "m6_optional_capture_packet": m6_hook["optional_capture_packet"],
        "m7_parity_single_row_hook_prepositioned": True,
        "m7_single_row_path": m7_hook["single_row_path"],
        "withheld_claims": withheld_claims,
    }


def synthetic_sidecar() -> dict[str, Any]:
    numeric_a = {
        "s": "496",
        "t": "-39",
        "p3sq": "7/2",
        "p4sq": "8",
        "m3sq": "2-I",
    }
    numeric_b = {
        "s": "125",
        "t": "-17",
        "p3sq": "0",
        "p4sq": "0",
        "m3sq": "3/2+1/5*I",
    }
    case_a = {
        "id": RETAINED_NUMERIC_CASE_ID,
        "direction": "NegIm",
        "target_location": "eta=0",
        "evidence_scope": RETAINED_NUMERIC_EVIDENCE_SCOPE,
        "contour_fingerprint": REVIEWED_SOURCE_CONTOUR_FINGERPRINT,
        "numeric_substitutions": numeric_a,
        "contour_waypoints": [
            {"real": "0", "imag": "-988.886458875053403341688"},
            {"real": "0", "imag": "-247.221614718763350835422"},
            {"real": "0", "imag": "-1"},
            {"real": "0", "imag": "-0.0625"},
            {"real": "0", "imag": "0"},
        ],
    }
    case_b = {
        "id": "scaled-complex-mass",
        "direction": "NegIm",
        "target_location": "eta=0",
        "evidence_scope": SMOKE_NUMERIC_EVIDENCE_SCOPE,
        "contour_fingerprint": "publication-smoke:scaled-complex-mass-v1",
        "numeric_substitutions": numeric_b,
        "contour_waypoints": [
            {"real": "0", "imag": "-256"},
            {"real": "0", "imag": "-16"},
            {"real": "0", "imag": "-0.25"},
            {"real": "0", "imag": "0"},
        ],
    }
    base_variant = {
        "id": "lane142-primitive-bubble-box1010",
        "endpoint_integral_id": "box[1,0,1,0]",
        "matrix_fingerprint": "lane142-b61n-selected5-primitive-bubble-v1",
        "endpoint_local_model_kind": "b61n-primitive-bubble-regular-taylor-r0",
        "transport_scope": "eta-zero-selected-endpoint-coefficients",
        "coefficient_publication": True,
        "endpoint_extraction_applied": True,
        "full_eta_zero_contour_applied": False,
        "final_solution_samples_used_as_input": False,
        "numeric_substitution_cases": [case_a, case_b],
    }
    new_variant = {
        **base_variant,
        "id": "lane5-next7-primitive-bubble-box0011",
        "endpoint_integral_id": "box[0,0,1,1]",
    }
    return {
        "schema_version": 1,
        "lane": "lane5-next7",
        "benchmark_id": "complex_kinematics",
        "source_evidence": list(REQUIRED_SOURCE_EVIDENCE),
        "publication_variants": [base_variant, new_variant],
        "m6_qualifier_hook": {
            "phase0_id": "complex_kinematics",
            "optional_capture_packet": "b61n-complex-eta-zero-single-row",
            "requires_full_eta_zero_contour_applied": True,
            "full_eta_zero_contour_applied_observed": False,
            "currently_promoted": False,
        },
        "m7_parity_signoff_hook": {
            "release_checklist_section": "parity-signoff",
            "single_row_path": "b61n-complex-contour-propagator-harness",
            "current_state": "blocked-on-m6",
        },
        "withheld_claims": list(WITHHELD_CLAIMS),
    }


def run_self_check() -> dict[str, Any]:
    with tempfile.TemporaryDirectory(prefix="amflow-b61n-publication-qualifier-") as tmp:
        root = Path(tmp)
        sidecar_path = root / "b61n-publication-hook.json"
        write_json(sidecar_path, synthetic_sidecar())
        summary_path = root / "summary.json"
        summary = audit_sidecar(sidecar_path)
        write_json(summary_path, summary)
        summary_written = summary_path.exists()

        off_axis_rejected = False
        try:
            bad = synthetic_sidecar()
            bad["publication_variants"][1]["numeric_substitution_cases"][0][
                "contour_waypoints"
            ][1]["real"] = "0.125"
            bad_path = root / "off-axis.json"
            write_json(bad_path, bad)
            audit_sidecar(bad_path)
        except RuntimeError as error:
            off_axis_rejected = "imaginary axis" in str(error)

        single_numeric_case_rejected = False
        try:
            bad = synthetic_sidecar()
            bad["publication_variants"][1]["numeric_substitution_cases"] = [
                bad["publication_variants"][1]["numeric_substitution_cases"][0]
            ]
            bad_path = root / "single-numeric.json"
            write_json(bad_path, bad)
            audit_sidecar(bad_path)
        except RuntimeError as error:
            single_numeric_case_rejected = "multiple Numeric substitutions" in str(error)

        m6_overclaim_rejected = False
        try:
            bad = synthetic_sidecar()
            bad["m6_qualifier_hook"]["currently_promoted"] = True
            bad_path = root / "m6-overclaim.json"
            write_json(bad_path, bad)
            audit_sidecar(bad_path)
        except RuntimeError as error:
            m6_overclaim_rejected = "self-certify promotion" in str(error)

        m6_promoted_sidecar_rejected = False
        try:
            promoted = synthetic_sidecar()
            promoted["m6_qualifier_hook"]["full_eta_zero_contour_applied_observed"] = True
            promoted["m6_qualifier_hook"]["currently_promoted"] = True
            promoted["m6_qualifier_hook"]["accepted_runtime_result"] = "accepted-runtime.json"
            promoted_path = root / "m6-promoted.json"
            write_json(promoted_path, promoted)
            audit_sidecar(promoted_path)
        except RuntimeError as error:
            m6_promoted_sidecar_rejected = (
                "pre-positioned" in str(error) or "self-certify promotion" in str(error)
            )

        swapped_variant_rejected = False
        try:
            bad = synthetic_sidecar()
            bad["publication_variants"][1]["endpoint_integral_id"] = "box[1,0,1,0]"
            bad_path = root / "swapped-variant.json"
            write_json(bad_path, bad)
            audit_sidecar(bad_path)
        except RuntimeError as error:
            swapped_variant_rejected = "does not match" in str(error)

        invalid_numeric_rejected = False
        try:
            bad = synthetic_sidecar()
            bad["publication_variants"][1]["numeric_substitution_cases"][0][
                "numeric_substitutions"
            ]["m3sq"] = "notI"
            bad_path = root / "invalid-numeric.json"
            write_json(bad_path, bad)
            audit_sidecar(bad_path)
        except (RuntimeError, ValueError) as error:
            invalid_numeric_rejected = (
                "must include real and imaginary parts" in str(error)
                or "must be a finite decimal" in str(error)
            )

        retained_numeric_drift_rejected = False
        try:
            bad = synthetic_sidecar()
            bad["publication_variants"][1]["numeric_substitution_cases"][0][
                "numeric_substitutions"
            ]["s"] = "100"
            bad_path = root / "retained-numeric-drift.json"
            write_json(bad_path, bad)
            audit_sidecar(bad_path)
        except RuntimeError as error:
            retained_numeric_drift_rejected = "retained Numeric case must match" in str(error)

        source_evidence_rejected = False
        try:
            bad = synthetic_sidecar()
            bad["source_evidence"] = bad["source_evidence"][:-1]
            bad_path = root / "missing-source-evidence.json"
            write_json(bad_path, bad)
            audit_sidecar(bad_path)
        except RuntimeError as error:
            source_evidence_rejected = "source_evidence missing" in str(error)

        m7_hook_rejected = False
        try:
            bad = synthetic_sidecar()
            bad["m7_parity_signoff_hook"]["single_row_path"] = "wrong-path"
            bad_path = root / "m7-bad-hook.json"
            write_json(bad_path, bad)
            audit_sidecar(bad_path)
        except RuntimeError as error:
            m7_hook_rejected = "single-row path" in str(error)

    return {
        "publication_gate_reviewed": summary["publication_gate_reviewed"],
        "new_lane5_endpoint_variant": summary["new_lane5_endpoint_variant"],
        "multi_numeric_negim_gate_regressed": summary[
            "multi_numeric_negim_gate_regressed"
        ],
        "smoke_numeric_cases_shape_only": summary["smoke_numeric_cases_shape_only"],
        "retained_contour_evidence_matched": summary["retained_contour_evidence_matched"],
        "retained_numeric_evidence_matched": summary["retained_numeric_evidence_matched"],
        "reviewed_endpoint_compare_evidence_matched": summary[
            "reviewed_endpoint_compare_evidence_matched"
        ],
        "m6_qualifier_hook_prepositioned": summary["m6_qualifier_hook_prepositioned"],
        "m7_parity_single_row_hook_prepositioned": summary[
            "m7_parity_single_row_hook_prepositioned"
        ],
        "off_axis_contour_rejected": off_axis_rejected,
        "single_numeric_case_rejected": single_numeric_case_rejected,
        "m6_overclaim_rejected": m6_overclaim_rejected,
        "m6_promoted_sidecar_rejected": m6_promoted_sidecar_rejected,
        "m7_hook_rejected": m7_hook_rejected,
        "source_evidence_rejected": source_evidence_rejected,
        "swapped_variant_rejected": swapped_variant_rejected,
        "invalid_numeric_rejected": invalid_numeric_rejected,
        "retained_numeric_drift_rejected": retained_numeric_drift_rejected,
        "summary_written": summary_written,
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--sidecar-path",
        type=Path,
        help="b61n publication qualifier sidecar path",
    )
    parser.add_argument(
        "--summary-path",
        type=Path,
        help="Optional output file for the audit summary",
    )
    parser.add_argument(
        "--self-check",
        action="store_true",
        help="Run synthetic b61n publication qualifier checks",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.self_check:
        print(json.dumps(run_self_check(), indent=2, sort_keys=True))
        return 0

    sidecar_path = (
        args.sidecar_path
        if args.sidecar_path is not None
        else repo_root()
        / "tools"
        / "reference-harness"
        / "specs"
        / "m6"
        / "lane5-next7"
        / "b61n-publication-qualifier-hook.json"
    )
    summary = audit_sidecar(sidecar_path)
    if args.summary_path is not None:
        write_json(args.summary_path, summary)
    print(json.dumps(summary, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
