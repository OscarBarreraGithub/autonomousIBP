#!/usr/bin/env python3
"""Summarize retained phase-0 evidence against the M6 qualification scaffold."""

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


def expect_path_within_root(path: Path, root: Path, label: str) -> None:
    resolved_path = path.resolve(strict=False)
    resolved_root = root.resolve(strict=False)
    try:
        resolved_path.relative_to(resolved_root)
    except ValueError as error:
        raise RuntimeError(f"{label} must stay under {root}: {path}") from error


def scaffold_phase0_examples(qualification: dict[str, Any]) -> list[dict[str, Any]]:
    raw_examples = qualification.get("phase0_example_classes", [])
    if not isinstance(raw_examples, list):
        raise TypeError("qualification phase0_example_classes must be a list")
    examples: list[dict[str, Any]] = []
    seen_ids: set[str] = set()
    for raw in raw_examples:
        if not isinstance(raw, dict):
            raise TypeError("qualification phase0_example_classes entries must be objects")
        benchmark_id = str(raw["id"]).strip()
        if not benchmark_id:
            raise ValueError("qualification phase0 example id must not be empty")
        if benchmark_id in seen_ids:
            raise ValueError(f"duplicate qualification phase0 example id: {benchmark_id}")
        seen_ids.add(benchmark_id)
        examples.append(
            {
                "id": benchmark_id,
                "required_capture": bool(raw.get("required_capture", False)),
                "current_evidence_state": str(raw.get("current_evidence_state", "")).strip(),
                "optional_capture_packet": str(raw.get("optional_capture_packet", "")).strip(),
                "next_runtime_lane": str(raw.get("next_runtime_lane", "")).strip(),
            }
        )
    return examples


def scaffold_case_studies(qualification: dict[str, Any]) -> list[dict[str, Any]]:
    raw_families = qualification.get("case_study_families", [])
    if not isinstance(raw_families, list):
        raise TypeError("qualification case_study_families must be a list")
    families: list[dict[str, Any]] = []
    seen_ids: set[str] = set()
    for raw in raw_families:
        if not isinstance(raw, dict):
            raise TypeError("qualification case_study_families entries must be objects")
        family_id = str(raw["id"]).strip()
        if not family_id:
            raise ValueError("qualification case-study id must not be empty")
        if family_id in seen_ids:
            raise ValueError(f"duplicate qualification case-study id: {family_id}")
        seen_ids.add(family_id)
        families.append(
            {
                "id": family_id,
                "next_runtime_lane": str(raw.get("next_runtime_lane", "")).strip(),
            }
        )
    return families


def locate_manifest(root: Path) -> Path:
    manifest_path = root / "manifests" / "phase0-reference.json"
    if manifest_path.exists():
        return manifest_path
    manifests_dir = root / "manifests"
    if manifests_dir.is_dir():
        manifests = sorted(path for path in manifests_dir.glob("*.json") if path.is_file())
        if len(manifests) == 1:
            return manifests[0]
    raise FileNotFoundError(f"could not locate phase-0 reference manifest under {root}")


def locate_capture_summary(root: Path, manifest: dict[str, Any]) -> Path:
    manifest_summary = str(manifest.get("phase0", {}).get("capture_summary", "")).strip()
    if manifest_summary:
        return Path(manifest_summary)
    fallback = root / "state" / "phase0-reference.capture.json"
    if fallback.exists():
        return fallback
    raise FileNotFoundError(f"could not locate phase-0 capture summary under {root}")


def infer_optional_capture_packet(
    benchmark_ids: list[str],
    phase0_by_id: dict[str, dict[str, Any]],
) -> str:
    packet_ids = {
        phase0_by_id[benchmark_id]["optional_capture_packet"]
        for benchmark_id in benchmark_ids
        if phase0_by_id[benchmark_id]["optional_capture_packet"]
    }
    if len(packet_ids) == 1:
        return next(iter(packet_ids))
    return ""


