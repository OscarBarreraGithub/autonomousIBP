#!/usr/bin/env python3
"""Verify accepted M5, M6, and M7 release cross-links."""

from __future__ import annotations

import argparse
import copy
import json
from pathlib import Path
from typing import Any, Callable

from audit_m7_sidecar_inventory import (
    M7_ROOT,
    build_inventory,
    verify_inventory,
    verify_schema_reconciliation,
)
from package_m7_release_evidence import (
    DEFAULT_BUNDLE_ROOT,
    DEFAULT_M5_ACCEPTANCE_SIDECAR,
    DEFAULT_READINESS_SIDECAR,
    READINESS_INPUT_PATH_FIELDS,
    build_manifest,
    validate_manifest_shape,
)
from validate_m7_release_sidecar_schemas import require_m7_root, validate_m7_sidecar


M5_ROOT = Path("tools/reference-harness/specs/m5")
M5_ACCEPTANCE_SCHEMA = "m7-prerequisite-m5-packet-acceptance"
M6_QUALIFICATION_SCHEMA = "milestone-m6-qualification"
M7_READINESS_SCHEMA = "release-readiness-output"
M7_PARITY_SIGNOFF_SCHEMA = "release-parity-signoff"
ACCEPTED_M5_STATE = "CLOSED/all-phase"
ACCEPTED_M5_PREREQ_STATE = "reviewed-and-accepted-m5-packet"
ACCEPTED_M6_STATE = "milestone-m6-qualified"
ACCEPTED_M6_PREREQ_STATE = "reviewed-and-accepted-m6-packet"
ACCEPTED_PARITY_STATE = "parity-signoff-reviewed"


class CrossLinkError(RuntimeError):
    """Raised when accepted M5/M6/M7 release cross-links drift."""


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise CrossLinkError(message)


def read_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as stream:
        payload = json.load(stream)
    expect(isinstance(payload, dict), f"{path} must contain a JSON object")
    return payload


def require_repo_file(root: Path, raw: Any, label: str) -> str:
    expect(isinstance(raw, str) and raw.strip(), f"{label} must be a non-empty path")
    expect(raw == raw.strip(), f"{label} must not carry surrounding whitespace")
    candidate = Path(raw)
    if not candidate.is_absolute():
        candidate = root / candidate
    try:
        relative = candidate.resolve(strict=False).relative_to(root.resolve(strict=True))
    except ValueError as error:
        raise CrossLinkError(f"{label} must stay within the repository: {raw}") from error
    expect(candidate.is_file(), f"{label} does not exist as a file: {raw}")
    return relative.as_posix()


def require_bool(payload: dict[str, Any], field: str, label: str) -> bool:
    value = payload.get(field)
    expect(type(value) is bool, f"{label} {field} must be a bool")
    return value


def require_string_list(payload: dict[str, Any], field: str, label: str) -> list[str]:
    raw = payload.get(field)
    expect(isinstance(raw, list), f"{label} {field} must be a list")
    values: list[str] = []
    for index, item in enumerate(raw):
        expect(
            isinstance(item, str) and item.strip(),
            f"{label} {field}[{index}] must be a non-empty string",
        )
        expect(item == item.strip(), f"{label} {field}[{index}] must not carry whitespace")
        values.append(item)
    expect(len(values) == len(set(values)), f"{label} {field} must not contain duplicates")
    return values


def require_path_field(
    root: Path,
    payload: dict[str, Any],
    field: str,
    label: str,
) -> str:
    return require_repo_file(root, payload.get(field), f"{label} {field}")


def require_schema(root: Path, relative_path: str, expected_schema: str, label: str) -> None:
    observed = validate_m7_sidecar(root / relative_path, root)
    expect(
        observed == expected_schema,
        f"{label} schema drifted for {relative_path}: expected {expected_schema}, got {observed}",
    )


