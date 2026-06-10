#!/usr/bin/env python3
"""Verify accepted M7 release-readiness references point at accepted sidecars."""

from __future__ import annotations

import argparse
import json
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any

from assert_m7_release_signoff_ready import (
    ACCEPTED_READINESS_SIDECAR,
    READINESS_INPUTS,
    read_json,
    repo_root,
)
from audit_m7_sidecar_inventory import (
    M7_ROOT,
    InventoryError,
    build_inventory,
    verify_inventory,
    verify_schema_reconciliation,
)
from validate_m7_release_sidecar_schemas import SchemaError, validate_m7_sidecar


@dataclass(frozen=True)
class SidecarReference:
    field: str
    path: str
    schema: str
    status: str
    basis: str


class ReachabilityError(RuntimeError):
    """Raised when an accepted release-readiness sidecar reference is stale."""


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise ReachabilityError(message)


def require_repo_file(root: Path, raw: Any, field: str) -> str:
    expect(isinstance(raw, str) and raw.strip(), f"{field} must be a non-empty path")
    value = raw.strip()
    candidate = Path(value)
    if not candidate.is_absolute():
        candidate = root / candidate

    try:
        relative = candidate.resolve(strict=False).relative_to(root.resolve(strict=True))
    except ValueError as error:
        raise ReachabilityError(f"{field} must stay within the repository: {value}") from error

    expect(candidate.is_file(), f"{field} does not exist as a file: {value}")
    return relative.as_posix()


def is_m7_json_sidecar(relative_path: str, m7_root: Path) -> bool:
    path = Path(relative_path)
    if path.suffix != ".json":
        return False
    try:
        path.relative_to(m7_root)
    except ValueError:
        return False
    return True


def collect_readiness_m7_sidecar_references(
    root: Path,
    readiness_payload: dict[str, Any],
    m7_root: Path,
) -> list[tuple[str, str]]:
    references: list[tuple[str, str]] = []
    seen: set[str] = set()
    for _option, field in READINESS_INPUTS:
        relative = require_repo_file(root, readiness_payload.get(field), field)
        if not is_m7_json_sidecar(relative, m7_root):
            continue
        expect(relative not in seen, f"accepted readiness references {relative} more than once")
        seen.add(relative)
        references.append((field, relative))
    expect(references, "accepted readiness input list must reference at least one M7 sidecar")
    return references


def verify_readiness_m7_sidecar_references(
    root: Path,
    readiness_payload: dict[str, Any],
    m7_root: Path = M7_ROOT,
) -> list[SidecarReference]:
    entries = build_inventory(root, m7_root)
    verify_inventory(entries)
    verify_schema_reconciliation(root, m7_root, entries)
    inventory_by_path = {entry.path: entry for entry in entries}

    verified: list[SidecarReference] = []
    for field, relative in collect_readiness_m7_sidecar_references(root, readiness_payload, m7_root):
        entry = inventory_by_path.get(relative)
        expect(entry is not None, f"{field} is not present in M7 sidecar inventory: {relative}")
        expect(
            entry.status == "accepted",
            f"{field} does not reference an accepted M7 sidecar: {relative} ({entry.basis})",
        )
        schema = validate_m7_sidecar(root / relative, root)
        expect(
            schema == entry.schema,
            f"{field} schema drifted for {relative}: inventory={entry.schema!r} direct={schema!r}",
        )
        verified.append(
            SidecarReference(
                field=field,
                path=relative,
                schema=schema,
                status=entry.status,
                basis=entry.basis,
            )
        )
    return verified


def self_check(root: Path) -> None:
    accepted_payload = read_json(root / ACCEPTED_READINESS_SIDECAR)
    verified = verify_readiness_m7_sidecar_references(root, accepted_payload)
    expect(verified, "self-check accepted fixture did not verify any M7 sidecars")

    missing = dict(accepted_payload)
    missing["phase0_qualification_summary_path"] = (
        "tools/reference-harness/specs/m7/lane133/missing-phase0-qualification.json"
    )
    try:
        verify_readiness_m7_sidecar_references(root, missing)
    except ReachabilityError as error:
        expect(
            "phase0_qualification_summary_path does not exist as a file" in str(error),
            f"missing sidecar self-check failed for the wrong reason: {error}",
        )
    else:
        raise ReachabilityError("missing sidecar self-check unexpectedly passed")

    non_file = dict(accepted_payload)
    non_file["phase0_qualification_summary_path"] = "tools/reference-harness/specs/m7/lane133"
    try:
        verify_readiness_m7_sidecar_references(root, non_file)
    except ReachabilityError as error:
        expect(
            "phase0_qualification_summary_path does not exist as a file" in str(error),
            f"non-file sidecar self-check failed for the wrong reason: {error}",
        )
    else:
        raise ReachabilityError("non-file sidecar self-check unexpectedly passed")

    unaccepted = dict(accepted_payload)
    unaccepted["qualification_corpus_summary_path"] = (
        "tools/reference-harness/specs/m7/lane76/release-qualification-corpus.json"
    )
    try:
        verify_readiness_m7_sidecar_references(root, unaccepted)
    except ReachabilityError as error:
        expect(
            "qualification_corpus_summary_path does not reference an accepted M7 sidecar" in str(error),
            f"unaccepted sidecar self-check failed for the wrong reason: {error}",
        )
    else:
        raise ReachabilityError("unaccepted sidecar self-check unexpectedly passed")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--readiness-sidecar",
        default=str(ACCEPTED_READINESS_SIDECAR),
        help="Repository-relative accepted release-readiness sidecar to verify.",
    )
    parser.add_argument(
        "--m7-root",
        default=str(M7_ROOT),
        help="Repository-relative M7 sidecar root.",
    )
    parser.add_argument(
        "--format",
        choices=("text", "json"),
        default="text",
        help="Output format.",
    )
    parser.add_argument(
        "--self-check",
        action="store_true",
        help="Also run negative checks for missing, non-file, and unaccepted references.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    root = repo_root()
    try:
        readiness_path = root / require_repo_file(root, args.readiness_sidecar, "readiness sidecar")
        readiness_payload = read_json(readiness_path)
        references = verify_readiness_m7_sidecar_references(
            root,
            readiness_payload,
            Path(args.m7_root),
        )
        if args.self_check:
            self_check(root)
    except (InventoryError, ReachabilityError, SchemaError, RuntimeError) as error:
        print(str(error), file=sys.stderr)
        return 1

    if args.format == "json":
        payload = {
            "schema_version": 1,
            "readiness_sidecar": require_repo_file(root, args.readiness_sidecar, "readiness sidecar"),
            "verified_m7_sidecar_reference_count": len(references),
            "references": [reference.__dict__ for reference in references],
        }
        print(json.dumps(payload, indent=2, sort_keys=True))
    else:
        print(
            "M7 release-readiness sidecar references verified: "
            f"{len(references)} accepted M7 sidecars reachable"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
