#!/usr/bin/env python3
"""Produce the M7 release-performance-review sidecar for release readiness."""

from __future__ import annotations

import argparse
import json
import tempfile
from pathlib import Path
from typing import Any

from freeze_phase0_goldens import load_json


PERFORMANCE_REVIEW_REQUIRED_INPUTS: tuple[str, ...] = (
    "mandatory benchmark timings",
    "reviewed benchmark-family scope",
    "clean rebuild gate output",
)

PERFORMANCE_REVIEW_REQUIRED_OUTPUTS: tuple[str, ...] = (
    "performance summary",
    "explicit unstable or unreviewed performance carve-outs",
)

WITHHELD_CLAIMS: tuple[str, ...] = (
    "This summary does not claim performance review completion.",
    "This summary does not claim Milestone M6 closure.",
    "This summary does not claim Milestone M7 closure.",
    "This summary does not claim release readiness.",
    "This summary does not run benchmark timings or widen runtime behavior.",
)


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def write_json(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def write_text(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")


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


def load_release_checklist(checklist_path: Path) -> dict[str, Any]:
    checklist = load_json(checklist_path)
    expect(checklist.get("schema_version") == 1, "release checklist schema_version must be 1")

    sources = checklist.get("sources")
    if not isinstance(sources, dict):
        raise TypeError("release checklist sources must be an object")
    normalized_sources: dict[str, str] = {}
    for key, value in sources.items():
        if not isinstance(key, str) or not key.strip():
            raise ValueError("release checklist source id must be a non-empty string")
        if not isinstance(value, str) or not value.strip():
            raise ValueError(f"release checklist source {key} must be a non-empty string path")
        normalized_sources[key.strip()] = value.strip()

    review_sections = checklist.get("review_sections")
    if not isinstance(review_sections, list):
        raise TypeError("release checklist review_sections must be a list")
    normalized_sections: list[dict[str, Any]] = []
    section_ids: list[str] = []
    for section in review_sections:
        if not isinstance(section, dict):
            raise TypeError("release checklist review_sections entries must be objects")
        section_id = str(section.get("id", "")).strip()
        if not section_id:
            raise ValueError("release checklist review section id must not be empty")
        section_ids.append(section_id)
        normalized_sections.append(
            {
                **section,
                "id": section_id,
                "required_inputs": normalize_string_list(
                    section.get("required_inputs", []),
                    f"release checklist review section {section_id} required_inputs",
                ),
                "required_outputs": normalize_string_list(
                    section.get("required_outputs", []),
                    f"release checklist review section {section_id} required_outputs",
                ),
            }
        )
    expect_unique(section_ids, "release checklist review section ids")

    return {
        **checklist,
        "sources": normalized_sources,
        "review_sections": normalized_sections,
    }


def load_qualification_scaffold(scaffold_path: Path) -> dict[str, Any]:
    scaffold = load_json(scaffold_path)
    expect(scaffold.get("schema_version") == 1, "qualification scaffold schema_version must be 1")

    phase0_example_classes = scaffold.get("phase0_example_classes")
    if not isinstance(phase0_example_classes, list):
        raise TypeError("qualification scaffold phase0_example_classes must be a list")
    case_study_families = scaffold.get("case_study_families")
    if not isinstance(case_study_families, list):
        raise TypeError("qualification scaffold case_study_families must be a list")

    phase0_ids: list[str] = []
    for entry in phase0_example_classes:
        if not isinstance(entry, dict):
            raise TypeError("qualification scaffold phase0_example_classes entries must be objects")
        entry_id = str(entry.get("id", "")).strip()
        if not entry_id:
            raise ValueError("qualification scaffold phase0 example id must not be empty")
        if not str(entry.get("digit_threshold_profile", "")).strip():
            raise ValueError(
                f"qualification scaffold phase0 example {entry_id} lacks digit_threshold_profile"
            )
        phase0_ids.append(entry_id)

    case_study_ids: list[str] = []
    for entry in case_study_families:
        if not isinstance(entry, dict):
            raise TypeError("qualification scaffold case_study_families entries must be objects")
        entry_id = str(entry.get("id", "")).strip()
        if not entry_id:
            raise ValueError("qualification scaffold case-study family id must not be empty")
        if not str(entry.get("digit_threshold_profile", "")).strip():
            raise ValueError(
                f"qualification scaffold case-study family {entry_id} lacks digit_threshold_profile"
            )
        case_study_ids.append(entry_id)

    expect_unique(phase0_ids, "qualification scaffold phase0 ids")
    expect_unique(case_study_ids, "qualification scaffold case-study ids")
    expect(phase0_ids, "qualification scaffold must name at least one phase0 example")
    expect(case_study_ids, "qualification scaffold must name at least one case-study family")

    return {
        **scaffold,
        "phase0_ids": phase0_ids,
        "case_study_ids": case_study_ids,
    }


def find_performance_review_section(checklist: dict[str, Any]) -> dict[str, Any] | None:
    for section in checklist["review_sections"]:
        if section["id"] == "performance-review":
            return section
    return None


def required_values_present(values: list[str], required_values: tuple[str, ...]) -> bool:
    return all(value in values for value in required_values)


def summarize_performance_review(*, checklist_path: Path, root: Path) -> dict[str, Any]:
    root = root.resolve(strict=False)
    checklist_path = checklist_path.resolve(strict=False)
    expect_path_within_root(checklist_path, root, "release checklist path")
    checklist = load_release_checklist(checklist_path)

    missing_sources: list[str] = []
    for source_id, relative_path in checklist["sources"].items():
        source_path = root / relative_path
        expect_path_within_root(source_path, root, f"release checklist source {source_id}")
        if not source_path.exists():
            missing_sources.append(f"{source_id}:{relative_path}")

    qualification_scaffold_path = root / checklist["sources"].get(
        "qualification_scaffold",
        "tools/reference-harness/templates/qualification-benchmarks.json",
    )
    expect_path_within_root(
        qualification_scaffold_path,
        root,
        "release performance qualification scaffold",
    )
    scaffold: dict[str, Any] | None = None
    if qualification_scaffold_path.exists():
        scaffold = load_qualification_scaffold(qualification_scaffold_path)

    performance_section = find_performance_review_section(checklist)
    performance_review_required_inputs_preserved = False
    performance_review_required_outputs_preserved = False
    if performance_section is not None:
        performance_review_required_inputs_preserved = required_values_present(
            performance_section["required_inputs"],
            PERFORMANCE_REVIEW_REQUIRED_INPUTS,
        )
        performance_review_required_outputs_preserved = required_values_present(
            performance_section["required_outputs"],
            PERFORMANCE_REVIEW_REQUIRED_OUTPUTS,
        )

    benchmark_family_scope_reviewed = (
        not missing_sources
        and performance_section is not None
        and performance_review_required_inputs_preserved
        and performance_review_required_outputs_preserved
        and scaffold is not None
        and bool(scaffold["phase0_ids"])
        and bool(scaffold["case_study_ids"])
    )

    mandatory_benchmark_timings_reviewed = False
    clean_rebuild_gate_reviewed = False
    unstable_performance_runs_reviewed = False

    missing_or_unreviewed_performance_paths: list[str] = []
    blocking_reasons: list[str] = []

    if performance_section is None:
        missing_or_unreviewed_performance_paths.append("release-checklist:performance-review")
        blocking_reasons.append("release checklist does not define the performance-review section")
    if not performance_review_required_inputs_preserved:
        missing_or_unreviewed_performance_paths.append("release-checklist:performance-review-inputs")
        blocking_reasons.append("release checklist performance-review required inputs are incomplete")
    if not performance_review_required_outputs_preserved:
        missing_or_unreviewed_performance_paths.append("release-checklist:performance-review-outputs")
        blocking_reasons.append("release checklist performance-review required outputs are incomplete")
    if missing_sources:
        missing_or_unreviewed_performance_paths.extend(
            f"release-checklist-source:{source}" for source in missing_sources
        )
        blocking_reasons.extend(
            f"release checklist source path is missing: {source}" for source in missing_sources
        )
    if scaffold is None:
        missing_or_unreviewed_performance_paths.append("qualification-scaffold")
        blocking_reasons.append("qualification scaffold is missing")
    if not mandatory_benchmark_timings_reviewed:
        missing_or_unreviewed_performance_paths.append("mandatory-benchmark-timings")
        blocking_reasons.append("mandatory benchmark timing evidence has not been reviewed")
    if not clean_rebuild_gate_reviewed:
        missing_or_unreviewed_performance_paths.append("clean-rebuild-gate-output")
        blocking_reasons.append("clean rebuild gate output has not been reviewed for performance")
    if not unstable_performance_runs_reviewed:
        missing_or_unreviewed_performance_paths.append("unstable-performance-run-review")
        blocking_reasons.append("unstable or unreviewed performance run evidence remains open")

    reviewed_benchmark_families = []
    if scaffold is not None:
        reviewed_benchmark_families = [
            *(f"phase0:{entry_id}" for entry_id in scaffold["phase0_ids"]),
            *(f"case-study:{entry_id}" for entry_id in scaffold["case_study_ids"]),
        ]
    expect_unique(reviewed_benchmark_families, "performance reviewed benchmark families")

    performance_review_complete = (
        mandatory_benchmark_timings_reviewed
        and benchmark_family_scope_reviewed
        and clean_rebuild_gate_reviewed
        and unstable_performance_runs_reviewed
        and not missing_or_unreviewed_performance_paths
        and not blocking_reasons
    )

    return {
        "schema_version": 1,
        "scope": "release-performance-review",
        "current_state": (
            "performance-review-reviewed"
            if performance_review_complete
            else "blocked-on-unreviewed-benchmark-timings"
        ),
        "checklist_path": str(checklist_path),
        "qualification_scaffold_path": str(qualification_scaffold_path),
        "performance_review_complete": performance_review_complete,
        "mandatory_benchmark_timings_reviewed": mandatory_benchmark_timings_reviewed,
        "benchmark_family_scope_reviewed": benchmark_family_scope_reviewed,
        "clean_rebuild_gate_reviewed": clean_rebuild_gate_reviewed,
        "unstable_performance_runs_reviewed": unstable_performance_runs_reviewed,
        "performance_review_required_inputs_preserved": (
            performance_review_required_inputs_preserved
        ),
        "performance_review_required_outputs_preserved": (
            performance_review_required_outputs_preserved
        ),
        "reviewed_benchmark_families": reviewed_benchmark_families,
        "missing_or_unreviewed_performance_paths": sorted(
            set(missing_or_unreviewed_performance_paths)
        ),
        "blocking_reasons": blocking_reasons,
        "withheld_claims": list(WITHHELD_CLAIMS),
    }


def write_synthetic_release_performance_root(
    root: Path,
    *,
    complete_checklist_inputs: bool = True,
) -> Path:
    checklist_path = root / "tools/reference-harness/templates/release-signoff-checklist.json"
    scaffold_path = root / "tools/reference-harness/templates/qualification-benchmarks.json"
    sources = {
        "release_signoff_markdown": "docs/release-signoff-checklist.md",
        "qualification_scaffold": "tools/reference-harness/templates/qualification-benchmarks.json",
        "release_readiness_helper": "tools/reference-harness/scripts/release_signoff_readiness.py",
        "performance_review_helper": "tools/reference-harness/scripts/review_release_performance.py",
        "parity_matrix": "specs/parity-matrix.yaml",
        "verification_strategy": "docs/verification-strategy.md",
    }
    required_inputs = list(PERFORMANCE_REVIEW_REQUIRED_INPUTS)
    if not complete_checklist_inputs:
        required_inputs = required_inputs[1:]

    checklist = {
        "schema_version": 1,
        "current_state": "planning-only",
        "sources": sources,
        "review_sections": [
            {
                "id": "performance-review",
                "required_inputs": required_inputs,
                "required_outputs": list(PERFORMANCE_REVIEW_REQUIRED_OUTPUTS),
                "notes": "Synthetic performance-review section.",
            }
        ],
    }
    scaffold = {
        "schema_version": 1,
        "phase0_example_classes": [
            {
                "id": "automatic_vs_manual",
                "digit_threshold_profile": "core-package-family-default",
            },
            {
                "id": "automatic_loop",
                "digit_threshold_profile": "core-package-family-default",
            },
        ],
        "case_study_families": [
            {
                "id": "ttbar-j",
                "digit_threshold_profile": "core-package-family-default",
            }
        ],
    }
    write_json(checklist_path, checklist)
    write_json(scaffold_path, scaffold)
    for relative_path in sources.values():
        if relative_path == "tools/reference-harness/templates/qualification-benchmarks.json":
            continue
        write_text(
            root / relative_path,
            "Synthetic release performance source\n"
            "release_signoff_readiness.py\n"
            "review_release_performance.py\n"
            "release-performance-review\n"
            "performance_review_required_inputs_preserved\n",
        )
    return checklist_path


def run_self_check() -> dict[str, Any]:
    with tempfile.TemporaryDirectory(prefix="amflow-release-performance-review-self-check-") as tmp:
        temp_root = Path(tmp)
        checklist_path = write_synthetic_release_performance_root(temp_root)
        summary_path = temp_root / "performance-review-summary.json"
        summary = summarize_performance_review(checklist_path=checklist_path, root=temp_root)
        write_json(summary_path, summary)
        summary_written = summary_path.exists()

        from release_signoff_readiness import load_performance_review_summary

        loaded_summary = load_performance_review_summary(summary_path)

    with tempfile.TemporaryDirectory(prefix="amflow-release-performance-inputs-self-check-") as tmp:
        incomplete_root = Path(tmp)
        incomplete_checklist_path = write_synthetic_release_performance_root(
            incomplete_root,
            complete_checklist_inputs=False,
        )
        incomplete_summary = summarize_performance_review(
            checklist_path=incomplete_checklist_path,
            root=incomplete_root,
        )

    return {
        "performance_review_complete": summary["performance_review_complete"],
        "performance_review_required_inputs_preserved": (
            summary["performance_review_required_inputs_preserved"]
        ),
        "benchmark_family_scope_reviewed": summary["benchmark_family_scope_reviewed"],
        "release_readiness_schema_compatible": (
            loaded_summary["scope"] == "release-performance-review"
            and loaded_summary["current_state"] == "blocked-on-unreviewed-benchmark-timings"
            and not loaded_summary["performance_review_complete"]
        ),
        "missing_timing_blocked": (
            not summary["performance_review_complete"]
            and "mandatory-benchmark-timings"
            in summary["missing_or_unreviewed_performance_paths"]
        ),
        "clean_rebuild_review_blocked": (
            "clean-rebuild-gate-output" in summary["missing_or_unreviewed_performance_paths"]
        ),
        "incomplete_checklist_blocked": (
            not incomplete_summary["benchmark_family_scope_reviewed"]
            and not incomplete_summary["performance_review_required_inputs_preserved"]
        ),
        "summary_written": summary_written,
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--checklist-path",
        type=Path,
        help="Release-signoff checklist JSON path",
    )
    parser.add_argument(
        "--summary-path",
        type=Path,
        help="Optional output file for the performance-review sidecar summary",
    )
    parser.add_argument(
        "--self-check",
        action="store_true",
        help="Run a synthetic performance-review sidecar producer check",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.self_check:
        print(json.dumps(run_self_check(), indent=2, sort_keys=True))
        return 0

    root = repo_root()
    checklist_path = (
        args.checklist_path
        if args.checklist_path is not None
        else root / "tools" / "reference-harness" / "templates" / "release-signoff-checklist.json"
    )
    summary = summarize_performance_review(checklist_path=checklist_path, root=root)
    if args.summary_path is not None:
        write_json(args.summary_path, summary)
    print(json.dumps(summary, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
