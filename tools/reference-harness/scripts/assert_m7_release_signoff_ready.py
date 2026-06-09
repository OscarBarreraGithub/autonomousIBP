#!/usr/bin/env python3
"""CTest gate for the accepted M7 release-readiness evidence packet."""

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


def build_readiness_command(root: Path, accepted_summary: dict[str, Any], output_path: Path) -> list[str]:
    command = [
        sys.executable,
        str(root / "tools/reference-harness/scripts/release_signoff_readiness.py"),
    ]
    for option, field in READINESS_INPUTS:
        command.extend([option, require_repo_path(root, accepted_summary.get(field), field)])
    command.extend(["--summary-path", str(output_path)])
    return command


def print_process_failure(completed: subprocess.CompletedProcess[str]) -> None:
    print("release_signoff_readiness.py failed during the M7 readiness gate", file=sys.stderr)
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


def main() -> int:
    root = repo_root()
    accepted_summary_path = root / ACCEPTED_READINESS_SIDECAR
    accepted_summary = read_json(accepted_summary_path)
    with tempfile.TemporaryDirectory(prefix="m7-release-readiness-") as temp_dir:
        output_path = Path(temp_dir) / "release-readiness.json"
        command = build_readiness_command(root, accepted_summary, output_path)
        completed = subprocess.run(
            command,
            cwd=root,
            text=True,
            capture_output=True,
            check=False,
        )
        if completed.returncode != 0:
            print_process_failure(completed)
            return completed.returncode
        fresh_summary = read_json(output_path)

    if fresh_summary.get("release_signoff_ready") is not True:
        print(
            "M7 release readiness regressed: fresh accepted-input run did not "
            "report release_signoff_ready=true",
            file=sys.stderr,
        )
        print(json.dumps(summarize_failure(fresh_summary), indent=2, sort_keys=True), file=sys.stderr)
        return 1

    blockers = fresh_summary.get("release_signoff_blockers")
    if blockers != []:
        print(
            "M7 release readiness regressed: release_signoff_blockers is not empty",
            file=sys.stderr,
        )
        print(json.dumps(summarize_failure(fresh_summary), indent=2, sort_keys=True), file=sys.stderr)
        return 1

    print("M7 release readiness accepted-input gate passed: release_signoff_ready=true")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
