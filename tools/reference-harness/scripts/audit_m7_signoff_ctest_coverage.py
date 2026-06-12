#!/usr/bin/env python3
"""Audit post-closure CTest coverage against the M7 signoff gate manifest."""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Any


MANIFEST_PATH = Path("tools/reference-harness/specs/release/m7-signoff-ctest-gate-coverage.json")
CMAKE_PATH = Path("CMakeLists.txt")
ADD_TEST_RE = re.compile(r"add_test\s*\((.*?)\)", re.S)
TEST_NAME_RE = re.compile(r"\bNAME\s+([^\s\)]+)")
MIN_REASON_LENGTH = 24


class CoverageError(RuntimeError):
    """Raised when a CTest gate is missing signoff classification."""


@dataclass(frozen=True)
class GateEntry:
    name: str
    reason: str


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise CoverageError(message)


def read_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as stream:
        payload = json.load(stream)
    expect(isinstance(payload, dict), f"{path} must contain a JSON object")
    return payload


def write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def parse_ctest_names_from_text(cmake_text: str) -> list[str]:
    names: list[str] = []
    for match in ADD_TEST_RE.finditer(cmake_text):
        block = match.group(1)
        name_match = TEST_NAME_RE.search(block)
        expect(name_match is not None, "add_test block is missing NAME")
        names.append(name_match.group(1))
    duplicates = sorted(name for name in set(names) if names.count(name) > 1)
    expect(not duplicates, "duplicate CTest names in CMakeLists.txt: " + ", ".join(duplicates))
    return names


def current_ctest_names(root: Path) -> list[str]:
    cmake_path = root / CMAKE_PATH
    expect(cmake_path.is_file(), f"missing {CMAKE_PATH}")
    return parse_ctest_names_from_text(cmake_path.read_text(encoding="utf-8"))


def git_file_text(root: Path, commit: str, path: Path) -> str:
    completed = subprocess.run(
        ["git", "show", f"{commit}:{path.as_posix()}"],
        cwd=root,
        text=True,
        capture_output=True,
        check=False,
    )
    expect(
        completed.returncode == 0,
        f"failed to read {path} at {commit}: {completed.stderr.strip()}",
    )
    return completed.stdout


def baseline_ctest_names(root: Path, manifest: dict[str, Any]) -> list[str]:
    fixture_names = manifest.get("baseline_ctest_names")
    if fixture_names is not None:
        expect(isinstance(fixture_names, list), "baseline_ctest_names must be a list")
        names: list[str] = []
        for item in fixture_names:
            expect(isinstance(item, str) and item.strip(), "baseline_ctest_names entries must be strings")
            names.append(item.strip())
        duplicates = sorted(name for name in set(names) if names.count(name) > 1)
        expect(not duplicates, "duplicate baseline_ctest_names: " + ", ".join(duplicates))
        return names

    commit = manifest.get("post_closure_baseline_commit")
    expect(
        isinstance(commit, str) and re.fullmatch(r"[0-9a-f]{40}", commit) is not None,
        "post_closure_baseline_commit must be a full lowercase 40-character SHA",
    )
    return parse_ctest_names_from_text(git_file_text(root, commit, CMAKE_PATH))


def parse_gate_entries(raw: Any, label: str) -> list[GateEntry]:
    expect(isinstance(raw, list), f"{label} must be a list")
    entries: list[GateEntry] = []
    seen: set[str] = set()
    for index, item in enumerate(raw):
        expect(isinstance(item, dict), f"{label}[{index}] must be an object")
        name = item.get("name")
        reason = item.get("reason")
        expect(isinstance(name, str) and name.strip(), f"{label}[{index}].name must be a string")
        expect(isinstance(reason, str) and reason.strip(), f"{label}[{index}].reason must be a string")
        name = name.strip()
        reason = reason.strip()
        expect(
            len(reason) >= MIN_REASON_LENGTH,
            f"{label}[{index}] reason for {name} is too short to justify signoff classification",
        )
        expect(name not in seen, f"duplicate gate in {label}: {name}")
        seen.add(name)
        entries.append(GateEntry(name=name, reason=reason))
    return entries


