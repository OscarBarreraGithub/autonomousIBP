#!/usr/bin/env python3
"""Audit candidate phase-0 failure-code coverage against the M6 qualification scaffold."""

from __future__ import annotations

import argparse
import json
import shutil
import tempfile
from pathlib import Path
from typing import Any

from compare_phase0_results_to_reference import (
    infer_packet_label,
    load_phase0_scaffold,
    locate_capture_summary,
    locate_manifest,
    repo_root,
    rewrite_candidate_result_manifest_pointer,
    write_candidate_packet,
    write_json,
)
from freeze_phase0_goldens import load_json


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def normalize_string(raw: Any, label: str) -> str:
    if not isinstance(raw, str):
        raise TypeError(f"{label} must be a string, got {type(raw).__name__}")
    value = raw.strip()
    expect(value, f"{label} must not be empty")
    return value


def normalize_benchmark_id(raw: Any, label: str) -> str:
    value = normalize_string(raw, label)
    expect(
        "/" not in value and "\\" not in value and value not in {".", ".."},
        f"{label} must be a plain benchmark id",
    )
    return value


def normalize_string_list(raw: Any, label: str) -> list[str]:
    if not isinstance(raw, list):
        raise TypeError(f"{label} must be a list, got {type(raw).__name__}")
    values: list[str] = []
    for item in raw:
        if not isinstance(item, str):
            raise TypeError(f"{label} entries must be strings, got {type(item).__name__}")
        value = item.strip()
        expect(value, f"{label} entries must not be empty")
        values.append(value)
    return values


def expect_unique(values: list[str], label: str) -> None:
    expect(len(set(values)) == len(values), f"{label} must not contain duplicates")


def expect_path_within_root(path: Path, root: Path, label: str) -> None:
    resolved_path = path.resolve(strict=False)
    resolved_root = root.resolve(strict=False)
    try:
        resolved_path.relative_to(resolved_root)
    except ValueError as error:
        raise RuntimeError(f"{label} must stay under {root}: {path}") from error


def require_candidate_root(candidate_root: Path | None) -> Path:
    expect(candidate_root is not None, "--candidate-root is required unless --self-check is set")
    return candidate_root


def select_candidate_benchmark_ids(
    *,
    candidate_root: Path,
    candidate_packet_benchmark_ids: list[str],
    benchmark_ids: list[str] | None,
) -> list[str]:
    if not benchmark_ids:
        selected_ids = list(candidate_packet_benchmark_ids)
    else:
        selected_ids = [normalize_benchmark_id(value, "--benchmark-id") for value in benchmark_ids]
        expect_unique(selected_ids, "--benchmark-id")
        undeclared = [
            benchmark_id
            for benchmark_id in selected_ids
            if benchmark_id not in set(candidate_packet_benchmark_ids)
        ]
        expect(
            not undeclared,
            "candidate capture summary does not declare benchmark ids: " + ", ".join(undeclared),
        )

    missing = [
        benchmark_id
        for benchmark_id in selected_ids
        if not (candidate_root / "results" / "phase0" / benchmark_id / "result-manifest.json").exists()
    ]
    expect(
        not missing,
        "candidate result manifest missing for benchmark ids: " + ", ".join(missing),
    )
    return selected_ids


