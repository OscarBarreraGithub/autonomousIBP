#!/usr/bin/env python3
"""Aggregate phase-0 failure-code audits across the retained packet split."""

from __future__ import annotations

import argparse
import json
import shutil
import tempfile
from pathlib import Path
from typing import Any

from audit_phase0_failure_codes import (
    audit_phase0_failure_codes,
    rewrite_candidate_packet_metadata,
    require_candidate_root,
    write_candidate_packet_metadata,
    write_failure_code_audit,
)
from compare_phase0_packet_set_to_reference import (
    REQUIRED_SET_LABEL,
    benchmark_spec,
    discover_candidate_benchmark_ids,
    expected_packet_label,
    normalize_string,
    write_candidate_packet,
)
from compare_phase0_results_to_reference import (
    BOOTSTRAP_ONLY,
    REFERENCE_CAPTURED,
    load_phase0_scaffold,
    repo_root,
    rewrite_candidate_result_manifest_pointer,
    write_json,
)


def require_candidate_roots(candidate_roots: list[Path] | None) -> list[Path]:
    if not candidate_roots:
        raise RuntimeError("--candidate-root is required unless --self-check is set")
    return [require_candidate_root(candidate_root) for candidate_root in candidate_roots]


def audit_phase0_packet_set_failure_codes(
    *,
    candidate_roots: list[Path],
    qualification_path: Path,
) -> dict[str, Any]:
    if not candidate_roots:
        raise RuntimeError("--candidate-root is required unless --self-check is set")

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
    packet_audits: list[dict[str, Any]] = []
    benchmark_summaries: list[dict[str, Any]] = []

    for candidate_root in candidate_roots:
        packet_summary = audit_phase0_failure_codes(
            candidate_root=candidate_root,
            benchmark_ids=None,
            qualification_path=qualification_path,
        )
        packet_label = normalize_string(
            packet_summary["candidate_packet_label"], "candidate packet label"
        )
        if packet_label in seen_packet_labels:
            raise RuntimeError(f"duplicate candidate packet label across packet roots: {packet_label}")
        seen_packet_labels.add(packet_label)

        raw_selected_benchmark_ids = packet_summary.get("selected_benchmark_ids", [])
        if not isinstance(raw_selected_benchmark_ids, list):
            raise TypeError("selected_benchmark_ids must be a list")
        selected_benchmark_ids = [
            normalize_string(value, f"{packet_label} selected benchmark id")
            for value in raw_selected_benchmark_ids
        ]
        if not selected_benchmark_ids:
            raise RuntimeError(f"candidate packet {packet_label} must audit at least one benchmark")
        if len(set(selected_benchmark_ids)) != len(selected_benchmark_ids):
            raise RuntimeError(f"selected benchmark ids for {packet_label} must not contain duplicates")

        candidate_benchmark_ids = discover_candidate_benchmark_ids(candidate_root)
        if set(candidate_benchmark_ids) != set(selected_benchmark_ids):
            raise RuntimeError(
                "candidate packet benchmark ids for "
                f"{packet_label} must match the packet summary benchmark set"
            )

        raw_benchmarks = packet_summary.get("benchmarks", [])
        if not isinstance(raw_benchmarks, list):
            raise TypeError(f"{packet_label} benchmarks must be a list")
        packet_benchmark_ids: list[str] = []
        packet_missing_required_failure_codes: set[str] = set()
        for benchmark_summary in raw_benchmarks:
            if not isinstance(benchmark_summary, dict):
                raise TypeError(f"{packet_label} benchmark summaries must be objects")
            benchmark_id = normalize_string(
                benchmark_summary.get("benchmark_id", ""),
                f"{packet_label} benchmark_id",
            )
            if benchmark_id not in scaffold_entries:
                raise RuntimeError(
                    f"audited benchmark {benchmark_id!r} is not present in the qualification scaffold"
                )
            if benchmark_id in seen_benchmark_ids:
                raise RuntimeError(
                    f"benchmark {benchmark_id} was audited more than once across candidate packet roots"
                )
            if packet_label != expected_packet_label(scaffold_entries[benchmark_id]):
                raise RuntimeError(
                    f"candidate packet label {packet_label} must match the scaffold packet label for "
                    f"{benchmark_id}"
                )
            raw_missing_required_failure_codes = benchmark_summary.get(
                "missing_required_failure_codes", []
            )
            if not isinstance(raw_missing_required_failure_codes, list):
                raise TypeError(
                    f"{packet_label}:{benchmark_id} missing_required_failure_codes must be a list"
                )
            packet_missing_required_failure_codes.update(
                normalize_string(
                    value, f"{packet_label}:{benchmark_id} missing required failure code"
                )
                for value in raw_missing_required_failure_codes
            )
            seen_benchmark_ids.add(benchmark_id)
            packet_benchmark_ids.append(benchmark_id)
            benchmark_summaries.append(dict(benchmark_summary))

        if set(packet_benchmark_ids) != set(selected_benchmark_ids):
            raise RuntimeError(
                f"packet summary benchmarks for {packet_label} must match selected_benchmark_ids"
            )

        packet_audits.append(
            {
                "candidate_root": str(candidate_root),
                "candidate_packet_label": packet_label,
                "candidate_capture_state": packet_summary["candidate_capture_state"],
                "candidate_benchmark_ids": sorted(candidate_benchmark_ids),
                "selected_benchmark_ids": sorted(selected_benchmark_ids),
                "candidate_packet_benchmark_ids_match_packet_summary": True,
                "all_selected_benchmarks_publish_failure_code_audits": packet_summary[
                    "all_selected_benchmarks_publish_failure_code_audits"
                ],
                "all_selected_benchmarks_report_required_failure_codes": packet_summary[
                    "all_selected_benchmarks_report_required_failure_codes"
                ],
                "any_selected_benchmarks_report_unexpected_failure_codes": packet_summary[
                    "any_selected_benchmarks_report_unexpected_failure_codes"
                ],
                "missing_required_failure_codes_across_selection": sorted(
                    packet_missing_required_failure_codes
                ),
            }
        )

    if REQUIRED_SET_LABEL not in seen_packet_labels:
        raise RuntimeError(f"candidate packet labels must include {REQUIRED_SET_LABEL}")
    if seen_packet_labels != set(expected_packet_labels):
        raise RuntimeError(
            "candidate packet labels must match the scaffold reference-captured packet set"
        )
    if seen_benchmark_ids != set(expected_benchmark_ids):
        raise RuntimeError(
            "audited phase-0 benchmark ids must match the scaffold reference-captured set"
        )

    packet_audits.sort(key=lambda item: item["candidate_packet_label"])
    benchmark_summaries.sort(key=lambda item: str(item.get("benchmark_id", "")).strip())

    missing_required_failure_codes_across_packet_set = sorted(
        {
            code
            for benchmark in benchmark_summaries
            for code in benchmark["missing_required_failure_codes"]
        }
    )

    return {
        "schema_version": 1,
        "qualification_path": str(qualification_path),
        "candidate_packet_count": len(packet_audits),
        "candidate_packet_labels": [item["candidate_packet_label"] for item in packet_audits],
        "expected_reference_captured_packet_labels": expected_packet_labels,
        "required_packet_present": REQUIRED_SET_LABEL in seen_packet_labels,
        "audited_phase0_ids": sorted(seen_benchmark_ids),
        "expected_reference_captured_phase0_ids": expected_benchmark_ids,
        "candidate_packet_labels_match_scaffold_reference_captured": True,
        "audited_phase0_ids_match_scaffold_reference_captured": True,
        "candidate_packet_benchmark_sets_match_packet_summaries": all(
            item["candidate_packet_benchmark_ids_match_packet_summary"]
            for item in packet_audits
        ),
        "all_compared_benchmarks_publish_failure_code_audits": all(
            benchmark["failure_code_audit_present"] for benchmark in benchmark_summaries
        ),
        "all_compared_benchmarks_report_required_failure_codes": all(
            not benchmark["missing_required_failure_codes"] for benchmark in benchmark_summaries
        ),
        "any_compared_benchmarks_report_unexpected_failure_codes": any(
            bool(benchmark["unexpected_failure_codes"]) for benchmark in benchmark_summaries
        ),
        "missing_required_failure_codes_across_packet_set": (
            missing_required_failure_codes_across_packet_set
        ),
        "digit_threshold_profiles_reported": True,
        "required_failure_code_profiles_reported": True,
        "regression_profiles_reported": True,
        "packet_audits": packet_audits,
        "benchmarks": benchmark_summaries,
    }


