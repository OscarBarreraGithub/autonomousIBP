#!/usr/bin/env python3
"""Produce the M7 release-diagnostic-review sidecar for release readiness."""

from __future__ import annotations

import argparse
import json
import tempfile
from pathlib import Path
from typing import Any

from freeze_phase0_goldens import load_json


DIAGNOSTIC_REVIEW_REQUIRED_INPUTS: tuple[str, ...] = (
    "required failure-code set from the parity matrix",
    "retained unstable-run evidence",
    "known-regression outcomes",
)

DIAGNOSTIC_REVIEW_REQUIRED_OUTPUTS: tuple[str, ...] = (
    "diagnostic coverage summary",
    "explicit missing or degraded diagnostic paths",
)

WITHHELD_CLAIMS: tuple[str, ...] = (
    "This summary does not claim diagnostic review completion.",
    "This summary does not claim Milestone M6 closure.",
    "This summary does not claim Milestone M7 closure.",
    "This summary does not claim release readiness.",
    "This summary does not run runtime diagnostics or widen runtime behavior.",
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

    required_failure_code_profiles = scaffold.get("required_failure_code_profiles")
    if not isinstance(required_failure_code_profiles, list):
        raise TypeError("qualification scaffold required_failure_code_profiles must be a list")
    known_regression_profiles = scaffold.get("known_regression_profiles")
    if not isinstance(known_regression_profiles, list):
        raise TypeError("qualification scaffold known_regression_profiles must be a list")

    failure_profile_ids: list[str] = []
    failure_codes: list[str] = []
    for entry in required_failure_code_profiles:
        if not isinstance(entry, dict):
            raise TypeError(
                "qualification scaffold required_failure_code_profiles entries must be objects"
            )
        entry_id = str(entry.get("id", "")).strip()
        if not entry_id:
            raise ValueError("qualification scaffold failure-code profile id must not be empty")
        codes = normalize_string_list(
            entry.get("codes", []),
            f"qualification scaffold failure-code profile {entry_id} codes",
        )
        expect(codes, f"qualification scaffold failure-code profile {entry_id} must name codes")
        expect_unique(codes, f"qualification scaffold failure-code profile {entry_id} codes")
        failure_profile_ids.append(entry_id)
        failure_codes.extend(codes)

    regression_profile_ids: list[str] = []
    regression_families: list[str] = []
    for entry in known_regression_profiles:
        if not isinstance(entry, dict):
            raise TypeError(
                "qualification scaffold known_regression_profiles entries must be objects"
            )
        entry_id = str(entry.get("id", "")).strip()
        if not entry_id:
            raise ValueError("qualification scaffold regression profile id must not be empty")
        families = normalize_string_list(
            entry.get("families", []),
            f"qualification scaffold regression profile {entry_id} families",
        )
        expect(families, f"qualification scaffold regression profile {entry_id} must name families")
        expect_unique(families, f"qualification scaffold regression profile {entry_id} families")
        regression_profile_ids.append(entry_id)
        regression_families.extend(families)

    expect_unique(failure_profile_ids, "qualification scaffold failure-code profile ids")
    expect_unique(regression_profile_ids, "qualification scaffold regression profile ids")
    expect(failure_codes, "qualification scaffold must name at least one required failure code")
    expect(
        regression_families,
        "qualification scaffold must name at least one known regression family",
    )

    return {
        **scaffold,
        "required_failure_codes": sorted(set(failure_codes)),
        "known_regression_families": sorted(set(regression_families)),
    }


def find_diagnostic_review_section(checklist: dict[str, Any]) -> dict[str, Any] | None:
    for section in checklist["review_sections"]:
        if section["id"] == "diagnostic-review":
            return section
    return None


def required_values_present(values: list[str], required_values: tuple[str, ...]) -> bool:
    return all(value in values for value in required_values)


def summarize_diagnostic_review(*, checklist_path: Path, root: Path) -> dict[str, Any]:
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
        "release diagnostic qualification scaffold",
    )
    scaffold: dict[str, Any] | None = None
    if qualification_scaffold_path.exists():
        scaffold = load_qualification_scaffold(qualification_scaffold_path)

    diagnostic_section = find_diagnostic_review_section(checklist)
    diagnostic_review_required_inputs_preserved = False
    diagnostic_review_required_outputs_preserved = False
    if diagnostic_section is not None:
        diagnostic_review_required_inputs_preserved = required_values_present(
            diagnostic_section["required_inputs"],
            DIAGNOSTIC_REVIEW_REQUIRED_INPUTS,
        )
        diagnostic_review_required_outputs_preserved = required_values_present(
            diagnostic_section["required_outputs"],
            DIAGNOSTIC_REVIEW_REQUIRED_OUTPUTS,
        )

    checklist_metadata_reviewed = (
        not missing_sources
        and diagnostic_section is not None
        and diagnostic_review_required_inputs_preserved
        and diagnostic_review_required_outputs_preserved
    )
    required_failure_code_profiles_reviewed = (
        checklist_metadata_reviewed
        and scaffold is not None
        and bool(scaffold["required_failure_codes"])
    )
    known_regression_outcomes_reviewed = (
        checklist_metadata_reviewed
        and scaffold is not None
        and bool(scaffold["known_regression_families"])
    )

    typed_failure_paths_preserved = False
    unstable_run_evidence_reviewed = False

    missing_or_degraded_diagnostic_paths: list[str] = []
    blocking_reasons: list[str] = []

    if diagnostic_section is None:
        missing_or_degraded_diagnostic_paths.append("release-checklist:diagnostic-review")
        blocking_reasons.append("release checklist does not define the diagnostic-review section")
    if not diagnostic_review_required_inputs_preserved:
        missing_or_degraded_diagnostic_paths.append("release-checklist:diagnostic-review-inputs")
        blocking_reasons.append("release checklist diagnostic-review required inputs are incomplete")
    if not diagnostic_review_required_outputs_preserved:
        missing_or_degraded_diagnostic_paths.append("release-checklist:diagnostic-review-outputs")
        blocking_reasons.append(
            "release checklist diagnostic-review required outputs are incomplete"
        )
    if missing_sources:
        missing_or_degraded_diagnostic_paths.extend(
            f"release-checklist-source:{source}" for source in missing_sources
        )
        blocking_reasons.extend(
            f"release checklist source path is missing: {source}" for source in missing_sources
        )
    if scaffold is None:
        missing_or_degraded_diagnostic_paths.append("qualification-scaffold")
        blocking_reasons.append("qualification scaffold is missing")
    if not required_failure_code_profiles_reviewed:
        missing_or_degraded_diagnostic_paths.append("required-failure-code-profiles")
        blocking_reasons.append("required failure-code profile metadata has not been reviewed")
    if not typed_failure_paths_preserved:
        missing_or_degraded_diagnostic_paths.append("typed-failure-path-review")
        blocking_reasons.append("typed failure-path preservation review is not complete")
    if not unstable_run_evidence_reviewed:
        missing_or_degraded_diagnostic_paths.append("retained-unstable-run-evidence")
        blocking_reasons.append("retained unstable-run diagnostic evidence has not been reviewed")
    if not known_regression_outcomes_reviewed:
        missing_or_degraded_diagnostic_paths.append("known-regression-outcomes")
        blocking_reasons.append("known-regression outcome metadata has not been reviewed")

    reviewed_failure_code_profiles = []
    if scaffold is not None:
        reviewed_failure_code_profiles = scaffold["required_failure_codes"]
    expect_unique(reviewed_failure_code_profiles, "diagnostic reviewed failure-code profiles")

    diagnostic_review_complete = (
        required_failure_code_profiles_reviewed
        and typed_failure_paths_preserved
        and unstable_run_evidence_reviewed
        and known_regression_outcomes_reviewed
        and not missing_or_degraded_diagnostic_paths
        and not blocking_reasons
    )

    return {
        "schema_version": 1,
        "scope": "release-diagnostic-review",
        "current_state": (
            "diagnostic-review-reviewed"
            if diagnostic_review_complete
            else "blocked-on-missing-typed-failure-review"
        ),
        "checklist_path": str(checklist_path),
        "qualification_scaffold_path": str(qualification_scaffold_path),
        "diagnostic_review_complete": diagnostic_review_complete,
        "required_failure_code_profiles_reviewed": required_failure_code_profiles_reviewed,
        "typed_failure_paths_preserved": typed_failure_paths_preserved,
        "unstable_run_evidence_reviewed": unstable_run_evidence_reviewed,
        "known_regression_outcomes_reviewed": known_regression_outcomes_reviewed,
        "diagnostic_review_required_inputs_preserved": (
            diagnostic_review_required_inputs_preserved
        ),
        "diagnostic_review_required_outputs_preserved": (
            diagnostic_review_required_outputs_preserved
        ),
        "reviewed_failure_code_profiles": reviewed_failure_code_profiles,
        "missing_or_degraded_diagnostic_paths": sorted(
            set(missing_or_degraded_diagnostic_paths)
        ),
        "blocking_reasons": blocking_reasons,
        "withheld_claims": list(WITHHELD_CLAIMS),
    }