def verify_manifest(root: Path, manifest_path: Path) -> dict[str, Any]:
    root = root.resolve()
    manifest = read_json(manifest_path)
    expect(manifest.get("schema_version") == 1, "manifest schema_version must be 1")
    expect(
        manifest.get("scope") == "m7-signoff-ctest-gate-coverage",
        "manifest scope must be m7-signoff-ctest-gate-coverage",
    )

    current_names = current_ctest_names(root)
    current_set = set(current_names)
    baseline_names = baseline_ctest_names(root, manifest)
    baseline_count = manifest.get("post_closure_baseline_test_count")
    if baseline_count is not None:
        expect(
            isinstance(baseline_count, int) and baseline_count == len(baseline_names),
            "post_closure_baseline_test_count does not match the parsed baseline CTest count",
        )
    baseline_set = set(baseline_names)
    post_closure_names = [name for name in current_names if name not in baseline_set]
    post_closure_set = set(post_closure_names)

    required_entries = parse_gate_entries(manifest.get("required_signoff_gates"), "required_signoff_gates")
    excluded_entries = parse_gate_entries(
        manifest.get("explicitly_excluded_post_closure_gates"),
        "explicitly_excluded_post_closure_gates",
    )
    required_set = {entry.name for entry in required_entries}
    excluded_set = {entry.name for entry in excluded_entries}

    overlap = sorted(required_set & excluded_set)
    expect(not overlap, "gates cannot be both required and excluded: " + ", ".join(overlap))

    missing_required = sorted(required_set - current_set)
    expect(not missing_required, "required signoff gates missing from CTest: " + ", ".join(missing_required))

    missing_excluded = sorted(excluded_set - current_set)
    expect(
        not missing_excluded,
        "excluded post-closure gates missing from CTest: " + ", ".join(missing_excluded),
    )

    stale_exclusions = sorted(excluded_set - post_closure_set)
    expect(
        not stale_exclusions,
        "excluded gates must be post-closure CTest additions: " + ", ".join(stale_exclusions),
    )

    unclassified = sorted(post_closure_set - required_set - excluded_set)
    expect(
        not unclassified,
        "post-closure CTest gates lack M7 signoff classification: " + ", ".join(unclassified),
    )

    return {
        "baseline_test_count": len(baseline_names),
        "current_test_count": len(current_names),
        "post_closure_test_count": len(post_closure_names),
        "required_signoff_gate_count": len(required_entries),
        "excluded_post_closure_gate_count": len(excluded_entries),
    }


def make_fixture(root: Path, manifest: dict[str, Any]) -> Path:
    write_text(
        root / CMAKE_PATH,
        """
include(CTest)
add_test(NAME old-base-test COMMAND old-base-test)
add_test(NAME new-signoff-test COMMAND new-signoff-test)
add_test(NAME new-excluded-test COMMAND new-excluded-test)
""".lstrip(),
    )
    manifest_path = root / MANIFEST_PATH
    manifest_path.parent.mkdir(parents=True, exist_ok=True)
    manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return manifest_path


def fixture_manifest() -> dict[str, Any]:
    return {
        "schema_version": 1,
        "scope": "m7-signoff-ctest-gate-coverage",
        "baseline_ctest_names": ["old-base-test"],
        "post_closure_baseline_test_count": 1,
        "required_signoff_gates": [
            {
                "name": "new-signoff-test",
                "reason": "Required fixture signoff gate with enough reason text.",
            }
        ],
        "explicitly_excluded_post_closure_gates": [
            {
                "name": "new-excluded-test",
                "reason": "Excluded fixture gate with an explicit release signoff rationale.",
            }
        ],
    }


def expect_fixture_failure(manifest: dict[str, Any], expected: str) -> None:
    with tempfile.TemporaryDirectory(prefix="m7-signoff-ctest-coverage-") as tmp:
        fixture = Path(tmp)
        manifest_path = make_fixture(fixture, manifest)
        try:
            verify_manifest(fixture, manifest_path)
        except CoverageError as exc:
            expect(expected in str(exc), f"unexpected self-check error: {exc}")
        else:
            raise CoverageError("self-check fixture unexpectedly passed")


def run_self_check() -> None:
    with tempfile.TemporaryDirectory(prefix="m7-signoff-ctest-coverage-") as tmp:
        fixture = Path(tmp)
        manifest_path = make_fixture(fixture, fixture_manifest())
        verify_manifest(fixture, manifest_path)

    missing = fixture_manifest()
    missing["explicitly_excluded_post_closure_gates"] = []
    expect_fixture_failure(missing, "lack M7 signoff classification")

    empty_reason = fixture_manifest()
    empty_reason["explicitly_excluded_post_closure_gates"] = [
        {"name": "new-excluded-test", "reason": "too short"}
    ]
    expect_fixture_failure(empty_reason, "reason for new-excluded-test is too short")

    missing_required = fixture_manifest()
    missing_required["required_signoff_gates"] = [
        {
            "name": "missing-required-test",
            "reason": "Missing required fixture gate should be rejected by this audit.",
        }
    ]
    expect_fixture_failure(missing_required, "required signoff gates missing from CTest")

    duplicate = fixture_manifest()
    duplicate["explicitly_excluded_post_closure_gates"] = [
        {
            "name": "new-signoff-test",
            "reason": "Duplicate classification should be rejected with enough reason text.",
        }
    ]
    expect_fixture_failure(duplicate, "both required and excluded")

    bad_count = fixture_manifest()
    bad_count["post_closure_baseline_test_count"] = 2
    expect_fixture_failure(bad_count, "baseline CTest count")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--verify",
        action="store_true",
        help="Validate committed post-closure CTest gate signoff coverage. This is the default.",
    )
    parser.add_argument(
        "--self-check",
        action="store_true",
        help="Run synthetic positive and negative checks for this validator.",
    )
    args = parser.parse_args(argv)

    try:
        if args.self_check:
            run_self_check()
            print("M7 signoff CTest coverage audit self-check passed")
            return 0
        summary = verify_manifest(repo_root(), repo_root() / MANIFEST_PATH)
        print(
            "M7 signoff CTest coverage audit passed: "
            f"{summary['post_closure_test_count']} post-closure tests classified "
            f"({summary['required_signoff_gate_count']} required, "
            f"{summary['excluded_post_closure_gate_count']} excluded)"
        )
        return 0
    except (CoverageError, OSError, json.JSONDecodeError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