def inventory_by_schema(root: Path) -> dict[str, dict[str, str]]:
    m7_root = require_m7_root(root, M7_ROOT)
    entries = build_inventory(root, m7_root)
    verify_inventory(entries)
    verify_schema_reconciliation(root, m7_root, entries)
    accepted: dict[str, dict[str, str]] = {}
    for entry in entries:
        if entry.status != "accepted":
            continue
        accepted.setdefault(entry.schema, {})[entry.path] = entry.basis
    return accepted


def discover_closed_m5_packets(root: Path) -> dict[str, dict[str, Any]]:
    packets: dict[str, dict[str, Any]] = {}
    for path in sorted((root / M5_ROOT).glob("m5-qualification-*.json")):
        relative = path.relative_to(root).as_posix()
        payload = read_json(path)
        if (
            payload.get("scope") == "m5-all-phase"
            and payload.get("current_state") == ACCEPTED_M5_STATE
            and payload.get("m5_all_phase_closed") is True
        ):
            packets[relative] = payload
    expect(packets, "no accepted M5 all-phase qualification packets were found")
    return packets


def require_closed_m5_packet(relative_path: str, payload: dict[str, Any]) -> None:
    label = relative_path
    expect(payload.get("scope") == "m5-all-phase", f"{label} scope is not m5-all-phase")
    expect(payload.get("current_state") == ACCEPTED_M5_STATE, f"{label} is not all-phase closed")
    expect(payload.get("m5_all_phase_closed") is True, f"{label} m5_all_phase_closed is not true")
    expect(payload.get("does_not_claim_m6") is True, f"{label} widened into M6 claims")
    expect(payload.get("does_not_claim_m7") is True, f"{label} widened into M7 claims")
    expect(
        payload.get("does_not_claim_release_readiness") is True,
        f"{label} widened into release-readiness claims",
    )


def require_prerequisite(
    readiness_payload: dict[str, Any],
    prerequisite_id: str,
    expected_state: str,
    label: str,
) -> None:
    prereqs = readiness_payload.get("release_prerequisites")
    expect(isinstance(prereqs, list), f"{label} release_prerequisites must be a list")
    matches = [
        entry
        for entry in prereqs
        if isinstance(entry, dict) and entry.get("id") == prerequisite_id
    ]
    expect(len(matches) == 1, f"{label} must carry one {prerequisite_id} prerequisite")
    prerequisite = matches[0]
    expect(
        prerequisite.get("current_state") == expected_state,
        f"{label} {prerequisite_id} current_state drifted",
    )
    expect(prerequisite.get("satisfied") is True, f"{label} {prerequisite_id} is not satisfied")
    expect(prerequisite.get("blockers") == [], f"{label} {prerequisite_id} has blockers")


def verify_m6_summary(root: Path, relative_path: str) -> None:
    require_schema(root, relative_path, M6_QUALIFICATION_SCHEMA, "M6 qualification")
    payload = read_json(root / relative_path)
    expect(payload.get("scope") == M6_QUALIFICATION_SCHEMA, f"{relative_path} scope drifted")
    expect(
        payload.get("current_state") == ACCEPTED_M6_STATE,
        f"{relative_path} is not milestone-m6-qualified",
    )
    expect(payload.get("milestone_m6_ready") is True, f"{relative_path} is not M6-ready")
    expect(payload.get("blocking_reasons") == [], f"{relative_path} has blocking_reasons")
    for field in ("phase0_qualification_summary_path", "case_study_qualification_summary_path"):
        require_path_field(root, payload, field, relative_path)
    withheld = require_string_list(payload, "withheld_claims", relative_path)
    expect(
        any("does not mark Milestone M7 or release readiness" in claim for claim in withheld),
        f"{relative_path} no longer withholds M7/release readiness claims",
    )


