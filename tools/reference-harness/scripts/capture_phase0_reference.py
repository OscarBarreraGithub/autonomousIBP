#!/usr/bin/env python3
"""Capture retained phase-0 upstream AMFlow goldens and reproducibility summaries."""

from __future__ import annotations

import argparse
import json
import re
import shutil
import subprocess
import tempfile
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from freeze_phase0_goldens import (
    load_json,
    normalize_benchmark_entry,
    normalize_capture_packet,
)


RUN_OUTPUT_PATTERN = re.compile(r'FileNameJoin\[\{current,\s*"([^"]+)"\}\]')
NTHREAD_PATTERN = re.compile(r'AMFlowInfo\["NThread"\]\s*=\s*\d+\s*;')
CACHE_SKIP_SUFFIXES = {".db"}
DEFAULT_MATHKERNEL = Path(
    "/n/sw/helmod/apps/centos7/Core/mathematica/Mathematica_13.3.0/Executables/MathKernel"
)


def utc_now() -> str:
    return datetime.now(timezone.utc).isoformat().replace("+00:00", "Z")


def ensure_dir(path: Path) -> None:
    path.mkdir(parents=True, exist_ok=True)


def write_json(path: Path, payload: dict[str, Any]) -> None:
    path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def write_text(path: Path, payload: str) -> None:
    path.write_text(payload, encoding="utf-8")


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def locate_manifest(root: Path, manifest_path: Path | None) -> Path:
    if manifest_path is not None:
        return manifest_path
    manifests_dir = root / "manifests"
    explicit = manifests_dir / "phase0-reference.json"
    if explicit.exists():
        return explicit
    manifests = sorted(manifests_dir.glob("*.json"))
    if len(manifests) == 1:
        return manifests[0]
    raise FileNotFoundError("could not infer harness manifest path")


def load_benchmark_catalog(path: Path) -> list[dict[str, Any]]:
    catalog = load_json(path)
    raw_benchmarks = catalog.get("phase0_benchmarks", [])
    return [normalize_benchmark_entry(entry) for entry in raw_benchmarks]


def select_benchmarks(
    benchmarks: list[dict[str, Any]],
    *,
    benchmark_ids: list[str] | None,
    required_only: bool,
    optional_capture_packet: str | None = None,
) -> list[dict[str, Any]]:
    selection_modes = (
        int(bool(benchmark_ids))
        + int(required_only)
        + int(optional_capture_packet is not None)
    )
    if selection_modes > 1:
        raise ValueError(
            "choose at most one of --benchmark-id, --optional-capture-packet, or --required-only"
        )
    if optional_capture_packet is not None:
        wanted_packet = normalize_capture_packet(optional_capture_packet)
        selected = [
            benchmark
            for benchmark in benchmarks
            if benchmark.get("optional_capture_packet", "") == wanted_packet
        ]
        if not selected:
            raise ValueError(f"unknown optional capture packet: {wanted_packet}")
        required_matches = [benchmark["id"] for benchmark in selected if benchmark["required"]]
        if required_matches:
            raise ValueError(
                "optional capture packet "
                f"{wanted_packet} includes required benchmark ids: {', '.join(required_matches)}"
            )
        return selected
    if benchmark_ids:
        wanted = set(benchmark_ids)
        selected = [benchmark for benchmark in benchmarks if benchmark["id"] in wanted]
        missing = sorted(wanted - {benchmark["id"] for benchmark in selected})
        if missing:
            raise ValueError(f"unknown benchmark ids: {', '.join(missing)}")
        return selected
    if required_only:
        selected = [benchmark for benchmark in benchmarks if benchmark["required"]]
        if not selected:
            raise ValueError("benchmark catalog contains no required phase-0 benchmarks")
        return selected
    return benchmarks


def stage_amflow_tree(source_root: Path, destination_root: Path) -> Path:
    if destination_root.exists():
        shutil.rmtree(destination_root)
    shutil.copytree(
        source_root,
        destination_root,
        ignore=shutil.ignore_patterns(".git", "__pycache__"),
    )
    return destination_root


def configure_kira_install(stage_amflow_root: Path, kira_executable: Path, fermat_executable: Path) -> None:
    install_path = stage_amflow_root / "ibp_interface" / "Kira" / "install.m"
    ensure_dir(install_path.parent)
    install_payload = (
        "(* ::Package:: *)\n\n"
        "(*kira executable, version 2.2 or later*)\n"
        f'$KiraExecutable = "{kira_executable}";\n'
        "(*fermat executable*)\n"
        f'$FermatExecutable = "{fermat_executable}";\n'
    )
    write_text(install_path, install_payload)


def patch_run_threads(run_path: Path, threads: int) -> None:
    text = run_path.read_text(encoding="utf-8")
    patched = NTHREAD_PATTERN.sub(f'AMFlowInfo["NThread"] = {threads};', text)
    if patched != text:
        write_text(run_path, patched)


def discover_output_names(run_path: Path) -> list[str]:
    names: list[str] = []
    seen: set[str] = set()
    text = run_path.read_text(encoding="utf-8")
    for match in RUN_OUTPUT_PATTERN.finditer(text):
        candidate = match.group(1)
        if candidate in seen:
            continue
        seen.add(candidate)
        names.append(candidate)
    if not names:
        raise RuntimeError(f"no Put[..., FileNameJoin[{{current, ...}}]] outputs found in {run_path}")
    return names


