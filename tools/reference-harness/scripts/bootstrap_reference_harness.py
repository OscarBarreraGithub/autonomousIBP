#!/usr/bin/env python3
"""Bootstrap the phase-0 AMFlow reference harness."""

from __future__ import annotations

import argparse
import json
import shutil
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


def main() -> int:
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
    args = parser.parse_args()

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

    state_summary = {
        "root": str(root),
        "manifest": str(manifest_path),
        "copied_templates": copied_templates,
        "upstream": upstream,
        "placeholders": placeholder_summary,
        "dry_run": args.dry_run,
    }
    state_summary_path = layout["state"] / f"{manifest_name}.bootstrap.json"
    write_text(state_summary_path, json.dumps(state_summary, indent=2, sort_keys=True) + "\n", args.dry_run)

    if args.dry_run:
        print("dry-run reference harness layout:")
    else:
        print("reference harness initialized:")
    for name, path in layout.items():
        print(f"- {name}: {path}")
    print(f"- manifest: {manifest_path}")
    print(f"- state-summary: {state_summary_path}")
    print(f"- templates-copied: {', '.join(copied_templates) if copied_templates else '(none)'}")
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
    print(f"- phase0-placeholders: {placeholder_summary['benchmark_count']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