def load_packet_root(
    *,
    root: Path,
    phase0_by_id: dict[str, dict[str, Any]],
    root_role: str,
) -> dict[str, Any]:
    expect(root.is_dir(), f"{root_role} root must exist: {root}")
    manifest_path = locate_manifest(root)
    expect_path_within_root(manifest_path, root, f"{root_role} manifest path")
    manifest = load_json(manifest_path)
    summary_path = locate_capture_summary(root, manifest)
    expect_path_within_root(summary_path, root, f"{root_role} capture summary path")
    summary = load_json(summary_path)

    manifest_benchmarks = normalize_string_list(
        manifest.get("phase0", {}).get("captured_benchmarks", []),
        f"{root_role} manifest captured_benchmarks",
    )
    summary_benchmarks = normalize_string_list(
        summary.get("selected_benchmarks", []),
        f"{root_role} summary selected_benchmarks",
    )
    benchmark_summaries = summary.get("benchmark_summaries", [])
    if not isinstance(benchmark_summaries, list):
        raise TypeError(f"{root_role} benchmark_summaries must be a list")
    summary_entry_ids = normalize_string_list(
        [entry.get("benchmark_id") for entry in benchmark_summaries],
        f"{root_role} summary benchmark ids",
    )
    capture_state = str(manifest.get("phase0", {}).get("capture_state", "")).strip()
    summary_capture_state = str(summary.get("capture_state", "")).strip()
    reported_optional_packet = str(summary.get("optional_capture_packet", "")).strip()

    expect_unique(manifest_benchmarks, f"{root_role} manifest captured_benchmarks")
    expect_unique(summary_benchmarks, f"{root_role} summary selected_benchmarks")
    expect_unique(summary_entry_ids, f"{root_role} summary benchmark ids")

    expect(
        capture_state in {"bootstrap-only", "reference-captured"},
        f"{root_role} capture_state must be bootstrap-only or reference-captured",
    )
    expect(
        summary_capture_state == capture_state,
        f"{root_role} capture summary capture_state must match the manifest capture_state",
    )
    expect(
        all(benchmark_id in phase0_by_id for benchmark_id in manifest_benchmarks),
        f"{root_role} captured benchmarks must all exist in the qualification scaffold",
    )
    expect(
        set(summary_benchmarks) == set(manifest_benchmarks),
        f"{root_role} selected_benchmarks must match manifest captured_benchmarks",
    )
    expect(
        set(summary_entry_ids) == set(manifest_benchmarks),
        f"{root_role} benchmark_summaries must match manifest captured_benchmarks",
    )

    inferred_optional_packet = infer_optional_capture_packet(manifest_benchmarks, phase0_by_id)
    if reported_optional_packet and inferred_optional_packet:
        expect(
            reported_optional_packet == inferred_optional_packet,
            f"{root_role} optional capture packet must agree with the scaffold packet hint",
        )
    effective_optional_packet = reported_optional_packet or inferred_optional_packet

    captured_examples: list[dict[str, Any]] = []
    for entry in benchmark_summaries:
        if not isinstance(entry, dict):
            raise TypeError(f"{root_role} benchmark_summaries entries must be objects")
        benchmark_id = str(entry["benchmark_id"]).strip()
        comparison_summary = Path(str(entry["comparison_summary"]))
        golden_manifest = Path(str(entry["golden_manifest"]))
        result_manifest = Path(str(entry["result_manifest"]))
        expect_path_within_root(comparison_summary, root, f"{root_role} comparison summary path")
        expect_path_within_root(golden_manifest, root, f"{root_role} golden manifest path")
        expect_path_within_root(result_manifest, root, f"{root_role} result manifest path")
        expect(comparison_summary.exists(),
               f"{root_role} comparison summary must exist for {benchmark_id}")
        expect(golden_manifest.exists(),
               f"{root_role} golden manifest must exist for {benchmark_id}")
        expect(result_manifest.exists(),
               f"{root_role} result manifest must exist for {benchmark_id}")
        comparison_payload = load_json(comparison_summary)
        golden_payload = load_json(golden_manifest)
        result_payload = load_json(result_manifest)
        golden_output_dir = Path(str(golden_payload.get("golden_output_dir", "")))
        comparison_status = str(comparison_payload.get("status", "")).strip()
        expect(
            str(comparison_payload.get("benchmark_id", "")).strip() == benchmark_id,
            f"{root_role} comparison summary benchmark_id must match {benchmark_id}",
        )
        expect(
            Path(str(comparison_payload.get("reference_golden", ""))) == golden_manifest,
            f"{root_role} comparison summary must point at the promoted golden manifest for "
            f"{benchmark_id}",
        )
        expect(
            Path(str(comparison_payload.get("latest_run_result", ""))) == result_manifest,
            f"{root_role} comparison summary must point at the retained result manifest for "
            f"{benchmark_id}",
        )
        expect(
            str(golden_payload.get("benchmark_id", "")).strip() == benchmark_id,
            f"{root_role} golden manifest benchmark_id must match {benchmark_id}",
        )
        expect_path_within_root(golden_output_dir, root,
                                f"{root_role} golden output directory path")
        expect(
            golden_output_dir.is_dir(),
            f"{root_role} golden output directory must exist for {benchmark_id}",
        )
        expect(
            golden_output_dir.resolve(strict=False).is_relative_to(root.resolve(strict=False)),
            f"{root_role} golden output directory must stay under the retained root for "
            f"{benchmark_id}",
        )
        expect(
            str(result_payload.get("benchmark_id", "")).strip() == benchmark_id,
            f"{root_role} result manifest benchmark_id must match {benchmark_id}",
        )
        expect(
            Path(str(result_payload.get("golden_manifest", ""))) == golden_manifest,
            f"{root_role} result manifest must point at the promoted golden manifest for "
            f"{benchmark_id}",
        )
        expect(
            Path(str(result_payload.get("manifest", ""))) == manifest_path,
            f"{root_role} result manifest must point at the retained packet manifest for "
            f"{benchmark_id}",
        )
        captured_examples.append(
            {
                "benchmark_id": benchmark_id,
                "comparison_summary": str(comparison_summary),
                "comparison_summary_exists": comparison_summary.exists(),
                "comparison_summary_within_root": True,
                "comparison_summary_benchmark_matches": True,
                "comparison_status": comparison_status,
                "golden_manifest": str(golden_manifest),
                "golden_manifest_exists": golden_manifest.exists(),
                "golden_manifest_within_root": True,
                "golden_manifest_benchmark_matches": True,
                "golden_output_dir": str(golden_output_dir),
                "golden_output_dir_exists": golden_output_dir.is_dir(),
                "golden_output_dir_within_root": True,
                "result_manifest": str(result_manifest),
                "result_manifest_exists": result_manifest.exists(),
                "result_manifest_within_root": True,
                "result_manifest_benchmark_matches": True,
                "backup_match_ok": bool(entry.get("backup_match_ok", False)),
                "rerun_match_ok": bool(entry.get("rerun_match_ok", False)),
            }
        )

    return {
        "root": str(root),
        "role": root_role,
        "manifest": str(manifest_path),
        "summary_path": str(summary_path),
        "capture_state": capture_state,
        "captured_benchmarks": manifest_benchmarks,
        "reported_optional_capture_packet": reported_optional_packet,
        "inferred_optional_capture_packet": inferred_optional_packet,
        "effective_optional_capture_packet": effective_optional_packet,
        "captured_examples": captured_examples,
    }


