#!/usr/bin/env python3
"""Validate M6 qualification scaffold readiness against retained phase-0 packets."""

from __future__ import annotations

import argparse
import json
import re
import tempfile
from pathlib import Path
from typing import Any

from freeze_phase0_goldens import load_json, normalize_capture_packet


REFERENCE_CAPTURED = "reference-captured"
BOOTSTRAP_ONLY = "bootstrap-only"
PENDING_CAPTURE = "cataloged-pending-capture"
CAPTURED_PACKET_ROOT = re.compile(r"^phase0-reference-captured-\d{8}-(.+)$")


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def locate_manifest(root: Path) -> Path:
    explicit = root / "manifests" / "phase0-reference.json"
    if explicit.exists():
        return explicit
    manifests = sorted((root / "manifests").glob("*.json"))
    expect(len(manifests) == 1, f"could not infer manifest under {root}")
    return manifests[0]


def normalize_evidence_state(raw_state: Any) -> str:
    if not isinstance(raw_state, str):
        raise TypeError(f"current_evidence_state must be a string, got {type(raw_state).__name__}")
    state = raw_state.strip()
    expect(
        state in {REFERENCE_CAPTURED, PENDING_CAPTURE},
        f"unsupported current_evidence_state: {raw_state!r}",
    )
    return state


def infer_packet_label(root: Path, summary: dict[str, Any]) -> str:
    packet = normalize_capture_packet(summary.get("optional_capture_packet"))
    if packet:
        return packet
    if bool(summary.get("required_only", False)):
        return "required-set"
    match = CAPTURED_PACKET_ROOT.fullmatch(root.name)
    if match is None:
        raise RuntimeError(f"could not infer optional capture packet from root name: {root}")
    return normalize_capture_packet(match.group(1))


def load_scaffold(scaffold_path: Path) -> tuple[dict[str, Any], dict[str, dict[str, Any]]]:
    scaffold = load_json(scaffold_path)
    raw_phase0_entries = scaffold.get("phase0_example_classes", [])
    expect(isinstance(raw_phase0_entries, list), "phase0_example_classes must be a list")

    by_catalog_id: dict[str, dict[str, Any]] = {}
    for raw_entry in raw_phase0_entries:
        expect(isinstance(raw_entry, dict), "phase0_example_classes entries must be objects")
        catalog_id = str(raw_entry.get("phase0_catalog_id", "")).strip()
        expect(catalog_id, "phase0_catalog_id must not be empty")
        expect(catalog_id not in by_catalog_id, f"duplicate phase0_catalog_id in scaffold: {catalog_id}")
        entry = dict(raw_entry)
        entry["phase0_catalog_id"] = catalog_id
        entry["required_capture"] = bool(raw_entry.get("required_capture", False))
        entry["current_evidence_state"] = normalize_evidence_state(
            raw_entry.get("current_evidence_state", "")
        )
        entry["optional_capture_packet"] = normalize_capture_packet(
            raw_entry.get("optional_capture_packet")
        )
        entry["next_runtime_lane"] = str(raw_entry.get("next_runtime_lane", "")).strip()
        if entry["required_capture"]:
            expect(
                not entry["optional_capture_packet"] and not entry["next_runtime_lane"],
                f"required_capture scaffold entry {catalog_id} should not carry optional or runtime-lane hints",
            )
        elif entry["current_evidence_state"] == REFERENCE_CAPTURED:
            expect(
                bool(entry["optional_capture_packet"]),
                f"captured optional scaffold entry {catalog_id} should declare optional_capture_packet",
            )
            expect(
                not entry["next_runtime_lane"],
                f"captured optional scaffold entry {catalog_id} should not carry next_runtime_lane",
            )
        else:
            expect(
                bool(entry["next_runtime_lane"]),
                f"pending scaffold entry {catalog_id} should declare next_runtime_lane",
            )
            expect(
                not entry["optional_capture_packet"],
                f"pending scaffold entry {catalog_id} should not declare optional_capture_packet",
            )
        by_catalog_id[catalog_id] = entry

    return scaffold, by_catalog_id


