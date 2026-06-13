#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-autoIBP-TBD
"""Verify or write the pinned CTest execution-time baseline."""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
import xml.etree.ElementTree as ET
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


DEFAULT_BASELINE = Path("tools/reference-harness/specs/release/ctest-time-baseline.json")
DEFAULT_TOLERANCE_MULTIPLIER = 3.0
DEFAULT_PARALLEL = 2
DEFAULT_EXCLUDED_TESTS = (
    {
        "name": "ctest-execution-time-baseline",
        "reason": "This registry gate is enforced by the pinned timeouts and is not part of the captured baseline.",
    },
    {
        "name": "ctest-execution-time-baseline-self-check",
        "reason": "This verifier unit self-check is intentionally excluded from the release timing baseline.",
    },
)


class CTestTimeBaselineError(RuntimeError):
    """Raised when the CTest timing registry is stale or malformed."""


@dataclass(frozen=True)
class CTestTiming:
    name: str
    seconds: float
    status: str
    completion_status: str


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def utc_now() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise CTestTimeBaselineError(message)


def run_command(command: list[str], *, cwd: Path, allow_failure: bool = False) -> subprocess.CompletedProcess[str]:
    completed = subprocess.run(
        command,
        cwd=cwd,
        check=False,
        capture_output=True,
        text=True,
    )
    if not allow_failure and completed.returncode != 0:
        output = "\n".join(part for part in (completed.stdout, completed.stderr) if part).strip()
        raise CTestTimeBaselineError(
            f"command failed ({completed.returncode}): {' '.join(command)}"
            + (f"\n{output}" if output else "")
        )
    return completed


def git_head(root: Path) -> str:
    return run_command(["git", "rev-parse", "HEAD"], cwd=root).stdout.strip()


def latest_test_xml(build_dir: Path) -> Path:
    tag_file = build_dir / "Testing" / "TAG"
    if tag_file.is_file():
        tag = tag_file.read_text(encoding="utf-8").splitlines()[0].strip()
        candidate = build_dir / "Testing" / tag / "Test.xml"
        if candidate.is_file():
            return candidate

    candidates = sorted(
        (path for path in (build_dir / "Testing").glob("*/Test.xml") if path.is_file()),
        key=lambda path: path.stat().st_mtime,
    )
    expect(bool(candidates), f"no CTest Test.xml found under {build_dir / 'Testing'}")
    return candidates[-1]


def latest_test_log(build_dir: Path) -> Path:
    candidate = build_dir / "Testing" / "Temporary" / "LastTest.log"
    expect(candidate.is_file(), f"no CTest LastTest.log found at {candidate}")
    return candidate


def named_measurement(test: ET.Element, name: str) -> str | None:
    for measurement in test.findall("./Results/NamedMeasurement"):
        if measurement.attrib.get("name") == name:
            value = measurement.findtext("Value")
            return value.strip() if value is not None else None
    return None


def parse_test_xml(path: Path) -> list[CTestTiming]:
    root = ET.parse(path).getroot()
    timings: list[CTestTiming] = []
    for test in root.findall(".//Test"):
        name = test.findtext("Name")
        raw_seconds = named_measurement(test, "Execution Time")
        expect(name is not None and name.strip(), f"{path} contains a test without a name")
        expect(raw_seconds is not None, f"{name} in {path} has no Execution Time")
        seconds = float(raw_seconds)
        expect(seconds >= 0.0, f"{name} has a negative execution time")
        timings.append(
            CTestTiming(
                name=name.strip(),
                seconds=seconds,
                status=test.attrib.get("Status", "").strip() or "unknown",
                completion_status=(named_measurement(test, "Completion Status") or "").strip(),
            )
        )
    expect(timings, f"{path} did not contain any CTest timings")
    names = [timing.name for timing in timings]
    duplicates = sorted({name for name in names if names.count(name) > 1})
    expect(not duplicates, "CTest XML contains duplicate test names: " + ", ".join(duplicates))
    return timings


def parse_last_test_log(path: Path) -> list[CTestTiming]:
    text = path.read_text(encoding="utf-8", errors="replace")
    markers = list(re.finditer(r"(?m)^(?P<ordinal>[0-9]+/[0-9]+) Testing: (?P<name>.+)$", text))
    expect(markers, f"{path} did not contain CTest Testing markers")
    timings: list[CTestTiming] = []
    for index, marker in enumerate(markers):
        block_end = markers[index + 1].start() if index + 1 < len(markers) else len(text)
        block = text[marker.start() : block_end]
        name = marker.group("name").strip()
        time_match = re.search(r"(?m)^Test time =\s*(?P<seconds>[0-9]+(?:\.[0-9]+)?) sec\s*$", block)
        expect(time_match is not None, f"{name} in {path} has no Test time")
        status_match = re.search(r"(?m)^Test (?P<status>Passed|Failed|Not Run|Timeout)\.$", block)
        status = status_match.group("status").lower().replace(" ", "-") if status_match else "unknown"
        timings.append(
            CTestTiming(
                name=name,
                seconds=float(time_match.group("seconds")),
                status=status,
                completion_status=status,
            )
        )
    names = [timing.name for timing in timings]
    duplicates = sorted({name for name in names if names.count(name) > 1})
    expect(not duplicates, "CTest log contains duplicate test names: " + ", ".join(duplicates))
    return timings


