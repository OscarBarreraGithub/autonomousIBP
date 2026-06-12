#!/usr/bin/env python3
"""Audit retained parity/status JSON sidecars against current committed state."""

from __future__ import annotations

import hashlib
import json
import re
import subprocess
import sys
from datetime import datetime
from pathlib import Path
from typing import Any

from assert_b61n_parity_status_json_fixture import (
    validate_summary_contract as validate_b61n_contract,
)
from assert_b63n_parity_status_json_fixture import (
    validate_summary_contract as validate_b63n_contract,
)


SPECS_ROOT = Path("tools/reference-harness/specs")
B61N_STATUS = Path("tools/reference-harness/specs/release/b61n-parity-status-summary.fixture.json")
B63N_STATUS = Path("tools/reference-harness/specs/release/b63n-parity-status-summary.fixture.json")
DIPHOTON_STATUS = Path(
    "tools/reference-harness/specs/case-studies/"
    "diphoton-heavy-quark-form-factors.lane102-final-status.json"
)
DIPHOTON_CURRENT_NUMERIC = Path(
    "tools/reference-harness/specs/case-studies/"
    "diphoton-heavy-quark-form-factors.numeric-evidence.json"
)
EXPECTED_STATUS_SIDECARS = (B61N_STATUS, B63N_STATUS, DIPHOTON_STATUS)
GIT_SHA_RE = re.compile(r"^[0-9a-f]{40}$")
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
MTIME_RE = re.compile(
    r"^(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2})(?:\.(\d{1,9}))? ([+-]\d{4})$"
)


class StatusSidecarAuditError(RuntimeError):
    """Raised when a retained status sidecar drifts."""


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise StatusSidecarAuditError(message)


def read_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as stream:
        payload = json.load(stream)
    expect(isinstance(payload, dict), f"{path} must contain a JSON object")
    return payload


def require_object(raw: Any, label: str) -> dict[str, Any]:
    expect(isinstance(raw, dict), f"{label} must be an object")
    return raw


def require_list(raw: Any, label: str) -> list[Any]:
    expect(isinstance(raw, list), f"{label} must be a list")
    return raw


def require_int(raw: Any, label: str) -> int:
    expect(isinstance(raw, int) and not isinstance(raw, bool), f"{label} must be an int")
    return raw


def require_bool(raw: Any, label: str) -> bool:
    expect(isinstance(raw, bool), f"{label} must be a bool")
    return raw


def require_str(raw: Any, label: str) -> str:
    expect(isinstance(raw, str) and raw, f"{label} must be a non-empty string")
    return raw


def require_git_sha(raw: Any, label: str) -> str:
    value = require_str(raw, label)
    expect(GIT_SHA_RE.fullmatch(value) is not None, f"{label} must be a full lowercase git SHA")
    return value


def require_sha256(raw: Any, label: str) -> str:
    value = require_str(raw, label)
    expect(SHA256_RE.fullmatch(value) is not None, f"{label} must be a lowercase sha256")
    return value


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def parse_iso_datetime(raw: Any, label: str) -> datetime:
    value = require_str(raw, label)
    try:
        return datetime.fromisoformat(value.replace("Z", "+00:00"))
    except ValueError as exc:
        raise StatusSidecarAuditError(f"{label} must be an ISO-8601 datetime: {value}") from exc


def parse_stat_mtime(raw: Any, label: str) -> datetime:
    value = require_str(raw, label)
    match = MTIME_RE.fullmatch(value)
    expect(match is not None, f"{label} must be a stat-style timestamp")
    base, fraction, offset = match.groups()
    fraction = (fraction or "0")[:6].ljust(6, "0")
    offset = f"{offset[:3]}:{offset[3:]}"
    return datetime.fromisoformat(f"{base}.{fraction}{offset}")


