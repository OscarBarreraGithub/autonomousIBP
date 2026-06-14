#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-autoIBP-TBD
"""Verify pinned master-integral counts for retained physics-channel specs."""

from __future__ import annotations

import argparse
import json
import sys
import tempfile
from pathlib import Path
from typing import Any, cast


DEFAULT_REGISTRY = Path("tools/reference-harness/specs/release/physics-channel-master-count-registry.json")
SCOPE = "physics-channel-master-count-registry"


class PhysicsChannelMasterCountError(RuntimeError):
    """Raised when a physics-channel master-integral count regresses."""


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise PhysicsChannelMasterCountError(message)


def read_json(path: Path) -> dict[str, Any]:
    try:
        with path.open("r", encoding="utf-8") as stream:
            payload = json.load(stream)
    except json.JSONDecodeError as exc:
        raise PhysicsChannelMasterCountError(f"{path} is not valid JSON: {exc}") from exc
    expect(isinstance(payload, dict), f"{path} must contain a JSON object")
    return cast(dict[str, Any], payload)


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


def require_string_list(value: Any, label: str) -> list[str]:
    expect(isinstance(value, list), f"{label} must be a list")
    entries = [require_string(item, f"{label}[{index}]") for index, item in enumerate(value)]
    duplicates = sorted(entry for entry in set(entries) if entries.count(entry) > 1)
    expect(not duplicates, f"{label} duplicates value(s): {', '.join(duplicates)}")
    return entries


def registry_relative(path: Path, root: Path) -> str:
    try:
        return path.resolve(strict=False).relative_to(root.resolve(strict=False)).as_posix()
    except ValueError:
        return str(path)


def field_value(payload: dict[str, Any], field_path: str, label: str) -> Any:
    current: Any = payload
    for segment in field_path.split("."):
        expect(bool(segment), f"{label} field path contains an empty segment")
        expect(isinstance(current, dict), f"{label}.{segment} parent must be an object")
        expect(segment in current, f"{label} missing field {field_path}")
        current = current[segment]
    return current


def master_label(value: Any, label: str) -> str:
    if isinstance(value, str):
        return require_string(value, label)
    expect(isinstance(value, dict), f"{label} must be a master label string or object")
    raw_master = cast(dict[str, Any], value)
    family = require_string(raw_master.get("family"), f"{label}.family")
    raw_indices = raw_master.get("indices")
    expect(isinstance(raw_indices, list), f"{label}.indices must be a list")
    indices: list[int] = []
    for index, raw_index in enumerate(cast(list[Any], raw_indices)):
        expect(
            type(raw_index) is int,
            f"{label}.indices[{index}] must be an integer",
        )
        indices.append(raw_index)
    return f"{family}[{','.join(str(index) for index in indices)}]"


def master_labels_from_spec(path: Path, field_path: str) -> list[str]:
    payload = read_json(path)
    raw_masters = field_value(payload, field_path, str(path))
    expect(isinstance(raw_masters, list), f"{path}.{field_path} must be a list")
    labels = [
        master_label(raw_master, f"{path}.{field_path}[{index}]")
        for index, raw_master in enumerate(cast(list[Any], raw_masters))
    ]
    duplicates = sorted(label for label in set(labels) if labels.count(label) > 1)
    expect(not duplicates, f"{path}.{field_path} duplicates master label(s): {', '.join(duplicates)}")
    return labels


def load_registry(path: Path) -> list[dict[str, Any]]:
    registry = read_json(path)
    expect(registry.get("schema_version") == 1, f"{path} schema_version must be 1")
    expect(registry.get("scope") == SCOPE, f"{path} scope must be {SCOPE}")
    require_string(registry.get("baseline_source_commit"), f"{path}.baseline_source_commit")
    require_string(registry.get("baseline_generated_at_utc"), f"{path}.baseline_generated_at_utc")
    require_string(registry.get("counting_rule"), f"{path}.counting_rule")
    require_string(registry.get("regression_policy"), f"{path}.regression_policy")

    raw_channels = registry.get("channels")
    expect(isinstance(raw_channels, list) and bool(raw_channels), f"{path}.channels must be a nonempty list")
    channels: list[dict[str, Any]] = []
    keys: set[tuple[str, str]] = set()
    for index, raw_channel in enumerate(cast(list[Any], raw_channels)):
        expect(isinstance(raw_channel, dict), f"{path}.channels[{index}] must be an object")
        channel_payload = cast(dict[str, Any], raw_channel)
        surface = require_string(channel_payload.get("surface"), f"{path}.channels[{index}].surface")
        channel = require_string(channel_payload.get("channel"), f"{path}.channels[{index}].channel")
        key = (surface, channel)
        expect(key not in keys, f"{path}.channels duplicates {surface}/{channel}")
        keys.add(key)
        source_spec = require_string(channel_payload.get("source_spec"), f"{surface}/{channel}.source_spec")
        master_field = require_string(channel_payload.get("master_field"), f"{surface}/{channel}.master_field")
        minimum = require_non_negative_int(
            channel_payload.get("minimum_master_count"),
            f"{surface}/{channel}.minimum_master_count",
        )
        baseline_labels = require_string_list(
            channel_payload.get("baseline_master_labels"),
            f"{surface}/{channel}.baseline_master_labels",
        )
        expect(
            len(baseline_labels) == minimum,
            f"{surface}/{channel}.baseline_master_labels length must match minimum_master_count",
        )
        channels.append(
            {
                "surface": surface,
                "channel": channel,
                "source_spec": source_spec,
                "master_field": master_field,
                "minimum_master_count": minimum,
            }
        )
    return channels


