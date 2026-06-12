#!/usr/bin/env python3
"""Verify provenance for the accepted M5 qualification packet surface."""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Any


DEFAULT_M5_PACKET = Path("tools/reference-harness/specs/m5/m5-qualification-lane62.json")
DEFAULT_PROVENANCE = Path("tools/reference-harness/specs/m5/lane63/m5-packet-provenance.json")
COMPARE_SCRIPT = Path("tools/reference-harness/scripts/compare_cpp_vs_amflow.py")
QUALIFY_SCRIPT = Path("tools/reference-harness/scripts/qualify_milestone_m5.py")
SHA256_RE = re.compile(r"^sha256:[0-9a-f]{64}$")
COMMIT_RE = re.compile(r"^[0-9a-f]{40}$")
TIMESTAMP_RE = re.compile(
    r"^[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}(?:Z|[+-][0-9]{2}:[0-9]{2})$"
)
REPO_PATH_PREFIXES = (
    "tools/",
    "tests/",
    "src/",
    "include/",
    "docs/",
    "cmake/",
    "CMakeLists.txt",
)
REQUIRED_PROVENANCE_FIELDS = (
    "source_commit",
    "generation_script",
    "generation_script_version",
    "input_hash",
    "output_hash",
    "timestamp",
    "signer",
)


class M5PacketProvenanceError(RuntimeError):
    """Raised when M5 packet provenance is missing or stale."""


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise M5PacketProvenanceError(message)


def load_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as stream:
        payload = json.load(stream)
    expect(isinstance(payload, dict), f"{path} must contain a JSON object")
    return payload


