#!/usr/bin/env python3
"""Verify the b63n D2/D4/D6 weighted-residue evidence sidecar schema."""

from __future__ import annotations

import argparse
import json
import re
import sys
import tempfile
from decimal import Decimal, InvalidOperation
from pathlib import Path
from typing import Any


DEFAULT_SIDECAR = Path(
    "tools/reference-harness/specs/m6/lane146/"
    "b63n-d246-weighted-residue-reference-evidence.json"
)

EXPECTED_SOURCE_FILES: dict[str, dict[str, Any]] = {
    "automatic_phasespace_run_wl": {
        "relative_path": "examples/automatic_phasespace/run.wl",
        "sha256": "f9f021e584334cc21fb682526eaf9f55de95b6f2a58b2971ae46a445fd46e2bf",
        "line_ranges": ["17-24", "28-33", "36-42"],
    },
    "amflow_m": {
        "relative_path": "AMFlow.m",
        "sha256": "6fd47002b36399ee71c38e3e43e5e75541d1f2641966ca103fc8b8ce37dc7add",
        "line_ranges": ["461-481", "485-489", "874-881", "911-916", "941-950"],
    },
    "desolver_m": {
        "relative_path": "diffeq_solver/DESolver.m",
        "sha256": "22c63b2aa4a4c8236a9593d39ba7ae8283efa12cb7730401e640ff1b43875585",
        "line_ranges": ["916-927", "1053-1061", "1065-1095"],
    },
}

EXPECTED_PROPAGATORS = [
    "l1^2-msq",
    "(l1+p1)^2",
    "l2^2",
    "(l1+l2+p1)^2",
    "(l1+l2+p1+p2)^2",
    "(l1+l2+p2)^2",
    "(l1+p2)^2",
]

EXPECTED_WEIGHTS: dict[str, dict[str, Any]] = {
    "D2": {
        "denominator_index": 1,
        "propagator_power": 2,
        "propagator": "(l1+p1)^2",
        "structural_form": "inverse_denominator_weight[D2(q2,cos_theta_a)]",
    },
    "D4": {
        "denominator_index": 3,
        "propagator_power": 1,
        "propagator": "(l1+l2+p1)^2",
        "structural_form": "inverse_denominator_weight[D4(q2,cos_theta_a,cos_theta_b)]",
    },
    "D6": {
        "denominator_index": 5,
        "propagator_power": 1,
        "propagator": "(l1+l2+p2)^2",
        "structural_form": "inverse_denominator_weight[D6(q2,cos_theta_a,cos_theta_b)]",
    },
}

SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
COMPARE_ARTIFACT_RE = re.compile(r"\.compare\d*(?:[.-][A-Za-z0-9_-]+)*\.json$")
MATHEMATICA_RUN_COMMAND_RE = re.compile(
    r"(^|[\s/])(math|wolframscript|MathKernel|WolframKernel|Mathematica)(?:$|[\s/.-])"
)
PLACEHOLDER_TEXT = {"", "placeholder", "todo", "tbd", "unknown", "none", "null"}
MINIMUM_DIGITS = 50
REQUIRED_PUBLISHED_EPS_ORDERS = [0, 1, 2, 3]
FULL_WEIGHTED_TARGET = "j[phase,1,2,1,1,1,1,1]"
BANNED_RETAINED_ARTIFACT_SUFFIXES = (
    ".cpp-result.json",
    ".stripped-result.json",
    ".amflow-state.json",
    ".golden-manifest.json",
)
BANNED_RUNTIME_RUN_COMMAND_MARKERS = (
    "amflow-cli",
    "solve-series",
    ".cpp-result.json",
    ".stripped-result.json",
    ".amflow-state.json",
    ".golden-manifest.json",
)
REQUIRED_D246_RUN_COMMAND_TOPIC_MARKERS = (
    "d246",
    "d2d4d6",
    "d2-d4-d6",
    "d2_d4_d6",
)
REQUIRED_D246_RUN_COMMAND_SCOPE_MARKERS = (
    "automatic_phasespace",
    "automatic-phasespace",
    "b63n",
)
REQUIRED_SKELETON_COEFFICIENT_SCOPE = (
    "contiguous epsilon range matching the future runtime publication scope"
)
REQUIRED_SKELETON_PUBLICATION_BLOCKERS: tuple[tuple[str, tuple[str, ...]], ...] = (
    ("precision request blocker", ("precision_requested_digits", "high precision")),
    ("epsilon scope blocker", ("eps_order_requested", "contiguous epsilon range")),
    (
        "run artifact blocker",
        ("run_command", "run_log", "raw_output", "raw_output_sha256"),
    ),
    (
        "empty coefficient blocker",
        ("D2", "D4", "D6", FULL_WEIGHTED_TARGET, "coefficient arrays"),
    ),
    ("comparison blocker", ("independent comparison", "50-digit")),
)
REQUIRED_SKELETON_ANTI_FAKE_NOTE_FRAGMENTS = (
    "Skeleton only",
    "publishes no AMFlow coefficient values",
    "must not promote b63n",
)
PUBLISHED_ANTI_FAKE_NOTE_BANNED_FRAGMENTS = (
    "Skeleton only",
    "pending",
    "publishes no AMFlow coefficient values",
    "must not promote b63n",
)


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


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


def require_object(raw: Any, label: str) -> dict[str, Any]:
    if not isinstance(raw, dict):
        raise TypeError(f"{label} must be an object")
    return raw


def require_list(raw: Any, label: str) -> list[Any]:
    if not isinstance(raw, list):
        raise TypeError(f"{label} must be a list")
    return raw


def require_string(raw: Any, label: str, *, allow_placeholder: bool = False) -> str:
    if not isinstance(raw, str):
        raise TypeError(f"{label} must be a string")
    value = raw.strip()
    expect(value, f"{label} must not be empty")
    if not allow_placeholder:
        expect(
            value.lower() not in PLACEHOLDER_TEXT and not (value.startswith("<") and value.endswith(">")),
            f"{label} must not be a placeholder",
        )
    return value


def require_bool(raw: Any, label: str) -> bool:
    if not isinstance(raw, bool):
        raise TypeError(f"{label} must be a bool")
    return raw


def require_int(raw: Any, label: str) -> int:
    if not isinstance(raw, int) or isinstance(raw, bool):
        raise TypeError(f"{label} must be an int")
    return raw


def require_exact(raw: Any, expected: Any, label: str) -> None:
    expect(raw == expected, f"{label} must be {expected!r}, got {raw!r}")


def require_explicit_null(payload: dict[str, Any], field: str, label: str) -> None:
    expect(field in payload, f"{label} must be explicitly null for skeleton evidence")
    expect(payload[field] is None, f"{label} must be null for skeleton evidence")


def require_sha256(raw: Any, label: str) -> str:
    if not isinstance(raw, str):
        raise TypeError(f"{label} must be a string")
    expect(raw == raw.strip(), f"{label} must not contain surrounding whitespace")
    value = require_string(raw, label)
    expect(SHA256_RE.fullmatch(value) is not None, f"{label} must be a lowercase sha256")
    return value