def validate_capture_packet(
    root: Path,
    scaffold_entries: dict[str, dict[str, Any]],
) -> dict[str, Any]:
    manifest_path = locate_manifest(root)
    manifest = load_json(manifest_path)
    summary_path = Path(manifest["phase0"]["capture_summary"])
    expect(summary_path.exists(), f"capture summary missing for retained packet {root}")
    summary = load_json(summary_path)
    expect(Path(summary["manifest"]) == manifest_path, f"capture summary manifest mismatch for {root}")
    expect(Path(summary["root"]) == root, f"capture summary root mismatch for {root}")
    expect(summary.get("capture_state", "") == manifest["phase0"]["capture_state"],
           f"capture summary state mismatch for {root}")

    packet_label = infer_packet_label(root, summary)
    manifest_capture_state = str(manifest["phase0"]["capture_state"])
    benchmark_summaries = summary.get("benchmark_summaries", [])
    expect(isinstance(benchmark_summaries, list), f"benchmark_summaries must be a list for {root}")

    golden_index = load_json(Path(manifest["phase0"]["golden_index"]))
    golden_entries = {
        str(entry.get("benchmark_id", "")): entry
        for entry in golden_index.get("benchmarks", [])
        if isinstance(entry, dict)
    }

    summary_ids: list[str] = []
    for benchmark_summary in benchmark_summaries:
        benchmark_id = str(benchmark_summary.get("benchmark_id", "")).strip()
        expect(benchmark_id in scaffold_entries, f"captured benchmark {benchmark_id!r} is not in the scaffold")
        expect(benchmark_id not in summary_ids, f"duplicate captured benchmark {benchmark_id} in {root}")
        summary_ids.append(benchmark_id)

        expect(
            benchmark_summary.get("status") == REFERENCE_CAPTURED,
            f"captured benchmark {benchmark_id} in {root} must stay reference-captured",
        )
        comparison_path = Path(benchmark_summary["comparison_summary"])
        golden_manifest_path = Path(benchmark_summary["golden_manifest"])
        result_manifest_path = Path(benchmark_summary["result_manifest"])
        expect(comparison_path.exists(), f"comparison summary missing for {benchmark_id} in {root}")
        expect(golden_manifest_path.exists(), f"golden manifest missing for {benchmark_id} in {root}")
        expect(result_manifest_path.exists(), f"result manifest missing for {benchmark_id} in {root}")

        comparison = load_json(comparison_path)
        expect(
            comparison.get("benchmark_id") == benchmark_id,
            f"comparison summary benchmark mismatch for {benchmark_id} in {root}",
        )
        expect(
            comparison.get("status") == REFERENCE_CAPTURED,
            f"comparison summary for {benchmark_id} in {root} must stay reference-captured",
        )

        golden_index_entry = golden_entries.get(benchmark_id)
        expect(golden_index_entry is not None, f"golden index entry missing for {benchmark_id} in {root}")
        expect(
            golden_index_entry.get("status") == REFERENCE_CAPTURED,
            f"golden index entry for {benchmark_id} in {root} must stay reference-captured",
        )

    manifest_captured = {
        str(benchmark_id).strip()
        for benchmark_id in manifest["phase0"].get("captured_benchmarks", [])
    }
    summary_selected = {
        str(benchmark_id).strip() for benchmark_id in summary.get("selected_benchmarks", [])
    }
    expect(manifest_captured == set(summary_ids),
           f"manifest captured_benchmarks mismatch for {root}")
    expect(summary_selected == set(summary_ids), f"capture summary selected_benchmarks mismatch for {root}")

    if bool(summary.get("required_only", False)):
        expect(
            manifest_capture_state == REFERENCE_CAPTURED,
            f"required capture packet {packet_label} must advance phase0.capture_state to reference-captured",
        )
    else:
        expect(
            manifest_capture_state in {BOOTSTRAP_ONLY, REFERENCE_CAPTURED},
            f"optional capture packet {packet_label} has unsupported capture_state {manifest_capture_state!r}",
        )

    return {
        "root": str(root),
        "packet_label": packet_label,
        "required_only": bool(summary.get("required_only", False)),
        "capture_state": manifest_capture_state,
        "benchmark_ids": summary_ids,
    }