def seed_candidate_packet(
    *,
    root: Path,
    benchmarks: list[dict[str, Any]],
    packet_label: str,
    capture_state: str,
) -> list[str]:
    write_candidate_packet(root=root, benchmarks=benchmarks)
    benchmark_ids = [
        normalize_string(item["benchmark_id"], "candidate benchmark id") for item in benchmarks
    ]
    write_candidate_packet_metadata(
        root=root,
        benchmark_id=benchmark_ids[0],
        packet_label=packet_label,
        capture_state=capture_state,
    )
    rewrite_candidate_packet_metadata(root, benchmark_ids)
    for benchmark_id in benchmark_ids:
        rewrite_candidate_result_manifest_pointer(root, benchmark_id)
    return benchmark_ids


def write_matching_failure_code_audits(
    *,
    root: Path,
    benchmark_ids: list[str],
    qualification_path: Path,
    extra_failure_codes_by_benchmark: dict[str, list[str]] | None = None,
) -> None:
    scaffold_entries = load_phase0_scaffold(qualification_path)
    for benchmark_id in benchmark_ids:
        required_failure_codes = sorted(scaffold_entries[benchmark_id]["required_failure_codes"])
        extra_failure_codes = []
        if extra_failure_codes_by_benchmark is not None:
            extra_failure_codes = extra_failure_codes_by_benchmark.get(benchmark_id, [])
        write_failure_code_audit(
            root=root,
            benchmark_id=benchmark_id,
            observed_failure_codes=required_failure_codes + extra_failure_codes,
        )


