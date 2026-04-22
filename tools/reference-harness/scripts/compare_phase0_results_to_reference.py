#!/usr/bin/env python3
"""Compare candidate phase-0 packet outputs against retained reference goldens."""

from __future__ import annotations

import argparse
import json
import re
import shutil
import tempfile
from pathlib import Path
from typing import Any

from freeze_phase0_goldens import load_json


REFERENCE_CAPTURED = "reference-captured"
BOOTSTRAP_ONLY = "bootstrap-only"
REQUIRED_SET_LABEL = "required-set"
CAPTURED_PACKET_ROOT = re.compile(r"^phase0-reference-captured-\d{8}-(.+)$")


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def write_json(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def expect_unique(values: list[str], label: str) -> None:
    expect(len(set(values)) == len(values), f"{label} must not contain duplicates")


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


def expect_path_within_root(path: Path, root: Path, label: str) -> None:
    resolved_path = path.resolve(strict=False)
    resolved_root = root.resolve(strict=False)
    try:
        resolved_path.relative_to(resolved_root)
    except ValueError as error:
        raise RuntimeError(f"{label} must stay under {root}: {path}") from error


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
            if not isinstance(value, list):
                raise TypeError(
                    f"{section}.{profile_id}.{value_key} must be a list, got "
                    f"{type(value).__name__}"
                )
            value = normalize_string_list(value, f"{section}.{profile_id}.{value_key}")
            expect(value, f"{section}.{profile_id}.{value_key} must not be empty")
        mapping[profile_id] = value
    expect(mapping, f"{value_label} profiles must not be empty")
    return mapping


def load_phase0_scaffold(qualification_path: Path) -> dict[str, dict[str, Any]]:
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
    raw_phase0_entries = qualification.get("phase0_example_classes", [])
    if not isinstance(raw_phase0_entries, list):
        raise TypeError("phase0_example_classes must be a list")

    by_catalog_id: dict[str, dict[str, Any]] = {}
    for raw in raw_phase0_entries:
        if not isinstance(raw, dict):
            raise TypeError("phase0_example_classes entries must be objects")
        catalog_id = str(raw.get("phase0_catalog_id", raw.get("id", ""))).strip()
        if not catalog_id:
            raise ValueError("phase0_catalog_id must not be empty")
        if catalog_id in by_catalog_id:
            raise ValueError(f"duplicate phase0_catalog_id: {catalog_id}")
        digit_profile_id = str(raw.get("digit_threshold_profile", "")).strip()
        failure_profile_id = str(raw.get("failure_code_profile", "")).strip()
        regression_profile_id = str(raw.get("regression_profile", "")).strip()
        expect(
            digit_profile_id in digit_thresholds,
            f"phase0 example {catalog_id} digit_threshold_profile must exist",
        )
        expect(
            failure_profile_id in failure_profiles,
            f"phase0 example {catalog_id} failure_code_profile must exist",
        )
        expect(
            regression_profile_id in regression_profiles,
            f"phase0 example {catalog_id} regression_profile must exist",
        )
        by_catalog_id[catalog_id] = {
            "id": str(raw.get("id", catalog_id)).strip(),
            "phase0_catalog_id": catalog_id,
            "required_capture": bool(raw.get("required_capture", False)),
            "current_evidence_state": str(raw.get("current_evidence_state", "")).strip(),
            "optional_capture_packet": str(raw.get("optional_capture_packet", "")).strip(),
            "next_runtime_lane": str(raw.get("next_runtime_lane", "")).strip(),
            "digit_threshold_profile": digit_profile_id,
            "minimum_correct_digits": digit_thresholds[digit_profile_id],
            "failure_code_profile": failure_profile_id,
            "required_failure_codes": failure_profiles[failure_profile_id],
            "regression_profile": regression_profile_id,
            "known_regression_families": regression_profiles[regression_profile_id],
        }
    expect(by_catalog_id, "phase0_example_classes must not be empty")
    return by_catalog_id


def locate_manifest(root: Path) -> Path:
    explicit = root / "manifests" / "phase0-reference.json"
    if explicit.exists():
        return explicit
    manifests = sorted((root / "manifests").glob("*.json"))
    expect(len(manifests) == 1, f"could not infer manifest under {root}")
    return manifests[0]


def locate_capture_summary(root: Path, manifest: dict[str, Any]) -> Path:
    manifest_summary = str(manifest.get("phase0", {}).get("capture_summary", "")).strip()
    if manifest_summary:
        return Path(manifest_summary)
    fallback = root / "state" / "phase0-reference.capture.json"
    if fallback.exists():
        return fallback
    raise FileNotFoundError(f"could not locate phase-0 capture summary under {root}")


def infer_packet_label(root: Path, summary: dict[str, Any]) -> str:
    packet = str(summary.get("optional_capture_packet", "")).strip()
    if packet:
        return packet
    if bool(summary.get("required_only", False)):
        return REQUIRED_SET_LABEL
    match = CAPTURED_PACKET_ROOT.fullmatch(root.name)
    expect(match is not None, f"could not infer packet label from root name: {root}")
    return match.group(1)


def load_output_entries(
    raw_outputs: Any,
    *,
    label: str,
) -> dict[str, dict[str, Any]]:
    if not isinstance(raw_outputs, list):
        raise TypeError(f"{label} must be a list")
    outputs: dict[str, dict[str, Any]] = {}
    for raw in raw_outputs:
        if not isinstance(raw, dict):
            raise TypeError(f"{label} entries must be objects")
        output_name = str(raw.get("name", "")).strip()
        if not output_name:
            raise ValueError(f"{label} output name must not be empty")
        if output_name in outputs:
            raise ValueError(f"{label} duplicate output name: {output_name}")
        canonical_sha256 = str(raw.get("canonical_sha256", "")).strip()
        if not canonical_sha256:
            raise ValueError(f"{label}.{output_name}.canonical_sha256 must not be empty")
        output_path = Path(str(raw.get("path", "")))
        canonical_text = Path(str(raw.get("canonical_text", "")))
        outputs[output_name] = {
            "name": output_name,
            "path": output_path,
            "canonical_text": canonical_text,
            "canonical_sha256": canonical_sha256,
        }
    expect(outputs, f"{label} must not be empty")
    return outputs


def load_reference_outputs(
    *,
    reference_root: Path,
    benchmark_id: str,
    golden_manifest_path: Path,
) -> tuple[dict[str, Any], dict[str, dict[str, Any]]]:
    expect(golden_manifest_path.exists(), f"golden manifest missing for {benchmark_id} in {reference_root}")
    expect_path_within_root(
        golden_manifest_path,
        reference_root,
        f"reference golden manifest path for {benchmark_id}",
    )
    golden_manifest = load_json(golden_manifest_path)
    expect(
        str(golden_manifest.get("benchmark_id", "")).strip() == benchmark_id,
        f"reference golden manifest benchmark_id must match {benchmark_id}",
    )
    golden_output_dir = Path(str(golden_manifest.get("golden_output_dir", "")))
    expect_path_within_root(
        golden_output_dir,
        reference_root,
        f"reference golden output directory for {benchmark_id}",
    )
    expect(
        golden_output_dir.is_dir(),
        f"reference golden output directory must exist for {benchmark_id}",
    )
    outputs = load_output_entries(
        golden_manifest.get("outputs", []),
        label=f"reference golden outputs for {benchmark_id}",
    )
    for output_name, output in outputs.items():
        expect_path_within_root(
            output["path"],
            reference_root,
            f"reference output path for {benchmark_id}:{output_name}",
        )
        expect(
            output["path"].exists(),
            f"reference output path must exist for {benchmark_id}:{output_name}",
        )
        expect_path_within_root(
            output["canonical_text"],
            reference_root,
            f"reference canonical text path for {benchmark_id}:{output_name}",
        )
        expect(
            output["canonical_text"].exists(),
            f"reference canonical text path must exist for {benchmark_id}:{output_name}",
        )
    return golden_manifest, outputs


def load_reference_packet(
    *,
    reference_root: Path,
    scaffold_entries: dict[str, dict[str, Any]],
) -> dict[str, Any]:
    expect(reference_root.is_dir(), f"reference root must exist: {reference_root}")
    manifest_path = locate_manifest(reference_root)
    expect_path_within_root(manifest_path, reference_root, "reference manifest path")
    manifest = load_json(manifest_path)
    summary_path = locate_capture_summary(reference_root, manifest)
    expect(summary_path.exists(), f"reference capture summary missing for {reference_root}")
    expect_path_within_root(summary_path, reference_root, "reference summary path")
    summary = load_json(summary_path)

    packet_label = infer_packet_label(reference_root, summary)
    manifest_capture_state = str(manifest.get("phase0", {}).get("capture_state", "")).strip()
    summary_capture_state = str(summary.get("capture_state", "")).strip()
    expect(
        manifest_capture_state in {REFERENCE_CAPTURED, BOOTSTRAP_ONLY},
        f"reference capture_state must be bootstrap-only or reference-captured, got "
        f"{manifest_capture_state!r}",
    )
    expect(
        summary_capture_state == manifest_capture_state,
        "reference capture summary capture_state must match the manifest",
    )
    expect(
        Path(str(summary.get("manifest", ""))) == manifest_path,
        "reference capture summary manifest must match the retained manifest path",
    )
    expect(
        Path(str(summary.get("root", ""))) == reference_root,
        "reference capture summary root must match the retained root path",
    )

    manifest_benchmarks = normalize_string_list(
        manifest.get("phase0", {}).get("captured_benchmarks", []),
        "reference manifest captured_benchmarks",
    )
    summary_benchmarks = normalize_string_list(
        summary.get("selected_benchmarks", []),
        "reference summary selected_benchmarks",
    )
    expect_unique(manifest_benchmarks, "reference manifest captured_benchmarks")
    expect_unique(summary_benchmarks, "reference summary selected_benchmarks")

    raw_benchmark_summaries = summary.get("benchmark_summaries", [])
    if not isinstance(raw_benchmark_summaries, list):
        raise TypeError("reference benchmark_summaries must be a list")

    benchmark_ids: list[str] = []
    benchmarks: dict[str, dict[str, Any]] = {}
    for raw in raw_benchmark_summaries:
        if not isinstance(raw, dict):
            raise TypeError("reference benchmark_summaries entries must be objects")
        benchmark_id = str(raw.get("benchmark_id", "")).strip()
        expect(benchmark_id, "reference benchmark_id must not be empty")
        expect(
            benchmark_id in scaffold_entries,
            f"reference benchmark {benchmark_id!r} is not present in the qualification scaffold",
        )
        expect(
            benchmark_id not in benchmarks,
            f"duplicate reference benchmark {benchmark_id} in {reference_root}",
        )
        status = str(raw.get("status", "")).strip()
        expect(
            status == REFERENCE_CAPTURED,
            f"reference benchmark {benchmark_id} must stay reference-captured",
        )
        expect(
            bool(raw.get("backup_match_ok", False)),
            f"reference benchmark {benchmark_id} must keep bundled-backup agreement",
        )
        expect(
            bool(raw.get("rerun_match_ok", False)),
            f"reference benchmark {benchmark_id} must keep rerun reproducibility agreement",
        )
        comparison_summary = Path(str(raw.get("comparison_summary", "")))
        result_manifest = Path(str(raw.get("result_manifest", "")))
        golden_manifest = Path(str(raw.get("golden_manifest", "")))
        expect(comparison_summary.exists(),
               f"reference comparison summary missing for {benchmark_id} in {reference_root}")
        expect(result_manifest.exists(),
               f"reference result manifest missing for {benchmark_id} in {reference_root}")
        expect_path_within_root(
            comparison_summary,
            reference_root,
            f"reference comparison summary path for {benchmark_id}",
        )
        expect_path_within_root(
            result_manifest,
            reference_root,
            f"reference result manifest path for {benchmark_id}",
        )
        comparison_payload = load_json(comparison_summary)
        expect(
            str(comparison_payload.get("benchmark_id", "")).strip() == benchmark_id,
            f"reference comparison summary benchmark_id must match {benchmark_id}",
        )
        expect(
            str(comparison_payload.get("status", "")).strip() == REFERENCE_CAPTURED,
            f"reference comparison summary for {benchmark_id} must stay reference-captured",
        )
        expect(
            Path(str(comparison_payload.get("reference_golden", ""))) == golden_manifest,
            f"reference comparison summary golden manifest must match {benchmark_id}",
        )
        expect(
            Path(str(comparison_payload.get("latest_run_result", ""))) == result_manifest,
            f"reference comparison summary result manifest must match {benchmark_id}",
        )
        golden_payload, golden_outputs = load_reference_outputs(
            reference_root=reference_root,
            benchmark_id=benchmark_id,
            golden_manifest_path=golden_manifest,
        )
        benchmark_ids.append(benchmark_id)
        benchmarks[benchmark_id] = {
            "comparison_summary": comparison_summary,
            "comparison_payload": comparison_payload,
            "result_manifest": result_manifest,
            "golden_manifest": golden_manifest,
            "golden_payload": golden_payload,
            "outputs": golden_outputs,
            "scaffold": scaffold_entries[benchmark_id],
        }

    expect(
        set(manifest_benchmarks) == set(benchmark_ids),
        "reference manifest captured_benchmarks must match benchmark_summaries",
    )
    expect(
        set(summary_benchmarks) == set(benchmark_ids),
        "reference summary selected_benchmarks must match benchmark_summaries",
    )

    return {
        "root": reference_root,
        "manifest_path": manifest_path,
        "summary_path": summary_path,
        "capture_state": manifest_capture_state,
        "packet_label": packet_label,
        "benchmark_ids": summary_benchmarks,
        "benchmarks": benchmarks,
    }


def select_benchmark_ids(
    reference_packet: dict[str, Any],
    requested_ids: list[str] | None,
) -> list[str]:
    if not requested_ids:
        return list(reference_packet["benchmark_ids"])
    selected: list[str] = []
    for benchmark_id in requested_ids:
        value = benchmark_id.strip()
        expect(value, "requested benchmark ids must not be empty")
        expect(
            value in reference_packet["benchmarks"],
            f"requested benchmark {value!r} is not present in the retained reference packet",
        )
        if value not in selected:
            selected.append(value)
    return selected


def compare_selected_benchmarks(
    *,
    reference_packet: dict[str, Any],
    candidate_root: Path,
    benchmark_ids: list[str],
) -> list[dict[str, Any]]:
    expect(candidate_root.is_dir(), f"candidate root must exist: {candidate_root}")
    benchmark_summaries: list[dict[str, Any]] = []
    for benchmark_id in benchmark_ids:
        reference_benchmark = reference_packet["benchmarks"][benchmark_id]
        scaffold_entry = reference_benchmark["scaffold"]
        candidate_result_manifest = (
            candidate_root / "results" / "phase0" / benchmark_id / "result-manifest.json"
        )
        expect(
            candidate_result_manifest.exists(),
            f"candidate result manifest missing for {benchmark_id} in {candidate_root}",
        )
        expect_path_within_root(
            candidate_result_manifest,
            candidate_root,
            f"candidate result manifest path for {benchmark_id}",
        )
        result_manifest = load_json(candidate_result_manifest)
        expect(
            str(result_manifest.get("benchmark_id", "")).strip() == benchmark_id,
            f"candidate result manifest benchmark_id must match {benchmark_id}",
        )
        primary_run_manifest = Path(str(result_manifest.get("primary_run_manifest", "")))
        expect(
            primary_run_manifest.exists(),
            f"candidate primary run manifest missing for {benchmark_id} in {candidate_root}",
        )
        expect_path_within_root(
            primary_run_manifest,
            candidate_root,
            f"candidate primary run manifest path for {benchmark_id}",
        )
        run_manifest = load_json(primary_run_manifest)
        expect(
            str(run_manifest.get("benchmark_id", "")).strip() == benchmark_id,
            f"candidate primary run manifest benchmark_id must match {benchmark_id}",
        )
        candidate_outputs = load_output_entries(
            run_manifest.get("outputs", []),
            label=f"candidate outputs for {benchmark_id}",
        )
        candidate_output_names = set(candidate_outputs)
        reference_output_names = set(reference_benchmark["outputs"])
        expect(
            candidate_output_names == reference_output_names,
            f"candidate outputs for {benchmark_id} must match the retained reference output set",
        )
        output_comparisons: list[dict[str, Any]] = []
        for output_name in sorted(reference_benchmark["outputs"]):
            candidate_output = candidate_outputs[output_name]
            reference_output = reference_benchmark["outputs"][output_name]
            expect_path_within_root(
                candidate_output["path"],
                candidate_root,
                f"candidate output path for {benchmark_id}:{output_name}",
            )
            expect(
                candidate_output["path"].exists(),
                f"candidate output path must exist for {benchmark_id}:{output_name}",
            )
            expect_path_within_root(
                candidate_output["canonical_text"],
                candidate_root,
                f"candidate canonical text path for {benchmark_id}:{output_name}",
            )
            expect(
                candidate_output["canonical_text"].exists(),
                f"candidate canonical text path must exist for {benchmark_id}:{output_name}",
            )
            expect(
                candidate_output["canonical_sha256"] == reference_output["canonical_sha256"],
                f"candidate canonical hash for {benchmark_id}:{output_name} does not match the "
                "retained reference golden",
            )
            output_comparisons.append(
                {
                    "name": output_name,
                    "reference_canonical_sha256": reference_output["canonical_sha256"],
                    "candidate_canonical_sha256": candidate_output["canonical_sha256"],
                    "status": "reference-match",
                }
            )
        benchmark_summaries.append(
            {
                "benchmark_id": benchmark_id,
                "status": "reference-match",
                "reference_packet_label": reference_packet["packet_label"],
                "reference_capture_state": reference_packet["capture_state"],
                "required_capture": scaffold_entry["required_capture"],
                "current_evidence_state": scaffold_entry["current_evidence_state"],
                "optional_capture_packet": scaffold_entry["optional_capture_packet"],
                "next_runtime_lane": scaffold_entry["next_runtime_lane"],
                "digit_threshold_profile": scaffold_entry["digit_threshold_profile"],
                "minimum_correct_digits": scaffold_entry["minimum_correct_digits"],
                "failure_code_profile": scaffold_entry["failure_code_profile"],
                "required_failure_codes": scaffold_entry["required_failure_codes"],
                "regression_profile": scaffold_entry["regression_profile"],
                "known_regression_families": scaffold_entry["known_regression_families"],
                "reference_golden_manifest": str(reference_benchmark["golden_manifest"]),
                "candidate_result_manifest": str(candidate_result_manifest),
                "candidate_primary_run_manifest": str(primary_run_manifest),
                "reference_output_names": sorted(reference_benchmark["outputs"]),
                "candidate_output_names": sorted(candidate_outputs),
                "output_comparisons": output_comparisons,
            }
        )
    return benchmark_summaries


def compare_phase0_results_to_reference(
    *,
    reference_root: Path,
    candidate_root: Path,
    benchmark_ids: list[str] | None,
    qualification_path: Path,
) -> dict[str, Any]:
    scaffold_entries = load_phase0_scaffold(qualification_path)
    reference_packet = load_reference_packet(
        reference_root=reference_root,
        scaffold_entries=scaffold_entries,
    )
    selected_benchmark_ids = select_benchmark_ids(reference_packet, benchmark_ids)
    benchmark_summaries = compare_selected_benchmarks(
        reference_packet=reference_packet,
        candidate_root=candidate_root,
        benchmark_ids=selected_benchmark_ids,
    )
    return {
        "schema_version": 1,
        "qualification_path": str(qualification_path),
        "reference_root": str(reference_root),
        "reference_packet_label": reference_packet["packet_label"],
        "reference_capture_state": reference_packet["capture_state"],
        "candidate_root": str(candidate_root),
        "selected_benchmark_ids": selected_benchmark_ids,
        "reference_benchmarks_pass_retained_capture_checks": True,
        "candidate_result_manifests_exist": True,
        "candidate_primary_run_manifests_exist": True,
        "candidate_output_names_match_reference": True,
        "candidate_output_hashes_match_reference": True,
        "digit_threshold_profiles_reported": True,
        "required_failure_code_profiles_reported": True,
        "regression_profiles_reported": True,
        "benchmarks": benchmark_summaries,
    }


def write_reference_packet(
    *,
    root: Path,
    benchmark_id: str,
    output_names: list[str],
    output_hashes: dict[str, str],
) -> None:
    manifests_dir = root / "manifests"
    state_dir = root / "state"
    comparisons_dir = root / "comparisons" / "phase0"
    goldens_dir = root / "goldens" / "phase0" / benchmark_id
    results_dir = root / "results" / "phase0" / benchmark_id
    manifests_dir.mkdir(parents=True, exist_ok=True)
    state_dir.mkdir(parents=True, exist_ok=True)
    comparisons_dir.mkdir(parents=True, exist_ok=True)
    (goldens_dir / "captured" / "canonical").mkdir(parents=True, exist_ok=True)
    results_dir.mkdir(parents=True, exist_ok=True)

    manifest_path = manifests_dir / "phase0-reference.json"
    summary_path = state_dir / "phase0-reference.capture.json"
    result_manifest_path = results_dir / "result-manifest.json"
    golden_manifest_path = goldens_dir / "golden-manifest.json"
    comparison_summary_path = comparisons_dir / f"{benchmark_id}.summary.json"

    golden_outputs: list[dict[str, Any]] = []
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
        manifest_path,
        {
            "schema_version": 1,
            "run_id": "phase0-reference",
            "created_at_utc": "2026-04-22T00:00:00Z",
            "phase0": {
                "capture_state": REFERENCE_CAPTURED,
                "capture_summary": str(summary_path),
                "captured_benchmarks": [benchmark_id],
            },
        },
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
    write_json(
        summary_path,
        {
            "schema_version": 1,
            "root": str(root),
            "manifest": str(manifest_path),
            "summary_path": str(summary_path),
            "capture_state": REFERENCE_CAPTURED,
            "required_only": True,
            "selected_benchmarks": [benchmark_id],
            "benchmark_summaries": [
                {
                    "benchmark_id": benchmark_id,
                    "status": REFERENCE_CAPTURED,
                    "backup_match_ok": True,
                    "rerun_match_ok": True,
                    "comparison_summary": str(comparison_summary_path),
                    "golden_manifest": str(golden_manifest_path),
                    "result_manifest": str(result_manifest_path),
                }
            ],
        },
    )


def write_candidate_packet(
    *,
    root: Path,
    benchmark_id: str,
    output_names: list[str],
    output_hashes: dict[str, str],
) -> None:
    results_dir = root / "results" / "phase0" / benchmark_id
    primary_dir = results_dir / "primary"
    canonical_dir = primary_dir / "canonical"
    canonical_dir.mkdir(parents=True, exist_ok=True)

    primary_run_manifest_path = primary_dir / "run-manifest.json"
    result_manifest_path = results_dir / "result-manifest.json"

    outputs: list[dict[str, Any]] = []
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


def rewrite_candidate_result_manifest_pointer(root: Path, benchmark_id: str) -> None:
    result_manifest_path = root / "results" / "phase0" / benchmark_id / "result-manifest.json"
    payload = load_json(result_manifest_path)
    payload["primary_run_manifest"] = str(
        root / "results" / "phase0" / benchmark_id / "primary" / "run-manifest.json"
    )
    write_json(result_manifest_path, payload)
    run_manifest_path = root / "results" / "phase0" / benchmark_id / "primary" / "run-manifest.json"
    run_payload = load_json(run_manifest_path)
    for output in run_payload.get("outputs", []):
        output_name = str(output.get("name", "")).strip()
        output["path"] = str(
            root / "results" / "phase0" / benchmark_id / "primary" / output_name
        )
        output["canonical_text"] = str(
            root
            / "results"
            / "phase0"
            / benchmark_id
            / "primary"
            / "canonical"
            / (output_name + ".canonical.txt")
        )
    write_json(run_manifest_path, run_payload)


def run_self_check(qualification_path: Path) -> dict[str, Any]:
    with tempfile.TemporaryDirectory(prefix="amflow-phase0-reference-compare-self-check-") as tmp:
        temp_root = Path(tmp)
        reference_root = temp_root / "phase0-reference-captured-20260419-required-set"
        matching_candidate_root = temp_root / "candidate-packet-match"
        summary_path = temp_root / "reference-compare-summary.json"
        benchmark_id = "automatic_loop"
        output_names = ["sol1", "sol2"]
        output_hashes = {
            "sol1": "1111111111111111111111111111111111111111111111111111111111111111",
            "sol2": "2222222222222222222222222222222222222222222222222222222222222222",
        }

        write_reference_packet(
            root=reference_root,
            benchmark_id=benchmark_id,
            output_names=output_names,
            output_hashes=output_hashes,
        )
        write_candidate_packet(
            root=matching_candidate_root,
            benchmark_id=benchmark_id,
            output_names=output_names,
            output_hashes=output_hashes,
        )

        summary = compare_phase0_results_to_reference(
            reference_root=reference_root,
            candidate_root=matching_candidate_root,
            benchmark_ids=None,
            qualification_path=qualification_path,
        )
        write_json(summary_path, summary)

        mismatched_candidate_root = temp_root / "candidate-packet-hash-mismatch"
        shutil.copytree(matching_candidate_root, mismatched_candidate_root)
        rewrite_candidate_result_manifest_pointer(mismatched_candidate_root, benchmark_id)
        mismatch_run_manifest = (
            mismatched_candidate_root
            / "results"
            / "phase0"
            / benchmark_id
            / "primary"
            / "run-manifest.json"
        )
        mismatch_run_payload = load_json(mismatch_run_manifest)
        mismatch_run_payload["outputs"][1]["canonical_sha256"] = (
            "3333333333333333333333333333333333333333333333333333333333333333"
        )
        write_json(mismatch_run_manifest, mismatch_run_payload)

        output_name_mismatch_root = temp_root / "candidate-packet-name-mismatch"
        shutil.copytree(matching_candidate_root, output_name_mismatch_root)
        rewrite_candidate_result_manifest_pointer(output_name_mismatch_root, benchmark_id)
        name_mismatch_manifest = (
            output_name_mismatch_root
            / "results"
            / "phase0"
            / benchmark_id
            / "primary"
            / "run-manifest.json"
        )
        name_mismatch_payload = load_json(name_mismatch_manifest)
        name_mismatch_payload["outputs"][1]["name"] = "solX"
        write_json(name_mismatch_manifest, name_mismatch_payload)

        missing_result_manifest_root = temp_root / "candidate-packet-missing-result"
        shutil.copytree(matching_candidate_root, missing_result_manifest_root)
        (
            missing_result_manifest_root
            / "results"
            / "phase0"
            / benchmark_id
            / "result-manifest.json"
        ).unlink()

        hash_mismatch_rejected = False
        try:
            compare_phase0_results_to_reference(
                reference_root=reference_root,
                candidate_root=mismatched_candidate_root,
                benchmark_ids=None,
                qualification_path=qualification_path,
            )
        except RuntimeError as error:
            hash_mismatch_rejected = "does not match the retained reference golden" in str(error)

        output_name_mismatch_rejected = False
        try:
            compare_phase0_results_to_reference(
                reference_root=reference_root,
                candidate_root=output_name_mismatch_root,
                benchmark_ids=None,
                qualification_path=qualification_path,
            )
        except RuntimeError as error:
            output_name_mismatch_rejected = "must match the retained reference output set" in str(
                error
            )

        missing_candidate_root_rejected = False
        try:
            require_candidate_root(None)
        except RuntimeError as error:
            missing_candidate_root_rejected = "--candidate-root is required" in str(error)

        missing_result_manifest_rejected = False
        try:
            compare_phase0_results_to_reference(
                reference_root=reference_root,
                candidate_root=missing_result_manifest_root,
                benchmark_ids=None,
                qualification_path=qualification_path,
            )
        except RuntimeError as error:
            missing_result_manifest_rejected = "candidate result manifest missing" in str(error)

        benchmark_summary = summary["benchmarks"][0]
        return {
            "matching_candidate_matches_reference": (
                summary["candidate_output_hashes_match_reference"]
                and benchmark_summary["status"] == "reference-match"
            ),
            "profiles_reported_from_scaffold": (
                benchmark_summary["digit_threshold_profile"] == "core-package-family-default"
                and benchmark_summary["minimum_correct_digits"] == 50
                and "insufficient_precision" in benchmark_summary["required_failure_codes"]
                and "unexpected master sets in Kira interface"
                in benchmark_summary["known_regression_families"]
            ),
            "hash_mismatch_rejected": hash_mismatch_rejected,
            "output_name_mismatch_rejected": output_name_mismatch_rejected,
            "missing_candidate_root_rejected": missing_candidate_root_rejected,
            "missing_result_manifest_rejected": missing_result_manifest_rejected,
            "summary_written": summary_path.exists(),
        }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--reference-root",
        type=Path,
        required=True,
        help="Retained reference packet root that publishes goldens and comparison summaries",
    )
    parser.add_argument(
        "--candidate-root",
        type=Path,
        help="Candidate packet root that publishes result-manifest and primary run-manifest files; required unless --self-check is set",
    )
    parser.add_argument(
        "--benchmark-id",
        action="append",
        help="Optional benchmark id to compare; defaults to all retained benchmarks in the reference packet",
    )
    parser.add_argument(
        "--qualification-path",
        type=Path,
        help="Qualification scaffold JSON path",
    )
    parser.add_argument(
        "--summary-path",
        type=Path,
        help="Optional output file for the comparison summary",
    )
    parser.add_argument(
        "--self-check",
        action="store_true",
        help="Run a synthetic packet comparison self-check",
    )
    return parser.parse_args()


def require_candidate_root(candidate_root: Path | None) -> Path:
    expect(candidate_root is not None, "--candidate-root is required unless --self-check is set")
    return candidate_root


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

    candidate_root = require_candidate_root(args.candidate_root)
    summary = compare_phase0_results_to_reference(
        reference_root=args.reference_root,
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
