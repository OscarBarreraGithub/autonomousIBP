#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-autoIBP-TBD
"""CTest gate for the pinned Python type-checking baseline."""

from __future__ import annotations

import argparse
import ast
import json
import re
import shutil
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


DEFAULT_BASELINE = Path("tools/reference-harness/specs/diagnostics/python-type-baseline.json")
DEFAULT_SCOPE_ROOTS = ("tools", "scripts")
MYPY_DIAGNOSTIC_RE = re.compile(r": error:")
MYPY_TOTAL_RE = re.compile(
    r"^\|\s*Total\s*\|\s*(?P<imprecision>[0-9]+(?:\.[0-9]+)?)% imprecise\s*\|\s*"
    r"(?P<lines>[0-9]+) LOC\s*\|"
)


class PythonTypeBaselineError(RuntimeError):
    """Raised when the Python type baseline regresses."""


@dataclass(frozen=True)
class AnnotationCoverage:
    annotated_functions: int
    unannotated_functions: int

    @property
    def total_functions(self) -> int:
        return self.annotated_functions + self.unannotated_functions

    @property
    def percent(self) -> float:
        if self.total_functions == 0:
            return 100.0
        return round((self.annotated_functions / self.total_functions) * 100.0, 2)

    def as_json(self) -> dict[str, Any]:
        return {
            "annotated_functions": self.annotated_functions,
            "unannotated_functions": self.unannotated_functions,
            "total_functions": self.total_functions,
            "percent": self.percent,
        }


@dataclass(frozen=True)
class MypySummary:
    executable: str
    version: str
    diagnostic_count: int
    imprecision_percent: float
    type_coverage_percent: float
    typed_lines: int
    source_args: list[str]

    def as_json(self) -> dict[str, Any]:
        return {
            "checker": "mypy",
            "checker_executable": Path(self.executable).name,
            "checker_version": self.version,
            "diagnostic_count": self.diagnostic_count,
            "type_coverage": {
                "percent": self.type_coverage_percent,
                "imprecision_percent": self.imprecision_percent,
                "lines": self.typed_lines,
                "report": "mypy --txt-report Total imprecision",
            },
            "command": [
                "mypy",
                "--no-incremental",
                "--cache-dir",
                "<temporary-cache-dir>",
                "--show-error-codes",
                "--no-error-summary",
                "--no-color-output",
                "--txt-report",
                "<temporary-report-dir>",
                *self.source_args,
            ],
        }


@dataclass(frozen=True)
class TypeMeasurement:
    tracked_python_file_count: int
    source_args: list[str]
    annotation_coverage: AnnotationCoverage
    mypy: MypySummary | None


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise PythonTypeBaselineError(message)


def utc_now() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def run_command(
    command: list[str],
    *,
    cwd: Path,
    allow_failure: bool = False,
) -> subprocess.CompletedProcess[str]:
    completed = subprocess.run(
        command,
        cwd=cwd,
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if not allow_failure and completed.returncode != 0:
        output = "\n".join(part for part in (completed.stdout, completed.stderr) if part).strip()
        raise PythonTypeBaselineError(
            f"command failed ({completed.returncode}): {' '.join(command)}"
            + (f"\n{output}" if output else "")
        )
    return completed


def run_git(root: Path, args: list[str]) -> bytes:
    completed = subprocess.run(
        ["git", "-C", str(root), *args],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if completed.returncode != 0:
        raise PythonTypeBaselineError(
            f"git {' '.join(args)} failed in {root}: "
            f"{completed.stderr.decode('utf-8', errors='replace').strip()}"
        )
    return completed.stdout


def git_head(root: Path) -> str:
    return run_git(root, ["rev-parse", "HEAD"]).decode("utf-8").strip()


def tracked_files(root: Path) -> list[str]:
    output = run_git(root, ["ls-files", "-z"])
    return sorted(item.decode("utf-8") for item in output.split(b"\0") if item)


def is_scoped_python(path_text: str) -> bool:
    path = Path(path_text)
    return path.suffix == ".py" and (
        len(path.parts) == 1 or (bool(path.parts) and path.parts[0] in DEFAULT_SCOPE_ROOTS)
    )


def scoped_python_files(root: Path) -> list[str]:
    return [path for path in tracked_files(root) if is_scoped_python(path)]


def source_args(root: Path, python_paths: list[str]) -> list[str]:
    args = [scope for scope in DEFAULT_SCOPE_ROOTS if (root / scope).is_dir()]
    top_level = sorted(path for path in python_paths if len(Path(path).parts) == 1)
    args.extend(top_level)
    expect(bool(args), "no Python sources found under tools/, scripts/, or repo-root *.py")
    return args


def first_arg_names(args: ast.arguments) -> set[str]:
    names: set[str] = set()
    positional = [*args.posonlyargs, *args.args]
    if positional:
        names.add(positional[0].arg)
    return names


def function_is_fully_annotated(node: ast.FunctionDef | ast.AsyncFunctionDef) -> bool:
    positional = [*node.args.posonlyargs, *node.args.args]
    skip_first = first_arg_names(node.args) & {"self", "cls"}
    for index, argument in enumerate(positional):
        if index == 0 and argument.arg in skip_first:
            continue
        if argument.annotation is None:
            return False
    for argument in node.args.kwonlyargs:
        if argument.annotation is None:
            return False
    if node.args.vararg is not None and node.args.vararg.annotation is None:
        return False
    if node.args.kwarg is not None and node.args.kwarg.annotation is None:
        return False
    return node.returns is not None


def annotation_coverage(root: Path, python_paths: list[str]) -> AnnotationCoverage:
    annotated = 0
    unannotated = 0
    for path_text in python_paths:
        path = root / path_text
        tree = ast.parse(path.read_text(encoding="utf-8"), filename=path_text)
        for node in ast.walk(tree):
            if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef)):
                if function_is_fully_annotated(node):
                    annotated += 1
                else:
                    unannotated += 1
    return AnnotationCoverage(annotated_functions=annotated, unannotated_functions=unannotated)


