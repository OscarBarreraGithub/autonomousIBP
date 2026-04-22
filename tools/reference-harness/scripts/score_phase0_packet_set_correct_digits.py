#!/usr/bin/env python3
"""Aggregate phase-0 correct-digit scoring across the retained packet split."""

from __future__ import annotations

import argparse
import json
import shutil
import tempfile
from pathlib import Path
from typing import Any

from compare_phase0_packet_set_to_reference import (
    REQUIRED_SET_LABEL,
    benchmark_spec,
    discover_candidate_benchmark_ids,
    expected_packet_label,
    normalize_string,
    parse_packet_root_pair,
    write_candidate_packet,
    write_reference_packet,
)
from compare_phase0_results_to_reference import (
    REFERENCE_CAPTURED,
    repo_root,
    rewrite_candidate_result_manifest_pointer,
    write_json,
)
from score_phase0_correct_digits import (
    overwrite_candidate_output_text,
    overwrite_reference_output_text,
    score_phase0_correct_digits,
)


def score_phase0_packet_set_correct_digits(
    *,
    packet_root_pairs: list[tuple[Path, Path]],
    qualification_path: Path,
) -> dict[str, Any]:
    if not packet_root_pairs:
        raise RuntimeError(
            "at least one --packet-root-pair is required unless --self-check is set"
        )

    from compare_phase0_results_to_reference import load_phase0_scaffold

    scaffold_entries = load_phase0_scaffold(qualification_path)
    expected_benchmark_ids = sorted(
        benchmark_id
        for benchmark_id, entry in scaffold_entries.items()
        if entry["current_evidence_state"] == REFERENCE_CAPTURED
    )
    expected_packet_labels = sorted(
        {
            expected_packet_label(entry)
            for entry in scaffold_entries.values()
            if entry["current_evidence_state"] == REFERENCE_CAPTURED
        }
    )

    seen_packet_labels: set[str] = set()
    seen_benchmark_ids: set[str] = set()
    packet_scores: list[dict[str, Any]] = []
    benchmark_summaries: list[dict[str, Any]] = []

    for reference_root, candidate_root in packet_root_pairs:
        packet_summary = score_phase0_correct_digits(
            reference_root=reference_root,
            candidate_root=candidate_root,
            benchmark_ids=None,
            qualification_path=qualification_path,
        )
        packet_label = normalize_string(
            packet_summary["reference_packet_label"], "reference packet label"
        )
        if packet_label in seen_packet_labels:
            raise RuntimeError(f"duplicate reference packet label across packet pairs: {packet_label}")
        seen_packet_labels.add(packet_label)

        raw_selected_benchmark_ids = packet_summary.get("selected_benchmark_ids", [])
        if not isinstance(raw_selected_benchmark_ids, list):
            raise TypeError("selected_benchmark_ids must be a list")
        selected_benchmark_ids = [
            normalize_string(value, f"{packet_label} selected benchmark id")
            for value in raw_selected_benchmark_ids
        ]
        if not selected_benchmark_ids:
            raise RuntimeError(f"reference packet {packet_label} must score at least one benchmark")
        if len(set(selected_benchmark_ids)) != len(selected_benchmark_ids):
            raise RuntimeError(f"selected benchmark ids for {packet_label} must not contain duplicates")

        candidate_benchmark_ids = discover_candidate_benchmark_ids(candidate_root)
        if set(candidate_benchmark_ids) != set(selected_benchmark_ids):
            raise RuntimeError(
                "candidate packet benchmark ids for "
                f"{packet_label} must match the retained reference packet benchmark set"
            )

        raw_benchmarks = packet_summary.get("benchmarks", [])
        if not isinstance(raw_benchmarks, list):
            raise TypeError(f"{packet_label} benchmarks must be a list")
        packet_benchmark_ids: list[str] = []
        packet_minimum_correct_digits: list[int] = []
        for benchmark_summary in raw_benchmarks:
            if not isinstance(benchmark_summary, dict):
                raise TypeError(f"{packet_label} benchmark summaries must be objects")
            benchmark_id = normalize_string(
                benchmark_summary.get("benchmark_id", ""),
                f"{packet_label} benchmark_id",
            )
            if benchmark_id not in scaffold_entries:
                raise RuntimeError(
                    f"scored benchmark {benchmark_id!r} is not present in the qualification scaffold"
                )
            if benchmark_id in seen_benchmark_ids:
                raise RuntimeError(
                    f"benchmark {benchmark_id} was scored more than once across packet pairs"
                )
            if packet_label != expected_packet_label(scaffold_entries[benchmark_id]):
                raise RuntimeError(
                    f"reference packet label {packet_label} must match the scaffold packet label for "
                    f"{benchmark_id}"
                )
            minimum_observed_correct_digits = benchmark_summary.get(
                "minimum_observed_correct_digits"
            )
            if not isinstance(minimum_observed_correct_digits, int):
                raise TypeError(
                    f"{packet_label}:{benchmark_id} minimum_observed_correct_digits must be an int"
                )
            seen_benchmark_ids.add(benchmark_id)
            packet_benchmark_ids.append(benchmark_id)
            packet_minimum_correct_digits.append(minimum_observed_correct_digits)
            benchmark_summaries.append(dict(benchmark_summary))

        if set(packet_benchmark_ids) != set(selected_benchmark_ids):
            raise RuntimeError(
                f"pair summary benchmarks for {packet_label} must match selected_benchmark_ids"
            )

        packet_scores.append(
            {
                "reference_root": str(reference_root),
                "candidate_root": str(candidate_root),
                "reference_packet_label": packet_label,
                "reference_capture_state": packet_summary["reference_capture_state"],
                "candidate_benchmark_ids": sorted(candidate_benchmark_ids),
                "selected_benchmark_ids": sorted(selected_benchmark_ids),
                "reference_benchmarks_pass_retained_capture_checks": packet_summary[
                    "reference_benchmarks_pass_retained_capture_checks"
                ],
                "candidate_benchmark_ids_match_reference": True,
                "candidate_result_manifests_exist": packet_summary[
                    "candidate_result_manifests_exist"
                ],
                "candidate_primary_run_manifests_exist": packet_summary[
                    "candidate_primary_run_manifests_exist"
                ],
                "candidate_output_names_match_reference": packet_summary[
                    "candidate_output_names_match_reference"
                ],
                "candidate_numeric_literal_skeletons_match_reference": packet_summary[
                    "candidate_numeric_literal_skeletons_match_reference"
                ],
                "all_selected_benchmarks_meet_digit_thresholds": packet_summary[
                    "all_selected_benchmarks_meet_digit_thresholds"
                ],
                "minimum_observed_correct_digits": min(packet_minimum_correct_digits),
            }
        )

    if REQUIRED_SET_LABEL not in seen_packet_labels:
        raise RuntimeError(f"reference packet labels must include {REQUIRED_SET_LABEL}")
    if seen_packet_labels != set(expected_packet_labels):
        raise RuntimeError(
            "reference packet labels must match the scaffold reference-captured packet set"
        )
    if seen_benchmark_ids != set(expected_benchmark_ids):
        raise RuntimeError(
            "scored phase-0 benchmark ids must match the scaffold reference-captured set"
        )

    packet_scores.sort(key=lambda item: item["reference_packet_label"])
    benchmark_summaries.sort(key=lambda item: str(item.get("benchmark_id", "")).strip())

    return {
        "schema_version": 1,
        "qualification_path": str(qualification_path),
        "packet_pair_count": len(packet_scores),
        "reference_packet_labels": [item["reference_packet_label"] for item in packet_scores],
        "expected_reference_packet_labels": expected_packet_labels,
        "required_packet_present": REQUIRED_SET_LABEL in seen_packet_labels,
        "compared_phase0_ids": sorted(seen_benchmark_ids),
        "expected_reference_captured_phase0_ids": expected_benchmark_ids,
        "reference_packet_labels_match_scaffold_reference_captured": True,
        "compared_phase0_ids_match_scaffold_reference_captured": True,
        "reference_benchmarks_pass_retained_capture_checks": all(
            item["reference_benchmarks_pass_retained_capture_checks"] for item in packet_scores
        ),
        "candidate_packet_benchmark_sets_match_reference": all(
            item["candidate_benchmark_ids_match_reference"] for item in packet_scores
        ),
        "candidate_result_manifests_exist": all(
            item["candidate_result_manifests_exist"] for item in packet_scores
        ),
        "candidate_primary_run_manifests_exist": all(
            item["candidate_primary_run_manifests_exist"] for item in packet_scores
        ),
        "candidate_output_names_match_reference": all(
            item["candidate_output_names_match_reference"] for item in packet_scores
        ),
        "candidate_numeric_literal_skeletons_match_reference": all(
            item["candidate_numeric_literal_skeletons_match_reference"] for item in packet_scores
        ),
        "digit_threshold_profiles_reported": True,
        "required_failure_code_profiles_reported": True,
        "regression_profiles_reported": True,
        "all_compared_benchmarks_meet_digit_thresholds": all(
            benchmark["all_numeric_outputs_meet_threshold"] for benchmark in benchmark_summaries
        ),
        "minimum_observed_correct_digits_across_packet_set": min(
            int(benchmark["minimum_observed_correct_digits"]) for benchmark in benchmark_summaries
        ),
        "packet_scores": packet_scores,
        "benchmarks": benchmark_summaries,
    }