def require_absolute_path_text(raw: Any, label: str) -> str:
    if not isinstance(raw, str):
        raise TypeError(f"{label} must be a string")
    expect(raw == raw.strip(), f"{label} must not contain surrounding whitespace")
    value = require_string(raw, label)
    expect("\\" not in value, f"{label} must use POSIX path separators")
    path = Path(value)
    expect(path.is_absolute(), f"{label} must be an absolute path")
    expect(path.as_posix() == value, f"{label} must be a normalized POSIX path")
    expect(
        "." not in path.parts and ".." not in path.parts,
        f"{label} must not contain . or .. components",
    )
    if value != "/":
        expect(not value.endswith("/"), f"{label} must not end with a slash")
    return value


def require_relative_artifact_path_text(raw: Any, label: str) -> str:
    if not isinstance(raw, str):
        raise TypeError(f"{label} must be a string")
    expect(raw == raw.strip(), f"{label} must not contain surrounding whitespace")
    value = require_string(raw, label)
    expect("\\" not in value, f"{label} must use POSIX path separators")
    path = Path(value)
    expect(not path.is_absolute(), f"{label} must be a relative artifact path")
    expect(path.as_posix() == value, f"{label} must be a normalized POSIX path")
    expect(
        "." not in path.parts and ".." not in path.parts,
        f"{label} must not contain . or .. components",
    )
    expect(not value.endswith("/"), f"{label} must not end with a slash")
    return value


def require_coefficient_source_path_text(raw: Any, label: str) -> str:
    value = require_relative_artifact_path_text(raw, label)
    filename = Path(value).name
    banned_suffix = next(
        (suffix for suffix in BANNED_RETAINED_ARTIFACT_SUFFIXES if filename.endswith(suffix)),
        None,
    )
    expect(
        banned_suffix is None,
        f"{label} must not cite retained runtime/comparison artifacts as coefficient source",
    )
    expect(
        COMPARE_ARTIFACT_RE.search(filename) is None,
        f"{label} must not cite retained runtime/comparison artifacts as coefficient source",
    )
    return value


def require_raw_output_path_text(raw: Any, label: str) -> str:
    value = require_relative_artifact_path_text(raw, label)
    filename = Path(value).name
    banned_suffix = next(
        (suffix for suffix in BANNED_RETAINED_ARTIFACT_SUFFIXES if filename.endswith(suffix)),
        None,
    )
    expect(
        banned_suffix is None,
        f"{label} must not cite retained runtime/comparison artifacts as raw AMFlow output",
    )
    expect(
        COMPARE_ARTIFACT_RE.search(filename) is None,
        f"{label} must not cite retained runtime/comparison artifacts as raw AMFlow output",
    )
    return value


def require_run_log_path_text(raw: Any, label: str) -> str:
    value = require_relative_artifact_path_text(raw, label)
    filename = Path(value).name
    banned_suffix = next(
        (suffix for suffix in BANNED_RETAINED_ARTIFACT_SUFFIXES if filename.endswith(suffix)),
        None,
    )
    expect(
        banned_suffix is None,
        f"{label} must not cite retained runtime/comparison artifacts as AMFlow run log",
    )
    expect(
        COMPARE_ARTIFACT_RE.search(filename) is None,
        f"{label} must not cite retained runtime/comparison artifacts as AMFlow run log",
    )
    return value


def require_comparison_artifact_path_text(raw: Any, label: str) -> str:
    value = require_relative_artifact_path_text(raw, label)
    filename = Path(value).name
    expect(
        COMPARE_ARTIFACT_RE.search(filename) is not None,
        f"{label} must cite a comparison artifact named *.compare*.json",
    )
    return value


def optional_relative_artifact_path_text(raw: Any, label: str) -> str | None:
    if raw is None:
        return None
    return require_relative_artifact_path_text(raw, label)


def optional_raw_output_path_text(raw: Any, label: str) -> str | None:
    if raw is None:
        return None
    return require_raw_output_path_text(raw, label)


def optional_run_log_path_text(raw: Any, label: str) -> str | None:
    if raw is None:
        return None
    return require_run_log_path_text(raw, label)


def require_decimal_string(raw: Any, label: str) -> str:
    value = require_string(raw, label)
    try:
        parsed = Decimal(value)
    except InvalidOperation as error:
        raise ValueError(f"{label} must be a finite decimal string") from error
    expect(parsed.is_finite(), f"{label} must be finite")
    return value


def optional_int(raw: Any, label: str) -> int | None:
    if raw is None:
        return None
    return require_int(raw, label)


def optional_string(raw: Any, label: str) -> str | None:
    if raw is None:
        return None
    return require_string(raw, label)


def require_run_command_text(raw: Any, label: str) -> str:
    if not isinstance(raw, str):
        raise TypeError(f"{label} must be a string")
    expect(raw == raw.strip(), f"{label} must not contain surrounding whitespace")
    value = require_string(raw, label)
    lowered = value.lower()
    banned_markers = [
        marker
        for marker in BANNED_RUNTIME_RUN_COMMAND_MARKERS
        if marker.lower() in lowered
    ]
    expect(
        not banned_markers,
        f"{label} must not invoke C++ runtime or retained-sample artifacts: {banned_markers!r}",
    )
    expect(
        MATHEMATICA_RUN_COMMAND_RE.search(value) is not None,
        f"{label} must invoke a Mathematica or Wolfram noninteractive runner",
    )
    expect(
        any(marker in lowered for marker in REQUIRED_D246_RUN_COMMAND_TOPIC_MARKERS),
        f"{label} must identify the D2/D4/D6 extraction scope",
    )
    expect(
        any(marker in lowered for marker in REQUIRED_D246_RUN_COMMAND_SCOPE_MARKERS),
        f"{label} must identify b63n or automatic_phasespace extraction scope",
    )
    return value


def optional_run_command_text(raw: Any, label: str) -> str | None:
    if raw is None:
        return None
    return require_run_command_text(raw, label)


def require_string_list(raw: Any, expected: list[str], label: str) -> None:
    values = require_list(raw, label)
    expect(all(isinstance(item, str) for item in values), f"{label} entries must be strings")
    require_exact(values, expected, label)


def require_text_fragments(value: str, label: str, fragments: tuple[str, ...]) -> None:
    missing = [fragment for fragment in fragments if fragment not in value]
    expect(not missing, f"{label} must contain {missing!r}")


def require_publication_blocker_coverage(blockers: list[str]) -> None:
    for label, fragments in REQUIRED_SKELETON_PUBLICATION_BLOCKERS:
        covered = any(all(fragment in blocker for fragment in fragments) for blocker in blockers)
        expect(covered, f"publication_blockers must include {label}")


def validate_source_files(payload: dict[str, Any]) -> None:
    source_root = require_absolute_path_text(payload.get("source_root"), "source_root")
    source_files = require_object(payload.get("source_files"), "source_files")
    expect(set(source_files) == set(EXPECTED_SOURCE_FILES), "source_files keys must match the D246 plan")
    for key, expected in EXPECTED_SOURCE_FILES.items():
        entry = require_object(source_files.get(key), f"source_files.{key}")
        source_path = require_absolute_path_text(entry.get("path"), f"source_files.{key}.path")
        expect(
            source_path.startswith(source_root.rstrip("/") + "/"),
            f"source_files.{key}.path must live under source_root",
        )
        expected_path = f"{source_root.rstrip('/')}/{expected['relative_path']}"
        require_exact(source_path, expected_path, f"source_files.{key}.path")
        require_exact(entry.get("sha256"), expected["sha256"], f"source_files.{key}.sha256")
        require_string_list(
            entry.get("line_ranges"),
            expected["line_ranges"],
            f"source_files.{key}.line_ranges",
        )