def load_candidate_packet_metadata(candidate_root: Path) -> dict[str, Any]:
    expect(candidate_root.is_dir(), f"candidate root must exist: {candidate_root}")
    manifest_path = locate_manifest(candidate_root)
    expect_path_within_root(manifest_path, candidate_root, "candidate manifest path")
    manifest = load_json(manifest_path)
    summary_path = locate_capture_summary(candidate_root, manifest)
    expect_path_within_root(summary_path, candidate_root, "candidate capture summary path")
    summary = load_json(summary_path)

    capture_state = normalize_string(
        manifest.get("phase0", {}).get("capture_state", ""),
        "candidate manifest capture_state",
    )
    summary_capture_state = normalize_string(
        summary.get("capture_state", ""),
        "candidate capture summary capture_state",
    )
    expect(
        summary_capture_state == capture_state,
        "candidate capture summary capture_state must match the manifest capture_state",
    )
    raw_selected_benchmark_ids = summary.get("selected_benchmarks", [])
    if not isinstance(raw_selected_benchmark_ids, list):
        raise TypeError("candidate capture summary selected_benchmarks must be a list")
    selected_benchmark_ids = [
        normalize_benchmark_id(value, "candidate capture summary selected_benchmarks entry")
        for value in raw_selected_benchmark_ids
    ]
    expect(
        selected_benchmark_ids,
        "candidate capture summary selected_benchmarks must not be empty",
    )
    expect_unique(
        selected_benchmark_ids,
        "candidate capture summary selected_benchmarks",
    )

    raw_benchmark_summaries = summary.get("benchmark_summaries", [])
    if not isinstance(raw_benchmark_summaries, list):
        raise TypeError("candidate capture summary benchmark_summaries must be a list")
    published_benchmark_ids: list[str] = []
    for benchmark_summary in raw_benchmark_summaries:
        if not isinstance(benchmark_summary, dict):
            raise TypeError("candidate capture summary benchmark_summaries entries must be objects")
        benchmark_id = normalize_benchmark_id(
            benchmark_summary.get("benchmark_id", ""),
            "candidate capture summary benchmark_summaries benchmark_id",
        )
        result_manifest_path = Path(
            normalize_string(
                benchmark_summary.get("result_manifest", ""),
                f"{benchmark_id} candidate capture summary result_manifest",
            )
        )
        expect_path_within_root(
            result_manifest_path,
            candidate_root,
            f"candidate capture summary result manifest path for {benchmark_id}",
        )
        published_benchmark_ids.append(benchmark_id)
    expect_unique(
        published_benchmark_ids,
        "candidate capture summary benchmark_summaries benchmark_id",
    )
    expect(
        set(published_benchmark_ids) == set(selected_benchmark_ids),
        "candidate capture summary benchmark_summaries must match selected_benchmarks",
    )

    return {
        "manifest_path": str(manifest_path),
        "summary_path": str(summary_path),
        "capture_state": capture_state,
        "packet_label": infer_packet_label(candidate_root, summary),
        "selected_benchmark_ids": selected_benchmark_ids,
    }


def validate_candidate_packet_benchmark(
    *,
    candidate_root: Path,
    benchmark_id: str,
) -> None:
    result_manifest_path = candidate_root / "results" / "phase0" / benchmark_id / "result-manifest.json"
    expect(
        result_manifest_path.exists(),
        f"candidate result manifest missing for {benchmark_id} in {candidate_root}",
    )
    expect_path_within_root(
        result_manifest_path,
        candidate_root,
        f"candidate result manifest path for {benchmark_id}",
    )
    result_manifest = load_json(result_manifest_path)
    expect(
        normalize_string(result_manifest.get("benchmark_id", ""), f"{benchmark_id} result_manifest benchmark_id")
        == benchmark_id,
        f"candidate result manifest benchmark_id must match {benchmark_id}",
    )

    primary_run_manifest_path = Path(
        normalize_string(
            result_manifest.get("primary_run_manifest", ""),
            f"{benchmark_id} primary_run_manifest",
        )
    )
    expect_path_within_root(
        primary_run_manifest_path,
        candidate_root,
        f"candidate primary run manifest path for {benchmark_id}",
    )
    expect(
        primary_run_manifest_path.exists(),
        f"candidate primary run manifest missing for {benchmark_id} in {candidate_root}",
    )

    primary_run_manifest = load_json(primary_run_manifest_path)
    expect(
        normalize_string(
            primary_run_manifest.get("benchmark_id", ""),
            f"{benchmark_id} primary run manifest benchmark_id",
        )
        == benchmark_id,
        f"candidate primary run manifest benchmark_id must match {benchmark_id}",
    )
    expect(
        normalize_string(
            primary_run_manifest.get("label", ""),
            f"{benchmark_id} primary run manifest label",
        )
        == "primary",
        f"candidate primary run manifest label must be primary for {benchmark_id}",
    )