def run_self_check(qualification_path: Path) -> dict[str, Any]:
    with tempfile.TemporaryDirectory(
        prefix="amflow-phase0-failure-code-audit-packet-set-self-check-"
    ) as tmp:
        temp_root = Path(tmp)
        summary_path = temp_root / "packet-set-failure-code-audit-summary.json"

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

        required_candidate_root = temp_root / "candidate-required-set"
        required_candidate_with_extra_root = temp_root / "candidate-required-set-extra"
        required_candidate_with_placeholder_root = temp_root / "candidate-required-set-placeholder"
        de_d0_candidate_root = temp_root / "candidate-de-d0-pair"
        de_d0_candidate_incomplete_root = temp_root / "candidate-de-d0-pair-incomplete"
        user_hook_candidate_root = temp_root / "candidate-user-hook-pair"
        user_hook_candidate_missing_audit_root = temp_root / "candidate-user-hook-pair-missing-audit"

        required_benchmark_ids = seed_candidate_packet(
            root=required_candidate_root,
            benchmarks=required_benchmarks,
            packet_label=REQUIRED_SET_LABEL,
            capture_state=REFERENCE_CAPTURED,
        )
        write_matching_failure_code_audits(
            root=required_candidate_root,
            benchmark_ids=required_benchmark_ids,
            qualification_path=qualification_path,
            extra_failure_codes_by_benchmark={
                "automatic_loop": ["unsupported_solver_path"],
            },
        )

        seed_candidate_packet(
            root=required_candidate_with_extra_root,
            benchmarks=required_benchmarks,
            packet_label=REQUIRED_SET_LABEL,
            capture_state=REFERENCE_CAPTURED,
        )
        write_candidate_packet(
            root=required_candidate_with_extra_root,
            benchmarks=[benchmark_spec("user_defined_amfmode", "d", "e")],
        )
        write_matching_failure_code_audits(
            root=required_candidate_with_extra_root,
            benchmark_ids=required_benchmark_ids,
            qualification_path=qualification_path,
        )

        seed_candidate_packet(
            root=required_candidate_with_placeholder_root,
            benchmarks=required_benchmarks,
            packet_label=REQUIRED_SET_LABEL,
            capture_state=REFERENCE_CAPTURED,
        )
        (
            required_candidate_with_placeholder_root
            / "results"
            / "phase0"
            / "automatic_phasespace"
        ).mkdir(parents=True, exist_ok=True)
        write_matching_failure_code_audits(
            root=required_candidate_with_placeholder_root,
            benchmark_ids=required_benchmark_ids,
            qualification_path=qualification_path,
        )

        de_d0_benchmark_ids = seed_candidate_packet(
            root=de_d0_candidate_root,
            benchmarks=de_d0_benchmarks,
            packet_label="de-d0-pair",
            capture_state=BOOTSTRAP_ONLY,
        )
        write_matching_failure_code_audits(
            root=de_d0_candidate_root,
            benchmark_ids=de_d0_benchmark_ids,
            qualification_path=qualification_path,
        )

        shutil.copytree(de_d0_candidate_root, de_d0_candidate_incomplete_root)
        rewrite_candidate_packet_metadata(de_d0_candidate_incomplete_root, de_d0_benchmark_ids)
        for benchmark_id in de_d0_benchmark_ids:
            rewrite_candidate_result_manifest_pointer(de_d0_candidate_incomplete_root, benchmark_id)
        write_failure_code_audit(
            root=de_d0_candidate_incomplete_root,
            benchmark_id="differential_equation_solver",
            observed_failure_codes=["insufficient_precision"],
        )

        user_hook_benchmark_ids = seed_candidate_packet(
            root=user_hook_candidate_root,
            benchmarks=user_hook_benchmarks,
            packet_label="user-hook-pair",
            capture_state=BOOTSTRAP_ONLY,
        )
        write_matching_failure_code_audits(
            root=user_hook_candidate_root,
            benchmark_ids=user_hook_benchmark_ids,
            qualification_path=qualification_path,
        )

        shutil.copytree(user_hook_candidate_root, user_hook_candidate_missing_audit_root)
        rewrite_candidate_packet_metadata(user_hook_candidate_missing_audit_root, user_hook_benchmark_ids)
        for benchmark_id in user_hook_benchmark_ids:
            rewrite_candidate_result_manifest_pointer(
                user_hook_candidate_missing_audit_root, benchmark_id
            )
        (
            user_hook_candidate_missing_audit_root
            / "results"
            / "phase0"
            / "user_defined_amfmode"
            / "failure-code-audit.json"
        ).unlink()

        matching_summary = audit_phase0_packet_set_failure_codes(
            candidate_roots=[
                user_hook_candidate_root,
                required_candidate_root,
                de_d0_candidate_root,
            ],
            qualification_path=qualification_path,
        )
        write_json(summary_path, matching_summary)

        missing_audit_summary = audit_phase0_packet_set_failure_codes(
            candidate_roots=[
                user_hook_candidate_missing_audit_root,
                required_candidate_root,
                de_d0_candidate_root,
            ],
            qualification_path=qualification_path,
        )

        incomplete_audit_summary = audit_phase0_packet_set_failure_codes(
            candidate_roots=[
                user_hook_candidate_root,
                required_candidate_root,
                de_d0_candidate_incomplete_root,
            ],
            qualification_path=qualification_path,
        )

        missing_packet_rejected = False
        try:
            audit_phase0_packet_set_failure_codes(
                candidate_roots=[
                    required_candidate_root,
                    de_d0_candidate_root,
                ],
                qualification_path=qualification_path,
            )
        except RuntimeError as error:
            missing_packet_rejected = (
                "must match the scaffold reference-captured packet set" in str(error)
            )

        placeholder_directories_ignored = False
        try:
            placeholder_summary = audit_phase0_packet_set_failure_codes(
                candidate_roots=[
                    user_hook_candidate_root,
                    required_candidate_with_placeholder_root,
                    de_d0_candidate_root,
                ],
                qualification_path=qualification_path,
            )
            placeholder_directories_ignored = placeholder_summary[
                "candidate_packet_benchmark_sets_match_packet_summaries"
            ]
        except RuntimeError:
            placeholder_directories_ignored = False

        extra_candidate_benchmark_rejected = False
        try:
            audit_phase0_packet_set_failure_codes(
                candidate_roots=[
                    user_hook_candidate_root,
                    required_candidate_with_extra_root,
                    de_d0_candidate_root,
                ],
                qualification_path=qualification_path,
            )
        except RuntimeError as error:
            extra_candidate_benchmark_rejected = (
                "candidate packet benchmark ids for required-set must match" in str(error)
            )

        duplicate_packet_label_rejected = False
        try:
            audit_phase0_packet_set_failure_codes(
                candidate_roots=[
                    user_hook_candidate_root,
                    required_candidate_root,
                    required_candidate_root,
                    de_d0_candidate_root,
                ],
                qualification_path=qualification_path,
            )
        except RuntimeError as error:
            duplicate_packet_label_rejected = "duplicate candidate packet label" in str(error)

        missing_candidate_root_rejected = False
        try:
            require_candidate_roots(None)
        except RuntimeError as error:
            missing_candidate_root_rejected = "--candidate-root is required" in str(error)

        matching_benchmark = next(
            benchmark
            for benchmark in matching_summary["benchmarks"]
            if benchmark["benchmark_id"] == "automatic_loop"
        )
        missing_audit_benchmark = next(
            benchmark
            for benchmark in missing_audit_summary["benchmarks"]
            if benchmark["benchmark_id"] == "user_defined_amfmode"
        )
        incomplete_audit_benchmark = next(
            benchmark
            for benchmark in incomplete_audit_summary["benchmarks"]
            if benchmark["benchmark_id"] == "differential_equation_solver"
        )
        return {
            "matching_packet_set_reports_required_failure_codes": (
                matching_summary["required_packet_present"]
                and matching_summary["candidate_packet_labels_match_scaffold_reference_captured"]
                and matching_summary["audited_phase0_ids_match_scaffold_reference_captured"]
                and matching_summary["all_compared_benchmarks_publish_failure_code_audits"]
                and matching_summary["all_compared_benchmarks_report_required_failure_codes"]
            ),
            "required_packet_present": matching_summary["required_packet_present"],
            "profiles_reported_from_scaffold": (
                matching_benchmark["digit_threshold_profile"] == "core-package-family-default"
                and matching_benchmark["minimum_correct_digits"] == 50
                and "insufficient_precision" in matching_benchmark["required_failure_codes"]
                and "unexpected master sets in Kira interface"
                in matching_benchmark["known_regression_families"]
            ),
            "unexpected_failure_codes_reported": (
                matching_summary["any_compared_benchmarks_report_unexpected_failure_codes"]
                and "unsupported_solver_path" in matching_benchmark["unexpected_failure_codes"]
            ),
            "missing_failure_code_audit_reported": (
                not missing_audit_summary["all_compared_benchmarks_publish_failure_code_audits"]
                and missing_audit_benchmark["status"] == "failure-code-audit-missing"
                and "insufficient_precision"
                in missing_audit_benchmark["missing_required_failure_codes"]
            ),
            "incomplete_failure_code_audit_reported": (
                not incomplete_audit_summary["all_compared_benchmarks_report_required_failure_codes"]
                and incomplete_audit_benchmark["status"] == "required-failure-codes-incomplete"
                and "boundary_unsolved"
                in incomplete_audit_benchmark["missing_required_failure_codes"]
            ),
            "missing_packet_rejected": missing_packet_rejected,
            "placeholder_directories_ignored": placeholder_directories_ignored,
            "extra_candidate_benchmark_rejected": extra_candidate_benchmark_rejected,
            "duplicate_packet_label_rejected": duplicate_packet_label_rejected,
            "missing_candidate_root_rejected": missing_candidate_root_rejected,
            "summary_written": summary_path.exists(),
        }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--candidate-root",
        action="append",
        type=Path,
        help="Candidate packet root that publishes retained phase-0 manifests and failure-code audits",
    )
    parser.add_argument(
        "--qualification-path",
        type=Path,
        help="Qualification scaffold JSON path",
    )
    parser.add_argument(
        "--summary-path",
        type=Path,
        help="Optional output file for the aggregated failure-code audit summary",
    )
    parser.add_argument(
        "--self-check",
        action="store_true",
        help="Run a synthetic packet-set failure-code audit self-check",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    qualification_path = (
        args.qualification_path
        if args.qualification_path is not None
        else repo_root() / "tools" / "reference-harness" / "templates" / "qualification-benchmarks.json"
    )

    if args.self_check:
        summary = run_self_check(qualification_path)
        if args.summary_path is not None:
            write_json(args.summary_path, summary)
        print(json.dumps(summary, indent=2, sort_keys=True))
        return 0

    candidate_roots = require_candidate_roots(args.candidate_root)
    summary = audit_phase0_packet_set_failure_codes(
        candidate_roots=candidate_roots,
        qualification_path=qualification_path,
    )
    if args.summary_path is not None:
        write_json(args.summary_path, summary)
    print(json.dumps(summary, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