def verify_parity_signoff(
    root: Path,
    parity_path: str,
    expected_m6_path: str,
    accepted_parity_signoffs: dict[str, str],
) -> None:
    expect(
        parity_path in accepted_parity_signoffs,
        f"M6 summary {expected_m6_path} does not reference an accepted M7 parity signoff: {parity_path}",
    )
    require_schema(root, parity_path, M7_PARITY_SIGNOFF_SCHEMA, "M7 parity signoff")
    payload = read_json(root / parity_path)
    expect(payload.get("current_state") == ACCEPTED_PARITY_STATE, f"{parity_path} state drifted")
    expect(payload.get("parity_signoff_complete") is True, f"{parity_path} is not complete")
    expect(
        payload.get("m6_qualification_summary_path") == expected_m6_path,
        f"{parity_path} does not point back to {expected_m6_path}",
    )
    expect(payload.get("blocking_reasons") == [], f"{parity_path} has blocking_reasons")


def verify_readiness_output(
    root: Path,
    readiness_path: str,
    readiness_payload: dict[str, Any],
    *,
    accepted_m5_packets: dict[str, dict[str, Any]],
    accepted_m6_summaries: dict[str, str],
    accepted_parity_signoffs: dict[str, str],
) -> tuple[str, str, str]:
    label = readiness_path
    expect(readiness_payload.get("release_signoff_ready") is True, f"{label} is not release-ready")
    expect(readiness_payload.get("release_signoff_blockers") == [], f"{label} has signoff blockers")

    m5_path = require_path_field(root, readiness_payload, "m5_qualification_summary_path", label)
    expect(m5_path in accepted_m5_packets, f"{label} does not reference an accepted M5 packet")
    require_closed_m5_packet(m5_path, accepted_m5_packets[m5_path])
    require_prerequisite(readiness_payload, "phase-f-feature-parity", ACCEPTED_M5_PREREQ_STATE, label)

    m6_path = require_path_field(root, readiness_payload, "m6_qualification_summary_path", label)
    expect(m6_path in accepted_m6_summaries, f"{label} does not reference an accepted M6 summary")
    verify_m6_summary(root, m6_path)
    require_prerequisite(readiness_payload, "milestone-m6", ACCEPTED_M6_PREREQ_STATE, label)

    parity_path = require_path_field(root, readiness_payload, "parity_signoff_summary_path", label)
    verify_parity_signoff(root, parity_path, m6_path, accepted_parity_signoffs)
    return m5_path, m6_path, parity_path


def verify_m5_acceptance(
    root: Path,
    acceptance_path: str,
    acceptance_payload: dict[str, Any],
    *,
    accepted_m5_packets: dict[str, dict[str, Any]],
    accepted_m6_summaries: dict[str, str],
    accepted_readiness_outputs: dict[str, str],
    accepted_parity_signoffs: dict[str, str],
) -> tuple[str, str]:
    label = acceptance_path
    expect(
        acceptance_payload.get("accepted_for_release_prerequisite") is True,
        f"{label} is not accepted_for_release_prerequisite",
    )
    expect(acceptance_payload.get("m5_packet_reviewed") is True, f"{label} M5 packet is not reviewed")
    expect(
        acceptance_payload.get("current_state") == ACCEPTED_M5_PREREQ_STATE,
        f"{label} current_state drifted",
    )
    expect(acceptance_payload.get("blocking_reasons") == [], f"{label} has blocking_reasons")

    m5_path = require_path_field(root, acceptance_payload, "m5_packet_path", label)
    expect(m5_path in accepted_m5_packets, f"{label} does not point at an accepted M5 packet")
    require_closed_m5_packet(m5_path, accepted_m5_packets[m5_path])

    evidence_paths = require_string_list(acceptance_payload, "evidence_paths", label)
    expect(m5_path in evidence_paths, f"{label} evidence_paths omits its M5 packet")

    readiness_path = require_path_field(root, acceptance_payload, "release_readiness_output_path", label)
    expect(readiness_path in evidence_paths, f"{label} evidence_paths omits release readiness output")
    expect(
        readiness_path in accepted_readiness_outputs,
        f"{label} does not point at an accepted M7 release-readiness output",
    )
    require_schema(root, readiness_path, M7_READINESS_SCHEMA, "M7 release readiness")
    readiness_payload = read_json(root / readiness_path)
    linked_m5_path, linked_m6_path, _parity_path = verify_readiness_output(
        root,
        readiness_path,
        readiness_payload,
        accepted_m5_packets=accepted_m5_packets,
        accepted_m6_summaries=accepted_m6_summaries,
        accepted_parity_signoffs=accepted_parity_signoffs,
    )
    expect(
        linked_m5_path == m5_path,
        f"{label} M5 packet does not match {readiness_path} m5_qualification_summary_path",
    )
    return m5_path, linked_m6_path