def run_mathkernel(mathkernel: Path, run_path: Path, cwd: Path, stdout_path: Path, stderr_path: Path) -> float:
    ensure_dir(stdout_path.parent)
    ensure_dir(stderr_path.parent)
    start = time.monotonic()
    with stdout_path.open("w", encoding="utf-8") as stdout_handle, stderr_path.open(
        "w", encoding="utf-8"
    ) as stderr_handle:
        completed = subprocess.run(
            [str(mathkernel), "-script", run_path.name],
            cwd=str(cwd),
            stdout=stdout_handle,
            stderr=stderr_handle,
            check=False,
            text=True,
        )
    duration_seconds = time.monotonic() - start
    if completed.returncode != 0:
        raise RuntimeError(
            f"MathKernel run failed for {run_path} with exit code {completed.returncode}; "
            f"see {stdout_path} and {stderr_path}"
        )
    return duration_seconds


def copy_output_files(example_root: Path, output_names: list[str], destination_root: Path) -> dict[str, Path]:
    ensure_dir(destination_root)
    copied: dict[str, Path] = {}
    for output_name in output_names:
        source_path = example_root / output_name
        if not source_path.exists():
            raise FileNotFoundError(f"expected benchmark output missing: {source_path}")
        destination_path = destination_root / output_name
        shutil.copy2(source_path, destination_path)
        copied[output_name] = destination_path
    return copied


def copy_filtered_tree(source_root: Path, destination_root: Path) -> list[str]:
    if not source_root.exists():
        return []
    copied: list[str] = []
    for source_path in sorted(path for path in source_root.rglob("*") if path.is_file()):
        if source_path.suffix in CACHE_SKIP_SUFFIXES:
            continue
        relative_path = source_path.relative_to(source_root)
        destination_path = destination_root / relative_path
        ensure_dir(destination_path.parent)
        shutil.copy2(source_path, destination_path)
        copied.append(str(destination_path))
    return copied


def canonicalize_mathematica_files(mathkernel: Path, files: dict[str, Path]) -> dict[str, dict[str, str]]:
    if not files:
        return {}

    with tempfile.TemporaryDirectory(prefix="amflow-canonicalize-") as tmp:
        temp_root = Path(tmp)
        spec_path = temp_root / "spec.json"
        output_path = temp_root / "output.json"
        script_path = temp_root / "canonicalize.wls"

        write_json(
            spec_path,
            {
                "files": [
                    {"name": name, "path": str(path)}
                    for name, path in sorted(files.items(), key=lambda item: item[0])
                ]
            },
        )

        script_payload = f"""
spec = Import["{spec_path}", "RawJSON"];
canonicalize[expr_] := Module[{{value = expr}},
  If[
    ListQ[value] && AllTrue[value, MatchQ[#, _Rule | _RuleDelayed] &],
    value = SortBy[
      value,
      ToString[InputForm[First[#]], CharacterEncoding -> "ASCII", PageWidth -> Infinity] &
    ]
  ];
  ToString[InputForm[value], CharacterEncoding -> "ASCII", PageWidth -> Infinity]
];
results = Table[
  canonical = canonicalize[Get[item["path"]]];
  <|
    "name" -> item["name"],
    "canonical_sha256" -> Hash[canonical, "SHA256", "HexString"],
    "canonical_text" -> canonical
  |>,
  {{item, spec["files"]}}
];
Export["{output_path}", <|"files" -> results|>, "RawJSON"];
Quit[];
"""
        write_text(script_path, script_payload)

        subprocess.run([str(mathkernel), "-script", str(script_path)], check=True, text=True)
        output = load_json(output_path)

    canonicalized: dict[str, dict[str, str]] = {}
    for entry in output.get("files", []):
        canonicalized[str(entry["name"])] = {
            "canonical_sha256": str(entry["canonical_sha256"]),
            "canonical_text": str(entry["canonical_text"]),
        }
    return canonicalized


def write_canonical_sidecars(canonicalized: dict[str, dict[str, str]], destination_root: Path) -> dict[str, str]:
    ensure_dir(destination_root)
    sidecars: dict[str, str] = {}
    for output_name, details in sorted(canonicalized.items()):
        sidecar_path = destination_root / f"{output_name}.canonical.txt"
        write_text(sidecar_path, details["canonical_text"] + "\n")
        sidecars[output_name] = str(sidecar_path)
    return sidecars


def compare_named_canonical_sets(
    expected: dict[str, dict[str, str]],
    actual: dict[str, dict[str, str]],
) -> dict[str, dict[str, Any]]:
    comparisons: dict[str, dict[str, Any]] = {}
    all_names = sorted(set(expected) | set(actual))
    for name in all_names:
        left = expected.get(name)
        right = actual.get(name)
        comparisons[name] = {
            "present_in_expected": left is not None,
            "present_in_actual": right is not None,
            "match": left is not None
            and right is not None
            and left["canonical_sha256"] == right["canonical_sha256"],
        }
        if left is not None:
            comparisons[name]["expected_sha256"] = left["canonical_sha256"]
        if right is not None:
            comparisons[name]["actual_sha256"] = right["canonical_sha256"]
    return comparisons


def all_comparisons_match(comparisons: dict[str, dict[str, Any]]) -> bool:
    return bool(comparisons) and all(bool(entry["match"]) for entry in comparisons.values())


def update_superseded_placeholder(path: Path, *, replacement_path: str, kind: str) -> None:
    payload = {
        "schema_version": 1,
        "status": "superseded-by-reference-capture",
        "kind": kind,
        "replacement_path": replacement_path,
        "updated_at_utc": utc_now(),
    }
    write_json(path, payload)