def validate_qualification_readiness(
    roots: list[Path],
    *,
    scaffold_path: Path,
) -> dict[str, Any]:
    scaffold, scaffold_entries = load_scaffold(scaffold_path)
    packets = [validate_capture_packet(root, scaffold_entries) for root in roots]

    coverage: dict[str, list[dict[str, Any]]] = {catalog_id: [] for catalog_id in scaffold_entries}
    for packet in packets:
        for benchmark_id in packet["benchmark_ids"]:
            coverage[benchmark_id].append(packet)

    required_capture_complete = True
    optional_annotations_match = True
    pending_lanes_match = True
    phase0_readiness: list[dict[str, Any]] = []
    for benchmark_id in sorted(scaffold_entries):
        entry = scaffold_entries[benchmark_id]
        packet_records = coverage[benchmark_id]
        packet_labels = sorted({record["packet_label"] for record in packet_records})
        captured = bool(packet_records)

        if entry["required_capture"]:
            has_required_reference_packet = any(
                record["required_only"] and record["capture_state"] == REFERENCE_CAPTURED
                for record in packet_records
            )
            required_capture_complete = required_capture_complete and has_required_reference_packet
            expect(
                has_required_reference_packet,
                f"required scaffold benchmark {benchmark_id} is missing a reference-captured required packet",
            )
        elif entry["optional_capture_packet"]:
            has_expected_optional_packet = entry["optional_capture_packet"] in packet_labels
            optional_annotations_match = optional_annotations_match and has_expected_optional_packet
            expect(
                has_expected_optional_packet,
                f"captured optional scaffold benchmark {benchmark_id} is missing packet "
                f"{entry['optional_capture_packet']}",
            )
        else:
            pending_lanes_match = pending_lanes_match and not captured
            expect(
                not captured,
                f"pending scaffold benchmark {benchmark_id} unexpectedly already has retained capture coverage",
            )

        phase0_readiness.append(
            {
                "benchmark_id": benchmark_id,
                "captured": captured,
                "current_evidence_state": entry["current_evidence_state"],
                "next_runtime_lane": entry["next_runtime_lane"],
                "optional_capture_packet": entry["optional_capture_packet"],
                "packet_labels": packet_labels,
                "required_capture": entry["required_capture"],
            }
        )

    captured_phase0_ids = sorted(
        benchmark_id for benchmark_id, packet_records in coverage.items() if packet_records
    )
    pending_phase0_ids = sorted(
        benchmark_id
        for benchmark_id, entry in scaffold_entries.items()
        if entry["current_evidence_state"] == PENDING_CAPTURE
    )

    return {
        "root_count": len(roots),
        "captured_packet_labels": sorted({packet["packet_label"] for packet in packets}),
        "captured_phase0_ids": captured_phase0_ids,
        "pending_phase0_ids": pending_phase0_ids,
        "required_capture_complete": required_capture_complete,
        "optional_packet_annotations_match_captured_benchmarks": optional_annotations_match,
        "pending_runtime_lane_annotations_match_uncaptured_benchmarks": pending_lanes_match,
        "phase0_example_class_count": len(scaffold_entries),
        "case_study_family_count": len(scaffold.get("case_study_families", [])),
        "required_failure_code_profile_count": len(
            scaffold.get("required_failure_code_profiles", [])
        ),
        "known_regression_profile_count": len(scaffold.get("known_regression_profiles", [])),
        "phase0_example_readiness": phase0_readiness,
    }


