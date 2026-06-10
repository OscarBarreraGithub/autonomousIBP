#!/usr/bin/env python3
"""Report stale or contradictory metadata across committed M6 sidecars."""

from __future__ import annotations

import argparse
import json
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from validate_m6_sidecar_shapes import (
    M6_ROOT,
    SQLITE_SUFFIXES,
    SchemaError,
    SidecarValidation,
    repo_root,
    resolve_m6_root,
    validate_json_sidecar,
    validate_sqlite_sidecar,
    write_json,
)


WITHHELD_CLAIMS: tuple[str, ...] = (
    "This report reads committed M6 sidecars only; it does not create new retained evidence.",
    "This report does not run AMFlow numerics, close runtime lanes, or claim Milestone M6 readiness.",
)

EXPECTED_UNACCEPTED_M6_SIDECARS: tuple[str, ...] = (
    "tools/reference-harness/specs/m6/lane1-next11/"
    "complex_kinematics.two-constant-long-timeout.eps0.compare50.json",
    "tools/reference-harness/specs/m6/lane1-next8/"
    "complex_kinematics.two-constant-long-timeout.eps0.compare50.json",
    "tools/reference-harness/specs/m6/lane124/b61n-runtime-evidence.json",
    "tools/reference-harness/specs/m6/lane142/b61n-selected5-real-coefficients-evidence.json",
    "tools/reference-harness/specs/m6/lane146/b63n-d246-weighted-residue-reference-evidence.json",
    "tools/reference-harness/specs/m6/lane5-next7/b61n-publication-amflow-cross-comparator.blocked.json",
    "tools/reference-harness/specs/m6/lane5-next7/b61n-publication-qualifier-hook.json",
    "tools/reference-harness/specs/m6/lane6-iter13/"
    "complex_kinematics.rk78-pole-aware.stripped.eps0.compare50.json",
)


class DriftError(RuntimeError):
    """Raised when an M6 sidecar drift report cannot be produced honestly."""


@dataclass(frozen=True)
class DriftEntry:
    path: str
    family: str
    benchmark_id: str
    lane: str
    accepted: bool
    status: str
    basis: str


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise DriftError(message)


def read_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as stream:
        payload = json.load(stream)
    expect(isinstance(payload, dict), f"{path} must contain a JSON object")
    return payload


def bool_token(value: bool) -> str:
    return "true" if value else "false"


def clean_str(value: Any) -> str | None:
    if isinstance(value, str) and value.strip():
        return value.strip()
    return None


def lane_from_path(path: Path, m6_root: Path) -> str:
    parts = path.relative_to(m6_root).parts
    return parts[0] if len(parts) > 1 else "<m6-root>"


def benchmark_id(payload: dict[str, Any]) -> str:
    return clean_str(payload.get("benchmark_id")) or "<unscoped>"


def embedded_bool_status(payload: dict[str, Any], field: str, label: str) -> str | None:
    value = payload.get(field)
    if type(value) is bool:
        return f"{label}={bool_token(value)}"
    return None


def object_bool_status(
    payload: dict[str, Any],
    object_field: str,
    bool_field: str,
    label: str,
) -> str | None:
    nested = payload.get(object_field)
    if isinstance(nested, dict) and type(nested.get(bool_field)) is bool:
        return f"{label}={bool_token(nested[bool_field])}"
    return None


def metadata_status(payload: dict[str, Any], validation: SidecarValidation) -> str:
    status = clean_str(payload.get("status"))
    if status is not None:
        return f"status={status}"

    for candidate in (
        embedded_bool_status(payload, "passed", "passed"),
        embedded_bool_status(payload, "golden_recapture_ready", "golden_recapture_ready"),
        object_bool_status(payload, "real_output_audit", "comparison_passed", "comparison_passed"),
        object_bool_status(payload, "comparator", "passed", "comparator.passed"),
        object_bool_status(payload, "comparison_summary", "passed", "comparison_summary.passed"),
    ):
        if candidate is not None:
            return candidate

    qualifier_pass = payload.get("qualifier_pass")
    if isinstance(qualifier_pass, dict):
        qualifier_status = clean_str(qualifier_pass.get("status"))
        if qualifier_status is not None:
            return f"qualifier_pass.status={qualifier_status}"

    return validation.basis