def resolve_example_root(manifest: dict[str, Any], benchmark_id: str) -> Path:
    upstream_root = Path(manifest["upstream"]["amflow"]["source"]["path"])
    candidate = upstream_root / "examples" / benchmark_id
    if candidate.exists():
        return candidate

    extracted_root = Path(manifest["upstream"]["cpc_archive"]["source"]["extracted_path"])
    secondary = next(iter(sorted(extracted_root.glob(f"*/examples/{benchmark_id}"))), None)
    if secondary is not None:
        return secondary
    raise FileNotFoundError(f"could not locate example root for benchmark {benchmark_id!r}")


def build_run_manifest(
    *,
    benchmark_id: str,
    label: str,
    duration_seconds: float,
    output_dir: Path,
    stdout_log: Path,
    stderr_log: Path,
    copied_cache_root: Path,
    output_names: list[str],
    raw_outputs: dict[str, Path],
    canonicalized_outputs: dict[str, dict[str, str]],
    canonical_sidecars: dict[str, str],
) -> dict[str, Any]:
    return {
        "schema_version": 1,
        "benchmark_id": benchmark_id,
        "label": label,
        "duration_seconds": round(duration_seconds, 6),
        "created_at_utc": utc_now(),
        "output_dir": str(output_dir),
        "stdout_log": str(stdout_log),
        "stderr_log": str(stderr_log),
        "generated_cache_root": str(copied_cache_root),
        "outputs": [
            {
                "name": output_name,
                "path": str(raw_outputs[output_name]),
                "canonical_sha256": canonicalized_outputs[output_name]["canonical_sha256"],
                "canonical_text": canonical_sidecars[output_name],
            }
            for output_name in output_names
        ],
    }


def load_existing_run_capture(
    *,
    root: Path,
    manifest: dict[str, Any],
    benchmark: dict[str, Any],
    label: str,
) -> dict[str, Any] | None:
    benchmark_id = benchmark["id"]
    output_dir = root / "results" / "phase0" / benchmark_id / label
    run_manifest_path = output_dir / "run-manifest.json"
    if not run_manifest_path.exists():
        return None

    run_manifest = load_json(run_manifest_path)
    if run_manifest.get("benchmark_id") != benchmark_id or run_manifest.get("label") != label:
        raise RuntimeError(f"existing run manifest does not match {benchmark_id}/{label}: {run_manifest_path}")

    output_names: list[str] = []
    raw_outputs: dict[str, str] = {}
    canonicalized_outputs: dict[str, dict[str, str]] = {}
    for output_entry in run_manifest.get("outputs", []):
        output_name = str(output_entry["name"])
        raw_output_path = Path(output_entry["path"])
        canonical_sidecar_path = Path(output_entry["canonical_text"])
        if not raw_output_path.exists() or not canonical_sidecar_path.exists():
            return None
        output_names.append(output_name)
        raw_outputs[output_name] = str(raw_output_path)
        canonical_text = read_text(canonical_sidecar_path).rstrip("\n")
        canonicalized_outputs[output_name] = {
            "canonical_sha256": str(output_entry["canonical_sha256"]),
            "canonical_text": canonical_text,
        }

    stdout_log = Path(run_manifest["stdout_log"])
    stderr_log = Path(run_manifest["stderr_log"])
    if not output_names or not stdout_log.exists() or not stderr_log.exists():
        return None

    return {
        "benchmark_id": benchmark_id,
        "label": label,
        "source_example_root": str(resolve_example_root(manifest, benchmark_id)),
        "output_names": output_names,
        "raw_outputs": raw_outputs,
        "canonicalized_outputs": canonicalized_outputs,
        "stdout_log": str(stdout_log),
        "stderr_log": str(stderr_log),
        "generated_cache_root": str(run_manifest["generated_cache_root"]),
        "duration_seconds": float(run_manifest["duration_seconds"]),
        "run_manifest": str(run_manifest_path),
        "resumed_existing": True,
    }


def run_benchmark_capture(
    *,
    root: Path,
    manifest: dict[str, Any],
    benchmark: dict[str, Any],
    label: str,
    keep_stage_workdirs: bool,
    resume_existing: bool,
) -> dict[str, Any]:
    benchmark_id = benchmark["id"]
    if resume_existing:
        existing_run = load_existing_run_capture(
            root=root,
            manifest=manifest,
            benchmark=benchmark,
            label=label,
        )
        if existing_run is not None:
            return existing_run

    source_amflow_root = Path(manifest["upstream"]["amflow"]["source"]["path"])
    source_example_root = resolve_example_root(manifest, benchmark_id)
    mathkernel = Path(manifest["upstream"]["wolfram"]["kernel"])
    kira_executable = Path(manifest["upstream"]["kira"]["executable"])
    fermat_executable = Path(manifest["upstream"]["fermat"]["executable"])
    threads = int(manifest["environment"].get("threads", 1))

    stage_root = root / "state" / "phase0-runs" / benchmark_id / label
    stage_amflow_root = stage_root / "amflow"
    stage_amflow_root = stage_amflow_tree(source_amflow_root, stage_amflow_root)
    configure_kira_install(stage_amflow_root, kira_executable, fermat_executable)

    stage_example_root = stage_amflow_root / "examples" / benchmark_id
    run_path = stage_example_root / "run.wl"
    patch_run_threads(run_path, threads)
    output_names = discover_output_names(run_path)

    log_root = root / "logs" / "phase0" / benchmark_id / label
    stdout_log = log_root / "mathkernel.stdout.log"
    stderr_log = log_root / "mathkernel.stderr.log"
    duration_seconds = run_mathkernel(mathkernel, run_path, stage_example_root, stdout_log, stderr_log)

    output_dir = root / "results" / "phase0" / benchmark_id / label
    raw_outputs = copy_output_files(stage_example_root, output_names, output_dir)
    canonicalized_outputs = canonicalize_mathematica_files(mathkernel, raw_outputs)
    canonical_sidecars = write_canonical_sidecars(
        canonicalized_outputs,
        output_dir / "canonical",
    )

    generated_cache_root = root / "generated-config" / "phase0" / benchmark_id / label / "cache"
    copied_cache_root = generated_cache_root
    copy_filtered_tree(stage_example_root / "cache", generated_cache_root)

    run_manifest = build_run_manifest(
        benchmark_id=benchmark_id,
        label=label,
        duration_seconds=duration_seconds,
        output_dir=output_dir,
        stdout_log=stdout_log,
        stderr_log=stderr_log,
        copied_cache_root=copied_cache_root,
        output_names=output_names,
        raw_outputs=raw_outputs,
        canonicalized_outputs=canonicalized_outputs,
        canonical_sidecars=canonical_sidecars,
    )
    write_json(output_dir / "run-manifest.json", run_manifest)

    if not keep_stage_workdirs:
        shutil.rmtree(stage_root)

    return {
        "benchmark_id": benchmark_id,
        "label": label,
        "source_example_root": str(source_example_root),
        "output_names": output_names,
        "raw_outputs": {name: str(path) for name, path in raw_outputs.items()},
        "canonicalized_outputs": canonicalized_outputs,
        "stdout_log": str(stdout_log),
        "stderr_log": str(stderr_log),
        "generated_cache_root": str(copied_cache_root),
        "duration_seconds": duration_seconds,
        "run_manifest": str(output_dir / "run-manifest.json"),
        "resumed_existing": False,
    }


