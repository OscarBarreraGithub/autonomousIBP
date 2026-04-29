#!/usr/bin/env python3
"""Compose the reviewed phase-0 and case-study verdicts into an M6 qualification summary."""

from __future__ import annotations

import argparse
import json
import tempfile
from pathlib import Path
from typing import Any

from freeze_phase0_goldens import load_json


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def write_json(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def normalize_bool(raw: Any, label: str) -> bool:
    if not isinstance(raw, bool):
        raise TypeError(f"{label} must be a bool")
    return raw


def normalize_string(raw: Any, label: str) -> str:
    if not isinstance(raw, str):
        raise TypeError(f"{label} must be a string")
    value = raw.strip()
    expect(value, f"{label} must not be empty")
    return value


def normalize_string_list(raw: Any, label: str) -> list[str]:
    if raw is None:
        return []
    if not isinstance(raw, list):
        raise TypeError(f"{label} must be a list, got {type(raw).__name__}")
    values: list[str] = []
    for item in raw:
        if not isinstance(item, str):
            raise TypeError(f"{label} entries must be strings, got {type(item).__name__}")
        value = item.strip()
        if not value:
            raise ValueError(f"{label} entries must not be empty")
        values.append(value)
    return values


def expect_unique(values: list[str], label: str) -> None:
    expect(len(set(values)) == len(values), f"{label} must not contain duplicates")


def normalize_runtime_lane_entries(raw: Any, label: str) -> list[dict[str, str]]:
    if raw is None:
        return []
    if not isinstance(raw, list):
        raise TypeError(f"{label} must be a list")
    entries: list[dict[str, str]] = []
    seen_ids: set[str] = set()
    for entry in raw:
        if not isinstance(entry, dict):
            raise TypeError(f"{label} entries must be objects")
        entry_id = normalize_string(entry.get("id"), f"{label} id")
        next_runtime_lane_raw = entry.get("next_runtime_lane", "")
        if not isinstance(next_runtime_lane_raw, str):
            raise TypeError(f"{label} next_runtime_lane must be a string")
        next_runtime_lane = next_runtime_lane_raw.strip()
        if entry_id in seen_ids:
            raise ValueError(f"duplicate {label} id: {entry_id}")
        seen_ids.add(entry_id)
        entries.append({"id": entry_id, "next_runtime_lane": next_runtime_lane})
    return entries


def normalize_case_study_family_profiles(
    raw: Any,
    label: str,
) -> tuple[dict[str, int], dict[str, list[str]]]:
    if raw is None:
        return {}, {}
    if not isinstance(raw, list):
        raise TypeError(f"{label} must be a list")

    digit_thresholds: dict[str, int] = {}
    required_failure_codes: dict[str, list[str]] = {}
    for entry in raw:
        if not isinstance(entry, dict):
            raise TypeError(f"{label} entries must be objects")
        family_id = normalize_string(entry.get("id"), f"{label} id")
        if family_id in digit_thresholds:
            raise ValueError(f"duplicate {label} id: {family_id}")

        minimum_correct_digits = entry.get("minimum_correct_digits")
        if not isinstance(minimum_correct_digits, int):
            raise TypeError(f"{label} {family_id} minimum_correct_digits must be an int")
        expect(
            minimum_correct_digits > 0,
            f"{label} {family_id} minimum_correct_digits must be positive",
        )

        family_required_codes = normalize_string_list(
            entry.get("required_failure_codes", []),
            f"{label} {family_id} required_failure_codes",
        )
        expect_unique(family_required_codes, f"{label} {family_id} required_failure_codes")

        digit_thresholds[family_id] = minimum_correct_digits
        required_failure_codes[family_id] = family_required_codes

    return digit_thresholds, required_failure_codes


def normalize_positive_int_map(raw: Any, label: str) -> dict[str, int]:
    if raw is None:
        return {}
    if not isinstance(raw, dict):
        raise TypeError(f"{label} must be an object")
    values: dict[str, int] = {}
    for key, value in raw.items():
        key_text = normalize_string(key, f"{label} key")
        if not isinstance(value, int):
            raise TypeError(f"{label} {key_text} must be an int")
        expect(value > 0, f"{label} {key_text} must be positive")
        values[key_text] = value
    return values


def normalize_string_list_map(raw: Any, label: str) -> dict[str, list[str]]:
    if raw is None:
        return {}
    if not isinstance(raw, dict):
        raise TypeError(f"{label} must be an object")
    values: dict[str, list[str]] = {}
    for key, value in raw.items():
        key_text = normalize_string(key, f"{label} key")
        string_values = normalize_string_list(value, f"{label} {key_text}")
        expect_unique(string_values, f"{label} {key_text}")
        values[key_text] = string_values
    return values


def load_phase0_qualification_summary(summary_path: Path) -> dict[str, Any]:
    summary = load_json(summary_path)
    expect(summary.get("schema_version") == 1, "phase-0 qualification schema_version must be 1")
    expect(
        summary.get("scope") == "phase0-packet-set-only",
        "phase-0 qualification scope must be phase0-packet-set-only",
    )

    phase0_reference_captured_ids = normalize_string_list(
        summary.get("phase0_reference_captured_ids", []),
        "phase-0 qualification phase0_reference_captured_ids",
    )
    phase0_pending_ids = normalize_string_list(
        summary.get("phase0_pending_ids", []), "phase-0 qualification phase0_pending_ids"
    )
    expect_unique(
        phase0_reference_captured_ids,
        "phase-0 qualification phase0_reference_captured_ids",
    )
    expect_unique(phase0_pending_ids, "phase-0 qualification phase0_pending_ids")
    missing_required_failure_codes = normalize_string_list(
        summary.get("missing_required_failure_codes_across_packet_set", []),
        "phase-0 qualification missing_required_failure_codes_across_packet_set",
    )
    withheld_claims = normalize_string_list(
        summary.get("withheld_claims", []), "phase-0 qualification withheld_claims"
    )
    digit_thresholds = normalize_positive_int_map(
        summary.get("phase0_digit_thresholds_by_benchmark"),
        "phase-0 qualification phase0_digit_thresholds_by_benchmark",
    )
    required_failure_codes = normalize_string_list_map(
        summary.get("phase0_required_failure_codes_by_benchmark"),
        "phase-0 qualification phase0_required_failure_codes_by_benchmark",
    )
    expect(
        set(digit_thresholds) == set(phase0_reference_captured_ids),
        "phase-0 qualification digit-threshold profiles must match phase0_reference_captured_ids",
    )
    expect(
        set(required_failure_codes) == set(phase0_reference_captured_ids),
        "phase-0 qualification required-failure-code profiles must match phase0_reference_captured_ids",
    )

    return {
        **summary,
        "current_state": normalize_string(
            summary.get("current_state"), "phase-0 qualification current_state"
        ),
        "phase0_reference_captured_ids": phase0_reference_captured_ids,
        "phase0_pending_ids": phase0_pending_ids,
        "blocked_phase0_examples": normalize_runtime_lane_entries(
            summary.get("blocked_phase0_examples", []),
            "phase-0 qualification blocked_phase0_examples",
        ),
        "blocking_reasons": normalize_string_list(
            summary.get("blocking_reasons", []), "phase-0 qualification blocking_reasons"
        ),
        "withheld_claims": withheld_claims,
        "phase0_packet_set_qualified": normalize_bool(
            summary.get("phase0_packet_set_qualified"),
            "phase-0 qualification phase0_packet_set_qualified",
        ),
        "qualification_evidence_coherent": normalize_bool(
            summary.get("qualification_evidence_coherent"),
            "phase-0 qualification qualification_evidence_coherent",
        ),
        "packet_set_reference_comparison_passed": normalize_bool(
            summary.get("packet_set_reference_comparison_passed"),
            "phase-0 qualification packet_set_reference_comparison_passed",
        ),
        "packet_set_correct_digits_passed": normalize_bool(
            summary.get("packet_set_correct_digits_passed"),
            "phase-0 qualification packet_set_correct_digits_passed",
        ),
        "packet_set_failure_code_audits_complete": normalize_bool(
            summary.get("packet_set_failure_code_audits_complete"),
            "phase-0 qualification packet_set_failure_code_audits_complete",
        ),
        "packet_set_required_failure_codes_satisfied": normalize_bool(
            summary.get("packet_set_required_failure_codes_satisfied"),
            "phase-0 qualification packet_set_required_failure_codes_satisfied",
        ),
        "missing_required_failure_codes_across_packet_set": missing_required_failure_codes,
        "phase0_digit_thresholds_by_benchmark": digit_thresholds,
        "phase0_required_failure_codes_by_benchmark": required_failure_codes,
        "milestone_m6_ready": normalize_bool(
            summary.get("milestone_m6_ready"), "phase-0 qualification milestone_m6_ready"
        ),
    }


def load_case_study_qualification_summary(summary_path: Path) -> dict[str, Any]:
    summary = load_json(summary_path)
    expect(
        summary.get("schema_version") == 1,
        "case-study qualification schema_version must be 1",
    )
    expect(
        summary.get("scope") == "case-study-families-only",
        "case-study qualification scope must be case-study-families-only",
    )

    case_study_ids = normalize_string_list(
        summary.get("case_study_ids", []), "case-study qualification case_study_ids"
    )
    runtime_blocked_ids = normalize_string_list(
        summary.get("runtime_blocked_case_study_ids", []),
        "case-study qualification runtime_blocked_case_study_ids",
    )
    missing_numeric_ids = normalize_string_list(
        summary.get("missing_case_study_numeric_ids", []),
        "case-study qualification missing_case_study_numeric_ids",
    )
    expect_unique(case_study_ids, "case-study qualification case_study_ids")
    expect_unique(
        runtime_blocked_ids, "case-study qualification runtime_blocked_case_study_ids"
    )
    expect_unique(missing_numeric_ids, "case-study qualification missing_case_study_numeric_ids")
    digit_thresholds, required_failure_codes = normalize_case_study_family_profiles(
        summary.get("case_study_families", []),
        "case-study qualification case_study_families",
    )
    expect(
        set(digit_thresholds) == set(case_study_ids),
        "case-study qualification family profiles must match case_study_ids",
    )

    return {
        **summary,
        "current_state": normalize_string(
            summary.get("current_state"), "case-study qualification current_state"
        ),
        "case_study_ids": case_study_ids,
        "runtime_blocked_case_study_ids": runtime_blocked_ids,
        "missing_case_study_numeric_ids": missing_numeric_ids,
        "blocked_case_study_families": normalize_runtime_lane_entries(
            summary.get("blocked_case_study_families", []),
            "case-study qualification blocked_case_study_families",
        ),
        "case_study_digit_thresholds_by_family": digit_thresholds,
        "case_study_required_failure_codes_by_family": required_failure_codes,
        "blocking_reasons": normalize_string_list(
            summary.get("blocking_reasons", []), "case-study qualification blocking_reasons"
        ),
        "withheld_claims": normalize_string_list(
            summary.get("withheld_claims", []), "case-study qualification withheld_claims"
        ),
        "readiness_contract_coherent": normalize_bool(
            summary.get("readiness_contract_coherent"),
            "case-study qualification readiness_contract_coherent",
        ),
        "case_study_numeric_evidence_present": normalize_bool(
            summary.get("case_study_numeric_evidence_present"),
            "case-study qualification case_study_numeric_evidence_present",
        ),
        "case_study_numeric_comparison_passed": normalize_bool(
            summary.get("case_study_numeric_comparison_passed"),
            "case-study qualification case_study_numeric_comparison_passed",
        ),
        "all_case_studies_meet_digit_thresholds": normalize_bool(
            summary.get("all_case_studies_meet_digit_thresholds"),
            "case-study qualification all_case_studies_meet_digit_thresholds",
        ),
        "numeric_metadata_coherent": normalize_bool(
            summary.get("numeric_metadata_coherent"),
            "case-study qualification numeric_metadata_coherent",
        ),
        "case_study_families_qualified": normalize_bool(
            summary.get("case_study_families_qualified"),
            "case-study qualification case_study_families_qualified",
        ),
        "milestone_m6_ready": normalize_bool(
            summary.get("milestone_m6_ready"), "case-study qualification milestone_m6_ready"
        ),
    }


def unique_sorted(values: list[str]) -> list[str]:
    return sorted(set(values))


def summarize_milestone_m6_qualification(
    *,
    phase0_qualification_summary_path: Path,
    case_study_qualification_summary_path: Path,
) -> dict[str, Any]:
    phase0 = load_phase0_qualification_summary(phase0_qualification_summary_path)
    case_study = load_case_study_qualification_summary(case_study_qualification_summary_path)

    phase0_pending_runtime_lanes_closed = (
        not phase0["phase0_pending_ids"] and not phase0["blocked_phase0_examples"]
    )
    phase0_ready_for_m6 = (
        phase0["phase0_packet_set_qualified"] and phase0_pending_runtime_lanes_closed
    )
    case_study_ready_for_m6 = case_study["case_study_families_qualified"]
    milestone_m6_ready = phase0_ready_for_m6 and case_study_ready_for_m6

    if milestone_m6_ready:
        current_state = "milestone-m6-qualified"
    elif not phase0["phase0_packet_set_qualified"]:
        current_state = "blocked-on-phase0-packet-set"
    elif not phase0_pending_runtime_lanes_closed:
        current_state = "blocked-on-phase0-runtime-lanes"
    else:
        current_state = "blocked-on-case-study-families"

    blocking_reasons: list[str] = []
    if not phase0["phase0_packet_set_qualified"]:
        if phase0["blocking_reasons"]:
            blocking_reasons.extend(f"phase0: {reason}" for reason in phase0["blocking_reasons"])
        else:
            blocking_reasons.append("phase0: phase-0 packet-set verdict is not qualified")
    if phase0["phase0_pending_ids"]:
        blocking_reasons.append("phase0: runtime-lane-blocked phase-0 examples remain pending")
    if not case_study["case_study_families_qualified"]:
        if case_study["blocking_reasons"]:
            blocking_reasons.extend(
                f"case-study: {reason}" for reason in case_study["blocking_reasons"]
            )
        else:
            blocking_reasons.append("case-study: case-study family verdict is not qualified")

    blocked_runtime_lanes = unique_sorted(
        [
            entry["next_runtime_lane"]
            for entry in phase0["blocked_phase0_examples"]
            if entry["next_runtime_lane"]
        ]
        + [
            entry["next_runtime_lane"]
            for entry in case_study["blocked_case_study_families"]
            if entry["next_runtime_lane"]
        ]
    )

    withheld_claims = [
        "This summary does not launch the C++ runtime or create new retained captures.",
        "This summary does not mark Milestone M7 or release readiness.",
        "This summary does not widen runtime or public behavior.",
    ]
    if not milestone_m6_ready:
        withheld_claims.insert(0, "This summary does not claim Milestone M6 closure.")

    return {
        "schema_version": 1,
        "scope": "milestone-m6-qualification",
        "current_state": current_state,
        "phase0_qualification_summary_path": str(phase0_qualification_summary_path),
        "case_study_qualification_summary_path": str(case_study_qualification_summary_path),
        "phase0_packet_set_qualified": phase0["phase0_packet_set_qualified"],
        "phase0_ready_for_m6": phase0_ready_for_m6,
        "phase0_pending_runtime_lanes_closed": phase0_pending_runtime_lanes_closed,
        "case_study_families_qualified": case_study["case_study_families_qualified"],
        "case_study_ready_for_m6": case_study_ready_for_m6,
        "milestone_m6_ready": milestone_m6_ready,
        "phase0_current_state": phase0["current_state"],
        "case_study_current_state": case_study["current_state"],
        "phase0_reference_captured_ids": phase0["phase0_reference_captured_ids"],
        "phase0_pending_ids": phase0["phase0_pending_ids"],
        "blocked_phase0_examples": phase0["blocked_phase0_examples"],
        "phase0_digit_thresholds_by_benchmark": phase0[
            "phase0_digit_thresholds_by_benchmark"
        ],
        "phase0_required_failure_codes_by_benchmark": phase0[
            "phase0_required_failure_codes_by_benchmark"
        ],
        "case_study_ids": case_study["case_study_ids"],
        "runtime_blocked_case_study_ids": case_study["runtime_blocked_case_study_ids"],
        "missing_case_study_numeric_ids": case_study["missing_case_study_numeric_ids"],
        "blocked_case_study_families": case_study["blocked_case_study_families"],
        "case_study_digit_thresholds_by_family": case_study[
            "case_study_digit_thresholds_by_family"
        ],
        "case_study_required_failure_codes_by_family": case_study[
            "case_study_required_failure_codes_by_family"
        ],
        "blocked_runtime_lanes": blocked_runtime_lanes,
        "missing_required_failure_codes_across_packet_set": phase0[
            "missing_required_failure_codes_across_packet_set"
        ],
        "phase0_blocking_reasons": phase0["blocking_reasons"],
        "case_study_blocking_reasons": case_study["blocking_reasons"],
        "blocking_reasons": blocking_reasons,
        "phase0_withheld_claims": phase0["withheld_claims"],
        "case_study_withheld_claims": case_study["withheld_claims"],
        "withheld_claims": withheld_claims,
    }


def write_synthetic_phase0_summary(
    path: Path,
    *,
    qualified: bool,
    pending_runtime_lanes: bool = False,
) -> None:
    write_json(
        path,
        {
            "schema_version": 1,
            "scope": "phase0-packet-set-only",
            "current_state": (
                "phase0-packet-set-qualified"
                if qualified
                else "blocked-on-correct-digit-thresholds"
            ),
            "phase0_reference_captured_ids": ["automatic_loop", "automatic_vs_manual"],
            "phase0_pending_ids": ["complex_kinematics"] if pending_runtime_lanes else [],
            "blocked_phase0_examples": (
                [{"id": "complex_kinematics", "next_runtime_lane": "b61n"}]
                if pending_runtime_lanes
                else []
            ),
            "qualification_evidence_coherent": True,
            "packet_set_reference_comparison_passed": True,
            "packet_set_correct_digits_passed": qualified,
            "packet_set_failure_code_audits_complete": qualified,
            "packet_set_required_failure_codes_satisfied": qualified,
            "missing_required_failure_codes_across_packet_set": (
                [] if qualified else ["insufficient_precision"]
            ),
            "phase0_digit_thresholds_by_benchmark": {
                "automatic_loop": 50,
                "automatic_vs_manual": 50,
            },
            "phase0_required_failure_codes_by_benchmark": {
                "automatic_loop": ["insufficient_precision", "boundary_unsolved"],
                "automatic_vs_manual": ["master_set_instability"],
            },
            "phase0_packet_set_qualified": qualified,
            "milestone_m6_ready": False,
            "blocking_reasons": (
                []
                if qualified
                else ["retained packet-set correct-digit scoring has not fully passed"]
            ),
            "withheld_claims": [
                "This summary does not compare retained case-study numerics.",
                "This summary does not by itself claim Milestone M6 closure.",
            ],
        },
    )


def write_synthetic_case_study_summary(
    path: Path,
    *,
    qualified: bool,
    runtime_blocked: bool = False,
) -> None:
    case_study_families = [
        {
            "id": "ttbar-h",
            "minimum_correct_digits": 100,
            "required_failure_codes": ["insufficient_precision", "master_set_instability"],
        },
        {
            "id": "one-singular-endpoint-case",
            "minimum_correct_digits": 50,
            "required_failure_codes": ["physical_kinematics_singular"],
        },
    ]
    blocked_families = (
        [{"id": "one-singular-endpoint-case", "next_runtime_lane": "b62p"}]
        if runtime_blocked
        else []
    )
    write_json(
        path,
        {
            "schema_version": 1,
            "scope": "case-study-families-only",
            "current_state": (
                "case-study-families-qualified"
                if qualified
                else "blocked-on-runtime-lanes"
                if runtime_blocked
                else "blocked-on-case-study-numeric-evidence"
            ),
            "case_study_ids": ["ttbar-h", "one-singular-endpoint-case"],
            "runtime_blocked_case_study_ids": (
                ["one-singular-endpoint-case"] if runtime_blocked else []
            ),
            "missing_case_study_numeric_ids": [] if qualified else ["ttbar-h"],
            "blocked_case_study_families": blocked_families,
            "case_study_families": case_study_families,
            "readiness_contract_coherent": True,
            "case_study_numeric_evidence_present": qualified,
            "case_study_numeric_comparison_passed": qualified,
            "all_case_studies_meet_digit_thresholds": qualified,
            "numeric_metadata_coherent": True,
            "case_study_families_qualified": qualified,
            "milestone_m6_ready": False,
            "blocking_reasons": (
                []
                if qualified
                else ["case-study family one-singular-endpoint-case is still blocked on a runtime lane"]
                if runtime_blocked
                else ["case-study numerics are not yet compared"]
            ),
            "withheld_claims": [
                "This summary does not compare phase-0 packet-set evidence.",
                "This summary does not by itself claim Milestone M6 closure.",
            ],
        },
    )


def run_self_check() -> dict[str, Any]:
    with tempfile.TemporaryDirectory(prefix="amflow-m6-qualification-self-check-") as tmp:
        temp_root = Path(tmp)
        phase0_passing_path = temp_root / "phase0-passing.json"
        phase0_blocked_path = temp_root / "phase0-blocked.json"
        phase0_pending_path = temp_root / "phase0-pending.json"
        case_passing_path = temp_root / "case-passing.json"
        case_blocked_path = temp_root / "case-blocked.json"
        summary_path = temp_root / "m6-summary.json"
        malformed_phase0_path = temp_root / "phase0-malformed.json"

        write_synthetic_phase0_summary(phase0_passing_path, qualified=True)
        write_synthetic_phase0_summary(phase0_blocked_path, qualified=False)
        write_synthetic_phase0_summary(
            phase0_pending_path,
            qualified=True,
            pending_runtime_lanes=True,
        )
        write_synthetic_case_study_summary(case_passing_path, qualified=True)
        write_synthetic_case_study_summary(
            case_blocked_path,
            qualified=False,
            runtime_blocked=True,
        )

        passing_summary = summarize_milestone_m6_qualification(
            phase0_qualification_summary_path=phase0_passing_path,
            case_study_qualification_summary_path=case_passing_path,
        )
        write_json(summary_path, passing_summary)
        phase0_blocked_summary = summarize_milestone_m6_qualification(
            phase0_qualification_summary_path=phase0_blocked_path,
            case_study_qualification_summary_path=case_passing_path,
        )
        phase0_pending_summary = summarize_milestone_m6_qualification(
            phase0_qualification_summary_path=phase0_pending_path,
            case_study_qualification_summary_path=case_passing_path,
        )
        case_blocked_summary = summarize_milestone_m6_qualification(
            phase0_qualification_summary_path=phase0_passing_path,
            case_study_qualification_summary_path=case_blocked_path,
        )
        both_blocked_summary = summarize_milestone_m6_qualification(
            phase0_qualification_summary_path=phase0_blocked_path,
            case_study_qualification_summary_path=case_blocked_path,
        )

        malformed = load_json(phase0_passing_path)
        malformed["scope"] = "phase0"
        write_json(malformed_phase0_path, malformed)
        malformed_phase0_rejected = False
        try:
            summarize_milestone_m6_qualification(
                phase0_qualification_summary_path=malformed_phase0_path,
                case_study_qualification_summary_path=case_passing_path,
            )
        except RuntimeError as error:
            malformed_phase0_rejected = "scope must be phase0-packet-set-only" in str(error)

        return {
            "schema_version": 1,
            "matching_subverdicts_qualify_m6": passing_summary["milestone_m6_ready"],
            "phase0_packet_set_blocker_blocks_m6": (
                phase0_blocked_summary["current_state"] == "blocked-on-phase0-packet-set"
                and not phase0_blocked_summary["milestone_m6_ready"]
            ),
            "phase0_runtime_lane_blocker_blocks_m6": (
                phase0_pending_summary["current_state"] == "blocked-on-phase0-runtime-lanes"
                and phase0_pending_summary["blocked_runtime_lanes"] == ["b61n"]
            ),
            "case_study_blocker_blocks_m6": (
                case_blocked_summary["current_state"] == "blocked-on-case-study-families"
                and not case_blocked_summary["milestone_m6_ready"]
            ),
            "phase0_profiles_preserved": (
                passing_summary["phase0_digit_thresholds_by_benchmark"] == {
                    "automatic_loop": 50,
                    "automatic_vs_manual": 50,
                }
                and passing_summary["phase0_required_failure_codes_by_benchmark"][
                    "automatic_loop"
                ]
                == ["insufficient_precision", "boundary_unsolved"]
                and passing_summary["phase0_required_failure_codes_by_benchmark"][
                    "automatic_vs_manual"
                ]
                == ["master_set_instability"]
            ),
            "case_study_profiles_preserved": (
                passing_summary["case_study_digit_thresholds_by_family"] == {
                    "ttbar-h": 100,
                    "one-singular-endpoint-case": 50,
                }
                and passing_summary["case_study_required_failure_codes_by_family"]["ttbar-h"]
                == ["insufficient_precision", "master_set_instability"]
                and passing_summary["case_study_required_failure_codes_by_family"][
                    "one-singular-endpoint-case"
                ]
                == ["physical_kinematics_singular"]
            ),
            "phase0_and_case_study_blockers_preserved": (
                "phase0: retained packet-set correct-digit scoring has not fully passed"
                in both_blocked_summary["blocking_reasons"]
                and "case-study: case-study family one-singular-endpoint-case is still blocked on a runtime lane"
                in both_blocked_summary["blocking_reasons"]
                and both_blocked_summary["blocked_runtime_lanes"] == ["b62p"]
            ),
            "subverdict_withheld_claims_preserved": (
                "This summary does not by itself claim Milestone M6 closure."
                in both_blocked_summary["phase0_withheld_claims"]
                and "This summary does not by itself claim Milestone M6 closure."
                in both_blocked_summary["case_study_withheld_claims"]
                and "This summary does not claim Milestone M6 closure."
                in both_blocked_summary["withheld_claims"]
            ),
            "malformed_phase0_scope_rejected": malformed_phase0_rejected,
            "summary_written": summary_path.exists(),
        }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--phase0-qualification-summary",
        type=Path,
        help="Path to the phase-0 packet-set verdict emitted by qualify_phase0_packet_set.py",
    )
    parser.add_argument(
        "--case-study-qualification-summary",
        type=Path,
        help="Path to the case-study-family verdict emitted by qualify_case_study_families.py",
    )
    parser.add_argument("--summary-path", type=Path, help="Optional path to write the JSON summary")
    parser.add_argument("--self-check", action="store_true", help="Run synthetic regression checks")
    args = parser.parse_args()

    if args.self_check:
        summary = run_self_check()
    else:
        expect(
            args.phase0_qualification_summary is not None,
            "--phase0-qualification-summary is required unless --self-check is used",
        )
        expect(
            args.case_study_qualification_summary is not None,
            "--case-study-qualification-summary is required unless --self-check is used",
        )
        summary = summarize_milestone_m6_qualification(
            phase0_qualification_summary_path=args.phase0_qualification_summary,
            case_study_qualification_summary_path=args.case_study_qualification_summary,
        )

    if args.summary_path is not None:
        write_json(args.summary_path, summary)
    print(json.dumps(summary, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