def required_bundle_paths(
    root: Path,
    readiness_path: str,
    m5_acceptance_paths: list[str],
    m6_summary_paths: list[str],
) -> set[str]:
    readiness_payload = read_json(root / readiness_path)
    required = {readiness_path}
    for field in READINESS_INPUT_PATH_FIELDS:
        required.add(require_path_field(root, readiness_payload, field, readiness_path))

    for acceptance_path in m5_acceptance_paths:
        required.add(acceptance_path)
        acceptance_payload = read_json(root / acceptance_path)
        required.add(require_path_field(root, acceptance_payload, "m5_packet_path", acceptance_path))
        required.add(
            require_path_field(root, acceptance_payload, "release_readiness_output_path", acceptance_path)
        )
        for index, raw_path in enumerate(require_string_list(acceptance_payload, "evidence_paths", acceptance_path)):
            relative = require_repo_file(root, raw_path, f"{acceptance_path} evidence_paths[{index}]")
            if relative.endswith(".json"):
                required.add(relative)

    required.update(m6_summary_paths)
    return required


def require_bundle_includes_cross_link_surfaces(
    manifest: dict[str, Any],
    required_paths: set[str],
) -> None:
    files = validate_manifest_shape(manifest)
    manifest_paths = {entry["path"] for entry in files}
    missing = sorted(required_paths.difference(manifest_paths))
    expect(
        not missing,
        "M7 release evidence bundle is missing accepted M5/M6/M7 surface(s): "
        + ", ".join(missing),
    )


