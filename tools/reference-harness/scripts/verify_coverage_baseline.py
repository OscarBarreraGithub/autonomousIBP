#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-autoIBP-TBD
"""CTest gate for the pinned amflow coverage baseline."""

from __future__ import annotations

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


DEFAULT_BASELINE = Path("tools/reference-harness/specs/coverage/amflow-coverage-baseline.json")
DEFAULT_SCOPE_PREFIXES = ("src/",)
DEFAULT_CXX_FLAGS = "--coverage"
DEFAULT_LINKER_FLAGS = "--coverage"
DEFAULT_PARALLEL = 2
DEFAULT_TOLERANCE_PERCENTAGE_POINTS = 1.0
DEFAULT_NESTED_CTEST_EXCLUSIONS = (
    {
        "name": "amflow-public-symbol-surface",
        "reason": (
            "Coverage instrumentation changes the exported weak symbol surface; "
            "the normal non-coverage CTest gate remains authoritative for ABI drift."
        ),
    },
    {
        "name": "source-license-header-gate",
        "reason": (
            "This metadata-only gate does not affect C++ coverage and is owned by "
            "the normal non-coverage CTest suite."
        ),
    },
)
GCOV_SOURCE_LINE_RE = re.compile(r"^\s*(?P<count>[-#=0-9]+):\s*(?P<line>[0-9]+):")
GCOV_FUNCTION_LINE_RE = re.compile(r"^function .* called (?P<count>[0-9]+)")


class CoverageBaselineError(RuntimeError):
    """Raised when coverage falls below the pinned baseline."""


@dataclass(frozen=True)
class CoverageMetric:
    covered: int
    total: int

    @property
    def percent(self) -> float:
        if self.total == 0:
            return 100.0
        return round((self.covered / self.total) * 100.0, 2)

    def as_json(self) -> dict[str, Any]:
        return {
            "covered": self.covered,
            "total": self.total,
            "percent": self.percent,
        }


@dataclass(frozen=True)
class CoverageSummary:
    line: CoverageMetric
    function: CoverageMetric
    files: list[dict[str, Any]]
    instrumentation: dict[str, int]
    coverage_engine: str
    fallback_reason: str | None = None


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise CoverageBaselineError(message)


def run_command(
    command: list[str],
    *,
    cwd: Path,
    env: dict[str, str] | None = None,
    allow_failure: bool = False,
) -> subprocess.CompletedProcess[str]:
    completed = subprocess.run(
        command,
        cwd=cwd,
        env=env,
        check=False,
        capture_output=True,
        text=True,
    )
    if not allow_failure and completed.returncode != 0:
        output = "\n".join(part for part in (completed.stdout, completed.stderr) if part).strip()
        raise CoverageBaselineError(
            f"command failed ({completed.returncode}): {' '.join(command)}"
            + (f"\n{output}" if output else "")
        )
    return completed


def git_head(root: Path) -> str:
    return run_command(["git", "rev-parse", "HEAD"], cwd=root).stdout.strip()


def compiler_version(root: Path) -> str:
    completed = run_command(["c++", "--version"], cwd=root, allow_failure=True)
    first_line = completed.stdout.splitlines()[0] if completed.stdout.splitlines() else ""
    return first_line.strip()


def utc_now() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def load_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as stream:
        payload = json.load(stream)
    expect(isinstance(payload, dict), f"{path} must contain a JSON object")
    return payload


def normalize_scope_prefixes(raw: Any) -> list[str]:
    expect(isinstance(raw, list) and raw, "source_scope_prefixes must be a non-empty list")
    prefixes: list[str] = []
    for item in raw:
        expect(isinstance(item, str) and item.strip(), "source scope entries must be strings")
        prefix = item.strip().replace("\\", "/")
        prefixes.append(prefix if prefix.endswith("/") else prefix + "/")
    return prefixes


def source_in_scope(path: str, prefixes: list[str]) -> bool:
    return any(path.startswith(prefix) for prefix in prefixes)