def promote_primary_golden(
    *,
    root: Path,
    benchmark_id: str,
    output_names: list[str],
    primary_outputs: dict[str, str],
    primary_canonical: dict[str, dict[str, str]],
) -> dict[str, Any]:
    golden_root = root / "goldens" / "phase0" / benchmark_id / "captured"
    ensure_dir(golden_root)
    golden_outputs: dict[str, str] = {}
    raw_source_paths = {name: Path(path) for name, path in primary_outputs.items()}
    for output_name, source_path in raw_source_paths.items():
        destination_path = golden_root / output_name
        shutil.copy2(source_path, destination_path)
        golden_outputs[output_name] = str(destination_path)

    canonical_sidecars = write_canonical_sidecars(
        primary_canonical,
        golden_root / "canonical",
    )
    golden_manifest = {
        "schema_version": 1,
        "benchmark_id": benchmark_id,
        "created_at_utc": utc_now(),
        "golden_output_dir": str(golden_root),
        "outputs": [
            {
                "name": output_name,
                "path": golden_outputs[output_name],
                "canonical_sha256": primary_canonical[output_name]["canonical_sha256"],
                "canonical_text": canonical_sidecars[output_name],
            }
            for output_name in output_names
        ],
    }
    golden_manifest_path = root / "goldens" / "phase0" / benchmark_id / "golden-manifest.json"
    write_json(golden_manifest_path, golden_manifest)
    return {
        "golden_output_dir": str(golden_root),
        "golden_manifest": str(golden_manifest_path),
        "golden_outputs": golden_outputs,
        "golden_canonical_sidecars": canonical_sidecars,
    }