def git_commit_exists(root: Path, commit: str, label: str) -> None:
    completed = subprocess.run(
        ["git", "cat-file", "-e", f"{commit}^{{commit}}"],
        cwd=root,
        text=True,
        capture_output=True,
        check=False,
    )
    expect(completed.returncode == 0, f"{label} does not resolve as a commit: {commit}")
    ancestor = subprocess.run(
        ["git", "merge-base", "--is-ancestor", commit, "HEAD"],
        cwd=root,
        text=True,
        capture_output=True,
        check=False,
    )
    expect(ancestor.returncode == 0, f"{label} is not an ancestor of current HEAD: {commit}")


def require_repo_path(root: Path, raw: Any, label: str) -> Path:
    value = require_str(raw, label)
    path = Path(value)
    expect(not path.is_absolute(), f"{label} must be repository-relative: {value}")
    target = root / path
    expect(target.exists(), f"{label} referenced path is missing: {value}")
    return target


def require_external_path(raw: Any, label: str) -> Path:
    value = require_str(raw, label)
    path = Path(value)
    expect(path.is_absolute(), f"{label} must be an absolute retained-output path: {value}")
    expect(path.exists(), f"{label} referenced external path is missing: {value}")
    return path


def verify_status_discovery(root: Path) -> None:
    discovered = tuple(
        sorted(
            path.relative_to(root)
            for path in (root / SPECS_ROOT).rglob("*.json")
            if "parity-status" in path.name or path.name.endswith("-status.json")
        )
    )
    expected = tuple(sorted(EXPECTED_STATUS_SIDECARS))
    expect(
        discovered == expected,
        "status sidecar discovery drifted: expected "
        + ", ".join(path.as_posix() for path in expected)
        + "; discovered "
        + ", ".join(path.as_posix() for path in discovered),
    )


def verify_evidence_sources(root: Path, payload: dict[str, Any], label: str) -> None:
    sources = require_object(payload.get("evidence_sources"), f"{label}.evidence_sources")
    expect(sources, f"{label}.evidence_sources must not be empty")
    for key, raw_path in sources.items():
        target = require_repo_path(root, raw_path, f"{label}.evidence_sources.{key}")
        expect(target.suffix == ".json", f"{label}.evidence_sources.{key} must point to JSON evidence")


def verify_withheld_claims(payload: dict[str, Any], label: str) -> None:
    claims = require_list(payload.get("withheld_claims"), f"{label}.withheld_claims")
    expect(claims, f"{label}.withheld_claims must not be empty")
    expect(
        all(isinstance(claim, str) and claim for claim in claims),
        f"{label}.withheld_claims entries must be non-empty strings",
    )


def run_fixture_gate(root: Path, script_name: str) -> None:
    script = root / "tools" / "reference-harness" / "scripts" / script_name
    completed = subprocess.run(
        [sys.executable, str(script)],
        cwd=root,
        text=True,
        capture_output=True,
        check=False,
    )
    if completed.returncode != 0:
        detail = (completed.stdout + completed.stderr).strip()
        raise StatusSidecarAuditError(f"{script_name} failed during status audit: {detail}")


