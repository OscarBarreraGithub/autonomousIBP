#!/usr/bin/env python3
"""Package the canonical M7 release evidence sidecars into a tarball."""

from __future__ import annotations

import argparse
import gzip
import hashlib
import io
import json
import subprocess
import sys
import tarfile
import tempfile
from pathlib import Path
from typing import Any, Callable

from validate_m7_release_sidecar_schemas import SchemaError, validate_m7_sidecar


DEFAULT_READINESS_SIDECAR = Path(
    "tools/reference-harness/specs/m7/lane3/"
    "release-readiness.m5-accepted.full-output.json"
)
DEFAULT_M5_ACCEPTANCE_SIDECAR = Path(
    "tools/reference-harness/specs/m7/lane3/"
    "m5-packet-acceptance.m5-accepted.json"
)
DEFAULT_BUNDLE_ROOT = "m7-release-evidence"
MANIFEST_NAME = "manifest.json"
EVIDENCE_CORPUS_DIGEST_ALGORITHM = "sha256-length-prefixed-path-and-bytes-v1"
SYNTHETIC_CORPUS_DIGEST = "b579c9c5587df3fb81c750b3cd133573aee5bda295a9881faf1da26194cfe1d9"

READINESS_INPUT_PATH_FIELDS: tuple[str, ...] = (
    "qualification_summary_path",
    "checklist_path",
    "qualification_corpus_summary_path",
    "m5_qualification_summary_path",
    "m6_qualification_summary_path",
    "phase0_qualification_summary_path",
    "case_study_qualification_summary_path",
    "performance_review_summary_path",
    "diagnostic_review_summary_path",
    "docs_completion_summary_path",
    "parity_signoff_summary_path",
)

WITHHELD_CLAIMS: tuple[str, ...] = (
    "This bundle does not create new retained benchmark evidence.",
    "This bundle does not modify release-readiness inputs.",
    "This bundle does not widen runtime, parity, or public behavior claims.",
)


class BundleError(RuntimeError):
    """Raised when release evidence cannot be packaged safely."""


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def read_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as stream:
        payload = json.load(stream)
    if not isinstance(payload, dict):
        raise BundleError(f"{path} must contain a JSON object")
    return payload


def git_head(root: Path) -> str:
    completed = subprocess.run(
        ["git", "rev-parse", "HEAD"],
        cwd=root,
        text=True,
        capture_output=True,
        check=False,
    )
    if completed.returncode != 0:
        stderr = completed.stderr.strip()
        raise BundleError(f"git rev-parse HEAD failed: {stderr}")
    return completed.stdout.strip()


def require_repo_file(root: Path, raw: Any, label: str) -> str:
    if not isinstance(raw, str) or not raw.strip():
        raise BundleError(f"{label} must be a non-empty string path")

    candidate = Path(raw.strip())
    if not candidate.is_absolute():
        candidate = root / candidate

    resolved_root = root.resolve(strict=True)
    resolved_candidate = candidate.resolve(strict=False)
    try:
        relative = resolved_candidate.relative_to(resolved_root)
    except ValueError as error:
        raise BundleError(f"{label} must stay within the repository: {raw}") from error

    if not resolved_candidate.is_file():
        raise BundleError(f"{label} does not exist as a file: {raw}")

    return relative.as_posix()


def require_m7_sidecar_schema(
    root: Path,
    relative_path: str,
    *,
    label: str,
    expected_schema: str,
) -> None:
    try:
        schema = validate_m7_sidecar(root / relative_path, root)
    except (SchemaError, RuntimeError, OSError) as error:
        raise BundleError(f"{label} failed M7 schema validation: {error}") from error
    if schema != expected_schema:
        raise BundleError(
            f"{label} must have schema {expected_schema}: {relative_path} has {schema}"
        )


def append_unique(paths: list[str], path: str) -> None:
    if path not in paths:
        paths.append(path)