def validate_parameter_set(parameters: dict[str, Any], *, published: bool) -> None:
    require_exact(parameters.get("family"), "phase", "amflow_parameter_set.family")
    require_exact(parameters.get("loops"), ["l1", "l2"], "amflow_parameter_set.loops")
    if "legs" in parameters:
        require_exact(parameters.get("legs"), ["p1", "p2"], "amflow_parameter_set.legs")
    if "conservation" in parameters:
        require_exact(parameters.get("conservation"), [], "amflow_parameter_set.conservation")
    if "replacement" in parameters:
        replacement = require_object(
            parameters.get("replacement"),
            "amflow_parameter_set.replacement",
        )
        require_exact(replacement.get("p1^2"), "0", "amflow_parameter_set.replacement.p1^2")
        require_exact(replacement.get("p2^2"), "0", "amflow_parameter_set.replacement.p2^2")
        require_exact(
            replacement.get("(p1+p2)^2"),
            "s",
            "amflow_parameter_set.replacement.(p1+p2)^2",
        )
    if "propagators" in parameters:
        require_exact(
            parameters.get("propagators"),
            EXPECTED_PROPAGATORS,
            "amflow_parameter_set.propagators",
        )
    numeric = require_object(parameters.get("numeric"), "amflow_parameter_set.numeric")
    require_exact(numeric.get("s"), "100", "amflow_parameter_set.numeric.s")
    require_exact(numeric.get("msq"), "1", "amflow_parameter_set.numeric.msq")
    require_exact(parameters.get("prescription"), [0, 0], "amflow_parameter_set.prescription")
    require_exact(parameters.get("cut"), [1, 0, 1, 0, 1, 0, 0], "amflow_parameter_set.cut")
    require_exact(
        parameters.get("target"),
        "j[phase,1,2,1,1,1,1,1]",
        "amflow_parameter_set.target",
    )

    precision = optional_int(
        parameters.get("precision_requested_digits"),
        "amflow_parameter_set.precision_requested_digits",
    )
    eps_order = optional_int(
        parameters.get("eps_order_requested"),
        "amflow_parameter_set.eps_order_requested",
    )
    working_precision = optional_int(
        parameters.get("working_precision_digits"),
        "amflow_parameter_set.working_precision_digits",
    )
    run_command = optional_run_command_text(
        parameters.get("run_command"),
        "amflow_parameter_set.run_command",
    )
    run_log = optional_run_log_path_text(
        parameters.get("run_log"),
        "amflow_parameter_set.run_log",
    )
    raw_output = optional_raw_output_path_text(
        parameters.get("raw_output"),
        "amflow_parameter_set.raw_output",
    )
    raw_output_sha256 = parameters.get("raw_output_sha256")
    if raw_output_sha256 is not None:
        require_sha256(raw_output_sha256, "amflow_parameter_set.raw_output_sha256")

    if not published:
        for field in (
            "precision_requested_digits",
            "eps_order_requested",
            "working_precision_digits",
            "run_command",
            "run_log",
            "raw_output",
            "raw_output_sha256",
        ):
            require_explicit_null(
                parameters,
                field,
                f"amflow_parameter_set.{field}",
            )
        expect(
            precision is None,
            "skeleton evidence must not record precision_requested_digits",
        )
        expect(
            eps_order is None,
            "skeleton evidence must not record eps_order_requested",
        )
        expect(
            working_precision is None,
            "skeleton evidence must not record working_precision_digits",
        )
        expect(run_command is None, "skeleton evidence must not record run_command")
        expect(run_log is None, "skeleton evidence must not record run_log")
        expect(raw_output is None, "skeleton evidence must not record raw_output")
        expect(
            raw_output_sha256 is None,
            "skeleton evidence must not record raw_output_sha256",
        )
        return

    expect(precision is not None and precision >= MINIMUM_DIGITS,
           "published evidence must request at least 50 precision digits")
    expect(eps_order is not None and eps_order >= 4,
           "published evidence must request epsilon order at least 4")
    expect(working_precision is not None and working_precision >= MINIMUM_DIGITS,
           "published evidence must record at least 50 working precision digits")
    expect(run_command is not None, "published evidence must record run_command")
    expect(run_log is not None, "published evidence must record run_log")
    expect(raw_output is not None, "published evidence must record raw_output")
    expect(raw_output_sha256 is not None, "published evidence must record raw_output_sha256")


def validate_cutkosky_structure(structure: dict[str, Any]) -> None:
    require_exact(structure.get("phase_volume_loop_count"), 2, "cutkosky_structure.phase_volume_loop_count")
    require_exact(
        structure.get("cut_denominators"),
        ["D1", "D3", "D5"],
        "cutkosky_structure.cut_denominators",
    )
    require_exact(
        structure.get("uncut_weight_denominators"),
        ["D2", "D4", "D6", "D7"],
        "cutkosky_structure.uncut_weight_denominators",
    )
    require_exact(
        structure.get("prefactor_formula"),
        "K_2(eps) = -2*(Pi^(2-eps)*(2*Pi)^(2*eps-4))^2",
        "cutkosky_structure.prefactor_formula",
    )
    require_exact(structure.get("eta_direction"), "NegIm", "cutkosky_structure.eta_direction")
    require_exact(
        structure.get("endpoint_selection_rule"),
        "DESolver PickZeroRuleS integer eta-zero term",
        "cutkosky_structure.endpoint_selection_rule",
    )
    require_exact(
        structure.get("final_projection"),
        "AMFlow Cutkosky Im projection",
        "cutkosky_structure.final_projection",
    )


def validate_coefficient(coefficient: dict[str, Any], label: str) -> tuple[int, int]:
    eps_order = require_int(coefficient.get("eps_order"), f"{label}.eps_order")
    require_exact(coefficient.get("eta_power"), 0, f"{label}.eta_power")
    require_exact(coefficient.get("log_power"), 0, f"{label}.log_power")
    require_exact(coefficient.get("region_key"), "integer", f"{label}.region_key")
    require_decimal_string(coefficient.get("real"), f"{label}.real")
    require_decimal_string(coefficient.get("imaginary"), f"{label}.imaginary")
    require_coefficient_source_path_text(coefficient.get("source"), f"{label}.source")
    require_sha256(coefficient.get("source_sha256"), f"{label}.source_sha256")
    require_string(coefficient.get("extraction_label"), f"{label}.extraction_label")
    working_precision = require_int(
        coefficient.get("working_precision_digits"),
        f"{label}.working_precision_digits",
    )
    agreement_digits = require_int(coefficient.get("agreement_digits"), f"{label}.agreement_digits")
    expect(
        working_precision >= MINIMUM_DIGITS,
        f"{label}.working_precision_digits must be at least 50",
    )
    expect(agreement_digits >= MINIMUM_DIGITS, f"{label}.agreement_digits must be at least 50")
    expect(
        agreement_digits <= working_precision,
        f"{label}.agreement_digits must not exceed working precision",
    )
    return eps_order, agreement_digits


