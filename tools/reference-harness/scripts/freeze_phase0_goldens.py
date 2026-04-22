#!/usr/bin/env python3
"""Freeze the phase-0 placeholder golden layout for the reference harness."""

from __future__ import annotations

import argparse
import copy
import json
import re
import tempfile
from pathlib import Path
from typing import Any


PLACEHOLDER_STATUSES = {"placeholder", "pending-reference-capture"}
VALID_BENCHMARK_ID = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]*$")
VALID_RUNTIME_LANE = re.compile(r"^[a-z][a-z0-9_]*$")


def ensure_dir(path: Path, dry_run: bool) -> None:
    if dry_run:
        return
    path.mkdir(parents=True, exist_ok=True)


def should_refresh_placeholder(path: Path, force: bool) -> bool:
    if force or not path.exists():
        return True
    existing = load_json(path)
    return str(existing.get("status", "")).strip() in PLACEHOLDER_STATUSES


def write_json(path: Path, payload: dict[str, Any], dry_run: bool, refresh: bool) -> None:
    if dry_run:
        return
    if not refresh:
        return
    path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def load_json(path: Path) -> dict[str, Any]:
    return json.loads(path.read_text(encoding="utf-8"))


def validate_benchmark_id(raw_id: Any) -> str:
    if not isinstance(raw_id, str):
        raise TypeError(f"benchmark id must be a string, got {type(raw_id).__name__}")
    if raw_id != raw_id.strip():
        raise ValueError(f"benchmark id has surrounding whitespace: {raw_id!r}")
    if not raw_id:
        raise ValueError("benchmark id must not be empty")
    if raw_id in {".", ".."}:
        raise ValueError(f"benchmark id is not path-safe: {raw_id!r}")
    if not VALID_BENCHMARK_ID.fullmatch(raw_id):
        raise ValueError(
            "benchmark id must match "
            f"{VALID_BENCHMARK_ID.pattern!r} and contain no path separators: {raw_id!r}"
        )
    parts = Path(raw_id).parts
    if len(parts) != 1 or parts[0] != raw_id:
        raise ValueError(f"benchmark id is not path-safe: {raw_id!r}")
    return raw_id


def normalize_runtime_lane(raw_lane: Any) -> str:
    if raw_lane is None:
        return ""
    if not isinstance(raw_lane, str):
        raise TypeError(f"runtime lane must be a string, got {type(raw_lane).__name__}")
    if raw_lane != raw_lane.strip():
        raise ValueError(f"runtime lane has surrounding whitespace: {raw_lane!r}")
    if not VALID_RUNTIME_LANE.fullmatch(raw_lane):
        raise ValueError(
            "runtime lane must match "
            f"{VALID_RUNTIME_LANE.pattern!r}: {raw_lane!r}"
        )
    return raw_lane


def normalize_benchmark_entry(raw: Any) -> dict[str, Any]:
    if isinstance(raw, str):
        benchmark_id = validate_benchmark_id(raw)
        return {
            "id": benchmark_id,
            "label": benchmark_id,
            "required": False,
            "feature_gate": "phase0",
            "oracle": "upstream-amflow",
            "notes": "",
        }

    benchmark_id = validate_benchmark_id(raw["id"])
    return {
        "id": benchmark_id,
        "label": raw.get("label", benchmark_id),
        "required": bool(raw.get("required", False)),
        "feature_gate": raw.get("feature_gate", "phase0"),
        "oracle": raw.get("oracle", "upstream-amflow"),
        "next_runtime_lane": normalize_runtime_lane(raw.get("next_runtime_lane")),
        "notes": raw.get("notes", ""),
    }


