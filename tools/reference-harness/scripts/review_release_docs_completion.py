#!/usr/bin/env python3
"""Produce the M7 release-docs-completion sidecar for release readiness."""

from __future__ import annotations

import argparse
import json
import tempfile
from pathlib import Path
from typing import Any

from freeze_phase0_goldens import load_json


DOC_TARGET_MARKERS: dict[str, tuple[str, ...]] = {
    "docs/public-contract.md": (
        "release_signoff_readiness.py",
        "review_release_docs_completion.py",
        "docs-completion",
        "release-docs-completion",
    ),
    "docs/implementation-ledger.md": (
        "Milestone M7 docs-completion release-readiness producer",
        "review_release_docs_completion.py",
        "release-docs-completion",
    ),
    "docs/verification-strategy.md": (
        "release_signoff_readiness.py",
        "review_release_docs_completion.py",
        "docs-completion",
        "release-docs-completion",
    ),
    "docs/reference-harness.md": (
        "release_signoff_readiness.py",
        "review_release_docs_completion.py",
        "docs-completion",
        "release-docs-completion",
    ),
    "tools/reference-harness/README.md": (
        "release_signoff_readiness.py",
        "review_release_docs_completion.py",
        "--summary-path",
        "release-docs-completion",
    ),
    "docs/full-amflow-completion-roadmap.md": (
        "Milestone M7 docs-completion release-readiness producer",
        "review_release_docs_completion.py",
        "release-docs-completion",
    ),
}

DOC_TARGET_FIELDS: dict[str, str] = {
    "docs/public-contract.md": "public_contract_aligned",
    "docs/implementation-ledger.md": "implementation_ledger_aligned",
    "docs/verification-strategy.md": "verification_strategy_aligned",
    "docs/reference-harness.md": "reference_harness_guide_aligned",
    "tools/reference-harness/README.md": "reference_harness_readme_aligned",
    "docs/full-amflow-completion-roadmap.md": "completion_roadmap_aligned",
}

REQUIRED_NON_CLAIM_MARKERS: tuple[str, ...] = (
    "Milestone M6",
    "Milestone M7",
    "release readiness",
    "captured benchmark",
    "runtime",
)