def write_json(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return "sha256:" + digest.hexdigest()


def normalize_digest(raw: Any, label: str) -> str:
    expect(isinstance(raw, str), f"{label} must be a string")
    expect(SHA256_RE.fullmatch(raw) is not None, f"{label} must be sha256:<64 lowercase hex>")
    return raw


def clean_path_reference(raw: Any) -> str | None:
    if isinstance(raw, Path):
        raw = raw.as_posix()
    if not isinstance(raw, str):
        return None
    value = raw.strip()
    if not value or value != raw.strip():
        return None
    if "\n" in value or "\r" in value or "\t" in value:
        return None
    if re.search(r":\d+$", value):
        value = value.rsplit(":", 1)[0]
    return value


def looks_repo_relative(value: str) -> bool:
    return value.startswith(REPO_PATH_PREFIXES)


def repo_relative_reference(root: Path, raw: Any) -> str | None:
    value = clean_path_reference(raw)
    if value is None:
        return None
    path = Path(value)
    if path.is_absolute():
        marker = "/tools/reference-harness/"
        if marker in value:
            return "tools/reference-harness/" + value.split(marker, 1)[1]
        try:
            return path.resolve(strict=False).relative_to(root.resolve(strict=False)).as_posix()
        except ValueError:
            return None
    if looks_repo_relative(value):
        return Path(value).as_posix()
    return None


def require_repo_file(root: Path, raw: Any, label: str) -> str:
    relative = repo_relative_reference(root, raw)
    expect(relative is not None, f"{label} must be a repo-local file path")
    path = root / relative
    expect(path.is_file(), f"{label} referenced file is missing: {relative}")
    return relative


def hash_file_set(root: Path, paths: list[str]) -> str:
    entries = []
    for relative in sorted(set(paths)):
        path = root / relative
        expect(path.is_file(), f"input artifact is missing: {relative}")
        entries.append({"path": relative, "sha256": sha256_file(path)})
    encoded = json.dumps(entries, separators=(",", ":"), sort_keys=True).encode("utf-8")
    return "sha256:" + hashlib.sha256(encoded).hexdigest()


def git_output(root: Path, args: list[str]) -> str:
    completed = subprocess.run(
        ["git", *args],
        cwd=root,
        check=True,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    return completed.stdout.strip()


def commit_exists(root: Path, commit: str) -> bool:
    completed = subprocess.run(
        ["git", "cat-file", "-e", f"{commit}^{{commit}}"],
        cwd=root,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    return completed.returncode == 0


def source_info_for_path(root: Path, relative_path: str) -> dict[str, str]:
    output = git_output(root, ["log", "-n1", "--format=%H%x00%aI%x00%an <%ae>", "--", relative_path])
    expect(output, f"no source commit found for {relative_path}")
    source_commit, timestamp, signer = output.split("\x00")
    return {
        "source_commit": source_commit,
        "timestamp": timestamp,
        "signer": signer,
    }


def accepted_entries(m5_packet: dict[str, Any]) -> tuple[list[dict[str, Any]], list[dict[str, Any]]]:
    examples = m5_packet.get("example_summaries")
    runtime_features = m5_packet.get("runtime_feature_summaries")
    expect(isinstance(examples, list), "M5 packet example_summaries must be a list")
    expect(
        isinstance(runtime_features, list),
        "M5 packet runtime_feature_summaries must be a list",
    )
    return (
        [entry for entry in examples if isinstance(entry, dict) and entry.get("accepted") is True],
        [
            entry
            for entry in runtime_features
            if isinstance(entry, dict) and entry.get("accepted") is True
        ],
    )


def evidence_path_to_file(root: Path, m5_root: Path, raw: Any) -> str:
    expect(isinstance(raw, str), "runtime feature evidence_path must be a string")
    file_part = raw.split("#", 1)[0]
    relative = repo_relative_reference(root, file_part)
    if relative is None:
        relative = (m5_root / file_part).relative_to(root).as_posix()
    expect((root / relative).is_file(), f"runtime feature evidence file is missing: {relative}")
    return relative


def collect_comparison_inputs(root: Path, comparison_path: str) -> tuple[list[str], list[str]]:
    comparison = load_json(root / comparison_path)
    inputs: list[str] = []
    external: list[str] = []
    for field in ("cpp_result", "amflow_golden", "reference_golden"):
        raw = comparison.get(field)
        if raw is None:
            continue
        relative = repo_relative_reference(root, raw)
        if relative is not None and (root / relative).is_file():
            inputs.append(relative)
        else:
            external.append(str(raw))
    return sorted(set(inputs)), sorted(set(external))


def collect_runtime_feature_inputs(
    *,
    root: Path,
    runtime_evidence_path: str,
    runtime_feature_id: str,
) -> tuple[list[str], list[str]]:
    evidence = load_json(root / runtime_evidence_path)
    raw_features = evidence.get("features")
    expect(isinstance(raw_features, list), f"{runtime_evidence_path} features must be a list")
    feature = None
    for candidate in raw_features:
        if isinstance(candidate, dict) and candidate.get("id") == runtime_feature_id:
            feature = candidate
            break
    expect(feature is not None, f"{runtime_evidence_path} has no feature {runtime_feature_id}")

    inputs: list[str] = []
    external: list[str] = []
    raw_evidence = feature.get("evidence", [])
    if isinstance(raw_evidence, list):
        for item in raw_evidence:
            relative = repo_relative_reference(root, item)
            if relative is not None and (root / relative).is_file():
                inputs.append(relative)
            else:
                external.append(str(item))
    raw_hashes = feature.get("artifact_sha256", {})
    if isinstance(raw_hashes, dict):
        for raw_path, raw_digest in raw_hashes.items():
            relative = repo_relative_reference(root, raw_path)
            if relative is not None and (root / relative).is_file():
                actual = sha256_file(root / relative)
                expect(
                    actual == "sha256:" + str(raw_digest),
                    f"{runtime_feature_id} artifact_sha256 drift for {relative}",
                )
                inputs.append(relative)
            else:
                external.append(str(raw_path))
    return sorted(set(inputs)), sorted(set(external))


def entry(
    *,
    root: Path,
    packet_id: str,
    packet_kind: str,
    output_path: str,
    generation_script: Path,
    input_artifacts: list[str],
    external_input_references: list[str] | None = None,
) -> dict[str, Any]:
    script_relative = generation_script.as_posix()
    source = source_info_for_path(root, output_path)
    return {
        "packet_id": packet_id,
        "packet_kind": packet_kind,
        "output_path": output_path,
        "source_commit": source["source_commit"],
        "generation_script": script_relative,
        "generation_script_version": sha256_file(root / script_relative),
        "input_artifacts": sorted(set(input_artifacts)),
        "input_hash": hash_file_set(root, input_artifacts),
        "output_hash": sha256_file(root / output_path),
        "timestamp": source["timestamp"],
        "signer": source["signer"],
        "external_input_references": sorted(set(external_input_references or [])),
    }


def generate_manifest(root: Path, m5_packet_path: Path) -> dict[str, Any]:
    packet_relative = require_repo_file(root, m5_packet_path, "M5 packet path")
    m5_packet = load_json(root / packet_relative)
    m5_root = root / "tools/reference-harness/specs/m5"
    examples, runtime_features = accepted_entries(m5_packet)
    provenance_entries: list[dict[str, Any]] = []

    feature_surface = require_repo_file(
        root,
        m5_packet.get("m5_feature_surface_evidence_path"),
        "m5_feature_surface_evidence_path",
    )
    closure_decision = require_repo_file(
        root,
        m5_packet.get("m5_all_phase_closure_decision_path"),
        "m5_all_phase_closure_decision_path",
    )
    feature_surface_inputs = {
        require_repo_file(root, item["comparison_summary"], f"{item.get('id')} comparison")
        for item in examples
    }
    for runtime_feature in runtime_features:
        feature_surface_inputs.add(
            evidence_path_to_file(root, m5_root, runtime_feature.get("evidence_path"))
        )
    provenance_entries.append(
        entry(
            root=root,
            packet_id="packet:m5-feature-surface",
            packet_kind="m5-feature-surface",
            output_path=feature_surface,
            generation_script=QUALIFY_SCRIPT,
            input_artifacts=sorted(feature_surface_inputs),
        )
    )
    provenance_entries.append(
        entry(
            root=root,
            packet_id="packet:m5-qualification",
            packet_kind="m5-qualification",
            output_path=packet_relative,
            generation_script=QUALIFY_SCRIPT,
            input_artifacts=[feature_surface, closure_decision],
        )
    )

    for example in examples:
        example_id = str(example.get("id"))
        output_path = require_repo_file(root, example.get("comparison_summary"), f"{example_id} comparison")
        inputs, external = collect_comparison_inputs(root, output_path)
        provenance_entries.append(
            entry(
                root=root,
                packet_id=f"example:{example_id}",
                packet_kind="accepted-example-comparison",
                output_path=output_path,
                generation_script=COMPARE_SCRIPT,
                input_artifacts=inputs,
                external_input_references=external,
            )
        )

    for runtime_feature in runtime_features:
        feature_id = str(runtime_feature.get("id"))
        output_path = evidence_path_to_file(
            root,
            m5_root,
            runtime_feature.get("evidence_path"),
        )
        inputs, external = collect_runtime_feature_inputs(
            root=root,
            runtime_evidence_path=output_path,
            runtime_feature_id=feature_id,
        )
        provenance_entries.append(
            entry(
                root=root,
                packet_id=f"runtime_feature:{feature_id}",
                packet_kind="accepted-runtime-feature-evidence",
                output_path=output_path,
                generation_script=QUALIFY_SCRIPT,
                input_artifacts=inputs,
                external_input_references=external,
            )
        )

    return {
        "schema_version": 1,
        "scope": "m5-packet-provenance",
        "m5_packet_path": packet_relative,
        "accepted_example_count": len(examples),
        "accepted_runtime_feature_count": len(runtime_features),
        "packet_entry_count": len(provenance_entries),
        "entries": sorted(provenance_entries, key=lambda item: item["packet_id"]),
        "withheld_claims": [
            "This provenance manifest hashes committed M5 packet artifacts only.",
            "This provenance manifest does not rerun AMFlow numerics or create new retained captures.",
            "This provenance manifest does not widen M5, M6, M7, or release-readiness claims.",
            "Historical external run paths are recorded as external_input_references and are not counted as repo-local input_hash material.",
        ],
    }


def expected_packet_ids(m5_packet: dict[str, Any]) -> set[str]:
    examples, runtime_features = accepted_entries(m5_packet)
    ids = {"packet:m5-feature-surface", "packet:m5-qualification"}
    ids.update(f"example:{entry.get('id')}" for entry in examples)
    ids.update(f"runtime_feature:{entry.get('id')}" for entry in runtime_features)
    return ids


def verify_entry(root: Path, raw_entry: Any, expected_ids: set[str]) -> dict[str, Any]:
    expect(isinstance(raw_entry, dict), "provenance entries must be objects")
    packet_id = raw_entry.get("packet_id")
    expect(isinstance(packet_id, str) and packet_id, "provenance entry packet_id is required")
    expect(packet_id in expected_ids, f"{packet_id} is not accepted by the M5 packet")
    for field in REQUIRED_PROVENANCE_FIELDS:
        expect(field in raw_entry, f"{packet_id} missing provenance field {field}")

    output_path = require_repo_file(root, raw_entry.get("output_path"), f"{packet_id} output_path")
    source_commit = raw_entry["source_commit"]
    expect(
        isinstance(source_commit, str) and COMMIT_RE.fullmatch(source_commit) is not None,
        f"{packet_id} source_commit must be a full lowercase commit",
    )
    expect(commit_exists(root, source_commit), f"{packet_id} source_commit is not a known commit")
    expected_source = source_info_for_path(root, output_path)
    expect(
        source_commit == expected_source["source_commit"],
        f"{packet_id} source_commit drift: expected {expected_source['source_commit']}, got {source_commit}",
    )
    timestamp = raw_entry["timestamp"]
    expect(
        isinstance(timestamp, str) and TIMESTAMP_RE.fullmatch(timestamp) is not None,
        f"{packet_id} timestamp must be ISO-8601 with timezone",
    )
    expect(
        timestamp == expected_source["timestamp"],
        f"{packet_id} timestamp drift: expected {expected_source['timestamp']}, got {timestamp}",
    )
    signer = raw_entry["signer"]
    expect(isinstance(signer, str) and signer.strip(), f"{packet_id} signer must not be empty")
    expect(
        signer == expected_source["signer"],
        f"{packet_id} signer drift: expected {expected_source['signer']}, got {signer}",
    )

    generation_script = require_repo_file(
        root,
        raw_entry.get("generation_script"),
        f"{packet_id} generation_script",
    )
    expected_script_version = sha256_file(root / generation_script)
    actual_script_version = normalize_digest(
        raw_entry.get("generation_script_version"),
        f"{packet_id} generation_script_version",
    )
    expect(
        actual_script_version == expected_script_version,
        f"{packet_id} generation_script_version mismatch",
    )

    raw_inputs = raw_entry.get("input_artifacts")
    expect(isinstance(raw_inputs, list), f"{packet_id} input_artifacts must be a list")
    inputs = [require_repo_file(root, item, f"{packet_id} input_artifacts entry") for item in raw_inputs]
    expected_input_hash = hash_file_set(root, inputs)
    actual_input_hash = normalize_digest(raw_entry.get("input_hash"), f"{packet_id} input_hash")
    expect(actual_input_hash == expected_input_hash, f"{packet_id} input_hash mismatch")
    actual_output_hash = normalize_digest(raw_entry.get("output_hash"), f"{packet_id} output_hash")
    expected_output_hash = sha256_file(root / output_path)
    expect(actual_output_hash == expected_output_hash, f"{packet_id} output_hash mismatch")

    raw_external = raw_entry.get("external_input_references", [])
    expect(isinstance(raw_external, list), f"{packet_id} external_input_references must be a list")
    for item in raw_external:
        expect(isinstance(item, str) and item.strip(), f"{packet_id} external input must be a string")

    return {
        "packet_id": packet_id,
        "output_path": output_path,
        "source_commit": source_commit,
        "input_count": len(inputs),
        "external_input_count": len(raw_external),
    }


def verify_manifest(root: Path, manifest_path: Path, m5_packet_path: Path) -> dict[str, Any]:
    manifest_file = manifest_path if manifest_path.is_absolute() else root / manifest_path
    expect(manifest_file.is_file(), f"provenance manifest is missing: {manifest_file}")
    try:
        manifest_relative = manifest_file.resolve(strict=False).relative_to(
            root.resolve(strict=False)
        ).as_posix()
    except ValueError:
        manifest_relative = str(manifest_file)
    packet_relative = require_repo_file(root, m5_packet_path, "M5 packet path")
    manifest = load_json(manifest_file)
    m5_packet = load_json(root / packet_relative)
    expect(manifest.get("schema_version") == 1, "M5 provenance schema_version must be 1")
    expect(manifest.get("scope") == "m5-packet-provenance", "M5 provenance scope mismatch")
    expect(manifest.get("m5_packet_path") == packet_relative, "M5 provenance packet path mismatch")

    expected_ids = expected_packet_ids(m5_packet)
    entries = manifest.get("entries")
    expect(isinstance(entries, list), "M5 provenance entries must be a list")
    seen: set[str] = set()
    verified: list[dict[str, Any]] = []
    for raw_entry in entries:
        packet_id = raw_entry.get("packet_id") if isinstance(raw_entry, dict) else None
        expect(packet_id not in seen, f"duplicate M5 provenance packet_id: {packet_id}")
        verified_entry = verify_entry(root, raw_entry, expected_ids)
        seen.add(verified_entry["packet_id"])
        verified.append(verified_entry)
    missing = sorted(expected_ids - seen)
    extra = sorted(seen - expected_ids)
    expect(not missing, "missing M5 provenance entries: " + ", ".join(missing))
    expect(not extra, "extra M5 provenance entries: " + ", ".join(extra))
    expect(
        manifest.get("packet_entry_count") == len(entries),
        "M5 provenance packet_entry_count mismatch",
    )
    return {
        "schema_version": 1,
        "scope": "m5-packet-provenance-verification",
        "m5_packet_path": packet_relative,
        "manifest_path": manifest_relative,
        "entry_count": len(verified),
        "accepted_example_count": sum(1 for item in verified if item["packet_id"].startswith("example:")),
        "accepted_runtime_feature_count": sum(
            1 for item in verified if item["packet_id"].startswith("runtime_feature:")
        ),
        "verified": verified,
    }


def write_temp_manifest(root: Path, payload: dict[str, Any]) -> Path:
    temp_dir = Path(tempfile.mkdtemp(prefix="m5-packet-provenance-self-check-"))
    manifest_path = temp_dir / DEFAULT_PROVENANCE.name
    write_json(manifest_path, payload)
    return manifest_path


def rejected(callable_obj, expected: str) -> bool:
    try:
        callable_obj()
    except M5PacketProvenanceError as error:
        return expected in str(error)
    return False


def run_self_check(root: Path, manifest_path: Path, m5_packet_path: Path) -> None:
    verify_manifest(root, manifest_path, m5_packet_path)
    manifest = load_json(root / manifest_path)

    def mutated_manifest(mutator) -> Path:
        payload = copy.deepcopy(manifest)
        mutator(payload["entries"][0])
        return write_temp_manifest(root, payload)

    if not rejected(
        lambda: verify_manifest(
            root,
            mutated_manifest(lambda entry: entry.__setitem__("source_commit", "0" * 40)),
            m5_packet_path,
        ),
        "source_commit is not a known commit",
    ):
        raise M5PacketProvenanceError("self-check failed to reject stale source_commit")
    if not rejected(
        lambda: verify_manifest(
            root,
            mutated_manifest(lambda entry: entry.__setitem__("generation_script_version", "sha256:" + "0" * 64)),
            m5_packet_path,
        ),
        "generation_script_version mismatch",
    ):
        raise M5PacketProvenanceError("self-check failed to reject stale script version")
    if not rejected(
        lambda: verify_manifest(
            root,
            mutated_manifest(lambda entry: entry.__setitem__("input_hash", "sha256:" + "0" * 64)),
            m5_packet_path,
        ),
        "input_hash mismatch",
    ):
        raise M5PacketProvenanceError("self-check failed to reject stale input_hash")
    if not rejected(
        lambda: verify_manifest(
            root,
            mutated_manifest(lambda entry: entry.__setitem__("output_hash", "sha256:" + "0" * 64)),
            m5_packet_path,
        ),
        "output_hash mismatch",
    ):
        raise M5PacketProvenanceError("self-check failed to reject stale output_hash")
    if not rejected(
        lambda: verify_manifest(
            root,
            mutated_manifest(lambda entry: entry.pop("signer", None)),
            m5_packet_path,
        ),
        "missing provenance field signer",
    ):
        raise M5PacketProvenanceError("self-check failed to reject missing signer")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=repo_root(), help="Repository root")
    parser.add_argument("--manifest", type=Path, default=DEFAULT_PROVENANCE)
    parser.add_argument("--m5-packet", type=Path, default=DEFAULT_M5_PACKET)
    parser.add_argument("--verify", action="store_true", help="Verify committed provenance")
    parser.add_argument("--self-check", action="store_true", help="Run synthetic drift checks")
    parser.add_argument("--write-manifest", action="store_true", help="Regenerate the manifest")
    parser.add_argument("--json", action="store_true", help="Print summary as JSON")
    args = parser.parse_args()

    root = args.root.resolve()
    manifest_path = args.manifest if args.manifest.is_absolute() else root / args.manifest
    m5_packet_path = args.m5_packet if args.m5_packet.is_absolute() else root / args.m5_packet

    try:
        if args.write_manifest:
            payload = generate_manifest(root, m5_packet_path)
            write_json(manifest_path, payload)
            summary = {"manifest_path": str(manifest_path.relative_to(root)), "entry_count": len(payload["entries"])}
        elif args.self_check:
            run_self_check(root, manifest_path, m5_packet_path)
            summary = {"self_check": True}
        else:
            summary = verify_manifest(root, manifest_path, m5_packet_path)
        if args.json:
            print(json.dumps(summary, indent=2, sort_keys=True))
        else:
            if args.write_manifest:
                print(f"M5 packet provenance manifest written: {summary['entry_count']} entries")
            elif args.self_check:
                print("M5 packet provenance self-check passed")
            else:
                print(f"M5 packet provenance verified: {summary['entry_count']} entries")
    except Exception as error:  # noqa: BLE001 - CTest should show the first blocker.
        print(f"M5 packet provenance verification failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
