#!/usr/bin/env python3
"""Fail-closed M5 feature-surface verdict composer."""

from __future__ import annotations

import argparse
import json
import tempfile
from pathlib import Path
from typing import Any

from freeze_phase0_goldens import load_json


REQUIRED_EXAMPLE_CLASSES = [
    "automatic_loop",
    "automatic_phasespace",
    "automatic_vs_manual",
    "complex_kinematics",
    "differential_equation_solver",
    "feynman_prescription",
    "linear_propagator",
    "spacetime_dimension",
    "user_defined_amfmode",
    "user_defined_ending",
]

REQUIRED_RUNTIME_FEATURES = [
    "arbitrary_D0",
    "fixed_eps",
    "complex_kinematics",
    "linear_propagators",
    "phase_space_integration",
    "feynman_prescription",
    "singular_kinematics_guardrails",
    "user_defined_hooks",
]

ACCEPTED_EVIDENCE_KINDS = {
    "live-solver-path",
    "approved-retained-state-exception",
}

M5_FEATURE_PARITY_SCOPE = "m5-feature-parity-only"
M5_FEATURE_PARITY_PASSED_STATE = "m5-feature-parity-passed"
M5_FEATURE_PARITY_BLOCKED_STATE = "blocked-on-m5-feature-parity"
M5_ALL_PHASE_DECISION_SCOPE = "m5-all-phase-closure-decision"
M5_ALL_PHASE_SCOPE = "m5-all-phase"
M5_ALL_PHASE_CLOSED_STATE = "CLOSED/all-phase"
M5_ALL_PHASE_BLOCKED_STATE = "blocked-on-m5-all-phase-closure"
M5_ALL_PHASE_DECISION = "accept-m5-feature-parity-as-all-phase-closure"