def mypy_version(executable: str, root: Path) -> str:
    completed = run_command([executable, "--version"], cwd=root, allow_failure=True)
    output = "\n".join(part for part in (completed.stdout, completed.stderr) if part).strip()
    return output.splitlines()[0].strip() if output.splitlines() else "unknown"


def parse_mypy_total(report_dir: Path) -> tuple[float, int]:
    index = report_dir / "index.txt"
    expect(index.is_file(), f"mypy did not write expected report: {index}")
    for line in index.read_text(encoding="utf-8", errors="replace").splitlines():
        match = MYPY_TOTAL_RE.match(line)
        if match is not None:
            return float(match.group("imprecision")), int(match.group("lines"))
    raise PythonTypeBaselineError("mypy report did not contain a Total imprecision row")


def run_mypy(root: Path, args: list[str]) -> MypySummary | None:
    executable = shutil.which("mypy")
    if executable is None:
        return None
    with tempfile.TemporaryDirectory(prefix="autoibp-mypy-report-") as report_tmp, tempfile.TemporaryDirectory(
        prefix="autoibp-mypy-cache-"
    ) as cache_tmp:
        command = [
            executable,
            "--no-incremental",
            "--cache-dir",
            cache_tmp,
            "--show-error-codes",
            "--no-error-summary",
            "--no-color-output",
            "--txt-report",
            report_tmp,
            *args,
        ]
        completed = run_command(command, cwd=root, allow_failure=True)
        output = "\n".join(part for part in (completed.stdout, completed.stderr) if part)
        expect(completed.returncode in {0, 1}, f"mypy failed unexpectedly with exit {completed.returncode}\n{output}")
        diagnostic_count = sum(1 for line in output.splitlines() if MYPY_DIAGNOSTIC_RE.search(line))
        imprecision, lines = parse_mypy_total(Path(report_tmp))
        return MypySummary(
            executable=executable,
            version=mypy_version(executable, root),
            diagnostic_count=diagnostic_count,
            imprecision_percent=round(imprecision, 2),
            type_coverage_percent=round(100.0 - imprecision, 2),
            typed_lines=lines,
            source_args=list(args),
        )


def collect_measurement(root: Path) -> TypeMeasurement:
    python_paths = scoped_python_files(root)
    args = source_args(root, python_paths)
    return TypeMeasurement(
        tracked_python_file_count=len(python_paths),
        source_args=args,
        annotation_coverage=annotation_coverage(root, python_paths),
        mypy=run_mypy(root, args),
    )


def source_surface(root: Path, python_paths: list[str], args: list[str]) -> dict[str, Any]:
    top_level = sorted(path for path in python_paths if len(Path(path).parts) == 1)
    return {
        "tracked_python_file_count": len(python_paths),
        "requested_surface": ["tools/", "scripts/", "repo-root *.py"],
        "source_args": args,
        "missing_requested_roots": [
            f"{scope}/" for scope in DEFAULT_SCOPE_ROOTS if not (root / scope).is_dir()
        ],
        "top_level_python_files": top_level,
    }