def entry_from_validation(
    path: Path,
    root: Path,
    absolute_m6_root: Path,
    validation: SidecarValidation,
    payload: dict[str, Any] | None,
) -> DriftEntry:
    if payload is None:
        scoped_benchmark = "<sqlite>"
        status = validation.basis
    else:
        scoped_benchmark = benchmark_id(payload)
        status = metadata_status(payload, validation)
    return DriftEntry(
        path=str(path.relative_to(root)),
        family=validation.family,
        benchmark_id=scoped_benchmark,
        lane=lane_from_path(path, absolute_m6_root),
        accepted=validation.accepted,
        status=status,
        basis=validation.basis,
    )


def collect_entries(root: Path, m6_root: Path) -> tuple[list[DriftEntry], int, int]:
    absolute_root = resolve_m6_root(root, m6_root)
    json_paths = sorted(absolute_root.rglob("*.json"))
    sqlite_paths = sorted(
        path for path in absolute_root.rglob("*") if path.is_file() and path.suffix in SQLITE_SUFFIXES
    )
    expect(json_paths or sqlite_paths, f"{m6_root} must contain JSON or SQLite sidecars")

    entries: list[DriftEntry] = []
    errors: list[str] = []
    for path in json_paths:
        try:
            payload = read_json(path)
            validation = validate_json_sidecar(path, root)
            entries.append(entry_from_validation(path, root, absolute_root, validation, payload))
        except Exception as error:  # noqa: BLE001 - report all sidecar drift at once.
            errors.append(f"{path.relative_to(root)}: {error}")

    for path in sqlite_paths:
        try:
            validation = validate_sqlite_sidecar(path, root)
            entries.append(entry_from_validation(path, root, absolute_root, validation, None))
        except Exception as error:  # noqa: BLE001 - report all sidecar drift at once.
            errors.append(f"{path.relative_to(root)}: {error}")

    if errors:
        raise DriftError("M6 sidecar drift audit failed shape validation:\n" + "\n".join(errors))
    return entries, len(json_paths), len(sqlite_paths)


def entry_payload(entry: DriftEntry) -> dict[str, Any]:
    return {
        "path": entry.path,
        "lane": entry.lane,
        "accepted": entry.accepted,
        "status": entry.status,
        "basis": entry.basis,
    }


def drift_group_payload(
    family: str,
    benchmark: str,
    group_entries: list[DriftEntry],
) -> dict[str, Any]:
    accepted = [entry for entry in group_entries if entry.accepted]
    unaccepted = [entry for entry in group_entries if not entry.accepted]
    return {
        "family": family,
        "benchmark_id": benchmark,
        "sidecar_count": len(group_entries),
        "accepted_count": len(accepted),
        "unaccepted_count": len(unaccepted),
        "lanes": sorted({entry.lane for entry in group_entries}),
        "status_values": sorted({entry.status for entry in group_entries}),
        "accepted_sidecars": [entry_payload(entry) for entry in accepted],
        "unaccepted_sidecars": [entry_payload(entry) for entry in unaccepted],
    }


def build_report(root: Path, m6_root: Path) -> dict[str, Any]:
    entries, json_count, sqlite_count = collect_entries(root, m6_root)
    paths = [entry.path for entry in entries]
    expect(len(paths) == len(set(paths)), "M6 drift inventory must not contain duplicate paths")

    groups: dict[tuple[str, str], list[DriftEntry]] = {}
    for entry in entries:
        groups.setdefault((entry.family, entry.benchmark_id), []).append(entry)

    drift_groups = [
        drift_group_payload(family, benchmark, sorted(group, key=lambda entry: entry.path))
        for (family, benchmark), group in sorted(groups.items())
        if any(entry.accepted for entry in group) and any(not entry.accepted for entry in group)
    ]
    stale_unaccepted = sum(group["unaccepted_count"] for group in drift_groups)
    accepted_count = sum(1 for entry in entries if entry.accepted)
    unaccepted_entries = sorted(
        (entry for entry in entries if not entry.accepted),
        key=lambda entry: entry.path,
    )
    unaccepted_count = len(unaccepted_entries)
    return {
        "schema_version": 1,
        "m6_root": str(m6_root),
        "status": "drift-observed" if drift_groups else "clean",
        "sidecar_count": len(entries),
        "json_sidecar_count": json_count,
        "sqlite_sidecar_count": sqlite_count,
        "accepted_count": accepted_count,
        "unaccepted_count": unaccepted_count,
        "unaccepted_sidecars": [entry_payload(entry) for entry in unaccepted_entries],
        "drift_group_count": len(drift_groups),
        "stale_unaccepted_sidecar_count": stale_unaccepted,
        "drift_groups": drift_groups,
        "withheld_claims": list(WITHHELD_CLAIMS),
    }