def validate_reference_validation(
    raw_validation: Any,
    *,
    label: str,
    published: bool,
) -> int | None:
    validation = require_object(raw_validation, f"{label}.reference_validation")
    passed = require_bool(validation.get("passed"), f"{label}.reference_validation.passed")
    coefficient_published = require_bool(
        validation.get("coefficient_published"),
        f"{label}.reference_validation.coefficient_published",
    )
    final_solution_samples = require_bool(
        validation.get("final_solution_samples_used_as_input"),
        f"{label}.reference_validation.final_solution_samples_used_as_input",
    )
    synthetic_fixture = require_bool(
        validation.get("synthetic_fixture"),
        f"{label}.reference_validation.synthetic_fixture",
    )
    expect(
        not final_solution_samples,
        f"{label} must not use final solution samples as input",
    )
    expect(not synthetic_fixture, f"{label} must not use synthetic fixtures")

    if not published:
        expect(not passed, f"{label} skeleton validation must not pass")
        expect(not coefficient_published, f"{label} skeleton must not publish coefficients")
        require_explicit_null(
            validation,
            "minimum_digit_agreement",
            f"{label}.reference_validation.minimum_digit_agreement",
        )
        require_explicit_null(
            validation,
            "comparison_artifact",
            f"{label}.reference_validation.comparison_artifact",
        )
        require_string(
            validation.get("blocked_reason"),
            f"{label}.reference_validation.blocked_reason",
        )
        return None

    expect(passed, f"{label} published validation must pass")
    expect(coefficient_published, f"{label} published validation must publish coefficients")
    expect(
        validation.get("blocked_reason") is None,
        f"{label} published validation must not retain blocked_reason",
    )
    minimum_digits = require_int(
        validation.get("minimum_digit_agreement"),
        f"{label}.reference_validation.minimum_digit_agreement",
    )
    expect(minimum_digits >= MINIMUM_DIGITS, f"{label} must retain at least 50 digit agreement")
    require_comparison_artifact_path_text(
        validation.get("comparison_artifact"),
        f"{label}.reference_validation.comparison_artifact",
    )
    return minimum_digits


def contiguous(values: list[int]) -> bool:
    if not values:
        return False
    ordered = sorted(values)
    return ordered == list(range(ordered[0], ordered[-1] + 1))


def validate_weight(weight: dict[str, Any], *, published: bool, index: int) -> tuple[str, list[int], int | None]:
    label = f"weights[{index}]"
    denominator_id = require_string(weight.get("denominator_id"), f"{label}.denominator_id")
    expected = EXPECTED_WEIGHTS.get(denominator_id)
    expect(expected is not None, f"{label}.denominator_id must be one of D2, D4, D6")
    for key, expected_value in expected.items():
        require_exact(weight.get(key), expected_value, f"{label}.{key}")

    coefficients = require_list(weight.get("coefficients"), f"{label}.coefficients")
    minimum_digits = validate_reference_validation(
        weight.get("reference_validation"),
        label=label,
        published=published,
    )

    if not published:
        require_exact(
            weight.get("required_coefficient_scope"),
            REQUIRED_SKELETON_COEFFICIENT_SCOPE,
            f"{label}.required_coefficient_scope",
        )
        reference_validation = require_object(
            weight.get("reference_validation"),
            f"{label}.reference_validation",
        )
        blocked_reason = require_string(
            reference_validation.get("blocked_reason"),
            f"{label}.reference_validation.blocked_reason",
        )
        require_text_fragments(
            blocked_reason,
            f"{label}.reference_validation.blocked_reason",
            ("pending", "AMFlow", denominator_id, FULL_WEIGHTED_TARGET),
        )
        expect(coefficients == [], f"{label}.coefficients must be empty for skeleton evidence")
        return denominator_id, [], None

    expect(coefficients, f"{label}.coefficients must not be empty for published evidence")
    coefficient_evidence = [
        validate_coefficient(
            require_object(coefficient, f"{label}.coefficients[{coefficient_index}]"),
            f"{label}.coefficients[{coefficient_index}]",
        )
        for coefficient_index, coefficient in enumerate(coefficients)
    ]
    eps_orders = [eps_order for eps_order, _agreement_digits in coefficient_evidence]
    coefficient_agreement_floor = min(
        agreement_digits for _eps_order, agreement_digits in coefficient_evidence
    )
    expect(
        minimum_digits is not None and minimum_digits <= coefficient_agreement_floor,
        f"{label}.reference_validation.minimum_digit_agreement must not exceed "
        "the coefficient agreement floor",
    )
    expect(len(set(eps_orders)) == len(eps_orders), f"{label}.coefficients has duplicate eps_order")
    expect(contiguous(eps_orders), f"{label}.coefficients must publish a contiguous eps_order range")
    return denominator_id, sorted(eps_orders), minimum_digits


def validate_anti_fake(anti_fake: dict[str, Any], *, published: bool) -> None:
    for key in (
        "retained_solution_samples_used_as_input",
        "synthetic_fixture",
        "self_comparison",
        "full_eta_zero_contour_applied",
    ):
        expect(not require_bool(anti_fake.get(key), f"anti_fake.{key}"), f"anti_fake.{key} must be false")
    tolerance_digits = require_int(anti_fake.get("tolerance_digits"), "anti_fake.tolerance_digits")
    expect(tolerance_digits >= MINIMUM_DIGITS, "anti_fake.tolerance_digits must be at least 50")
    if not published:
        note = require_string(anti_fake.get("notes"), "anti_fake.notes")
        require_text_fragments(
            note,
            "anti_fake.notes",
            REQUIRED_SKELETON_ANTI_FAKE_NOTE_FRAGMENTS,
        )
        return
    if "notes" not in anti_fake:
        return
    note = require_string(anti_fake.get("notes"), "anti_fake.notes")
    banned = [fragment for fragment in PUBLISHED_ANTI_FAKE_NOTE_BANNED_FRAGMENTS if fragment in note]
    expect(
        not banned,
        f"published anti_fake.notes must not retain skeleton non-claim text: {banned!r}",
    )


