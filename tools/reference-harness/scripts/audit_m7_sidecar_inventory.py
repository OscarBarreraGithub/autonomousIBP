#!/usr/bin/env python3
"""List committed M7 JSON sidecars by accepted/unaccepted release status."""

from __future__ import annotations

import argparse
import json
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Callable

from validate_m7_release_sidecar_schemas import (
    M7_ROOT,
    SchemaError,
    collect_m7_sidecar_schemas,
    repo_root,
    require_m7_root,
    validate_m7_sidecar,
)


PRIMARY_BOOL_BY_SCHEMA = {
    "case-study-families-only": "case_study_families_qualified",
    "case-study-numerics": "case_study_numeric_comparison_passed",
    "comparison-output": "passed",
    "milestone-m6-qualification": "milestone_m6_ready",
    "phase0-correct-digits": "all_compared_benchmarks_meet_digit_thresholds",
    "phase0-packet-set-only": "phase0_packet_set_qualified",
    "release-diagnostic-review": "diagnostic_review_complete",
    "release-docs-completion": "docs_completion_review_complete",
    "release-parity-signoff": "parity_signoff_complete",
    "release-performance-review": "performance_review_complete",
    "release-qualification-corpus": "qualification_corpus_review_complete",
    "release-readiness-output": "release_signoff_ready",
    "m7-prerequisite-m5-packet-acceptance": "accepted_for_release_prerequisite",
}

ACCEPTED_STATUS_VALUES = {"accepted", "complete", "reviewed", "success"}


@dataclass(frozen=True)
class InventoryEntry:
    path: str
    schema: str
    status: str
    basis: str


class InventoryError(RuntimeError):
    """Raised when an M7 sidecar cannot be inventoried honestly."""


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise InventoryError(message)


def read_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as stream:
        payload = json.load(stream)
    expect(isinstance(payload, dict), f"{path} must contain a JSON object")
    return payload


def bool_status(payload: dict[str, Any], field: str) -> tuple[str, str]:
    value = payload.get(field)
    expect(type(value) is bool, f"{field} must be a bool")
    return ("accepted" if value else "unaccepted", f"{field}={str(value).lower()}")


def all_top_level_bools_status(payload: dict[str, Any], label: str) -> tuple[str, str]:
    bool_fields = sorted(key for key, value in payload.items() if type(value) is bool)
    expect(bool_fields, f"{label} has no top-level invariant booleans")
    failed = [field for field in bool_fields if payload[field] is not True]
    if failed:
        return "unaccepted", "failed_invariants=" + ",".join(failed)
    return "accepted", f"{len(bool_fields)} invariants passed"


def status_field_status(payload: dict[str, Any]) -> tuple[str, str]:
    value = payload.get("status")
    expect(isinstance(value, str) and value.strip(), "status must be a non-empty string")
    normalized = value.strip().lower()
    status = "accepted" if normalized in ACCEPTED_STATUS_VALUES else "unaccepted"
    return status, f"status={value.strip()}"


def phase0_failure_codes_status(payload: dict[str, Any]) -> tuple[str, str]:
    required = (
        "all_compared_benchmarks_publish_failure_code_audits",
        "all_compared_benchmarks_report_required_failure_codes",
        "any_compared_benchmarks_report_unexpected_failure_codes",
    )
    for field in required:
        expect(type(payload.get(field)) is bool, f"{field} must be a bool")
    accepted = (
        payload["all_compared_benchmarks_publish_failure_code_audits"]
        and payload["all_compared_benchmarks_report_required_failure_codes"]
        and not payload["any_compared_benchmarks_report_unexpected_failure_codes"]
    )
    basis = (
        "publish_audits="
        f"{str(payload['all_compared_benchmarks_publish_failure_code_audits']).lower()},"
        "required_codes="
        f"{str(payload['all_compared_benchmarks_report_required_failure_codes']).lower()},"
        "unexpected_codes="
        f"{str(payload['any_compared_benchmarks_report_unexpected_failure_codes']).lower()}"
    )
    return ("accepted" if accepted else "unaccepted", basis)