def coverage_flags_from_manifest(manifest: dict[str, Any]) -> tuple[str, str, str]:
    raw = manifest.get("coverage_flags", {})
    expect(isinstance(raw, dict), "coverage_flags must be an object")
    cxx_flags = raw.get("cxx", DEFAULT_CXX_FLAGS)
    exe_linker_flags = raw.get("exe_linker", DEFAULT_LINKER_FLAGS)
    shared_linker_flags = raw.get("shared_linker", DEFAULT_LINKER_FLAGS)
    expect(isinstance(cxx_flags, str), "coverage_flags.cxx must be a string")
    expect(isinstance(exe_linker_flags, str), "coverage_flags.exe_linker must be a string")
    expect(isinstance(shared_linker_flags, str), "coverage_flags.shared_linker must be a string")
    return cxx_flags, exe_linker_flags, shared_linker_flags


def nested_ctest_excluded_tests(manifest: dict[str, Any]) -> list[str]:
    raw = manifest.get("nested_ctest_excluded_tests", list(DEFAULT_NESTED_CTEST_EXCLUSIONS))
    expect(isinstance(raw, list), "nested_ctest_excluded_tests must be a list")
    names: list[str] = []
    for index, item in enumerate(raw):
        expect(isinstance(item, dict), f"nested_ctest_excluded_tests[{index}] must be an object")
        name = item.get("name")
        reason = item.get("reason")
        expect(isinstance(name, str) and name.strip(), "nested CTest exclusion names must be strings")
        expect(isinstance(reason, str) and len(reason.strip()) >= 24, "nested CTest exclusions need reasons")
        names.append(name.strip())
    return names


def configure_build_and_test(root: Path, build_dir: Path, manifest: dict[str, Any], parallel: int) -> None:
    if build_dir.exists():
        shutil.rmtree(build_dir)

    cxx_flags, exe_linker_flags, shared_linker_flags = coverage_flags_from_manifest(manifest)
    configure = ["cmake", "-S", str(root), "-B", str(build_dir)]
    if shutil.which("ninja"):
        configure.extend(["-G", "Ninja"])
    configure.extend(
        [
            "-DCMAKE_BUILD_TYPE=Debug",
            "-DBUILD_TESTING=ON",
            "-DAMFLOW_ENABLE_COVERAGE_BASELINE_GATE=OFF",
            "-DAMFLOW_ENABLE_CTEST_TIME_BASELINE_GATE=OFF",
            "-DAMFLOW_ENABLE_CTEST_TEST_COUNT_GATE=OFF",
            "-DAMFLOW_WITH_GINAC=OFF",
            "-DAMFLOW_WITH_MPFR=OFF",
            "-DAMFLOW_WITH_YAML_CPP=OFF",
            f"-DCMAKE_CXX_FLAGS={cxx_flags}",
            f"-DCMAKE_EXE_LINKER_FLAGS={exe_linker_flags}",
            f"-DCMAKE_SHARED_LINKER_FLAGS={shared_linker_flags}",
        ]
    )
    run_command(configure, cwd=root)
    run_command(["cmake", "--build", str(build_dir), "--parallel", str(parallel)], cwd=root)

    amflow_tests = build_dir / "amflow-tests"
    expect(amflow_tests.is_file(), f"missing built test executable: {amflow_tests}")
    run_command([str(amflow_tests)], cwd=root)

    env = dict(os.environ)
    env["AMFLOW_COVERAGE_BASELINE_NESTED"] = "1"
    ctest_command = [
        "ctest",
        "--test-dir",
        str(build_dir),
        "--output-on-failure",
        "--parallel",
        str(parallel),
    ]
    excluded_tests = nested_ctest_excluded_tests(manifest)
    if excluded_tests:
        ctest_command.extend(["--exclude-regex", "^(" + "|".join(re.escape(name) for name in excluded_tests) + ")$"])
    run_command(ctest_command, cwd=root, env=env)


def source_rel_from_gcno(root: Path, build_dir: Path, gcno: Path, prefixes: list[str]) -> str | None:
    try:
        rel_parts = gcno.relative_to(build_dir).parts
    except ValueError:
        return None
    if not rel_parts or not rel_parts[0] == "CMakeFiles":
        return None
    try:
        dir_index = next(index for index, part in enumerate(rel_parts) if part.endswith(".dir"))
    except StopIteration:
        return None
    source_rel = Path(*rel_parts[dir_index + 1 :]).with_suffix("").as_posix()
    if not source_rel.endswith(".cpp"):
        return None
    if not source_in_scope(source_rel, prefixes):
        return None
    if not (root / source_rel).is_file():
        return None
    return source_rel