def finalize_benchmark_capture(
    *,
    root: Path,
    manifest_path: Path,
    manifest: dict[str, Any],
    benchmark: dict[str, Any],
    primary_run: dict[str, Any],
    rerun: dict[str, Any],
) -> dict[str, Any]:
    benchmark_id = benchmark["id"]
    output_names = list(primary_run["output_names"])
    source_example_root = Path(primary_run["source_example_root"])
    backup_files: dict[str, Path] = {}
    for output_name in output_names:
        backup_path = source_example_root / "backup" / f"kira_{output_name}"
        if backup_path.exists():
            backup_files[output_name] = backup_path

    mathkernel = Path(manifest["upstream"]["wolfram"]["kernel"])
    primary_canonical = primary_run["canonicalized_outputs"]
    rerun_canonical = rerun["canonicalized_outputs"]
    backup_canonical = canonicalize_mathematica_files(mathkernel, backup_files)

    backup_comparisons = compare_named_canonical_sets(backup_canonical, primary_canonical)
    rerun_comparisons = compare_named_canonical_sets(primary_canonical, rerun_canonical)
    output_presence_ok = all(
        Path(primary_run["raw_outputs"][name]).exists() and Path(rerun["raw_outputs"][name]).exists()
        for name in output_names
    )
    backup_match_ok = all_comparisons_match(backup_comparisons)
    rerun_match_ok = all_comparisons_match(rerun_comparisons)

    promoted = promote_primary_golden(
        root=root,
        benchmark_id=benchmark_id,
        output_names=output_names,
        primary_outputs=primary_run["raw_outputs"],
        primary_canonical=primary_canonical,
    )

    result_manifest_path = root / "results" / "phase0" / benchmark_id / "result-manifest.json"
    result_manifest = {
        "schema_version": 1,
        "benchmark_id": benchmark_id,
        "created_at_utc": utc_now(),
        "manifest": str(manifest_path),
        "primary_run_manifest": primary_run["run_manifest"],
        "rerun_run_manifest": rerun["run_manifest"],
        "golden_manifest": promoted["golden_manifest"],
        "backup_root": str(source_example_root / "backup"),
    }
    write_json(result_manifest_path, result_manifest)

    comparison_summary_path = root / "comparisons" / "phase0" / f"{benchmark_id}.summary.json"
    comparison_summary = {
        "schema_version": 1,
        "benchmark_id": benchmark_id,
        "status": "reference-captured" if output_presence_ok and backup_match_ok and rerun_match_ok else "capture-failed",
        "reference_golden": promoted["golden_manifest"],
        "latest_run_result": str(result_manifest_path),
        "bundled_kira_backup": str(source_example_root / "backup"),
        "runs": [
            {
                "label": primary_run["label"],
                "run_manifest": primary_run["run_manifest"],
                "duration_seconds": round(float(primary_run["duration_seconds"]), 6),
                "stdout_log": primary_run["stdout_log"],
                "stderr_log": primary_run["stderr_log"],
            },
            {
                "label": rerun["label"],
                "run_manifest": rerun["run_manifest"],
                "duration_seconds": round(float(rerun["duration_seconds"]), 6),
                "stdout_log": rerun["stdout_log"],
                "stderr_log": rerun["stderr_log"],
            },
        ],
        "output_names": output_names,
        "backup_comparisons": backup_comparisons,
        "rerun_comparisons": rerun_comparisons,
        "checks": [
            {
                "name": "reference_run_present",
                "status": "passed" if Path(primary_run["run_manifest"]).exists() else "failed",
            },
            {
                "name": "required_outputs_present",
                "status": "passed" if output_presence_ok else "failed",
            },
            {
                "name": "bundled_kira_backup_match",
                "status": "passed" if backup_match_ok else "failed",
            },
            {
                "name": "rerun_reproducible",
                "status": "passed" if rerun_match_ok else "failed",
            },
        ],
    }
    write_json(comparison_summary_path, comparison_summary)

    comparison_pointer = root / "comparisons" / "phase0" / f"{benchmark_id}.pending.json"
    update_superseded_placeholder(
        comparison_pointer,
        replacement_path=str(comparison_summary_path),
        kind="comparison-summary",
    )

    coefficient_pointer = root / "goldens" / "phase0" / benchmark_id / "coefficients.placeholder.json"
    update_superseded_placeholder(
        coefficient_pointer,
        replacement_path=promoted["golden_manifest"],
        kind="golden-output-manifest",
    )

    metadata_path = root / "goldens" / "phase0" / benchmark_id / "metadata.json"
    metadata = load_json(metadata_path)
    metadata["status"] = comparison_summary["status"]
    metadata["expected_outputs"]["result_manifest"] = str(result_manifest_path)
    metadata["expected_outputs"]["replacement_rules"] = promoted["golden_output_dir"]
    metadata["expected_outputs"]["coefficient_table"] = promoted["golden_manifest"]
    metadata["expected_outputs"]["wolfram_log"] = primary_run["stdout_log"]
    metadata["expected_outputs"]["kira_log"] = primary_run["generated_cache_root"]
    metadata["expected_outputs"]["generated_config_dir"] = primary_run["generated_cache_root"]
    metadata["captured_outputs"] = {
        "golden_manifest": promoted["golden_manifest"],
        "golden_output_dir": promoted["golden_output_dir"],
        "primary_run_manifest": primary_run["run_manifest"],
        "rerun_run_manifest": rerun["run_manifest"],
        "comparison_summary": str(comparison_summary_path),
    }
    metadata["notes"] = [
        note
        for note in metadata.get("notes", [])
        if "Populate from the pinned upstream AMFlow reference harness" not in note
    ]
    metadata["notes"].append(
        "Real upstream-AMFlow outputs are now retained under captured/, with canonicalized hashes and rerun reproducibility summaries."
    )
    write_json(metadata_path, metadata)

    index_path = root / "goldens" / "phase0" / "index.json"
    index = load_json(index_path)
    for entry in index.get("benchmarks", []):
        if entry.get("benchmark_id") != benchmark_id:
            continue
        entry["status"] = comparison_summary["status"]
        entry["golden_manifest"] = promoted["golden_manifest"]
        entry["result_manifest"] = str(result_manifest_path)
        entry["comparison_summary"] = str(comparison_summary_path)
        entry["golden_output_dir"] = promoted["golden_output_dir"]
    write_json(index_path, index)

    return {
        "benchmark_id": benchmark_id,
        "status": comparison_summary["status"],
        "comparison_summary": str(comparison_summary_path),
        "golden_manifest": promoted["golden_manifest"],
        "result_manifest": str(result_manifest_path),
        "backup_match_ok": backup_match_ok,
        "rerun_match_ok": rerun_match_ok,
    }


def update_manifest_capture_state(
    *,
    manifest_path: Path,
    catalog_benchmarks: list[dict[str, Any]],
    benchmark_summaries: list[dict[str, Any]],
    summary_path: Path,
) -> None:
    manifest = load_json(manifest_path)
    required_ids = {benchmark["id"] for benchmark in catalog_benchmarks if benchmark["required"]}
    passed_ids = {
        summary["benchmark_id"]
        for summary in benchmark_summaries
        if summary["status"] == "reference-captured"
    }
    already_captured = set(manifest["phase0"].get("captured_benchmarks", []))
    required_set_complete = required_ids.issubset(already_captured | passed_ids)
    manifest["phase0"]["capture_state"] = "reference-captured" if required_set_complete else "bootstrap-only"
    manifest["phase0"]["capture_summary"] = str(summary_path)
    manifest["phase0"]["captured_benchmarks"] = sorted(already_captured | passed_ids)
    write_json(manifest_path, manifest)


