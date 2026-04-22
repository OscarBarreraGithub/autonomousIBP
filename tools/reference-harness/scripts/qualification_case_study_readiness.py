#!/usr/bin/env python3
"""Summarize M6 case-study-family readiness against the frozen qualification scaffold."""

from __future__ import annotations

import argparse
import json
import tempfile
from pathlib import Path
from typing import Any

from bootstrap_reference_harness import (
    LANDED_CASE_STUDY_RUNTIME_PREDECESSORS,
    THEORY_BLOCKED_CASE_STUDY_RUNTIME_LANES,
    load_recorded_batch_ids,
    load_selected_benchmark_metadata,
    load_verification_strategy_digit_thresholds,
    load_yaml_string_list,
)
from freeze_phase0_goldens import load_json


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def write_json(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def write_text(path: Path, payload: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(payload, encoding="utf-8")


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


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


def load_profile_map(
    qualification: dict[str, Any],
    *,
    section: str,
    value_key: str,
    value_label: str,
) -> dict[str, Any]:
    raw_entries = qualification.get(section, [])
    if not isinstance(raw_entries, list):
        raise TypeError(f"{section} must be a list")
    mapping: dict[str, Any] = {}
    for raw in raw_entries:
        if not isinstance(raw, dict):
            raise TypeError(f"{section} entries must be objects")
        profile_id = str(raw.get("id", "")).strip()
        if not profile_id:
            raise ValueError(f"{section} entry id must not be empty")
        if profile_id in mapping:
            raise ValueError(f"duplicate {section} id: {profile_id}")
        value = raw.get(value_key)
        if value_key == "minimum_correct_digits":
            if not isinstance(value, int):
                raise TypeError(
                    f"{section}.{profile_id}.{value_key} must be an integer, got "
                    f"{type(value).__name__}"
                )
            expect(value > 0, f"{section}.{profile_id}.{value_key} must be positive")
        else:
            value = normalize_string_list(value, f"{section}.{profile_id}.{value_key}")
            expect(value, f"{section}.{profile_id}.{value_key} must not be empty")
        mapping[profile_id] = value
    expect(mapping, f"{value_label} profiles must not be empty")
    return mapping


def load_case_study_families(qualification_path: Path) -> list[dict[str, Any]]:
    qualification = load_json(qualification_path)
    digit_thresholds = load_profile_map(
        qualification,
        section="digit_threshold_profiles",
        value_key="minimum_correct_digits",
        value_label="digit threshold",
    )
    failure_profiles = load_profile_map(
        qualification,
        section="required_failure_code_profiles",
        value_key="codes",
        value_label="failure code",
    )
    regression_profiles = load_profile_map(
        qualification,
        section="known_regression_profiles",
        value_key="families",
        value_label="regression",
    )
    raw_case_studies = qualification.get("case_study_families", [])
    if not isinstance(raw_case_studies, list):
        raise TypeError("case_study_families must be a list")

    case_studies: list[dict[str, Any]] = []
    seen_ids: set[str] = set()
    for raw in raw_case_studies:
        if not isinstance(raw, dict):
            raise TypeError("case_study_families entries must be objects")
        family_id = str(raw.get("id", "")).strip()
        expect(family_id, "case-study family id must not be empty")
        expect(family_id not in seen_ids, f"duplicate case-study family id: {family_id}")
        seen_ids.add(family_id)

        digit_profile_id = str(raw.get("digit_threshold_profile", "")).strip()
        failure_profile_id = str(raw.get("failure_code_profile", "")).strip()
        regression_profile_id = str(raw.get("regression_profile", "")).strip()
        expect(
            digit_profile_id in digit_thresholds,
            f"case-study family {family_id} digit_threshold_profile must exist",
        )
        expect(
            failure_profile_id in failure_profiles,
            f"case-study family {family_id} failure_code_profile must exist",
        )
        expect(
            regression_profile_id in regression_profiles,
            f"case-study family {family_id} regression_profile must exist",
        )

        selected_refs = normalize_string_list(
            raw.get("selected_benchmark_refs", []),
            f"case-study family {family_id} selected_benchmark_refs",
        )
        expect_unique(selected_refs, f"case-study family {family_id} selected_benchmark_refs")

        case_studies.append(
            {
                "id": family_id,
                "parity_matrix_label": str(raw.get("parity_matrix_label", "")).strip(),
                "selected_benchmark_refs": selected_refs,
                "digit_threshold_profile": digit_profile_id,
                "minimum_correct_digits": digit_thresholds[digit_profile_id],
                "failure_code_profile": failure_profile_id,
                "required_failure_codes": failure_profiles[failure_profile_id],
                "regression_profile": regression_profile_id,
                "known_regression_families": regression_profiles[regression_profile_id],
                "next_runtime_lane": str(raw.get("next_runtime_lane", "")).strip(),
            }
        )

    expect(case_studies, "case_study_families must not be empty")
    return case_studies


def summarize_case_study_readiness(
    *,
    qualification_path: Path,
    selected_benchmarks_path: Path,
    parity_matrix_path: Path,
    verification_strategy_path: Path,
    implementation_ledger_path: Path,
) -> dict[str, Any]:
    case_studies = load_case_study_families(qualification_path)
    literature_ids, qualification_ids, qualification_anchor_refs = load_selected_benchmark_metadata(
        selected_benchmarks_path
    )
    parity_matrix_benchmarks = load_yaml_string_list(parity_matrix_path, "benchmarks")
    parity_failure_codes = load_yaml_string_list(parity_matrix_path, "required_failure_codes")
    parity_regressions = load_yaml_string_list(parity_matrix_path, "known_regressions")
    verification_thresholds = load_verification_strategy_digit_thresholds(verification_strategy_path)
    recorded_batch_ids = load_recorded_batch_ids(implementation_ledger_path)

    case_study_ids = [entry["id"] for entry in case_studies]
    parity_labels = [entry["parity_matrix_label"] for entry in case_studies]

    expect(
        set(case_study_ids) == qualification_ids,
        "case-study family ids must match the selected-benchmarks qualification ids",
    )
    expect(
        set(parity_labels) == set(parity_matrix_benchmarks),
        "case-study parity labels must match the parity-matrix benchmark set",
    )

    stronger_profiles = set(verification_thresholds) - {"core-package-family-default"}
    default_digits = verification_thresholds["core-package-family-default"]

    literature_anchor_ids: list[str] = []
    matrix_only_ids: list[str] = []
    runtime_blocked_ids: list[str] = []
    strong_precision_ids: list[str] = []
    case_study_summaries: list[dict[str, Any]] = []

    for entry in case_studies:
        family_id = entry["id"]
        anchor_ref = qualification_anchor_refs[family_id]
        expected_anchor_refs = [anchor_ref] if anchor_ref is not None else []
        expect(
            entry["selected_benchmark_refs"] == expected_anchor_refs,
            f"case-study family {family_id} selected_benchmark_refs must match "
            "selected-benchmarks.md",
        )
        expect(
            all(ref in literature_ids for ref in entry["selected_benchmark_refs"]),
            f"case-study family {family_id} selected_benchmark_refs must reference only "
            "selected-benchmarks literature ids",
        )

        if anchor_ref in stronger_profiles:
            expect(
                entry["digit_threshold_profile"] == anchor_ref,
                f"case-study family {family_id} should inherit stronger digit_threshold_profile "
                f"{anchor_ref} from selected-benchmarks.md",
            )
        else:
            expect(
                entry["digit_threshold_profile"] == "core-package-family-default",
                f"case-study family {family_id} should keep the default digit_threshold_profile",
            )
        expect(
            verification_thresholds[entry["digit_threshold_profile"]] == entry["minimum_correct_digits"],
            f"case-study family {family_id} minimum_correct_digits must match "
            "docs/verification-strategy.md",
        )
        expect(
            entry["required_failure_codes"] == parity_failure_codes,
            f"case-study family {family_id} required_failure_codes must match the parity matrix",
        )
        expect(
            entry["known_regression_families"] == parity_regressions,
            f"case-study family {family_id} known_regression_families must match the parity matrix",
        )

        expected_runtime_lane = THEORY_BLOCKED_CASE_STUDY_RUNTIME_LANES.get(family_id, "")
        expect(
            entry["next_runtime_lane"] == expected_runtime_lane,
            f"case-study family {family_id} next_runtime_lane must match the current theory frontier",
        )
        landed_predecessor = LANDED_CASE_STUDY_RUNTIME_PREDECESSORS.get(family_id, "")
        if expected_runtime_lane:
            expect(
                landed_predecessor,
                f"case-study family {family_id} must publish a landed runtime-lane predecessor",
            )
            expect(
                landed_predecessor in recorded_batch_ids,
                f"case-study family {family_id} landed predecessor {landed_predecessor} must be "
                "recorded in the implementation ledger",
            )
            family_state = "runtime-blocked"
            runtime_blocked_ids.append(family_id)
        elif entry["selected_benchmark_refs"]:
            family_state = "literature-anchor-selected"
            literature_anchor_ids.append(family_id)
        else:
            family_state = "matrix-only-anchor"
            matrix_only_ids.append(family_id)

        if entry["minimum_correct_digits"] > default_digits:
            strong_precision_ids.append(family_id)

        case_study_summaries.append(
            {
                "id": family_id,
                "family_state": family_state,
                "parity_matrix_label": entry["parity_matrix_label"],
                "selected_benchmark_refs": entry["selected_benchmark_refs"],
                "selected_benchmark_anchor_ref": anchor_ref or "",
                "digit_threshold_profile": entry["digit_threshold_profile"],
                "minimum_correct_digits": entry["minimum_correct_digits"],
                "failure_code_profile": entry["failure_code_profile"],
                "required_failure_codes": entry["required_failure_codes"],
                "regression_profile": entry["regression_profile"],
                "known_regression_families": entry["known_regression_families"],
                "next_runtime_lane": entry["next_runtime_lane"],
                "landed_runtime_predecessor": landed_predecessor,
            }
        )

    return {
        "schema_version": 1,
        "qualification_path": str(qualification_path),
        "selected_benchmarks_path": str(selected_benchmarks_path),
        "parity_matrix_path": str(parity_matrix_path),
        "verification_strategy_path": str(verification_strategy_path),
        "implementation_ledger_path": str(implementation_ledger_path),
        "default_minimum_correct_digits": default_digits,
        "case_study_family_count": len(case_study_summaries),
        "case_study_ids": case_study_ids,
        "literature_anchor_case_study_ids": literature_anchor_ids,
        "matrix_only_case_study_ids": matrix_only_ids,
        "runtime_blocked_case_study_ids": runtime_blocked_ids,
        "strong_precision_case_study_ids": strong_precision_ids,
        "case_study_ids_match_selected_benchmarks": True,
        "parity_labels_match_parity_matrix": True,
        "selected_benchmark_refs_match_selected_benchmarks": True,
        "digit_threshold_profiles_match_verification_strategy": True,
        "stronger_threshold_assignments_match_selected_benchmark_anchors": True,
        "failure_code_profile_matches_parity_matrix": True,
        "regression_profile_matches_parity_matrix": True,
        "runtime_blocked_case_study_lanes_match_theory_frontier": True,
        "runtime_lane_predecessors_recorded": True,
        "case_study_families": case_study_summaries,
    }


def run_self_check() -> dict[str, Any]:
    with tempfile.TemporaryDirectory(prefix="amflow-case-study-readiness-self-check-") as tmp:
        temp_root = Path(tmp)
        singular_case_study_id = "one-singular-endpoint-case"
        singular_runtime_lane = THEORY_BLOCKED_CASE_STUDY_RUNTIME_LANES[singular_case_study_id]
        singular_landed_predecessor = LANDED_CASE_STUDY_RUNTIME_PREDECESSORS[singular_case_study_id]
        singular_landed_predecessor_row = (
            singular_landed_predecessor[1:]
            if singular_landed_predecessor.startswith("b")
            else singular_landed_predecessor
        )
        qualification_path = temp_root / "qualification-benchmarks.json"
        selected_benchmarks_path = temp_root / "selected-benchmarks.md"
        parity_matrix_path = temp_root / "parity-matrix.yaml"
        verification_strategy_path = temp_root / "verification-strategy.md"
        implementation_ledger_path = temp_root / "implementation-ledger.md"
        summary_path = temp_root / "summary.json"

        write_json(
            qualification_path,
            {
                "schema_version": 1,
                "digit_threshold_profiles": [
                    {"id": "core-package-family-default", "minimum_correct_digits": 50},
                    {"id": "2024-tth-light-quark-loop-mi", "minimum_correct_digits": 100},
                ],
                "required_failure_code_profiles": [
                    {
                        "id": "default-required-failure-codes",
                        "codes": [
                            "insufficient_precision",
                            "master_set_instability",
                            "boundary_unsolved",
                            "continuation_budget_exhausted",
                        ],
                    }
                ],
                "known_regression_profiles": [
                    {
                        "id": "current-reviewed-regressions",
                        "families": [
                            "asymptotic-series overflow",
                            "unexpected master sets in Kira interface",
                        ],
                    }
                ],
                "case_study_families": [
                    {
                        "id": "ttbar-h",
                        "parity_matrix_label": "ttbar H",
                        "selected_benchmark_refs": ["2024-tth-light-quark-loop-mi"],
                        "digit_threshold_profile": "2024-tth-light-quark-loop-mi",
                        "failure_code_profile": "default-required-failure-codes",
                        "regression_profile": "current-reviewed-regressions",
                    },
                    {
                        "id": "package-double-box",
                        "parity_matrix_label": "package double box",
                        "selected_benchmark_refs": [],
                        "digit_threshold_profile": "core-package-family-default",
                        "failure_code_profile": "default-required-failure-codes",
                        "regression_profile": "current-reviewed-regressions",
                    },
                    {
                        "id": singular_case_study_id,
                        "parity_matrix_label": "one singular-endpoint case",
                        "selected_benchmark_refs": [],
                        "digit_threshold_profile": "core-package-family-default",
                        "failure_code_profile": "default-required-failure-codes",
                        "regression_profile": "current-reviewed-regressions",
                        "next_runtime_lane": singular_runtime_lane,
                    },
                ],
            },
        )
        write_text(
            selected_benchmarks_path,
            """# Selected Benchmarks

## Direct AMFlow Use Or Clear Build-On

- `2024-tth-light-quark-loop-mi`

## Qualification Scaffold IDs

- `ttbar-h`
  Current preferred precision anchor: `2024-tth-light-quark-loop-mi`, which carries the stronger `100`-digit floor.
- `package-double-box`
  Internal parity-matrix anchor for the baseline package family; no dedicated literature packet is frozen yet.
- `one-singular-endpoint-case`
  Internal parity-matrix guardrail anchor for the singular-surface family; no dedicated literature packet is frozen yet.
""",
        )
        write_text(
            parity_matrix_path,
            """benchmarks:
  - "ttbar H"
  - "package double box"
  - "one singular-endpoint case"
required_failure_codes:
  - "insufficient_precision"
  - "master_set_instability"
  - "boundary_unsolved"
  - "continuation_budget_exhausted"
known_regressions:
  - "asymptotic-series overflow"
  - "unexpected master sets in Kira interface"
""",
        )
        write_text(
            verification_strategy_path,
            """# Verification Strategy Bootstrap

- `>= 50` correct digits on core package families
- `>= 100` digits on `2024-tth-light-quark-loop-mi`
""",
        )
        write_text(
            implementation_ledger_path,
            f"""# Implementation And Review Ledger

| Item | Status | Evidence | Notes |
| --- | --- | --- | --- |
| `Batch {singular_landed_predecessor_row}` | implemented | synthetic | predecessor anchor |
""",
        )

        summary = summarize_case_study_readiness(
            qualification_path=qualification_path,
            selected_benchmarks_path=selected_benchmarks_path,
            parity_matrix_path=parity_matrix_path,
            verification_strategy_path=verification_strategy_path,
            implementation_ledger_path=implementation_ledger_path,
        )
        write_json(summary_path, summary)

        unknown_selected_benchmark_ref_rejected = False
        try:
            bad_qualification = load_json(qualification_path)
            bad_qualification["case_study_families"][0]["selected_benchmark_refs"] = [
                "2024-missing-anchor"
            ]
            unknown_ref_path = temp_root / "qualification-unknown-anchor.json"
            write_json(unknown_ref_path, bad_qualification)
            summarize_case_study_readiness(
                qualification_path=unknown_ref_path,
                selected_benchmarks_path=selected_benchmarks_path,
                parity_matrix_path=parity_matrix_path,
                verification_strategy_path=verification_strategy_path,
                implementation_ledger_path=implementation_ledger_path,
            )
        except RuntimeError as error:
            unknown_selected_benchmark_ref_rejected = (
                "selected_benchmark_refs must match selected-benchmarks.md" in str(error)
            )

        stronger_threshold_mismatch_rejected = False
        try:
            bad_qualification = load_json(qualification_path)
            bad_qualification["case_study_families"][0]["digit_threshold_profile"] = (
                "core-package-family-default"
            )
            bad_threshold_path = temp_root / "qualification-bad-threshold.json"
            write_json(bad_threshold_path, bad_qualification)
            summarize_case_study_readiness(
                qualification_path=bad_threshold_path,
                selected_benchmarks_path=selected_benchmarks_path,
                parity_matrix_path=parity_matrix_path,
                verification_strategy_path=verification_strategy_path,
                implementation_ledger_path=implementation_ledger_path,
            )
        except RuntimeError as error:
            stronger_threshold_mismatch_rejected = (
                "should inherit stronger digit_threshold_profile" in str(error)
            )

        blocked_lane_mismatch_rejected = False
        try:
            bad_qualification = load_json(qualification_path)
            bad_qualification["case_study_families"][2]["next_runtime_lane"] = "b62x"
            bad_lane_path = temp_root / "qualification-bad-lane.json"
            write_json(bad_lane_path, bad_qualification)
            summarize_case_study_readiness(
                qualification_path=bad_lane_path,
                selected_benchmarks_path=selected_benchmarks_path,
                parity_matrix_path=parity_matrix_path,
                verification_strategy_path=verification_strategy_path,
                implementation_ledger_path=implementation_ledger_path,
            )
        except RuntimeError as error:
            blocked_lane_mismatch_rejected = (
                "next_runtime_lane must match the current theory frontier" in str(error)
            )

        missing_predecessor_rejected = False
        try:
            missing_predecessor_ledger = temp_root / "implementation-ledger-missing-predecessor.md"
            write_text(
                missing_predecessor_ledger,
                """# Implementation And Review Ledger

| Item | Status | Evidence | Notes |
| --- | --- | --- | --- |
| `Batch 61i` | implemented | synthetic | unrelated |
""",
            )
            summarize_case_study_readiness(
                qualification_path=qualification_path,
                selected_benchmarks_path=selected_benchmarks_path,
                parity_matrix_path=parity_matrix_path,
                verification_strategy_path=verification_strategy_path,
                implementation_ledger_path=missing_predecessor_ledger,
            )
        except RuntimeError as error:
            missing_predecessor_rejected = (
                f"landed predecessor {singular_landed_predecessor} must be recorded in the "
                "implementation ledger"
                in str(error)
            )

        return {
            "case_study_ids_match_selected_benchmarks": summary[
                "case_study_ids_match_selected_benchmarks"
            ],
            "parity_labels_match_parity_matrix": summary["parity_labels_match_parity_matrix"],
            "selected_benchmark_refs_match_selected_benchmarks": summary[
                "selected_benchmark_refs_match_selected_benchmarks"
            ],
            "digit_threshold_profiles_match_verification_strategy": summary[
                "digit_threshold_profiles_match_verification_strategy"
            ],
            "stronger_threshold_assignments_match_selected_benchmark_anchors": summary[
                "stronger_threshold_assignments_match_selected_benchmark_anchors"
            ],
            "failure_code_profile_matches_parity_matrix": summary[
                "failure_code_profile_matches_parity_matrix"
            ],
            "regression_profile_matches_parity_matrix": summary[
                "regression_profile_matches_parity_matrix"
            ],
            "runtime_blocked_case_study_lanes_match_theory_frontier": summary[
                "runtime_blocked_case_study_lanes_match_theory_frontier"
            ],
            "runtime_lane_predecessors_recorded": summary["runtime_lane_predecessors_recorded"],
            "literature_anchor_case_study_ids_match_expected_set": (
                summary["literature_anchor_case_study_ids"] == ["ttbar-h"]
            ),
            "matrix_only_case_study_ids_match_expected_set": (
                summary["matrix_only_case_study_ids"] == ["package-double-box"]
            ),
            "runtime_blocked_case_study_ids_match_expected_set": (
                summary["runtime_blocked_case_study_ids"] == ["one-singular-endpoint-case"]
            ),
            "unknown_selected_benchmark_ref_rejected": unknown_selected_benchmark_ref_rejected,
            "stronger_threshold_mismatch_rejected": stronger_threshold_mismatch_rejected,
            "blocked_lane_mismatch_rejected": blocked_lane_mismatch_rejected,
            "missing_predecessor_rejected": missing_predecessor_rejected,
            "summary_written": summary_path.exists(),
        }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--qualification-path",
        type=Path,
        help="Qualification scaffold JSON path",
    )
    parser.add_argument(
        "--selected-benchmarks-path",
        type=Path,
        help="Selected benchmark markdown path",
    )
    parser.add_argument(
        "--parity-matrix-path",
        type=Path,
        help="Parity matrix YAML path",
    )
    parser.add_argument(
        "--verification-strategy-path",
        type=Path,
        help="Verification strategy markdown path",
    )
    parser.add_argument(
        "--implementation-ledger-path",
        type=Path,
        help="Implementation ledger markdown path",
    )
    parser.add_argument(
        "--summary-path",
        type=Path,
        help="Optional output file for the case-study readiness summary",
    )
    parser.add_argument(
        "--self-check",
        action="store_true",
        help="Run a synthetic case-study readiness check",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    root = repo_root()
    qualification_path = (
        args.qualification_path
        if args.qualification_path is not None
        else root / "tools" / "reference-harness" / "templates" / "qualification-benchmarks.json"
    )
    selected_benchmarks_path = (
        args.selected_benchmarks_path
        if args.selected_benchmarks_path is not None
        else root / "references" / "case-studies" / "selected-benchmarks.md"
    )
    parity_matrix_path = (
        args.parity_matrix_path
        if args.parity_matrix_path is not None
        else root / "specs" / "parity-matrix.yaml"
    )
    verification_strategy_path = (
        args.verification_strategy_path
        if args.verification_strategy_path is not None
        else root / "docs" / "verification-strategy.md"
    )
    implementation_ledger_path = (
        args.implementation_ledger_path
        if args.implementation_ledger_path is not None
        else root / "docs" / "implementation-ledger.md"
    )

    if args.self_check:
        print(json.dumps(run_self_check(), indent=2, sort_keys=True))
        return 0

    summary = summarize_case_study_readiness(
        qualification_path=qualification_path,
        selected_benchmarks_path=selected_benchmarks_path,
        parity_matrix_path=parity_matrix_path,
        verification_strategy_path=verification_strategy_path,
        implementation_ledger_path=implementation_ledger_path,
    )
    if args.summary_path is not None:
        write_json(args.summary_path, summary)
    print(json.dumps(summary, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
