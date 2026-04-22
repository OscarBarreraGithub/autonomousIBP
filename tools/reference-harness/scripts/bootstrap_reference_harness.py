#!/usr/bin/env python3
"""Bootstrap the phase-0 AMFlow reference harness."""

from __future__ import annotations

import argparse
import json
import re
import shutil
import tempfile
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from fetch_upstream_amflow import fetch_upstream_inputs
from freeze_phase0_goldens import freeze_phase0_placeholders


def ensure_dir(path: Path, dry_run: bool) -> None:
    if dry_run:
        return
    path.mkdir(parents=True, exist_ok=True)


def write_text(path: Path, content: str, dry_run: bool) -> None:
    if dry_run:
        return
    path.write_text(content, encoding="utf-8")


def copy_templates(source_dir: Path, destination_dir: Path, dry_run: bool) -> list[str]:
    copied: list[str] = []
    ensure_dir(destination_dir, dry_run)
    for template_path in sorted(source_dir.glob("*")):
        if not template_path.is_file():
            continue
        copied.append(template_path.name)
        if dry_run:
            continue
        shutil.copyfile(template_path, destination_dir / template_path.name)
    return copied


def load_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def load_yaml_string_list(path: Path, key: str) -> list[str]:
    lines = path.read_text(encoding="utf-8").splitlines()
    key_indent: int | None = None
    values: list[str] = []

    for line in lines:
        stripped = line.strip()
        current_indent = len(line) - len(line.lstrip(" "))
        if key_indent is None:
            if stripped == f"{key}:":
                key_indent = current_indent
            continue

        if stripped and current_indent <= key_indent:
            break
        if stripped.startswith("- ") and current_indent == key_indent + 2:
            value = stripped[2:].strip()
            if value.startswith(("'", '"')) and value.endswith(("'", '"')):
                value = value[1:-1]
            elif " #" in value:
                value = value.split(" #", 1)[0].rstrip()
            values.append(value)

    expect(key_indent is not None, f"failed to locate YAML list {key} in {path}")
    return values


def load_selected_benchmark_metadata(path: Path) -> tuple[set[str], set[str], dict[str, str | None]]:
    literature_ids: set[str] = set()
    qualification_ids: set[str] = set()
    qualification_anchor_refs: dict[str, str | None] = {}
    section = ""
    current_qualification_id: str | None = None

    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if line.startswith("## "):
            section = line[3:]
            current_qualification_id = None
            continue

        match = re.match(r"- `([^`]+)`$", line)
        if match is None:
            if section == "Qualification Scaffold IDs" and current_qualification_id is not None:
                anchor_refs = [
                    token for token in re.findall(r"`([^`]+)`", line) if token in literature_ids
                ]
                if anchor_refs:
                    qualification_anchor_refs[current_qualification_id] = anchor_refs[0]
            continue

        benchmark_id = match.group(1)
        if section in {"Direct AMFlow Use Or Clear Build-On", "Strong Adjacent Benchmarks"}:
            literature_ids.add(benchmark_id)
            current_qualification_id = None
        elif section == "Qualification Scaffold IDs":
            qualification_ids.add(benchmark_id)
            qualification_anchor_refs[benchmark_id] = None
            current_qualification_id = benchmark_id

    expect(literature_ids, f"failed to locate selected benchmark literature ids in {path}")
    expect(qualification_ids, f"failed to locate qualification scaffold ids in {path}")
    return literature_ids, qualification_ids, qualification_anchor_refs


def load_verification_strategy_digit_thresholds(path: Path) -> dict[str, int]:
    thresholds: dict[str, int] = {}

    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        default_match = re.match(r"- `>= ([0-9]+)` correct digits on core package families$", line)
        if default_match:
            thresholds["core-package-family-default"] = int(default_match.group(1))
            continue

        literature_match = re.match(r"- `>= ([0-9]+)` digits on `([^`]+)`$", line)
        if literature_match:
            thresholds[literature_match.group(2)] = int(literature_match.group(1))

    expect(thresholds, f"failed to locate digit-threshold defaults in {path}")
    return thresholds


