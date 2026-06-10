#!/usr/bin/env python3
"""CTest gate for the M7 release evidence corpus digest fixture."""

from __future__ import annotations

import difflib
import json
import sys
from pathlib import Path
from typing import Any

from package_m7_release_evidence import (
    DEFAULT_BUNDLE_ROOT,
    DEFAULT_READINESS_SIDECAR,
    EVIDENCE_CORPUS_DIGEST_ALGORITHM,
    BundleError,
    build_manifest,
    repo_root,
)


EXPECTED_DIGEST_FIXTURE = Path(
    "tools/reference-harness/specs/release/"
    "m7-release-evidence-manifest-digest.fixture.json"
)

WITHHELD_CLAIMS: tuple[str, ...] = (
    "This fixture hashes committed release evidence files only.",
    "This fixture does not run AMFlow numerics or create new retained benchmark evidence.",
    "This fixture does not widen release readiness or runtime behavior claims.",
)


def read_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as stream:
        payload = json.load(stream)
    if not isinstance(payload, dict):
        raise RuntimeError(f"{path} must contain a JSON object")
    return payload


def canonical_json(payload: dict[str, Any]) -> str:
    return json.dumps(payload, indent=2, sort_keys=True) + "\n"


def digest_fixture_payload(manifest: dict[str, Any]) -> dict[str, Any]:
    return {
        "schema_version": 1,
        "bundle_kind": "m7-release-evidence-corpus-digest",
        "readiness_sidecar": manifest["readiness_sidecar"],
        "bundle_root": manifest["bundle_root"],
        "file_count": manifest["file_count"],
        "evidence_corpus_digest_algorithm": manifest["evidence_corpus_digest_algorithm"],
        "evidence_corpus_sha256": manifest["evidence_corpus_sha256"],
        "digest_scope": "sorted repository-relative release evidence paths plus exact file bytes",
        "withheld_claims": list(WITHHELD_CLAIMS),
    }


def print_json_diff(expected: dict[str, Any], actual: dict[str, Any]) -> None:
    diff = difflib.unified_diff(
        canonical_json(expected).splitlines(keepends=True),
        canonical_json(actual).splitlines(keepends=True),
        fromfile=str(EXPECTED_DIGEST_FIXTURE),
        tofile="package_m7_release_evidence.py manifest digest",
        n=3,
    )
    print("M7 release evidence corpus digest fixture drifted", file=sys.stderr)
    print("".join(diff), file=sys.stderr)


def main() -> int:
    root = repo_root()
    try:
        manifest = build_manifest(root, DEFAULT_READINESS_SIDECAR, DEFAULT_BUNDLE_ROOT)
    except BundleError as error:
        print(f"release evidence manifest build failed: {error}", file=sys.stderr)
        return 1

    actual = digest_fixture_payload(manifest)
    if actual.get("evidence_corpus_digest_algorithm") != EVIDENCE_CORPUS_DIGEST_ALGORITHM:
        print("release evidence digest fixture used an unsupported algorithm", file=sys.stderr)
        return 1

    expected = read_json(root / EXPECTED_DIGEST_FIXTURE)
    if actual != expected:
        print_json_diff(expected, actual)
        return 1

    print("M7 release evidence corpus digest fixture gate passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
