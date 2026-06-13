#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-autoIBP-TBD
"""Verify pinned accepted M5 packet counts by category."""

from __future__ import annotations

import argparse
import copy
import json
import re
import sys
import tempfile
from pathlib import Path
from typing import Any, cast

from verify_m5_packet_provenance import (  # type: ignore[import-not-found]
    DEFAULT_M5_PACKET,
    DEFAULT_PROVENANCE,
    verify_manifest,
)


DEFAULT_REGISTRY = Path("tools/reference-harness/specs/m5/m5-accepted-packet-count-registry.json")
SCOPE = "m5-accepted-packet-count-registry"
M5_PREFIX = "tools/reference-harness/specs/m5/"
LANE_RE = re.compile(r"(?:^|[-_/])lane([0-9]+)(?=$|[-_/.])")


class M5AcceptedPacketCountError(RuntimeError):
    """Raised when accepted M5 packet count floors regress."""


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise M5AcceptedPacketCountError(message)


def read_json(path: Path) -> dict[str, Any]:
    try:
        with path.open("r", encoding="utf-8") as stream:
            payload = json.load(stream)
    except json.JSONDecodeError as error:
        raise M5AcceptedPacketCountError(f"{path} is not valid JSON: {error}") from error
    expect(isinstance(payload, dict), f"{path} must contain a JSON object")
    return payload