def verify_counts(root: Path, registry_path: Path) -> dict[str, Any]:
    channels = load_registry(registry_path)
    summaries: list[dict[str, Any]] = []
    errors: list[str] = []
    for channel in channels:
        source_spec = root / str(channel["source_spec"])
        if not source_spec.is_file():
            errors.append(f"{channel['surface']}/{channel['channel']} source spec is missing: {source_spec}")
            continue
        labels = master_labels_from_spec(source_spec, str(channel["master_field"]))
        actual_count = len(labels)
        minimum_count = int(channel["minimum_master_count"])
        if actual_count < minimum_count:
            errors.append(
                f"{channel['surface']}/{channel['channel']} master count dropped: "
                f"actual={actual_count}, pinned_minimum={minimum_count}"
            )
        summaries.append(
            {
                "surface": channel["surface"],
                "channel": channel["channel"],
                "source_spec": registry_relative(source_spec, root),
                "master_count": actual_count,
                "minimum_master_count": minimum_count,
            }
        )
    if errors:
        raise PhysicsChannelMasterCountError("; ".join(errors))
    return {
        "registry": registry_relative(registry_path, root),
        "channels": summaries,
    }


def spec_payload(benchmark_id: str, masters: list[str]) -> dict[str, Any]:
    return {
        "schema_version": 1,
        "benchmark_id": benchmark_id,
        "masters": masters,
    }


def registry_payload() -> dict[str, Any]:
    return {
        "schema_version": 1,
        "scope": SCOPE,
        "baseline_source_commit": "0" * 40,
        "baseline_generated_at_utc": "2026-06-14T00:00:00Z",
        "counting_rule": "Count structured entries in each retained physics-channel spec's configured master field.",
        "channels": [
            {
                "surface": "b61n",
                "channel": "complex_kinematics",
                "source_spec": "tools/reference-harness/specs/phase0/complex_kinematics.amflow-state.json",
                "master_field": "masters",
                "minimum_master_count": 2,
                "baseline_master_labels": ["box[0,0,0,1]", "box[1,1,1,1]"],
            },
            {
                "surface": "b63n",
                "channel": "automatic_phasespace",
                "source_spec": "tools/reference-harness/specs/phase0/automatic_phasespace.amflow-state.json",
                "master_field": "masters",
                "minimum_master_count": 1,
                "baseline_master_labels": ["phase[1,0,1,0,1,0,0]"],
            },
        ],
        "regression_policy": "Fail if any pinned channel's master count drops below its minimum.",
    }


def run_self_check() -> None:
    with tempfile.TemporaryDirectory(prefix="physics-channel-master-counts-") as tmp:
        root = Path(tmp)
        registry = root / DEFAULT_REGISTRY
        complex_spec = root / "tools/reference-harness/specs/phase0/complex_kinematics.amflow-state.json"
        phase_spec = root / "tools/reference-harness/specs/phase0/automatic_phasespace.amflow-state.json"
        write_json(registry, registry_payload())
        write_json(complex_spec, spec_payload("complex_kinematics", ["box[0,0,0,1]", "box[1,1,1,1]"]))
        write_json(phase_spec, spec_payload("automatic_phasespace", ["phase[1,0,1,0,1,0,0]"]))
        verify_counts(root, registry)

        write_json(complex_spec, spec_payload("complex_kinematics", ["box[0,0,0,1]"]))
        try:
            verify_counts(root, registry)
        except PhysicsChannelMasterCountError as exc:
            expect("master count dropped" in str(exc), f"wrong count-drop error: {exc}")
        else:
            raise PhysicsChannelMasterCountError("self-check count-drop fixture unexpectedly passed")

        write_json(complex_spec, spec_payload("complex_kinematics", ["box[0,0,0,1]", "box[0,0,0,1]"]))
        try:
            verify_counts(root, registry)
        except PhysicsChannelMasterCountError as exc:
            expect("duplicates master label" in str(exc), f"wrong duplicate-master error: {exc}")
        else:
            raise PhysicsChannelMasterCountError("self-check duplicate-master fixture unexpectedly passed")

        invalid_registry = registry_payload()
        invalid_registry["channels"][0]["minimum_master_count"] = 3
        write_json(registry, invalid_registry)
        try:
            load_registry(registry)
        except PhysicsChannelMasterCountError as exc:
            expect("baseline_master_labels length" in str(exc), f"wrong registry validation error: {exc}")
        else:
            raise PhysicsChannelMasterCountError("self-check invalid registry unexpectedly passed")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--registry", type=Path, default=DEFAULT_REGISTRY)
    parser.add_argument("--verify", action="store_true")
    parser.add_argument("--self-check", action="store_true")
    args = parser.parse_args(argv)

    try:
        if args.self_check:
            run_self_check()
            print("physics-channel master-count verifier self-check passed")
            return 0

        root = repo_root()
        registry = args.registry if args.registry.is_absolute() else root / args.registry
        summary = verify_counts(root, registry)
        channel_summary = ", ".join(
            f"{item['surface']}/{item['channel']}={item['master_count']}"
            for item in summary["channels"]
        )
        print(f"physics-channel master counts verified: {channel_summary}")
        return 0
    except (PhysicsChannelMasterCountError, OSError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