def load_failure_code_audit(
    *,
    candidate_root: Path,
    benchmark_id: str,
) -> tuple[Path, list[str] | None]:
    audit_path = candidate_root / "results" / "phase0" / benchmark_id / "failure-code-audit.json"
    if not audit_path.exists():
        return audit_path, None

    expect_path_within_root(audit_path, candidate_root, f"failure-code audit path for {benchmark_id}")
    payload = load_json(audit_path)
    if "schema_version" in payload:
        expect(
            payload["schema_version"] == 1,
            f"failure-code audit schema_version must be 1 for {benchmark_id}",
        )
    expect(
        normalize_string(payload.get("benchmark_id", ""), f"{benchmark_id} failure-code audit benchmark_id")
        == benchmark_id,
        f"failure-code audit benchmark_id must match {benchmark_id}",
    )
    expect(
        "observed_failure_codes" in payload,
        f"failure-code audit observed_failure_codes must be present for {benchmark_id}",
    )
    observed_failure_codes = normalize_string_list(
        payload["observed_failure_codes"],
        f"{benchmark_id} observed_failure_codes",
    )
    expect_unique(observed_failure_codes, f"{benchmark_id} observed_failure_codes")
    return audit_path, observed_failure_codes


def audit_phase0_failure_codes(
    *,
    candidate_root: Path,
    benchmark_ids: list[str] | None,
    qualification_path: Path,
) -> dict[str, Any]:
    packet_metadata = load_candidate_packet_metadata(candidate_root)
    scaffold_entries = load_phase0_scaffold(qualification_path)
    selected_benchmark_ids = select_candidate_benchmark_ids(
        candidate_root=candidate_root,
        candidate_packet_benchmark_ids=packet_metadata["selected_benchmark_ids"],
        benchmark_ids=benchmark_ids,
    )

    benchmark_summaries: list[dict[str, Any]] = []
    missing_required_codes_across_selection: set[str] = set()

    for benchmark_id in selected_benchmark_ids:
        expect(
            benchmark_id in scaffold_entries,
            f"candidate benchmark {benchmark_id!r} is not present in the qualification scaffold",
        )
        validate_candidate_packet_benchmark(candidate_root=candidate_root, benchmark_id=benchmark_id)
        scaffold_entry = scaffold_entries[benchmark_id]
        audit_path, observed_failure_codes = load_failure_code_audit(
            candidate_root=candidate_root,
            benchmark_id=benchmark_id,
        )
        required_failure_codes = sorted(scaffold_entry["required_failure_codes"])
        observed = sorted(observed_failure_codes or [])
        missing_required_failure_codes = sorted(
            set(required_failure_codes) - set(observed)
        )
        unexpected_failure_codes = sorted(set(observed) - set(required_failure_codes))
        missing_required_codes_across_selection.update(missing_required_failure_codes)

        status = "failure-code-audit-missing"
        if observed_failure_codes is not None:
            status = (
                "required-failure-codes-complete"
                if not missing_required_failure_codes
                else "required-failure-codes-incomplete"
            )

        benchmark_summaries.append(
            {
                "benchmark_id": benchmark_id,
                "failure_code_audit_path": str(audit_path),
                "failure_code_audit_present": observed_failure_codes is not None,
                "status": status,
                "digit_threshold_profile": scaffold_entry["digit_threshold_profile"],
                "minimum_correct_digits": scaffold_entry["minimum_correct_digits"],
                "failure_code_profile": scaffold_entry["failure_code_profile"],
                "required_failure_codes": required_failure_codes,
                "observed_failure_codes": observed,
                "missing_required_failure_codes": missing_required_failure_codes,
                "unexpected_failure_codes": unexpected_failure_codes,
                "regression_profile": scaffold_entry["regression_profile"],
                "known_regression_families": scaffold_entry["known_regression_families"],
            }
        )

    benchmark_summaries.sort(key=lambda item: item["benchmark_id"])

    return {
        "schema_version": 1,
        "qualification_path": str(qualification_path),
        "candidate_root": str(candidate_root),
        "candidate_packet_label": packet_metadata["packet_label"],
        "candidate_capture_state": packet_metadata["capture_state"],
        "selected_benchmark_ids": selected_benchmark_ids,
        "all_selected_benchmarks_publish_failure_code_audits": all(
            benchmark["failure_code_audit_present"] for benchmark in benchmark_summaries
        ),
        "all_selected_benchmarks_report_required_failure_codes": all(
            not benchmark["missing_required_failure_codes"] for benchmark in benchmark_summaries
        ),
        "any_selected_benchmarks_report_unexpected_failure_codes": any(
            bool(benchmark["unexpected_failure_codes"]) for benchmark in benchmark_summaries
        ),
        "missing_required_failure_codes_across_selection": sorted(
            missing_required_codes_across_selection
        ),
        "digit_threshold_profiles_reported": True,
        "required_failure_code_profiles_reported": True,
        "regression_profiles_reported": True,
        "benchmarks": benchmark_summaries,
    }