def write_json(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def require_string(value: Any, label: str) -> str:
    expect(isinstance(value, str) and bool(value.strip()), f"{label} must be a nonempty string")
    expect(value == value.strip(), f"{label} must not carry surrounding whitespace")
    return value


def require_non_negative_int(value: Any, label: str) -> int:
    expect(type(value) is int and value >= 0, f"{label} must be a nonnegative integer")
    return value


def require_count_map(value: Any, label: str) -> dict[str, int]:
    expect(isinstance(value, dict) and bool(value), f"{label} must be a nonempty object")
    raw_counts = cast(dict[Any, Any], value)
    counts: dict[str, int] = {}
    for key, raw_count in raw_counts.items():
        category = require_string(key, f"{label} key")
        expect(category not in counts, f"{label} duplicates category {category}")
        counts[category] = require_non_negative_int(raw_count, f"{label}.{category}")
    return counts


def load_registry(path: Path) -> dict[str, Any]:
    registry = read_json(path)
    expect(registry.get("schema_version") == 1, f"{path} schema_version must be 1")
    expect(registry.get("scope") == SCOPE, f"{path} scope must be {SCOPE}")
    source_manifest = require_string(registry.get("source_manifest"), f"{path}.source_manifest")
    source_m5_packet = require_string(registry.get("source_m5_packet"), f"{path}.source_m5_packet")
    expect(source_manifest == DEFAULT_PROVENANCE.as_posix(), f"{path}.source_manifest mismatch")
    expect(source_m5_packet == DEFAULT_M5_PACKET.as_posix(), f"{path}.source_m5_packet mismatch")
    total_minimum = require_non_negative_int(
        registry.get("minimum_total_count"),
        f"{path}.minimum_total_count",
    )
    raw_minimum_counts = registry.get("minimum_counts")
    expect(isinstance(raw_minimum_counts, dict), f"{path}.minimum_counts must be an object")
    minimum_count_payload = cast(dict[str, Any], raw_minimum_counts)
    minimum_counts: dict[str, dict[str, int]] = {}
    for dimension in ("by_channel", "by_packet_kind", "by_packet_mode", "by_source_lane"):
        minimum_counts[dimension] = require_count_map(
            minimum_count_payload.get(dimension),
            f"{path}.minimum_counts.{dimension}",
        )
        dimension_total = sum(minimum_counts[dimension].values())
        expect(
            dimension_total == total_minimum,
            f"{path}.minimum_counts.{dimension} must sum to minimum_total_count",
        )
    return {
        "minimum_total_count": total_minimum,
        "minimum_counts": minimum_counts,
    }


def packet_channel(packet_id: str) -> str:
    prefix = packet_id.split(":", 1)[0]
    if prefix == "example":
        return "example_comparison"
    if prefix == "runtime_feature":
        return "runtime_feature_evidence"
    if prefix == "packet":
        return "packet_artifact"
    raise M5AcceptedPacketCountError(f"unsupported M5 packet channel prefix: {packet_id}")


def packet_mode(packet_id: str) -> str:
    parts = packet_id.split(":", 1)
    expect(len(parts) == 2 and bool(parts[1]), f"invalid packet_id: {packet_id}")
    return parts[1].replace("-", "_")


def packet_source_lane(packet_id: str, output_path: str) -> str:
    matches = LANE_RE.findall(output_path)
    expect(bool(matches), f"{packet_id} output_path does not include a lane marker: {output_path}")
    return "lane" + matches[-1]


def increment(counts: dict[str, int], category: str) -> None:
    counts[category] = counts.get(category, 0) + 1


def count_manifest_entries(root: Path, manifest_path: Path, m5_packet_path: Path) -> dict[str, Any]:
    verify_manifest(root, manifest_path, m5_packet_path)
    manifest = read_json(manifest_path)
    raw_entries = manifest.get("entries")
    expect(isinstance(raw_entries, list), "M5 provenance manifest entries must be a list")
    entries = cast(list[Any], raw_entries)

    counts: dict[str, dict[str, int]] = {
        "by_channel": {},
        "by_packet_kind": {},
        "by_packet_mode": {},
        "by_source_lane": {},
    }
    packet_ids: set[str] = set()
    for index, raw_entry in enumerate(entries):
        expect(isinstance(raw_entry, dict), f"M5 provenance entry[{index}] must be an object")
        packet_id = require_string(raw_entry.get("packet_id"), f"entry[{index}].packet_id")
        packet_kind = require_string(raw_entry.get("packet_kind"), f"{packet_id}.packet_kind")
        output_path = require_string(raw_entry.get("output_path"), f"{packet_id}.output_path")
        expect(packet_id not in packet_ids, f"duplicate accepted M5 packet id: {packet_id}")
        expect(
            output_path.startswith(M5_PREFIX),
            f"{packet_id} output_path is outside {M5_PREFIX}: {output_path}",
        )
        packet_ids.add(packet_id)
        increment(counts["by_channel"], packet_channel(packet_id))
        increment(counts["by_packet_kind"], packet_kind)
        increment(counts["by_packet_mode"], packet_mode(packet_id))
        increment(counts["by_source_lane"], packet_source_lane(packet_id, output_path))

    return {
        "total_count": len(entries),
        "counts": {dimension: dict(sorted(values.items())) for dimension, values in counts.items()},
    }


def registry_relative(path: Path, root: Path) -> str:
    try:
        return path.resolve(strict=False).relative_to(root.resolve(strict=False)).as_posix()
    except ValueError:
        return str(path)


def verify_counts(
    *,
    root: Path,
    registry_path: Path,
    manifest_path: Path,
    m5_packet_path: Path,
) -> dict[str, Any]:
    registry = load_registry(registry_path)
    actual = count_manifest_entries(root, manifest_path, m5_packet_path)
    total_count = actual["total_count"]
    minimum_total = registry["minimum_total_count"]
    errors: list[str] = []
    if total_count < minimum_total:
        errors.append(f"total accepted M5 packet count dropped: actual={total_count}, pinned_minimum={minimum_total}")

    minimum_counts = registry["minimum_counts"]
    actual_counts = actual["counts"]
    for dimension, expected_counts in minimum_counts.items():
        for category, minimum_count in expected_counts.items():
            actual_count = actual_counts.get(dimension, {}).get(category, 0)
            if actual_count < minimum_count:
                errors.append(
                    f"{dimension}.{category} accepted M5 packet count dropped: "
                    f"actual={actual_count}, pinned_minimum={minimum_count}"
                )
    if errors:
        raise M5AcceptedPacketCountError("; ".join(errors))

    return {
        "registry": registry_relative(registry_path, root),
        "manifest": registry_relative(manifest_path, root),
        "m5_packet": registry_relative(m5_packet_path, root),
        "total_count": total_count,
        "minimum_total_count": minimum_total,
        "counts": actual_counts,
    }


def registry_from_counts(root: Path, manifest_path: Path, m5_packet_path: Path) -> dict[str, Any]:
    actual = count_manifest_entries(root, manifest_path, m5_packet_path)
    return {
        "schema_version": 1,
        "scope": SCOPE,
        "source_manifest": DEFAULT_PROVENANCE.as_posix(),
        "source_m5_packet": DEFAULT_M5_PACKET.as_posix(),
        "counting_policy": [
            "Counts each entry in the verified committed M5 packet provenance manifest as one accepted M5 packet.",
            "The source-lane category is derived from the accepted packet output path lane marker.",
            "Packet mode is the canonical packet_id suffix after removing the example/runtime_feature/packet channel prefix.",
            "Pinned values are minimum floors so future additions may raise counts without weakening this gate.",
        ],
        "minimum_total_count": actual["total_count"],
        "minimum_counts": actual["counts"],
    }


def expect_failure(action: Any, expected: str) -> None:
    try:
        action()
    except M5AcceptedPacketCountError as error:
        expect(expected in str(error), f"unexpected self-check error: {error}")
        return
    raise M5AcceptedPacketCountError("self-check fixture unexpectedly passed")


def write_temp_registry(payload: dict[str, Any]) -> Path:
    temp_dir = Path(tempfile.mkdtemp(prefix="m5-accepted-packet-count-self-check-"))
    registry_path = temp_dir / DEFAULT_REGISTRY.name
    write_json(registry_path, payload)
    return registry_path


def run_self_check(root: Path, registry_path: Path, manifest_path: Path, m5_packet_path: Path) -> None:
    verify_counts(
        root=root,
        registry_path=registry_path,
        manifest_path=manifest_path,
        m5_packet_path=m5_packet_path,
    )
    registry = read_json(registry_path)

    total_drop_fixture = copy.deepcopy(registry)
    total_drop_fixture["minimum_total_count"] += 1
    for dimension_counts in total_drop_fixture["minimum_counts"].values():
        first_category = sorted(dimension_counts)[0]
        dimension_counts[first_category] += 1
    expect_failure(
        lambda: verify_counts(
            root=root,
            registry_path=write_temp_registry(total_drop_fixture),
            manifest_path=manifest_path,
            m5_packet_path=m5_packet_path,
        ),
        "total accepted M5 packet count dropped",
    )

    category_drop_fixture = copy.deepcopy(registry)
    category_drop_fixture["minimum_counts"]["by_source_lane"]["lane39"] += 1
    category_drop_fixture["minimum_total_count"] += 1
    for dimension, dimension_counts in category_drop_fixture["minimum_counts"].items():
        if dimension == "by_source_lane":
            continue
        first_category = sorted(dimension_counts)[0]
        dimension_counts[first_category] += 1
    expect_failure(
        lambda: verify_counts(
            root=root,
            registry_path=write_temp_registry(category_drop_fixture),
            manifest_path=manifest_path,
            m5_packet_path=m5_packet_path,
        ),
        "by_source_lane.lane39 accepted M5 packet count dropped",
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=repo_root(), help="Repository root")
    parser.add_argument("--registry", type=Path, default=DEFAULT_REGISTRY)
    parser.add_argument("--manifest", type=Path, default=DEFAULT_PROVENANCE)
    parser.add_argument("--m5-packet", type=Path, default=DEFAULT_M5_PACKET)
    parser.add_argument("--verify", action="store_true", help="Verify the committed count registry")
    parser.add_argument("--self-check", action="store_true", help="Run synthetic count-regression checks")
    parser.add_argument("--write-registry", action="store_true", help="Regenerate the count registry")
    parser.add_argument("--json", action="store_true", help="Print summary as JSON")
    args = parser.parse_args()

    root = args.root.resolve()
    registry_path = args.registry if args.registry.is_absolute() else root / args.registry
    manifest_path = args.manifest if args.manifest.is_absolute() else root / args.manifest
    m5_packet_path = args.m5_packet if args.m5_packet.is_absolute() else root / args.m5_packet

    try:
        if args.write_registry:
            payload = registry_from_counts(root, manifest_path, m5_packet_path)
            write_json(registry_path, payload)
            summary = {
                "registry": registry_relative(registry_path, root),
                "total_count": payload["minimum_total_count"],
            }
        elif args.self_check:
            run_self_check(root, registry_path, manifest_path, m5_packet_path)
            summary = {"self_check": True}
        else:
            summary = verify_counts(
                root=root,
                registry_path=registry_path,
                manifest_path=manifest_path,
                m5_packet_path=m5_packet_path,
            )

        if args.json:
            print(json.dumps(summary, indent=2, sort_keys=True))
        elif args.write_registry:
            print(f"M5 accepted packet count registry written: {summary['total_count']} packets")
        elif args.self_check:
            print("M5 accepted packet count self-check passed")
        else:
            print(f"M5 accepted packet counts verified: {summary['total_count']} packets")
    except Exception as error:  # noqa: BLE001 - CTest should show the first blocker.
        print(f"M5 accepted packet count verification failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
