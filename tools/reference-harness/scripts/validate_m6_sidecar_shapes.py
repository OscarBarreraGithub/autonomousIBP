#!/usr/bin/env python3
"""Validate committed M6 sidecar JSON and SQLite shapes."""

from __future__ import annotations

import argparse
import json
import sqlite3
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Any


M6_ROOT = Path("tools/reference-harness/specs/m6")
SQLITE_SUFFIXES = {".db", ".sqlite", ".sqlite3"}


class SchemaError(RuntimeError):
    """Raised when an M6 sidecar does not match its expected shape."""


@dataclass(frozen=True)
class SidecarValidation:
    path: str
    family: str
    accepted: bool
    basis: str


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise SchemaError(message)


def read_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as stream:
        payload = json.load(stream)
    expect(isinstance(payload, dict), f"{path} must contain a JSON object")
    return payload


def write_json(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")


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
        expect(type(value) is int and type(value) is not bool, f"{label} {field} must be an int")
    elif expected_type is float:
        expect(
            type(value) in {int, float} and type(value) is not bool,
            f"{label} {field} must be numeric",
        )
    else:
        expect(isinstance(value, expected_type), f"{label} {field} has unexpected type")
    return value


def require_object(payload: dict[str, Any], field: str, label: str) -> dict[str, Any]:
    return require_field(payload, field, dict, label)


def require_list(payload: dict[str, Any], field: str, label: str) -> list[Any]:
    return require_field(payload, field, list, label)


def require_non_empty_str(payload: dict[str, Any], field: str, label: str) -> str:
    value = require_field(payload, field, str, label)
    expect(value.strip(), f"{label} {field} must not be empty")
    return value.strip()


def require_schema_version(payload: dict[str, Any], label: str) -> None:
    value = payload.get("schema_version")
    expect(type(value) is int and value == 1, f"{label} schema_version must be integer 1")


def require_nonnegative_int(payload: dict[str, Any], field: str, label: str) -> int:
    value = require_field(payload, field, int, label)
    expect(value >= 0, f"{label} {field} must be nonnegative")
    return value


def require_string_list(payload: dict[str, Any], field: str, label: str) -> list[str]:
    raw = require_list(payload, field, label)
    values: list[str] = []
    for index, item in enumerate(raw):
        expect(
            isinstance(item, str) and item.strip(),
            f"{label} {field}[{index}] must be a non-empty string",
        )
        values.append(item.strip())
    return values


def require_object_list(payload: dict[str, Any], field: str, label: str) -> list[dict[str, Any]]:
    raw = require_list(payload, field, label)
    values: list[dict[str, Any]] = []
    for index, item in enumerate(raw):
        expect(isinstance(item, dict), f"{label} {field}[{index}] must be an object")
        values.append(item)
    return values


def require_repo_path(root: Path, raw: Any, field: str) -> str:
    expect(isinstance(raw, str) and raw.strip(), f"{field} must be a non-empty string")
    value = raw.strip()
    candidate = Path(value)
    resolved = candidate.resolve(strict=False) if candidate.is_absolute() else (root / candidate).resolve(strict=False)
    try:
        resolved.relative_to(root.resolve(strict=False))
    except ValueError as error:
        if candidate.is_absolute():
            return value
        raise SchemaError(f"{field} must stay within the repository: {value}") from error
    expect(resolved.exists(), f"{field} does not exist: {value}")
    return value


def resolve_m6_root(root: Path, m6_root: Path) -> Path:
    candidate = m6_root if m6_root.is_absolute() else root / m6_root
    resolved_root = root.resolve(strict=False)
    resolved = candidate.resolve(strict=False)
    try:
        resolved.relative_to(resolved_root)
    except ValueError as error:
        raise SchemaError(f"{m6_root} must stay within the repository") from error
    expect(resolved.is_dir(), f"{m6_root} must exist")
    return resolved


def require_optional_repo_path(
    root: Path,
    payload: dict[str, Any],
    field: str,
    label: str,
) -> None:
    if field in payload:
        require_repo_path(root, payload[field], f"{label} {field}")


def validate_comparison_summary(payload: dict[str, Any], label: str) -> bool:
    passed = require_field(payload, "passed", bool, label)
    compared = require_nonnegative_int(payload, "compared_coefficient_count", label)
    passed_count = require_nonnegative_int(payload, "passed_coefficient_count", label)
    expect(
        passed_count <= compared,
        f"{label} passed_coefficient_count must not exceed compared_coefficient_count",
    )
    if "minimum_digit_agreement" in payload:
        require_nonnegative_int(payload, "minimum_digit_agreement", label)
    if "tolerance_digits" in payload:
        require_nonnegative_int(payload, "tolerance_digits", label)
    if "matched_integral_count" in payload:
        require_nonnegative_int(payload, "matched_integral_count", label)
    return passed


def validate_comparison_output(path: Path, root: Path, payload: dict[str, Any]) -> SidecarValidation:
    label = str(path.relative_to(root))
    require_schema_version(payload, label)
    require_non_empty_str(payload, "benchmark_id", label)
    require_non_empty_str(payload, "comparison", label)
    require_repo_path(root, payload.get("cpp_result"), f"{label} cpp_result")
    require_repo_path(root, payload.get("amflow_golden"), f"{label} amflow_golden")
    require_optional_repo_path(root, payload, "amflow_state", label)
    passed = validate_comparison_summary(payload, label)
    failures = require_list(payload, "failures", label)
    integrals = require_object_list(payload, "integrals", label)
    matched = require_nonnegative_int(payload, "matched_integral_count", label)
    expect(matched <= len(integrals), f"{label} matched_integral_count exceeds integrals length")
    if passed:
        expect(not failures, f"{label} passed comparisons must not carry failures")
    accepted = passed and not path.name.endswith(".blocked.json")
    return SidecarValidation(label, "comparison-output", accepted, f"passed={str(passed).lower()}")


def validate_cpp_result(path: Path, root: Path, payload: dict[str, Any]) -> SidecarValidation:
    label = str(path.relative_to(root))
    require_schema_version(payload, label)
    require_non_empty_str(payload, "benchmark_id", label)
    require_non_empty_str(payload, "family", label)
    status = require_non_empty_str(payload, "status", label)
    targets = require_list(payload, "targets", label)
    require_object(payload, "solver", label)
    require_object(payload, "boundary_state", label)
    require_object(payload, "continuation", label)
    results = require_object_list(payload, "results", label)
    require_field(payload, "duration_seconds", float, label)
    accepted = status == "success"
    if accepted:
        expect(targets, f"{label} successful result sidecars must include targets")
        expect(results, f"{label} successful result sidecars must include results")
    return SidecarValidation(label, "cpp-result-output", accepted, f"status={status}")


def validate_failure_code_audit(path: Path, root: Path, payload: dict[str, Any]) -> SidecarValidation:
    label = str(path.relative_to(root))
    require_schema_version(payload, label)
    require_non_empty_str(payload, "audit_kind", label)
    require_non_empty_str(payload, "benchmark_id", label)
    require_string_list(payload, "required_failure_codes", label)
    require_string_list(payload, "observed_failure_codes", label)
    audit = require_object(payload, "real_output_audit", label)
    accepted = False
    if "comparison_passed" in audit:
        accepted = require_field(audit, "comparison_passed", bool, f"{label} real_output_audit")
    return SidecarValidation(
        label,
        "phase0-failure-code-audit",
        accepted,
        f"comparison_passed={str(accepted).lower()}",
    )


def validate_readiness(path: Path, root: Path, payload: dict[str, Any]) -> SidecarValidation:
    label = str(path.relative_to(root))
    require_schema_version(payload, label)
    require_non_empty_str(payload, "benchmark_id", label)
    status = require_non_empty_str(payload, "status", label)
    ready = require_field(payload, "golden_recapture_ready", bool, label)
    checks = require_object(payload, "checks", label)
    for name, value in checks.items():
        expect(type(value) is bool, f"{label} checks.{name} must be a bool")
    require_string_list(payload, "blocking_reasons", label)
    require_object(payload, "failure_code_postmortem", label)
    require_field(payload, "required_runtime_evidence", (dict, list), label)
    return SidecarValidation(
        label,
        "b64ag-golden-recapture-readiness",
        ready and status == "ready",
        f"status={status},golden_recapture_ready={str(ready).lower()}",
    )


def validate_evidence(path: Path, root: Path, payload: dict[str, Any]) -> SidecarValidation:
    label = str(path.relative_to(root))
    require_schema_version(payload, label)
    require_non_empty_str(payload, "benchmark_id", label)
    status = payload.get("status")
    if status is not None:
        expect(isinstance(status, str) and status.strip(), f"{label} status must be a non-empty string")

    for field in ("cpp_result", "full_stripped_result", "amflow_golden_slice"):
        require_optional_repo_path(root, payload, field, label)

    embedded_passed = False
    comparator = payload.get("comparator")
    if comparator is not None:
        expect(isinstance(comparator, dict), f"{label} comparator must be an object")
        embedded_passed = validate_comparison_summary(comparator, f"{label} comparator")
        require_optional_repo_path(root, comparator, "path", f"{label} comparator")

    comparison_summary = payload.get("comparison_summary")
    if isinstance(comparison_summary, dict):
        embedded_passed = (
            validate_comparison_summary(comparison_summary, f"{label} comparison_summary")
            or embedded_passed
        )
    elif isinstance(comparison_summary, str):
        require_repo_path(root, comparison_summary, f"{label} comparison_summary")

    qualifier_pass = payload.get("qualifier_pass")
    qualifier_ready = False
    if qualifier_pass is not None:
        expect(isinstance(qualifier_pass, dict), f"{label} qualifier_pass must be an object")
        qualifier_status = require_non_empty_str(qualifier_pass, "status", f"{label} qualifier_pass")
        qualifier_ready = qualifier_status == "ready"
        require_optional_repo_path(root, qualifier_pass, "summary", f"{label} qualifier_pass")
        if "golden_recapture_ready" in qualifier_pass:
            require_field(qualifier_pass, "golden_recapture_ready", bool, f"{label} qualifier_pass")

    top_level_passed = payload.get("passed")
    if top_level_passed is not None:
        expect(type(top_level_passed) is bool, f"{label} passed must be a bool")

    blocked_status = isinstance(status, str) and any(
        token in status.lower() for token in ("blocked", "partial", "pending")
    )
    accepted = bool(top_level_passed or embedded_passed or qualifier_ready) and not blocked_status
    return SidecarValidation(
        label,
        "runtime-evidence",
        accepted,
        "status=" + (status.strip() if isinstance(status, str) else "unspecified"),
    )


def validate_qualifier_hook(path: Path, root: Path, payload: dict[str, Any]) -> SidecarValidation:
    label = str(path.relative_to(root))
    require_schema_version(payload, label)
    require_non_empty_str(payload, "lane", label)
    require_non_empty_str(payload, "benchmark_id", label)
    for source in require_string_list(payload, "source_evidence", label):
        require_repo_path(root, source, f"{label} source_evidence")
    variants = require_object_list(payload, "publication_variants", label)
    for index, variant in enumerate(variants):
        variant_label = f"{label} publication_variants[{index}]"
        require_non_empty_str(variant, "id", variant_label)
        require_field(variant, "coefficient_publication", bool, variant_label)
        binding = require_object(variant, "source_evidence_binding", variant_label)
        for field in (
            "coefficient_evidence",
            "compare_summary",
            "stripped_result",
            "retained_numeric_source",
        ):
            require_optional_repo_path(root, binding, field, f"{variant_label} source_evidence_binding")
        gate = require_object(variant, "amflow_cross_comparator_publication_gate", variant_label)
        require_optional_repo_path(root, gate, "comparison_summary", f"{variant_label} publication_gate")
        require_optional_repo_path(root, gate, "diagnostic_evidence", f"{variant_label} publication_gate")
        require_field(gate, "currently_allows_publication", bool, f"{variant_label} publication_gate")
    require_object(payload, "m6_qualifier_hook", label)
    require_object(payload, "m7_parity_signoff_hook", label)
    return SidecarValidation(label, "m6-publication-qualifier-hook", False, "hook-only")


def classify_json_sidecar(path: Path, payload: dict[str, Any]) -> str:
    name = path.name
    if {"comparison", "cpp_result", "amflow_golden"}.issubset(payload):
        return "comparison-output"
    if name.endswith((".cpp-result.json", ".stripped-result.json")):
        return "cpp-result-output"
    if name.endswith(".failure-code-audit.json"):
        return "phase0-failure-code-audit"
    if "golden_recapture_ready" in payload:
        return "b64ag-golden-recapture-readiness"
    if "m6_qualifier_hook" in payload:
        return "m6-publication-qualifier-hook"
    if name.endswith("-evidence.json"):
        return "runtime-evidence"
    return "generic-json-sidecar"


def validate_json_sidecar(path: Path, root: Path) -> SidecarValidation:
    payload = read_json(path)
    family = classify_json_sidecar(path, payload)
    if family == "comparison-output":
        return validate_comparison_output(path, root, payload)
    if family == "cpp-result-output":
        return validate_cpp_result(path, root, payload)
    if family == "phase0-failure-code-audit":
        return validate_failure_code_audit(path, root, payload)
    if family == "b64ag-golden-recapture-readiness":
        return validate_readiness(path, root, payload)
    if family == "m6-publication-qualifier-hook":
        return validate_qualifier_hook(path, root, payload)
    if family == "runtime-evidence":
        return validate_evidence(path, root, payload)

    label = str(path.relative_to(root))
    require_schema_version(payload, label)
    return SidecarValidation(label, family, False, "schema-only")


def validate_sqlite_sidecar(path: Path, root: Path) -> SidecarValidation:
    label = str(path.relative_to(root))
    try:
        connection = sqlite3.connect(f"file:{path}?mode=ro", uri=True)
    except sqlite3.Error as error:
        raise SchemaError(f"{label} could not be opened as SQLite: {error}") from error
    try:
        quick_check = connection.execute("PRAGMA quick_check").fetchone()
        expect(quick_check is not None and quick_check[0] == "ok", f"{label} failed PRAGMA quick_check")
        connection.execute("SELECT name, type FROM sqlite_master LIMIT 1").fetchall()
    except sqlite3.Error as error:
        raise SchemaError(f"{label} has unreadable SQLite schema metadata: {error}") from error
    finally:
        connection.close()
    return SidecarValidation(label, "sqlite-sidecar", True, "quick_check=ok")


def validate_all_m6_sidecars(root: Path, m6_root: Path) -> dict[str, Any]:
    absolute_root = resolve_m6_root(root, m6_root)
    json_paths = sorted(absolute_root.rglob("*.json"))
    sqlite_paths = sorted(
        path for path in absolute_root.rglob("*") if path.is_file() and path.suffix in SQLITE_SUFFIXES
    )
    expect(json_paths or sqlite_paths, f"{m6_root} must contain JSON or SQLite sidecars")

    validations: list[SidecarValidation] = []
    errors: list[str] = []
    for path in json_paths:
        try:
            validations.append(validate_json_sidecar(path, root))
        except Exception as error:  # noqa: BLE001 - collect all schema drift before failing.
            errors.append(f"{path.relative_to(root)}: {error}")
    for path in sqlite_paths:
        try:
            validations.append(validate_sqlite_sidecar(path, root))
        except Exception as error:  # noqa: BLE001 - collect all schema drift before failing.
            errors.append(f"{path.relative_to(root)}: {error}")
    if errors:
        raise SchemaError("M6 sidecar shape validation failed:\n" + "\n".join(errors))

    family_counts: dict[str, int] = {}
    accepted_paths: list[str] = []
    for validation in validations:
        family_counts[validation.family] = family_counts.get(validation.family, 0) + 1
        if validation.accepted:
            accepted_paths.append(validation.path)

    return {
        "schema_version": 1,
        "m6_root": str(m6_root),
        "json_sidecar_count": len(json_paths),
        "sqlite_sidecar_count": len(sqlite_paths),
        "accepted_sidecar_count": len(accepted_paths),
        "families": dict(sorted(family_counts.items())),
        "accepted_sidecars": sorted(accepted_paths),
    }


def run_self_check() -> dict[str, Any]:
    with tempfile.TemporaryDirectory() as temp_dir:
        root = Path(temp_dir)
        lane = root / M6_ROOT / "lane-self-check"
        golden = lane / "sample.amflow-golden.txt"
        golden.parent.mkdir(parents=True, exist_ok=True)
        golden.write_text("sample golden\n", encoding="utf-8")
        write_json(
            lane / "sample.cpp-result.json",
            {
                "schema_version": 1,
                "benchmark_id": "sample",
                "family": "sample",
                "targets": [{"integral": "sample[1]"}],
                "solver": {"method": "synthetic"},
                "boundary_state": {"source": "synthetic"},
                "continuation": {"path": []},
                "results": [{"integral": "sample[1]", "orders": []}],
                "status": "success",
                "duration_seconds": 0.0,
            },
        )
        write_json(
            lane / "sample.compare30.json",
            {
                "schema_version": 1,
                "benchmark_id": "sample",
                "comparison": "cpp-vs-amflow",
                "cpp_result": str((lane / "sample.cpp-result.json").relative_to(root)),
                "amflow_golden": str(golden.relative_to(root)),
                "compared_coefficient_count": 1,
                "passed_coefficient_count": 1,
                "minimum_digit_agreement": 30,
                "tolerance_digits": 30,
                "matched_integral_count": 1,
                "integrals": [{"integral": "sample[1]"}],
                "failures": [],
                "passed": True,
            },
        )

        sqlite_path = lane / "sample.sqlite"
        connection = sqlite3.connect(sqlite_path)
        try:
            connection.execute("CREATE TABLE sample(id INTEGER PRIMARY KEY)")
            connection.commit()
        finally:
            connection.close()

        summary = validate_all_m6_sidecars(root, M6_ROOT)

        bad_schema_rejected = False
        bad_schema_path = lane / "bad-evidence.json"
        write_json(
            bad_schema_path,
            {
                "schema_version": 2,
                "benchmark_id": "sample",
            },
        )
        try:
            validate_json_sidecar(bad_schema_path, root)
        except SchemaError:
            bad_schema_rejected = True

        escaped_path_rejected = False
        escaped_path = lane / "escaped.compare30.json"
        write_json(
            escaped_path,
            {
                "schema_version": 1,
                "benchmark_id": "sample",
                "comparison": "cpp-vs-amflow",
                "cpp_result": "../outside.json",
                "amflow_golden": str(golden.relative_to(root)),
                "compared_coefficient_count": 1,
                "passed_coefficient_count": 1,
                "minimum_digit_agreement": 30,
                "tolerance_digits": 30,
                "matched_integral_count": 1,
                "integrals": [{"integral": "sample[1]"}],
                "failures": [],
                "passed": True,
            },
        )
        try:
            validate_json_sidecar(escaped_path, root)
        except SchemaError:
            escaped_path_rejected = True

        escaped_m6_root_rejected = False
        with tempfile.TemporaryDirectory(
            dir=root.parent,
            prefix=f"{root.name}-outside-m6-",
        ) as outside_dir:
            outside_root = Path(outside_dir)
            outside_lane = outside_root / "lane-external"
            write_json(
                outside_lane / "external.cpp-result.json",
                {
                    "schema_version": 1,
                    "benchmark_id": "external",
                    "family": "external",
                    "targets": [{"integral": "external[1]"}],
                    "solver": {"method": "synthetic"},
                    "boundary_state": {"source": "synthetic"},
                    "continuation": {"path": []},
                    "results": [{"integral": "external[1]", "orders": []}],
                    "status": "success",
                    "duration_seconds": 0.0,
                },
            )
            try:
                validate_all_m6_sidecars(root, outside_root)
            except SchemaError:
                escaped_m6_root_rejected = True

        expect(summary["json_sidecar_count"] == 2, "self-check JSON sidecar count drifted")
        expect(summary["sqlite_sidecar_count"] == 1, "self-check SQLite sidecar count drifted")
        expect(summary["accepted_sidecar_count"] == 3, "self-check accepted sidecar count drifted")
        expect(bad_schema_rejected, "self-check did not reject a bad schema version")
        expect(escaped_path_rejected, "self-check did not reject an escaped repository path")
        expect(escaped_m6_root_rejected, "self-check did not reject an escaped M6 root")

        return {
            "valid_json_sidecars": summary["json_sidecar_count"],
            "valid_sqlite_sidecars": summary["sqlite_sidecar_count"],
            "accepted_sidecars": summary["accepted_sidecar_count"],
            "bad_schema_rejected": bad_schema_rejected,
            "escaped_path_rejected": escaped_path_rejected,
            "escaped_m6_root_rejected": escaped_m6_root_rejected,
        }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--m6-root",
        default=str(M6_ROOT),
        help="Repository-relative M6 sidecar root to validate.",
    )
    parser.add_argument(
        "--format",
        choices=("text", "json"),
        default="text",
        help="Summary output format.",
    )
    parser.add_argument(
        "--verify",
        action="store_true",
        help="Validate sidecars and exit nonzero on drift. This is the default behavior.",
    )
    parser.add_argument(
        "--self-check",
        action="store_true",
        help="Run synthetic validator regression checks.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.self_check:
        print(json.dumps(run_self_check(), indent=2, sort_keys=True))
        return 0

    root = repo_root()
    summary = validate_all_m6_sidecars(root, Path(args.m6_root))
    if args.format == "json":
        print(json.dumps(summary, indent=2, sort_keys=True))
    else:
        counts = ", ".join(
            f"{family}={count}" for family, count in sorted(summary["families"].items())
        )
        print(
            "M6 sidecar shape validation passed: "
            f"{summary['json_sidecar_count']} JSON sidecars, "
            f"{summary['sqlite_sidecar_count']} SQLite sidecars, "
            f"{summary['accepted_sidecar_count']} accepted sidecars ({counts})"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
