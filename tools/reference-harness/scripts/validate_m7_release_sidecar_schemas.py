#!/usr/bin/env python3
"""Validate committed M7 release sidecar JSON shapes."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Any, Callable

from release_signoff_readiness import (
    load_case_study_qualification_summary,
    load_diagnostic_review_summary,
    load_docs_completion_summary,
    load_m6_qualification_summary,
    load_parity_signoff_summary,
    load_performance_review_summary,
    load_phase0_qualification_summary,
    load_qualification_corpus_review_summary,
    load_qualification_summary,
)


M7_ROOT = Path("tools/reference-harness/specs/m7")
ACCEPTED_READINESS_SIDECAR = Path(
    "tools/reference-harness/specs/m7/lane3/"
    "release-readiness.m5-accepted.full-output.json"
)
SOURCE_COMMIT_PATTERN = re.compile(r"^[0-9a-f]{40}$")
SOURCE_PROVENANCE_SHA256_PATTERN = re.compile(r"^[0-9a-f]{64}$")
PROVENANCE_DIGEST_FIELDS = frozenset(
    ("source_payload_sha256", "source_provenance_sha256")
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

STRICT_SCOPE_VALIDATORS: dict[str, Callable[[Path], dict[str, Any]]] = {
    "case-study-families-only": load_case_study_qualification_summary,
    "milestone-m6-qualification": load_m6_qualification_summary,
    "phase0-packet-set-only": load_phase0_qualification_summary,
    "release-diagnostic-review": load_diagnostic_review_summary,
    "release-docs-completion": load_docs_completion_summary,
    "release-parity-signoff": load_parity_signoff_summary,
    "release-performance-review": load_performance_review_summary,
    "release-qualification-corpus": load_qualification_corpus_review_summary,
}


class SchemaError(RuntimeError):
    """Raised when a committed sidecar no longer matches its expected shape."""


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise SchemaError(message)


def require_m7_root(root: Path, raw: Path) -> Path:
    value = str(raw)
    expect(value.strip(), "m7 root must not be empty")
    expect(value == value.strip(), "m7 root must not carry surrounding whitespace")
    expect(not raw.is_absolute(), f"m7 root must be repository-relative: {value}")
    expect(".." not in raw.parts, f"m7 root must not contain '..': {value}")

    candidate = (root / raw).resolve(strict=False)
    resolved_root = root.resolve(strict=True)
    try:
        relative = candidate.relative_to(resolved_root)
    except ValueError as error:
        raise SchemaError(f"m7 root must stay within the repository: {value}") from error
    expect(candidate.is_dir(), f"m7 root does not exist as a directory: {value}")
    return relative


def read_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as stream:
        payload = json.load(stream)
    expect(isinstance(payload, dict), f"{path} must contain a JSON object")
    return payload


def payload_without_source_provenance_digest(payload: dict[str, Any]) -> dict[str, Any]:
    return {
        key: value
        for key, value in payload.items()
        if key not in PROVENANCE_DIGEST_FIELDS
    }


def source_provenance_sha256(payload: dict[str, Any]) -> str:
    encoded = json.dumps(
        payload_without_source_provenance_digest(payload),
        ensure_ascii=False,
        separators=(",", ":"),
        sort_keys=True,
    ).encode("utf-8")
    return hashlib.sha256(encoded).hexdigest()


def require_git_commit(root: Path, commit: str, label: str) -> None:
    completed = subprocess.run(
        ["git", "cat-file", "-e", f"{commit}^{{commit}}"],
        cwd=root,
        text=True,
        capture_output=True,
        check=False,
    )
    expect(completed.returncode == 0, f"{label} source_commit is not a known commit")


def validate_source_provenance(path: Path, root: Path, payload: dict[str, Any]) -> None:
    label = str(path.relative_to(root))
    commit = require_exact_non_empty_str(payload, "source_commit", label)
    expect(
        SOURCE_COMMIT_PATTERN.fullmatch(commit) is not None,
        f"{label} source_commit must be a full lowercase 40-character git SHA",
    )
    require_git_commit(root, commit, label)

    digest = require_exact_non_empty_str(payload, "source_provenance_sha256", label)
    expect(
        SOURCE_PROVENANCE_SHA256_PATTERN.fullmatch(digest) is not None,
        f"{label} source_provenance_sha256 must be a lowercase SHA-256 digest",
    )
    expected_digest = source_provenance_sha256(payload)
    expect(
        digest == expected_digest,
        f"{label} source provenance drifted: expected {expected_digest}, got {digest}",
    )


def require_schema_version(payload: dict[str, Any], label: str) -> None:
    value = payload.get("schema_version")
    expect(type(value) is int and value == 1, f"{label} schema_version must be integer 1")


def require_field(
    payload: dict[str, Any],
    field: str,
    expected_type: type | tuple[type, ...],
    label: str,
) -> Any:
    expect(field in payload, f"{label} missing required field {field}")
    value = payload[field]
    if expected_type is bool:
        expect(type(value) is bool, f"{label} {field} must be a bool")
    elif expected_type is int:
        expect(type(value) is int, f"{label} {field} must be an int")
    elif expected_type is float:
        expect(
            type(value) in {int, float} and type(value) is not bool,
            f"{label} {field} must be numeric",
        )
    else:
        expect(isinstance(value, expected_type), f"{label} {field} has unexpected type")
    return value


def require_non_empty_str(payload: dict[str, Any], field: str, label: str) -> str:
    value = require_field(payload, field, str, label)
    expect(value.strip(), f"{label} {field} must not be empty")
    return value.strip()


def require_exact_non_empty_str(payload: dict[str, Any], field: str, label: str) -> str:
    value = require_field(payload, field, str, label)
    expect(value.strip(), f"{label} {field} must not be empty")
    expect(value == value.strip(), f"{label} {field} must not carry surrounding whitespace")
    return value


def require_bool_fields(payload: dict[str, Any], fields: tuple[str, ...], label: str) -> None:
    for field in fields:
        require_field(payload, field, bool, label)


def require_nonnegative_int(payload: dict[str, Any], field: str, label: str) -> int:
    value = require_field(payload, field, int, label)
    expect(value >= 0, f"{label} {field} must be nonnegative")
    return value


def require_string_list(payload: dict[str, Any], field: str, label: str) -> list[str]:
    raw = require_field(payload, field, list, label)
    values: list[str] = []
    for index, item in enumerate(raw):
        expect(
            isinstance(item, str) and item.strip(),
            f"{label} {field}[{index}] must be a non-empty string",
        )
        values.append(item.strip())
    return values


def require_object_list(payload: dict[str, Any], field: str, label: str) -> list[dict[str, Any]]:
    raw = require_field(payload, field, list, label)
    values: list[dict[str, Any]] = []
    for index, item in enumerate(raw):
        expect(isinstance(item, dict), f"{label} {field}[{index}] must be an object")
        values.append(item)
    return values


def require_object(payload: dict[str, Any], field: str, label: str) -> dict[str, Any]:
    return require_field(payload, field, dict, label)


def require_unique(values: list[str], label: str) -> None:
    expect(len(values) == len(set(values)), f"{label} must not contain duplicates")


def require_repo_path(root: Path, raw: Any, field: str) -> str:
    expect(isinstance(raw, str) and raw.strip(), f"{field} must be a non-empty string")
    value = raw.strip()
    path = Path(value)
    if not path.is_absolute():
        path = root / value
    try:
        path.resolve(strict=False).relative_to(root.resolve(strict=False))
    except ValueError as error:
        raise SchemaError(f"{field} must stay within the repository: {value}") from error
    expect(path.exists(), f"{field} does not exist: {value}")
    return value


def validate_runtime_lane_entries(entries: list[dict[str, Any]], label: str) -> None:
    ids: list[str] = []
    for index, entry in enumerate(entries):
        entry_label = f"{label}[{index}]"
        ids.append(require_non_empty_str(entry, "id", entry_label))
        require_field(entry, "next_runtime_lane", str, entry_label)
    require_unique(ids, label)


def validate_release_prerequisites(entries: list[dict[str, Any]], label: str) -> None:
    ids: list[str] = []
    for index, entry in enumerate(entries):
        entry_label = f"{label}[{index}]"
        ids.append(require_non_empty_str(entry, "id", entry_label))
        require_non_empty_str(entry, "current_state", entry_label)
        require_non_empty_str(entry, "required_state", entry_label)
        require_field(entry, "satisfied", bool, entry_label)
        require_string_list(entry, "blockers", entry_label)
        require_non_empty_str(entry, "notes", entry_label)
    require_unique(ids, label)


def validate_review_sections(entries: list[dict[str, Any]], label: str) -> None:
    ids: list[str] = []
    for index, entry in enumerate(entries):
        entry_label = f"{label}[{index}]"
        ids.append(require_non_empty_str(entry, "id", entry_label))
        require_non_empty_str(entry, "status", entry_label)
        require_string_list(entry, "required_inputs", entry_label)
        require_string_list(entry, "required_outputs", entry_label)
        require_string_list(entry, "blockers", entry_label)
        require_non_empty_str(entry, "notes", entry_label)
    require_unique(ids, label)


def validate_repo_path_records(
    entries: list[dict[str, Any]],
    label: str,
    *,
    id_field: str | None = None,
) -> None:
    ids: list[str] = []
    for index, entry in enumerate(entries):
        entry_label = f"{label}[{index}]"
        if id_field is not None:
            ids.append(require_non_empty_str(entry, id_field, entry_label))
        require_non_empty_str(entry, "path", entry_label)
        require_field(entry, "exists", bool, entry_label)
        require_field(entry, "within_repo", bool, entry_label)
        if "expectation" in entry:
            require_non_empty_str(entry, "expectation", entry_label)
    if id_field is not None:
        require_unique(ids, label)


def validate_release_readiness_output(path: Path, payload: dict[str, Any]) -> str:
    label = str(path)
    require_schema_version(payload, label)
    require_bool_fields(
        payload,
        (
            "release_signoff_ready",
            "qualification_evidence_coherent",
            "checklist_sources_present",
            "checklist_sources_within_repo",
            "docs_completion_targets_present",
            "docs_completion_targets_within_repo",
        ),
        label,
    )
    for field in (
        "checklist_path",
        "qualification_summary_path",
        "qualification_corpus_summary_path",
        "phase0_qualification_summary_path",
        "case_study_qualification_summary_path",
        "performance_review_summary_path",
        "diagnostic_review_summary_path",
        "docs_completion_summary_path",
        "parity_signoff_summary_path",
    ):
        require_non_empty_str(payload, field, label)

    if "release_signoff_blockers" in payload:
        release_signoff_blockers = require_string_list(payload, "release_signoff_blockers", label)
    else:
        release_signoff_blockers = []
    require_string_list(payload, "withheld_claims", label)
    require_string_list(payload, "blocked_runtime_lanes", label)
    validate_runtime_lane_entries(
        require_object_list(payload, "blocked_phase0_examples", label),
        f"{label} blocked_phase0_examples",
    )
    validate_runtime_lane_entries(
        require_object_list(payload, "blocked_case_study_families", label),
        f"{label} blocked_case_study_families",
    )
    validate_release_prerequisites(
        require_object_list(payload, "release_prerequisites", label),
        f"{label} release_prerequisites",
    )
    validate_review_sections(
        require_object_list(payload, "review_sections", label),
        f"{label} review_sections",
    )
    validate_repo_path_records(
        require_object_list(payload, "checklist_sources", label),
        f"{label} checklist_sources",
        id_field="id",
    )
    validate_repo_path_records(
        require_object_list(payload, "docs_completion_targets", label),
        f"{label} docs_completion_targets",
    )
    if payload["release_signoff_ready"]:
        expect(
            "release_signoff_blockers" in payload and release_signoff_blockers == [],
            f"{label} ready output must have no release_signoff_blockers",
        )
    else:
        expect(
            bool(release_signoff_blockers)
            or any(prerequisite["blockers"] for prerequisite in payload["release_prerequisites"])
            or any(section["blockers"] for section in payload["review_sections"]),
            f"{label} blocked output must explain blockers",
        )
    return "release-readiness-output"


def validate_phase0_packet_comparison(path: Path, payload: dict[str, Any]) -> str:
    label = str(path)
    require_schema_version(payload, label)
    require_bool_fields(
        payload,
        (
            "candidate_output_hashes_match_reference",
            "candidate_output_names_match_reference",
            "candidate_packet_benchmark_sets_match_reference",
            "candidate_primary_run_manifests_exist",
            "candidate_result_manifests_exist",
            "compared_phase0_ids_match_scaffold_reference_captured",
            "digit_threshold_profiles_reported",
            "reference_benchmarks_pass_retained_capture_checks",
            "reference_packet_labels_match_scaffold_reference_captured",
            "regression_profiles_reported",
            "required_failure_code_profiles_reported",
            "required_packet_present",
        ),
        label,
    )
    require_nonnegative_int(payload, "packet_pair_count", label)
    for field in (
        "compared_phase0_ids",
        "expected_reference_captured_phase0_ids",
        "expected_reference_packet_labels",
        "reference_packet_labels",
    ):
        require_unique(require_string_list(payload, field, label), f"{label} {field}")
    benchmark_ids: list[str] = []
    for index, benchmark in enumerate(require_object_list(payload, "benchmarks", label)):
        entry_label = f"{label} benchmarks[{index}]"
        benchmark_ids.append(require_non_empty_str(benchmark, "benchmark_id", entry_label))
        for field in (
            "current_evidence_state",
            "digit_threshold_profile",
            "failure_code_profile",
            "regression_profile",
        ):
            require_non_empty_str(benchmark, field, entry_label)
    require_unique(benchmark_ids, f"{label} benchmark ids")
    for index, packet in enumerate(require_object_list(payload, "packet_comparisons", label)):
        entry_label = f"{label} packet_comparisons[{index}]"
        require_non_empty_str(packet, "candidate_root", entry_label)
        require_non_empty_str(packet, "reference_packet_label", entry_label)
        require_string_list(packet, "candidate_benchmark_ids", entry_label)
    return "phase0-packet-comparison"


def validate_phase0_correct_digits(path: Path, payload: dict[str, Any]) -> str:
    label = str(path)
    require_schema_version(payload, label)
    require_bool_fields(
        payload,
        (
            "all_compared_benchmarks_meet_digit_thresholds",
            "candidate_numeric_literal_skeletons_match_reference",
            "candidate_output_names_match_reference",
            "candidate_packet_benchmark_sets_match_reference",
            "candidate_primary_run_manifests_exist",
            "candidate_result_manifests_exist",
            "compared_phase0_ids_match_scaffold_reference_captured",
            "digit_threshold_profiles_reported",
            "reference_benchmarks_pass_retained_capture_checks",
            "reference_packet_labels_match_scaffold_reference_captured",
            "regression_profiles_reported",
            "required_failure_code_profiles_reported",
            "required_packet_present",
        ),
        label,
    )
    require_nonnegative_int(payload, "minimum_observed_correct_digits_across_packet_set", label)
    require_nonnegative_int(payload, "packet_pair_count", label)
    for field in (
        "compared_phase0_ids",
        "expected_reference_captured_phase0_ids",
        "expected_reference_packet_labels",
        "reference_packet_labels",
    ):
        require_unique(require_string_list(payload, field, label), f"{label} {field}")
    for index, benchmark in enumerate(require_object_list(payload, "benchmarks", label)):
        entry_label = f"{label} benchmarks[{index}]"
        require_non_empty_str(benchmark, "benchmark_id", entry_label)
        require_field(benchmark, "all_numeric_outputs_meet_threshold", bool, entry_label)
        require_nonnegative_int(benchmark, "minimum_correct_digits", entry_label)
    for index, score in enumerate(require_object_list(payload, "packet_scores", label)):
        entry_label = f"{label} packet_scores[{index}]"
        require_non_empty_str(score, "candidate_root", entry_label)
        require_string_list(score, "candidate_benchmark_ids", entry_label)
        require_nonnegative_int(score, "minimum_observed_correct_digits", entry_label)
    return "phase0-correct-digits"


def validate_phase0_failure_codes(path: Path, payload: dict[str, Any]) -> str:
    label = str(path)
    require_schema_version(payload, label)
    require_bool_fields(
        payload,
        (
            "all_compared_benchmarks_publish_failure_code_audits",
            "all_compared_benchmarks_report_required_failure_codes",
            "any_compared_benchmarks_report_unexpected_failure_codes",
            "audited_phase0_ids_match_scaffold_reference_captured",
            "candidate_packet_benchmark_sets_match_packet_summaries",
            "candidate_packet_labels_match_scaffold_reference_captured",
            "digit_threshold_profiles_reported",
            "regression_profiles_reported",
            "required_failure_code_profiles_reported",
            "required_packet_present",
        ),
        label,
    )
    require_nonnegative_int(payload, "candidate_packet_count", label)
    for field in (
        "audited_phase0_ids",
        "candidate_packet_labels",
        "expected_reference_captured_packet_labels",
        "expected_reference_captured_phase0_ids",
        "missing_required_failure_codes_across_packet_set",
    ):
        require_unique(require_string_list(payload, field, label), f"{label} {field}")
    for index, benchmark in enumerate(require_object_list(payload, "benchmarks", label)):
        entry_label = f"{label} benchmarks[{index}]"
        require_non_empty_str(benchmark, "benchmark_id", entry_label)
        require_non_empty_str(benchmark, "failure_code_profile", entry_label)
        require_string_list(benchmark, "required_failure_codes", entry_label)
        require_string_list(benchmark, "observed_failure_codes", entry_label)
        require_string_list(benchmark, "missing_required_failure_codes", entry_label)
    for index, audit in enumerate(require_object_list(payload, "packet_audits", label)):
        entry_label = f"{label} packet_audits[{index}]"
        require_non_empty_str(audit, "candidate_packet_label", entry_label)
        require_string_list(audit, "candidate_benchmark_ids", entry_label)
        require_string_list(audit, "missing_required_failure_codes_across_selection", entry_label)
    return "phase0-failure-codes"


def validate_failure_code_audit(path: Path, payload: dict[str, Any]) -> str:
    label = str(path)
    require_schema_version(payload, label)
    for field in (
        "audit_kind",
        "benchmark_id",
        "failure_code_profile",
        "published_packet_sidecar_path",
    ):
        require_non_empty_str(payload, field, label)
    for field in (
        "observed_failure_codes",
        "qualifier_observed_failure_codes",
        "required_failure_codes",
        "withheld_claims",
    ):
        require_unique(require_string_list(payload, field, label), f"{label} {field}")
    real_output_audit = require_object(payload, "real_output_audit", label)
    require_field(real_output_audit, "comparison_passed", bool, f"{label} real_output_audit")
    require_nonnegative_int(
        real_output_audit,
        "compared_coefficient_count",
        f"{label} real_output_audit",
    )
    require_nonnegative_int(
        real_output_audit,
        "passed_coefficient_count",
        f"{label} real_output_audit",
    )
    return "phase0-failure-code-audit"


def validate_case_study_readiness(path: Path, payload: dict[str, Any]) -> str:
    label = str(path)
    require_schema_version(payload, label)
    require_bool_fields(
        payload,
        (
            "case_study_ids_match_selected_benchmarks",
            "digit_threshold_profiles_match_verification_strategy",
            "failure_code_profile_matches_parity_matrix",
            "parity_labels_match_parity_matrix",
            "regression_profile_matches_parity_matrix",
            "runtime_blocked_case_study_lanes_match_theory_frontier",
            "runtime_lane_predecessors_recorded",
            "selected_benchmark_refs_match_selected_benchmarks",
            "stronger_threshold_assignments_match_selected_benchmark_anchors",
        ),
        label,
    )
    family_count = require_nonnegative_int(payload, "case_study_family_count", label)
    require_nonnegative_int(payload, "default_minimum_correct_digits", label)
    case_study_ids = require_string_list(payload, "case_study_ids", label)
    require_unique(case_study_ids, f"{label} case_study_ids")
    for field in (
        "literature_anchor_case_study_ids",
        "matrix_only_case_study_ids",
        "runtime_blocked_case_study_ids",
        "strong_precision_case_study_ids",
    ):
        require_unique(require_string_list(payload, field, label), f"{label} {field}")
    families = require_object_list(payload, "case_study_families", label)
    expect(len(families) == family_count, f"{label} case_study_family_count mismatch")
    for index, family in enumerate(families):
        entry_label = f"{label} case_study_families[{index}]"
        for field in (
            "id",
            "digit_threshold_profile",
            "failure_code_profile",
            "family_state",
            "parity_matrix_label",
            "regression_profile",
        ):
            require_non_empty_str(family, field, entry_label)
        require_field(family, "next_runtime_lane", str, entry_label)
        require_field(family, "landed_runtime_predecessor", str, entry_label)
        require_nonnegative_int(family, "minimum_correct_digits", entry_label)
        require_string_list(family, "known_regression_families", entry_label)
        require_string_list(family, "required_failure_codes", entry_label)
        require_field(family, "selected_benchmark_refs", list, entry_label)
    return "case-study-readiness"


def validate_case_study_numerics(path: Path, payload: dict[str, Any]) -> str:
    label = str(path)
    require_schema_version(payload, label)
    expect(payload.get("scope") == "case-study-numerics", f"{label} scope drifted")
    require_bool_fields(
        payload,
        (
            "all_case_studies_meet_digit_thresholds",
            "case_study_numeric_comparison_passed",
            "digit_threshold_profiles_reported",
            "regression_profiles_reported",
            "required_failure_code_profiles_reported",
        ),
        label,
    )
    require_nonnegative_int(payload, "case_study_numeric_evidence_count", label)
    for field in (
        "blocking_reasons",
        "compared_case_study_ids",
        "missing_case_study_numeric_ids",
        "numeric_evidence_paths",
        "withheld_claims",
    ):
        require_unique(require_string_list(payload, field, label), f"{label} {field}")
    for field in (
        "case_study_digit_threshold_profiles_by_family",
        "case_study_failure_code_profiles_by_family",
        "case_study_regression_profiles_by_family",
        "minimum_observed_correct_digits_by_case_study",
    ):
        require_object(payload, field, label)
    for index, summary in enumerate(
        require_object_list(payload, "case_study_numeric_family_summaries", label)
    ):
        entry_label = f"{label} case_study_numeric_family_summaries[{index}]"
        require_non_empty_str(summary, "id", entry_label)
        require_non_empty_str(summary, "evidence_path", entry_label)
        require_field(summary, "comparison_passed", bool, entry_label)
        require_field(summary, "digit_threshold_met", bool, entry_label)
        require_field(summary, "metadata_matches_readiness", bool, entry_label)
        require_nonnegative_int(summary, "minimum_observed_correct_digits", entry_label)
        require_nonnegative_int(summary, "required_minimum_correct_digits", entry_label)
    return "case-study-numerics"


def validate_comparison_output(path: Path, payload: dict[str, Any]) -> str:
    label = str(path)
    require_schema_version(payload, label)
    for field in ("amflow_golden", "benchmark_id", "comparison", "cpp_result"):
        require_non_empty_str(payload, field, label)
    require_field(payload, "passed", bool, label)
    for field in (
        "compared_coefficient_count",
        "matched_integral_count",
        "minimum_digit_agreement",
        "passed_coefficient_count",
        "tolerance_digits",
    ):
        require_nonnegative_int(payload, field, label)
    require_field(payload, "family_aliases", dict, label)
    require_field(payload, "failures", list, label)
    integrals = require_object_list(payload, "integrals", label)
    expect(integrals, f"{label} integrals must not be empty")
    for index, integral in enumerate(integrals):
        entry_label = f"{label} integrals[{index}]"
        require_non_empty_str(integral, "integral", entry_label)
        require_non_empty_str(integral, "status", entry_label)
        require_field(integral, "coefficients", list, entry_label)
    if "reference_floor_matches" in payload:
        require_object_list(payload, "reference_floor_matches", label)
    return "comparison-output"


def validate_cpp_result(path: Path, payload: dict[str, Any]) -> str:
    label = str(path)
    require_schema_version(payload, label)
    require_non_empty_str(payload, "benchmark_id", label)
    require_non_empty_str(payload, "family", label)
    require_non_empty_str(payload, "status", label)
    require_field(payload, "duration_seconds", float, label)
    require_object(payload, "solver", label)
    results = require_object_list(payload, "results", label)
    expect(results, f"{label} results must not be empty")
    for index, result in enumerate(results):
        entry_label = f"{label} results[{index}]"
        require_non_empty_str(result, "integral", entry_label)
    require_field(payload, "targets", list, label)
    if "state_results" in payload:
        require_object_list(payload, "state_results", label)
    return "cpp-result-output"


def validate_b61n_row56_diagnostic(path: Path, payload: dict[str, Any]) -> str:
    label = str(path)
    require_schema_version(payload, label)
    for field in ("amflow_golden", "benchmark_id", "cpp_result", "diagnostic_id"):
        require_non_empty_str(payload, field, label)
    require_nonnegative_int(payload, "tolerance_digits", label)
    for field in ("classification", "comparison_summary", "runtime_path", "source_references"):
        require_object(payload, field, label)
    target_diagnostics = require_object_list(payload, "target_diagnostics", label)
    expect(target_diagnostics, f"{label} target_diagnostics must not be empty")
    return "b61n-row56-specific-target-diagnostic"


def validate_reference_floor_manifest(path: Path, payload: dict[str, Any]) -> str:
    label = str(path)
    require_schema_version(payload, label)
    require_non_empty_str(payload, "benchmark_id", label)
    require_non_empty_str(payload, "golden_manifest", label)
    require_string_list(payload, "notes", label)
    entries = require_object_list(payload, "compare_cpp_vs_amflow_reference_floors", label)
    expect(entries, f"{label} compare_cpp_vs_amflow_reference_floors must not be empty")
    for index, entry in enumerate(entries):
        entry_label = f"{label} compare_cpp_vs_amflow_reference_floors[{index}]"
        require_non_empty_str(entry, "integral", entry_label)
        order = require_field(entry, "order", (int, str), entry_label)
        expect(type(order) is not bool, f"{entry_label} order must be an int or string")
        if isinstance(order, str):
            expect(order.strip(), f"{entry_label} order must not be empty")
        require_non_empty_str(entry, "reason", entry_label)
    return "b61n-reference-floor-golden-manifest"


def validate_one_off_scoped_sidecar(path: Path, payload: dict[str, Any]) -> str:
    label = str(path)
    require_schema_version(payload, label)
    scope = require_non_empty_str(payload, "scope", label)
    if scope == "lane115 M6 phase-0 and case-study failure-code audit evidence":
        require_non_empty_str(payload, "required_failure_code_profile", label)
        for field in (
            "anti_fake_parity_notes",
            "case_study_families",
            "phase0_reference_captured_benchmarks",
            "required_failure_codes",
            "withheld_claims",
        ):
            require_field(payload, field, list, label)
    elif scope == "phase0-loop-50digit-recapture":
        for field in ("lane", "diagnosis"):
            require_non_empty_str(payload, field, label)
        for field in ("baseline", "recapture", "post_fix", "anti_fake_parity"):
            require_object(payload, field, label)
    elif scope == "lane135-user-defined-amfmode-packet-manifest-gate":
        require_non_empty_str(payload, "source_packet_root", label)
        for field in (
            "anti_fake_parity",
            "failure_observed_in_lane132",
            "manifest_chain",
            "manifest_evidence",
            "numeric_evidence",
        ):
            require_object(payload, field, label)
    elif scope == "post-M7 b61n row 5/6 reference-floor alternative-path precision evidence":
        for field in ("evidence_id", "generated_at_utc", "source_cpp_result", "source_diagnostic"):
            require_non_empty_str(payload, field, label)
        for field in ("method", "summary"):
            require_object(payload, field, label)
        require_object_list(payload, "targets", label)
    elif scope == "m7-prerequisite-m5-packet-acceptance":
        require_bool_fields(payload, ("accepted_for_release_prerequisite", "m5_packet_reviewed"), label)
        for field in (
            "current_state",
            "m5_packet_path",
            "release_readiness_consumer",
            "release_readiness_output_path",
        ):
            require_non_empty_str(payload, field, label)
        for field in ("blocking_reasons", "evidence_paths", "withheld_claims"):
            require_string_list(payload, field, label)
        for field in ("packet_assertions", "review_evidence"):
            require_object(payload, field, label)
    elif scope == "m7-parity-signoff-scope-audit":
        for field in ("status", "option_selected", "generated_at"):
            require_non_empty_str(payload, field, label)
        for field in ("base", "current_retained_prerequisites", "local_probe_results"):
            require_object(payload, field, label)
        for field in (
            "non_claims",
            "exact_blockers",
            "missing_artifacts",
            "missing_tests",
            "not_blockers_for_parity_sidecar_itself",
            "next_atomic_actions",
        ):
            require_field(payload, field, list, label)
    elif scope == "automatic_loop eps21 eps22 implementation":
        require_nonnegative_int(payload, "lane", label)
        for field in ("status", "recorded_at"):
            require_non_empty_str(payload, field, label)
        for field in (
            "dependency_gate",
            "manifest_gate",
            "theory_preplan",
            "derived_constant_prep",
            "anti_fake_parity_audit",
            "partial_gate_results",
            "role_reviews",
        ):
            require_object(payload, field, label)
        require_string_list(payload, "withheld_claims", label)
    else:
        raise SchemaError(f"{label} has no registered one-off schema for scope {scope!r}")
    return scope


def validate_m7_sidecar(path: Path, root: Path) -> str:
    payload = read_json(path)
    validate_source_provenance(path, root, payload)
    relative_path = path.relative_to(root)
    scope = payload.get("scope")

    if "release_signoff_ready" in payload:
        return validate_release_readiness_output(relative_path, payload)
    if relative_path.name == "qualification-readiness.json":
        load_qualification_summary(path)
        return "qualification-readiness"
    if scope in STRICT_SCOPE_VALIDATORS:
        STRICT_SCOPE_VALIDATORS[str(scope)](path)
        return str(scope)
    if scope == "case-study-numerics":
        return validate_case_study_numerics(relative_path, payload)
    if relative_path.name == "case-study-readiness.json":
        return validate_case_study_readiness(relative_path, payload)
    if relative_path.name == "phase0-packet-comparison.json":
        return validate_phase0_packet_comparison(relative_path, payload)
    if relative_path.name == "phase0-correct-digits.json":
        return validate_phase0_correct_digits(relative_path, payload)
    if relative_path.name == "phase0-failure-codes.json":
        return validate_phase0_failure_codes(relative_path, payload)
    if relative_path.name.endswith(".failure-code-audit.json"):
        return validate_failure_code_audit(relative_path, payload)
    if relative_path.name.endswith(".cpp-result.json"):
        return validate_cpp_result(relative_path, payload)
    if ".compare" in relative_path.name and relative_path.name.endswith(".json"):
        return validate_comparison_output(relative_path, payload)
    if relative_path.name == "b61n-row56-specific-target-diagnostic.json":
        return validate_b61n_row56_diagnostic(relative_path, payload)
    if relative_path.name == "complex_kinematics.b61n-reference-floor-golden-manifest.json":
        return validate_reference_floor_manifest(relative_path, payload)
    if isinstance(scope, str):
        return validate_one_off_scoped_sidecar(relative_path, payload)
    raise SchemaError(f"{relative_path} has no M7 schema validator")


def build_readiness_command(root: Path, accepted_summary: dict[str, Any], output_path: Path) -> list[str]:
    command = [
        sys.executable,
        str(root / "tools/reference-harness/scripts/release_signoff_readiness.py"),
    ]
    for option, field in READINESS_INPUTS:
        command.extend([option, require_repo_path(root, accepted_summary.get(field), field)])
    command.extend(["--summary-path", str(output_path)])
    return command


def validate_fresh_accepted_readiness_output(root: Path) -> None:
    accepted_path = root / ACCEPTED_READINESS_SIDECAR
    accepted_summary = read_json(accepted_path)
    with tempfile.TemporaryDirectory(prefix="m7-release-schema-") as temp_dir:
        output_path = Path(temp_dir) / "release-readiness.json"
        completed = subprocess.run(
            build_readiness_command(root, accepted_summary, output_path),
            cwd=root,
            text=True,
            capture_output=True,
            check=False,
        )
        if completed.returncode != 0:
            if completed.stdout:
                print(completed.stdout, file=sys.stderr)
            if completed.stderr:
                print(completed.stderr, file=sys.stderr)
            raise SchemaError("fresh accepted release-readiness output generation failed")
        fresh_summary = read_json(output_path)

    validate_release_readiness_output(ACCEPTED_READINESS_SIDECAR, accepted_summary)
    validate_release_readiness_output(Path("<fresh accepted release-readiness output>"), fresh_summary)
    accepted_keys = set(accepted_summary)
    fresh_keys = set(fresh_summary)
    expect(
        fresh_keys == accepted_keys,
        "fresh accepted release-readiness output key set drifted: "
        f"missing={sorted(accepted_keys - fresh_keys)} added={sorted(fresh_keys - accepted_keys)}",
    )


def expect_schema_error(label: str, expected: str, action: Callable[[], None]) -> None:
    try:
        action()
    except SchemaError as error:
        expect(expected in str(error), f"{label} failed for the wrong reason: {error}")
        return
    raise SchemaError(f"{label} unexpectedly passed")


def validate_source_provenance_self_check(root: Path) -> None:
    accepted_summary = read_json(root / ACCEPTED_READINESS_SIDECAR)
    accepted_commit = require_exact_non_empty_str(
        accepted_summary,
        "source_commit",
        ACCEPTED_READINESS_SIDECAR.as_posix(),
    )
    synthetic_path = (
        root
        / "tools/reference-harness/specs/m7/lane3/synthetic-source-provenance.json"
    )
    base_payload = {"source_commit": accepted_commit}
    base_payload["source_provenance_sha256"] = source_provenance_sha256(base_payload)
    validate_source_provenance(synthetic_path, root, base_payload)

    whitespace_commit = dict(base_payload)
    whitespace_commit["source_commit"] = f" {accepted_commit} "
    whitespace_commit["source_provenance_sha256"] = source_provenance_sha256(whitespace_commit)
    expect_schema_error(
        "source_commit surrounding whitespace guard",
        "source_commit must not carry surrounding whitespace",
        lambda: validate_source_provenance(synthetic_path, root, whitespace_commit),
    )

    whitespace_digest = dict(base_payload)
    whitespace_digest["source_provenance_sha256"] = (
        f" {base_payload['source_provenance_sha256']} "
    )
    expect_schema_error(
        "source_provenance_sha256 surrounding whitespace guard",
        "source_provenance_sha256 must not carry surrounding whitespace",
        lambda: validate_source_provenance(synthetic_path, root, whitespace_digest),
    )


def collect_m7_sidecar_schemas(root: Path, m7_root: Path) -> dict[str, str]:
    sidecar_schemas: dict[str, str] = {}
    m7_root = require_m7_root(root, m7_root)
    paths = sorted((root / m7_root).rglob("*.json"))
    expect(paths, f"{m7_root} must contain M7 JSON sidecars")
    errors: list[str] = []
    for path in paths:
        relative = str(path.relative_to(root))
        try:
            schema_name = validate_m7_sidecar(path, root)
            expect(relative not in sidecar_schemas, f"{relative} was validated twice")
            sidecar_schemas[relative] = schema_name
        except Exception as error:  # noqa: BLE001 - report every schema drift before failing.
            errors.append(f"{relative}: {error}")
    if errors:
        raise SchemaError("M7 sidecar schema validation failed:\n" + "\n".join(errors))
    return sidecar_schemas


def validate_all_m7_sidecars(root: Path, m7_root: Path) -> dict[str, int]:
    schema_counts: dict[str, int] = {}
    for schema_name in collect_m7_sidecar_schemas(root, m7_root).values():
        schema_counts[schema_name] = schema_counts.get(schema_name, 0) + 1
    return schema_counts


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--m7-root",
        default=str(M7_ROOT),
        help="Repository-relative M7 sidecar root to validate.",
    )
    parser.add_argument(
        "--skip-fresh-readiness",
        action="store_true",
        help="Only validate committed sidecars; do not regenerate the accepted readiness output.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    root = repo_root()
    m7_root = Path(args.m7_root)
    schema_counts = validate_all_m7_sidecars(root, m7_root)
    if not args.skip_fresh_readiness:
        validate_fresh_accepted_readiness_output(root)
    validate_source_provenance_self_check(root)
    total = sum(schema_counts.values())
    counts = ", ".join(f"{name}={count}" for name, count in sorted(schema_counts.items()))
    print(f"M7 release sidecar schema validation passed: {total} JSON sidecars ({counts})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