def freeze_phase0_placeholders(
    *,
    root: Path,
    manifest_path: Path | None,
    benchmark_catalog: Path,
    template_dir: Path,
    dry_run: bool,
    force: bool,
) -> dict[str, Any]:
    catalog = load_json(benchmark_catalog)
    raw_benchmarks = catalog.get("phase0_benchmarks", [])
    benchmarks = [normalize_benchmark_entry(entry) for entry in raw_benchmarks]
    seen_ids: set[str] = set()
    for benchmark in benchmarks:
        benchmark_id = benchmark["id"]
        if benchmark_id in seen_ids:
            raise ValueError(f"duplicate benchmark id in catalog: {benchmark_id}")
        seen_ids.add(benchmark_id)

    golden_template = load_json(template_dir / "phase0-golden.template.json")
    comparison_template = load_json(template_dir / "comparison-summary.template.json")

    phase_root = root / "goldens" / "phase0"
    comparison_root = root / "comparisons" / "phase0"
    logs_root = root / "logs" / "phase0"
    results_root = root / "results" / "phase0"
    config_root = root / "generated-config" / "phase0"

    created: list[dict[str, Any]] = []
    refreshed_count = 0
    preserved_count = 0
    for benchmark in benchmarks:
        benchmark_id = benchmark["id"]
        benchmark_golden_root = phase_root / benchmark_id
        benchmark_results_root = results_root / benchmark_id
        benchmark_logs_root = logs_root / benchmark_id
        benchmark_config_root = config_root / benchmark_id

        ensure_dir(benchmark_golden_root, dry_run)
        ensure_dir(benchmark_results_root, dry_run)
        ensure_dir(benchmark_logs_root, dry_run)
        ensure_dir(benchmark_config_root, dry_run)
        ensure_dir(comparison_root, dry_run)

        golden_metadata_path = benchmark_golden_root / "metadata.json"
        coefficient_placeholder_path = benchmark_golden_root / "coefficients.placeholder.json"
        comparison_path = comparison_root / f"{benchmark_id}.pending.json"

        golden_metadata = copy.deepcopy(golden_template)
        golden_metadata["benchmark_id"] = benchmark_id
        golden_metadata["benchmark_label"] = benchmark["label"]
        golden_metadata["required"] = benchmark["required"]
        golden_metadata["feature_gate"] = benchmark["feature_gate"]
        if benchmark["next_runtime_lane"]:
            golden_metadata["next_runtime_lane"] = benchmark["next_runtime_lane"]
        golden_metadata["oracle"]["kind"] = benchmark["oracle"]
        golden_metadata["oracle"]["manifest"] = str(manifest_path) if manifest_path else ""
        golden_metadata["oracle"]["benchmark_catalog"] = str(benchmark_catalog)
        golden_metadata["expected_outputs"]["result_manifest"] = str(
            benchmark_results_root / "result-manifest.json"
        )
        golden_metadata["expected_outputs"]["replacement_rules"] = str(
            benchmark_results_root / "replacement-rules.json"
        )
        golden_metadata["expected_outputs"]["coefficient_table"] = str(
            coefficient_placeholder_path
        )
        golden_metadata["expected_outputs"]["wolfram_log"] = str(
            benchmark_logs_root / "wolfram.log"
        )
        golden_metadata["expected_outputs"]["kira_log"] = str(benchmark_logs_root / "kira.log")
        golden_metadata["expected_outputs"]["generated_config_dir"] = str(benchmark_config_root)
        if benchmark["notes"]:
            golden_metadata["notes"].append(benchmark["notes"])

        coefficient_placeholder = {
            "schema_version": 1,
            "benchmark_id": benchmark_id,
            "status": "placeholder",
            "coefficients": [],
            "notes": [
                "Populate this table from the pinned upstream AMFlow reference run.",
                "The file exists now so downstream comparison tooling can bind to a stable path.",
            ],
        }

        comparison_summary = copy.deepcopy(comparison_template)
        comparison_summary["benchmark_id"] = benchmark_id
        comparison_summary["reference_golden"] = str(golden_metadata_path)
        comparison_summary["latest_run_result"] = str(benchmark_results_root / "result-manifest.json")

        metadata_refresh = should_refresh_placeholder(golden_metadata_path, force)
        coefficient_refresh = should_refresh_placeholder(coefficient_placeholder_path, force)
        comparison_refresh = should_refresh_placeholder(comparison_path, force)

        write_json(golden_metadata_path, golden_metadata, dry_run, metadata_refresh)
        write_json(coefficient_placeholder_path, coefficient_placeholder, dry_run, coefficient_refresh)
        write_json(comparison_path, comparison_summary, dry_run, comparison_refresh)

        refreshed_count += sum([metadata_refresh, coefficient_refresh, comparison_refresh])
        preserved_count += sum([not metadata_refresh, not coefficient_refresh, not comparison_refresh])

        created.append(
            {
                "benchmark_id": benchmark_id,
                "golden_metadata": str(golden_metadata_path),
                "coefficient_placeholder": str(coefficient_placeholder_path),
                "comparison_summary": str(comparison_path),
                "metadata_refreshed": metadata_refresh,
                "coefficient_placeholder_refreshed": coefficient_refresh,
                "comparison_summary_refreshed": comparison_refresh,
            }
        )

    index = {
        "schema_version": 1,
        "phase": "phase0",
        "manifest": str(manifest_path) if manifest_path else "",
        "benchmark_catalog": str(benchmark_catalog),
        "refresh_mode": "placeholder-only" if not force else "force-all",
        "benchmarks": created,
    }
    index_path = phase_root / "index.json"
    write_json(index_path, index, dry_run, True)
    return {
        "benchmark_count": len(created),
        "index_path": str(index_path),
        "refresh_mode": index["refresh_mode"],
        "refreshed_placeholder_files": refreshed_count,
        "preserved_existing_files": preserved_count,
        "benchmarks": created,
    }