def require_bundle_root(raw: str) -> str:
    value = raw.strip()
    if not value:
        raise BundleError("bundle root must not be empty")
    path = Path(value)
    if path.is_absolute() or ".." in path.parts:
        raise BundleError(f"bundle root must be a safe relative path: {raw}")
    return path.as_posix()


def require_manifest_string(payload: dict[str, Any], field: str, label: str) -> str:
    value = payload.get(field)
    if not isinstance(value, str) or not value.strip():
        raise BundleError(f"{label}.{field} must be a non-empty string")
    if value != value.strip():
        raise BundleError(f"{label}.{field} must not carry surrounding whitespace")
    return value


def require_manifest_relative_path(value: str, label: str) -> str:
    parts = value.split("/")
    if (
        value.startswith("/")
        or "\\" in value
        or any(part in {"", ".", ".."} for part in parts)
    ):
        raise BundleError(f"{label} must be a safe repository-relative POSIX path: {value}")
    return value


def require_manifest_sha256(value: str, label: str) -> str:
    if len(value) != 64 or any(char not in "0123456789abcdef" for char in value):
        raise BundleError(f"{label} must be a lowercase 64-character sha256 hex digest")
    return value


def require_manifest_source_commit(value: str, label: str) -> str:
    if len(value) != 40 or any(char not in "0123456789abcdef" for char in value):
        raise BundleError(f"{label} must be a full lowercase 40-character git SHA")
    return value


def require_manifest_string_list(
    manifest: dict[str, Any],
    field: str,
    label: str,
) -> list[str]:
    raw = manifest.get(field)
    if not isinstance(raw, list):
        raise BundleError(f"{label}.{field} must be a list")
    values: list[str] = []
    for index, entry in enumerate(raw):
        if not isinstance(entry, str) or not entry.strip():
            raise BundleError(f"{label}.{field}[{index}] must be a non-empty string")
        if entry != entry.strip():
            raise BundleError(f"{label}.{field}[{index}] must not carry surrounding whitespace")
        values.append(entry)
    if len(values) != len(set(values)):
        raise BundleError(f"{label}.{field} must not contain duplicates")
    return values