def scoped_gcno_files(root: Path, build_dir: Path, prefixes: list[str]) -> dict[Path, str]:
    entries: dict[Path, str] = {}
    for gcno in sorted(build_dir.rglob("*.gcno")):
        source_rel = source_rel_from_gcno(root, build_dir, gcno, prefixes)
        if source_rel is not None:
            entries[gcno] = source_rel
    return entries


def parse_gcov_file(path: Path) -> tuple[CoverageMetric, CoverageMetric]:
    expect(path.is_file(), f"gcov did not create expected report {path.name}")
    covered_lines = 0
    total_lines = 0
    covered_functions = 0
    total_functions = 0
    for raw_line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        line_match = GCOV_SOURCE_LINE_RE.match(raw_line)
        if line_match:
            line_number = int(line_match.group("line"))
            count = line_match.group("count").strip()
            if line_number == 0 or count == "-":
                continue
            total_lines += 1
            if count.isdigit() and int(count) > 0:
                covered_lines += 1
            continue

        function_match = GCOV_FUNCTION_LINE_RE.match(raw_line)
        if function_match:
            total_functions += 1
            if int(function_match.group("count")) > 0:
                covered_functions += 1

    return CoverageMetric(covered_lines, total_lines), CoverageMetric(covered_functions, total_functions)


def collect_with_gcov(root: Path, build_dir: Path, prefixes: list[str]) -> CoverageSummary:
    gcov = shutil.which("gcov")
    gcno_entries = scoped_gcno_files(root, build_dir, prefixes)
    touched_gcda_count = sum(1 for gcno in gcno_entries if gcno.with_suffix(".gcda").is_file())
    instrumentation = {
        "source_gcno_count": len(gcno_entries),
        "touched_source_gcda_count": touched_gcda_count,
    }
    if gcov is None:
        return CoverageSummary(
            line=CoverageMetric(touched_gcda_count, max(len(gcno_entries), 1)),
            function=CoverageMetric(touched_gcda_count, max(len(gcno_entries), 1)),
            files=[],
            instrumentation=instrumentation,
            coverage_engine="instrumentation-count",
            fallback_reason="gcov was not available; using touched source .gcda count over scoped .gcno files",
        )

    expect(gcno_entries, "no scoped .gcno files found; coverage instrumentation did not build")
    files: list[dict[str, Any]] = []
    total_lines = CoverageMetric(0, 0)
    total_functions = CoverageMetric(0, 0)

    with tempfile.TemporaryDirectory(prefix="amflow-gcov-") as tmp:
        gcov_cwd = Path(tmp)
        for gcno, source_rel in sorted(gcno_entries.items(), key=lambda item: item[1]):
            completed = run_command(
                [gcov, "-b", "-c", "-f", str(gcno)],
                cwd=gcov_cwd,
                allow_failure=True,
            )
            output = "\n".join(part for part in (completed.stdout, completed.stderr) if part)
            if completed.returncode != 0:
                raise CoverageBaselineError(
                    f"gcov failed for {source_rel} ({completed.returncode})\n{output.strip()}"
                )
            line_metric, function_metric = parse_gcov_file(gcov_cwd / f"{Path(source_rel).name}.gcov")
            if line_metric.total == 0:
                continue
            total_lines = CoverageMetric(
                total_lines.covered + line_metric.covered,
                total_lines.total + line_metric.total,
            )
            total_functions = CoverageMetric(
                total_functions.covered + function_metric.covered,
                total_functions.total + function_metric.total,
            )
            files.append(
                {
                    "path": source_rel,
                    "line_coverage": line_metric.as_json(),
                    "function_coverage": function_metric.as_json(),
                }
            )

    expect(total_lines.total > 0, "gcov did not report executable source lines")
    expect(total_functions.total > 0, "gcov did not report source functions")
    return CoverageSummary(
        line=total_lines,
        function=total_functions,
        files=files,
        instrumentation=instrumentation,
        coverage_engine="gcov",
    )