def run_self_check(template_dir: Path) -> dict[str, Any]:
    with tempfile.TemporaryDirectory(prefix="amflow-phase0-self-check-") as tmp:
        temp_root = Path(tmp) / "harness"
        benchmark_catalog = Path(tmp) / "catalog.json"
        bad_catalog = Path(tmp) / "bad-catalog.json"
        manifest_path = temp_root / "manifests" / "self-check.json"

        ensure_dir(manifest_path.parent, False)
        manifest_path.write_text("{}\n", encoding="utf-8")
        benchmark_catalog.write_text(
            json.dumps(
                {
                    "phase0_benchmarks": [
                        {
                            "id": "automatic_vs_manual",
                            "label": "old-label",
                            "required": True,
                        }
                    ]
                },
                indent=2,
                sort_keys=True,
            )
            + "\n",
            encoding="utf-8",
        )
        first_summary = freeze_phase0_placeholders(
            root=temp_root,
            manifest_path=manifest_path,
            benchmark_catalog=benchmark_catalog,
            template_dir=template_dir,
            dry_run=False,
            force=False,
        )

        metadata_path = temp_root / "goldens" / "phase0" / "automatic_vs_manual" / "metadata.json"
        coefficients_path = (
            temp_root / "goldens" / "phase0" / "automatic_vs_manual" / "coefficients.placeholder.json"
        )

        benchmark_catalog.write_text(
            json.dumps(
                {
                    "phase0_benchmarks": [
                        {
                            "id": "automatic_vs_manual",
                            "label": "new-label",
                            "required": False,
                        }
                    ]
                },
                indent=2,
                sort_keys=True,
            )
            + "\n",
            encoding="utf-8",
        )
        second_summary = freeze_phase0_placeholders(
            root=temp_root,
            manifest_path=manifest_path,
            benchmark_catalog=benchmark_catalog,
            template_dir=template_dir,
            dry_run=False,
            force=False,
        )
        refreshed_metadata = load_json(metadata_path)

        coefficients_path.write_text(
            json.dumps(
                {
                    "schema_version": 1,
                    "benchmark_id": "automatic_vs_manual",
                    "status": "captured",
                    "coefficients": [{"order": 0, "value": "1.0"}],
                },
                indent=2,
                sort_keys=True,
            )
            + "\n",
            encoding="utf-8",
        )
        preserved_summary = freeze_phase0_placeholders(
            root=temp_root,
            manifest_path=manifest_path,
            benchmark_catalog=benchmark_catalog,
            template_dir=template_dir,
            dry_run=False,
            force=False,
        )
        preserved_coefficients = load_json(coefficients_path)

        bad_catalog.write_text(
            json.dumps({"phase0_benchmarks": [{"id": "../../escaped"}]}, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        unsafe_id_rejected = False
        unsafe_id_message = ""
        try:
            freeze_phase0_placeholders(
                root=temp_root,
                manifest_path=manifest_path,
                benchmark_catalog=bad_catalog,
                template_dir=template_dir,
                dry_run=False,
                force=False,
            )
        except ValueError as error:
            unsafe_id_rejected = "benchmark id" in str(error)
            unsafe_id_message = str(error)

        return {
            "initial_benchmark_count": first_summary["benchmark_count"],
            "placeholder_metadata_refreshed": refreshed_metadata["benchmark_label"] == "new-label"
            and refreshed_metadata["required"] is False,
            "placeholder_refresh_reported": second_summary["refreshed_placeholder_files"] >= 1,
            "promoted_artifact_preserved": preserved_coefficients["status"] == "captured",
            "preserved_existing_reported": preserved_summary["preserved_existing_files"] >= 1,
            "unsafe_id_rejected": unsafe_id_rejected,
            "unsafe_id_message": unsafe_id_message,
        }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True, help="Reference harness root")
    parser.add_argument("--manifest-path", type=Path, help="Reference manifest for this bootstrap")
    parser.add_argument(
        "--benchmark-catalog",
        type=Path,
        help="Phase-0 benchmark catalog JSON. Defaults to the copied template under the root.",
    )
    parser.add_argument(
        "--template-dir",
        type=Path,
        default=Path(__file__).resolve().parent.parent / "templates",
        help="Directory containing the golden metadata templates",
    )
    parser.add_argument("--dry-run", action="store_true", help="Plan the golden layout without writing")
    parser.add_argument(
        "--force",
        action="store_true",
        help=(
            "Overwrite existing files even if they no longer carry a placeholder status. "
            "Default refresh only rewrites missing or placeholder-status files."
        ),
    )
    parser.add_argument(
        "--self-check",
        action="store_true",
        help="Run a local regression check for path-safe benchmark IDs and placeholder refresh behavior",
    )
    args = parser.parse_args()

    if args.self_check:
        print(json.dumps(run_self_check(args.template_dir), indent=2, sort_keys=True))
        return 0

    benchmark_catalog = args.benchmark_catalog or args.root / "templates" / "phase0-benchmarks.json"
    summary = freeze_phase0_placeholders(
        root=args.root,
        manifest_path=args.manifest_path,
        benchmark_catalog=benchmark_catalog,
        template_dir=args.template_dir,
        dry_run=args.dry_run,
        force=args.force,
    )
    print(json.dumps(summary, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