def capture_reference_packet(
    *,
    root: Path,
    manifest_path: Path,
    benchmark_ids: list[str] | None,
    required_only: bool,
    optional_capture_packet: str | None,
    keep_stage_workdirs: bool,
    resume_existing: bool,
) -> dict[str, Any]:
    manifest = load_json(manifest_path)
    benchmark_catalog_path = Path(manifest["phase0"]["benchmark_catalog"])
    benchmarks = load_benchmark_catalog(benchmark_catalog_path)
    selected_benchmarks = select_benchmarks(
        benchmarks,
        benchmark_ids=benchmark_ids,
        required_only=required_only,
        optional_capture_packet=optional_capture_packet,
    )

    benchmark_summaries: list[dict[str, Any]] = []
    for benchmark in selected_benchmarks:
        primary = run_benchmark_capture(
            root=root,
            manifest=manifest,
            benchmark=benchmark,
            label="primary",
            keep_stage_workdirs=keep_stage_workdirs,
            resume_existing=resume_existing,
        )
        rerun = run_benchmark_capture(
            root=root,
            manifest=manifest,
            benchmark=benchmark,
            label="rerun",
            keep_stage_workdirs=keep_stage_workdirs,
            resume_existing=resume_existing,
        )
        benchmark_summaries.append(
            finalize_benchmark_capture(
                root=root,
                manifest_path=manifest_path,
                manifest=manifest,
                benchmark=benchmark,
                primary_run=primary,
                rerun=rerun,
            )
        )

    summary_path = root / "state" / "phase0-reference.capture.json"
    packet_summary = {
        "schema_version": 1,
        "created_at_utc": utc_now(),
        "manifest": str(manifest_path),
        "root": str(root),
        "required_only": required_only,
        "optional_capture_packet": optional_capture_packet or "",
        "resume_existing": resume_existing,
        "selected_benchmarks": [benchmark["id"] for benchmark in selected_benchmarks],
        "benchmark_summaries": benchmark_summaries,
    }
    write_json(summary_path, packet_summary)
    update_manifest_capture_state(
        manifest_path=manifest_path,
        catalog_benchmarks=benchmarks,
        benchmark_summaries=benchmark_summaries,
        summary_path=summary_path,
    )

    packet_summary["summary_path"] = str(summary_path)
    packet_summary["capture_state"] = load_json(manifest_path)["phase0"]["capture_state"]
    write_json(summary_path, packet_summary)
    return packet_summary