def verify(root: Path) -> dict[str, Any]:
    accepted_by_schema = inventory_by_schema(root)
    accepted_m5_packets = discover_closed_m5_packets(root)
    accepted_m5_acceptances = accepted_by_schema.get(M5_ACCEPTANCE_SCHEMA, {})
    accepted_m6_summaries = accepted_by_schema.get(M6_QUALIFICATION_SCHEMA, {})
    accepted_readiness_outputs = accepted_by_schema.get(M7_READINESS_SCHEMA, {})
    accepted_parity_signoffs = accepted_by_schema.get(M7_PARITY_SIGNOFF_SCHEMA, {})

    expect(accepted_m5_acceptances, "no accepted M5 acceptance sidecars were found")
    expect(accepted_m6_summaries, "no accepted M6 qualification sidecars were found")
    expect(accepted_readiness_outputs, "no accepted M7 release-readiness outputs were found")
    expect(accepted_parity_signoffs, "no accepted M7 parity signoff sidecars were found")

    m5_to_m6: dict[str, str] = {}
    acceptance_by_m5: dict[str, str] = {}
    for acceptance_path in sorted(accepted_m5_acceptances):
        require_schema(root, acceptance_path, M5_ACCEPTANCE_SCHEMA, "M5 acceptance")
        m5_path, m6_path = verify_m5_acceptance(
            root,
            acceptance_path,
            read_json(root / acceptance_path),
            accepted_m5_packets=accepted_m5_packets,
            accepted_m6_summaries=accepted_m6_summaries,
            accepted_readiness_outputs=accepted_readiness_outputs,
            accepted_parity_signoffs=accepted_parity_signoffs,
        )
        expect(m5_path not in acceptance_by_m5, f"multiple accepted M5 sidecars consume {m5_path}")
        acceptance_by_m5[m5_path] = acceptance_path
        m5_to_m6[m5_path] = m6_path

    missing_acceptance = sorted(set(accepted_m5_packets).difference(acceptance_by_m5))
    expect(
        not missing_acceptance,
        "accepted M5 packet(s) lack accepted M7 acceptance/M6 cross-link: "
        + ", ".join(missing_acceptance),
    )

    m6_to_readiness: dict[str, str] = {}
    for readiness_path in sorted(accepted_readiness_outputs):
        require_schema(root, readiness_path, M7_READINESS_SCHEMA, "M7 release readiness")
        _m5_path, m6_path, _parity_path = verify_readiness_output(
            root,
            readiness_path,
            read_json(root / readiness_path),
            accepted_m5_packets=accepted_m5_packets,
            accepted_m6_summaries=accepted_m6_summaries,
            accepted_parity_signoffs=accepted_parity_signoffs,
        )
        m6_to_readiness.setdefault(m6_path, readiness_path)

    missing_m7_signoff = sorted(set(accepted_m6_summaries).difference(m6_to_readiness))
    expect(
        not missing_m7_signoff,
        "accepted M6 summary sidecar(s) lack an accepted M7 signoff: "
        + ", ".join(missing_m7_signoff),
    )

    default_readiness = require_repo_file(
        root,
        DEFAULT_READINESS_SIDECAR.as_posix(),
        "default release readiness sidecar",
    )
    expect(
        default_readiness in accepted_readiness_outputs,
        f"default release readiness sidecar is not accepted: {default_readiness}",
    )
    default_acceptance = require_repo_file(
        root,
        DEFAULT_M5_ACCEPTANCE_SIDECAR.as_posix(),
        "default M5 acceptance sidecar",
    )
    expect(
        default_acceptance in accepted_m5_acceptances,
        f"default M5 acceptance sidecar is not accepted: {default_acceptance}",
    )
    manifest = build_manifest(root, DEFAULT_READINESS_SIDECAR, DEFAULT_BUNDLE_ROOT)
    bundle_required = required_bundle_paths(
        root,
        default_readiness,
        sorted(accepted_m5_acceptances),
        sorted(accepted_m6_summaries),
    )
    require_bundle_includes_cross_link_surfaces(manifest, bundle_required)

    return {
        "schema_version": 1,
        "status": "m5-m6-m7-cross-links-consistent",
        "accepted_m5_packets": sorted(accepted_m5_packets),
        "accepted_m5_acceptance_sidecars": sorted(accepted_m5_acceptances),
        "accepted_m6_summaries": sorted(accepted_m6_summaries),
        "accepted_m7_readiness_outputs": sorted(accepted_readiness_outputs),
        "accepted_m7_parity_signoffs": sorted(accepted_parity_signoffs),
        "m5_to_m6": m5_to_m6,
        "m6_to_m7_readiness": m6_to_readiness,
        "bundle_required_surface_count": len(bundle_required),
        "bundle_file_count": manifest["file_count"],
    }


def expect_cross_link_error(label: str, expected: str, action: Callable[[], None]) -> None:
    try:
        action()
    except CrossLinkError as error:
        expect(expected in str(error), f"{label} failed for the wrong reason: {error}")
        return
    raise CrossLinkError(f"{label} unexpectedly passed")


