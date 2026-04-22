#!/usr/bin/env python3
"""Compare a candidate phase-0 packet set against the retained reference packet split."""

from __future__ import annotations

import argparse
import json
import tempfile
from pathlib import Path
from typing import Any

from compare_phase0_results_to_reference import (
    BOOTSTRAP_ONLY,
    REFERENCE_CAPTURED,
    REQUIRED_SET_LABEL,
    compare_phase0_results_to_reference,
    load_phase0_scaffold,
    repo_root,
    write_json,
)


PACKET_PAIR_SEPARATOR = "::"


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def normalize_string(raw: Any, label: str) -> str:
    if not isinstance(raw, str):
        raise TypeError(f"{label} must be a string, got {type(raw).__name__}")
    value = raw.strip()
    expect(value, f"{label} must not be empty")
    return value


def parse_packet_root_pair(raw_pair: str) -> tuple[Path, Path]:
    value = normalize_string(raw_pair, "packet root pair")
    parts = value.split(PACKET_PAIR_SEPARATOR, 1)
    expect(
        len(parts) == 2,
        "--packet-root-pair must use the form <reference_root>::<candidate_root>",
    )
    reference_root = Path(parts[0].strip())
    candidate_root = Path(parts[1].strip())
    expect(str(reference_root).strip(), "reference root in --packet-root-pair must not be empty")
    expect(str(candidate_root).strip(), "candidate root in --packet-root-pair must not be empty")
    return reference_root, candidate_root


def expected_packet_label(scaffold_entry: dict[str, Any]) -> str:
    if bool(scaffold_entry["required_capture"]):
        return REQUIRED_SET_LABEL
    packet_label = normalize_string(
        scaffold_entry["optional_capture_packet"],
        f"{scaffold_entry['phase0_catalog_id']} optional_capture_packet",
    )
    return packet_label


def discover_candidate_benchmark_ids(candidate_root: Path) -> list[str]:
    phase0_root = candidate_root / "results" / "phase0"
    expect(phase0_root.is_dir(), f"candidate phase-0 results root must exist: {phase0_root}")
    benchmark_ids: list[str] = []
    for entry in sorted(phase0_root.iterdir()):
        if not entry.is_dir():
            continue
        benchmark_id = normalize_string(entry.name, "candidate benchmark directory")
        result_manifest_path = entry / "result-manifest.json"
        if not result_manifest_path.exists():
            continue
        benchmark_ids.append(benchmark_id)
    expect(benchmark_ids, f"candidate packet must publish at least one benchmark under {candidate_root}")
    expect(
        len(set(benchmark_ids)) == len(benchmark_ids),
        f"candidate packet benchmark ids under {candidate_root} must not contain duplicates",
    )
    return benchmark_ids


