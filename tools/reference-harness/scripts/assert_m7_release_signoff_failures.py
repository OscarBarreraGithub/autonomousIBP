#!/usr/bin/env python3
"""CTest gate for M7 release-readiness failure-mode regressions."""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Any


ACCEPTED_READINESS_SIDECAR = Path(
    "tools/reference-harness/specs/m7/lane3/"
    "release-readiness.m5-accepted.full-output.json"
)

READINESS_INPUTS: tuple[tuple[str, str], ...] = (
    ("--qualification-summary", "qualification_summary_path"),
    ("--checklist-path", "checklist_path"),
    ("--qualification-corpus-summary", "qualification_corpus_summary_path"),
    ("--m5-qualification-summary", "m5_qualification_summary_path"),
    ("--m6-qualification-summary", "m6_qualification_summary_path"),
    ("--phase0-qualification-summary", "phase0_qualification_summary_path"),
    ("--case-study-qualification-summary", "case_study_qualification_summary_path"),
    ("--performance-review-summary", "performance_review_summary_path"),
    ("--diagnostic-review-summary", "diagnostic_review_summary_path"),
    ("--docs-completion-summary", "docs_completion_summary_path"),
    ("--parity-signoff-summary", "parity_signoff_summary_path"),
)


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def read_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as stream:
        payload = json.load(stream)
    expect(isinstance(payload, dict), f"{path} must contain a JSON object")
    return payload