def verify_report(report: dict[str, Any]) -> None:
    sidecar_count = report.get("sidecar_count")
    json_count = report.get("json_sidecar_count")
    sqlite_count = report.get("sqlite_sidecar_count")
    accepted_count = report.get("accepted_count")
    unaccepted_count = report.get("unaccepted_count")
    drift_group_count = report.get("drift_group_count")
    stale_unaccepted = report.get("stale_unaccepted_sidecar_count")
    for field, value in (
        ("sidecar_count", sidecar_count),
        ("json_sidecar_count", json_count),
        ("sqlite_sidecar_count", sqlite_count),
        ("accepted_count", accepted_count),
        ("unaccepted_count", unaccepted_count),
        ("drift_group_count", drift_group_count),
        ("stale_unaccepted_sidecar_count", stale_unaccepted),
    ):
        expect(type(value) is int and value >= 0, f"{field} must be a nonnegative integer")
    expect(sidecar_count == json_count + sqlite_count, "sidecar type counts do not sum")
    expect(sidecar_count == accepted_count + unaccepted_count, "accepted/unaccepted counts do not sum")
    expect(stale_unaccepted <= unaccepted_count, "stale unaccepted count exceeds unaccepted total")
    unaccepted_sidecars = report.get("unaccepted_sidecars")
    expect(isinstance(unaccepted_sidecars, list), "unaccepted_sidecars must be a list")
    expect(
        len(unaccepted_sidecars) == unaccepted_count,
        "unaccepted_sidecars length does not match unaccepted_count",
    )
    for index, sidecar in enumerate(unaccepted_sidecars):
        expect(isinstance(sidecar, dict), f"unaccepted_sidecars[{index}] must be an object")
        expect(
            isinstance(sidecar.get("path"), str) and sidecar["path"].strip(),
            f"unaccepted_sidecars[{index}].path must be a non-empty string",
        )
    drift_groups = report.get("drift_groups")
    expect(isinstance(drift_groups, list), "drift_groups must be a list")
    expect(drift_group_count == len(drift_groups), "drift_group_count does not match drift_groups")
    expected_status = "drift-observed" if drift_groups else "clean"
    expect(report.get("status") == expected_status, "status does not match drift group state")


def render_text(report: dict[str, Any], *, summary_only: bool) -> str:
    lines = [
        "M6 sidecar drift report",
        f"status: {str(report['status']).upper()}",
        (
            "inventory: "
            f"total={report['sidecar_count']} "
            f"json={report['json_sidecar_count']} "
            f"sqlite={report['sqlite_sidecar_count']} "
            f"accepted={report['accepted_count']} "
            f"unaccepted={report['unaccepted_count']}"
        ),
        (
            "drift: "
            f"groups={report['drift_group_count']} "
            f"stale_unaccepted_sidecars={report['stale_unaccepted_sidecar_count']}"
        ),
    ]
    if not summary_only:
        for group in report["drift_groups"]:
            lines.append(
                "group: "
                f"family={group['family']} "
                f"benchmark_id={group['benchmark_id']} "
                f"accepted={group['accepted_count']} "
                f"unaccepted={group['unaccepted_count']} "
                f"lanes={','.join(group['lanes'])}"
            )
            for sidecar in group["unaccepted_sidecars"]:
                lines.append(
                    "  stale: "
                    f"{sidecar['path']} ({sidecar['status']}; {sidecar['basis']})"
                )
    lines.append("withheld_claims: " + " ".join(WITHHELD_CLAIMS))
    return "\n".join(lines)


def verify_no_drift(report: dict[str, Any]) -> None:
    if report["drift_group_count"] != 0:
        paths = [
            sidecar["path"]
            for group in report["drift_groups"]
            for sidecar in group["unaccepted_sidecars"]
        ]
        raise DriftError(
            "M6 sidecar drift observed; stale unaccepted sidecars remain:\n"
            + "\n".join(paths)
        )


def verify_unaccepted_sidecars_not_promoted(
    report: dict[str, Any],
    *,
    expected_paths: tuple[str, ...] = EXPECTED_UNACCEPTED_M6_SIDECARS,
) -> None:
    unaccepted_sidecars = report.get("unaccepted_sidecars")
    expect(isinstance(unaccepted_sidecars, list), "unaccepted_sidecars must be a list")
    observed_paths = {
        str(sidecar.get("path", "")).strip()
        for sidecar in unaccepted_sidecars
        if isinstance(sidecar, dict)
    }
    expected = set(expected_paths)
    missing = sorted(expected - observed_paths)
    if missing:
        raise DriftError(
            "Expected unaccepted M6 sidecars were promoted, removed, or renamed:\n"
            + "\n".join(missing)
        )
    expect(
        len(expected_paths) == len(expected),
        "expected unaccepted M6 sidecar guard paths must be unique",
    )