def summarize_readiness(
    *,
    required_root: Path,
    optional_packet_roots: list[Path],
    qualification_path: Path,
) -> dict[str, Any]:
    qualification = load_json(qualification_path)
    phase0_examples = scaffold_phase0_examples(qualification)
    phase0_by_id = {entry["id"]: entry for entry in phase0_examples}
    case_studies = scaffold_case_studies(qualification)
    phase0_order = [entry["id"] for entry in phase0_examples]
    phase0_position = {benchmark_id: index for index, benchmark_id in enumerate(phase0_order)}

    required_capture_ids = sorted(
        entry["id"] for entry in phase0_examples if entry["required_capture"]
    )
    scaffold_reference_captured_ids = sorted(
        entry["id"]
        for entry in phase0_examples
        if entry["current_evidence_state"] == "reference-captured"
    )
    scaffold_pending_ids = sorted(
        entry["id"]
        for entry in phase0_examples
        if entry["current_evidence_state"] != "reference-captured"
    )

    packet_roots: list[dict[str, Any]] = [
        load_packet_root(root=required_root, phase0_by_id=phase0_by_id, root_role="required-root")
    ]
    for root in optional_packet_roots:
        packet_roots.append(
            load_packet_root(root=root, phase0_by_id=phase0_by_id, root_role="optional-packet")
        )

    observed_by_id: dict[str, dict[str, Any]] = {}
    for packet_root in packet_roots:
        for captured in packet_root["captured_examples"]:
            benchmark_id = captured["benchmark_id"]
            if benchmark_id in observed_by_id:
                raise RuntimeError(f"duplicate retained phase-0 evidence for benchmark {benchmark_id}")
            observed_by_id[benchmark_id] = {
                "root": packet_root["root"],
                "capture_state": packet_root["capture_state"],
                "effective_optional_capture_packet": packet_root["effective_optional_capture_packet"],
                "reported_optional_capture_packet": packet_root["reported_optional_capture_packet"],
                "inferred_optional_capture_packet": packet_root["inferred_optional_capture_packet"],
                **captured,
            }

    observed_reference_captured_ids = sorted(observed_by_id, key=lambda item: phase0_position[item])
    blocked_phase0_examples = [
        {
            "id": entry["id"],
            "next_runtime_lane": entry["next_runtime_lane"],
        }
        for entry in phase0_examples
        if entry["current_evidence_state"] != "reference-captured" and entry["next_runtime_lane"]
    ]
    blocked_case_study_families = [
        {
            "id": entry["id"],
            "next_runtime_lane": entry["next_runtime_lane"],
        }
        for entry in case_studies
        if entry["next_runtime_lane"]
    ]
    ready_optional_capture_packets: dict[str, list[str]] = {}
    for entry in phase0_examples:
        packet_id = entry["optional_capture_packet"]
        if not packet_id:
            continue
        ready_optional_capture_packets.setdefault(packet_id, []).append(entry["id"])

    required_root_reference_captured = packet_roots[0]["capture_state"] == "reference-captured"
    required_root_captures_required_set = set(required_capture_ids).issubset(
        set(packet_roots[0]["captured_benchmarks"])
    )
    required_root_only_captures_required_set = (
        set(packet_roots[0]["captured_benchmarks"]) == set(required_capture_ids)
        and len(packet_roots[0]["captured_benchmarks"]) == len(required_capture_ids)
    )
    expect(
        required_root_only_captures_required_set,
        "required-root captured benchmarks must match the accepted required phase-0 set exactly",
    )
    optional_packets_preserve_bootstrap_only_state = all(
        packet_root["capture_state"] == "bootstrap-only" for packet_root in packet_roots[1:]
    )
    optional_capture_packets_match_scaffold = all(
        bool(packet_root["effective_optional_capture_packet"])
        and set(packet_root["captured_benchmarks"])
        == set(ready_optional_capture_packets[packet_root["effective_optional_capture_packet"]])
        and len(packet_root["captured_benchmarks"])
        == len(ready_optional_capture_packets[packet_root["effective_optional_capture_packet"]])
        and not any(phase0_by_id[benchmark_id]["required_capture"]
                    for benchmark_id in packet_root["captured_benchmarks"])
        for packet_root in packet_roots[1:]
    )
    expect(
        optional_capture_packets_match_scaffold,
        "optional-packet captured benchmarks must match exactly one scaffold optional packet "
        "and stay outside the required phase-0 set",
    )
    captured_examples_publish_reference_artifacts = all(
        captured["comparison_summary_exists"]
        and captured["comparison_summary_within_root"]
        and captured["comparison_summary_benchmark_matches"]
        and captured["golden_manifest_exists"]
        and captured["golden_manifest_within_root"]
        and captured["golden_manifest_benchmark_matches"]
        and captured["golden_output_dir_exists"]
        and captured["golden_output_dir_within_root"]
        and captured["result_manifest_exists"]
        and captured["result_manifest_within_root"]
        and captured["result_manifest_benchmark_matches"]
        for captured in observed_by_id.values()
    )
    captured_examples_pass_comparison_checks = all(
        captured["comparison_status"] == "reference-captured"
        and captured["backup_match_ok"]
        and captured["rerun_match_ok"]
        for captured in observed_by_id.values()
    )
    observed_reference_captured_matches_scaffold = (
        set(observed_reference_captured_ids) == set(scaffold_reference_captured_ids)
    )
    pending_examples_preserve_runtime_lane_hints = all(
        entry["next_runtime_lane"] for entry in blocked_phase0_examples
    )
    blocked_case_study_families_preserve_runtime_lane_hints = all(
        entry["next_runtime_lane"] for entry in blocked_case_study_families
    )

    phase0_example_summaries: list[dict[str, Any]] = []
    for benchmark_id in phase0_order:
        scaffold_entry = phase0_by_id[benchmark_id]
        observed = observed_by_id.get(benchmark_id)
        phase0_example_summaries.append(
            {
                "id": benchmark_id,
                "required_capture": scaffold_entry["required_capture"],
                "scaffold_evidence_state": scaffold_entry["current_evidence_state"],
                "observed_evidence_state": "reference-captured"
                if observed
                else "cataloged-pending-capture",
                "source_root": observed["root"] if observed else "",
                "packet_capture_state": observed["capture_state"] if observed else "",
                "optional_capture_packet": (
                    observed["effective_optional_capture_packet"]
                    if observed
                    else scaffold_entry["optional_capture_packet"]
                ),
                "reported_optional_capture_packet": (
                    observed["reported_optional_capture_packet"] if observed else ""
                ),
                "inferred_optional_capture_packet": (
                    observed["inferred_optional_capture_packet"] if observed else ""
                ),
                "next_runtime_lane": scaffold_entry["next_runtime_lane"],
            }
        )

    return {
        "schema_version": 1,
        "qualification_path": str(qualification_path),
        "required_root": str(required_root),
        "optional_packet_roots": [str(root) for root in optional_packet_roots],
        "packet_roots": packet_roots,
        "phase0_examples": phase0_example_summaries,
        "case_study_family_ids": [entry["id"] for entry in case_studies],
        "blocked_case_study_families": blocked_case_study_families,
        "phase0_reference_captured_ids": observed_reference_captured_ids,
        "phase0_pending_ids": scaffold_pending_ids,
        "blocked_phase0_examples": blocked_phase0_examples,
        "ready_optional_capture_packets": ready_optional_capture_packets,
        "required_root_reference_captured": required_root_reference_captured,
        "required_root_captures_required_set": required_root_captures_required_set,
        "required_root_only_captures_required_set": required_root_only_captures_required_set,
        "optional_packets_preserve_bootstrap_only_state": optional_packets_preserve_bootstrap_only_state,
        "optional_capture_packets_match_scaffold": optional_capture_packets_match_scaffold,
        "captured_examples_publish_reference_artifacts": captured_examples_publish_reference_artifacts,
        "captured_examples_pass_comparison_checks": captured_examples_pass_comparison_checks,
        "observed_reference_captured_matches_scaffold": observed_reference_captured_matches_scaffold,
        "pending_examples_preserve_runtime_lane_hints": pending_examples_preserve_runtime_lane_hints,
        "blocked_case_study_families_preserve_runtime_lane_hints": (
            blocked_case_study_families_preserve_runtime_lane_hints
        ),
    }