def write_json(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def require_repo_path(root: Path, raw: Any, field: str) -> str:
    expect(isinstance(raw, str) and raw.strip(), f"{field} must be a non-empty string")
    value = raw.strip()
    path = root / value
    try:
        path.resolve(strict=False).relative_to(root.resolve(strict=False))
    except ValueError as error:
        raise RuntimeError(f"{field} must stay within the repository: {value}") from error
    expect(path.exists(), f"{field} does not exist: {value}")
    return value


def build_readiness_command(
    root: Path,
    accepted_summary: dict[str, Any],
    output_path: Path,
    *,
    omitted_fields: set[str] | None = None,
    field_overrides: dict[str, str] | None = None,
) -> list[str]:
    omitted_fields = omitted_fields or set()
    field_overrides = field_overrides or {}
    command = [
        sys.executable,
        str(root / "tools/reference-harness/scripts/release_signoff_readiness.py"),
    ]
    for option, field in READINESS_INPUTS:
        if field in omitted_fields:
            continue
        if field in field_overrides:
            value = field_overrides[field]
        else:
            value = require_repo_path(root, accepted_summary.get(field), field)
        command.extend([option, value])
    command.extend(["--summary-path", str(output_path)])
    return command


def print_process_failure(case_name: str, completed: subprocess.CompletedProcess[str]) -> None:
    print(f"{case_name}: release_signoff_readiness.py exited nonzero", file=sys.stderr)
    if completed.stdout:
        print("stdout:", file=sys.stderr)
        print(completed.stdout, file=sys.stderr)
    if completed.stderr:
        print("stderr:", file=sys.stderr)
        print(completed.stderr, file=sys.stderr)


def summarize_failure(summary: dict[str, Any]) -> dict[str, Any]:
    return {
        "release_signoff_ready": summary.get("release_signoff_ready"),
        "release_signoff_blockers": summary.get("release_signoff_blockers"),
        "release_prerequisites": summary.get("release_prerequisites"),
        "review_sections": summary.get("review_sections"),
    }


def run_failure_case(
    root: Path,
    accepted_summary: dict[str, Any],
    case_name: str,
    output_path: Path,
    *,
    omitted_fields: set[str] | None = None,
    field_overrides: dict[str, str] | None = None,
) -> dict[str, Any]:
    command = build_readiness_command(
        root,
        accepted_summary,
        output_path,
        omitted_fields=omitted_fields,
        field_overrides=field_overrides,
    )
    completed = subprocess.run(
        command,
        cwd=root,
        text=True,
        capture_output=True,
        check=False,
    )
    if completed.returncode != 0:
        print_process_failure(case_name, completed)
        raise RuntimeError(f"{case_name}: readiness subprocess failed")
    return read_json(output_path)


def require_exact_blockers(
    case_name: str,
    summary: dict[str, Any],
    expected_blockers: list[str],
) -> None:
    if summary.get("release_signoff_ready") is not False:
        print(f"{case_name}: release_signoff_ready was not false", file=sys.stderr)
        print(json.dumps(summarize_failure(summary), indent=2, sort_keys=True), file=sys.stderr)
        raise RuntimeError(f"{case_name}: expected release_signoff_ready=false")

    blockers = summary.get("release_signoff_blockers")
    if blockers != expected_blockers:
        print(f"{case_name}: unexpected release_signoff_blockers", file=sys.stderr)
        print("expected:", json.dumps(expected_blockers, indent=2), file=sys.stderr)
        print("actual:", json.dumps(blockers, indent=2), file=sys.stderr)
        print(json.dumps(summarize_failure(summary), indent=2, sort_keys=True), file=sys.stderr)
        raise RuntimeError(f"{case_name}: release_signoff_blockers drifted")


def find_entry(entries: Any, entry_id: str, label: str) -> dict[str, Any]:
    expect(isinstance(entries, list), f"{label} must be a list")
    matches = [entry for entry in entries if isinstance(entry, dict) and entry.get("id") == entry_id]
    expect(len(matches) == 1, f"{label} must contain exactly one {entry_id} entry")
    return matches[0]


def assert_missing_m5_packet_failure(summary: dict[str, Any]) -> None:
    require_exact_blockers(
        "missing M5 packet",
        summary,
        ["prerequisite:phase-f-feature-parity:awaiting-reviewed-and-accepted-m5-packet"],
    )
    expect(summary.get("m5_qualification_evidence_present") is False, "missing M5 must be absent")
    expect(summary.get("m5_qualification_ready") is False, "missing M5 must not be ready")
    prerequisite = find_entry(
        summary.get("release_prerequisites"),
        "phase-f-feature-parity",
        "release_prerequisites",
    )
    expect(
        prerequisite.get("current_state") == "awaiting-reviewed-and-accepted-m5-packet",
        "missing M5 must keep phase-F prerequisite in the awaiting-M5 state",
    )
    expect(prerequisite.get("satisfied") is False, "missing M5 prerequisite must not be satisfied")
    expect(prerequisite.get("blockers") == [], "missing M5 prerequisite must not invent blockers")


def assert_missing_m6_packet_failure(summary: dict[str, Any]) -> None:
    require_exact_blockers(
        "missing M6 packet",
        summary,
        [
            "prerequisite:milestone-m6:case-study-numerics",
            "prerequisite:retained-reference-evidence:case-study-numerics",
            "review:parity-signoff:milestone-m6",
        ],
    )
    expect(summary.get("m5_qualification_ready") is True, "missing M6 must isolate M5")
    expect(summary.get("m6_qualification_evidence_present") is False, "missing M6 must be absent")
    expect(summary.get("m6_qualification_ready") is False, "missing M6 must not be ready")
    expect(
        summary.get("m6_qualification_blockers") == [],
        "missing M6 must not invent sidecar-local blockers",
    )

    milestone_m6 = find_entry(
        summary.get("release_prerequisites"),
        "milestone-m6",
        "release_prerequisites",
    )
    expect(
        milestone_m6.get("current_state") == "blocked-on-qualification-closure",
        "missing M6 must keep the M6 prerequisite blocked on qualification closure",
    )
    expect(
        milestone_m6.get("blockers") == ["case-study-numerics"],
        "missing M6 must preserve the derived case-study-numerics blocker",
    )
    expect(milestone_m6.get("satisfied") is False, "missing M6 prerequisite must not be satisfied")

    retained_reference = find_entry(
        summary.get("release_prerequisites"),
        "retained-reference-evidence",
        "release_prerequisites",
    )
    expect(
        retained_reference.get("current_state") == "captured-but-phase0-not-qualified",
        "missing M6 must keep retained references in the unqualified phase-0 state",
    )
    expect(
        retained_reference.get("blockers") == ["case-study-numerics"],
        "missing M6 must keep the retained-reference derived blocker visible",
    )
    expect(
        retained_reference.get("satisfied") is False,
        "missing M6 retained-reference prerequisite must not be satisfied",
    )

    parity_signoff = find_entry(summary.get("review_sections"), "parity-signoff", "review_sections")
    expect(
        parity_signoff.get("status") == "blocked"
        and parity_signoff.get("blockers") == ["milestone-m6"],
        "missing M6 must keep parity signoff blocked on milestone-m6",
    )


def stale_qualification_summary(root: Path, accepted_summary: dict[str, Any], path: Path) -> str:
    qualification_path = root / require_repo_path(
        root,
        accepted_summary.get("qualification_summary_path"),
        "qualification_summary_path",
    )
    payload = read_json(qualification_path)
    expect(
        payload.get("observed_reference_captured_matches_scaffold") is True,
        "accepted qualification fixture must begin as scaffold-matched",
    )
    payload["observed_reference_captured_matches_scaffold"] = False
    write_json(path, payload)
    return str(path)


def assert_stale_qualification_failure(summary: dict[str, Any]) -> None:
    require_exact_blockers(
        "stale qualification",
        summary,
        [
            "prerequisite:milestone-m6:qualification-evidence",
            "prerequisite:retained-reference-evidence:case-study-numerics",
            "review:parity-signoff:milestone-m6",
        ],
    )
    expect(
        summary.get("qualification_evidence_coherent") is False,
        "stale qualification must mark qualification evidence incoherent",
    )
    expect(summary.get("m5_qualification_ready") is True, "stale qualification must isolate M5")
    expect(summary.get("m6_qualification_ready") is False, "stale qualification must block M6")
    milestone_m6 = find_entry(
        summary.get("release_prerequisites"),
        "milestone-m6",
        "release_prerequisites",
    )
    expect(
        milestone_m6.get("current_state") == "blocked-on-qualification-closure",
        "stale qualification must block the M6 prerequisite",
    )
    expect(
        milestone_m6.get("blockers") == ["qualification-evidence"],
        "stale qualification must preserve the qualification-evidence blocker",
    )
    parity_signoff = find_entry(summary.get("review_sections"), "parity-signoff", "review_sections")
    expect(
        parity_signoff.get("status") == "blocked"
        and parity_signoff.get("blockers") == ["milestone-m6"],
        "stale qualification must keep parity signoff blocked on milestone-m6",
    )


def main() -> int:
    root = repo_root()
    accepted_summary = read_json(root / ACCEPTED_READINESS_SIDECAR)
    with tempfile.TemporaryDirectory(prefix="m7-release-readiness-failures-") as temp_dir:
        temp_root = Path(temp_dir)

        missing_m5_summary = run_failure_case(
            root,
            accepted_summary,
            "missing M5 packet",
            temp_root / "missing-m5-release-readiness.json",
            omitted_fields={"m5_qualification_summary_path"},
        )
        assert_missing_m5_packet_failure(missing_m5_summary)

        missing_m6_summary = run_failure_case(
            root,
            accepted_summary,
            "missing M6 packet",
            temp_root / "missing-m6-release-readiness.json",
            omitted_fields={"m6_qualification_summary_path"},
        )
        assert_missing_m6_packet_failure(missing_m6_summary)

        stale_qualification_path = stale_qualification_summary(
            root,
            accepted_summary,
            temp_root / "stale-qualification-readiness.json",
        )
        stale_qualification = run_failure_case(
            root,
            accepted_summary,
            "stale qualification",
            temp_root / "stale-qualification-release-readiness.json",
            field_overrides={"qualification_summary_path": stale_qualification_path},
        )
        assert_stale_qualification_failure(stale_qualification)

    print(
        "M7 release readiness failure-mode gate passed: "
        "missing M5, missing M6, and stale qualification block"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
