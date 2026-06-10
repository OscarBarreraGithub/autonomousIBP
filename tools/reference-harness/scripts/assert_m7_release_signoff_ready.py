#!/usr/bin/env python3
"""CTest gate for the accepted M7 release-readiness evidence packet."""

from __future__ import annotations

import difflib
import hashlib
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
        "--source-commit-override",
        require_source_commit(accepted_summary.get("source_commit")),
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


def require_source_commit(raw: Any) -> str:
    expect(isinstance(raw, str) and raw.strip(), "accepted source_commit must be a non-empty string")
    value = raw.strip()
    expect(
        7 <= len(value) <= 64 and all(character in "0123456789abcdefABCDEF" for character in value),
        f"accepted source_commit must be a git object id: {value}",
    )
    return value


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def print_byte_drift(label: str, expected: bytes, actual: bytes) -> None:
    print(f"M7 release readiness determinism regressed: {label} drifted", file=sys.stderr)
    print(
        f"expected bytes={len(expected)} sha256={sha256(expected)}",
        file=sys.stderr,
    )
    print(f"actual bytes={len(actual)} sha256={sha256(actual)}", file=sys.stderr)
    expected_text = expected.decode("utf-8", errors="replace").splitlines(keepends=True)
    actual_text = actual.decode("utf-8", errors="replace").splitlines(keepends=True)
    diff = difflib.unified_diff(
        expected_text,
        actual_text,
        fromfile="recorded-release-readiness.json",
        tofile=label,
        n=3,
    )
    print("".join(list(diff)[:120]), file=sys.stderr)


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
        accepted_bytes = accepted_summary_path.read_bytes()
        fresh_bytes = output_path.read_bytes()
        if fresh_bytes != accepted_bytes:
            print_byte_drift("fresh-summary-file", accepted_bytes, fresh_bytes)
            return 1
        stdout_bytes = completed.stdout.encode("utf-8")
        if stdout_bytes != accepted_bytes:
            print_byte_drift("fresh-stdout", accepted_bytes, stdout_bytes)
            return 1
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