def ctest_show_json(build_dir: Path, ctest: str) -> dict[str, Any]:
    completed = run_command(
        [ctest, "--test-dir", str(build_dir), "--show-only=json-v1"],
        cwd=build_dir,
    )
    payload = json.loads(completed.stdout)
    expect(isinstance(payload, dict), "ctest --show-only=json-v1 did not return an object")
    expect(isinstance(payload.get("tests"), list), "ctest show JSON has no tests list")
    return payload


def configured_test_names(show_payload: dict[str, Any]) -> set[str]:
    names: set[str] = set()
    for index, item in enumerate(show_payload["tests"]):
        expect(isinstance(item, dict), f"ctest show tests[{index}] must be an object")
        name = item.get("name")
        expect(isinstance(name, str) and name.strip(), f"ctest show tests[{index}].name must be a string")
        names.add(name.strip())
    return names


def configured_timeouts(show_payload: dict[str, Any]) -> dict[str, float]:
    timeouts: dict[str, float] = {}
    for item in show_payload["tests"]:
        name = item["name"].strip()
        for prop in item.get("properties", []):
            if prop.get("name") == "TIMEOUT":
                timeouts[name] = float(prop.get("value"))
                break
    return timeouts


def load_manifest(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as stream:
        manifest = json.load(stream)
    expect(isinstance(manifest, dict), "baseline manifest must contain a JSON object")
    validate_manifest(manifest)
    return manifest


def excluded_test_names(manifest: dict[str, Any]) -> set[str]:
    raw_exclusions = manifest.get("excluded_tests", [])
    expect(isinstance(raw_exclusions, list), "excluded_tests must be a list")
    names: set[str] = set()
    for index, item in enumerate(raw_exclusions):
        expect(isinstance(item, dict), f"excluded_tests[{index}] must be an object")
        name = item.get("name")
        reason = item.get("reason")
        expect(isinstance(name, str) and name.strip(), "excluded test names must be strings")
        expect(isinstance(reason, str) and len(reason.strip()) >= 24, "excluded tests need reasons")
        names.add(name.strip())
    return names


def validate_manifest(manifest: dict[str, Any]) -> None:
    expect(manifest.get("schema_version") == 1, "baseline schema_version must be 1")
    expect(manifest.get("tool") == "ctest-execution-time-baseline", "baseline tool mismatch")
    expect(manifest.get("scope") == "release-ctest", "baseline scope must be release-ctest")
    tolerance = manifest.get("tolerance_multiplier")
    expect(isinstance(tolerance, (int, float)) and tolerance > 1.0, "tolerance_multiplier must be > 1")
    tests = manifest.get("tests")
    expect(isinstance(tests, list) and tests, "tests must be a non-empty list")
    names: set[str] = set()
    for index, item in enumerate(tests):
        expect(isinstance(item, dict), f"tests[{index}] must be an object")
        name = item.get("name")
        baseline_seconds = item.get("baseline_seconds")
        max_seconds = item.get("max_seconds")
        expect(isinstance(name, str) and name.strip(), f"tests[{index}].name must be a string")
        expect(name not in names, f"duplicate baseline test name: {name}")
        names.add(name)
        expect(
            isinstance(baseline_seconds, (int, float)) and baseline_seconds >= 0.0,
            f"{name}.baseline_seconds must be non-negative",
        )
        expect(
            isinstance(max_seconds, (int, float)) and max_seconds >= baseline_seconds,
            f"{name}.max_seconds must be >= baseline_seconds",
        )
        expected_max = round(float(baseline_seconds) * float(tolerance), 6)
        expect(
            abs(float(max_seconds) - expected_max) <= 0.001,
            f"{name}.max_seconds must be baseline_seconds * tolerance_multiplier",
        )
    declared_count = manifest.get("test_count")
    expect(declared_count == len(tests), "test_count does not match tests length")
    excluded_test_names(manifest)


def manifest_payload(
    root: Path,
    timings: list[CTestTiming],
    parallel: int,
    *,
    baseline_commit: str | None = None,
) -> dict[str, Any]:
    tolerance = DEFAULT_TOLERANCE_MULTIPLIER
    tests: list[dict[str, Any]] = []
    capture_order = {timing.name: index for index, timing in enumerate(timings, start=1)}
    for timing in sorted(timings, key=lambda item: item.name):
        baseline_seconds = round(timing.seconds, 6)
        tests.append(
            {
                "name": timing.name,
                "baseline_seconds": baseline_seconds,
                "max_seconds": round(baseline_seconds * tolerance, 6),
                "status_at_capture": timing.status,
                "completion_status_at_capture": timing.completion_status,
                "capture_order": capture_order[timing.name],
            }
        )
    return {
        "schema_version": 1,
        "scope": "release-ctest",
        "tool": "ctest-execution-time-baseline",
        "baseline_commit": baseline_commit or git_head(root),
        "baseline_captured_at_utc": utc_now(),
        "tolerance_multiplier": tolerance,
        "ctest_parallel": parallel,
        "enforcement": "CTest TIMEOUT is set to max_seconds for each baselined test at configure time.",
        "excluded_tests": list(DEFAULT_EXCLUDED_TESTS),
        "test_count": len(tests),
        "tests": tests,
    }


def write_baseline(root: Path, build_dir: Path, baseline: Path, parallel: int) -> int:
    try:
        timing_source = latest_test_xml(build_dir)
        timings = parse_test_xml(timing_source)
    except CTestTimeBaselineError:
        timing_source = latest_test_log(build_dir)
        timings = parse_last_test_log(timing_source)
    payload = manifest_payload(root, timings, parallel)
    baseline.parent.mkdir(parents=True, exist_ok=True)
    baseline.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    print(f"wrote CTest time baseline: {len(timings)} tests from {timing_source}")
    return 0


def verify_baseline(build_dir: Path, baseline: Path, ctest: str) -> int:
    manifest = load_manifest(baseline)
    show_payload = ctest_show_json(build_dir, ctest)
    configured = configured_test_names(show_payload)
    expected = {item["name"] for item in manifest["tests"]}
    excluded = excluded_test_names(manifest)

    missing = sorted(expected - configured)
    unbaselined = sorted(configured - expected - excluded)
    problems: list[str] = []
    if missing:
        problems.append("baselined tests are not configured: " + ", ".join(missing))
    if unbaselined:
        problems.append("configured tests missing from timing baseline: " + ", ".join(unbaselined))

    timeouts = configured_timeouts(show_payload)
    expected_timeouts = {item["name"]: float(item["max_seconds"]) for item in manifest["tests"]}
    for name in sorted(expected & configured):
        actual_timeout = timeouts.get(name)
        if actual_timeout is None:
            problems.append(f"{name} has no CTest TIMEOUT property")
            continue
        expected_timeout = expected_timeouts[name]
        if abs(actual_timeout - expected_timeout) > 0.001:
            problems.append(
                f"{name} TIMEOUT {actual_timeout:.6f}s does not match baseline window {expected_timeout:.6f}s"
            )

    if problems:
        raise CTestTimeBaselineError("\n".join(problems))
    print(f"CTest time baseline verified: {len(expected)} tests have pinned 3x timeout windows")
    return 0


def run_self_check() -> None:
    xml = """<?xml version="1.0"?>
<Site><Testing>
  <Test Status="passed">
    <Name>fast-test</Name>
    <Results>
      <NamedMeasurement type="numeric/double" name="Execution Time"><Value>0.25</Value></NamedMeasurement>
      <NamedMeasurement type="text/string" name="Completion Status"><Value>Completed</Value></NamedMeasurement>
    </Results>
  </Test>
</Testing></Site>
"""
    root = ET.fromstring(xml)
    fixture = Path.cwd() / "ctest-time-baseline-self-check.xml"
    fixture.write_text(ET.tostring(root, encoding="unicode"), encoding="utf-8")
    try:
        timings = parse_test_xml(fixture)
    finally:
        fixture.unlink(missing_ok=True)
    expect(len(timings) == 1, "XML parser should find one test")
    payload = manifest_payload(Path.cwd(), timings, DEFAULT_PARALLEL, baseline_commit="0" * 40)
    validate_manifest(payload)
    payload["tests"][0]["max_seconds"] = 1.0
    try:
        validate_manifest(payload)
    except CTestTimeBaselineError:
        return
    raise CTestTimeBaselineError("manifest validation should reject a non-3x timeout")


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", type=Path, default=DEFAULT_BASELINE)
    parser.add_argument("--build-dir", type=Path)
    parser.add_argument("--ctest", default="ctest")
    parser.add_argument("--write-baseline", action="store_true")
    parser.add_argument("--parallel", type=int, default=DEFAULT_PARALLEL)
    parser.add_argument("--self-check", action="store_true")
    args = parser.parse_args(argv)

    try:
        if args.self_check:
            run_self_check()
            print("CTest time baseline verifier self-check passed")
            return 0

        root = repo_root()
        baseline = args.baseline if args.baseline.is_absolute() else root / args.baseline
        expect(args.build_dir is not None, "--build-dir is required")
        build_dir = args.build_dir if args.build_dir.is_absolute() else root / args.build_dir
        if args.write_baseline:
            return write_baseline(root, build_dir, baseline, args.parallel)
        return verify_baseline(build_dir, baseline, args.ctest)
    except (CTestTimeBaselineError, OSError, json.JSONDecodeError, ET.ParseError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