def run_self_check(root: Path) -> dict[str, Any]:
    summary = verify(root)
    accepted_by_schema = inventory_by_schema(root)
    accepted_m5_packets = discover_closed_m5_packets(root)
    accepted_m6_summaries = accepted_by_schema[M6_QUALIFICATION_SCHEMA]
    accepted_readiness_outputs = accepted_by_schema[M7_READINESS_SCHEMA]
    accepted_parity_signoffs = accepted_by_schema[M7_PARITY_SIGNOFF_SCHEMA]

    acceptance_path = require_repo_file(
        root,
        DEFAULT_M5_ACCEPTANCE_SIDECAR.as_posix(),
        "default M5 acceptance sidecar",
    )
    acceptance_payload = read_json(root / acceptance_path)
    missing_m5 = copy.deepcopy(acceptance_payload)
    missing_m5["m5_packet_path"] = "tools/reference-harness/specs/m5/missing-m5-packet.json"
    expect_cross_link_error(
        "missing M5 packet rejection",
        "m5_packet_path does not exist as a file",
        lambda: verify_m5_acceptance(
            root,
            acceptance_path,
            missing_m5,
            accepted_m5_packets=accepted_m5_packets,
            accepted_m6_summaries=accepted_m6_summaries,
            accepted_readiness_outputs=accepted_readiness_outputs,
            accepted_parity_signoffs=accepted_parity_signoffs,
        ),
    )

    readiness_path = require_repo_file(
        root,
        DEFAULT_READINESS_SIDECAR.as_posix(),
        "default release readiness sidecar",
    )
    readiness_payload = read_json(root / readiness_path)
    wrong_m6 = copy.deepcopy(readiness_payload)
    wrong_m6["m6_qualification_summary_path"] = "tools/reference-harness/specs/m7/lane92/m6-qualification.json"
    expect_cross_link_error(
        "unaccepted M6 sidecar rejection",
        "does not reference an accepted M6 summary",
        lambda: verify_readiness_output(
            root,
            readiness_path,
            wrong_m6,
            accepted_m5_packets=accepted_m5_packets,
            accepted_m6_summaries=accepted_m6_summaries,
            accepted_parity_signoffs=accepted_parity_signoffs,
        ),
    )

    wrong_signoff = copy.deepcopy(readiness_payload)
    wrong_signoff["parity_signoff_summary_path"] = (
        "tools/reference-harness/specs/m7/lane83/release-parity-signoff.json"
    )
    expect_cross_link_error(
        "unaccepted M7 signoff rejection",
        "does not reference an accepted M7 parity signoff",
        lambda: verify_readiness_output(
            root,
            readiness_path,
            wrong_signoff,
            accepted_m5_packets=accepted_m5_packets,
            accepted_m6_summaries=accepted_m6_summaries,
            accepted_parity_signoffs=accepted_parity_signoffs,
        ),
    )

    bundle_required = required_bundle_paths(
        root,
        readiness_path,
        sorted(accepted_by_schema[M5_ACCEPTANCE_SCHEMA]),
        sorted(accepted_m6_summaries),
    )
    manifest = build_manifest(root, DEFAULT_READINESS_SIDECAR, DEFAULT_BUNDLE_ROOT)
    tampered_manifest = copy.deepcopy(manifest)
    m5_packet_path = readiness_payload["m5_qualification_summary_path"]
    tampered_manifest["files"] = [
        entry for entry in tampered_manifest["files"] if entry["path"] != m5_packet_path
    ]
    tampered_manifest["file_count"] = len(tampered_manifest["files"])
    expect_cross_link_error(
        "bundle missing M5 packet rejection",
        "missing accepted M5/M6/M7 surface",
        lambda: require_bundle_includes_cross_link_surfaces(tampered_manifest, bundle_required),
    )

    summary["self_check"] = {
        "missing_m5_packet_rejected": True,
        "unaccepted_m6_sidecar_rejected": True,
        "unaccepted_m7_signoff_rejected": True,
        "bundle_missing_m5_packet_rejected": True,
    }
    return summary


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-check", action="store_true", help="Run synthetic drift checks")
    parser.add_argument(
        "--summary-path",
        type=Path,
        help="Optional path to write the cross-link summary JSON",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        root = repo_root()
        summary = run_self_check(root) if args.self_check else verify(root)
        if args.summary_path is not None:
            args.summary_path.parent.mkdir(parents=True, exist_ok=True)
            args.summary_path.write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n")
        print(json.dumps(summary, indent=2, sort_keys=True))
        return 0
    except Exception as error:  # noqa: BLE001 - surface CTest failures explicitly.
        print(
            json.dumps(
                {
                    "schema_version": 1,
                    "status": "m5-m6-m7-cross-link-drift",
                    "blocking_reasons": [str(error)],
                },
                indent=2,
                sort_keys=True,
            )
        )
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
