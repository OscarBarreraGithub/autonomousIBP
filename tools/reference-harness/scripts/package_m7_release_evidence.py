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
from typing import Any


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


def collect_release_evidence_paths(root: Path, readiness_sidecar: Path) -> list[str]:
    evidence_paths: list[str] = []
    readiness_rel = require_repo_file(root, readiness_sidecar.as_posix(), "readiness sidecar")
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
    bundle_root = manifest["bundle_root"]
    output_path.parent.mkdir(parents=True, exist_ok=True)
    manifest_bytes = json.dumps(manifest, indent=2, sort_keys=True).encode("utf-8") + b"\n"
    archive_entries: list[tuple[str, bytes]] = [(f"{bundle_root}/{MANIFEST_NAME}", manifest_bytes)]
    for entry in manifest["files"]:
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


def validate_bundle(root: Path, output_path: Path, manifest: dict[str, Any]) -> None:
    expected_corpus_digest = manifest.get("evidence_corpus_sha256")
    if (
        not isinstance(expected_corpus_digest, str)
        or len(expected_corpus_digest) != 64
        or any(char not in "0123456789abcdef" for char in expected_corpus_digest)
    ):
        raise BundleError("bundle manifest must publish a 64-character evidence_corpus_sha256")
    if manifest.get("evidence_corpus_digest_algorithm") != EVIDENCE_CORPUS_DIGEST_ALGORITHM:
        raise BundleError("bundle manifest has an unsupported evidence corpus digest algorithm")
    actual_corpus_digest = evidence_corpus_digest(
        root,
        [entry["path"] for entry in manifest.get("files", [])],
    )
    if actual_corpus_digest != expected_corpus_digest:
        raise BundleError("bundle manifest evidence_corpus_sha256 does not match the release evidence corpus")

    expected_names = {entry["archive_path"] for entry in manifest["files"]}
    expected_names.add(f"{manifest['bundle_root']}/{MANIFEST_NAME}")

    with tarfile.open(output_path, "r:gz") as tar:
        members = tar.getmembers()
        names = [member.name for member in members]
        for name in names:
            assert_safe_archive_name(name)
        if names != sorted(names):
            raise BundleError("bundle entries must be sorted for reproducibility")
        if set(names) != expected_names:
            missing = sorted(expected_names.difference(names))
            unexpected = sorted(set(names).difference(expected_names))
            raise BundleError(f"bundle member drift; missing={missing}, unexpected={unexpected}")

        bundled_manifest = tar.extractfile(f"{manifest['bundle_root']}/{MANIFEST_NAME}")
        if bundled_manifest is None:
            raise BundleError("bundle manifest is missing")
        bundled_manifest_payload = json.loads(bundled_manifest.read().decode("utf-8"))
        if bundled_manifest_payload != manifest:
            raise BundleError("bundle manifest content drifted")

        for entry in manifest["files"]:
            member = tar.extractfile(entry["archive_path"])
            if member is None:
                raise BundleError(f"bundle member missing: {entry['archive_path']}")
            data = member.read()
            if hashlib.sha256(data).hexdigest() != entry["sha256"]:
                raise BundleError(f"bundle checksum mismatch: {entry['archive_path']}")
            if data != (root / entry["path"]).read_bytes():
                raise BundleError(f"bundle content mismatch: {entry['archive_path']}")


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