def verify_b61n(root: Path) -> None:
    payload = read_json(root / B61N_STATUS)
    validate_b61n_contract(payload, label=B61N_STATUS.as_posix())
    run_fixture_gate(root, "assert_b61n_parity_status_json_fixture.py")
    verify_evidence_sources(root, payload, B61N_STATUS.as_posix())
    verify_withheld_claims(payload, B61N_STATUS.as_posix())

    reference_floor = require_object(payload.get("reference_floor"), "b61n.reference_floor")
    targets = require_list(reference_floor.get("targets"), "b61n.reference_floor.targets")
    observed_digits = [
        require_int(target.get(field), f"b61n.reference_floor.targets[{index}].{field}")
        for index, target in enumerate(targets)
        if isinstance(target, dict)
        for field in ("reference_floor_real_digits", "reference_floor_imag_digits")
    ]
    expect(observed_digits, "b61n reference-floor targets must record digit counts")
    minimum_digits = min(observed_digits)
    expect(
        reference_floor.get("minimum_digit_agreement") == minimum_digits,
        "b61n reference_floor.minimum_digit_agreement drifted from target minima",
    )

    precision = require_object(payload.get("precision_uplift"), "b61n.precision_uplift")
    expect(
        precision.get("target_count") == len(targets),
        "b61n precision_uplift.target_count drifted from reference-floor targets",
    )
    expect(
        precision.get("component_count") == len(targets) * 2,
        "b61n precision_uplift.component_count drifted from real/imag target components",
    )
    expect(
        precision.get("minimum_reference_floor_digits") == minimum_digits,
        "b61n precision_uplift.minimum_reference_floor_digits drifted",
    )

    publication = require_object(payload.get("publication_gate"), "b61n.publication_gate")
    required = require_int(
        publication.get("minimum_digit_agreement_required"),
        "b61n.publication_gate.minimum_digit_agreement_required",
    )
    observed = require_int(
        publication.get("minimum_digit_agreement_observed"),
        "b61n.publication_gate.minimum_digit_agreement_observed",
    )
    expect(observed == minimum_digits, "b61n publication observed digit floor drifted")
    expect(observed < required, "b61n publication gate should remain below the required floor")
    expect(require_bool(publication.get("gate_passed"), "b61n.publication_gate.gate_passed") is False,
           "b61n publication gate must remain blocked")
    expect(
        publication.get("blocked_variant_count") == publication.get("variant_count"),
        "b61n publication gate blocked count drifted from variant count",
    )

    audit = require_object(payload.get("audit_fingerprints"), "b61n.audit_fingerprints")
    entries = require_list(audit.get("entries"), "b61n.audit_fingerprints.entries")
    expect(audit.get("entry_count") == len(entries), "b61n audit fingerprint count drifted")


def verify_b63n(root: Path) -> None:
    payload = read_json(root / B63N_STATUS)
    validate_b63n_contract(payload, label=B63N_STATUS.as_posix())
    run_fixture_gate(root, "assert_b63n_parity_status_json_fixture.py")
    verify_evidence_sources(root, payload, B63N_STATUS.as_posix())
    verify_withheld_claims(payload, B63N_STATUS.as_posix())

    first = require_object(payload.get("first_coefficient"), "b63n.first_coefficient")
    first_orders = require_list(first.get("orders"), "b63n.first_coefficient.orders")
    expect(
        first.get("compared_coefficient_count") == len(first_orders),
        "b63n first coefficient count drifted from orders",
    )
    expect(
        require_bool(first.get("full_eta_zero_contour_applied"), "b63n.first.full_eta_zero_contour_applied")
        is False,
        "b63n first coefficient must not claim full eta=0 contour execution",
    )

    selected4 = require_object(payload.get("selected4_parity"), "b63n.selected4_parity")
    transported = require_list(
        selected4.get("transported_integrals"),
        "b63n.selected4_parity.transported_integrals",
    )
    expect(
        selected4.get("transported_integral_count") == len(transported),
        "b63n selected4 transported count drifted",
    )
    expect(
        selected4.get("published_d7_integral") in transported,
        "b63n selected4 published D7 integral is not in transported set",
    )
    expect(
        require_bool(selected4.get("full_eta_zero_contour_applied"), "b63n.selected4.full_eta_zero_contour_applied")
        is False,
        "b63n selected4 must not claim full eta=0 contour execution",
    )

    d246 = require_object(
        payload.get("d246_weighted_residue_surface"),
        "b63n.d246_weighted_residue_surface",
    )
    expect(require_bool(d246.get("published_evidence"), "b63n.d246.published_evidence") is False,
           "b63n D246 must remain unpublished")
    expect(require_bool(d246.get("skeleton_evidence"), "b63n.d246.skeleton_evidence") is True,
           "b63n D246 must remain a skeleton sidecar")
    expect(require_list(d246.get("publication_blockers"), "b63n.d246.publication_blockers"),
           "b63n D246 publication blockers must not be empty")

    scoped = require_object(payload.get("scoped_gate_audit"), "b63n.scoped_gate_audit")
    expect(scoped.get("runtime_checked") is False, "b63n scoped gate runtime check must remain false")
    expect(scoped.get("entry_count") is None, "b63n scoped gate entry count must remain null without runtime")
    expect(scoped.get("passed") is None, "b63n scoped gate passed field must remain null without runtime")