FEATURE_PARITY_INTEGRITY_FIELDS = [
    "scope",
    "current_state",
    "m5_feature_parity_passed",
    "does_not_claim_m6",
    "does_not_claim_m7",
    "does_not_claim_release_readiness",
    "required_tolerance_digits",
    "m0b_accepted",
    "required_example_classes",
    "required_runtime_features",
    "missing_example_classes",
    "unknown_example_classes",
    "missing_runtime_features",
    "unknown_runtime_features",
    "aggregate_compared_coefficient_count",
    "aggregate_passed_coefficient_count",
    "blocking_reasons",
    "example_summaries",
    "runtime_feature_summaries",
]


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def write_json(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def normalize_string(raw: Any, label: str) -> str:
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
    if raw is None:
        return []
    if not isinstance(raw, list):
        raise TypeError(f"{label} must be a list")
    values: list[str] = []
    for item in raw:
        if not isinstance(item, str):
            raise TypeError(f"{label} entries must be strings")
        value = item.strip()
        expect(value, f"{label} entries must not be empty")
        values.append(value)
    return values


def normalize_nonnegative_int(raw: Any, label: str) -> int:
    if not isinstance(raw, int):
        raise TypeError(f"{label} must be an int")
    expect(raw >= 0, f"{label} must be nonnegative")
    return raw


def resolve_path(raw: Any, base_dir: Path, label: str) -> Path:
    value = normalize_string(raw, label)
    path = Path(value)
    if not path.is_absolute():
        path = base_dir / path
    return path


def normalize_entries(raw: Any, label: str) -> dict[str, dict[str, Any]]:
    if not isinstance(raw, list):
        raise TypeError(f"{label} must be a list")
    entries: dict[str, dict[str, Any]] = {}
    for entry in raw:
        if not isinstance(entry, dict):
            raise TypeError(f"{label} entries must be objects")
        entry_id = normalize_string(entry.get("id"), f"{label} id")
        if entry_id in entries:
            raise ValueError(f"duplicate {label} id: {entry_id}")
        entries[entry_id] = entry
    return entries


def load_comparison_summary(
    *,
    comparison_path: Path,
    required_tolerance_digits: int,
) -> tuple[dict[str, Any], list[str]]:
    blockers: list[str] = []
    if not comparison_path.exists():
        return {}, [f"comparison summary is missing: {comparison_path}"]

    summary = load_json(comparison_path)
    passed = summary.get("passed")
    if not isinstance(passed, bool):
        blockers.append("comparison summary field passed must be a bool")
        passed = False

    compared_count = normalize_nonnegative_int(
        summary.get("compared_coefficient_count"),
        "comparison compared_coefficient_count",
    )
    passed_count = normalize_nonnegative_int(
        summary.get("passed_coefficient_count"),
        "comparison passed_coefficient_count",
    )
    tolerance_digits = normalize_nonnegative_int(
        summary.get("tolerance_digits"),
        "comparison tolerance_digits",
    )
    minimum_digit_agreement = summary.get("minimum_digit_agreement")
    if minimum_digit_agreement is not None:
        normalize_nonnegative_int(minimum_digit_agreement, "comparison minimum_digit_agreement")

    raw_failures = summary.get("failures", [])
    if raw_failures is None:
        raw_failures = []
    if not isinstance(raw_failures, list):
        raise TypeError("comparison failures must be a list when present")
    failure_count = len(raw_failures)

    if not passed:
        blockers.append("comparison did not pass")
    if compared_count <= 0:
        blockers.append("comparison is not coefficient-bearing")
    if passed_count != compared_count:
        blockers.append("not every compared coefficient passed")
    if tolerance_digits != required_tolerance_digits:
        blockers.append(
            f"comparison tolerance_digits {tolerance_digits} != {required_tolerance_digits}"
        )
    if failure_count:
        blockers.append(f"comparison reports {failure_count} failures")

    return (
        {
            "comparison_summary": str(comparison_path),
            "comparison_passed": bool(passed),
            "compared_coefficient_count": compared_count,
            "passed_coefficient_count": passed_count,
            "minimum_digit_agreement": minimum_digit_agreement,
            "tolerance_digits": tolerance_digits,
            "failure_count": failure_count,
        },
        blockers,
    )


def evaluate_example_entry(
    *,
    example_id: str,
    entry: dict[str, Any],
    base_dir: Path,
    required_tolerance_digits: int,
) -> tuple[dict[str, Any], list[str]]:
    blockers = normalize_string_list(entry.get("blockers", []), f"{example_id} blockers")
    promoted_golden = normalize_bool(
        entry.get("promoted_golden", False),
        f"{example_id} promoted_golden",
    )
    coefficient_bearing = normalize_bool(
        entry.get("coefficient_bearing", False),
        f"{example_id} coefficient_bearing",
    )
    evidence_kind = normalize_string(entry.get("evidence_kind", ""), f"{example_id} evidence_kind")
    exception_reference = str(entry.get("exception_reference", "")).strip()

    if not promoted_golden:
        blockers.append("promoted retained AMFlow golden is not confirmed")
    if not coefficient_bearing:
        blockers.append("C++ result surface is not marked coefficient-bearing")
    if evidence_kind not in ACCEPTED_EVIDENCE_KINDS:
        blockers.append(f"evidence kind {evidence_kind!r} is not accepted for M5 closure")
    if evidence_kind == "approved-retained-state-exception" and not exception_reference:
        blockers.append("retained-state exception lacks an explicit review reference")

    comparison_info: dict[str, Any] = {}
    comparison_blockers: list[str] = []
    raw_comparison_path = entry.get("comparison_summary")
    if raw_comparison_path is None:
        comparison_blockers.append("comparison_summary is required")
    else:
        comparison_path = resolve_path(
            raw_comparison_path,
            base_dir,
            f"{example_id} comparison_summary",
        )
        comparison_info, comparison_blockers = load_comparison_summary(
            comparison_path=comparison_path,
            required_tolerance_digits=required_tolerance_digits,
        )
    blockers.extend(comparison_blockers)

    return (
        {
            "id": example_id,
            "accepted": not blockers,
            "evidence_kind": evidence_kind,
            "promoted_golden": promoted_golden,
            "coefficient_bearing": coefficient_bearing,
            "exception_reference": exception_reference,
            "blockers": blockers,
            **comparison_info,
        },
        blockers,
    )


def evaluate_runtime_feature_entry(
    *,
    feature_id: str,
    entry: dict[str, Any],
) -> tuple[dict[str, Any], list[str]]:
    blockers = normalize_string_list(entry.get("blockers", []), f"{feature_id} blockers")
    evidence_kind = normalize_string(entry.get("evidence_kind", ""), f"{feature_id} evidence_kind")
    exception_reference = str(entry.get("exception_reference", "")).strip()
    evidence_path = str(entry.get("evidence_path", "")).strip()

    if evidence_kind not in ACCEPTED_EVIDENCE_KINDS:
        blockers.append(f"evidence kind {evidence_kind!r} is not accepted for M5 closure")
    if evidence_kind == "approved-retained-state-exception" and not exception_reference:
        blockers.append("retained-state exception lacks an explicit review reference")
    if not evidence_path and not exception_reference:
        blockers.append("feature row lacks evidence_path or exception_reference")

    return (
        {
            "id": feature_id,
            "accepted": not blockers,
            "evidence_kind": evidence_kind,
            "evidence_path": evidence_path,
            "exception_reference": exception_reference,
            "blockers": blockers,
        },
        blockers,
    )


def qualify_milestone_m5(
    *,
    evidence_summary_path: Path,
    required_tolerance_digits: int,
) -> dict[str, Any]:
    evidence_summary = load_json(evidence_summary_path)
    expect(evidence_summary.get("schema_version") == 1, "M5 evidence schema_version must be 1")
    expect(
        evidence_summary.get("scope") == "m5-feature-surface",
        "M5 evidence scope must be m5-feature-surface",
    )

    base_dir = evidence_summary_path.parent
    m0b_accepted = normalize_bool(evidence_summary.get("m0b_accepted"), "m0b_accepted")
    example_entries = normalize_entries(
        evidence_summary.get("examples", []),
        "M5 examples",
    )
    runtime_feature_entries = normalize_entries(
        evidence_summary.get("runtime_features", []),
        "M5 runtime_features",
    )

    blockers: list[str] = []
    if not m0b_accepted:
        blockers.append("Milestone M0b is not accepted")

    missing_examples = sorted(set(REQUIRED_EXAMPLE_CLASSES) - set(example_entries))
    unknown_examples = sorted(set(example_entries) - set(REQUIRED_EXAMPLE_CLASSES))
    missing_runtime_features = sorted(set(REQUIRED_RUNTIME_FEATURES) - set(runtime_feature_entries))
    unknown_runtime_features = sorted(
        set(runtime_feature_entries) - set(REQUIRED_RUNTIME_FEATURES)
    )
    for example_id in missing_examples:
        blockers.append(f"missing required frozen example class: {example_id}")
    for example_id in unknown_examples:
        blockers.append(f"unknown frozen example class in evidence sidecar: {example_id}")
    for feature_id in missing_runtime_features:
        blockers.append(f"missing required Phase F runtime feature: {feature_id}")
    for feature_id in unknown_runtime_features:
        blockers.append(f"unknown Phase F runtime feature in evidence sidecar: {feature_id}")

    example_summaries: list[dict[str, Any]] = []
    runtime_feature_summaries: list[dict[str, Any]] = []
    total_compared = 0
    total_passed = 0

    for example_id in REQUIRED_EXAMPLE_CLASSES:
        entry = example_entries.get(example_id)
        if entry is None:
            continue
        summary, entry_blockers = evaluate_example_entry(
            example_id=example_id,
            entry=entry,
            base_dir=base_dir,
            required_tolerance_digits=required_tolerance_digits,
        )
        blockers.extend(f"{example_id}: {blocker}" for blocker in entry_blockers)
        total_compared += int(summary.get("compared_coefficient_count", 0))
        total_passed += int(summary.get("passed_coefficient_count", 0))
        example_summaries.append(summary)

    for feature_id in REQUIRED_RUNTIME_FEATURES:
        entry = runtime_feature_entries.get(feature_id)
        if entry is None:
            continue
        summary, entry_blockers = evaluate_runtime_feature_entry(
            feature_id=feature_id,
            entry=entry,
        )
        blockers.extend(f"{feature_id}: {blocker}" for blocker in entry_blockers)
        runtime_feature_summaries.append(summary)

    m5_passed = not blockers
    return {
        "schema_version": 1,
        "scope": M5_FEATURE_PARITY_SCOPE,
        "current_state": M5_FEATURE_PARITY_PASSED_STATE
        if m5_passed
        else M5_FEATURE_PARITY_BLOCKED_STATE,
        "m5_feature_parity_passed": m5_passed,
        "does_not_claim_m6": True,
        "does_not_claim_m7": True,
        "does_not_claim_release_readiness": True,
        "required_tolerance_digits": required_tolerance_digits,
        "m0b_accepted": m0b_accepted,
        "required_example_classes": REQUIRED_EXAMPLE_CLASSES,
        "required_runtime_features": REQUIRED_RUNTIME_FEATURES,
        "missing_example_classes": missing_examples,
        "unknown_example_classes": unknown_examples,
        "missing_runtime_features": missing_runtime_features,
        "unknown_runtime_features": unknown_runtime_features,
        "aggregate_compared_coefficient_count": total_compared,
        "aggregate_passed_coefficient_count": total_passed,
        "blocking_reasons": blockers,
        "example_summaries": example_summaries,
        "runtime_feature_summaries": runtime_feature_summaries,
    }


def load_committed_feature_parity_summary(path_text: str) -> dict[str, Any] | None:
    summary_path = Path(path_text)
    if not summary_path.is_absolute():
        summary_path = Path.cwd() / summary_path
    if not summary_path.exists():
        return None
    return load_json(summary_path)


def qualify_milestone_m5_all_phase(
    *,
    feature_summary: dict[str, Any],
    evidence_summary_path: Path,
    closure_decision_path: Path,
    required_tolerance_digits: int,
) -> dict[str, Any]:
    decision = load_json(closure_decision_path)
    blockers: list[str] = []

    schema_version = decision.get("schema_version")
    if not isinstance(schema_version, int):
        raise TypeError("M5 all-phase closure decision schema_version must be an int")
    if schema_version != 1:
        blockers.append(f"closure decision schema_version {schema_version} != 1")

    decision_scope = normalize_string(decision.get("scope"), "M5 closure decision scope")
    if decision_scope != M5_ALL_PHASE_DECISION_SCOPE:
        blockers.append(
            f"closure decision scope {decision_scope!r} != {M5_ALL_PHASE_DECISION_SCOPE!r}"
        )

    milestone = normalize_string(decision.get("milestone"), "M5 closure decision milestone")
    if milestone != "M5":
        blockers.append(f"closure decision milestone {milestone!r} != 'M5'")

    decision_id = normalize_string(decision.get("decision"), "M5 closure decision")
    if decision_id != M5_ALL_PHASE_DECISION:
        blockers.append(f"closure decision {decision_id!r} != {M5_ALL_PHASE_DECISION!r}")

    approved_state = normalize_string(
        decision.get("approved_current_state"),
        "M5 closure decision approved_current_state",
    )
    if approved_state != M5_ALL_PHASE_CLOSED_STATE:
        blockers.append(
            f"closure decision approved_current_state {approved_state!r} "
            f"!= {M5_ALL_PHASE_CLOSED_STATE!r}"
        )

    closure_approved = normalize_bool(
        decision.get("m5_all_phase_closure_approved", False),
        "M5 closure decision m5_all_phase_closure_approved",
    )
    if not closure_approved:
        blockers.append("closure decision does not approve M5 all-phase closure")

    decision_blockers = normalize_string_list(
        decision.get("blockers", []),
        "M5 closure decision blockers",
    )
    blockers.extend(f"closure-decision: {blocker}" for blocker in decision_blockers)

    evidence_paths = normalize_string_list(
        decision.get("evidence_paths", []),
        "M5 closure decision evidence_paths",
    )
    feature_parity_summary_path = normalize_string(
        decision.get("m5_feature_parity_summary_path"),
        "M5 closure decision m5_feature_parity_summary_path",
    )
    feature_surface_evidence_path = normalize_string(
        decision.get("m5_feature_surface_evidence_path"),
        "M5 closure decision m5_feature_surface_evidence_path",
    )
    expected_evidence_path = str(evidence_summary_path)
    if feature_surface_evidence_path != expected_evidence_path:
        blockers.append(
            "closure decision m5_feature_surface_evidence_path does not match "
            f"the evaluated evidence summary: {feature_surface_evidence_path!r} "
            f"!= {expected_evidence_path!r}"
        )
    if expected_evidence_path not in evidence_paths:
        blockers.append("closure decision evidence_paths does not cite the evaluated evidence")
    if feature_parity_summary_path not in evidence_paths:
        blockers.append("closure decision evidence_paths does not cite the feature-parity summary")

    decision_does_not_claim_m6 = normalize_bool(
        decision.get("does_not_claim_m6", False),
        "M5 closure decision does_not_claim_m6",
    )
    decision_does_not_claim_m7 = normalize_bool(
        decision.get("does_not_claim_m7", False),
        "M5 closure decision does_not_claim_m7",
    )
    decision_does_not_claim_release = normalize_bool(
        decision.get("does_not_claim_release_readiness", False),
        "M5 closure decision does_not_claim_release_readiness",
    )
    if not decision_does_not_claim_m6:
        blockers.append("closure decision must not claim Milestone M6")
    if not decision_does_not_claim_m7:
        blockers.append("closure decision must not claim Milestone M7")
    if not decision_does_not_claim_release:
        blockers.append("closure decision must not claim release readiness")

    committed_feature_summary = load_committed_feature_parity_summary(feature_parity_summary_path)
    if committed_feature_summary is None:
        blockers.append(f"committed feature-parity summary is missing: {feature_parity_summary_path}")
    else:
        for field in FEATURE_PARITY_INTEGRITY_FIELDS:
            if committed_feature_summary.get(field) != feature_summary.get(field):
                blockers.append(f"committed feature-parity summary field differs: {field}")

    feature_scope = normalize_string(feature_summary.get("scope"), "M5 feature summary scope")
    feature_current_state = normalize_string(
        feature_summary.get("current_state"),
        "M5 feature summary current_state",
    )
    feature_passed = normalize_bool(
        feature_summary.get("m5_feature_parity_passed"),
        "M5 feature summary m5_feature_parity_passed",
    )
    feature_blockers = normalize_string_list(
        feature_summary.get("blocking_reasons", []),
        "M5 feature summary blocking_reasons",
    )
    does_not_claim_m6 = normalize_bool(
        feature_summary.get("does_not_claim_m6"),
        "M5 feature summary does_not_claim_m6",
    )
    does_not_claim_m7 = normalize_bool(
        feature_summary.get("does_not_claim_m7"),
        "M5 feature summary does_not_claim_m7",
    )
    does_not_claim_release = normalize_bool(
        feature_summary.get("does_not_claim_release_readiness"),
        "M5 feature summary does_not_claim_release_readiness",
    )

    aggregate_compared = normalize_nonnegative_int(
        feature_summary.get("aggregate_compared_coefficient_count"),
        "M5 feature summary aggregate_compared_coefficient_count",
    )
    aggregate_passed = normalize_nonnegative_int(
        feature_summary.get("aggregate_passed_coefficient_count"),
        "M5 feature summary aggregate_passed_coefficient_count",
    )

    if feature_scope != M5_FEATURE_PARITY_SCOPE:
        blockers.append(f"feature-parity scope {feature_scope!r} != {M5_FEATURE_PARITY_SCOPE!r}")
    if feature_current_state != M5_FEATURE_PARITY_PASSED_STATE:
        blockers.append(
            f"feature-parity current_state {feature_current_state!r} "
            f"!= {M5_FEATURE_PARITY_PASSED_STATE!r}"
        )
    if not feature_passed:
        blockers.append("feature-parity result is not passing")
    if feature_blockers:
        blockers.extend(f"feature-parity: {blocker}" for blocker in feature_blockers)
    if not does_not_claim_m6:
        blockers.append("feature-parity summary must not claim Milestone M6")
    if not does_not_claim_m7:
        blockers.append("feature-parity summary must not claim Milestone M7")
    if not does_not_claim_release:
        blockers.append("feature-parity summary must not claim release readiness")
    if aggregate_compared <= 0:
        blockers.append("feature-parity summary is not coefficient-bearing")
    if aggregate_passed != aggregate_compared:
        blockers.append("feature-parity summary has unpassed compared coefficients")
    if required_tolerance_digits != feature_summary.get("required_tolerance_digits"):
        blockers.append("feature-parity summary required tolerance does not match this run")
    for field in (
        "missing_example_classes",
        "unknown_example_classes",
        "missing_runtime_features",
        "unknown_runtime_features",
    ):
        if feature_summary.get(field):
            blockers.append(f"feature-parity summary has nonempty {field}")

    m5_closed = not blockers
    withheld_claims = [
        "This summary does not launch the C++ runtime or create new retained captures.",
        "This summary does not claim Milestone M6, Milestone M7, or release readiness.",
    ]
    if not m5_closed:
        withheld_claims.insert(0, "This summary does not claim Milestone M5 closure.")

    return {
        "schema_version": 1,
        "scope": M5_ALL_PHASE_SCOPE,
        "current_state": M5_ALL_PHASE_CLOSED_STATE
        if m5_closed
        else M5_ALL_PHASE_BLOCKED_STATE,
        "m5_all_phase_closed": m5_closed,
        "m5_feature_parity_passed": feature_passed,
        "m5_feature_parity_scope": feature_scope,
        "m5_feature_parity_current_state": feature_current_state,
        "m5_feature_parity_summary_path": feature_parity_summary_path,
        "m5_feature_surface_evidence_path": feature_surface_evidence_path,
        "m5_all_phase_closure_decision_path": str(closure_decision_path),
        "m5_all_phase_closure_decision": decision_id,
        "m5_all_phase_closure_approved": closure_approved,
        "approved_current_state": approved_state,
        "does_not_claim_m6": does_not_claim_m6 and decision_does_not_claim_m6,
        "does_not_claim_m7": does_not_claim_m7 and decision_does_not_claim_m7,
        "does_not_claim_release_readiness": does_not_claim_release
        and decision_does_not_claim_release,
        "required_tolerance_digits": required_tolerance_digits,
        "m0b_accepted": feature_summary.get("m0b_accepted"),
        "required_example_classes": feature_summary.get("required_example_classes", []),
        "required_runtime_features": feature_summary.get("required_runtime_features", []),
        "missing_example_classes": feature_summary.get("missing_example_classes", []),
        "unknown_example_classes": feature_summary.get("unknown_example_classes", []),
        "missing_runtime_features": feature_summary.get("missing_runtime_features", []),
        "unknown_runtime_features": feature_summary.get("unknown_runtime_features", []),
        "aggregate_compared_coefficient_count": aggregate_compared,
        "aggregate_passed_coefficient_count": aggregate_passed,
        "feature_parity_blocking_reasons": feature_blockers,
        "closure_decision_blockers": decision_blockers,
        "blocking_reasons": blockers,
        "withheld_claims": withheld_claims,
        "evidence_paths": evidence_paths,
        "example_summaries": feature_summary.get("example_summaries", []),
        "runtime_feature_summaries": feature_summary.get("runtime_feature_summaries", []),
    }


def self_check_comparison(path: Path, *, passed: bool = True) -> None:
    payload = {
        "schema_version": 1,
        "passed": passed,
        "matched_integral_count": 1,
        "compared_coefficient_count": 1,
        "passed_coefficient_count": 1 if passed else 0,
        "minimum_digit_agreement": 30 if passed else 0,
        "tolerance_digits": 30,
        "failures": [] if passed else [{"integral": "toy[1]", "epsilon_order": 0}],
    }
    write_json(path, payload)


def self_check_evidence(
    path: Path,
    *,
    comparison_path: Path,
    evidence_kind: str = "live-solver-path",
    omit_runtime_feature: str = "",
    failed_example: str = "",
) -> None:
    examples: list[dict[str, Any]] = []
    for example_id in REQUIRED_EXAMPLE_CLASSES:
        examples.append(
            {
                "id": example_id,
                "promoted_golden": True,
                "coefficient_bearing": True,
                "evidence_kind": "metadata-only" if example_id == failed_example else evidence_kind,
                "exception_reference": "docs/milestones/m5-m6-closure-plan.md"
                if evidence_kind == "approved-retained-state-exception"
                else "",
                "comparison_summary": str(comparison_path),
                "blockers": [],
            }
        )

    runtime_features: list[dict[str, Any]] = []
    for feature_id in REQUIRED_RUNTIME_FEATURES:
        if feature_id == omit_runtime_feature:
            continue
        runtime_features.append(
            {
                "id": feature_id,
                "evidence_kind": evidence_kind,
                "evidence_path": "docs/milestones/m5-m6-closure-plan.md",
                "exception_reference": "docs/milestones/m5-m6-closure-plan.md"
                if evidence_kind == "approved-retained-state-exception"
                else "",
                "blockers": [],
            }
        )

    write_json(
        path,
        {
            "schema_version": 1,
            "scope": "m5-feature-surface",
            "m0b_accepted": True,
            "examples": examples,
            "runtime_features": runtime_features,
        },
    )


def self_check_closure_decision(
    path: Path,
    *,
    evidence_path: Path,
    feature_summary_path: Path,
    approved: bool = True,
    blockers: list[str] | None = None,
) -> None:
    write_json(
        path,
        {
            "schema_version": 1,
            "scope": M5_ALL_PHASE_DECISION_SCOPE,
            "milestone": "M5",
            "decision": M5_ALL_PHASE_DECISION,
            "approved_current_state": M5_ALL_PHASE_CLOSED_STATE,
            "m5_all_phase_closure_approved": approved,
            "m5_feature_parity_summary_path": str(feature_summary_path),
            "m5_feature_surface_evidence_path": str(evidence_path),
            "evidence_paths": [str(feature_summary_path), str(evidence_path)],
            "blockers": blockers or [],
            "does_not_claim_m6": True,
            "does_not_claim_m7": True,
            "does_not_claim_release_readiness": True,
        },
    )


def run_self_check() -> None:
    with tempfile.TemporaryDirectory(prefix="amflow-m5-qualification-self-check-") as tmp:
        root = Path(tmp)
        passing_compare = root / "passing.compare.json"
        failing_compare = root / "failing.compare.json"
        passing_evidence = root / "passing-evidence.json"
        exception_evidence = root / "exception-evidence.json"
        blocked_evidence = root / "blocked-evidence.json"
        passing_summary_path = root / "passing-feature-summary.json"
        blocked_summary_path = root / "blocked-feature-summary.json"
        passing_closure_decision = root / "passing-closure-decision.json"
        withheld_closure_decision = root / "withheld-closure-decision.json"
        blocked_feature_closure_decision = root / "blocked-feature-closure-decision.json"

        self_check_comparison(passing_compare, passed=True)
        self_check_comparison(failing_compare, passed=False)

        self_check_evidence(passing_evidence, comparison_path=passing_compare)
        passing_summary = qualify_milestone_m5(
            evidence_summary_path=passing_evidence,
            required_tolerance_digits=30,
        )
        write_json(passing_summary_path, passing_summary)
        expect(passing_summary["m5_feature_parity_passed"], "passing M5 self-check should pass")
        expect(
            passing_summary["scope"] == M5_FEATURE_PARITY_SCOPE
            and passing_summary["current_state"] == M5_FEATURE_PARITY_PASSED_STATE,
            "legacy M5 self-check should remain feature-parity-only",
        )
        expect(
            passing_summary["does_not_claim_m6"]
            and passing_summary["does_not_claim_m7"]
            and passing_summary["does_not_claim_release_readiness"],
            "M5 self-check must preserve M6/M7/release non-claims",
        )
        self_check_closure_decision(
            passing_closure_decision,
            evidence_path=passing_evidence,
            feature_summary_path=passing_summary_path,
        )
        closed_summary = qualify_milestone_m5_all_phase(
            feature_summary=passing_summary,
            evidence_summary_path=passing_evidence,
            closure_decision_path=passing_closure_decision,
            required_tolerance_digits=30,
        )
        expect(
            closed_summary["current_state"] == M5_ALL_PHASE_CLOSED_STATE
            and closed_summary["m5_all_phase_closed"],
            "all-phase M5 self-check should close only with passing parity plus decision evidence",
        )

        self_check_closure_decision(
            withheld_closure_decision,
            evidence_path=passing_evidence,
            feature_summary_path=passing_summary_path,
            approved=False,
            blockers=["synthetic closure approval withheld"],
        )
        withheld_summary = qualify_milestone_m5_all_phase(
            feature_summary=passing_summary,
            evidence_summary_path=passing_evidence,
            closure_decision_path=withheld_closure_decision,
            required_tolerance_digits=30,
        )
        expect(
            withheld_summary["current_state"] == M5_ALL_PHASE_BLOCKED_STATE
            and not withheld_summary["m5_all_phase_closed"],
            "withheld closure decision must block all-phase M5 closure",
        )

        self_check_evidence(
            exception_evidence,
            comparison_path=passing_compare,
            evidence_kind="approved-retained-state-exception",
        )
        exception_summary = qualify_milestone_m5(
            evidence_summary_path=exception_evidence,
            required_tolerance_digits=30,
        )
        expect(
            exception_summary["m5_feature_parity_passed"],
            "explicit retained-state exception self-check should pass",
        )

        self_check_evidence(
            blocked_evidence,
            comparison_path=failing_compare,
            omit_runtime_feature="fixed_eps",
            failed_example="automatic_loop",
        )
        blocked_summary = qualify_milestone_m5(
            evidence_summary_path=blocked_evidence,
            required_tolerance_digits=30,
        )
        write_json(blocked_summary_path, blocked_summary)
        expect(
            not blocked_summary["m5_feature_parity_passed"],
            "blocked M5 self-check should fail",
        )
        blockers = "\n".join(blocked_summary["blocking_reasons"])
        expect("missing required Phase F runtime feature: fixed_eps" in blockers, blockers)
        expect("automatic_loop: evidence kind 'metadata-only'" in blockers, blockers)
        expect("automatic_loop: comparison did not pass" in blockers, blockers)
        self_check_closure_decision(
            blocked_feature_closure_decision,
            evidence_path=blocked_evidence,
            feature_summary_path=blocked_summary_path,
        )
        blocked_feature_summary = qualify_milestone_m5_all_phase(
            feature_summary=blocked_summary,
            evidence_summary_path=blocked_evidence,
            closure_decision_path=blocked_feature_closure_decision,
            required_tolerance_digits=30,
        )
        expect(
            blocked_feature_summary["current_state"] == M5_ALL_PHASE_BLOCKED_STATE
            and not blocked_feature_summary["m5_all_phase_closed"],
            "approved closure decision must not override blocked feature parity",
        )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--evidence-summary", type=Path, help="M5 feature evidence sidecar")
    parser.add_argument("--out", type=Path, help="Optional output summary path")
    parser.add_argument("--required-tolerance-digits", type=int, default=30)
    parser.add_argument(
        "--all-phase-closure-decision",
        type=Path,
        help="Optional M5 all-phase closure-decision sidecar",
    )
    parser.add_argument("--self-check", action="store_true", help="Run synthetic self-checks")
    args = parser.parse_args()

    if args.self_check:
        run_self_check()
        if args.evidence_summary is None:
            return 0

    if args.evidence_summary is None:
        parser.error("--evidence-summary is required unless --self-check is used alone")

    summary = qualify_milestone_m5(
        evidence_summary_path=args.evidence_summary,
        required_tolerance_digits=args.required_tolerance_digits,
    )
    if args.all_phase_closure_decision is not None:
        summary = qualify_milestone_m5_all_phase(
            feature_summary=summary,
            evidence_summary_path=args.evidence_summary,
            closure_decision_path=args.all_phase_closure_decision,
            required_tolerance_digits=args.required_tolerance_digits,
        )
    if args.out is not None:
        write_json(args.out, summary)
    else:
        print(json.dumps(summary, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
