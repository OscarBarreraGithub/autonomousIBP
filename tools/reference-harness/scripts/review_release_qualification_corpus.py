#!/usr/bin/env python3
"""Produce the M7 release-qualification-corpus sidecar for release readiness."""

from __future__ import annotations

import argparse
import json
import tempfile
from pathlib import Path
from typing import Any

from freeze_phase0_goldens import load_json
from release_signoff_readiness import (
    QUALIFICATION_CORPUS_REQUIRED_WITHHELD_CLAIMS,
    case_study_qualification_blockers,
    load_case_study_qualification_summary,
    load_phase0_qualification_summary,
    load_qualification_corpus_review_summary,
    load_qualification_summary,
    phase0_failure_code_blockers,
    write_synthetic_case_study_qualification_summary,
    write_synthetic_phase0_qualification_summary,
    write_synthetic_qualification_summary,
)


QUALIFICATION_CORPUS_REQUIRED_INPUTS: tuple[str, ...] = (
    "qualification readiness summary",
    "retained benchmark comparison summaries",
    "known-regression coverage notes",
)

QUALIFICATION_CORPUS_REQUIRED_OUTPUTS: tuple[str, ...] = (
    "closed benchmark-family coverage statement",
    "explicit residual blockers or carve-outs",
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


def find_qualification_corpus_section(checklist: dict[str, Any]) -> dict[str, Any] | None:
    for section in checklist["review_sections"]:
        if section["id"] == "qualification-corpus":
            return section
    return None


def required_values_present(values: list[str], required_values: tuple[str, ...]) -> bool:
    return all(value in values for value in required_values)


def qualification_evidence_is_coherent(summary: dict[str, Any]) -> bool:
    return all(
        summary[field]
        for field in [
            "required_root_reference_captured",
            "required_root_captures_required_set",
            "required_root_only_captures_required_set",
            "optional_packets_preserve_bootstrap_only_state",
            "optional_capture_packets_match_scaffold",
            "captured_examples_publish_reference_artifacts",
            "captured_examples_pass_comparison_checks",
            "observed_reference_captured_matches_scaffold",
            "pending_examples_preserve_runtime_lane_hints",
            "blocked_case_study_families_preserve_runtime_lane_hints",
        ]
    )


def summarize_qualification_corpus_review(
    *,
    checklist_path: Path,
    qualification_summary_path: Path,
    phase0_qualification_summary_path: Path | None,
    case_study_qualification_summary_path: Path | None,
    root: Path,
) -> dict[str, Any]:
    root = root.resolve(strict=False)
    checklist_path = checklist_path.resolve(strict=False)
    expect_path_within_root(checklist_path, root, "release checklist path")
    checklist = load_release_checklist(checklist_path)
    qualification_summary = load_qualification_summary(qualification_summary_path)
    phase0_summary = (
        load_phase0_qualification_summary(phase0_qualification_summary_path)
        if phase0_qualification_summary_path is not None
        else None
    )
    case_study_summary = (
        load_case_study_qualification_summary(case_study_qualification_summary_path)
        if case_study_qualification_summary_path is not None
        else None
    )

    missing_sources: list[str] = []
    for source_id, relative_path in checklist["sources"].items():
        source_path = root / relative_path
        expect_path_within_root(source_path, root, f"release checklist source {source_id}")
        if not source_path.exists():
            missing_sources.append(f"{source_id}:{relative_path}")

    qualification_section = find_qualification_corpus_section(checklist)
    qualification_corpus_required_inputs_preserved = False
    qualification_corpus_required_outputs_preserved = False
    if qualification_section is not None:
        qualification_corpus_required_inputs_preserved = required_values_present(
            qualification_section["required_inputs"],
            QUALIFICATION_CORPUS_REQUIRED_INPUTS,
        )
        qualification_corpus_required_outputs_preserved = required_values_present(
            qualification_section["required_outputs"],
            QUALIFICATION_CORPUS_REQUIRED_OUTPUTS,
        )

    qualification_evidence_coherent = qualification_evidence_is_coherent(qualification_summary)
    reviewed_phase0_ids = qualification_summary["phase0_reference_captured_ids"]
    pending_phase0_ids = qualification_summary["phase0_pending_ids"]
    blocked_case_study_ids = [
        entry["id"] for entry in qualification_summary["blocked_case_study_families"]
    ]
    phase0_blockers = phase0_failure_code_blockers(phase0_summary)
    case_study_blockers = case_study_qualification_blockers(case_study_summary)
    if (
        case_study_summary is not None
        and case_study_summary["milestone_m6_requires_phase0_verdict"]
        and (phase0_summary is None or not phase0_summary["phase0_packet_set_qualified"])
    ):
        case_study_blockers.append("case-study-requires-phase0-verdict")

    missing_or_blocked_paths: list[str] = []
    blocking_reasons: list[str] = []

    if qualification_section is None:
        missing_or_blocked_paths.append("release-checklist:qualification-corpus")
        blocking_reasons.append("release checklist does not define qualification-corpus review")
    if not qualification_corpus_required_inputs_preserved:
        missing_or_blocked_paths.append("release-checklist:qualification-corpus-inputs")
        blocking_reasons.append("qualification-corpus checklist required inputs are incomplete")
    if not qualification_corpus_required_outputs_preserved:
        missing_or_blocked_paths.append("release-checklist:qualification-corpus-outputs")
        blocking_reasons.append("qualification-corpus checklist required outputs are incomplete")
    if missing_sources:
        missing_or_blocked_paths.extend(
            f"release-checklist-source:{source}" for source in missing_sources
        )
        blocking_reasons.extend(
            f"release checklist source path is missing: {source}" for source in missing_sources
        )
    if not qualification_evidence_coherent:
        missing_or_blocked_paths.append("qualification-readiness-summary")
        blocking_reasons.append("qualification readiness evidence is incoherent")
    for phase0_id in pending_phase0_ids:
        missing_or_blocked_paths.append(f"phase0-pending:{phase0_id}")
    for case_study_id in blocked_case_study_ids:
        missing_or_blocked_paths.append(f"case-study-runtime:{case_study_id}")
    if phase0_summary is None:
        missing_or_blocked_paths.append("phase0-packet-set-verdict")
        blocking_reasons.append("phase-0 packet-set qualification verdict is not provided")
    elif not phase0_summary["phase0_packet_set_qualified"]:
        missing_or_blocked_paths.append(f"phase0-packet-set:{phase0_summary['current_state']}")
        blocking_reasons.extend(phase0_summary["blocking_reasons"])
    missing_or_blocked_paths.extend(phase0_blockers)
    if case_study_summary is None:
        missing_or_blocked_paths.append("case-study-family-verdict")
        blocking_reasons.append("case-study-family qualification verdict is not provided")
    elif not case_study_summary["case_study_families_qualified"]:
        missing_or_blocked_paths.append(f"case-study:{case_study_summary['current_state']}")
        blocking_reasons.extend(case_study_summary["blocking_reasons"])
    missing_or_blocked_paths.extend(case_study_blockers)

    phase0_packet_set_qualified = (
        phase0_summary["phase0_packet_set_qualified"] if phase0_summary is not None else False
    )
    case_study_families_qualified = (
        case_study_summary["case_study_families_qualified"]
        if case_study_summary is not None
        else False
    )
    closed_benchmark_family_coverage_statement_reviewed = (
        qualification_evidence_coherent
        and not pending_phase0_ids
        and not blocked_case_study_ids
        and phase0_packet_set_qualified
        and case_study_families_qualified
    )
    if not closed_benchmark_family_coverage_statement_reviewed:
        blocking_reasons.append("closed benchmark-family coverage statement is not reviewed")

    deduplicated_paths: list[str] = []
    seen_paths: set[str] = set()
    for path in missing_or_blocked_paths:
        if path not in seen_paths:
            deduplicated_paths.append(path)
            seen_paths.add(path)

    deduplicated_reasons: list[str] = []
    seen_reasons: set[str] = set()
    for reason in blocking_reasons:
        if reason not in seen_reasons:
            deduplicated_reasons.append(reason)
            seen_reasons.add(reason)

    residual_blockers_or_carveouts_preserved = bool(deduplicated_paths) or bool(
        deduplicated_reasons
    )
    qualification_corpus_review_complete = (
        qualification_corpus_required_inputs_preserved
        and qualification_corpus_required_outputs_preserved
        and qualification_evidence_coherent
        and phase0_summary is not None
        and phase0_packet_set_qualified
        and case_study_summary is not None
        and case_study_families_qualified
        and closed_benchmark_family_coverage_statement_reviewed
        and not deduplicated_paths
        and not deduplicated_reasons
    )

    if qualification_corpus_review_complete:
        current_state = "qualification-corpus-reviewed"
        residual_blockers_or_carveouts_preserved = True
    elif not qualification_evidence_coherent:
        current_state = "blocked-on-incoherent-qualification-evidence"
    else:
        current_state = "blocked-on-qualification-corpus-closure"

    return {
        "schema_version": 1,
        "scope": "release-qualification-corpus",
        "current_state": current_state,
        "checklist_path": str(checklist_path),
        "qualification_summary_path": str(qualification_summary_path),
        "phase0_qualification_summary_path": (
            str(phase0_qualification_summary_path)
            if phase0_qualification_summary_path is not None
            else ""
        ),
        "case_study_qualification_summary_path": (
            str(case_study_qualification_summary_path)
            if case_study_qualification_summary_path is not None
            else ""
        ),
        "qualification_corpus_review_complete": qualification_corpus_review_complete,
        "qualification_corpus_required_inputs_preserved": (
            qualification_corpus_required_inputs_preserved
        ),
        "qualification_corpus_required_outputs_preserved": (
            qualification_corpus_required_outputs_preserved
        ),
        "qualification_evidence_coherent": qualification_evidence_coherent,
        "phase0_packet_set_verdict_present": phase0_summary is not None,
        "phase0_packet_set_qualified": phase0_packet_set_qualified,
        "case_study_verdict_present": case_study_summary is not None,
        "case_study_families_qualified": case_study_families_qualified,
        "closed_benchmark_family_coverage_statement_reviewed": (
            closed_benchmark_family_coverage_statement_reviewed
        ),
        "residual_blockers_or_carveouts_preserved": (
            residual_blockers_or_carveouts_preserved
        ),
        "reviewed_phase0_ids": reviewed_phase0_ids,
        "pending_phase0_ids": pending_phase0_ids,
        "blocked_case_study_ids": blocked_case_study_ids,
        "phase0_failure_code_blockers": phase0_blockers,
        "case_study_qualification_blockers": case_study_blockers,
        "missing_or_blocked_qualification_paths": deduplicated_paths,
        "blocking_reasons": deduplicated_reasons,
        "withheld_claims": list(QUALIFICATION_CORPUS_REQUIRED_WITHHELD_CLAIMS),
    }


def write_synthetic_release_qualification_root(
    root: Path,
    *,
    complete_checklist_inputs: bool = True,
) -> Path:
    checklist_path = root / "tools/reference-harness/templates/release-signoff-checklist.json"
    sources = {
        "release_signoff_markdown": "docs/release-signoff-checklist.md",
        "qualification_scaffold": "tools/reference-harness/templates/qualification-benchmarks.json",
        "qualification_readiness_helper": "tools/reference-harness/scripts/qualification_readiness.py",
        "release_readiness_helper": "tools/reference-harness/scripts/release_signoff_readiness.py",
        "qualification_corpus_review_helper": (
            "tools/reference-harness/scripts/review_release_qualification_corpus.py"
        ),
        "parity_matrix": "specs/parity-matrix.yaml",
        "verification_strategy": "docs/verification-strategy.md",
    }
    required_inputs = list(QUALIFICATION_CORPUS_REQUIRED_INPUTS)
    if not complete_checklist_inputs:
        required_inputs = required_inputs[1:]

    write_json(
        checklist_path,
        {
            "schema_version": 1,
            "current_state": "planning-only",
            "sources": sources,
            "review_sections": [
                {
                    "id": "qualification-corpus",
                    "required_inputs": required_inputs,
                    "required_outputs": list(QUALIFICATION_CORPUS_REQUIRED_OUTPUTS),
                    "notes": "Synthetic qualification-corpus section.",
                }
            ],
        },
    )
    for relative_path in sources.values():
        write_text(
            root / relative_path,
            "Synthetic release qualification-corpus source\n"
            "qualification_readiness.py\n"
            "release_signoff_readiness.py\n"
            "review_release_qualification_corpus.py\n"
            "release-qualification-corpus\n",
        )
    return checklist_path


def run_self_check() -> dict[str, Any]:
    with tempfile.TemporaryDirectory(prefix="amflow-release-qualification-review-self-check-") as tmp:
        temp_root = Path(tmp)
        checklist_path = write_synthetic_release_qualification_root(temp_root)
        qualification_summary_path = temp_root / "qualification-summary.json"
        phase0_qualification_summary_path = temp_root / "phase0-qualification-summary.json"
        case_study_qualification_summary_path = (
            temp_root / "case-study-qualification-summary.json"
        )
        summary_path = temp_root / "qualification-corpus-summary.json"

        write_synthetic_qualification_summary(qualification_summary_path)
        write_synthetic_phase0_qualification_summary(phase0_qualification_summary_path)
        write_synthetic_case_study_qualification_summary(case_study_qualification_summary_path)

        summary = summarize_qualification_corpus_review(
            checklist_path=checklist_path,
            qualification_summary_path=qualification_summary_path,
            phase0_qualification_summary_path=phase0_qualification_summary_path,
            case_study_qualification_summary_path=case_study_qualification_summary_path,
            root=temp_root,
        )
        write_json(summary_path, summary)
        summary_written = summary_path.exists()
        loaded_summary = load_qualification_corpus_review_summary(summary_path)

    with tempfile.TemporaryDirectory(prefix="amflow-release-qualification-inputs-self-check-") as tmp:
        incomplete_root = Path(tmp)
        incomplete_checklist_path = write_synthetic_release_qualification_root(
            incomplete_root,
            complete_checklist_inputs=False,
        )
        qualification_summary_path = incomplete_root / "qualification-summary.json"
        write_synthetic_qualification_summary(qualification_summary_path)
        incomplete_summary = summarize_qualification_corpus_review(
            checklist_path=incomplete_checklist_path,
            qualification_summary_path=qualification_summary_path,
            phase0_qualification_summary_path=None,
            case_study_qualification_summary_path=None,
            root=incomplete_root,
        )

    return {
        "qualification_corpus_review_complete": summary[
            "qualification_corpus_review_complete"
        ],
        "qualification_corpus_required_inputs_preserved": summary[
            "qualification_corpus_required_inputs_preserved"
        ],
        "qualification_corpus_required_outputs_preserved": summary[
            "qualification_corpus_required_outputs_preserved"
        ],
        "release_readiness_schema_compatible": (
            loaded_summary["scope"] == "release-qualification-corpus"
            and loaded_summary["current_state"] == "blocked-on-qualification-corpus-closure"
            and not loaded_summary["qualification_corpus_review_complete"]
        ),
        "phase0_verdict_blocked": (
            not summary["phase0_packet_set_qualified"]
            and "phase0-packet-set:blocked-on-failure-code-audit"
            in summary["missing_or_blocked_qualification_paths"]
        ),
        "case_study_verdict_blocked": (
            not summary["case_study_families_qualified"]
            and "case-study:blocked-on-runtime-lanes"
            in summary["missing_or_blocked_qualification_paths"]
        ),
        "closed_coverage_statement_blocked": (
            not summary["closed_benchmark_family_coverage_statement_reviewed"]
            and "closed benchmark-family coverage statement is not reviewed"
            in summary["blocking_reasons"]
        ),
        "incomplete_checklist_blocked": (
            not incomplete_summary["qualification_corpus_required_inputs_preserved"]
            and "release-checklist:qualification-corpus-inputs"
            in incomplete_summary["missing_or_blocked_qualification_paths"]
        ),
        "summary_written": summary_written,
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--qualification-summary",
        type=Path,
        help="Path to the machine-readable summary emitted by qualification_readiness.py",
    )
    parser.add_argument(
        "--phase0-qualification-summary",
        type=Path,
        help="Optional path to the phase-0 packet-set qualification verdict summary",
    )
    parser.add_argument(
        "--case-study-qualification-summary",
        type=Path,
        help="Optional path to the case-study-family qualification verdict summary",
    )
    parser.add_argument(
        "--checklist-path",
        type=Path,
        help="Release-signoff checklist JSON path",
    )
    parser.add_argument(
        "--summary-path",
        type=Path,
        help="Optional output file for the qualification-corpus sidecar summary",
    )
    parser.add_argument(
        "--self-check",
        action="store_true",
        help="Run a synthetic qualification-corpus sidecar producer check",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.self_check:
        print(json.dumps(run_self_check(), indent=2, sort_keys=True))
        return 0
    if args.qualification_summary is None:
        raise SystemExit("--qualification-summary is required unless --self-check is used")

    root = repo_root()
    checklist_path = (
        args.checklist_path
        if args.checklist_path is not None
        else root / "tools" / "reference-harness" / "templates" / "release-signoff-checklist.json"
    )
    summary = summarize_qualification_corpus_review(
        checklist_path=checklist_path,
        qualification_summary_path=args.qualification_summary,
        phase0_qualification_summary_path=args.phase0_qualification_summary,
        case_study_qualification_summary_path=args.case_study_qualification_summary,
        root=root,
    )
    if args.summary_path is not None:
        write_json(args.summary_path, summary)
    print(json.dumps(summary, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