def run_self_check(mathkernel: Path) -> dict[str, Any]:
    with tempfile.TemporaryDirectory(prefix="amflow-capture-self-check-") as tmp:
        temp_root = Path(tmp)
        harness_root = temp_root / "harness"
        manifest_path = harness_root / "manifests" / "phase0-reference.json"
        benchmark_catalog_path = harness_root / "templates" / "phase0-benchmarks.json"
        source_amflow = temp_root / "source-amflow"
        example_root = source_amflow / "examples" / "demo_benchmark"
        extracted_example_root = temp_root / "cpc-extracted" / "amflow-cpc" / "examples" / "demo_benchmark"
        selection_benchmarks = [
            normalize_benchmark_entry(
                {
                    "id": "required_alpha",
                    "label": "required_alpha",
                    "required": True,
                    "feature_gate": "phase0",
                    "oracle": "upstream-amflow",
                }
            ),
            normalize_benchmark_entry(
                {
                    "id": "optional_beta",
                    "label": "optional_beta",
                    "required": False,
                    "feature_gate": "phase0",
                    "oracle": "upstream-amflow",
                    "optional_capture_packet": "ready-pair",
                }
            ),
            normalize_benchmark_entry(
                {
                    "id": "required_gamma",
                    "label": "required_gamma",
                    "required": True,
                    "feature_gate": "phase0",
                    "oracle": "upstream-amflow",
                }
            ),
            normalize_benchmark_entry(
                {
                    "id": "optional_delta",
                    "label": "optional_delta",
                    "required": False,
                    "feature_gate": "phase0",
                    "oracle": "upstream-amflow",
                    "optional_capture_packet": "ready-pair",
                }
            ),
        ]

        ensure_dir(example_root / "backup")
        ensure_dir(extracted_example_root)
        ensure_dir(source_amflow / "ibp_interface" / "Kira")
        ensure_dir(harness_root / "goldens" / "phase0" / "demo_benchmark")
        ensure_dir(harness_root / "comparisons" / "phase0")
        ensure_dir(harness_root / "results" / "phase0" / "demo_benchmark")
        ensure_dir(harness_root / "generated-config" / "phase0" / "demo_benchmark")
        ensure_dir(harness_root / "logs" / "phase0" / "demo_benchmark")
        ensure_dir(harness_root / "state")
        ensure_dir(harness_root / "templates")
        ensure_dir(manifest_path.parent)

        write_text(
            example_root / "run.wl",
            'current = If[$FrontEnd===Null,$InputFileName,NotebookFileName[]]//DirectoryName;\n'
            'Put[{b -> 1, a -> 2}, FileNameJoin[{current, "sol"}]];\n'
            'Quit[];\n',
        )
        write_text(example_root / "backup" / "kira_sol", "{a -> 2, b -> 1}\n")
        write_json(
            harness_root / "goldens" / "phase0" / "demo_benchmark" / "metadata.json",
            {
                "schema_version": 1,
                "benchmark_id": "demo_benchmark",
                "benchmark_label": "demo_benchmark",
                "required": True,
                "feature_gate": "phase0",
                "status": "placeholder",
                "oracle": {"kind": "upstream-amflow", "manifest": str(manifest_path), "benchmark_catalog": str(benchmark_catalog_path)},
                "expected_outputs": {
                    "result_manifest": str(harness_root / "results" / "phase0" / "demo_benchmark" / "result-manifest.json"),
                    "replacement_rules": "",
                    "coefficient_table": str(harness_root / "goldens" / "phase0" / "demo_benchmark" / "coefficients.placeholder.json"),
                    "wolfram_log": "",
                    "kira_log": "",
                    "generated_config_dir": "",
                },
                "notes": ["Populate from the pinned upstream AMFlow reference harness once a licensed Wolfram environment is available."],
            },
        )
        write_json(
            harness_root / "goldens" / "phase0" / "demo_benchmark" / "coefficients.placeholder.json",
            {"schema_version": 1, "benchmark_id": "demo_benchmark", "status": "placeholder", "coefficients": []},
        )
        write_json(
            harness_root / "goldens" / "phase0" / "index.json",
            {
                "schema_version": 1,
                "phase": "phase0",
                "manifest": str(manifest_path),
                "benchmark_catalog": str(benchmark_catalog_path),
                "refresh_mode": "placeholder-only",
                "benchmarks": [
                    {
                        "benchmark_id": "demo_benchmark",
                        "golden_metadata": str(harness_root / "goldens" / "phase0" / "demo_benchmark" / "metadata.json"),
                        "coefficient_placeholder": str(harness_root / "goldens" / "phase0" / "demo_benchmark" / "coefficients.placeholder.json"),
                        "comparison_summary": str(harness_root / "comparisons" / "phase0" / "demo_benchmark.pending.json"),
                    }
                ],
            },
        )
        write_json(
            harness_root / "comparisons" / "phase0" / "demo_benchmark.pending.json",
            {"schema_version": 1, "benchmark_id": "demo_benchmark", "status": "pending-reference-capture"},
        )
        write_json(
            benchmark_catalog_path,
            {
                "phase0_benchmarks": [
                    {
                        "id": "required_benchmark",
                        "label": "required_benchmark",
                        "required": True,
                        "feature_gate": "phase0",
                        "oracle": "upstream-amflow",
                    },
                    {
                        "id": "demo_benchmark",
                        "label": "demo_benchmark",
                        "required": False,
                        "feature_gate": "phase0",
                        "oracle": "upstream-amflow",
                        "optional_capture_packet": "demo-packet",
                    }
                ]
            },
        )
        write_json(
            manifest_path,
            {
                "schema_version": 1,
                "run_id": "phase0-reference",
                "created_at_utc": utc_now(),
                "upstream": {
                    "amflow": {"source": {"path": str(source_amflow)}},
                    "cpc_archive": {"source": {"extracted_path": str(temp_root / "unused-extracted")}},
                    "kira": {"executable": "/bin/true"},
                    "fermat": {"executable": "/bin/true"},
                    "wolfram": {"kernel": str(mathkernel)},
                },
                "environment": {"threads": 1},
                "phase0": {
                    "benchmark_catalog": str(benchmark_catalog_path),
                    "golden_index": str(harness_root / "goldens" / "phase0" / "index.json"),
                    "placeholder_goldens_frozen": True,
                    "capture_state": "bootstrap-only",
                    "capture_summary": "",
                    "captured_benchmarks": ["required_benchmark"],
                },
            },
        )

        parsed_args = parse_args(
            [
                "--root",
                str(harness_root),
                "--manifest-path",
                str(manifest_path),
                "--optional-capture-packet",
                "demo-packet",
            ]
        )
        summary = capture_reference_packet(
            root=parsed_args.root,
            manifest_path=parsed_args.manifest_path,
            benchmark_ids=parsed_args.benchmark_id,
            required_only=parsed_args.required_only,
            optional_capture_packet=parsed_args.optional_capture_packet,
            keep_stage_workdirs=parsed_args.keep_stage_workdirs,
            resume_existing=parsed_args.resume_existing,
        )
        benchmark = next(
            entry
            for entry in load_benchmark_catalog(benchmark_catalog_path)
            if entry["id"] == "demo_benchmark"
        )
        resumed_primary = run_benchmark_capture(
            root=harness_root,
            manifest=load_json(manifest_path),
            benchmark=benchmark,
            label="primary",
            keep_stage_workdirs=False,
            resume_existing=True,
        )
        resumed_rerun = run_benchmark_capture(
            root=harness_root,
            manifest=load_json(manifest_path),
            benchmark=benchmark,
            label="rerun",
            keep_stage_workdirs=False,
            resume_existing=True,
        )
        updated_manifest = load_json(manifest_path)
        comparison_summary = load_json(harness_root / "comparisons" / "phase0" / "demo_benchmark.summary.json")
        selected_ids = [
            entry["id"]
            for entry in select_benchmarks(
                selection_benchmarks,
                benchmark_ids=["optional_beta", "required_alpha", "optional_beta"],
                required_only=False,
            )
        ]
        fallback_example_root = resolve_example_root(
            {
                "upstream": {
                    "amflow": {"source": {"path": str(temp_root / "missing-amflow")}},
                    "cpc_archive": {"source": {"extracted_path": str(temp_root / "cpc-extracted")}},
                }
            },
            "demo_benchmark",
        )
        required_only_ids = [
            entry["id"]
            for entry in select_benchmarks(
                selection_benchmarks,
                benchmark_ids=None,
                required_only=True,
            )
        ]
        packet_selected_ids = [
            entry["id"]
            for entry in select_benchmarks(
                selection_benchmarks,
                benchmark_ids=None,
                required_only=False,
                optional_capture_packet="ready-pair",
            )
        ]
        selection_conflict_rejected = False
        try:
            select_benchmarks(
                selection_benchmarks,
                benchmark_ids=["optional_beta"],
                required_only=True,
            )
        except ValueError as exc:
            selection_conflict_rejected = (
                str(exc)
                == "choose at most one of --benchmark-id, --optional-capture-packet, or --required-only"
            )
        required_only_packet_conflict_rejected = False
        try:
            select_benchmarks(
                selection_benchmarks,
                benchmark_ids=None,
                required_only=True,
                optional_capture_packet="ready-pair",
            )
        except ValueError as exc:
            required_only_packet_conflict_rejected = (
                str(exc)
                == "choose at most one of --benchmark-id, --optional-capture-packet, or --required-only"
            )
        packet_selection_conflict_rejected = False
        try:
            select_benchmarks(
                selection_benchmarks,
                benchmark_ids=["optional_beta"],
                required_only=False,
                optional_capture_packet="ready-pair",
            )
        except ValueError as exc:
            packet_selection_conflict_rejected = (
                str(exc)
                == "choose at most one of --benchmark-id, --optional-capture-packet, or --required-only"
            )
        unknown_benchmark_rejected = False
        try:
            select_benchmarks(
                selection_benchmarks,
                benchmark_ids=["missing_benchmark"],
                required_only=False,
            )
        except ValueError as exc:
            unknown_benchmark_rejected = (
                str(exc) == "unknown benchmark ids: missing_benchmark"
            )
        required_packet_rejected = False
        try:
            select_benchmarks(
                selection_benchmarks
                + [
                    normalize_benchmark_entry(
                        {
                            "id": "required_packet_member",
                            "label": "required_packet_member",
                            "required": True,
                            "feature_gate": "phase0",
                            "oracle": "upstream-amflow",
                            "optional_capture_packet": "bad-packet",
                        }
                    )
                ],
                benchmark_ids=None,
                required_only=False,
                optional_capture_packet="bad-packet",
            )
        except ValueError as exc:
            required_packet_rejected = (
                str(exc)
                == "optional capture packet bad-packet includes required benchmark ids: "
                "required_packet_member"
            )
        unknown_capture_packet_rejected = False
        try:
            select_benchmarks(
                selection_benchmarks,
                benchmark_ids=None,
                required_only=False,
                optional_capture_packet="missing_packet",
            )
        except ValueError as exc:
            unknown_capture_packet_rejected = (
                str(exc) == "unknown optional capture packet: missing_packet"
            )
        return {
            "capture_state_reference_captured": updated_manifest["phase0"]["capture_state"] == "reference-captured",
            "summary_written": Path(summary["summary_path"]).exists(),
            "summary_records_selected_packet": summary["optional_capture_packet"] == "demo-packet",
            "backup_match_ok": comparison_summary["checks"][2]["status"] == "passed",
            "rerun_match_ok": comparison_summary["checks"][3]["status"] == "passed",
            "cpc_fallback_example_root_resolved": fallback_example_root == extracted_example_root,
            "selected_ids_follow_catalog_order": selected_ids == ["required_alpha", "optional_beta"],
            "required_only_selects_required_subset": required_only_ids == [
                "required_alpha",
                "required_gamma",
            ],
            "optional_capture_packet_selects_catalog_order": packet_selected_ids == [
                "optional_beta",
                "optional_delta",
            ],
            "benchmark_selection_conflict_rejected": selection_conflict_rejected,
            "required_only_packet_conflict_rejected": required_only_packet_conflict_rejected,
            "packet_selection_conflict_rejected": packet_selection_conflict_rejected,
            "unknown_benchmark_rejected": unknown_benchmark_rejected,
            "required_packet_rejected": required_packet_rejected,
            "unknown_capture_packet_rejected": unknown_capture_packet_rejected,
            "resume_reused_existing_runs": bool(
                resumed_primary.get("resumed_existing") and resumed_rerun.get("resumed_existing")
            ),
        }


def parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True, help="Reference harness root")
    parser.add_argument("--manifest-path", type=Path, help="Harness manifest JSON path")
    parser.add_argument(
        "--benchmark-id",
        action="append",
        help=(
            "Benchmark id to capture; may be passed multiple times, duplicates collapse, and "
            "execution follows the frozen catalog order"
        ),
    )
    parser.add_argument(
        "--required-only",
        action="store_true",
        help="Capture only required benchmarks from the phase-0 catalog",
    )
    parser.add_argument(
        "--optional-capture-packet",
        help=(
            "Capture every non-required benchmark whose phase-0 catalog entry carries this "
            "optional_capture_packet hint; execution follows the frozen catalog order"
        ),
    )
    parser.add_argument(
        "--keep-stage-workdirs",
        action="store_true",
        help="Preserve staged AMFlow work directories under state/phase0-runs/",
    )
    parser.add_argument(
        "--resume-existing",
        action="store_true",
        help="Reuse existing per-run manifests and retained outputs instead of replaying completed labels",
    )
    parser.add_argument(
        "--self-check",
        action="store_true",
        help="Run a local capture regression check against a synthetic benchmark",
    )
    parser.add_argument(
        "--mathkernel",
        type=Path,
        help="Override the Mathematica kernel path used by --self-check",
    )
    return parser.parse_args(argv)


def main() -> int:
    args = parse_args()

    if args.self_check:
        mathkernel = args.mathkernel or DEFAULT_MATHKERNEL
        summary = run_self_check(mathkernel)
        print(json.dumps(summary, indent=2, sort_keys=True))
        return 0

    manifest_path = locate_manifest(args.root, args.manifest_path)
    packet_summary = capture_reference_packet(
        root=args.root,
        manifest_path=manifest_path,
        benchmark_ids=args.benchmark_id,
        required_only=args.required_only,
        optional_capture_packet=args.optional_capture_packet,
        keep_stage_workdirs=args.keep_stage_workdirs,
        resume_existing=args.resume_existing,
    )
    print(json.dumps(packet_summary, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