def validate_sidecar_payload(payload: dict[str, Any]) -> dict[str, Any]:
    require_exact(payload.get("schema_version"), 1, "schema_version")
    require_exact(payload.get("runtime_lane"), "b63n", "runtime_lane")
    require_exact(payload.get("benchmark_id"), "automatic_phasespace", "benchmark_id")
    require_exact(payload.get("surface_label"), "phase[1,2,1,1,1,1,1]", "surface_label")
    require_exact(payload.get("source_kind"), "upstream-amflow-mathematica", "source_kind")

    passed = require_bool(payload.get("passed"), "passed")
    skeleton = False
    if "skeleton" in payload:
        skeleton = require_bool(payload.get("skeleton"), "skeleton")
    expect(not (passed and skeleton), "skeleton evidence cannot also pass publication")
    expect(
        passed or skeleton,
        "D246 evidence must be either published evidence or an explicit skeleton",
    )
    published = passed and not skeleton
    if skeleton:
        status = require_string(payload.get("status"), "status")
        status_text = status.lower()
        expect(
            "skeleton" in status_text and "pending" in status_text,
            "skeleton evidence status must say skeleton and pending",
        )
    elif payload.get("status") is not None:
        status = require_string(payload.get("status"), "status")
        status_text = status.lower()
        expect(
            "skeleton" not in status_text and "pending" not in status_text,
            "published evidence status must not retain skeleton or pending text",
        )

    validate_source_files(payload)
    validate_parameter_set(
        require_object(payload.get("amflow_parameter_set"), "amflow_parameter_set"),
        published=published,
    )
    validate_cutkosky_structure(
        require_object(payload.get("cutkosky_structure"), "cutkosky_structure")
    )
    validate_anti_fake(require_object(payload.get("anti_fake"), "anti_fake"), published=published)

    weights = require_list(payload.get("weights"), "weights")
    expect(len(weights) == 3, "weights must contain exactly D2, D4, and D6")
    seen: set[str] = set()
    eps_orders_by_weight: dict[str, list[int]] = {}
    validation_digit_floor: list[int] = []
    for index, raw_weight in enumerate(weights):
        denominator_id, eps_orders, minimum_digits = validate_weight(
            require_object(raw_weight, f"weights[{index}]"),
            published=published,
            index=index,
        )
        expect(denominator_id not in seen, f"duplicate weight entry for {denominator_id}")
        seen.add(denominator_id)
        eps_orders_by_weight[denominator_id] = eps_orders
        if minimum_digits is not None:
            validation_digit_floor.append(minimum_digits)
    expect(seen == set(EXPECTED_WEIGHTS), "weights must contain D2, D4, and D6")

    if published:
        expected_orders: list[int] | None = None
        for denominator_id in sorted(eps_orders_by_weight):
            orders = eps_orders_by_weight[denominator_id]
            if expected_orders is None:
                expected_orders = orders
            else:
                expect(
                    orders == expected_orders,
                    "published weights must share the same contiguous eps_order scope",
                )
        expect(
            expected_orders is not None
            and all(order in expected_orders for order in REQUIRED_PUBLISHED_EPS_ORDERS),
            "published weights must include the D246 eps^0..eps^3 coefficient scope",
        )
        blockers = payload.get("publication_blockers", [])
        expect(blockers in ([], None), "published evidence must not retain publication_blockers")
    else:
        blockers = require_list(payload.get("publication_blockers"), "publication_blockers")
        expect(blockers, "skeleton evidence must record publication_blockers")
        blocker_texts: list[str] = []
        for blocker_index, raw_blocker in enumerate(blockers):
            blocker_texts.append(require_string(raw_blocker, f"publication_blockers[{blocker_index}]"))
        require_publication_blocker_coverage(blocker_texts)

    return {
        "schema_version": 1,
        "verifier": "b63n-d246-weighted-residue-evidence-v1",
        "schema_valid": True,
        "published_evidence": published,
        "skeleton_evidence": skeleton,
        "runtime_lane": payload["runtime_lane"],
        "benchmark_id": payload["benchmark_id"],
        "surface_label": payload["surface_label"],
        "weights": sorted(seen),
        "eps_orders_by_weight": eps_orders_by_weight,
        "minimum_digit_agreement": min(validation_digit_floor) if validation_digit_floor else None,
        "m6_closure_claimed": False,
        "m7_closure_claimed": False,
    }


def verify_sidecar(path: Path) -> dict[str, Any]:
    summary = validate_sidecar_payload(read_json(path))
    summary["sidecar_path"] = str(path)
    return summary


def sample_coefficient(denominator_id: str, eps_order: int) -> dict[str, Any]:
    return {
        "eps_order": eps_order,
        "eta_power": 0,
        "log_power": 0,
        "region_key": "integer",
        "real": f"{eps_order + 1}.125",
        "imaginary": f"-0.{eps_order + 1}25",
        "source": f"artifacts/{denominator_id.lower()}-eps{eps_order}.json",
        "source_sha256": "a" * 64,
        "extraction_label": f"{denominator_id}-integer-eps{eps_order}",
        "working_precision_digits": 80,
        "agreement_digits": 60,
    }


def skeleton_fixture() -> dict[str, Any]:
    return {
        "schema_version": 1,
        "runtime_lane": "b63n",
        "benchmark_id": "automatic_phasespace",
        "surface_label": "phase[1,2,1,1,1,1,1]",
        "source_kind": "upstream-amflow-mathematica",
        "status": "skeleton-pending-amflow-reference-run",
        "skeleton": True,
        "source_root": "/example/amflow",
        "source_files": {
            key: {
                "path": f"/example/amflow/{spec['relative_path']}",
                "sha256": spec["sha256"],
                "line_ranges": spec["line_ranges"],
            }
            for key, spec in EXPECTED_SOURCE_FILES.items()
        },
        "amflow_parameter_set": {
            "family": "phase",
            "loops": ["l1", "l2"],
            "legs": ["p1", "p2"],
            "conservation": [],
            "replacement": {
                "p1^2": "0",
                "p2^2": "0",
                "(p1+p2)^2": "s",
            },
            "propagators": EXPECTED_PROPAGATORS,
            "numeric": {"s": "100", "msq": "1"},
            "prescription": [0, 0],
            "cut": [1, 0, 1, 0, 1, 0, 0],
            "target": "j[phase,1,2,1,1,1,1,1]",
            "precision_requested_digits": None,
            "eps_order_requested": None,
            "working_precision_digits": None,
            "run_command": None,
            "run_log": None,
            "raw_output": None,
            "raw_output_sha256": None,
        },
        "cutkosky_structure": {
            "phase_volume_loop_count": 2,
            "cut_denominators": ["D1", "D3", "D5"],
            "uncut_weight_denominators": ["D2", "D4", "D6", "D7"],
            "prefactor_formula": "K_2(eps) = -2*(Pi^(2-eps)*(2*Pi)^(2*eps-4))^2",
            "eta_direction": "NegIm",
            "endpoint_selection_rule": "DESolver PickZeroRuleS integer eta-zero term",
            "final_projection": "AMFlow Cutkosky Im projection",
        },
        "weights": [
            {
                "denominator_id": denominator_id,
                **spec,
                "coefficients": [],
                "required_coefficient_scope": (
                    "contiguous epsilon range matching the future runtime publication scope"
                ),
                "reference_validation": {
                    "passed": False,
                    "minimum_digit_agreement": None,
                    "comparison_artifact": None,
                    "final_solution_samples_used_as_input": False,
                    "synthetic_fixture": False,
                    "coefficient_published": False,
                    "blocked_reason": (
                        f"pending high-precision AMFlow extraction for {denominator_id} "
                        f"from the full {FULL_WEIGHTED_TARGET} target"
                    ),
                },
            }
            for denominator_id, spec in EXPECTED_WEIGHTS.items()
        ],
        "anti_fake": {
            "retained_solution_samples_used_as_input": False,
            "synthetic_fixture": False,
            "self_comparison": False,
            "tolerance_digits": 50,
            "full_eta_zero_contour_applied": False,
            "notes": (
                "Skeleton only. This file establishes the D2/D4/D6 evidence location and schema, "
                "but it publishes no AMFlow coefficient values and must not promote b63n."
            ),
        },
        "publication_blockers": [
            "precision_requested_digits unset; rerun or recapture the upstream Mathematica surface at reviewed high precision",
            "eps_order_requested unset; publish a contiguous epsilon range matching the runtime scope",
            "run_command, run_log, raw_output, and raw_output_sha256 unset",
            (
                "D2, D4, and D6 coefficient arrays are empty for the full "
                f"{FULL_WEIGHTED_TARGET} target"
            ),
            "independent comparison artifacts and 50-digit agreement claims are absent",
        ],
        "passed": False,
    }