def failure_code_audit_status(payload: dict[str, Any]) -> tuple[str, str]:
    audit = payload.get("real_output_audit")
    expect(isinstance(audit, dict), "real_output_audit must be an object")
    value = audit.get("comparison_passed")
    expect(type(value) is bool, "real_output_audit.comparison_passed must be a bool")
    return (
        "accepted" if value else "unaccepted",
        f"real_output_audit.comparison_passed={str(value).lower()}",
    )


def cpp_result_status(payload: dict[str, Any]) -> tuple[str, str]:
    value = payload.get("status")
    expect(isinstance(value, str) and value.strip(), "status must be a non-empty string")
    normalized = value.strip().lower()
    return ("accepted" if normalized == "success" else "unaccepted", f"status={value.strip()}")


def row56_diagnostic_status(payload: dict[str, Any]) -> tuple[str, str]:
    summary = payload.get("comparison_summary")
    expect(isinstance(summary, dict), "comparison_summary must be an object")
    value = summary.get("passed")
    expect(type(value) is bool, "comparison_summary.passed must be a bool")
    return ("accepted" if value else "unaccepted", f"comparison_summary.passed={str(value).lower()}")


def recapture_status(payload: dict[str, Any]) -> tuple[str, str]:
    post_fix = payload.get("post_fix")
    expect(isinstance(post_fix, dict), "post_fix must be an object")
    value = post_fix.get("phase0_packet_set_qualified")
    expect(type(value) is bool, "post_fix.phase0_packet_set_qualified must be a bool")
    return (
        "accepted" if value else "unaccepted",
        f"post_fix.phase0_packet_set_qualified={str(value).lower()}",
    )


def user_defined_manifest_gate_status(payload: dict[str, Any]) -> tuple[str, str]:
    anti_fake = payload.get("anti_fake_parity")
    numeric = payload.get("numeric_evidence")
    expect(isinstance(anti_fake, dict), "anti_fake_parity must be an object")
    expect(isinstance(numeric, dict), "numeric_evidence must be an object")
    canonical_real = anti_fake.get("canonical_text_contains_real_user_defined_amfmode_values")
    synthesized = anti_fake.get("manifest_values_synthesized_for_test")
    digits = numeric.get("minimum_observed_correct_digits")
    expect(type(canonical_real) is bool, "anti_fake_parity canonical_text flag must be a bool")
    expect(type(synthesized) is bool, "anti_fake_parity synthesized flag must be a bool")
    expect(type(digits) is int and digits >= 0, "numeric_evidence minimum digits must be nonnegative")
    accepted = canonical_real and not synthesized and digits >= 30
    basis = (
        f"canonical_real={str(canonical_real).lower()},"
        f"synthesized={str(synthesized).lower()},minimum_digits={digits}"
    )
    return ("accepted" if accepted else "unaccepted", basis)


def b61n_precision_status(payload: dict[str, Any]) -> tuple[str, str]:
    summary = payload.get("summary")
    expect(isinstance(summary, dict), "summary must be an object")
    gates = {
        key: value
        for key, value in summary.items()
        if key.startswith("all_") and type(value) is bool
    }
    expect(gates, "summary must contain all_* boolean gates")
    failed = [field for field, value in sorted(gates.items()) if not value]
    if failed:
        return "unaccepted", "failed_summary_gates=" + ",".join(failed)
    return "accepted", f"{len(gates)} summary gates passed"