def load_recorded_batch_ids(path: Path) -> set[str]:
    batch_ids: set[str] = set()
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        match = re.match(r"^\| `Batch ([0-9A-Za-z.]+)` \|", raw_line)
        if match:
            batch_ids.add("b" + match.group(1))
    expect(batch_ids, f"failed to locate any batch rows in {path}")
    return batch_ids


THEORY_BLOCKED_PHASE0_RUNTIME_LANES = {
    "automatic_phasespace": "b63k",
    "complex_kinematics": "b61n",
    "feynman_prescription": "b63k",
    "linear_propagator": "b64k",
}

THEORY_BLOCKED_CASE_STUDY_RUNTIME_LANES = {
    "one-singular-endpoint-case": "b62n",
}

LANDED_PHASE0_RUNTIME_PREDECESSORS = {
    "automatic_phasespace": "b63j",
    "complex_kinematics": "b61j",
    "feynman_prescription": "b63j",
    "linear_propagator": "b64j",
}

LANDED_CASE_STUDY_RUNTIME_PREDECESSORS = {
    "one-singular-endpoint-case": "b62m",
}

READY_OPTIONAL_CAPTURED_PHASE0_EXAMPLES = {
    "differential_equation_solver",
    "spacetime_dimension",
    "user_defined_amfmode",
    "user_defined_ending",
}

READY_OPTIONAL_PENDING_PHASE0_EXAMPLES: set[str] = set()

OPTIONAL_CAPTURE_PACKET_HINTS = {
    "differential_equation_solver": "de-d0-pair",
    "spacetime_dimension": "de-d0-pair",
    "user_defined_amfmode": "user-hook-pair",
    "user_defined_ending": "user-hook-pair",
}


def build_manifest(
    *,
    template_path: Path,
    manifest_name: str,
    root: Path,
    layout: dict[str, Path],
    upstream: dict[str, Any],
    args: argparse.Namespace,
    benchmark_catalog_path: Path,
    golden_index_path: Path,
) -> dict[str, Any]:
    manifest = load_json(template_path)
    manifest["run_id"] = manifest_name
    manifest["created_at_utc"] = datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")
    manifest["environment"]["platform"] = args.platform
    manifest["environment"]["threads"] = args.threads
    manifest["environment"]["work_root"] = str(root)

    manifest["upstream"]["amflow"]["source"]["kind"] = upstream["amflow"]["kind"]
    manifest["upstream"]["amflow"]["source"]["url"] = upstream["amflow"].get("url", "")
    manifest["upstream"]["amflow"]["source"]["path"] = upstream["amflow"].get("path", "")
    manifest["upstream"]["amflow"]["source"]["requested_ref"] = upstream["amflow"].get(
        "requested_ref",
        "",
    )
    manifest["upstream"]["amflow"]["source"]["resolved_commit"] = upstream["amflow"].get(
        "resolved_commit",
        "",
    )
    manifest["upstream"]["amflow"]["source"]["status"] = upstream["amflow"].get("status", "")

    manifest["upstream"]["cpc_archive"]["source"]["kind"] = upstream["cpc_archive"]["kind"]
    manifest["upstream"]["cpc_archive"]["source"]["url"] = upstream["cpc_archive"].get("url", "")
    manifest["upstream"]["cpc_archive"]["source"]["path"] = upstream["cpc_archive"].get(
        "source_path",
        upstream["cpc_archive"].get("archive_path", ""),
    )
    manifest["upstream"]["cpc_archive"]["source"]["archive_path"] = upstream["cpc_archive"].get(
        "archive_path",
        "",
    )
    manifest["upstream"]["cpc_archive"]["source"]["extracted_path"] = upstream["cpc_archive"].get(
        "extracted_path",
        "",
    )
    manifest["upstream"]["cpc_archive"]["source"]["sha256"] = upstream["cpc_archive"].get(
        "sha256",
        "",
    )
    manifest["upstream"]["cpc_archive"]["source"]["status"] = upstream["cpc_archive"].get(
        "status",
        "",
    )

    manifest["upstream"]["kira"]["version"] = args.kira_version
    manifest["upstream"]["kira"]["executable"] = str(args.kira_executable) if args.kira_executable else ""
    manifest["upstream"]["fermat"]["version"] = args.fermat_version
    manifest["upstream"]["fermat"]["executable"] = (
        str(args.fermat_executable) if args.fermat_executable else ""
    )
    manifest["upstream"]["wolfram"]["kernel"] = str(args.wolfram_kernel) if args.wolfram_kernel else ""
    manifest["upstream"]["wolfram"]["mathematica_version"] = args.mathematica_version

    manifest["layout"] = {key: str(value) for key, value in layout.items()}
    manifest["phase0"]["benchmark_catalog"] = str(benchmark_catalog_path)
    manifest["phase0"]["golden_index"] = str(golden_index_path)
    manifest["phase0"]["placeholder_goldens_frozen"] = bool(args.freeze_placeholders)
    manifest["phase0"]["capture_state"] = "bootstrap-only"
    manifest["phase0"]["capture_summary"] = ""
    manifest["phase0"]["captured_benchmarks"] = []
    return manifest