def numeric_text(output_name: str) -> str:
    return (
        "{"
        + output_name
        + " -> 1.1111111111111111111111111111111111111111111111111111111111111111`70.}"
    )


def failing_numeric_text(output_name: str) -> str:
    return (
        "{"
        + output_name
        + " -> 1.1111111111111111111111111111211111111111111111111111111111111111`70.}"
    )


def structural_text(output_name: str) -> str:
    return "{" + output_name + " -> {{(1 - 2*eps)/s, -s^(-1)}}}"


def rewrite_candidate_result_manifest_pointers(root: Path, benchmark_ids: list[str]) -> None:
    for benchmark_id in benchmark_ids:
        rewrite_candidate_result_manifest_pointer(root, benchmark_id)


def set_matching_texts(root: Path, benchmark_ids: list[str], outputs: dict[str, list[str]]) -> None:
    for benchmark_id in benchmark_ids:
        for output_name in outputs[benchmark_id]:
            overwrite_reference_output_text(
                root=root,
                benchmark_id=benchmark_id,
                output_name=output_name,
                text=(
                    structural_text(output_name)
                    if output_name == "diffeq"
                    else numeric_text(output_name)
                ),
            )


def set_matching_candidate_texts(
    root: Path, benchmark_ids: list[str], outputs: dict[str, list[str]]
) -> None:
    for benchmark_id in benchmark_ids:
        for output_name in outputs[benchmark_id]:
            overwrite_candidate_output_text(
                root=root,
                benchmark_id=benchmark_id,
                output_name=output_name,
                text=(
                    structural_text(output_name)
                    if output_name == "diffeq"
                    else numeric_text(output_name)
                ),
            )