def write_packet_root(
    *,
    root: Path,
    capture_state: str,
    benchmark_ids: list[str],
    optional_capture_packet: str = "",
) -> None:
    manifests_dir = root / "manifests"
    state_dir = root / "state"
    comparisons_dir = root / "comparisons" / "phase0"
    goldens_dir = root / "goldens" / "phase0"
    results_dir = root / "results" / "phase0"
    manifests_dir.mkdir(parents=True, exist_ok=True)
    state_dir.mkdir(parents=True, exist_ok=True)
    comparisons_dir.mkdir(parents=True, exist_ok=True)
    goldens_dir.mkdir(parents=True, exist_ok=True)
    results_dir.mkdir(parents=True, exist_ok=True)

    manifest_path = manifests_dir / "phase0-reference.json"
    summary_path = state_dir / "phase0-reference.capture.json"
    manifest = {
        "schema_version": 1,
        "run_id": "phase0-reference",
        "created_at_utc": "2026-04-22T00:00:00Z",
        "phase0": {
            "capture_state": capture_state,
            "capture_summary": str(summary_path),
            "captured_benchmarks": benchmark_ids,
        },
    }
    write_json(manifest_path, manifest)

    benchmark_summaries: list[dict[str, Any]] = []
    for benchmark_id in benchmark_ids:
        comparison_summary = comparisons_dir / f"{benchmark_id}.summary.json"
        golden_manifest = goldens_dir / benchmark_id / "golden-manifest.json"
        golden_output_dir = goldens_dir / benchmark_id / "captured"
        result_manifest = results_dir / benchmark_id / "result-manifest.json"
        golden_output_dir.mkdir(parents=True, exist_ok=True)
        write_json(
            comparison_summary,
            {
                "benchmark_id": benchmark_id,
                "status": "reference-captured",
                "reference_golden": str(golden_manifest),
                "latest_run_result": str(result_manifest),
            },
        )
        write_json(
            golden_manifest,
            {
                "benchmark_id": benchmark_id,
                "golden_output_dir": str(golden_output_dir),
            },
        )
        write_json(
            result_manifest,
            {
                "benchmark_id": benchmark_id,
                "golden_manifest": str(golden_manifest),
                "manifest": str(manifest_path),
            },
        )
        benchmark_summaries.append(
            {
                "benchmark_id": benchmark_id,
                "comparison_summary": str(comparison_summary),
                "golden_manifest": str(golden_manifest),
                "result_manifest": str(result_manifest),
                "status": "reference-captured",
                "backup_match_ok": True,
                "rerun_match_ok": True,
            }
        )

    summary = {
        "schema_version": 1,
        "root": str(root),
        "manifest": str(manifest_path),
        "summary_path": str(summary_path),
        "capture_state": capture_state,
        "selected_benchmarks": benchmark_ids,
        "required_only": False,
        "resume_existing": False,
        "benchmark_summaries": benchmark_summaries,
    }
    if optional_capture_packet:
        summary["optional_capture_packet"] = optional_capture_packet
    write_json(summary_path, summary)