def bootstrap_reference_harness(args: argparse.Namespace) -> dict[str, Any]:
    root = args.root
    manifest_name = args.manifest_name
    script_root = Path(__file__).resolve().parent.parent
    template_source_dir = script_root / "templates"

    layout = {
        "manifests": root / "manifests",
        "logs": root / "logs",
        "generated-config": root / "generated-config",
        "results": root / "results",
        "comparisons": root / "comparisons",
        "goldens": root / "goldens",
        "templates": root / "templates",
        "state": root / "state",
        "inputs": root / "inputs",
        "upstream": root / "inputs" / "upstream",
        "downloads": root / "inputs" / "downloads",
        "extracted": root / "inputs" / "extracted",
    }

    for path in layout.values():
        ensure_dir(path, args.dry_run)

    copied_templates = copy_templates(template_source_dir, layout["templates"], args.dry_run)
    upstream = fetch_upstream_inputs(
        root=root,
        amflow_url=args.amflow_url,
        amflow_ref=args.amflow_ref,
        amflow_path=args.amflow_path,
        cpc_url=args.cpc_url,
        cpc_archive_path=args.cpc_archive_path,
        dry_run=args.dry_run,
    )

    manifest_path = layout["manifests"] / f"{manifest_name}.json"
    benchmark_catalog_path = layout["templates"] / "phase0-benchmarks.json"
    runtime_benchmark_catalog_path = (
        benchmark_catalog_path if not args.dry_run else template_source_dir / "phase0-benchmarks.json"
    )
    golden_index_path = layout["goldens"] / "phase0" / "index.json"

    manifest = build_manifest(
        template_path=template_source_dir / "reference-manifest.template.json",
        manifest_name=manifest_name,
        root=root,
        layout=layout,
        upstream=upstream,
        args=args,
        benchmark_catalog_path=benchmark_catalog_path,
        golden_index_path=golden_index_path,
    )
    write_text(manifest_path, json.dumps(manifest, indent=2, sort_keys=True) + "\n", args.dry_run)

    placeholder_summary: dict[str, Any] = {
        "benchmark_count": 0,
        "index_path": str(golden_index_path),
        "refresh_mode": "placeholder-only",
        "refreshed_placeholder_files": 0,
        "preserved_existing_files": 0,
        "benchmarks": [],
    }
    if args.freeze_placeholders:
        placeholder_summary = freeze_phase0_placeholders(
            root=root,
            manifest_path=manifest_path,
            benchmark_catalog=runtime_benchmark_catalog_path,
            template_dir=layout["templates"] if not args.dry_run else template_source_dir,
            dry_run=args.dry_run,
            force=False,
        )

    state_summary_path = layout["state"] / f"{manifest_name}.bootstrap.json"
    state_summary = {
        "root": str(root),
        "manifest": str(manifest_path),
        "state_summary_path": str(state_summary_path),
        "copied_templates": copied_templates,
        "upstream": upstream,
        "placeholders": placeholder_summary,
        "dry_run": args.dry_run,
        "layout": {name: str(path) for name, path in layout.items()},
    }
    write_text(state_summary_path, json.dumps(state_summary, indent=2, sort_keys=True) + "\n", args.dry_run)
    return state_summary


