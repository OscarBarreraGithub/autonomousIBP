#!/usr/bin/env python3
"""Validate AMFlow live-rerun freshness metadata discipline."""

from __future__ import annotations

import argparse
import json
import re
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Any


RELEASE_DOCS_ROOT = Path("docs/release")
REGISTRY_PATH = RELEASE_DOCS_ROOT / "amflow-live-rerun-freshness-registry.json"
LIVE_RERUN_DOC_PATTERN = "amflow-live-rerun-*.md"
PENDING_STATUS = "pending-first-re-verify"
LAST_REVERIFIED_LINE_PATTERN = re.compile(
    r"(?m)^last-re-verified:[ \t]*([0-9]{4}-[0-9]{2}-[0-9]{2})(?:[ \t]+\([^)\n]+\))?[ \t]*$"
)
LAST_REVERIFIED_PREFIX_PATTERN = re.compile(r"(?m)^last-re-verified:\s*(.+)$")


@dataclass(frozen=True)
class PendingEntry:
    example: str
    path: Path
    status: str


@dataclass(frozen=True)
class FreshnessRecord:
    example: str
    path: str
    last_reverified: str | None
    pending_first_reverify: bool


class FreshnessError(RuntimeError):
    """Raised when the freshness validator self-check cannot continue."""


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise FreshnessError(message)


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def relative_to_root(path: Path, root: Path) -> Path:
    return path.relative_to(root)


def example_from_live_doc_path(path: Path) -> str:
    return path.stem.removeprefix("amflow-live-rerun-")


def parse_last_reverified(path: Path, root: Path, errors: list[str]) -> str | None:
    relative_path = str(relative_to_root(path, root))
    text = path.read_text(encoding="utf-8")
    last_reverified_lines = LAST_REVERIFIED_PREFIX_PATTERN.findall(text)
    if len(last_reverified_lines) > 1:
        errors.append(f"{relative_path} must contain at most one last-re-verified line")
    if not last_reverified_lines:
        return None

    match = LAST_REVERIFIED_LINE_PATTERN.search(text)
    if match is None:
        errors.append(f"{relative_path} has malformed last-re-verified line")
        return None
    return match.group(0)