def full_fixture() -> dict[str, Any]:
    payload = skeleton_fixture()
    payload.pop("status")
    payload["skeleton"] = False
    payload["passed"] = True
    payload["publication_blockers"] = []
    payload["anti_fake"].pop("notes", None)
    parameter_set = payload["amflow_parameter_set"]
    parameter_set["precision_requested_digits"] = 80
    parameter_set["eps_order_requested"] = 4
    parameter_set["working_precision_digits"] = 80
    parameter_set["run_command"] = "math -script automatic_phasespace_d246.wl"
    parameter_set["run_log"] = "artifacts/d246/run.log"
    parameter_set["raw_output"] = "artifacts/d246/raw-output.json"
    parameter_set["raw_output_sha256"] = "b" * 64
    for weight in payload["weights"]:
        denominator_id = weight["denominator_id"]
        weight["coefficients"] = [
            sample_coefficient(denominator_id, eps_order) for eps_order in range(4)
        ]
        weight["reference_validation"] = {
            "passed": True,
            "minimum_digit_agreement": 60,
            "comparison_artifact": f"artifacts/{denominator_id.lower()}.compare.json",
            "final_solution_samples_used_as_input": False,
            "synthetic_fixture": False,
            "coefficient_published": True,
        }
    return payload


def rejected(payload: dict[str, Any], expected: str) -> bool:
    try:
        validate_sidecar_payload(payload)
    except Exception as error:  # noqa: BLE001 - self-check records fail-closed behavior.
        return expected in str(error)
    return False