def print_bootstrap_summary(state_summary: dict[str, Any]) -> None:
    if state_summary["dry_run"]:
        print("dry-run reference harness layout:")
    else:
        print("reference harness initialized:")
    for name, path in state_summary["layout"].items():
        print(f"- {name}: {path}")
    print(f"- manifest: {state_summary['manifest']}")
    print(f"- state-summary: {state_summary['state_summary_path']}")
    copied_templates = state_summary["copied_templates"]
    print(f"- templates-copied: {', '.join(copied_templates) if copied_templates else '(none)'}")
    upstream = state_summary["upstream"]
    print(
        "- upstream-amflow: "
        f"{upstream['amflow'].get('status', 'unconfigured')} "
        f"({upstream['amflow'].get('resolved_commit') or upstream['amflow'].get('path') or 'not-set'})"
    )
    print(
        "- cpc-archive: "
        f"{upstream['cpc_archive'].get('status', 'unconfigured')} "
        f"({upstream['cpc_archive'].get('archive_path') or upstream['cpc_archive'].get('extracted_path') or 'not-set'})"
    )
    print(f"- phase0-placeholders: {state_summary['placeholders']['benchmark_count']}")


def run_self_check() -> dict[str, Any]:
    repo_root = Path(__file__).resolve().parents[3]
    parity_matrix_path = repo_root / "specs" / "parity-matrix.yaml"
    selected_benchmarks_path = repo_root / "references" / "case-studies" / "selected-benchmarks.md"
    verification_strategy_path = repo_root / "docs" / "verification-strategy.md"
    implementation_ledger_path = repo_root / "docs" / "implementation-ledger.md"

    with tempfile.TemporaryDirectory(prefix="amflow-bootstrap-self-check-") as tmp:
        temp_root = Path(tmp)
        harness_root = temp_root / "harness"
        args = argparse.Namespace(
            root=harness_root,
            manifest_name="phase0-reference",
            platform="ubuntu-22.04-x86_64",
            threads=1,
            amflow_url=None,
            amflow_ref=None,
            amflow_path=None,
            cpc_url=None,
            cpc_archive_path=None,
            kira_version="3.1",
            kira_executable=None,
            fermat_version="replace-me",
            fermat_executable=None,
            wolfram_kernel=None,
            mathematica_version="replace-me",
            freeze_placeholders=True,
            dry_run=False,
        )
        summary = bootstrap_reference_harness(args)

        manifest_path = Path(summary["manifest"])
        state_summary_path = Path(summary["state_summary_path"])
        manifest = load_json(manifest_path)
        phase0_catalog_path = harness_root / "templates" / "phase0-benchmarks.json"
        qualification_path = harness_root / "templates" / "qualification-benchmarks.json"
        golden_index_path = harness_root / "goldens" / "phase0" / "index.json"

        phase0_catalog = load_json(phase0_catalog_path)
        qualification = load_json(qualification_path)
        golden_index = load_json(golden_index_path)
        implementation_ledger_text = implementation_ledger_path.read_text(encoding="utf-8")
        parity_example_classes = load_yaml_string_list(parity_matrix_path, "example_classes")
        parity_benchmarks = load_yaml_string_list(parity_matrix_path, "benchmarks")
        parity_regressions = load_yaml_string_list(parity_matrix_path, "known_regressions")
        parity_failure_codes = load_yaml_string_list(parity_matrix_path, "required_failure_codes")
        selected_benchmark_ids, qualification_scaffold_ids, qualification_anchor_refs = (
            load_selected_benchmark_metadata(
            selected_benchmarks_path
        ))
        verification_thresholds = load_verification_strategy_digit_thresholds(verification_strategy_path)

        catalog_entries = phase0_catalog["phase0_benchmarks"]
        catalog_ids = [entry["id"] for entry in catalog_entries]
        required_catalog_ids = {entry["id"] for entry in catalog_entries if entry["required"]}
        phase0_example_classes = qualification["phase0_example_classes"]
        qualification_phase0_ids = [entry["phase0_catalog_id"] for entry in phase0_example_classes]
        case_study_families = qualification["case_study_families"]
        case_study_ids = [entry["id"] for entry in case_study_families]
        case_study_labels = [entry["parity_matrix_label"] for entry in case_study_families]
        ready_optional_captured_examples = {
            entry["id"]
            for entry in phase0_example_classes
            if entry["current_evidence_state"] == "reference-captured" and not entry["required_capture"]
        }
        ready_optional_examples_without_runtime_lane = {
            entry["id"]
            for entry in phase0_example_classes
            if entry["id"] in READY_OPTIONAL_CAPTURED_PHASE0_EXAMPLES and not entry.get("next_runtime_lane")
        }
        ready_optional_pending_examples = {
            entry["id"]
            for entry in phase0_example_classes
            if entry["current_evidence_state"] == "cataloged-pending-capture"
            and entry["id"] in READY_OPTIONAL_PENDING_PHASE0_EXAMPLES
            and not entry.get("next_runtime_lane")
        }
        catalog_runtime_lanes = {
            entry["id"]: entry.get("next_runtime_lane", "")
            for entry in catalog_entries
            if entry.get("next_runtime_lane")
        }
        catalog_capture_packets = {
            entry["id"]: entry.get("optional_capture_packet", "")
            for entry in catalog_entries
            if entry.get("optional_capture_packet")
        }
        qualification_phase0_runtime_lanes = {
            entry["id"]: entry.get("next_runtime_lane", "")
            for entry in phase0_example_classes
            if entry.get("next_runtime_lane")
        }
        qualification_phase0_capture_packets = {
            entry["id"]: entry.get("optional_capture_packet", "")
            for entry in phase0_example_classes
            if entry.get("optional_capture_packet")
        }
        qualification_case_study_runtime_lanes = {
            entry["id"]: entry.get("next_runtime_lane", "")
            for entry in case_study_families
            if entry.get("next_runtime_lane")
        }
        placeholder_runtime_lanes = {
            benchmark_id: load_json(
                harness_root / "goldens" / "phase0" / benchmark_id / "metadata.json"
            ).get("next_runtime_lane", "")
            for benchmark_id in THEORY_BLOCKED_PHASE0_RUNTIME_LANES
        }
        placeholder_capture_packets = {
            benchmark_id: load_json(
                harness_root / "goldens" / "phase0" / benchmark_id / "metadata.json"
            ).get("optional_capture_packet", "")
            for benchmark_id in OPTIONAL_CAPTURE_PACKET_HINTS
        }
        digit_threshold_profiles = {
            entry["id"]: entry["minimum_correct_digits"]
            for entry in qualification["digit_threshold_profiles"]
        }
        failure_code_profiles = {
            entry["id"]: entry["codes"] for entry in qualification["required_failure_code_profiles"]
        }
        regression_profiles = {
            entry["id"]: entry["families"] for entry in qualification["known_regression_profiles"]
        }
        golden_index_ids = [entry["benchmark_id"] for entry in golden_index["benchmarks"]]
        recorded_batch_ids = load_recorded_batch_ids(implementation_ledger_path)
        recorded_phase0_runtime_predecessors = set(LANDED_PHASE0_RUNTIME_PREDECESSORS.values()).issubset(
            recorded_batch_ids
        )
        recorded_case_study_runtime_predecessors = set(
            LANDED_CASE_STUDY_RUNTIME_PREDECESSORS.values()
        ).issubset(recorded_batch_ids)

        expect(summary["placeholders"]["benchmark_count"] == len(catalog_ids),
               "bootstrap self-check should freeze one placeholder benchmark per phase-0 catalog entry")
        expect("qualification-benchmarks.json" in summary["copied_templates"],
               "bootstrap self-check should copy the qualification scaffold template")
        expect(set(catalog_ids) == set(parity_example_classes),
               "phase-0 benchmark catalog should stay locked to the parity-matrix example class set")
        expect(set(qualification_phase0_ids) == set(catalog_ids),
               "qualification scaffold phase-0 examples should stay locked to the copied phase-0 catalog set")
        expect(len(set(qualification_phase0_ids)) == len(qualification_phase0_ids),
               "qualification scaffold phase-0 catalog ids should remain unique")
        expect(
            all(entry["required_capture"] == (entry["phase0_catalog_id"] in required_catalog_ids)
                for entry in phase0_example_classes),
            "qualification scaffold required_capture flags should stay synchronized with the phase-0 catalog",
        )
        expect(len(set(case_study_ids)) == len(case_study_ids),
               "qualification scaffold case-study ids should remain unique")
        expect(set(case_study_ids) == qualification_scaffold_ids,
               "qualification scaffold case-study ids should stay locked to selected-benchmarks.md")
        expect(len(set(case_study_labels)) == len(case_study_labels),
               "qualification scaffold case-study labels should remain unique")
        expect(set(case_study_labels) == set(parity_benchmarks),
               "qualification scaffold case-study labels should stay locked to the parity-matrix benchmark set")
        expect(
            all(
                set(entry["selected_benchmark_refs"]) ==
                ({qualification_anchor_refs[entry["id"]]}
                 if qualification_anchor_refs[entry["id"]] is not None
                 else set())
                for entry in case_study_families
            ),
            "qualification scaffold selected benchmark refs should stay locked to selected-benchmarks anchors",
        )
        expect(
            catalog_runtime_lanes == THEORY_BLOCKED_PHASE0_RUNTIME_LANES,
            "phase-0 catalog theory-blocked runtime lanes should stay locked to the reviewed next-slice plan",
        )
        expect(
            qualification_phase0_runtime_lanes == THEORY_BLOCKED_PHASE0_RUNTIME_LANES,
            "qualification scaffold phase-0 runtime lanes should stay locked to the reviewed next-slice plan",
        )
        expect(
            qualification_case_study_runtime_lanes == THEORY_BLOCKED_CASE_STUDY_RUNTIME_LANES,
            "qualification scaffold case-study runtime lanes should stay locked to the reviewed next-slice plan",
        )
        expect(
            recorded_phase0_runtime_predecessors and recorded_case_study_runtime_predecessors,
            "bootstrap self-check should anchor forward blocker hints to recorded landed runtime predecessors",
        )
        expect(
            placeholder_runtime_lanes == THEORY_BLOCKED_PHASE0_RUNTIME_LANES,
            "bootstrap self-check should preserve theory-blocked runtime lanes in placeholder metadata",
        )
        expect(
            catalog_capture_packets == OPTIONAL_CAPTURE_PACKET_HINTS
            and qualification_phase0_capture_packets == OPTIONAL_CAPTURE_PACKET_HINTS,
            "phase-0 catalog and qualification scaffold should keep optional capture-packet hints "
            "locked to the reviewed ready-example packet plan",
        )
        expect(
            placeholder_capture_packets == OPTIONAL_CAPTURE_PACKET_HINTS,
            "bootstrap self-check should preserve optional capture-packet hints in placeholder metadata",
        )
        expect(
            ready_optional_captured_examples == READY_OPTIONAL_CAPTURED_PHASE0_EXAMPLES
            and ready_optional_examples_without_runtime_lane == READY_OPTIONAL_CAPTURED_PHASE0_EXAMPLES,
            "qualification scaffold should keep the ready optional retained captures visible "
            "without stale runtime-lane blockers",
        )
        expect(
            ready_optional_pending_examples == READY_OPTIONAL_PENDING_PHASE0_EXAMPLES,
            "qualification scaffold should not leave any ready optional examples pending capture "
            "once the retained user-hook packet has landed",
        )
        expect(set(verification_thresholds).issubset(digit_threshold_profiles),
               "qualification scaffold should keep the verification-strategy digit-threshold profiles")
        expect(
            all(digit_threshold_profiles[profile_id] == threshold
                for profile_id, threshold in verification_thresholds.items()),
            "qualification scaffold digit-threshold values should stay synchronized with the verification strategy",
        )
        stronger_threshold_profiles = set(verification_thresholds) - {"core-package-family-default"}
        expect(
            all(
                entry["digit_threshold_profile"] == (
                    qualification_anchor_refs[entry["id"]]
                    if qualification_anchor_refs[entry["id"]] in stronger_threshold_profiles
                    else "core-package-family-default"
                )
                for entry in case_study_families
            ),
            "qualification scaffold case-study threshold assignments should stay synchronized with the selected benchmark anchors",
        )
        expect(all(entry["digit_threshold_profile"] in digit_threshold_profiles for entry in phase0_example_classes),
               "qualification scaffold phase-0 entries should only reference defined digit-threshold profiles")
        expect(all(entry["digit_threshold_profile"] in digit_threshold_profiles for entry in case_study_families),
               "qualification scaffold case-study entries should only reference defined digit-threshold profiles")
        expect(
            all(entry["failure_code_profile"] in failure_code_profiles for entry in phase0_example_classes)
            and all(entry["failure_code_profile"] in failure_code_profiles for entry in case_study_families),
            "qualification scaffold entries should only reference defined failure-code profiles",
        )
        expect(
            all(entry["regression_profile"] in regression_profiles for entry in phase0_example_classes)
            and all(entry["regression_profile"] in regression_profiles for entry in case_study_families),
            "qualification scaffold entries should only reference defined regression profiles",
        )
        expect(failure_code_profiles["default-required-failure-codes"] == parity_failure_codes,
               "qualification scaffold failure-code defaults should stay locked to the parity matrix")
        expect(regression_profiles["current-reviewed-regressions"] == parity_regressions,
               "qualification scaffold regression defaults should stay locked to the parity matrix")
        expect(golden_index["benchmark_catalog"] == str(phase0_catalog_path),
               "bootstrap self-check should wire the copied phase-0 catalog into the golden index")
        expect(len(set(golden_index_ids)) == len(golden_index_ids),
               "bootstrap self-check should keep golden-index benchmark ids unique")
        expect(set(golden_index_ids) == set(catalog_ids),
               "bootstrap self-check should keep the golden-index benchmark ids synchronized with the phase-0 catalog")
        expect(manifest["phase0"]["capture_state"] == "bootstrap-only" and
               manifest["phase0"]["placeholder_goldens_frozen"] is True,
               "bootstrap self-check should preserve the bootstrap-only placeholder state")
        expect(manifest["phase0"]["benchmark_catalog"] == str(phase0_catalog_path),
               "bootstrap self-check should record the copied phase-0 benchmark catalog in the manifest")
        expect(manifest["phase0"]["golden_index"] == str(golden_index_path),
               "bootstrap self-check should record the frozen golden index in the manifest")
        expect(state_summary_path.exists(), "bootstrap self-check should write the bootstrap state summary")

        return {
            "copied_qualification_template": "qualification-benchmarks.json" in summary["copied_templates"],
            "phase0_catalog_matches_parity_example_classes": set(catalog_ids) == set(parity_example_classes),
            "phase0_catalog_matches_qualification_scaffold": set(qualification_phase0_ids) == set(catalog_ids),
            "case_studies_match_parity_and_selected_benchmarks": (
                set(case_study_labels) == set(parity_benchmarks)
                and all(
                    set(entry["selected_benchmark_refs"]) ==
                    ({qualification_anchor_refs[entry["id"]]}
                     if qualification_anchor_refs[entry["id"]] is not None
                    else set())
                    for entry in case_study_families
                )
            ),
            "theory_blocked_phase0_runtime_lanes_locked": (
                catalog_runtime_lanes == THEORY_BLOCKED_PHASE0_RUNTIME_LANES
                and qualification_phase0_runtime_lanes == THEORY_BLOCKED_PHASE0_RUNTIME_LANES
            ),
            "theory_blocked_phase0_runtime_lanes": THEORY_BLOCKED_PHASE0_RUNTIME_LANES,
            "singular_case_study_runtime_lane_locked": (
                qualification_case_study_runtime_lanes
                == THEORY_BLOCKED_CASE_STUDY_RUNTIME_LANES
            ),
            "singular_case_study_runtime_lanes": THEORY_BLOCKED_CASE_STUDY_RUNTIME_LANES,
            "runtime_lane_predecessors_recorded": (
                recorded_phase0_runtime_predecessors
                and recorded_case_study_runtime_predecessors
            ),
            "recorded_phase0_runtime_predecessors": LANDED_PHASE0_RUNTIME_PREDECESSORS,
            "recorded_case_study_runtime_predecessors": LANDED_CASE_STUDY_RUNTIME_PREDECESSORS,
            "placeholder_metadata_preserves_runtime_lane_hints": (
                placeholder_runtime_lanes == THEORY_BLOCKED_PHASE0_RUNTIME_LANES
            ),
            "optional_capture_packets_locked": (
                catalog_capture_packets == OPTIONAL_CAPTURE_PACKET_HINTS
                and qualification_phase0_capture_packets == OPTIONAL_CAPTURE_PACKET_HINTS
            ),
            "placeholder_metadata_preserves_capture_packet_hints": (
                placeholder_capture_packets == OPTIONAL_CAPTURE_PACKET_HINTS
            ),
            "ready_optional_examples_reference_captured": (
                ready_optional_captured_examples == READY_OPTIONAL_CAPTURED_PHASE0_EXAMPLES
                and ready_optional_examples_without_runtime_lane
                == READY_OPTIONAL_CAPTURED_PHASE0_EXAMPLES
            ),
            "no_ready_optional_examples_pending_capture": (
                ready_optional_pending_examples == READY_OPTIONAL_PENDING_PHASE0_EXAMPLES
            ),
            "digit_threshold_profiles_match_verification_strategy": all(
                digit_threshold_profiles[profile_id] == threshold
                for profile_id, threshold in verification_thresholds.items()
            ) and all(
                entry["digit_threshold_profile"] == (
                    qualification_anchor_refs[entry["id"]]
                    if qualification_anchor_refs[entry["id"]]
                    in set(verification_thresholds) - {"core-package-family-default"}
                    else "core-package-family-default"
                )
                for entry in case_study_families
            ),
            "failure_code_profile_matches_parity_matrix": (
                failure_code_profiles["default-required-failure-codes"] == parity_failure_codes
            ),
            "regression_profile_matches_parity_matrix": (
                regression_profiles["current-reviewed-regressions"] == parity_regressions
            ),
            "placeholder_count_matches_catalog": summary["placeholders"]["benchmark_count"] == len(catalog_ids),
            "golden_index_matches_catalog_ids": set(golden_index_ids) == set(catalog_ids),
            "manifest_records_bootstrap_only_state": (
                manifest["phase0"]["capture_state"] == "bootstrap-only"
                and manifest["phase0"]["placeholder_goldens_frozen"] is True
            ),
            "state_summary_written": state_summary_path.exists(),
        }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True, help="Reference harness root")
    parser.add_argument(
        "--manifest-name",
        default="reference-bootstrap",
        help="Manifest file stem",
    )
    parser.add_argument(
        "--platform",
        default="ubuntu-22.04-x86_64",
        help="Pinned reference environment label",
    )
    parser.add_argument("--threads", type=int, default=8, help="Pinned thread count for the harness")
    parser.add_argument("--amflow-url", help="Public AMFlow Git URL to clone into the harness")
    parser.add_argument("--amflow-ref", help="Optional git ref to check out")
    parser.add_argument("--amflow-path", type=Path, help="Existing local AMFlow checkout to record")
    parser.add_argument("--cpc-url", help="CPC archive URL to download and extract")
    parser.add_argument(
        "--cpc-archive-path",
        type=Path,
        help="Existing local CPC archive or extracted directory to record",
    )
    parser.add_argument("--kira-version", default="3.1", help="Pinned Kira version")
    parser.add_argument("--kira-executable", type=Path, help="Kira executable path to record")
    parser.add_argument("--fermat-version", default="replace-me", help="Pinned Fermat version")
    parser.add_argument("--fermat-executable", type=Path, help="Fermat executable path to record")
    parser.add_argument("--wolfram-kernel", type=Path, help="Wolfram kernel path to record")
    parser.add_argument(
        "--mathematica-version",
        default="replace-me",
        help="Pinned Mathematica version",
    )
    parser.add_argument(
        "--freeze-placeholders",
        action="store_true",
        help="Create phase-0 placeholder golden metadata and benchmark directories",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Print the intended layout without writing files",
    )
    parser.add_argument(
        "--self-check",
        action="store_true",
        help=(
            "Run a local regression check for bootstrap layout creation, placeholder freeze wiring, "
            "qualification scaffold/catalog coherence, and theory-blocked runtime-lane hints"
        ),
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()

    if args.self_check:
        print(json.dumps(run_self_check(), indent=2, sort_keys=True))
        return 0

    state_summary = bootstrap_reference_harness(args)

    print_bootstrap_summary(state_summary)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