def baseline_manifest(root: Path, measurement: TypeMeasurement) -> dict[str, Any]:
    python_paths = scoped_python_files(root)
    if measurement.mypy is not None:
        tool = "mypy"
        diagnostic_count: int | None = measurement.mypy.diagnostic_count
        type_coverage: dict[str, Any] | None = measurement.mypy.as_json()["type_coverage"]
        checker: dict[str, Any] | None = measurement.mypy.as_json()
    else:
        tool = "annotation-fallback"
        diagnostic_count = None
        type_coverage = None
        checker = None
    return {
        "schema_version": 1,
        "scope": "python-type-hint-baseline",
        "tool": tool,
        "baseline_commit": git_head(root),
        "baseline_captured_at_utc": utc_now(),
        "source_surface": source_surface(root, python_paths, measurement.source_args),
        "diagnostic_count": diagnostic_count,
        "type_coverage": type_coverage,
        "annotation_fallback": measurement.annotation_coverage.as_json(),
        "checker": checker,
        "comparison": {
            "diagnostic_count": "actual must not exceed pinned diagnostic_count when mypy is available",
            "type_coverage": "actual percent must not fall below pinned type_coverage.percent",
            "annotation_fallback": (
                "if mypy is unavailable, annotated function percent must not fall below "
                "annotation_fallback.percent"
            ),
        },
    }