def run_self_check() -> dict[str, Any]:
    with tempfile.TemporaryDirectory() as temp_dir:
        root = Path(temp_dir)
        old_lane = root / M6_ROOT / "lane-old"
        new_lane = root / M6_ROOT / "lane-new"
        stale_sidecar = old_lane / "sample-runtime-evidence.json"
        accepted_sidecar = new_lane / "sample-runtime-evidence.json"
        write_json(
            stale_sidecar,
            {
                "schema_version": 1,
                "benchmark_id": "sample",
                "status": "blocked-pending",
                "passed": False,
            },
        )
        write_json(
            accepted_sidecar,
            {
                "schema_version": 1,
                "benchmark_id": "sample",
                "status": "accepted",
                "passed": True,
            },
        )
        write_json(
            new_lane / "other-runtime-evidence.json",
            {
                "schema_version": 1,
                "benchmark_id": "other",
                "status": "accepted",
                "passed": True,
            },
        )

        report = build_report(root, M6_ROOT)
        verify_report(report)
        no_drift_rejected = False
        try:
            verify_no_drift(report)
        except DriftError:
            no_drift_rejected = True
        promotion_guard_passed = False
        verify_unaccepted_sidecars_not_promoted(
            report,
            expected_paths=(str(stale_sidecar.relative_to(root)),),
        )
        promotion_guard_passed = True
        promoted_sidecar_rejected = False
        try:
            verify_unaccepted_sidecars_not_promoted(
                report,
                expected_paths=(str(accepted_sidecar.relative_to(root)),),
            )
        except DriftError:
            promoted_sidecar_rejected = True
        expect(report["status"] == "drift-observed", "self-check must observe drift")
        expect(report["sidecar_count"] == 3, "self-check sidecar count drifted")
        expect(report["accepted_count"] == 2, "self-check accepted count drifted")
        expect(report["unaccepted_count"] == 1, "self-check unaccepted count drifted")
        expect(report["drift_group_count"] == 1, "self-check drift group count drifted")
        expect(report["stale_unaccepted_sidecar_count"] == 1, "self-check stale count drifted")
        expect(no_drift_rejected, "self-check verify_no_drift did not reject drift")
        expect(promotion_guard_passed, "self-check promotion guard did not pass stale sidecar")
        expect(
            promoted_sidecar_rejected,
            "self-check promotion guard did not reject accepted sidecar",
        )
        return {
            "sidecar_count": report["sidecar_count"],
            "accepted_count": report["accepted_count"],
            "unaccepted_count": report["unaccepted_count"],
            "drift_group_count": report["drift_group_count"],
            "stale_unaccepted_sidecar_count": report["stale_unaccepted_sidecar_count"],
            "no_drift_rejected": no_drift_rejected,
            "promotion_guard_passed": promotion_guard_passed,
            "promoted_sidecar_rejected": promoted_sidecar_rejected,
        }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--m6-root",
        default=str(M6_ROOT),
        help="Repository-relative M6 sidecar root to audit.",
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
        help="Verify report invariants after validating sidecar shapes.",
    )
    parser.add_argument(
        "--verify-no-drift",
        action="store_true",
        help="Fail if accepted and unaccepted sidecars coexist for the same benchmark/family.",
    )
    parser.add_argument(
        "--verify-unaccepted-not-promoted",
        action="store_true",
        help="Fail if any pinned unaccepted M6 sidecar validates as accepted.",
    )
    parser.add_argument(
        "--self-check",
        action="store_true",
        help="Run synthetic drift-detection regression checks.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        if args.self_check:
            print(json.dumps(run_self_check(), indent=2, sort_keys=True))
            return 0

        report = build_report(repo_root(), Path(args.m6_root))
        if args.verify:
            verify_report(report)
        if args.verify_no_drift:
            verify_no_drift(report)
        if args.verify_unaccepted_not_promoted:
            verify_unaccepted_sidecars_not_promoted(report)
    except (DriftError, SchemaError) as error:
        print(str(error), file=sys.stderr)
        return 1

    if args.format == "json":
        print(json.dumps(report, indent=2, sort_keys=True))
    else:
        print(render_text(report, summary_only=args.summary_only))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