def validate_manifest_shape(manifest: dict[str, Any]) -> list[dict[str, Any]]:
    if manifest.get("schema_version") != 1:
        raise BundleError("bundle manifest schema_version must be integer 1")
    if manifest.get("bundle_kind") != "m7-release-evidence-bundle":
        raise BundleError("bundle manifest bundle_kind must be m7-release-evidence-bundle")
    require_manifest_source_commit(
        require_manifest_string(manifest, "source_commit", "bundle manifest"),
        "bundle manifest source_commit",
    )
    readiness_sidecar = require_manifest_relative_path(
        require_manifest_string(manifest, "readiness_sidecar", "bundle manifest"),
        "bundle manifest readiness_sidecar",
    )
    if (
        manifest.get("evidence_corpus_digest_algorithm")
        != EVIDENCE_CORPUS_DIGEST_ALGORITHM
    ):
        raise BundleError("bundle manifest has an unsupported evidence corpus digest algorithm")
    require_manifest_sha256(
        require_manifest_string(manifest, "evidence_corpus_sha256", "bundle manifest"),
        "bundle manifest evidence_corpus_sha256",
    )
    if require_manifest_string_list(manifest, "withheld_claims", "bundle manifest") != list(
        WITHHELD_CLAIMS
    ):
        raise BundleError("bundle manifest withheld_claims must match the canonical release caveats")

    raw_bundle_root = manifest.get("bundle_root")
    if not isinstance(raw_bundle_root, str):
        raise BundleError("bundle_root must be a string")
    bundle_root = require_bundle_root(raw_bundle_root)
    if bundle_root != raw_bundle_root:
        raise BundleError("bundle_root must be normalized without surrounding whitespace")

    files = manifest.get("files")
    if not isinstance(files, list):
        raise BundleError("bundle manifest files must be a list")
    file_count = manifest.get("file_count")
    if type(file_count) is not int or file_count != len(files):
        raise BundleError("bundle manifest file_count must match files length")

    normalized_files: list[dict[str, Any]] = []
    seen_paths: set[str] = set()
    seen_archive_paths: set[str] = set()
    for index, entry in enumerate(files):
        label = f"files[{index}]"
        if not isinstance(entry, dict):
            raise BundleError(f"{label} must be an object")
        relative_path = require_manifest_relative_path(
            require_manifest_string(entry, "path", label),
            f"{label}.path",
        )
        archive_path = require_manifest_relative_path(
            require_manifest_string(entry, "archive_path", label),
            f"{label}.archive_path",
        )
        expected_archive_path = f"{bundle_root}/{relative_path}"
        if archive_path != expected_archive_path:
            raise BundleError(
                f"{label}.archive_path must match bundle_root/path: {expected_archive_path}"
            )
        byte_count = entry.get("bytes")
        if type(byte_count) is not int or byte_count < 0:
            raise BundleError(f"{label}.bytes must be a nonnegative integer")
        sha256 = require_manifest_sha256(
            require_manifest_string(entry, "sha256", label),
            f"{label}.sha256",
        )
        if relative_path in seen_paths:
            raise BundleError(f"bundle manifest contains duplicate path: {relative_path}")
        if archive_path in seen_archive_paths:
            raise BundleError(f"bundle manifest contains duplicate archive_path: {archive_path}")
        seen_paths.add(relative_path)
        seen_archive_paths.add(archive_path)
        normalized_files.append(
            {
                "path": relative_path,
                "archive_path": archive_path,
                "bytes": byte_count,
                "sha256": sha256,
            }
        )

    paths = [entry["path"] for entry in normalized_files]
    if readiness_sidecar not in paths:
        raise BundleError("bundle manifest readiness_sidecar must be included in files")
    if paths != sorted(paths):
        raise BundleError("bundle manifest files must be sorted by repository path")
    return normalized_files


def collect_release_evidence_paths(root: Path, readiness_sidecar: Path) -> list[str]:
    evidence_paths: list[str] = []
    readiness_rel = require_repo_file(root, readiness_sidecar.as_posix(), "readiness sidecar")
    require_m7_sidecar_schema(
        root,
        readiness_rel,
        label="readiness sidecar",
        expected_schema="release-readiness-output",
    )
    append_unique(evidence_paths, readiness_rel)

    readiness_payload = read_json(root / readiness_rel)
    if readiness_payload.get("release_signoff_ready") is not True:
        raise BundleError("canonical release evidence bundle requires release_signoff_ready=true")

    for field in READINESS_INPUT_PATH_FIELDS:
        append_unique(evidence_paths, require_repo_file(root, readiness_payload.get(field), field))

    m5_acceptance_rel = require_repo_file(
        root,
        DEFAULT_M5_ACCEPTANCE_SIDECAR.as_posix(),
        "M5 acceptance sidecar",
    )
    require_m7_sidecar_schema(
        root,
        m5_acceptance_rel,
        label="M5 acceptance sidecar",
        expected_schema="m7-prerequisite-m5-packet-acceptance",
    )
    append_unique(evidence_paths, m5_acceptance_rel)

    m5_acceptance = read_json(root / m5_acceptance_rel)
    if m5_acceptance.get("accepted_for_release_prerequisite") is not True:
        raise BundleError("M5 acceptance sidecar must be accepted_for_release_prerequisite=true")
    for index, raw_path in enumerate(m5_acceptance.get("evidence_paths", [])):
        rel_path = require_repo_file(root, raw_path, f"M5 acceptance evidence_paths[{index}]")
        if rel_path.endswith(".json"):
            append_unique(evidence_paths, rel_path)

    return sorted(evidence_paths)