def load_manifest(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as stream:
        manifest = json.load(stream)
    expect(isinstance(manifest, dict), "baseline manifest must contain a JSON object")
    validate_manifest(manifest)
    return manifest


def validate_annotation_payload(payload: Any) -> None:
    expect(isinstance(payload, dict), "annotation_fallback must be an object")
    for key in ("annotated_functions", "unannotated_functions", "total_functions"):
        expect(isinstance(payload.get(key), int), f"annotation_fallback.{key} must be an integer")
    expect(
        payload["total_functions"] == payload["annotated_functions"] + payload["unannotated_functions"],
        "annotation_fallback total_functions disagrees with component counts",
    )
    expect(isinstance(payload.get("percent"), (int, float)), "annotation_fallback.percent must be numeric")


def validate_manifest(manifest: dict[str, Any]) -> None:
    expect(manifest.get("schema_version") == 1, "baseline schema_version must be 1")
    expect(manifest.get("scope") == "python-type-hint-baseline", "baseline scope mismatch")
    expect(manifest.get("tool") in {"mypy", "annotation-fallback"}, "baseline tool mismatch")
    validate_annotation_payload(manifest.get("annotation_fallback"))
    if manifest.get("tool") == "mypy":
        expect(isinstance(manifest.get("diagnostic_count"), int), "diagnostic_count must be an integer")
        type_coverage = manifest.get("type_coverage")
        if not isinstance(type_coverage, dict):
            raise PythonTypeBaselineError("type_coverage must be an object")
        expect(isinstance(type_coverage.get("percent"), (int, float)), "type_coverage.percent must be numeric")
        expect(
            isinstance(type_coverage.get("imprecision_percent"), (int, float)),
            "type_coverage.imprecision_percent must be numeric",
        )
        expect(isinstance(type_coverage.get("lines"), int), "type_coverage.lines must be an integer")


def verify_against_manifest(manifest: dict[str, Any], measurement: TypeMeasurement) -> list[str]:
    problems: list[str] = []
    if manifest.get("tool") == "mypy" and measurement.mypy is not None:
        expected_count = int(manifest["diagnostic_count"])
        if measurement.mypy.diagnostic_count > expected_count:
            problems.append(
                f"mypy diagnostic regression: {measurement.mypy.diagnostic_count} exceeds baseline "
                f"{expected_count}"
            )
        expected_coverage = float(manifest["type_coverage"]["percent"])
        if measurement.mypy.type_coverage_percent + 1e-9 < expected_coverage:
            problems.append(
                f"mypy type coverage regression: {measurement.mypy.type_coverage_percent:.2f}% "
                f"is below baseline {expected_coverage:.2f}%"
            )
        return problems

    expected_annotation = float(manifest["annotation_fallback"]["percent"])
    actual_annotation = measurement.annotation_coverage.percent
    if actual_annotation + 1e-9 < expected_annotation:
        problems.append(
            f"annotation fallback regression: {actual_annotation:.2f}% annotated functions "
            f"is below baseline {expected_annotation:.2f}%"
        )
    return problems


def write_baseline(root: Path, baseline: Path, measurement: TypeMeasurement) -> int:
    payload = baseline_manifest(root, measurement)
    baseline.parent.mkdir(parents=True, exist_ok=True)
    baseline.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    if measurement.mypy is not None:
        print(
            "wrote Python type baseline: "
            f"{measurement.mypy.diagnostic_count} mypy diagnostics, "
            f"{measurement.mypy.type_coverage_percent:.2f}% type coverage"
        )
    else:
        print(
            "wrote Python type annotation fallback baseline: "
            f"{measurement.annotation_coverage.percent:.2f}% annotated functions"
        )
    return 0


def verify_baseline(root: Path, baseline: Path, measurement: TypeMeasurement) -> int:
    manifest = load_manifest(baseline)
    problems = verify_against_manifest(manifest, measurement)
    if problems:
        raise PythonTypeBaselineError("\n".join(problems))
    if measurement.mypy is not None and manifest.get("tool") == "mypy":
        print(
            "Python type baseline verified: "
            f"{measurement.mypy.diagnostic_count} mypy diagnostics "
            f"(baseline {int(manifest['diagnostic_count'])}), "
            f"{measurement.mypy.type_coverage_percent:.2f}% type coverage "
            f"(baseline {float(manifest['type_coverage']['percent']):.2f}%)"
        )
    else:
        print(
            "Python type annotation fallback verified: "
            f"{measurement.annotation_coverage.percent:.2f}% annotated functions "
            f"(baseline {float(manifest['annotation_fallback']['percent']):.2f}%)"
        )
    return 0


def run_self_check() -> None:
    output = "\n".join(
        [
            "tools/a.py:1: error: first [misc]",
            "tools/a.py:1: note: extra context",
            "tools/b.py:2: error: second [arg-type]",
        ]
    )
    expect(
        sum(1 for line in output.splitlines() if MYPY_DIAGNOSTIC_RE.search(line)) == 2,
        "mypy diagnostic counter should count only error lines",
    )
    with tempfile.TemporaryDirectory(prefix="autoibp-mypy-report-fixture-") as tmp:
        report = Path(tmp)
        (report / "index.txt").write_text(
            "| Total                                          |  22.71% imprecise | 58696 LOC |\n",
            encoding="utf-8",
        )
        imprecision, lines = parse_mypy_total(report)
        expect(imprecision == 22.71 and lines == 58696, "mypy total parser should read report totals")

    tree = ast.parse(
        "\n".join(
            [
                "def typed(value: int) -> int:",
                "    return value",
                "def untyped(value):",
                "    return value",
            ]
        )
    )
    functions = [node for node in ast.walk(tree) if isinstance(node, ast.FunctionDef)]
    expect(function_is_fully_annotated(functions[0]), "typed fixture should be annotated")
    expect(not function_is_fully_annotated(functions[1]), "untyped fixture should be unannotated")

    measurement = TypeMeasurement(
        tracked_python_file_count=1,
        source_args=["tools"],
        annotation_coverage=AnnotationCoverage(annotated_functions=1, unannotated_functions=1),
        mypy=MypySummary(
            executable="/usr/bin/mypy",
            version="mypy fixture",
            diagnostic_count=2,
            imprecision_percent=22.71,
            type_coverage_percent=77.29,
            typed_lines=10,
            source_args=["tools"],
        ),
    )
    manifest = baseline_manifest(root=Path.cwd(), measurement=measurement)
    validate_manifest(manifest)
    expect(not verify_against_manifest(manifest, measurement), "matching mypy baseline should pass")
    regressed = TypeMeasurement(
        tracked_python_file_count=1,
        source_args=["tools"],
        annotation_coverage=measurement.annotation_coverage,
        mypy=MypySummary(
            executable="/usr/bin/mypy",
            version="mypy fixture",
            diagnostic_count=3,
            imprecision_percent=23.0,
            type_coverage_percent=77.0,
            typed_lines=10,
            source_args=["tools"],
        ),
    )
    expect(bool(verify_against_manifest(manifest, regressed)), "mypy regression should fail")


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", type=Path, default=DEFAULT_BASELINE)
    parser.add_argument("--write-baseline", action="store_true")
    parser.add_argument("--self-check", action="store_true")
    args = parser.parse_args(argv)

    try:
        if args.self_check:
            run_self_check()
            print("Python type baseline verifier self-check passed")
            return 0

        root = repo_root()
        baseline = args.baseline if args.baseline.is_absolute() else root / args.baseline
        measurement = collect_measurement(root)
        if args.write_baseline:
            return write_baseline(root, baseline, measurement)
        return verify_baseline(root, baseline, measurement)
    except (PythonTypeBaselineError, OSError, SyntaxError, json.JSONDecodeError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