def compare_phase0_packet_set_to_reference(
    *,
    packet_root_pairs: list[tuple[Path, Path]],
    qualification_path: Path,
) -> dict[str, Any]:
    expect(packet_root_pairs, "at least one --packet-root-pair is required unless --self-check is set")
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
    packet_comparisons: list[dict[str, Any]] = []
    benchmark_summaries: list[dict[str, Any]] = []

    for reference_root, candidate_root in packet_root_pairs:
        packet_summary = compare_phase0_results_to_reference(
            reference_root=reference_root,
            candidate_root=candidate_root,
            benchmark_ids=None,
            qualification_path=qualification_path,
        )
        packet_label = normalize_string(
            packet_summary["reference_packet_label"], "reference packet label"
        )
        expect(
            packet_label not in seen_packet_labels,
            f"duplicate reference packet label across packet pairs: {packet_label}",
        )
        seen_packet_labels.add(packet_label)

        raw_selected_benchmark_ids = packet_summary.get("selected_benchmark_ids", [])
        if not isinstance(raw_selected_benchmark_ids, list):
            raise TypeError("selected_benchmark_ids must be a list")
        selected_benchmark_ids = [
            normalize_string(value, f"{packet_label} selected benchmark id")
            for value in raw_selected_benchmark_ids
        ]
        expect(
            selected_benchmark_ids,
            f"reference packet {packet_label} must compare at least one benchmark",
        )
        expect(
            len(set(selected_benchmark_ids)) == len(selected_benchmark_ids),
            f"selected benchmark ids for {packet_label} must not contain duplicates",
        )
        candidate_benchmark_ids = discover_candidate_benchmark_ids(candidate_root)
        expect(
            set(candidate_benchmark_ids) == set(selected_benchmark_ids),
            f"candidate packet benchmark ids for {packet_label} must match the retained "
            "reference packet benchmark set",
        )

        raw_benchmarks = packet_summary.get("benchmarks", [])
        if not isinstance(raw_benchmarks, list):
            raise TypeError(f"{packet_label} benchmarks must be a list")
        packet_benchmark_ids: list[str] = []
        for benchmark_summary in raw_benchmarks:
            if not isinstance(benchmark_summary, dict):
                raise TypeError(f"{packet_label} benchmark summaries must be objects")
            benchmark_id = normalize_string(
                benchmark_summary.get("benchmark_id", ""),
                f"{packet_label} benchmark_id",
            )
            expect(
                benchmark_id in scaffold_entries,
                f"compared benchmark {benchmark_id!r} is not present in the qualification scaffold",
            )
            expect(
                benchmark_id not in seen_benchmark_ids,
                f"benchmark {benchmark_id} was compared more than once across packet pairs",
            )
            expect(
                packet_label == expected_packet_label(scaffold_entries[benchmark_id]),
                f"reference packet label {packet_label} must match the scaffold packet label for "
                f"{benchmark_id}",
            )
            seen_benchmark_ids.add(benchmark_id)
            packet_benchmark_ids.append(benchmark_id)
            benchmark_summaries.append(dict(benchmark_summary))

        expect(
            set(packet_benchmark_ids) == set(selected_benchmark_ids),
            f"pair summary benchmarks for {packet_label} must match selected_benchmark_ids",
        )

        packet_comparisons.append(
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
                "candidate_output_hashes_match_reference": packet_summary[
                    "candidate_output_hashes_match_reference"
                ],
            }
        )

    expect(
        REQUIRED_SET_LABEL in seen_packet_labels,
        f"reference packet labels must include {REQUIRED_SET_LABEL}",
    )
    expect(
        seen_packet_labels == set(expected_packet_labels),
        "reference packet labels must match the scaffold reference-captured packet set",
    )
    expect(
        seen_benchmark_ids == set(expected_benchmark_ids),
        "compared phase-0 benchmark ids must match the scaffold reference-captured set",
    )

    packet_comparisons.sort(key=lambda item: item["reference_packet_label"])
    benchmark_summaries.sort(key=lambda item: str(item.get("benchmark_id", "")).strip())

    return {
        "schema_version": 1,
        "qualification_path": str(qualification_path),
        "packet_pair_count": len(packet_comparisons),
        "reference_packet_labels": [item["reference_packet_label"] for item in packet_comparisons],
        "expected_reference_packet_labels": expected_packet_labels,
        "required_packet_present": REQUIRED_SET_LABEL in seen_packet_labels,
        "compared_phase0_ids": sorted(seen_benchmark_ids),
        "expected_reference_captured_phase0_ids": expected_benchmark_ids,
        "reference_packet_labels_match_scaffold_reference_captured": True,
        "compared_phase0_ids_match_scaffold_reference_captured": True,
        "reference_benchmarks_pass_retained_capture_checks": all(
            item["reference_benchmarks_pass_retained_capture_checks"]
            for item in packet_comparisons
        ),
        "candidate_packet_benchmark_sets_match_reference": all(
            item["candidate_benchmark_ids_match_reference"] for item in packet_comparisons
        ),
        "candidate_result_manifests_exist": all(
            item["candidate_result_manifests_exist"] for item in packet_comparisons
        ),
        "candidate_primary_run_manifests_exist": all(
            item["candidate_primary_run_manifests_exist"] for item in packet_comparisons
        ),
        "candidate_output_names_match_reference": all(
            item["candidate_output_names_match_reference"] for item in packet_comparisons
        ),
        "candidate_output_hashes_match_reference": all(
            item["candidate_output_hashes_match_reference"] for item in packet_comparisons
        ),
        "digit_threshold_profiles_reported": True,
        "required_failure_code_profiles_reported": True,
        "regression_profiles_reported": True,
        "packet_comparisons": packet_comparisons,
        "benchmarks": benchmark_summaries,
    }