def write_candidate_packet_metadata(
    *,
    root: Path,
    benchmark_id: str,
    packet_label: str,
    capture_state: str,
) -> None:
    manifests_dir = root / "manifests"
    state_dir = root / "state"
    manifests_dir.mkdir(parents=True, exist_ok=True)
    state_dir.mkdir(parents=True, exist_ok=True)

    manifest_path = manifests_dir / "phase0-reference.json"
    summary_path = state_dir / "phase0-reference.capture.json"
    result_manifest_path = root / "results" / "phase0" / benchmark_id / "result-manifest.json"

    summary_payload = {
        "schema_version": 1,
        "root": str(root),
        "manifest": str(manifest_path),
        "summary_path": str(summary_path),
        "capture_state": capture_state,
        "required_only": packet_label == "required-set",
        "selected_benchmarks": [benchmark_id],
        "benchmark_summaries": [
            {
                "benchmark_id": benchmark_id,
                "status": capture_state,
                "result_manifest": str(result_manifest_path),
            }
        ],
    }
    if packet_label != "required-set":
        summary_payload["optional_capture_packet"] = packet_label

    write_json(
        manifest_path,
        {
            "schema_version": 1,
            "run_id": "phase0-reference",
            "created_at_utc": "2026-04-22T00:00:00Z",
            "phase0": {
                "capture_state": capture_state,
                "capture_summary": str(summary_path),
                "captured_benchmarks": [benchmark_id],
            },
        },
    )
    write_json(summary_path, summary_payload)


def rewrite_candidate_packet_metadata(root: Path, benchmark_ids: list[str]) -> None:
    manifest_path = root / "manifests" / "phase0-reference.json"
    summary_path = root / "state" / "phase0-reference.capture.json"

    manifest = load_json(manifest_path)
    manifest["phase0"]["capture_summary"] = str(summary_path)
    write_json(manifest_path, manifest)

    summary = load_json(summary_path)
    summary["root"] = str(root)
    summary["manifest"] = str(manifest_path)
    summary["summary_path"] = str(summary_path)
    summary["selected_benchmarks"] = benchmark_ids
    summary["benchmark_summaries"] = [
        {
            "benchmark_id": benchmark_id,
            "status": summary["capture_state"],
            "result_manifest": str(
                root / "results" / "phase0" / benchmark_id / "result-manifest.json"
            ),
        }
        for benchmark_id in benchmark_ids
    ]
    write_json(summary_path, summary)


def write_failure_code_audit(
    *,
    root: Path,
    benchmark_id: str,
    observed_failure_codes: list[str],
) -> None:
    write_json(
        root / "results" / "phase0" / benchmark_id / "failure-code-audit.json",
        {
            "schema_version": 1,
            "benchmark_id": benchmark_id,
            "observed_failure_codes": observed_failure_codes,
        },
    )