WITHHELD_CLAIMS: tuple[str, ...] = (
    "This summary does not claim Milestone M6 closure.",
    "This summary does not claim Milestone M7 closure.",
    "This summary does not claim release readiness.",
    "This summary does not claim new captured benchmark evidence.",
    "This summary does not widen runtime or public behavior.",
)


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def write_json(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def normalize_string_list(raw: Any, label: str) -> list[str]:
    if raw is None:
        return []
    if not isinstance(raw, list):
        raise TypeError(f"{label} must be a list, got {type(raw).__name__}")
    values: list[str] = []
    for item in raw:
        if not isinstance(item, str):
            raise TypeError(f"{label} entries must be strings, got {type(item).__name__}")
        value = item.strip()
        if not value:
            raise ValueError(f"{label} entries must not be empty")
        values.append(value)
    return values


def expect_unique(values: list[str], label: str) -> None:
    expect(len(set(values)) == len(values), f"{label} must not contain duplicates")


def expect_path_within_root(path: Path, root: Path, label: str) -> None:
    resolved_path = path.resolve(strict=False)
    resolved_root = root.resolve(strict=False)
    try:
        resolved_path.relative_to(resolved_root)
    except ValueError as error:
        raise RuntimeError(f"{label} must stay under {root}: {path}") from error


def load_release_checklist(checklist_path: Path) -> dict[str, Any]:
    checklist = load_json(checklist_path)
    expect(checklist.get("schema_version") == 1, "release checklist schema_version must be 1")

    sources = checklist.get("sources")
    if not isinstance(sources, dict):
        raise TypeError("release checklist sources must be an object")
    normalized_sources: dict[str, str] = {}
    for key, value in sources.items():
        if not isinstance(key, str) or not key.strip():
            raise ValueError("release checklist source id must be a non-empty string")
        if not isinstance(value, str) or not value.strip():
            raise ValueError(f"release checklist source {key} must be a non-empty string path")
        normalized_sources[key.strip()] = value.strip()

    docs_completion_targets = checklist.get("docs_completion_targets")
    if not isinstance(docs_completion_targets, list):
        raise TypeError("release checklist docs_completion_targets must be a list")
    normalized_targets: list[dict[str, str]] = []
    doc_target_paths: list[str] = []
    for target in docs_completion_targets:
        if not isinstance(target, dict):
            raise TypeError("release checklist docs_completion_targets entries must be objects")
        path = str(target.get("path", "")).strip()
        if not path:
            raise ValueError("release checklist docs_completion_targets path must not be empty")
        expectation = str(target.get("expectation", "")).strip()
        if not expectation:
            raise ValueError(
                f"release checklist docs_completion_targets expectation must not be empty for {path}"
            )
        normalized_targets.append({"path": path, "expectation": expectation})
        doc_target_paths.append(path)
    expect_unique(doc_target_paths, "release checklist docs_completion_targets paths")

    explicit_non_claims = normalize_string_list(
        checklist.get("explicit_non_claims", []),
        "release checklist explicit_non_claims",
    )

    return {
        **checklist,
        "sources": normalized_sources,
        "docs_completion_targets": normalized_targets,
        "explicit_non_claims": explicit_non_claims,
    }


def explicit_non_claims_cover_release_blockers(non_claims: list[str]) -> bool:
    combined = "\n".join(non_claims)
    return all(marker in combined for marker in REQUIRED_NON_CLAIM_MARKERS)


def audit_target_markers(root: Path, relative_path: str) -> tuple[bool, list[str]]:
    target_path = root / relative_path
    if not target_path.exists():
        return False, list(DOC_TARGET_MARKERS.get(relative_path, ()))
    content = target_path.read_text(encoding="utf-8")
    missing_markers = [
        marker for marker in DOC_TARGET_MARKERS.get(relative_path, ()) if marker not in content
    ]
    return not missing_markers, missing_markers


def summarize_docs_completion(*, checklist_path: Path, root: Path) -> dict[str, Any]:
    root = root.resolve(strict=False)
    checklist_path = checklist_path.resolve(strict=False)
    expect_path_within_root(checklist_path, root, "release checklist path")
    checklist = load_release_checklist(checklist_path)

    checklist_sources_present = True
    missing_sources: list[str] = []
    for source_id, relative_path in checklist["sources"].items():
        source_path = root / relative_path
        expect_path_within_root(source_path, root, f"release checklist source {source_id}")
        if not source_path.exists():
            checklist_sources_present = False
            missing_sources.append(f"{source_id}:{relative_path}")

    target_paths = [target["path"] for target in checklist["docs_completion_targets"]]
    expected_target_paths = list(DOC_TARGET_MARKERS)
    target_set_matches_producer = set(target_paths) == set(expected_target_paths)

    docs_targets_exist = True
    aligned_fields = {field: False for field in DOC_TARGET_FIELDS.values()}
    missing_or_stale_doc_paths: list[str] = []
    marker_blockers: list[str] = []

    for target in checklist["docs_completion_targets"]:
        relative_path = target["path"]
        target_path = root / relative_path
        expect_path_within_root(target_path, root, f"docs completion target {relative_path}")
        exists = target_path.exists()
        docs_targets_exist = docs_targets_exist and exists
        markers_aligned, missing_markers = audit_target_markers(root, relative_path)
        if relative_path in DOC_TARGET_FIELDS:
            aligned_fields[DOC_TARGET_FIELDS[relative_path]] = exists and markers_aligned
        if not exists:
            missing_or_stale_doc_paths.append(relative_path)
            marker_blockers.append(f"docs target path is missing: {relative_path}")
        elif missing_markers:
            missing_or_stale_doc_paths.append(relative_path)
            marker_blockers.append(
                "docs target markers missing: "
                + relative_path
                + " -> "
                + ", ".join(missing_markers)
            )

    unknown_target_paths = sorted(set(target_paths) - set(expected_target_paths))
    missing_expected_target_paths = sorted(set(expected_target_paths) - set(target_paths))
    if unknown_target_paths:
        marker_blockers.append(
            "release checklist contains docs-completion targets not covered by producer: "
            + ", ".join(unknown_target_paths)
        )
    if missing_expected_target_paths:
        marker_blockers.append(
            "release checklist omits docs-completion targets required by producer: "
            + ", ".join(missing_expected_target_paths)
        )

    explicit_non_claims_reviewed = explicit_non_claims_cover_release_blockers(
        checklist["explicit_non_claims"]
    )
    if not explicit_non_claims_reviewed:
        marker_blockers.append("release checklist explicit non-claims are incomplete")

    if missing_sources:
        marker_blockers.extend(
            f"release checklist source path is missing: {source}" for source in missing_sources
        )

    docs_targets_reviewed = docs_targets_exist and target_set_matches_producer
    completion_booleans = {
        "docs_targets_reviewed": docs_targets_reviewed,
        "explicit_non_claims_reviewed": explicit_non_claims_reviewed,
        **aligned_fields,
    }
    docs_completion_review_complete = (
        checklist_sources_present
        and all(completion_booleans.values())
        and not missing_or_stale_doc_paths
        and not marker_blockers
    )

    return {
        "schema_version": 1,
        "scope": "release-docs-completion",
        "current_state": (
            "docs-completion-reviewed"
            if docs_completion_review_complete
            else "blocked-on-doc-alignment-review"
        ),
        "checklist_path": str(checklist_path),
        "docs_completion_review_complete": docs_completion_review_complete,
        **completion_booleans,
        "reviewed_doc_targets": target_paths,
        "missing_or_stale_doc_paths": sorted(set(missing_or_stale_doc_paths)),
        "blocking_reasons": marker_blockers,
        "withheld_claims": list(WITHHELD_CLAIMS),
    }


def write_text(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")


def write_synthetic_release_docs_root(root: Path, *, complete_non_claims: bool = True) -> Path:
    checklist_path = root / "tools/reference-harness/templates/release-signoff-checklist.json"
    sources = {
        "release_signoff_markdown": "docs/release-signoff-checklist.md",
        "qualification_scaffold": "tools/reference-harness/templates/qualification-benchmarks.json",
        "qualification_readiness_helper": "tools/reference-harness/scripts/qualification_readiness.py",
        "release_readiness_helper": "tools/reference-harness/scripts/release_signoff_readiness.py",
        "docs_completion_review_helper": "tools/reference-harness/scripts/review_release_docs_completion.py",
        "parity_matrix": "specs/parity-matrix.yaml",
        "verification_strategy": "docs/verification-strategy.md",
        "roadmap": "docs/full-amflow-completion-roadmap.md",
        "public_contract": "docs/public-contract.md",
        "implementation_ledger": "docs/implementation-ledger.md",
    }
    explicit_non_claims = [
        "Editing this scaffold does not claim Milestone M6 closure.",
        "Editing this scaffold does not claim Milestone M7 closure.",
        "Editing this scaffold does not claim release readiness.",
        "Editing this scaffold does not claim new captured benchmark evidence.",
        "Editing this scaffold does not widen the reviewed runtime or public contract.",
    ]
    if not complete_non_claims:
        explicit_non_claims = explicit_non_claims[:2]

    checklist = {
        "schema_version": 1,
        "current_state": "planning-only",
        "sources": sources,
        "docs_completion_targets": [
            {
                "path": path,
                "expectation": "Synthetic docs target preserves the release-docs-completion markers.",
            }
            for path in DOC_TARGET_MARKERS
        ],
        "explicit_non_claims": explicit_non_claims,
    }
    write_json(checklist_path, checklist)

    for relative_path in set(sources.values()) | set(DOC_TARGET_MARKERS):
        markers = "\n".join(DOC_TARGET_MARKERS.get(relative_path, ()))
        write_text(
            root / relative_path,
            "Synthetic release docs target\n"
            "release_signoff_readiness.py\n"
            "review_release_docs_completion.py\n"
            "docs-completion\n"
            "release-docs-completion\n"
            "Milestone M7 docs-completion release-readiness producer\n"
            f"{markers}\n",
        )
    return checklist_path


def run_self_check() -> dict[str, Any]:
    with tempfile.TemporaryDirectory(prefix="amflow-release-docs-completion-self-check-") as tmp:
        temp_root = Path(tmp)
        checklist_path = write_synthetic_release_docs_root(temp_root)
        summary_path = temp_root / "docs-completion-summary.json"
        summary = summarize_docs_completion(checklist_path=checklist_path, root=temp_root)
        write_json(summary_path, summary)
        summary_written = summary_path.exists()

        from release_signoff_readiness import load_docs_completion_summary

        loaded_summary = load_docs_completion_summary(summary_path)

        stale_public_contract = temp_root / "docs/public-contract.md"
        stale_public_contract.write_text(
            stale_public_contract.read_text(encoding="utf-8").replace(
                "release-docs-completion",
                "release docs completion",
            ),
            encoding="utf-8",
        )
        stale_summary = summarize_docs_completion(checklist_path=checklist_path, root=temp_root)

    with tempfile.TemporaryDirectory(prefix="amflow-release-docs-nonclaims-self-check-") as tmp:
        nonclaim_root = Path(tmp)
        nonclaim_checklist = write_synthetic_release_docs_root(
            nonclaim_root,
            complete_non_claims=False,
        )
        nonclaim_summary = summarize_docs_completion(
            checklist_path=nonclaim_checklist,
            root=nonclaim_root,
        )

    return {
        "docs_completion_review_complete": summary["docs_completion_review_complete"],
        "docs_completion_target_markers_preserved": (
            not summary["missing_or_stale_doc_paths"] and not summary["blocking_reasons"]
        ),
        "release_readiness_schema_compatible": (
            loaded_summary["scope"] == "release-docs-completion"
            and loaded_summary["docs_completion_review_complete"]
        ),
        "missing_marker_blocked": (
            not stale_summary["docs_completion_review_complete"]
            and "docs/public-contract.md" in stale_summary["missing_or_stale_doc_paths"]
        ),
        "explicit_non_claims_reviewed": summary["explicit_non_claims_reviewed"],
        "incomplete_non_claims_blocked": (
            not nonclaim_summary["docs_completion_review_complete"]
            and not nonclaim_summary["explicit_non_claims_reviewed"]
        ),
        "summary_written": summary_written,
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--checklist-path",
        type=Path,
        help="Release-signoff checklist JSON path",
    )
    parser.add_argument(
        "--summary-path",
        type=Path,
        help="Optional output file for the docs-completion sidecar summary",
    )
    parser.add_argument(
        "--self-check",
        action="store_true",
        help="Run a synthetic docs-completion sidecar producer check",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if args.self_check:
        print(json.dumps(run_self_check(), indent=2, sort_keys=True))
        return 0

    root = repo_root()
    checklist_path = (
        args.checklist_path
        if args.checklist_path is not None
        else root / "tools" / "reference-harness" / "templates" / "release-signoff-checklist.json"
    )
    summary = summarize_docs_completion(checklist_path=checklist_path, root=root)
    if args.summary_path is not None:
        write_json(args.summary_path, summary)
    print(json.dumps(summary, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