def classify_sidecar(schema_name: str, payload: dict[str, Any]) -> tuple[str, str]:
    if schema_name in PRIMARY_BOOL_BY_SCHEMA:
        return bool_status(payload, PRIMARY_BOOL_BY_SCHEMA[schema_name])
    if schema_name in {"case-study-readiness", "qualification-readiness", "phase0-packet-comparison"}:
        return all_top_level_bools_status(payload, schema_name)
    if schema_name == "phase0-failure-codes":
        return phase0_failure_codes_status(payload)
    if schema_name == "phase0-failure-code-audit":
        return failure_code_audit_status(payload)
    if schema_name == "cpp-result-output":
        return cpp_result_status(payload)
    if schema_name == "b61n-row56-specific-target-diagnostic":
        return row56_diagnostic_status(payload)
    if schema_name == "b61n-reference-floor-golden-manifest":
        return "accepted", "schema-validated reference-floor manifest"
    if schema_name == "lane115 M6 phase-0 and case-study failure-code audit evidence":
        return "accepted", "schema-validated failure-code profile audit"
    if schema_name == "phase0-loop-50digit-recapture":
        return recapture_status(payload)
    if schema_name == "lane135-user-defined-amfmode-packet-manifest-gate":
        return user_defined_manifest_gate_status(payload)
    if schema_name == "post-M7 b61n row 5/6 reference-floor alternative-path precision evidence":
        return b61n_precision_status(payload)
    if schema_name in {"m7-parity-signoff-scope-audit", "automatic_loop eps21 eps22 implementation"}:
        return status_field_status(payload)
    raise InventoryError(f"no inventory classifier registered for schema {schema_name!r}")


def build_inventory(root: Path, m7_root: Path) -> list[InventoryEntry]:
    m7_root = require_m7_root(root, m7_root)
    paths = sorted((root / m7_root).rglob("*.json"))
    expect(paths, f"{m7_root} must contain M7 JSON sidecars")
    entries: list[InventoryEntry] = []
    errors: list[str] = []
    for path in paths:
        relative = str(path.relative_to(root))
        try:
            payload = read_json(path)
            schema_name = validate_m7_sidecar(path, root)
            status, basis = classify_sidecar(schema_name, payload)
            expect(status in {"accepted", "unaccepted"}, f"{relative} returned invalid status")
            entries.append(
                InventoryEntry(
                    path=relative,
                    schema=schema_name,
                    status=status,
                    basis=basis,
                )
            )
        except Exception as error:  # noqa: BLE001 - report the full inventory drift.
            errors.append(f"{relative}: {error}")
    if errors:
        raise InventoryError("M7 sidecar inventory failed:\n" + "\n".join(errors))
    return entries


def expect_inventory_error(label: str, expected: str, action: Callable[[], None]) -> None:
    try:
        action()
    except (InventoryError, SchemaError) as error:
        expect(expected in str(error), f"{label} failed for the wrong reason: {error}")
        return
    raise InventoryError(f"{label} unexpectedly passed")


def self_check(root: Path) -> None:
    entries = build_inventory(root, M7_ROOT)
    verify_inventory(entries)
    expect_inventory_error(
        "absolute m7 root guard",
        "m7 root must be repository-relative",
        lambda: build_inventory(root, root / M7_ROOT),
    )
    expect_inventory_error(
        "parent traversal m7 root guard",
        "m7 root must not contain '..'",
        lambda: build_inventory(root, Path("../autoIBP/tools/reference-harness/specs/m7")),
    )
    expect_inventory_error(
        "non-directory m7 root guard",
        "m7 root does not exist as a directory",
        lambda: build_inventory(root, Path("CMakeLists.txt")),
    )


def render_text(entries: list[InventoryEntry], *, summary_only: bool) -> str:
    accepted = sum(1 for entry in entries if entry.status == "accepted")
    unaccepted = len(entries) - accepted
    lines = [
        (
            "M7 sidecar inventory: "
            f"total={len(entries)} accepted={accepted} unaccepted={unaccepted}"
        )
    ]
    if summary_only:
        return "\n".join(lines)
    for entry in entries:
        lines.append(f"{entry.status:10} {entry.schema:64} {entry.path} ({entry.basis})")
    return "\n".join(lines)