def verify_diphoton(root: Path) -> None:
    payload = read_json(root / DIPHOTON_STATUS)
    current = read_json(root / DIPHOTON_CURRENT_NUMERIC)
    label = DIPHOTON_STATUS.as_posix()

    expect(payload.get("schema_version") == 1, f"{label}.schema_version must be 1")
    expect(
        payload.get("case_study_id") == "diphoton-heavy-quark-form-factors",
        f"{label}.case_study_id drifted",
    )
    expect(payload.get("lane") == "lane102", f"{label}.lane must remain lane102")
    expect(
        payload.get("status") == "dedicated-amflow-output-produced-but-verifier-failed",
        f"{label}.status drifted",
    )
    expect(payload.get("comparison_passed") is False, f"{label}.comparison_passed must remain false")
    expect(payload.get("minimum_observed_correct_digits") is None, f"{label}.minimum digits must remain null")
    verify_withheld_claims(payload, label)

    assessed_commit = require_git_sha(payload.get("repo_head_at_assessment"), f"{label}.repo_head_at_assessment")
    git_commit_exists(root, assessed_commit, f"{label}.repo_head_at_assessment")

    superseded_by = require_object(payload.get("superseded_by"), f"{label}.superseded_by")
    expect(superseded_by.get("lane") == "lane103", f"{label}.superseded_by.lane must be lane103")
    superseded_path = require_repo_path(root, superseded_by.get("path"), f"{label}.superseded_by.path")
    expect(
        superseded_path == root / DIPHOTON_CURRENT_NUMERIC,
        f"{label}.superseded_by.path must point to current diphoton numeric evidence",
    )
    superseding_commit = require_git_sha(superseded_by.get("commit"), f"{label}.superseded_by.commit")
    git_commit_exists(root, superseding_commit, f"{label}.superseded_by.commit")
    expect(
        current.get("comparison_passed") is True
        and current.get("minimum_observed_correct_digits") == 200
        and current.get("evidence_kind") == "dedicated-amflow-target-verifier",
        "current diphoton numeric evidence no longer supersedes the lane102 failure as expected",
    )
    coverage = require_object(current.get("coverage_summary"), "diphoton.current.coverage_summary")
    expect(
        coverage.get("passing_fraction_after_this_lane") == "10/10",
        "current diphoton numeric evidence coverage fraction drifted",
    )
    manifest_path = require_repo_path(
        root,
        current.get("amflow_golden_manifest_path"),
        "diphoton.current.amflow_golden_manifest_path",
    )
    manifest = read_json(manifest_path)
    manifest_outputs = {
        require_str(entry.get("name"), "diphoton.manifest.outputs[].name"): require_object(
            entry,
            "diphoton.manifest.outputs[]",
        )
        for entry in require_list(manifest.get("outputs"), "diphoton.manifest.outputs")
        if isinstance(entry, dict)
    }
    current_verify_output = require_object(
        manifest_outputs.get("sol-diphoton-npl-j39-j42.verify.txt"),
        "diphoton.manifest verify output",
    )
    current_verify_sha = require_sha256(
        current_verify_output.get("canonical_sha256"),
        "diphoton.manifest.verify_output.canonical_sha256",
    )
    current_verify_size = require_int(
        current_verify_output.get("size_bytes"),
        "diphoton.manifest.verify_output.size_bytes",
    )
    verifier_rerun = require_object(
        require_object(manifest.get("verification"), "diphoton.manifest.verification").get("verifier_rerun"),
        "diphoton.manifest.verification.verifier_rerun",
    )
    expect(
        verifier_rerun.get("verify_output_sha256") == current_verify_sha,
        "diphoton manifest verifier-rerun hash drifted from output pin",
    )

    captured_at = parse_iso_datetime(payload.get("captured_at"), f"{label}.captured_at")
    scheduler = require_object(payload.get("scheduler"), f"{label}.scheduler")
    start = parse_iso_datetime(scheduler.get("start"), f"{label}.scheduler.start")
    end = parse_iso_datetime(scheduler.get("end"), f"{label}.scheduler.end")
    if captured_at.tzinfo is not None:
        start = start.replace(tzinfo=captured_at.tzinfo)
        end = end.replace(tzinfo=captured_at.tzinfo)
    expect(start <= end <= captured_at, f"{label} scheduler/capture timestamps are inconsistent")
    expect(scheduler.get("job_id") == "10204166", f"{label}.scheduler.job_id drifted")
    expect(scheduler.get("sacct_state") == "FAILED", f"{label}.scheduler.sacct_state drifted")
    expect(scheduler.get("exit_code") == "4:0", f"{label}.scheduler.exit_code drifted")

    paths = require_object(payload.get("paths"), f"{label}.paths")
    for key, raw_path in paths.items():
        if key == "metadata":
            continue
        require_external_path(raw_path, f"{label}.paths.{key}")
    require_external_path(paths.get("metadata"), f"{label}.paths.metadata")

    output_files = require_object(payload.get("output_files"), f"{label}.output_files")
    for key in ("solution", "metadata", "verify_output"):
        info = require_object(output_files.get(key), f"{label}.output_files.{key}")
        expect(require_bool(info.get("present"), f"{label}.output_files.{key}.present") is True,
               f"{label}.output_files.{key}.present must remain true")
        path = require_external_path(paths.get(key), f"{label}.paths.{key}")
        recorded_size = require_int(info.get("size_bytes"), f"{label}.output_files.{key}.size_bytes")
        parse_stat_mtime(info.get("mtime"), f"{label}.output_files.{key}.mtime")
        pinned = require_sha256(info.get("sha256"), f"{label}.output_files.{key}.sha256")
        actual_sha = sha256_file(path)
        if key == "verify_output":
            expect(
                actual_sha == current_verify_sha and path.stat().st_size == current_verify_size,
                f"{label}.output_files.verify_output path must resolve to the lane103 superseding verifier output",
            )
            expect(
                pinned != actual_sha and recorded_size != path.stat().st_size,
                f"{label}.output_files.verify_output should remain the historical lane102 failed-output pin",
            )
        else:
            expect(
                recorded_size == path.stat().st_size,
                f"{label}.output_files.{key}.size_bytes drifted from retained artifact",
            )
            expect(actual_sha == pinned, f"{label}.output_files.{key}.sha256 drifted")

    stdout = require_object(payload.get("amflow_stdout_summary"), f"{label}.amflow_stdout_summary")
    expect(stdout.get("kernel_exit") == 0, f"{label}.amflow_stdout_summary.kernel_exit drifted")
    expect(
        require_int(stdout.get("black_box_amflow_finished_seconds"), f"{label}.black_box_amflow_finished_seconds")
        <= require_int(stdout.get("solve_integrals_finished_seconds"), f"{label}.solve_integrals_finished_seconds"),
        f"{label} AMFlow stdout timing order drifted",
    )

    verifier = require_object(payload.get("verifier"), f"{label}.verifier")
    requested_targets = require_list(verifier.get("requested_targets"), f"{label}.verifier.requested_targets")
    expect(len(requested_targets) == 4, f"{label}.verifier requested target count drifted")
    expect(verifier.get("exit_code") == 4, f"{label}.verifier.exit_code drifted")
    expect(
        require_int(verifier.get("reported_solution_rule_count"), f"{label}.verifier.reported_solution_rule_count")
        >= len(requested_targets),
        f"{label}.verifier solution rule count is less than requested targets",
    )
    expect(
        verifier.get("failure") == "VERIFY FAIL: expected J39-J42 target count",
        f"{label}.verifier.failure drifted",
    )


def main() -> int:
    root = repo_root()
    try:
        verify_status_discovery(root)
        verify_b61n(root)
        verify_b63n(root)
        verify_diphoton(root)
        print("parity/status sidecar audit passed: 3 status JSON sidecars verified")
        return 0
    except StatusSidecarAuditError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