def baseline_manifest(
    *,
    root: Path,
    summary: CoverageSummary,
    prefixes: list[str],
    tolerance: float,
    baseline_commit: str | None = None,
) -> dict[str, Any]:
    return {
        "schema_version": 1,
        "scope": "amflow-coverage-baseline",
        "tool": "coverage-baseline",
        "baseline_commit": baseline_commit or git_head(root),
        "baseline_captured_at_utc": utc_now(),
        "baseline_compiler": compiler_version(root),
        "coverage_engine": summary.coverage_engine,
        "fallback_reason": summary.fallback_reason,
        "source_scope_prefixes": prefixes,
        "coverage_flags": {
            "cxx": DEFAULT_CXX_FLAGS,
            "exe_linker": DEFAULT_LINKER_FLAGS,
            "shared_linker": DEFAULT_LINKER_FLAGS,
        },
        "test_sequence": [
            "amflow-tests",
            "ctest --output-on-failure with nested_ctest_excluded_tests",
        ],
        "nested_ctest_excluded_tests": list(DEFAULT_NESTED_CTEST_EXCLUSIONS),
        "tolerance_percentage_points": tolerance,
        "line_coverage": summary.line.as_json(),
        "function_coverage": summary.function.as_json(),
        "instrumentation": summary.instrumentation,
        "file_count": len(summary.files),
        "files": summary.files,
    }


def validate_manifest(manifest: dict[str, Any]) -> None:
    expect(manifest.get("schema_version") == 1, "baseline schema_version must be 1")
    expect(manifest.get("scope") == "amflow-coverage-baseline", "baseline scope mismatch")
    expect(manifest.get("tool") == "coverage-baseline", "baseline tool mismatch")
    for key in ("line_coverage", "function_coverage", "instrumentation"):
        expect(isinstance(manifest.get(key), dict), f"{key} must be an object")
    tolerance = manifest.get("tolerance_percentage_points")
    expect(
        isinstance(tolerance, (int, float)) and tolerance >= 0.0,
        "tolerance_percentage_points must be a non-negative number",
    )
    for key in ("line_coverage", "function_coverage"):
        metric = manifest[key]
        expect(isinstance(metric.get("covered"), int), f"{key}.covered must be an integer")
        expect(isinstance(metric.get("total"), int) and metric["total"] > 0, f"{key}.total must be positive")
        expect(isinstance(metric.get("percent"), (int, float)), f"{key}.percent must be numeric")
    nested_ctest_excluded_tests(manifest)


def compare_percent(label: str, baseline: dict[str, Any], actual: CoverageMetric, tolerance: float) -> str | None:
    expected_percent = float(baseline[label]["percent"])
    floor = expected_percent - tolerance
    if actual.percent + 1e-9 < floor:
        return (
            f"{label.replace('_', ' ')} regression: {actual.percent:.2f}% "
            f"is below baseline {expected_percent:.2f}% minus {tolerance:.2f}pp tolerance"
        )
    return None


def verify_against_manifest(manifest: dict[str, Any], summary: CoverageSummary) -> list[str]:
    validate_manifest(manifest)
    tolerance = float(manifest["tolerance_percentage_points"])
    if summary.coverage_engine != "gcov":
        expected = manifest["instrumentation"]
        actual = summary.instrumentation
        problems: list[str] = []
        for key in ("source_gcno_count", "touched_source_gcda_count"):
            expected_count = int(expected.get(key, 0))
            actual_count = int(actual.get(key, 0))
            if actual_count < expected_count:
                problems.append(
                    f"{key} regression: {actual_count} is below baseline {expected_count}"
                )
        return problems

    problems = [
        item
        for item in (
            compare_percent("line_coverage", manifest, summary.line, tolerance),
            compare_percent("function_coverage", manifest, summary.function, tolerance),
        )
        if item is not None
    ]
    return problems


def write_report(path: Path, summary: CoverageSummary) -> None:
    payload = {
        "coverage_engine": summary.coverage_engine,
        "fallback_reason": summary.fallback_reason,
        "line_coverage": summary.line.as_json(),
        "function_coverage": summary.function.as_json(),
        "instrumentation": summary.instrumentation,
        "file_count": len(summary.files),
        "files": summary.files,
    }
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")


def write_baseline(root: Path, baseline: Path, summary: CoverageSummary, prefixes: list[str], tolerance: float) -> int:
    payload = baseline_manifest(root=root, summary=summary, prefixes=prefixes, tolerance=tolerance)
    baseline.parent.mkdir(parents=True, exist_ok=True)
    baseline.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    print(
        "wrote coverage baseline: "
        f"lines {summary.line.percent:.2f}%, functions {summary.function.percent:.2f}% "
        f"using {summary.coverage_engine}"
    )
    return 0