def run_self_check(qualification_path: Path) -> dict[str, Any]:
    with tempfile.TemporaryDirectory(prefix="amflow-qualification-readiness-self-check-") as tmp:
        temp_root = Path(tmp)
        required_root = temp_root / "required-reference-captured"
        de_d0_root = temp_root / "optional-de-d0-pair"
        user_hook_root = temp_root / "optional-user-hook-pair"
        summary_path = temp_root / "qualification-summary.json"

        write_packet_root(
            root=required_root,
            capture_state="reference-captured",
            benchmark_ids=["automatic_vs_manual", "automatic_loop"],
        )
        write_packet_root(
            root=de_d0_root,
            capture_state="bootstrap-only",
            benchmark_ids=["differential_equation_solver", "spacetime_dimension"],
        )
        write_packet_root(
            root=user_hook_root,
            capture_state="bootstrap-only",
            benchmark_ids=["user_defined_amfmode", "user_defined_ending"],
            optional_capture_packet="user-hook-pair",
        )

        summary = summarize_readiness(
            required_root=required_root,
            optional_packet_roots=[de_d0_root, user_hook_root],
            qualification_path=qualification_path,
        )
        write_json(summary_path, summary)
        return {
            "required_root_reference_captured": summary["required_root_reference_captured"],
            "required_root_captures_required_set": summary["required_root_captures_required_set"],
            "required_root_only_captures_required_set": (
                summary["required_root_only_captures_required_set"]
            ),
            "optional_packets_preserve_bootstrap_only_state": (
                summary["optional_packets_preserve_bootstrap_only_state"]
            ),
            "optional_capture_packet_inferred_from_scaffold": any(
                packet["inferred_optional_capture_packet"] == "de-d0-pair"
                and not packet["reported_optional_capture_packet"]
                for packet in summary["packet_roots"]
            ),
            "optional_capture_packets_match_scaffold": summary["optional_capture_packets_match_scaffold"],
            "captured_examples_publish_reference_artifacts": (
                summary["captured_examples_publish_reference_artifacts"]
            ),
            "captured_examples_pass_comparison_checks": (
                summary["captured_examples_pass_comparison_checks"]
            ),
            "observed_reference_captured_matches_scaffold": (
                summary["observed_reference_captured_matches_scaffold"]
            ),
            "pending_examples_preserve_runtime_lane_hints": (
                summary["pending_examples_preserve_runtime_lane_hints"]
            ),
            "blocked_case_study_families_preserve_runtime_lane_hints": (
                summary["blocked_case_study_families_preserve_runtime_lane_hints"]
            ),
            "summary_written": summary_path.exists(),
        }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--root",
        type=Path,
        required=True,
        help="Accepted required-set retained root (the M0b reference-captured packet)",
    )
    parser.add_argument(
        "--optional-packet-root",
        action="append",
        type=Path,
        help="Additional retained optional packet roots to aggregate",
    )
    parser.add_argument(
        "--qualification-path",
        type=Path,
        help="Qualification scaffold JSON path",
    )
    parser.add_argument(
        "--summary-path",
        type=Path,
        help="Optional output file for the aggregated readiness summary",
    )
    parser.add_argument(
        "--self-check",
        action="store_true",
        help="Run a synthetic retained-packet aggregation check",
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
        print(json.dumps(run_self_check(qualification_path), indent=2, sort_keys=True))
        return 0

    summary = summarize_readiness(
        required_root=args.root,
        optional_packet_roots=args.optional_packet_root or [],
        qualification_path=qualification_path,
    )
    if args.summary_path is not None:
        write_json(args.summary_path, summary)
    print(json.dumps(summary, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