def write_reference_packet(
    *,
    root: Path,
    benchmarks: list[dict[str, Any]],
    capture_state: str,
    required_only: bool,
    optional_capture_packet: str = "",
) -> None:
    manifests_dir = root / "manifests"
    state_dir = root / "state"
    comparisons_dir = root / "comparisons" / "phase0"
    manifests_dir.mkdir(parents=True, exist_ok=True)
    state_dir.mkdir(parents=True, exist_ok=True)
    comparisons_dir.mkdir(parents=True, exist_ok=True)

    manifest_path = manifests_dir / "phase0-reference.json"
    summary_path = state_dir / "phase0-reference.capture.json"
    benchmark_summaries: list[dict[str, Any]] = []
    benchmark_ids: list[str] = []

    for benchmark in benchmarks:
        benchmark_id = normalize_string(benchmark["benchmark_id"], "reference benchmark id")
        benchmark_ids.append(benchmark_id)
        goldens_dir = root / "goldens" / "phase0" / benchmark_id
        results_dir = root / "results" / "phase0" / benchmark_id
        (goldens_dir / "captured" / "canonical").mkdir(parents=True, exist_ok=True)
        results_dir.mkdir(parents=True, exist_ok=True)

        result_manifest_path = results_dir / "result-manifest.json"
        golden_manifest_path = goldens_dir / "golden-manifest.json"
        comparison_summary_path = comparisons_dir / f"{benchmark_id}.summary.json"

        golden_outputs: list[dict[str, Any]] = []
        output_names = benchmark["output_names"]
        output_hashes = benchmark["output_hashes"]
        for output_name in output_names:
            raw_output_path = goldens_dir / "captured" / output_name
            canonical_text_path = goldens_dir / "captured" / "canonical" / (
                output_name + ".canonical.txt"
            )
            raw_output_path.write_text(f"{benchmark_id}:{output_name}\n", encoding="utf-8")
            canonical_text_path.write_text(
                f"{benchmark_id}:{output_name}:reference\n",
                encoding="utf-8",
            )
            golden_outputs.append(
                {
                    "name": output_name,
                    "path": str(raw_output_path),
                    "canonical_text": str(canonical_text_path),
                    "canonical_sha256": output_hashes[output_name],
                }
            )

        write_json(
            golden_manifest_path,
            {
                "schema_version": 1,
                "benchmark_id": benchmark_id,
                "golden_output_dir": str(goldens_dir / "captured"),
                "outputs": golden_outputs,
            },
        )
        write_json(
            result_manifest_path,
            {
                "schema_version": 1,
                "benchmark_id": benchmark_id,
                "manifest": str(manifest_path),
                "golden_manifest": str(golden_manifest_path),
            },
        )
        write_json(
            comparison_summary_path,
            {
                "schema_version": 1,
                "benchmark_id": benchmark_id,
                "status": REFERENCE_CAPTURED,
                "reference_golden": str(golden_manifest_path),
                "latest_run_result": str(result_manifest_path),
            },
        )
        benchmark_summaries.append(
            {
                "benchmark_id": benchmark_id,
                "status": REFERENCE_CAPTURED,
                "backup_match_ok": True,
                "rerun_match_ok": True,
                "comparison_summary": str(comparison_summary_path),
                "golden_manifest": str(golden_manifest_path),
                "result_manifest": str(result_manifest_path),
            }
        )

    write_json(
        manifest_path,
        {
            "schema_version": 1,
            "run_id": "phase0-reference",
            "created_at_utc": "2026-04-22T00:00:00Z",
            "phase0": {
                "capture_state": capture_state,
                "capture_summary": str(summary_path),
                "captured_benchmarks": benchmark_ids,
            },
        },
    )
    summary: dict[str, Any] = {
        "schema_version": 1,
        "root": str(root),
        "manifest": str(manifest_path),
        "summary_path": str(summary_path),
        "capture_state": capture_state,
        "required_only": required_only,
        "selected_benchmarks": benchmark_ids,
        "benchmark_summaries": benchmark_summaries,
    }
    if optional_capture_packet:
        summary["optional_capture_packet"] = optional_capture_packet
    write_json(summary_path, summary)