def run_self_check() -> None:
    summary = CoverageSummary(
        line=CoverageMetric(90, 100),
        function=CoverageMetric(45, 50),
        files=[],
        instrumentation={"source_gcno_count": 3, "touched_source_gcda_count": 3},
        coverage_engine="gcov",
    )
    manifest = baseline_manifest(
        root=Path.cwd(),
        summary=summary,
        prefixes=["src/"],
        tolerance=1.0,
        baseline_commit="0" * 40,
    )
    validate_manifest(manifest)
    expect(not verify_against_manifest(manifest, summary), "matching coverage should pass")
    regressed = CoverageSummary(
        line=CoverageMetric(88, 100),
        function=CoverageMetric(45, 50),
        files=[],
        instrumentation={"source_gcno_count": 3, "touched_source_gcda_count": 3},
        coverage_engine="gcov",
    )
    expect(
        verify_against_manifest(manifest, regressed),
        "coverage below tolerance should fail",
    )
    fallback = CoverageSummary(
        line=CoverageMetric(3, 3),
        function=CoverageMetric(3, 3),
        files=[],
        instrumentation={"source_gcno_count": 3, "touched_source_gcda_count": 3},
        coverage_engine="instrumentation-count",
        fallback_reason="fixture",
    )
    expect(not verify_against_manifest(manifest, fallback), "matching fallback counts should pass")


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", type=Path, default=DEFAULT_BASELINE)
    parser.add_argument("--build-dir", type=Path)
    parser.add_argument("--write-baseline", action="store_true")
    parser.add_argument("--collect-existing", action="store_true")
    parser.add_argument("--parallel", type=int, default=DEFAULT_PARALLEL)
    parser.add_argument("--report-output", type=Path)
    parser.add_argument("--self-check", action="store_true")
    args = parser.parse_args(argv)

    try:
        if args.self_check:
            run_self_check()
            print("coverage baseline verifier self-check passed")
            return 0

        root = repo_root()
        baseline = args.baseline if args.baseline.is_absolute() else root / args.baseline
        expect(args.build_dir is not None, "--build-dir is required")
        build_dir = args.build_dir if args.build_dir.is_absolute() else root / args.build_dir
        report_output = (
            args.report_output
            if args.report_output is not None and args.report_output.is_absolute()
            else build_dir / "amflow-coverage-summary.json"
            if args.report_output is None
            else root / args.report_output
        )

        if args.write_baseline and not baseline.exists():
            manifest: dict[str, Any] = {
                "source_scope_prefixes": list(DEFAULT_SCOPE_PREFIXES),
                "coverage_flags": {
                    "cxx": DEFAULT_CXX_FLAGS,
                    "exe_linker": DEFAULT_LINKER_FLAGS,
                    "shared_linker": DEFAULT_LINKER_FLAGS,
                },
            }
            prefixes = list(DEFAULT_SCOPE_PREFIXES)
            tolerance = DEFAULT_TOLERANCE_PERCENTAGE_POINTS
        else:
            manifest = load_json(baseline)
            validate_manifest(manifest)
            prefixes = normalize_scope_prefixes(manifest.get("source_scope_prefixes"))
            tolerance = float(manifest["tolerance_percentage_points"])

        if not args.collect_existing:
            configure_build_and_test(root, build_dir, manifest, args.parallel)

        summary = collect_with_gcov(root, build_dir, prefixes)
        write_report(report_output, summary)

        if args.write_baseline:
            return write_baseline(root, baseline, summary, prefixes, tolerance)

        problems = verify_against_manifest(manifest, summary)
        if problems:
            raise CoverageBaselineError("\n".join(problems))
        if summary.coverage_engine == "gcov":
            print(
                "coverage baseline verified: "
                f"lines {summary.line.percent:.2f}% "
                f"(baseline {float(manifest['line_coverage']['percent']):.2f}%), "
                f"functions {summary.function.percent:.2f}% "
                f"(baseline {float(manifest['function_coverage']['percent']):.2f}%)"
            )
        else:
            print(
                "coverage instrumentation fallback verified: "
                f"{summary.instrumentation['touched_source_gcda_count']} touched source .gcda files"
            )
        return 0
    except (CoverageBaselineError, OSError, json.JSONDecodeError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