def write_synthetic_release_diagnostic_root(
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
        "diagnostic_review_helper": "tools/reference-harness/scripts/review_release_diagnostic.py",
        "parity_matrix": "specs/parity-matrix.yaml",
        "verification_strategy": "docs/verification-strategy.md",
    }
    required_inputs = list(DIAGNOSTIC_REVIEW_REQUIRED_INPUTS)
    if not complete_checklist_inputs:
        required_inputs = required_inputs[1:]

    checklist = {
        "schema_version": 1,
        "current_state": "planning-only",
        "sources": sources,
        "review_sections": [
            {
                "id": "diagnostic-review",
                "required_inputs": required_inputs,
                "required_outputs": list(DIAGNOSTIC_REVIEW_REQUIRED_OUTPUTS),
                "notes": "Synthetic diagnostic-review section.",
            }
        ],
    }
    scaffold = {
        "schema_version": 1,
        "required_failure_code_profiles": [
            {
                "id": "default-required-failure-codes",
                "codes": [
                    "boundary_unsolved",
                    "continuation_budget_exhausted",
                    "unsupported_solver_path",
                ],
            }
        ],
        "known_regression_profiles": [
            {
                "id": "current-reviewed-regressions",
                "families": [
                    "asymptotic-series overflow",
                    "unexpected master sets in Kira interface",
                ],
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
            "Synthetic release diagnostic source\n"
            "release_signoff_readiness.py\n"
            "review_release_diagnostic.py\n"
            "release-diagnostic-review\n"
            "diagnostic_review_required_inputs_preserved\n",
        )
    return checklist_path


def run_self_check() -> dict[str, Any]:
    with tempfile.TemporaryDirectory(prefix="amflow-release-diagnostic-review-self-check-") as tmp:
        temp_root = Path(tmp)
        checklist_path = write_synthetic_release_diagnostic_root(temp_root)
        summary_path = temp_root / "diagnostic-review-summary.json"
        summary = summarize_diagnostic_review(checklist_path=checklist_path, root=temp_root)
        write_json(summary_path, summary)
        summary_written = summary_path.exists()

        from release_signoff_readiness import load_diagnostic_review_summary

        loaded_summary = load_diagnostic_review_summary(summary_path)

    with tempfile.TemporaryDirectory(prefix="amflow-release-diagnostic-inputs-self-check-") as tmp:
        incomplete_root = Path(tmp)
        incomplete_checklist_path = write_synthetic_release_diagnostic_root(
            incomplete_root,
            complete_checklist_inputs=False,
        )
        incomplete_summary = summarize_diagnostic_review(
            checklist_path=incomplete_checklist_path,
            root=incomplete_root,
        )

    return {
        "diagnostic_review_complete": summary["diagnostic_review_complete"],
        "diagnostic_review_required_inputs_preserved": (
            summary["diagnostic_review_required_inputs_preserved"]
        ),
        "required_failure_code_profiles_reviewed": (
            summary["required_failure_code_profiles_reviewed"]
        ),
        "known_regression_outcomes_reviewed": summary["known_regression_outcomes_reviewed"],
        "release_readiness_schema_compatible": (
            loaded_summary["scope"] == "release-diagnostic-review"
            and loaded_summary["current_state"] == "blocked-on-missing-typed-failure-review"
            and not loaded_summary["diagnostic_review_complete"]
        ),
        "typed_failure_paths_blocked": (
            "typed-failure-path-review" in summary["missing_or_degraded_diagnostic_paths"]
        ),
        "unstable_run_evidence_blocked": (
            "retained-unstable-run-evidence" in summary["missing_or_degraded_diagnostic_paths"]
        ),
        "incomplete_checklist_blocked": (
            not incomplete_summary["required_failure_code_profiles_reviewed"]
            and not incomplete_summary["diagnostic_review_required_inputs_preserved"]
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
        help="Optional output file for the diagnostic-review sidecar summary",
    )
    parser.add_argument(
        "--self-check",
        action="store_true",
        help="Run a synthetic diagnostic-review sidecar producer check",
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
    summary = summarize_diagnostic_review(checklist_path=checklist_path, root=root)
    if args.summary_path is not None:
        write_json(args.summary_path, summary)
    print(json.dumps(summary, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