def load_registry(root: Path, errors: list[str]) -> dict[Path, PendingEntry]:
    registry_path = root / REGISTRY_PATH
    if not registry_path.is_file():
        errors.append(f"{REGISTRY_PATH} is missing")
        return {}

    try:
        payload = json.loads(registry_path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        errors.append(f"{REGISTRY_PATH} is not valid JSON: {exc}")
        return {}

    if not isinstance(payload, dict):
        errors.append(f"{REGISTRY_PATH} must contain a JSON object")
        return {}

    raw_entries = payload.get("pending_first_reverify")
    if not isinstance(raw_entries, list):
        errors.append(f"{REGISTRY_PATH} must contain a pending_first_reverify list")
        return {}

    pending_by_path: dict[Path, PendingEntry] = {}
    seen_examples: set[str] = set()
    for index, raw_entry in enumerate(raw_entries):
        label = f"{REGISTRY_PATH}:pending_first_reverify[{index}]"
        if not isinstance(raw_entry, dict):
            errors.append(f"{label} must be an object")
            continue

        path_value = raw_entry.get("path")
        status_value = raw_entry.get("status")
        example_value = raw_entry.get("example")

        if not isinstance(path_value, str) or not path_value:
            errors.append(f"{label}.path must be a nonempty repo-relative path")
            continue
        if not isinstance(status_value, str):
            errors.append(f"{label}.status must be `{PENDING_STATUS}`")
            status_value = ""
        if status_value != PENDING_STATUS:
            errors.append(f"{label} must use status `{PENDING_STATUS}`")

        candidate = Path(path_value)
        if candidate.is_absolute() or ".." in candidate.parts:
            errors.append(f"{label}.path must stay within this repository: {path_value}")
            continue
        if candidate.parent != RELEASE_DOCS_ROOT or not candidate.match(LIVE_RERUN_DOC_PATTERN):
            errors.append(f"{label}.path must name a docs/release/amflow-live-rerun-*.md file")
            continue

        example = example_from_live_doc_path(candidate)
        if not isinstance(example_value, str) or not example_value:
            errors.append(f"{label}.example must name the live-rerun example")
        elif example_value != example:
            errors.append(f"{label}.example `{example_value}` does not match `{candidate}`")
        if candidate in pending_by_path:
            errors.append(f"{REGISTRY_PATH} duplicates pending path {candidate}")
            continue
        if example in seen_examples:
            errors.append(f"{REGISTRY_PATH} duplicates pending example {example}")
            continue
        seen_examples.add(example)
        pending_by_path[candidate] = PendingEntry(
            example=example,
            path=candidate,
            status=status_value,
        )

    return pending_by_path


def validate_root(root: Path) -> tuple[dict[str, Any], list[str]]:
    errors: list[str] = []
    docs_root = root / RELEASE_DOCS_ROOT
    live_docs = sorted(docs_root.glob(LIVE_RERUN_DOC_PATTERN))
    if not live_docs:
        errors.append(f"no live-rerun documents found under {RELEASE_DOCS_ROOT}")

    pending_by_path = load_registry(root, errors)
    live_doc_paths = {relative_to_root(path, root) for path in live_docs}
    for pending_path in sorted(pending_by_path):
        if pending_path not in live_doc_paths:
            errors.append(
                f"{REGISTRY_PATH} pending entry {pending_path} does not match a live-rerun doc"
            )

    records: list[FreshnessRecord] = []
    for path in live_docs:
        relative_path = relative_to_root(path, root)
        example = example_from_live_doc_path(path)
        last_reverified = parse_last_reverified(path, root, errors)
        pending_first_reverify = relative_path in pending_by_path

        if last_reverified is not None and pending_first_reverify:
            errors.append(
                f"{relative_path} has last-re-verified metadata and must not remain {PENDING_STATUS}"
            )
        if last_reverified is None and not pending_first_reverify:
            errors.append(
                f"{relative_path} missing freshness metadata: add last-re-verified after a real rerun "
                f"or register it as {PENDING_STATUS} in {REGISTRY_PATH}"
            )

        records.append(
            FreshnessRecord(
                example=example,
                path=str(relative_path),
                last_reverified=last_reverified,
                pending_first_reverify=pending_first_reverify,
            )
        )

    summary: dict[str, Any] = {
        "live_doc_count": len(live_docs),
        "last_reverified_count": sum(1 for record in records if record.last_reverified is not None),
        "pending_first_reverify_count": sum(
            1 for record in records if record.pending_first_reverify
        ),
        "docs": [
            {
                "example": record.example,
                "path": record.path,
                "last_reverified": record.last_reverified,
                "pending_first_reverify": record.pending_first_reverify,
            }
            for record in records
        ],
    }
    return summary, errors


def fixture_doc(example: str, *, last_reverified: bool = False) -> str:
    lines = [
        f"# AMFlow Live Rerun: {example}",
        "",
        "## Example",
        "",
        f"- Example: `{example}`",
        "",
        "## Status",
        "",
        "`reproduced-fully-live`",
        "",
    ]
    if last_reverified:
        lines.append("last-re-verified: 2026-06-12 (fixture matched retained output)")
        lines.append("")
    return "\n".join(lines)


def registry_fixture(entries: list[dict[str, str]]) -> str:
    return json.dumps({"pending_first_reverify": entries}, indent=2, sort_keys=True) + "\n"


def pending_entry(example: str, *, status: str = PENDING_STATUS) -> dict[str, str]:
    return {
        "example": example,
        "path": str(RELEASE_DOCS_ROOT / f"amflow-live-rerun-{example}.md"),
        "status": status,
    }


def write_fixture_root(root: Path) -> None:
    docs_root = root / RELEASE_DOCS_ROOT
    docs_root.mkdir(parents=True, exist_ok=True)
    (docs_root / "amflow-live-rerun-alpha.md").write_text(
        fixture_doc("alpha", last_reverified=True), encoding="utf-8"
    )
    (docs_root / "amflow-live-rerun-beta.md").write_text(
        fixture_doc("beta"), encoding="utf-8"
    )
    (root / REGISTRY_PATH).write_text(
        registry_fixture([pending_entry("beta")]), encoding="utf-8"
    )


def run_fixture_validation(mutator=None) -> tuple[dict[str, Any], list[str]]:
    with tempfile.TemporaryDirectory(prefix="amflow-live-rerun-freshness-") as temp_dir:
        root = Path(temp_dir)
        write_fixture_root(root)
        if mutator is not None:
            mutator(root)
        return validate_root(root)


def expect_self_check_failure(label: str, mutator, expected: str) -> bool:
    _, errors = run_fixture_validation(mutator)
    expect(errors, f"{label} unexpectedly passed")
    detail = "\n".join(errors)
    expect(expected in detail, f"{label} failed for the wrong reason: {detail}")
    return True


def run_self_check() -> dict[str, Any]:
    summary, errors = run_fixture_validation()
    expect(not errors, "valid live-rerun freshness fixture failed: " + "; ".join(errors))
    expect(summary["live_doc_count"] == 2, "valid fixture live doc count drifted")
    expect(summary["last_reverified_count"] == 1, "valid fixture reverified count drifted")
    expect(summary["pending_first_reverify_count"] == 1, "valid fixture pending count drifted")

    checks = {
        "valid_fixture": True,
        "missing_freshness_rejected": expect_self_check_failure(
            "missing-freshness",
            lambda root: (root / REGISTRY_PATH).write_text(
                registry_fixture([]), encoding="utf-8"
            ),
            "missing freshness metadata",
        ),
        "missing_registry_rejected": expect_self_check_failure(
            "missing-registry",
            lambda root: (root / REGISTRY_PATH).unlink(),
            f"{REGISTRY_PATH} is missing",
        ),
        "malformed_last_reverified_rejected": expect_self_check_failure(
            "malformed-last-reverified",
            lambda root: (root / RELEASE_DOCS_ROOT / "amflow-live-rerun-alpha.md").write_text(
                fixture_doc("alpha") + "last-re-verified: June 12, 2026\n",
                encoding="utf-8",
            ),
            "malformed last-re-verified line",
        ),
        "bad_pending_status_rejected": expect_self_check_failure(
            "bad-pending-status",
            lambda root: (root / REGISTRY_PATH).write_text(
                registry_fixture([pending_entry("beta", status="pending")]),
                encoding="utf-8",
            ),
            f"must use status `{PENDING_STATUS}`",
        ),
        "duplicate_pending_rejected": expect_self_check_failure(
            "duplicate-pending",
            lambda root: (root / REGISTRY_PATH).write_text(
                registry_fixture([pending_entry("beta"), pending_entry("beta")]),
                encoding="utf-8",
            ),
            "duplicates pending path",
        ),
        "stale_pending_rejected": expect_self_check_failure(
            "stale-pending",
            lambda root: (root / REGISTRY_PATH).write_text(
                registry_fixture([pending_entry("alpha"), pending_entry("beta")]),
                encoding="utf-8",
            ),
            f"must not remain {PENDING_STATUS}",
        ),
        "unknown_pending_doc_rejected": expect_self_check_failure(
            "unknown-pending-doc",
            lambda root: (root / REGISTRY_PATH).write_text(
                registry_fixture([pending_entry("beta"), pending_entry("gamma")]),
                encoding="utf-8",
            ),
            "does not match a live-rerun doc",
        ),
    }
    expect(all(checks.values()), "live-rerun freshness self-check failed")
    return {
        "self_check_passed": True,
        "checks": checks,
        "valid_summary": summary,
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--verify",
        action="store_true",
        help="Validate committed live-rerun freshness metadata. This is the default unless --self-check is used.",
    )
    parser.add_argument(
        "--self-check",
        action="store_true",
        help="Run fixture-backed positive and negative checks for this validator.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        if args.self_check:
            print(json.dumps(run_self_check(), indent=2, sort_keys=True))
            return 0
        summary, errors = validate_root(repo_root())
    except FreshnessError as error:
        print(f"AMFlow live-rerun freshness validation failed: {error}", file=sys.stderr)
        return 1

    if errors:
        print("AMFlow live-rerun freshness validation failed:", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        return 1

    print(
        "AMFlow live-rerun freshness validation passed: "
        f"{summary['live_doc_count']} live rerun doc(s), "
        f"{summary['last_reverified_count']} last-re-verified line(s), "
        f"{summary['pending_first_reverify_count']} pending-first-re-verify entry(s)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