def write_candidate_packet(*, root: Path, benchmarks: list[dict[str, Any]]) -> None:
    for benchmark in benchmarks:
        benchmark_id = normalize_string(benchmark["benchmark_id"], "candidate benchmark id")
        results_dir = root / "results" / "phase0" / benchmark_id
        primary_dir = results_dir / "primary"
        canonical_dir = primary_dir / "canonical"
        canonical_dir.mkdir(parents=True, exist_ok=True)

        primary_run_manifest_path = primary_dir / "run-manifest.json"
        result_manifest_path = results_dir / "result-manifest.json"
        outputs: list[dict[str, Any]] = []
        output_names = benchmark["output_names"]
        output_hashes = benchmark["output_hashes"]
        for output_name in output_names:
            raw_output_path = primary_dir / output_name
            canonical_text_path = canonical_dir / (output_name + ".canonical.txt")
            raw_output_path.write_text(f"{benchmark_id}:{output_name}\n", encoding="utf-8")
            canonical_text_path.write_text(
                f"{benchmark_id}:{output_name}:candidate\n",
                encoding="utf-8",
            )
            outputs.append(
                {
                    "name": output_name,
                    "path": str(raw_output_path),
                    "canonical_text": str(canonical_text_path),
                    "canonical_sha256": output_hashes[output_name],
                }
            )

        write_json(
            primary_run_manifest_path,
            {
                "schema_version": 1,
                "benchmark_id": benchmark_id,
                "label": "primary",
                "output_dir": str(primary_dir),
                "outputs": outputs,
            },
        )
        write_json(
            result_manifest_path,
            {
                "schema_version": 1,
                "benchmark_id": benchmark_id,
                "primary_run_manifest": str(primary_run_manifest_path),
            },
        )


def benchmark_spec(benchmark_id: str, first_char: str, second_char: str) -> dict[str, Any]:
    return {
        "benchmark_id": benchmark_id,
        "output_names": ["sol1", "sol2"],
        "output_hashes": {
            "sol1": first_char * 64,
            "sol2": second_char * 64,
        },
    }