def run_self_check(qualification_path: Path) -> dict[str, Any]:
    with tempfile.TemporaryDirectory(prefix="amflow-phase0-correct-digit-packet-set-self-check-") as tmp:
        temp_root = Path(tmp)
        required_reference_root = temp_root / "phase0-reference-captured-20260419-required-set"
        required_candidate_root = temp_root / "candidate-required-set"
        required_candidate_below_threshold_root = temp_root / "candidate-required-set-below-threshold"
        required_candidate_with_placeholder_root = temp_root / "candidate-required-set-placeholder"
        required_candidate_with_extra_root = temp_root / "candidate-required-set-extra"
        de_d0_reference_root = temp_root / "phase0-reference-captured-20260422-de-d0-pair"
        de_d0_candidate_root = temp_root / "candidate-de-d0-pair"
        user_hook_reference_root = temp_root / "phase0-reference-captured-20260422-user-hook-pair"
        user_hook_candidate_root = temp_root / "candidate-user-hook-pair"
        user_hook_candidate_skeleton_mismatch_root = (
            temp_root / "candidate-user-hook-skeleton-mismatch"
        )
        summary_path = temp_root / "packet-set-correct-digit-summary.json"

        required_benchmarks = [
            benchmark_spec("automatic_loop", "1", "2"),
            {"benchmark_id": "automatic_vs_manual", "output_names": ["auto"], "output_hashes": {"auto": "3" * 64}},
        ]
        de_d0_benchmarks = [
            {
                "benchmark_id": "differential_equation_solver",
                "output_names": ["sol1", "diffeq"],
                "output_hashes": {"sol1": "4" * 64, "diffeq": "5" * 64},
            },
            {
                "benchmark_id": "spacetime_dimension",
                "output_names": ["sol13D"],
                "output_hashes": {"sol13D": "6" * 64},
            },
        ]
        user_hook_benchmarks = [
            {
                "benchmark_id": "user_defined_amfmode",
                "output_names": ["sol"],
                "output_hashes": {"sol": "7" * 64},
            },
            {
                "benchmark_id": "user_defined_ending",
                "output_names": ["final_usr"],
                "output_hashes": {"final_usr": "8" * 64},
            },
        ]

        write_reference_packet(
            root=required_reference_root,
            benchmarks=required_benchmarks,
            capture_state=REFERENCE_CAPTURED,
            required_only=True,
        )
        write_candidate_packet(root=required_candidate_root, benchmarks=required_benchmarks)
        write_candidate_packet(
            root=required_candidate_with_placeholder_root, benchmarks=required_benchmarks
        )
        (
            required_candidate_with_placeholder_root / "results" / "phase0" / "automatic_phasespace"
        ).mkdir(parents=True, exist_ok=True)
        write_candidate_packet(
            root=required_candidate_with_extra_root,
            benchmarks=required_benchmarks
            + [{"benchmark_id": "user_defined_amfmode", "output_names": ["sol"], "output_hashes": {"sol": "9" * 64}}],
        )

        write_reference_packet(
            root=de_d0_reference_root,
            benchmarks=de_d0_benchmarks,
            capture_state="bootstrap-only",
            required_only=False,
            optional_capture_packet="de-d0-pair",
        )
        write_candidate_packet(root=de_d0_candidate_root, benchmarks=de_d0_benchmarks)

        write_reference_packet(
            root=user_hook_reference_root,
            benchmarks=user_hook_benchmarks,
            capture_state="bootstrap-only",
            required_only=False,
            optional_capture_packet="user-hook-pair",
        )
        write_candidate_packet(root=user_hook_candidate_root, benchmarks=user_hook_benchmarks)

        outputs = {
            "automatic_loop": ["sol1", "sol2"],
            "automatic_vs_manual": ["auto"],
            "differential_equation_solver": ["sol1", "diffeq"],
            "spacetime_dimension": ["sol13D"],
            "user_defined_amfmode": ["sol"],
            "user_defined_ending": ["final_usr"],
        }
        set_matching_texts(
            required_reference_root,
            ["automatic_loop", "automatic_vs_manual"],
            outputs,
        )
        set_matching_texts(
            de_d0_reference_root,
            ["differential_equation_solver", "spacetime_dimension"],
            outputs,
        )
        set_matching_texts(
            user_hook_reference_root,
            ["user_defined_amfmode", "user_defined_ending"],
            outputs,
        )
        set_matching_candidate_texts(
            required_candidate_root,
            ["automatic_loop", "automatic_vs_manual"],
            outputs,
        )
        set_matching_candidate_texts(
            required_candidate_with_placeholder_root,
            ["automatic_loop", "automatic_vs_manual"],
            outputs,
        )
        set_matching_candidate_texts(
            required_candidate_with_extra_root,
            ["automatic_loop", "automatic_vs_manual"],
            outputs,
        )
        set_matching_candidate_texts(
            de_d0_candidate_root,
            ["differential_equation_solver", "spacetime_dimension"],
            outputs,
        )
        set_matching_candidate_texts(
            user_hook_candidate_root,
            ["user_defined_amfmode", "user_defined_ending"],
            outputs,
        )

        matching_summary = score_phase0_packet_set_correct_digits(
            packet_root_pairs=[
                (required_reference_root, required_candidate_root),
                (de_d0_reference_root, de_d0_candidate_root),
                (user_hook_reference_root, user_hook_candidate_root),
            ],
            qualification_path=qualification_path,
        )
        write_json(summary_path, matching_summary)

        shutil.copytree(required_candidate_root, required_candidate_below_threshold_root)
        rewrite_candidate_result_manifest_pointers(
            required_candidate_below_threshold_root,
            ["automatic_loop", "automatic_vs_manual"],
        )
        overwrite_candidate_output_text(
            root=required_candidate_below_threshold_root,
            benchmark_id="automatic_loop",
            output_name="sol1",
            text=failing_numeric_text("sol1"),
        )
        threshold_failure_summary = score_phase0_packet_set_correct_digits(
            packet_root_pairs=[
                (required_reference_root, required_candidate_below_threshold_root),
                (de_d0_reference_root, de_d0_candidate_root),
                (user_hook_reference_root, user_hook_candidate_root),
            ],
            qualification_path=qualification_path,
        )

        shutil.copytree(user_hook_candidate_root, user_hook_candidate_skeleton_mismatch_root)
        rewrite_candidate_result_manifest_pointers(
            user_hook_candidate_skeleton_mismatch_root,
            ["user_defined_amfmode", "user_defined_ending"],
        )
        overwrite_candidate_output_text(
            root=user_hook_candidate_skeleton_mismatch_root,
            benchmark_id="user_defined_amfmode",
            output_name="sol",
            text="{sol -> {1.1111111111111111111111111111111111111111111111111111111111111111`70.}}",
        )
        skeleton_mismatch_rejected = False
        try:
            score_phase0_packet_set_correct_digits(
                packet_root_pairs=[
                    (required_reference_root, required_candidate_root),
                    (de_d0_reference_root, de_d0_candidate_root),
                    (user_hook_reference_root, user_hook_candidate_skeleton_mismatch_root),
                ],
                qualification_path=qualification_path,
            )
        except RuntimeError as error:
            skeleton_mismatch_rejected = "must preserve the retained nonnumeric skeleton" in str(
                error
            )

        missing_packet_rejected = False
        try:
            score_phase0_packet_set_correct_digits(
                packet_root_pairs=[
                    (required_reference_root, required_candidate_root),
                    (de_d0_reference_root, de_d0_candidate_root),
                ],
                qualification_path=qualification_path,
            )
        except RuntimeError as error:
            missing_packet_rejected = "must match the scaffold reference-captured packet set" in str(
                error
            )

        placeholder_directories_ignored = False
        try:
            placeholder_summary = score_phase0_packet_set_correct_digits(
                packet_root_pairs=[
                    (required_reference_root, required_candidate_with_placeholder_root),
                    (de_d0_reference_root, de_d0_candidate_root),
                    (user_hook_reference_root, user_hook_candidate_root),
                ],
                qualification_path=qualification_path,
            )
            placeholder_directories_ignored = placeholder_summary[
                "candidate_packet_benchmark_sets_match_reference"
            ]
        except RuntimeError:
            placeholder_directories_ignored = False

        extra_candidate_benchmark_rejected = False
        try:
            score_phase0_packet_set_correct_digits(
                packet_root_pairs=[
                    (required_reference_root, required_candidate_with_extra_root),
                    (de_d0_reference_root, de_d0_candidate_root),
                    (user_hook_reference_root, user_hook_candidate_root),
                ],
                qualification_path=qualification_path,
            )
        except RuntimeError as error:
            extra_candidate_benchmark_rejected = (
                "candidate packet benchmark ids for required-set must match" in str(error)
            )

        duplicate_packet_label_rejected = False
        try:
            score_phase0_packet_set_correct_digits(
                packet_root_pairs=[
                    (required_reference_root, required_candidate_root),
                    (de_d0_reference_root, de_d0_candidate_root),
                    (de_d0_reference_root, de_d0_candidate_root),
                    (user_hook_reference_root, user_hook_candidate_root),
                ],
                qualification_path=qualification_path,
            )
        except RuntimeError as error:
            duplicate_packet_label_rejected = "duplicate reference packet label" in str(error)

        malformed_packet_pair_rejected = False
        try:
            parse_packet_root_pair("missing-separator")
        except RuntimeError as error:
            malformed_packet_pair_rejected = (
                "--packet-root-pair must use the form" in str(error)
            )

        differential_equation_summary = next(
            benchmark
            for benchmark in matching_summary["benchmarks"]
            if benchmark["benchmark_id"] == "differential_equation_solver"
        )
        return {
            "matching_packet_set_meets_digit_thresholds": matching_summary[
                "all_compared_benchmarks_meet_digit_thresholds"
            ],
            "required_packet_present": matching_summary["required_packet_present"],
            "profiles_reported_from_scaffold": (
                matching_summary["benchmarks"][0]["digit_threshold_profile"]
                == "core-package-family-default"
                and matching_summary["benchmarks"][0]["minimum_correct_digits"] == 50
                and "insufficient_precision"
                in matching_summary["benchmarks"][0]["required_failure_codes"]
                and "unexpected master sets in Kira interface"
                in matching_summary["benchmarks"][0]["known_regression_families"]
            ),
            "structural_only_output_preserved": (
                differential_equation_summary["status"] == "digit-threshold-met"
                and differential_equation_summary["structural_only_output_names"] == ["diffeq"]
            ),
            "threshold_failure_detected": (
                not threshold_failure_summary["all_compared_benchmarks_meet_digit_thresholds"]
                and threshold_failure_summary["minimum_observed_correct_digits_across_packet_set"]
                < 50
            ),
            "missing_packet_rejected": missing_packet_rejected,
            "placeholder_directories_ignored": placeholder_directories_ignored,
            "extra_candidate_benchmark_rejected": extra_candidate_benchmark_rejected,
            "duplicate_packet_label_rejected": duplicate_packet_label_rejected,
            "malformed_packet_pair_rejected": malformed_packet_pair_rejected,
            "skeleton_mismatch_rejected": skeleton_mismatch_rejected,
            "summary_written": summary_path.exists(),
        }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--packet-root-pair",
        action="append",
        help="Reference/candidate packet pair in the form <reference_root>::<candidate_root>",
    )
    parser.add_argument(
        "--qualification-path",
        type=Path,
        help="Qualification scaffold JSON path",
    )
    parser.add_argument(
        "--summary-path",
        type=Path,
        help="Optional output file for the aggregated correct-digit summary",
    )
    parser.add_argument(
        "--self-check",
        action="store_true",
        help="Run a synthetic packet-set correct-digit self-check",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    qualification_path = (
        args.qualification_path
        if args.qualification_path is not None
        else repo_root()
        / "tools"
        / "reference-harness"
        / "templates"
        / "qualification-benchmarks.json"
    )

    if args.self_check:
        summary = run_self_check(qualification_path)
        if args.summary_path is not None:
            write_json(args.summary_path, summary)
        print(json.dumps(summary, indent=2, sort_keys=True))
        return 0

    raw_pairs = args.packet_root_pair
    if not raw_pairs:
        raise RuntimeError("--packet-root-pair is required unless --self-check is set")
    packet_root_pairs = [parse_packet_root_pair(raw_pair) for raw_pair in raw_pairs]
    summary = score_phase0_packet_set_correct_digits(
        packet_root_pairs=packet_root_pairs,
        qualification_path=qualification_path,
    )
    if args.summary_path is not None:
        write_json(args.summary_path, summary)
    print(json.dumps(summary, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