def sha256_hex(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def update_length_prefixed(digest: Any, data: bytes) -> None:
    digest.update(len(data).to_bytes(8, "big"))
    digest.update(data)


def evidence_corpus_digest_from_records(records: list[tuple[str, bytes]]) -> str:
    digest = hashlib.sha256()
    digest.update(b"autoibp-m7-release-evidence-corpus-v1\n")
    for relative_path, data in sorted(records):
        update_length_prefixed(digest, relative_path.encode("utf-8"))
        update_length_prefixed(digest, data)
    return digest.hexdigest()


def evidence_corpus_digest(root: Path, evidence_paths: list[str]) -> str:
    records = [(relative_path, (root / relative_path).read_bytes()) for relative_path in evidence_paths]
    return evidence_corpus_digest_from_records(records)


def manifest_bytes(manifest: dict[str, Any]) -> bytes:
    return json.dumps(manifest, indent=2, sort_keys=True).encode("utf-8") + b"\n"


def build_manifest(root: Path, readiness_sidecar: Path, bundle_root: str) -> dict[str, Any]:
    bundle_root = require_bundle_root(bundle_root)
    evidence_paths = collect_release_evidence_paths(root, readiness_sidecar)
    files: list[dict[str, Any]] = []
    corpus_records: list[tuple[str, bytes]] = []
    for relative_path in evidence_paths:
        file_path = root / relative_path
        data = file_path.read_bytes()
        corpus_records.append((relative_path, data))
        files.append(
            {
                "path": relative_path,
                "archive_path": f"{bundle_root}/{relative_path}",
                "bytes": len(data),
                "sha256": sha256_hex(data),
            }
        )

    return {
        "schema_version": 1,
        "bundle_kind": "m7-release-evidence-bundle",
        "source_commit": git_head(root),
        "readiness_sidecar": require_repo_file(root, readiness_sidecar.as_posix(), "readiness sidecar"),
        "bundle_root": bundle_root,
        "file_count": len(files),
        "evidence_corpus_digest_algorithm": EVIDENCE_CORPUS_DIGEST_ALGORITHM,
        "evidence_corpus_sha256": evidence_corpus_digest_from_records(corpus_records),
        "files": files,
        "withheld_claims": list(WITHHELD_CLAIMS),
    }


def make_tar_info(name: str, size: int) -> tarfile.TarInfo:
    info = tarfile.TarInfo(name)
    info.size = size
    info.mtime = 0
    info.uid = 0
    info.gid = 0
    info.uname = ""
    info.gname = ""
    info.mode = 0o644
    return info


def add_bytes(tar: tarfile.TarFile, archive_name: str, data: bytes) -> None:
    tar.addfile(make_tar_info(archive_name, len(data)), io.BytesIO(data))


def write_bundle(root: Path, output_path: Path, manifest: dict[str, Any]) -> None:
    files = validate_manifest_shape(manifest)
    bundle_root = manifest["bundle_root"]
    output_path.parent.mkdir(parents=True, exist_ok=True)
    archive_entries: list[tuple[str, bytes]] = [(f"{bundle_root}/{MANIFEST_NAME}", manifest_bytes(manifest))]
    for entry in files:
        relative_path = entry["path"]
        data = (root / relative_path).read_bytes()
        digest = hashlib.sha256(data).hexdigest()
        if digest != entry["sha256"]:
            raise BundleError(f"{relative_path} changed while the bundle was being written")
        archive_entries.append((entry["archive_path"], data))

    with output_path.open("wb") as raw_output:
        with gzip.GzipFile(filename="", mode="wb", fileobj=raw_output, mtime=0) as gz_output:
            with tarfile.open(fileobj=gz_output, mode="w") as tar:
                for archive_name, data in sorted(archive_entries):
                    add_bytes(tar, archive_name, data)


def assert_safe_archive_name(name: str) -> None:
    path = Path(name)
    if path.is_absolute() or ".." in path.parts:
        raise BundleError(f"unsafe archive member path: {name}")


def expected_archive_member_sizes(
    manifest: dict[str, Any],
    files: list[dict[str, Any]],
) -> dict[str, int]:
    expected = {f"{manifest['bundle_root']}/{MANIFEST_NAME}": len(manifest_bytes(manifest))}
    expected.update({entry["archive_path"]: entry["bytes"] for entry in files})
    return expected


def assert_reproducible_archive_member(member: tarfile.TarInfo, expected_size: int) -> None:
    if not member.isfile():
        raise BundleError(f"bundle member is not a regular file: {member.name}")
    if member.size != expected_size:
        raise BundleError(
            f"bundle member size mismatch: {member.name} "
            f"manifest={expected_size} archive={member.size}"
        )
    expected_metadata = {
        "mtime": 0,
        "uid": 0,
        "gid": 0,
        "uname": "",
        "gname": "",
        "mode": 0o644,
    }
    actual_metadata = {
        "mtime": member.mtime,
        "uid": member.uid,
        "gid": member.gid,
        "uname": member.uname,
        "gname": member.gname,
        "mode": member.mode,
    }
    for key, expected_value in expected_metadata.items():
        if actual_metadata[key] != expected_value:
            raise BundleError(
                f"bundle member metadata drift: {member.name} "
                f"{key}={actual_metadata[key]!r} expected={expected_value!r}"
            )


def validate_bundle(root: Path, output_path: Path, manifest: dict[str, Any]) -> None:
    files = validate_manifest_shape(manifest)
    expected_corpus_digest = manifest.get("evidence_corpus_sha256")
    actual_corpus_digest = evidence_corpus_digest(
        root,
        [entry["path"] for entry in files],
    )
    if actual_corpus_digest != expected_corpus_digest:
        raise BundleError("bundle manifest evidence_corpus_sha256 does not match the release evidence corpus")

    expected_names = {entry["archive_path"] for entry in files}
    expected_names.add(f"{manifest['bundle_root']}/{MANIFEST_NAME}")
    expected_sizes = expected_archive_member_sizes(manifest, files)

    with tarfile.open(output_path, "r:gz") as tar:
        members = tar.getmembers()
        names = [member.name for member in members]
        for name in names:
            assert_safe_archive_name(name)
        duplicate_names = sorted({name for name in names if names.count(name) > 1})
        if duplicate_names:
            raise BundleError(f"bundle contains duplicate archive members: {duplicate_names}")
        if names != sorted(names):
            raise BundleError("bundle entries must be sorted for reproducibility")
        if set(names) != expected_names:
            missing = sorted(expected_names.difference(names))
            unexpected = sorted(set(names).difference(expected_names))
            raise BundleError(f"bundle member drift; missing={missing}, unexpected={unexpected}")
        for member in members:
            assert_reproducible_archive_member(member, expected_sizes[member.name])

        bundled_manifest = tar.extractfile(f"{manifest['bundle_root']}/{MANIFEST_NAME}")
        if bundled_manifest is None:
            raise BundleError("bundle manifest is missing")
        bundled_manifest_payload = json.loads(bundled_manifest.read().decode("utf-8"))
        if bundled_manifest_payload != manifest:
            raise BundleError("bundle manifest content drifted")

        for entry in files:
            member = tar.extractfile(entry["archive_path"])
            if member is None:
                raise BundleError(f"bundle member missing: {entry['archive_path']}")
            data = member.read()
            if hashlib.sha256(data).hexdigest() != entry["sha256"]:
                raise BundleError(f"bundle checksum mismatch: {entry['archive_path']}")
            if data != (root / entry["path"]).read_bytes():
                raise BundleError(f"bundle content mismatch: {entry['archive_path']}")


def round_trip_bundle(root: Path, output_path: Path, manifest: dict[str, Any]) -> None:
    files = validate_manifest_shape(manifest)
    expected_names = sorted(
        [f"{manifest['bundle_root']}/{MANIFEST_NAME}"]
        + [entry["archive_path"] for entry in files]
    )
    expected_sizes = expected_archive_member_sizes(manifest, files)

    with tempfile.TemporaryDirectory(prefix="m7-release-evidence-roundtrip-") as temp_dir:
        extract_root = Path(temp_dir) / "extract"
        extract_root.mkdir()
        resolved_extract_root = extract_root.resolve(strict=True)

        with tarfile.open(output_path, "r:gz") as tar:
            seen_names: set[str] = set()
            for member in tar.getmembers():
                assert_safe_archive_name(member.name)
                if member.name in seen_names:
                    raise BundleError(f"bundle contains duplicate archive member: {member.name}")
                seen_names.add(member.name)
                if member.name not in expected_sizes:
                    raise BundleError(f"unexpected bundle member: {member.name}")
                assert_reproducible_archive_member(member, expected_sizes[member.name])
                member_stream = tar.extractfile(member)
                if member_stream is None:
                    raise BundleError(f"bundle member could not be read: {member.name}")
                target_path = extract_root / member.name
                resolved_target = target_path.resolve(strict=False)
                try:
                    resolved_target.relative_to(resolved_extract_root)
                except ValueError as error:
                    raise BundleError(f"bundle member escapes extraction root: {member.name}") from error
                target_path.parent.mkdir(parents=True, exist_ok=True)
                target_path.write_bytes(member_stream.read())

        extracted_names = sorted(
            path.relative_to(extract_root).as_posix()
            for path in extract_root.rglob("*")
            if path.is_file()
        )
        if extracted_names != expected_names:
            missing = sorted(set(expected_names).difference(extracted_names))
            unexpected = sorted(set(extracted_names).difference(expected_names))
            raise BundleError(
                f"round-trip extracted file drift; missing={missing}, unexpected={unexpected}"
            )

        manifest_path = extract_root / manifest["bundle_root"] / MANIFEST_NAME
        extracted_manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        if extracted_manifest != manifest:
            raise BundleError("round-trip manifest content drifted")

        for entry in files:
            data = (extract_root / entry["archive_path"]).read_bytes()
            if len(data) != entry["bytes"]:
                raise BundleError(f"round-trip byte count mismatch: {entry['archive_path']}")
            if hashlib.sha256(data).hexdigest() != entry["sha256"]:
                raise BundleError(f"round-trip checksum mismatch: {entry['archive_path']}")
            if data != (root / entry["path"]).read_bytes():
                raise BundleError(f"round-trip content mismatch: {entry['archive_path']}")


def expect_bundle_error(label: str, expected: str, action: Callable[[], None]) -> None:
    try:
        action()
    except BundleError as error:
        if expected not in str(error):
            raise BundleError(f"{label} failed for the wrong reason: {error}") from error
        return
    raise BundleError(f"{label} unexpectedly passed")


def clone_manifest(manifest: dict[str, Any]) -> dict[str, Any]:
    return json.loads(json.dumps(manifest))


def self_check(root: Path) -> None:
    synthetic_records = [
        ("b/release.json", b"{\"status\":\"accepted\"}\n"),
        ("a/readiness.json", b"{\"ready\":true}\n"),
    ]
    synthetic_digest = evidence_corpus_digest_from_records(list(reversed(synthetic_records)))
    if synthetic_digest != SYNTHETIC_CORPUS_DIGEST:
        raise BundleError("synthetic release evidence corpus digest drifted")
    mutated_digest = evidence_corpus_digest_from_records(
        [("b/release.json", b"{\"status\":\"blocked\"}\n"), synthetic_records[1]]
    )
    if mutated_digest == synthetic_digest:
        raise BundleError("release evidence corpus digest did not detect content drift")

    with tempfile.TemporaryDirectory(prefix="m7-release-evidence-bundle-") as temp_dir:
        output_path = Path(temp_dir) / "m7-release-evidence.tar.gz"
        manifest = build_manifest(root, DEFAULT_READINESS_SIDECAR, DEFAULT_BUNDLE_ROOT)
        write_bundle(root, output_path, manifest)
        validate_bundle(root, output_path, manifest)
        round_trip_bundle(root, output_path, manifest)

        metadata_drift_path = Path(temp_dir) / "m7-release-evidence-metadata-drift.tar.gz"
        files = validate_manifest_shape(manifest)
        archive_entries: list[tuple[str, bytes]] = [
            (f"{manifest['bundle_root']}/{MANIFEST_NAME}", manifest_bytes(manifest))
        ]
        for entry in files:
            archive_entries.append((entry["archive_path"], (root / entry["path"]).read_bytes()))
        with metadata_drift_path.open("wb") as raw_output:
            with gzip.GzipFile(filename="", mode="wb", fileobj=raw_output, mtime=0) as gz_output:
                with tarfile.open(fileobj=gz_output, mode="w") as tar:
                    for index, (archive_name, data) in enumerate(sorted(archive_entries)):
                        info = make_tar_info(archive_name, len(data))
                        if index == 0:
                            info.mtime = 1
                        tar.addfile(info, io.BytesIO(data))
        expect_bundle_error(
            "archive metadata reproducibility check",
            "bundle member metadata drift",
            lambda: validate_bundle(root, metadata_drift_path, manifest),
        )

        duplicate_member_path = Path(temp_dir) / "m7-release-evidence-duplicate-member.tar.gz"
        with duplicate_member_path.open("wb") as raw_output:
            with gzip.GzipFile(filename="", mode="wb", fileobj=raw_output, mtime=0) as gz_output:
                with tarfile.open(fileobj=gz_output, mode="w") as tar:
                    first_entry = sorted(archive_entries)[0]
                    for archive_name, data in sorted(archive_entries) + [first_entry]:
                        add_bytes(tar, archive_name, data)
        expect_bundle_error(
            "round-trip duplicate member check",
            "duplicate archive member",
            lambda: round_trip_bundle(root, duplicate_member_path, manifest),
        )

        bad_count_manifest = clone_manifest(manifest)
        bad_count_manifest["file_count"] = bad_count_manifest["file_count"] + 1
        expect_bundle_error(
            "manifest file_count coherence check",
            "file_count must match files length",
            lambda: validate_bundle(root, output_path, bad_count_manifest),
        )

        short_commit_manifest = clone_manifest(manifest)
        short_commit_manifest["source_commit"] = short_commit_manifest["source_commit"][:12]
        expect_bundle_error(
            "manifest source commit provenance check",
            "source_commit must be a full lowercase 40-character git SHA",
            lambda: validate_bundle(root, output_path, short_commit_manifest),
        )

        missing_readiness_manifest = clone_manifest(manifest)
        missing_readiness_manifest["readiness_sidecar"] = (
            "tools/reference-harness/specs/m7/lane3/not-bundled.json"
        )
        expect_bundle_error(
            "manifest readiness sidecar inclusion check",
            "readiness_sidecar must be included in files",
            lambda: validate_bundle(root, output_path, missing_readiness_manifest),
        )

        missing_claim_manifest = clone_manifest(manifest)
        missing_claim_manifest["withheld_claims"] = missing_claim_manifest["withheld_claims"][:-1]
        expect_bundle_error(
            "manifest withheld claims check",
            "withheld_claims must match the canonical release caveats",
            lambda: validate_bundle(root, output_path, missing_claim_manifest),
        )

        bad_archive_manifest = clone_manifest(manifest)
        bad_archive_manifest["files"][0]["archive_path"] = (
            f"{manifest['bundle_root']}/../escape.json"
        )
        expect_bundle_error(
            "manifest archive path coherence check",
            "archive_path must be a safe",
            lambda: write_bundle(root, output_path, bad_archive_manifest),
        )

        unsorted_manifest = clone_manifest(manifest)
        unsorted_manifest["files"] = list(reversed(unsorted_manifest["files"]))
        expect_bundle_error(
            "manifest sorted-path check",
            "files must be sorted",
            lambda: validate_bundle(root, output_path, unsorted_manifest),
        )
        expect_bundle_error(
            "readiness schema anchor check",
            "readiness sidecar must have schema release-readiness-output",
            lambda: collect_release_evidence_paths(
                root,
                Path("tools/reference-harness/specs/m7/lane133/release-qualification-corpus.json"),
            ),
        )
        expect_bundle_error(
            "M5 acceptance schema anchor check",
            "M5 acceptance sidecar must have schema m7-prerequisite-m5-packet-acceptance",
            lambda: require_m7_sidecar_schema(
                root,
                "tools/reference-harness/specs/m7/lane133/release-qualification-corpus.json",
                label="M5 acceptance sidecar",
                expected_schema="m7-prerequisite-m5-packet-acceptance",
            ),
        )

    required_paths = {
        DEFAULT_READINESS_SIDECAR.as_posix(),
        DEFAULT_M5_ACCEPTANCE_SIDECAR.as_posix(),
        "tools/reference-harness/specs/m7/lane133/release-qualification-corpus.json",
        "tools/reference-harness/specs/m7/lane70/release-performance-review.json",
        "tools/reference-harness/specs/m7/lane76/release-diagnostic-review.json",
        "tools/reference-harness/specs/m7/lane92/release-docs-completion.json",
        "tools/reference-harness/specs/m7/lane3/release-parity-signoff.post-a1f0e1d.json",
        "tools/reference-harness/specs/m5/m5-qualification-lane62.json",
        "tools/reference-harness/specs/m5/m5-feature-surface-lane50.json",
        "tools/reference-harness/specs/m5/m5-all-phase-closure-decision-lane62.json",
    }
    manifest_paths = {entry["path"] for entry in manifest["files"]}
    missing = sorted(required_paths.difference(manifest_paths))
    if missing:
        raise BundleError(f"self-check bundle omitted required release evidence: {missing}")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Package canonical M7 release evidence sidecars into a deterministic tar.gz archive.",
    )
    parser.add_argument(
        "--readiness-sidecar",
        type=Path,
        default=DEFAULT_READINESS_SIDECAR,
        help="Accepted release readiness sidecar to package.",
    )
    parser.add_argument(
        "--output",
        type=Path,
        help="Output tar.gz path. Required unless --self-check is used.",
    )
    parser.add_argument(
        "--bundle-root",
        default=DEFAULT_BUNDLE_ROOT,
        help="Top-level directory name inside the archive.",
    )
    parser.add_argument(
        "--self-check",
        action="store_true",
        help="Build and validate a temporary bundle for CTest.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    root = repo_root()

    try:
        if args.self_check:
            self_check(root)
            print("M7 release evidence bundle self-check passed")
            return 0

        if args.output is None:
            raise BundleError("--output is required unless --self-check is used")

        manifest = build_manifest(root, args.readiness_sidecar, args.bundle_root)
        write_bundle(root, args.output, manifest)
        validate_bundle(root, args.output, manifest)
        round_trip_bundle(root, args.output, manifest)
        print(
            json.dumps(
                {
                    "bundle": str(args.output),
                    "evidence_corpus_sha256": manifest["evidence_corpus_sha256"],
                    "file_count": manifest["file_count"],
                    "manifest": f"{manifest['bundle_root']}/{MANIFEST_NAME}",
                    "source_commit": manifest["source_commit"],
                },
                indent=2,
                sort_keys=True,
            )
        )
        return 0
    except BundleError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