def render_json(entries: list[InventoryEntry]) -> str:
    accepted = sum(1 for entry in entries if entry.status == "accepted")
    payload = {
        "schema_version": 1,
        "sidecar_count": len(entries),
        "accepted_count": accepted,
        "unaccepted_count": len(entries) - accepted,
        "sidecars": [entry.__dict__ for entry in entries],
    }
    return json.dumps(payload, indent=2, sort_keys=True)


def verify_inventory(entries: list[InventoryEntry]) -> None:
    expect(entries, "inventory must not be empty")
    statuses = {entry.status for entry in entries}
    expect(statuses == {"accepted", "unaccepted"}, "inventory must include accepted and unaccepted sidecars")
    paths = [entry.path for entry in entries]
    expect(len(paths) == len(set(paths)), "inventory must not contain duplicate paths")


def verify_schema_reconciliation(root: Path, m7_root: Path, entries: list[InventoryEntry]) -> None:
    schema_by_path = collect_m7_sidecar_schemas(root, m7_root)
    inventory_by_path = {entry.path: entry for entry in entries}
    expect(
        len(inventory_by_path) == len(entries),
        "inventory/schema reconciliation requires unique inventory paths",
    )
    schema_paths = set(schema_by_path)
    inventory_paths = set(inventory_by_path)
    missing_from_inventory = sorted(schema_paths - inventory_paths)
    missing_from_schema_validation = sorted(inventory_paths - schema_paths)
    schema_mismatches = [
        (
            path,
            inventory_by_path[path].schema,
            schema_by_path[path],
        )
        for path in sorted(schema_paths & inventory_paths)
        if inventory_by_path[path].schema != schema_by_path[path]
    ]
    if missing_from_inventory or missing_from_schema_validation or schema_mismatches:
        lines = ["M7 inventory/schema reconciliation failed:"]
        for path in missing_from_inventory:
            lines.append(f"schema-valid sidecar missing from inventory: {path}")
        for path in missing_from_schema_validation:
            lines.append(f"inventory sidecar missing from schema validation: {path}")
        for path, inventory_schema, schema_validation_schema in schema_mismatches:
            lines.append(
                "inventory/schema classification mismatch: "
                f"{path}: inventory={inventory_schema!r} schema={schema_validation_schema!r}"
            )
        raise InventoryError("\n".join(lines))


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--m7-root",
        default=str(M7_ROOT),
        help="Repository-relative M7 sidecar root to inventory.",
    )
    parser.add_argument(
        "--format",
        choices=("text", "json"),
        default="text",
        help="Output format.",
    )
    parser.add_argument(
        "--summary-only",
        action="store_true",
        help="Only print aggregate counts in text mode.",
    )
    parser.add_argument(
        "--verify",
        action="store_true",
        help="Fail if the inventory is empty, duplicate, or lacks either status class.",
    )
    parser.add_argument(
        "--verify-schema-reconciliation",
        action="store_true",
        help=(
            "Fail unless the inventory path/schema set exactly matches the M7 sidecar "
            "schema validator."
        ),
    )
    parser.add_argument(
        "--self-check",
        action="store_true",
        help="Run positive and negative checks for M7 inventory root validation.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    root = repo_root()
    m7_root = Path(args.m7_root)
    try:
        if args.self_check:
            self_check(root)
            print("M7 sidecar inventory root self-check passed")
            return 0
        entries = build_inventory(root, m7_root)
        if args.verify:
            verify_inventory(entries)
        if args.verify_schema_reconciliation:
            verify_schema_reconciliation(root, m7_root, entries)
    except (InventoryError, SchemaError) as error:
        print(str(error), file=sys.stderr)
        return 1

    if args.format == "json":
        print(render_json(entries))
    else:
        print(render_text(entries, summary_only=args.summary_only))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