def write_capture_packet(
    root: Path,
    benchmark_ids: list[str],
    *,
    capture_state: str,
    required_only: bool,
    optional_capture_packet: str = "",
    include_optional_capture_packet_field: bool = True,
) -> None:
    (root / "manifests").mkdir(parents=True, exist_ok=True)
    (root / "state").mkdir(parents=True, exist_ok=True)
    (root / "comparisons" / "phase0").mkdir(parents=True, exist_ok=True)
    (root / "results" / "phase0").mkdir(parents=True, exist_ok=True)
    (root / "goldens" / "phase0").mkdir(parents=True, exist_ok=True)

    benchmark_summaries: list[dict[str, Any]] = []
    golden_entries: list[dict[str, Any]] = []
    for benchmark_id in benchmark_ids:
        comparison_path = root / "comparisons" / "phase0" / f"{benchmark_id}.summary.json"
        result_manifest_path = root / "results" / "phase0" / benchmark_id / "result-manifest.json"
        golden_manifest_path = root / "goldens" / "phase0" / benchmark_id / "golden-manifest.json"
        result_manifest_path.parent.mkdir(parents=True, exist_ok=True)
        golden_manifest_path.parent.mkdir(parents=True, exist_ok=True)
        comparison_path.write_text(
            json.dumps(
                {
                    "benchmark_id": benchmark_id,
                    "schema_version": 1,
                    "status": REFERENCE_CAPTURED,
                },
                indent=2,
                sort_keys=True,
            )
            + "\n",
            encoding="utf-8",
        )
        result_manifest_path.write_text("{\"schema_version\": 1}\n", encoding="utf-8")
        golden_manifest_path.write_text("{\"schema_version\": 1}\n", encoding="utf-8")

        benchmark_summaries.append(
            {
                "backup_match_ok": True,
                "benchmark_id": benchmark_id,
                "comparison_summary": str(comparison_path),
                "golden_manifest": str(golden_manifest_path),
                "rerun_match_ok": True,
                "result_manifest": str(result_manifest_path),
                "status": REFERENCE_CAPTURED,
            }
        )
        golden_entries.append(
            {
                "benchmark_id": benchmark_id,
                "comparison_summary": str(comparison_path),
                "golden_manifest": str(golden_manifest_path),
                "result_manifest": str(result_manifest_path),
                "status": REFERENCE_CAPTURED,
            }
        )

    golden_index_path = root / "goldens" / "phase0" / "index.json"
    golden_index_path.write_text(
        json.dumps({"benchmarks": golden_entries, "schema_version": 1}, indent=2, sort_keys=True)
        + "\n",
        encoding="utf-8",
    )

    summary_path = root / "state" / "phase0-reference.capture.json"
    manifest_path = root / "manifests" / "phase0-reference.json"
    summary: dict[str, Any] = {
        "benchmark_summaries": benchmark_summaries,
        "capture_state": capture_state,
        "manifest": str(manifest_path),
        "required_only": required_only,
        "root": str(root),
        "schema_version": 1,
        "selected_benchmarks": benchmark_ids,
        "summary_path": str(summary_path),
    }
    if optional_capture_packet and include_optional_capture_packet_field:
        summary["optional_capture_packet"] = optional_capture_packet
    summary_path.write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n", encoding="utf-8")

    manifest = {
        "phase0": {
            "benchmark_catalog": str(root / "templates" / "phase0-benchmarks.json"),
            "capture_state": capture_state,
            "capture_summary": str(summary_path),
            "captured_benchmarks": sorted(benchmark_ids),
            "golden_index": str(golden_index_path),
            "placeholder_goldens_frozen": True,
        },
        "schema_version": 1,
    }
    manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def run_self_check() -> dict[str, Any]:
    repo_root = Path(__file__).resolve().parents[3]
    scaffold_path = repo_root / "tools" / "reference-harness" / "templates" / "qualification-benchmarks.json"

    with tempfile.TemporaryDirectory(prefix="amflow-qualification-scaffold-self-check-") as tmp:
        temp_root = Path(tmp)
        required_root = temp_root / "phase0-reference-captured-20260419-required-set"
        de_d0_root = temp_root / "phase0-reference-captured-20260422-de-d0-pair"
        user_hook_root = temp_root / "phase0-reference-captured-20260422-user-hook-pair"

        write_capture_packet(
            required_root,
            ["automatic_vs_manual", "automatic_loop"],
            capture_state=REFERENCE_CAPTURED,
            required_only=True,
        )
        write_capture_packet(
            de_d0_root,
            ["differential_equation_solver", "spacetime_dimension"],
            capture_state=BOOTSTRAP_ONLY,
            required_only=False,
            optional_capture_packet="de-d0-pair",
            include_optional_capture_packet_field=False,
        )
        write_capture_packet(
            user_hook_root,
            ["user_defined_amfmode", "user_defined_ending"],
            capture_state=BOOTSTRAP_ONLY,
            required_only=False,
            optional_capture_packet="user-hook-pair",
        )

        summary = validate_qualification_readiness(
            [required_root, de_d0_root, user_hook_root],
            scaffold_path=scaffold_path,
        )

        bad_required_packet_rejected = False
        try:
            bad_required_root = temp_root / "phase0-reference-captured-20260423-required-set"
            write_capture_packet(
                bad_required_root,
                ["automatic_vs_manual", "automatic_loop"],
                capture_state=BOOTSTRAP_ONLY,
                required_only=True,
            )
            validate_qualification_readiness([bad_required_root], scaffold_path=scaffold_path)
        except RuntimeError as exc:
            bad_required_packet_rejected = (
                str(exc)
                == "required capture packet required-set must advance phase0.capture_state to reference-captured"
            )

        mismatched_optional_packet_rejected = False
        try:
            wrong_optional_root = temp_root / "phase0-reference-captured-20260423-wrong-pair"
            write_capture_packet(
                wrong_optional_root,
                ["differential_equation_solver", "spacetime_dimension"],
                capture_state=BOOTSTRAP_ONLY,
                required_only=False,
                optional_capture_packet="wrong-pair",
                include_optional_capture_packet_field=False,
            )
            validate_qualification_readiness(
                [required_root, wrong_optional_root, user_hook_root],
                scaffold_path=scaffold_path,
            )
        except RuntimeError as exc:
            mismatched_optional_packet_rejected = (
                str(exc)
                == "captured optional scaffold benchmark differential_equation_solver is missing packet de-d0-pair"
            )

        unknown_benchmark_rejected = False
        try:
            unknown_root = temp_root / "phase0-reference-captured-20260423-unknown-pair"
            write_capture_packet(
                unknown_root,
                ["imaginary_example"],
                capture_state=BOOTSTRAP_ONLY,
                required_only=False,
                optional_capture_packet="unknown-pair",
            )
            validate_qualification_readiness([required_root, unknown_root], scaffold_path=scaffold_path)
        except RuntimeError as exc:
            unknown_benchmark_rejected = (
                str(exc) == "captured benchmark 'imaginary_example' is not in the scaffold"
            )

        return {
            "captured_phase0_ids_match_expected_set": summary["captured_phase0_ids"]
            == [
                "automatic_loop",
                "automatic_vs_manual",
                "differential_equation_solver",
                "spacetime_dimension",
                "user_defined_amfmode",
                "user_defined_ending",
            ],
            "pending_phase0_ids_match_expected_set": summary["pending_phase0_ids"]
            == [
                "automatic_phasespace",
                "complex_kinematics",
                "feynman_prescription",
                "linear_propagator",
            ],
            "required_capture_complete": summary["required_capture_complete"],
            "optional_packet_label_inferred_from_root_name": "de-d0-pair"
            in summary["captured_packet_labels"],
            "optional_packet_annotations_match_captured_benchmarks": summary[
                "optional_packet_annotations_match_captured_benchmarks"
            ],
            "pending_runtime_lane_annotations_match_uncaptured_benchmarks": summary[
                "pending_runtime_lane_annotations_match_uncaptured_benchmarks"
            ],
            "bad_required_packet_rejected": bad_required_packet_rejected,
            "mismatched_optional_packet_rejected": mismatched_optional_packet_rejected,
            "unknown_benchmark_rejected": unknown_benchmark_rejected,
        }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--root",
        type=Path,
        action="append",
        required=True,
        help="Retained phase-0 capture packet root; pass once per packet",
    )
    parser.add_argument(
        "--scaffold",
        type=Path,
        help="Qualification scaffold JSON path; defaults to the repo template",
    )
    parser.add_argument(
        "--self-check",
        action="store_true",
        help="Run synthetic readiness validation against mock retained packet roots",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.self_check:
        print(json.dumps(run_self_check(), indent=2, sort_keys=True))
        return 0

    repo_root = Path(__file__).resolve().parents[3]
    scaffold_path = (
        args.scaffold
        if args.scaffold is not None
        else repo_root / "tools" / "reference-harness" / "templates" / "qualification-benchmarks.json"
    )
    summary = validate_qualification_readiness(args.root, scaffold_path=scaffold_path)
    print(json.dumps(summary, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
