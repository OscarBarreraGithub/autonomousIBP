#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-autoIBP-TBD
"""Verify the pinned configured CTest test count."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Any


DEFAULT_REGISTRY = Path("tools/reference-harness/specs/release/ctest-test-count-registry.json")
SCOPE = "release-ctest-test-count-registry"


class CTestCountError(RuntimeError):
    """Raised when the configured CTest test count drops below the pin."""


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise CTestCountError(message)


def read_json(path: Path) -> dict[str, Any]:
    try:
        with path.open("r", encoding="utf-8") as stream:
            payload = json.load(stream)
    except json.JSONDecodeError as exc:
        raise CTestCountError(f"{path} is not valid JSON: {exc}") from exc
    expect(isinstance(payload, dict), f"{path} must contain a JSON object")
    return payload


def require_string(value: Any, label: str) -> str:
    expect(isinstance(value, str) and bool(value.strip()), f"{label} must be a nonempty string")
    expect(value == value.strip(), f"{label} must not carry surrounding whitespace")
    return value


def require_positive_int(value: Any, label: str) -> int:
    expect(type(value) is int and value > 0, f"{label} must be a positive integer")
    return value


def require_string_list(value: Any, label: str) -> list[str]:
    expect(isinstance(value, list), f"{label} must be a list")
    names = [require_string(item, f"{label}[{index}]") for index, item in enumerate(value)]
    duplicates = sorted(name for name in set(names) if names.count(name) > 1)
    expect(not duplicates, f"{label} duplicates value(s): {', '.join(duplicates)}")
    return names


def load_registry(path: Path) -> dict[str, Any]:
    registry = read_json(path)
    expect(registry.get("schema_version") == 1, f"{path} schema_version must be 1")
    expect(registry.get("scope") == SCOPE, f"{path} scope must be {SCOPE}")
    require_string(registry.get("baseline_source_commit"), f"{path}.baseline_source_commit")
    require_string(registry.get("baseline_generated_at_utc"), f"{path}.baseline_generated_at_utc")
    require_string(registry.get("baseline_command"), f"{path}.baseline_command")
    observed = require_positive_int(
        registry.get("observed_total_tests_before_gate"),
        f"{path}.observed_total_tests_before_gate",
    )
    gate_added_tests = require_string_list(registry.get("gate_added_tests"), f"{path}.gate_added_tests")
    minimum = require_positive_int(registry.get("minimum_total_tests"), f"{path}.minimum_total_tests")
    expect(
        minimum == observed + len(gate_added_tests),
        f"{path}.minimum_total_tests must equal observed_total_tests_before_gate plus gate_added_tests",
    )
    require_string(registry.get("regression_policy"), f"{path}.regression_policy")
    return registry


def run_command(command: list[str], *, cwd: Path) -> subprocess.CompletedProcess[str]:
    completed = subprocess.run(
        command,
        cwd=cwd,
        check=False,
        capture_output=True,
        text=True,
    )
    if completed.returncode != 0:
        output = "\n".join(part for part in (completed.stdout, completed.stderr) if part).strip()
        raise CTestCountError(
            f"command failed ({completed.returncode}): {' '.join(command)}"
            + (f"\n{output}" if output else "")
        )
    return completed


def ctest_show_json(build_dir: Path, ctest: str) -> dict[str, Any]:
    completed = run_command(
        [ctest, "--test-dir", str(build_dir), "--show-only=json-v1"],
        cwd=build_dir,
    )
    payload = json.loads(completed.stdout)
    expect(isinstance(payload, dict), "ctest --show-only=json-v1 did not return an object")
    expect(isinstance(payload.get("tests"), list), "ctest show JSON has no tests list")
    return payload


def configured_test_names(show_payload: dict[str, Any]) -> list[str]:
    names: list[str] = []
    for index, item in enumerate(show_payload["tests"]):
        expect(isinstance(item, dict), f"ctest show tests[{index}] must be an object")
        name = item.get("name")
        names.append(require_string(name, f"ctest show tests[{index}].name"))
    duplicates = sorted(name for name in set(names) if names.count(name) > 1)
    expect(not duplicates, "CTest configured duplicate test name(s): " + ", ".join(duplicates))
    return names


def verify_count(show_payload: dict[str, Any], registry: dict[str, Any]) -> dict[str, Any]:
    names = configured_test_names(show_payload)
    current_count = len(names)
    minimum = int(registry["minimum_total_tests"])
    if current_count < minimum:
        raise CTestCountError(
            f"configured CTest test count regressed: actual={current_count}, pinned_minimum={minimum}"
        )
    return {
        "current_count": current_count,
        "minimum_total_tests": minimum,
    }


def self_check_registry() -> dict[str, Any]:
    return {
        "schema_version": 1,
        "scope": SCOPE,
        "baseline_source_commit": "0" * 40,
        "baseline_generated_at_utc": "2026-06-13T00:00:00Z",
        "baseline_command": "ctest -N --test-dir build",
        "observed_total_tests_before_gate": 2,
        "gate_added_tests": [
            "ctest-test-count-registry",
            "ctest-test-count-registry-self-check",
        ],
        "minimum_total_tests": 4,
        "regression_policy": "Fail if configured CTest test count drops below minimum_total_tests.",
    }


def self_check_payload(names: list[str]) -> dict[str, Any]:
    return {
        "tests": [{"name": name} for name in names],
    }


def run_self_check() -> None:
    registry = self_check_registry()
    verify_count(
        self_check_payload(
            [
                "alpha",
                "beta",
                "ctest-test-count-registry",
                "ctest-test-count-registry-self-check",
            ]
        ),
        registry,
    )

    try:
        verify_count(
            self_check_payload(["alpha", "ctest-test-count-registry", "ctest-test-count-registry-self-check"]),
            registry,
        )
    except CTestCountError as exc:
        expect("configured CTest test count regressed" in str(exc), f"wrong regression error: {exc}")
    else:
        raise CTestCountError("self-check regression fixture unexpectedly passed")

    duplicate_payload = self_check_payload(["alpha", "alpha", "beta", "gamma"])
    try:
        verify_count(duplicate_payload, registry)
    except CTestCountError as exc:
        expect("duplicate test name" in str(exc), f"wrong duplicate-name error: {exc}")
    else:
        raise CTestCountError("self-check duplicate-name fixture unexpectedly passed")

    invalid_registry = dict(registry)
    invalid_registry["minimum_total_tests"] = 5
    with tempfile.TemporaryDirectory(prefix="ctest-count-registry-") as tmp:
        path = Path(tmp) / "registry.json"
        path.write_text(json.dumps(invalid_registry, indent=2) + "\n", encoding="utf-8")
        try:
            load_registry(path)
        except CTestCountError as exc:
            expect("minimum_total_tests" in str(exc), f"wrong registry validation error: {exc}")
        else:
            raise CTestCountError("self-check invalid registry unexpectedly passed")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--registry", type=Path, default=DEFAULT_REGISTRY)
    parser.add_argument("--build-dir", type=Path)
    parser.add_argument("--ctest", default="ctest")
    parser.add_argument("--verify", action="store_true")
    parser.add_argument("--self-check", action="store_true")
    args = parser.parse_args(argv)

    try:
        if args.self_check:
            run_self_check()
            print("CTest test-count verifier self-check passed")
            return 0

        root = repo_root()
        registry_path = args.registry if args.registry.is_absolute() else root / args.registry
        registry = load_registry(registry_path)
        expect(args.build_dir is not None, "--build-dir is required")
        build_dir = args.build_dir if args.build_dir.is_absolute() else root / args.build_dir
        summary = verify_count(ctest_show_json(build_dir, args.ctest), registry)
        print(
            "CTest test count verified: "
            f"current={summary['current_count']}, minimum={summary['minimum_total_tests']}"
        )
        return 0
    except (CTestCountError, OSError, json.JSONDecodeError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