def run_self_check(qualification_path: Path) -> dict[str, Any]:
    with tempfile.TemporaryDirectory(prefix="amflow-phase0-failure-code-audit-self-check-") as tmp:
        temp_root = Path(tmp)
        candidate_root = temp_root / "candidate-packet-match"
        summary_path = temp_root / "failure-code-audit-summary.json"
        benchmark_id = "automatic_loop"
        output_names = ["sol1", "sol2"]
        output_hashes = {
            "sol1": "1111111111111111111111111111111111111111111111111111111111111111",
            "sol2": "2222222222222222222222222222222222222222222222222222222222222222",
        }
        required_failure_codes = sorted(
            load_phase0_scaffold(qualification_path)[benchmark_id]["required_failure_codes"]
        )

        write_candidate_packet(
            root=candidate_root,
            benchmark_id=benchmark_id,
            output_names=output_names,
            output_hashes=output_hashes,
        )
        write_candidate_packet_metadata(
            root=candidate_root,
            benchmark_id=benchmark_id,
            packet_label="required-set",
            capture_state="reference-captured",
        )
        write_failure_code_audit(
            root=candidate_root,
            benchmark_id=benchmark_id,
            observed_failure_codes=required_failure_codes + ["unsupported_solver_path"],
        )

        matching_summary = audit_phase0_failure_codes(
            candidate_root=candidate_root,
            benchmark_ids=None,
            qualification_path=qualification_path,
        )
        write_json(summary_path, matching_summary)

        missing_audit_root = temp_root / "candidate-packet-missing-audit"
        shutil.copytree(candidate_root, missing_audit_root)
        rewrite_candidate_packet_metadata(missing_audit_root, [benchmark_id])
        rewrite_candidate_result_manifest_pointer(missing_audit_root, benchmark_id)
        (missing_audit_root / "results" / "phase0" / benchmark_id / "failure-code-audit.json").unlink()
        missing_audit_summary = audit_phase0_failure_codes(
            candidate_root=missing_audit_root,
            benchmark_ids=None,
            qualification_path=qualification_path,
        )

        incomplete_audit_root = temp_root / "candidate-packet-incomplete-audit"
        shutil.copytree(candidate_root, incomplete_audit_root)
        rewrite_candidate_packet_metadata(incomplete_audit_root, [benchmark_id])
        rewrite_candidate_result_manifest_pointer(incomplete_audit_root, benchmark_id)
        write_failure_code_audit(
            root=incomplete_audit_root,
            benchmark_id=benchmark_id,
            observed_failure_codes=["insufficient_precision"],
        )
        incomplete_audit_summary = audit_phase0_failure_codes(
            candidate_root=incomplete_audit_root,
            benchmark_ids=None,
            qualification_path=qualification_path,
        )

        duplicate_code_root = temp_root / "candidate-packet-duplicate-codes"
        shutil.copytree(candidate_root, duplicate_code_root)
        rewrite_candidate_packet_metadata(duplicate_code_root, [benchmark_id])
        rewrite_candidate_result_manifest_pointer(duplicate_code_root, benchmark_id)
        write_failure_code_audit(
            root=duplicate_code_root,
            benchmark_id=benchmark_id,
            observed_failure_codes=["insufficient_precision", "insufficient_precision"],
        )

        missing_result_manifest_root = temp_root / "candidate-packet-missing-result"
        shutil.copytree(candidate_root, missing_result_manifest_root)
        rewrite_candidate_packet_metadata(missing_result_manifest_root, [benchmark_id])
        (
            missing_result_manifest_root
            / "results"
            / "phase0"
            / benchmark_id
            / "result-manifest.json"
        ).unlink()

        missing_observed_failure_codes_root = temp_root / "candidate-packet-missing-observed-codes"
        shutil.copytree(candidate_root, missing_observed_failure_codes_root)
        rewrite_candidate_packet_metadata(missing_observed_failure_codes_root, [benchmark_id])
        rewrite_candidate_result_manifest_pointer(missing_observed_failure_codes_root, benchmark_id)
        write_json(
            missing_observed_failure_codes_root
            / "results"
            / "phase0"
            / benchmark_id
            / "failure-code-audit.json",
            {
                "schema_version": 1,
                "benchmark_id": benchmark_id,
            },
        )

        null_observed_failure_codes_root = temp_root / "candidate-packet-null-observed-codes"
        shutil.copytree(candidate_root, null_observed_failure_codes_root)
        rewrite_candidate_packet_metadata(null_observed_failure_codes_root, [benchmark_id])
        rewrite_candidate_result_manifest_pointer(null_observed_failure_codes_root, benchmark_id)
        write_json(
            null_observed_failure_codes_root
            / "results"
            / "phase0"
            / benchmark_id
            / "failure-code-audit.json",
            {
                "schema_version": 1,
                "benchmark_id": benchmark_id,
                "observed_failure_codes": None,
            },
        )

        duplicate_observed_failure_codes_rejected = False
        try:
            audit_phase0_failure_codes(
                candidate_root=duplicate_code_root,
                benchmark_ids=None,
                qualification_path=qualification_path,
            )
        except RuntimeError as error:
            duplicate_observed_failure_codes_rejected = "must not contain duplicates" in str(error)

        missing_candidate_root_rejected = False
        try:
            require_candidate_root(None)
        except RuntimeError as error:
            missing_candidate_root_rejected = "--candidate-root is required" in str(error)

        missing_result_manifest_rejected = False
        try:
            audit_phase0_failure_codes(
                candidate_root=missing_result_manifest_root,
                benchmark_ids=None,
                qualification_path=qualification_path,
            )
        except RuntimeError as error:
            missing_result_manifest_rejected = "candidate result manifest missing" in str(error)

        missing_observed_failure_codes_rejected = False
        try:
            audit_phase0_failure_codes(
                candidate_root=missing_observed_failure_codes_root,
                benchmark_ids=None,
                qualification_path=qualification_path,
            )
        except RuntimeError as error:
            missing_observed_failure_codes_rejected = (
                "failure-code audit observed_failure_codes must be present" in str(error)
            )

        null_observed_failure_codes_rejected = False
        try:
            audit_phase0_failure_codes(
                candidate_root=null_observed_failure_codes_root,
                benchmark_ids=None,
                qualification_path=qualification_path,
            )
        except TypeError as error:
            null_observed_failure_codes_rejected = "observed_failure_codes must be a list" in str(error)

        pathlike_benchmark_id_rejected = False
        try:
            audit_phase0_failure_codes(
                candidate_root=candidate_root,
                benchmark_ids=["../escaped-benchmark"],
                qualification_path=qualification_path,
            )
        except RuntimeError as error:
            pathlike_benchmark_id_rejected = "--benchmark-id must be a plain benchmark id" in str(error)

        matching_benchmark = matching_summary["benchmarks"][0]
        missing_audit_benchmark = missing_audit_summary["benchmarks"][0]
        incomplete_audit_benchmark = incomplete_audit_summary["benchmarks"][0]
        return {
            "matching_candidate_reports_required_failure_codes": (
                matching_summary["all_selected_benchmarks_publish_failure_code_audits"]
                and matching_summary["all_selected_benchmarks_report_required_failure_codes"]
                and matching_benchmark["status"] == "required-failure-codes-complete"
            ),
            "profiles_reported_from_scaffold": (
                matching_benchmark["digit_threshold_profile"] == "core-package-family-default"
                and matching_benchmark["minimum_correct_digits"] == 50
                and "insufficient_precision" in matching_benchmark["required_failure_codes"]
                and "unexpected master sets in Kira interface"
                in matching_benchmark["known_regression_families"]
            ),
            "unexpected_failure_codes_reported": (
                matching_summary["any_selected_benchmarks_report_unexpected_failure_codes"]
                and "unsupported_solver_path" in matching_benchmark["unexpected_failure_codes"]
            ),
            "missing_failure_code_audit_reported": (
                not missing_audit_summary["all_selected_benchmarks_publish_failure_code_audits"]
                and missing_audit_benchmark["status"] == "failure-code-audit-missing"
                and missing_audit_benchmark["missing_required_failure_codes"] == required_failure_codes
            ),
            "incomplete_failure_code_audit_reported": (
                not incomplete_audit_summary["all_selected_benchmarks_report_required_failure_codes"]
                and incomplete_audit_benchmark["status"] == "required-failure-codes-incomplete"
                and "boundary_unsolved"
                in incomplete_audit_benchmark["missing_required_failure_codes"]
            ),
            "duplicate_observed_failure_codes_rejected": duplicate_observed_failure_codes_rejected,
            "missing_observed_failure_codes_rejected": missing_observed_failure_codes_rejected,
            "null_observed_failure_codes_rejected": null_observed_failure_codes_rejected,
            "pathlike_benchmark_id_rejected": pathlike_benchmark_id_rejected,
            "missing_candidate_root_rejected": missing_candidate_root_rejected,
            "missing_result_manifest_rejected": missing_result_manifest_rejected,
            "summary_written": summary_path.exists(),
        }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--candidate-root",
        type=Path,
        help="Candidate packet root that publishes retained phase-0 manifests and failure-code audits",
    )
    parser.add_argument(
        "--benchmark-id",
        action="append",
        help="Optional benchmark id to audit; defaults to every benchmark published by the candidate packet",
    )
    parser.add_argument(
        "--qualification-path",
        type=Path,
        help="Qualification scaffold JSON path",
    )
    parser.add_argument(
        "--summary-path",
        type=Path,
        help="Optional output file for the audit summary",
    )
    parser.add_argument(
        "--self-check",
        action="store_true",
        help="Run a synthetic failure-code audit self-check",
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

    candidate_root = require_candidate_root(args.candidate_root)
    summary = audit_phase0_failure_codes(
        candidate_root=candidate_root,
        benchmark_ids=args.benchmark_id,
        qualification_path=qualification_path,
    )
    if args.summary_path is not None:
        write_json(args.summary_path, summary)
    print(json.dumps(summary, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