def run_self_check() -> dict[str, Any]:
    with tempfile.TemporaryDirectory() as tmp:
        skeleton_path = Path(tmp) / "skeleton.json"
        full_path = Path(tmp) / "full.json"
        write_json(skeleton_path, skeleton_fixture())
        write_json(full_path, full_fixture())
        skeleton_summary = verify_sidecar(skeleton_path)
        full_summary = verify_sidecar(full_path)

    bad_skeleton_passed = skeleton_fixture()
    bad_skeleton_passed["passed"] = True

    bad_unclassified_unpublished = skeleton_fixture()
    bad_unclassified_unpublished["skeleton"] = False
    bad_unclassified_unpublished.pop("status")

    bad_skeleton_placeholder_blocker = skeleton_fixture()
    bad_skeleton_placeholder_blocker["weights"][0]["reference_validation"][
        "blocked_reason"
    ] = "todo"

    bad_skeleton_blocker_missing_target = skeleton_fixture()
    bad_skeleton_blocker_missing_target["weights"][0]["reference_validation"][
        "blocked_reason"
    ] = "pending high-precision AMFlow extraction for D2"

    bad_skeleton_scope = skeleton_fixture()
    bad_skeleton_scope["weights"][1]["required_coefficient_scope"] = "eps0 only"

    bad_skeleton_precision_claim = skeleton_fixture()
    bad_skeleton_precision_claim["amflow_parameter_set"]["precision_requested_digits"] = 80

    bad_skeleton_run_artifact_claim = skeleton_fixture()
    bad_skeleton_run_artifact_claim["amflow_parameter_set"]["raw_output_sha256"] = "c" * 64

    bad_skeleton_missing_artifact_slot = skeleton_fixture()
    del bad_skeleton_missing_artifact_slot["amflow_parameter_set"]["raw_output"]

    bad_skeleton_placeholder_publication_blocker = skeleton_fixture()
    bad_skeleton_placeholder_publication_blocker["publication_blockers"] = ["todo"]

    bad_skeleton_missing_publication_blocker_class = skeleton_fixture()
    bad_skeleton_missing_publication_blocker_class["publication_blockers"] = [
        blocker
        for blocker in bad_skeleton_missing_publication_blocker_class["publication_blockers"]
        if "precision_requested_digits" not in blocker
    ]

    bad_skeleton_publication_blocker_missing_target = skeleton_fixture()
    bad_skeleton_publication_blocker_missing_target["publication_blockers"][3] = (
        "D2, D4, and D6 coefficient arrays are empty"
    )

    bad_skeleton_anti_fake_note = skeleton_fixture()
    bad_skeleton_anti_fake_note["anti_fake"]["notes"] = (
        "Skeleton only. This file establishes the evidence location."
    )

    bad_skeleton_digit_claim = skeleton_fixture()
    bad_skeleton_digit_claim["weights"][1]["reference_validation"][
        "minimum_digit_agreement"
    ] = 50

    bad_skeleton_comparison_claim = skeleton_fixture()
    bad_skeleton_comparison_claim["weights"][2]["reference_validation"][
        "comparison_artifact"
    ] = "artifacts/d6.compare.json"

    bad_skeleton_missing_comparison_slot = skeleton_fixture()
    del bad_skeleton_missing_comparison_slot["weights"][0]["reference_validation"][
        "comparison_artifact"
    ]

    bad_full_skeleton_status = full_fixture()
    bad_full_skeleton_status["status"] = "skeleton-pending-amflow-reference-run"

    bad_full_skeleton_anti_fake_note = full_fixture()
    bad_full_skeleton_anti_fake_note["anti_fake"]["notes"] = (
        "Skeleton only. This file publishes no AMFlow coefficient values and must not promote b63n."
    )

    bad_source_path = skeleton_fixture()
    bad_source_path["source_files"]["automatic_phasespace_run_wl"][
        "path"
    ] = "/example/amflow/examples/feynman_prescription/run.wl"

    bad_source_root_whitespace = skeleton_fixture()
    bad_source_root_whitespace["source_root"] = " /example/amflow"

    bad_source_path_parent_component = skeleton_fixture()
    bad_source_path_parent_component["source_files"]["amflow_m"][
        "path"
    ] = "/example/amflow/examples/../AMFlow.m"

    bad_full_blocked_reason = full_fixture()
    bad_full_blocked_reason["weights"][0]["reference_validation"][
        "blocked_reason"
    ] = "pending high-precision AMFlow extraction for D2"

    bad_full_missing_coefficients = full_fixture()
    bad_full_missing_coefficients["weights"][0]["coefficients"] = []

    bad_noncontiguous_eps = full_fixture()
    bad_noncontiguous_eps["weights"][1]["coefficients"] = [
        sample_coefficient("D4", 0),
        sample_coefficient("D4", 2),
    ]

    bad_truncated_eps = full_fixture()
    for weight in bad_truncated_eps["weights"]:
        denominator_id = weight["denominator_id"]
        weight["coefficients"] = [sample_coefficient(denominator_id, 0)]

    bad_low_digits = full_fixture()
    bad_low_digits["weights"][2]["coefficients"][0]["agreement_digits"] = 49

    bad_absolute_coefficient_source = full_fixture()
    bad_absolute_coefficient_source["weights"][0]["coefficients"][0][
        "source"
    ] = "/tmp/d2-eps0.json"

    bad_parent_coefficient_source = full_fixture()
    bad_parent_coefficient_source["weights"][0]["coefficients"][0][
        "source"
    ] = "artifacts/../d2-eps0.json"

    bad_cpp_result_coefficient_source = full_fixture()
    bad_cpp_result_coefficient_source["weights"][0]["coefficients"][0][
        "source"
    ] = (
        "tools/reference-harness/specs/m5/proof-runs/lane45/"
        "automatic_phasespace.cpp-result.json"
    )

    bad_stripped_result_coefficient_source = full_fixture()
    bad_stripped_result_coefficient_source["weights"][0]["coefficients"][0][
        "source"
    ] = (
        "tools/reference-harness/specs/m6/lane146/"
        "automatic_phasespace.selected4-cutkosky.stripped-result.json"
    )

    bad_compare_coefficient_source = full_fixture()
    bad_compare_coefficient_source["weights"][0]["coefficients"][0][
        "source"
    ] = (
        "tools/reference-harness/specs/m6/lane146/"
        "automatic_phasespace.selected4-cutkosky.compare30.json"
    )

    bad_suffixed_compare_coefficient_source = full_fixture()
    bad_suffixed_compare_coefficient_source["weights"][0]["coefficients"][0][
        "source"
    ] = (
        "tools/reference-harness/specs/m7/lane2/"
        "complex_kinematics.c267-stripped.eps0.compare50.reference-floor.json"
    )

    bad_amflow_state_coefficient_source = full_fixture()
    bad_amflow_state_coefficient_source["weights"][0]["coefficients"][0][
        "source"
    ] = "tools/reference-harness/specs/phase0/automatic_phasespace.amflow-state.json"

    bad_golden_manifest_coefficient_source = full_fixture()
    bad_golden_manifest_coefficient_source["weights"][0]["coefficients"][0][
        "source"
    ] = "tools/reference-harness/specs/phase0/automatic_phasespace.golden-manifest.json"

    bad_minimum_digit_overclaim = full_fixture()
    bad_minimum_digit_overclaim["weights"][0]["reference_validation"][
        "minimum_digit_agreement"
    ] = 61

    bad_fake_samples = full_fixture()
    bad_fake_samples["weights"][0]["reference_validation"][
        "final_solution_samples_used_as_input"
    ] = True

    bad_weight_metadata = full_fixture()
    bad_weight_metadata["weights"][1]["denominator_index"] = 4

    bad_raw_sha = full_fixture()
    bad_raw_sha["amflow_parameter_set"]["raw_output_sha256"] = "not-a-sha"

    bad_raw_sha_whitespace = full_fixture()
    bad_raw_sha_whitespace["amflow_parameter_set"]["raw_output_sha256"] = (
        " " + bad_raw_sha_whitespace["amflow_parameter_set"]["raw_output_sha256"] + " "
    )

    bad_cpp_runtime_run_command = full_fixture()
    bad_cpp_runtime_run_command["amflow_parameter_set"]["run_command"] = (
        "./build/amflow-cli solve-series "
        "tools/reference-harness/specs/phase0/automatic_phasespace.amflow-state.json "
        "--eps-order 4 --digits 80"
    )

    bad_generic_mathematica_run_command = full_fixture()
    bad_generic_mathematica_run_command["amflow_parameter_set"][
        "run_command"
    ] = "math -script unrelated_reference_run.wls"

    bad_absolute_run_log = full_fixture()
    bad_absolute_run_log["amflow_parameter_set"]["run_log"] = "/tmp/d246/run.log"

    bad_cpp_result_run_log = full_fixture()
    bad_cpp_result_run_log["amflow_parameter_set"]["run_log"] = (
        "tools/reference-harness/specs/m5/proof-runs/lane45/"
        "automatic_phasespace.cpp-result.json"
    )

    bad_compare_run_log = full_fixture()
    bad_compare_run_log["amflow_parameter_set"]["run_log"] = (
        "tools/reference-harness/specs/m6/lane146/"
        "automatic_phasespace.selected4-cutkosky.compare30.json"
    )

    bad_golden_manifest_run_log = full_fixture()
    bad_golden_manifest_run_log["amflow_parameter_set"]["run_log"] = (
        "tools/reference-harness/specs/phase0/automatic_phasespace.golden-manifest.json"
    )

    bad_parent_raw_output = full_fixture()
    bad_parent_raw_output["amflow_parameter_set"]["raw_output"] = (
        "artifacts/../d246/raw-output.json"
    )

    bad_cpp_result_raw_output = full_fixture()
    bad_cpp_result_raw_output["amflow_parameter_set"]["raw_output"] = (
        "tools/reference-harness/specs/m5/proof-runs/lane45/"
        "automatic_phasespace.cpp-result.json"
    )

    bad_compare_raw_output = full_fixture()
    bad_compare_raw_output["amflow_parameter_set"]["raw_output"] = (
        "tools/reference-harness/specs/m6/lane146/"
        "automatic_phasespace.selected4-cutkosky.compare30.json"
    )

    bad_golden_manifest_raw_output = full_fixture()
    bad_golden_manifest_raw_output["amflow_parameter_set"]["raw_output"] = (
        "tools/reference-harness/specs/phase0/automatic_phasespace.golden-manifest.json"
    )

    bad_absolute_comparison_artifact = full_fixture()
    bad_absolute_comparison_artifact["weights"][0]["reference_validation"][
        "comparison_artifact"
    ] = "/tmp/d2.compare.json"

    bad_noncompare_comparison_artifact = full_fixture()
    bad_noncompare_comparison_artifact["weights"][0]["reference_validation"][
        "comparison_artifact"
    ] = "artifacts/d2-validation.json"

    bad_cpp_result_comparison_artifact = full_fixture()
    bad_cpp_result_comparison_artifact["weights"][0]["reference_validation"][
        "comparison_artifact"
    ] = (
        "tools/reference-harness/specs/m5/proof-runs/lane45/"
        "automatic_phasespace.cpp-result.json"
    )

    checks = {
        "skeleton_valid_but_not_published": (
            skeleton_summary["schema_valid"] is True
            and skeleton_summary["published_evidence"] is False
            and skeleton_summary["skeleton_evidence"] is True
        ),
        "full_evidence_valid_and_published": (
            full_summary["schema_valid"] is True
            and full_summary["published_evidence"] is True
            and full_summary["minimum_digit_agreement"] == 60
        ),
        "skeleton_cannot_pass_publication": rejected(
            bad_skeleton_passed,
            "skeleton evidence cannot also pass publication",
        ),
        "unpublished_evidence_must_be_explicit_skeleton": rejected(
            bad_unclassified_unpublished,
            "must be either published evidence or an explicit skeleton",
        ),
        "skeleton_rejects_placeholder_blocker": rejected(
            bad_skeleton_placeholder_blocker,
            "blocked_reason must not be a placeholder",
        ),
        "skeleton_rejects_blocker_without_full_target": rejected(
            bad_skeleton_blocker_missing_target,
            "blocked_reason must contain",
        ),
        "skeleton_rejects_required_scope_drift": rejected(
            bad_skeleton_scope,
            "required_coefficient_scope",
        ),
        "skeleton_rejects_precision_claim": rejected(
            bad_skeleton_precision_claim,
            "amflow_parameter_set.precision_requested_digits must be null",
        ),
        "skeleton_rejects_run_artifact_claim": rejected(
            bad_skeleton_run_artifact_claim,
            "amflow_parameter_set.raw_output_sha256 must be null",
        ),
        "skeleton_requires_explicit_artifact_null_slot": rejected(
            bad_skeleton_missing_artifact_slot,
            "amflow_parameter_set.raw_output must be explicitly null",
        ),
        "skeleton_rejects_placeholder_publication_blocker": rejected(
            bad_skeleton_placeholder_publication_blocker,
            "publication_blockers[0] must not be a placeholder",
        ),
        "skeleton_rejects_missing_publication_blocker_class": rejected(
            bad_skeleton_missing_publication_blocker_class,
            "publication_blockers must include precision request blocker",
        ),
        "skeleton_rejects_publication_blocker_without_full_target": rejected(
            bad_skeleton_publication_blocker_missing_target,
            "publication_blockers must include empty coefficient blocker",
        ),
        "skeleton_rejects_anti_fake_note_without_nonclaims": rejected(
            bad_skeleton_anti_fake_note,
            "anti_fake.notes must contain",
        ),
        "skeleton_rejects_digit_agreement_claim": rejected(
            bad_skeleton_digit_claim,
            "minimum_digit_agreement must be null",
        ),
        "skeleton_rejects_comparison_artifact_claim": rejected(
            bad_skeleton_comparison_claim,
            "comparison_artifact must be null",
        ),
        "skeleton_requires_explicit_comparison_null_slot": rejected(
            bad_skeleton_missing_comparison_slot,
            "weights[0].reference_validation.comparison_artifact must be explicitly null",
        ),
        "published_rejects_skeleton_status": rejected(
            bad_full_skeleton_status,
            "published evidence status must not retain skeleton or pending text",
        ),
        "published_rejects_skeleton_anti_fake_note": rejected(
            bad_full_skeleton_anti_fake_note,
            "published anti_fake.notes must not retain skeleton non-claim text",
        ),
        "rejects_wrong_source_file_path": rejected(
            bad_source_path,
            "source_files.automatic_phasespace_run_wl.path",
        ),
        "rejects_source_root_surrounding_whitespace": rejected(
            bad_source_root_whitespace,
            "source_root must not contain surrounding whitespace",
        ),
        "rejects_source_path_parent_component": rejected(
            bad_source_path_parent_component,
            "source_files.amflow_m.path must not contain . or .. components",
        ),
        "published_rejects_weight_blocked_reason": rejected(
            bad_full_blocked_reason,
            "published validation must not retain blocked_reason",
        ),
        "full_requires_nonempty_coefficients": rejected(
            bad_full_missing_coefficients,
            "must not be empty for published evidence",
        ),
        "full_requires_contiguous_eps_scope": rejected(
            bad_noncontiguous_eps,
            "contiguous eps_order range",
        ),
        "full_rejects_truncated_eps_scope": rejected(
            bad_truncated_eps,
            "eps^0..eps^3 coefficient scope",
        ),
        "full_rejects_low_digit_agreement": rejected(
            bad_low_digits,
            "agreement_digits must be at least 50",
        ),
        "full_rejects_absolute_coefficient_source": rejected(
            bad_absolute_coefficient_source,
            "source must be a relative artifact path",
        ),
        "full_rejects_parent_coefficient_source": rejected(
            bad_parent_coefficient_source,
            "source must not contain . or .. components",
        ),
        "full_rejects_cpp_result_coefficient_source": rejected(
            bad_cpp_result_coefficient_source,
            "retained runtime/comparison artifacts",
        ),
        "full_rejects_stripped_result_coefficient_source": rejected(
            bad_stripped_result_coefficient_source,
            "retained runtime/comparison artifacts",
        ),
        "full_rejects_compare_coefficient_source": rejected(
            bad_compare_coefficient_source,
            "retained runtime/comparison artifacts",
        ),
        "full_rejects_suffixed_compare_coefficient_source": rejected(
            bad_suffixed_compare_coefficient_source,
            "retained runtime/comparison artifacts",
        ),
        "full_rejects_amflow_state_coefficient_source": rejected(
            bad_amflow_state_coefficient_source,
            "retained runtime/comparison artifacts",
        ),
        "full_rejects_golden_manifest_coefficient_source": rejected(
            bad_golden_manifest_coefficient_source,
            "retained runtime/comparison artifacts",
        ),
        "full_rejects_minimum_digit_overclaim": rejected(
            bad_minimum_digit_overclaim,
            "minimum_digit_agreement must not exceed the coefficient agreement floor",
        ),
        "full_rejects_final_solution_sample_input": rejected(
            bad_fake_samples,
            "must not use final solution samples as input",
        ),
        "full_rejects_weight_metadata_drift": rejected(
            bad_weight_metadata,
            "weights[1].denominator_index",
        ),
        "full_rejects_bad_raw_sha": rejected(
            bad_raw_sha,
            "raw_output_sha256 must be a lowercase sha256",
        ),
        "full_rejects_raw_sha_surrounding_whitespace": rejected(
            bad_raw_sha_whitespace,
            "raw_output_sha256 must not contain surrounding whitespace",
        ),
        "full_rejects_cpp_runtime_run_command": rejected(
            bad_cpp_runtime_run_command,
            "run_command must not invoke C++ runtime",
        ),
        "full_rejects_generic_mathematica_run_command": rejected(
            bad_generic_mathematica_run_command,
            "run_command must identify the D2/D4/D6 extraction scope",
        ),
        "full_rejects_absolute_run_log": rejected(
            bad_absolute_run_log,
            "run_log must be a relative artifact path",
        ),
        "full_rejects_cpp_result_run_log": rejected(
            bad_cpp_result_run_log,
            "AMFlow run log",
        ),
        "full_rejects_compare_run_log": rejected(
            bad_compare_run_log,
            "AMFlow run log",
        ),
        "full_rejects_golden_manifest_run_log": rejected(
            bad_golden_manifest_run_log,
            "AMFlow run log",
        ),
        "full_rejects_parent_raw_output": rejected(
            bad_parent_raw_output,
            "raw_output must not contain . or .. components",
        ),
        "full_rejects_cpp_result_raw_output": rejected(
            bad_cpp_result_raw_output,
            "raw AMFlow output",
        ),
        "full_rejects_compare_raw_output": rejected(
            bad_compare_raw_output,
            "raw AMFlow output",
        ),
        "full_rejects_golden_manifest_raw_output": rejected(
            bad_golden_manifest_raw_output,
            "raw AMFlow output",
        ),
        "full_rejects_absolute_comparison_artifact": rejected(
            bad_absolute_comparison_artifact,
            "comparison_artifact must be a relative artifact path",
        ),
        "full_rejects_noncompare_comparison_artifact": rejected(
            bad_noncompare_comparison_artifact,
            "comparison artifact named *.compare*.json",
        ),
        "full_rejects_cpp_result_comparison_artifact": rejected(
            bad_cpp_result_comparison_artifact,
            "comparison artifact named *.compare*.json",
        ),
    }
    expect(all(checks.values()), "b63n D246 evidence verifier self-check failed")
    return {
        "schema_version": 1,
        "verifier": "b63n-d246-weighted-residue-evidence-v1",
        "self_check_passed": True,
        "checks": checks,
    }


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--sidecar-path",
        type=Path,
        default=repo_root() / DEFAULT_SIDECAR,
        help="D246 evidence sidecar JSON path",
    )
    parser.add_argument("--summary-path", type=Path, help="Optional JSON output path")
    parser.add_argument(
        "--require-published",
        action="store_true",
        help="Fail unless the sidecar is full published evidence",
    )
    parser.add_argument("--self-check", action="store_true", help="Run synthetic verifier checks")
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    try:
        if args.self_check:
            summary = run_self_check()
        else:
            summary = verify_sidecar(args.sidecar_path)
            if args.require_published and not summary["published_evidence"]:
                summary["schema_valid"] = False
                summary["blocking_reasons"] = [
                    "sidecar is schema-valid but does not contain published D2/D4/D6 evidence"
                ]
        if args.summary_path is not None:
            write_json(args.summary_path, summary)
        print_json(summary)
        return 0 if summary.get("schema_valid", True) else 1
    except Exception as error:  # noqa: BLE001 - command-line verifier should fail closed.
        summary = {
            "schema_version": 1,
            "verifier": "b63n-d246-weighted-residue-evidence-v1",
            "schema_valid": False,
            "published_evidence": False,
            "skeleton_evidence": False,
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