def run_self_check(qualification_path: Path) -> dict[str, Any]:
    with tempfile.TemporaryDirectory(
        prefix="amflow-phase0-reference-packet-set-compare-self-check-"
    ) as tmp:
        temp_root = Path(tmp)
        required_reference_root = temp_root / "phase0-reference-captured-20260419-required-set"
        required_candidate_root = temp_root / "candidate-required-set"
        required_candidate_with_extra_root = temp_root / "candidate-required-set-extra"
        required_candidate_with_placeholder_root = temp_root / "candidate-required-set-placeholder"
        de_d0_reference_root = temp_root / "phase0-reference-captured-20260422-de-d0-pair"
        de_d0_candidate_root = temp_root / "candidate-de-d0-pair"
        user_hook_reference_root = temp_root / "phase0-reference-captured-20260422-user-hook-pair"
        user_hook_candidate_root = temp_root / "candidate-user-hook-pair"
        summary_path = temp_root / "packet-set-summary.json"

        required_benchmarks = [
            benchmark_spec("automatic_loop", "1", "2"),
            benchmark_spec("automatic_vs_manual", "3", "4"),
        ]
        de_d0_benchmarks = [
            benchmark_spec("differential_equation_solver", "5", "6"),
            benchmark_spec("spacetime_dimension", "7", "8"),
        ]
        user_hook_benchmarks = [
            benchmark_spec("user_defined_amfmode", "9", "a"),
            benchmark_spec("user_defined_ending", "b", "c"),
        ]

        write_reference_packet(
            root=required_reference_root,
            benchmarks=required_benchmarks,
            capture_state=REFERENCE_CAPTURED,
            required_only=True,
        )
        write_candidate_packet(root=required_candidate_root, benchmarks=required_benchmarks)
        write_candidate_packet(
            root=required_candidate_with_extra_root,
            benchmarks=required_benchmarks + [benchmark_spec("user_defined_amfmode", "d", "e")],
        )
        write_candidate_packet(
            root=required_candidate_with_placeholder_root,
            benchmarks=required_benchmarks,
        )
        (
            required_candidate_with_placeholder_root
            / "results"
            / "phase0"
            / "automatic_phasespace"
        ).mkdir(parents=True, exist_ok=True)
        write_reference_packet(
            root=de_d0_reference_root,
            benchmarks=de_d0_benchmarks,
            capture_state=BOOTSTRAP_ONLY,
            required_only=False,
            optional_capture_packet="de-d0-pair",
        )
        write_candidate_packet(root=de_d0_candidate_root, benchmarks=de_d0_benchmarks)
        write_reference_packet(
            root=user_hook_reference_root,
            benchmarks=user_hook_benchmarks,
            capture_state=BOOTSTRAP_ONLY,
            required_only=False,
            optional_capture_packet="user-hook-pair",
        )
        write_candidate_packet(root=user_hook_candidate_root, benchmarks=user_hook_benchmarks)

        summary = compare_phase0_packet_set_to_reference(
            packet_root_pairs=[
                (user_hook_reference_root, user_hook_candidate_root),
                (required_reference_root, required_candidate_root),
                (de_d0_reference_root, de_d0_candidate_root),
            ],
            qualification_path=qualification_path,
        )
        write_json(summary_path, summary)

        missing_packet_rejected = False
        try:
            compare_phase0_packet_set_to_reference(
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
            placeholder_summary = compare_phase0_packet_set_to_reference(
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
            compare_phase0_packet_set_to_reference(
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
            compare_phase0_packet_set_to_reference(
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

        first_benchmark = summary["benchmarks"][0]
        return {
            "matching_packet_set_matches_reference": (
                summary["required_packet_present"]
                and summary["reference_packet_labels_match_scaffold_reference_captured"]
                and summary["compared_phase0_ids_match_scaffold_reference_captured"]
                and summary["candidate_output_hashes_match_reference"]
            ),
            "required_packet_present": summary["required_packet_present"],
            "profiles_reported_from_scaffold": (
                first_benchmark["digit_threshold_profile"] == "core-package-family-default"
                and first_benchmark["minimum_correct_digits"] == 50
                and "insufficient_precision" in first_benchmark["required_failure_codes"]
                and "unexpected master sets in Kira interface"
                in first_benchmark["known_regression_families"]
            ),
            "missing_packet_rejected": missing_packet_rejected,
            "placeholder_directories_ignored": placeholder_directories_ignored,
            "extra_candidate_benchmark_rejected": extra_candidate_benchmark_rejected,
            "duplicate_packet_label_rejected": duplicate_packet_label_rejected,
            "malformed_packet_pair_rejected": malformed_packet_pair_rejected,
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
        help="Optional output file for the aggregated comparison summary",
    )
    parser.add_argument(
        "--self-check",
        action="store_true",
        help="Run a synthetic packet-set comparison self-check",
    )
    return parser.parse_args()


def require_packet_root_pairs(raw_pairs: list[str] | None) -> list[tuple[Path, Path]]:
    expect(raw_pairs, "--packet-root-pair is required unless --self-check is set")
    return [parse_packet_root_pair(raw_pair) for raw_pair in raw_pairs]


def main() -> int:
    args = parse_args()
    qualification_path = (
        args.qualification_path
        if args.qualification_path is not None
        else repo_root() / "tools" / "reference-harness" / "templates" / "qualification-benchmarks.json"
    )

    if args.self_check:
        print(json.dumps(run_self_check(qualification_path), indent=2, sort_keys=True))
        return 0

    packet_root_pairs = require_packet_root_pairs(args.packet_root_pair)
    summary = compare_phase0_packet_set_to_reference(
        packet_root_pairs=packet_root_pairs,
        qualification_path=qualification_path,
    )
    if args.summary_path is not None:
        write_json(args.summary_path, summary)
    print(json.dumps(summary, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
