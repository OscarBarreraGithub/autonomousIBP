#!/usr/bin/env python3
"""CTest gate for the pinned compile-warning diagnostic baseline."""

from __future__ import annotations

import argparse
import json
import re
import shutil
import subprocess
import sys
from collections import Counter, defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import Any


DEFAULT_BASELINE = Path("tools/reference-harness/specs/diagnostics/compile-warning-src-baseline.json")
DEFAULT_SCOPE = "src"
DEFAULT_WERROR_DEMOTIONS = ("unused-function",)
WARNING_RE = re.compile(
    r"^(?P<path>.*?):(?P<line>[0-9]+):(?P<column>[0-9]+): warning: "
    r"(?P<message>.*?)(?: \[(?P<option>-[^\]]+)\])?$"
)


class CompileWarningBaselineError(RuntimeError):
    """Raised when compile warnings exceed the pinned baseline."""


@dataclass(frozen=True)
class WarningDiagnostic:
    path: str
    option: str
    message: str
    line: int
    column: int

    def signature(self) -> tuple[str, str, str]:
        return (self.path, self.option, self.message)

    def location(self) -> dict[str, int]:
        return {"line": self.line, "column": self.column}


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise CompileWarningBaselineError(message)


def relative_path(root: Path, path_text: str) -> str:
    path = Path(path_text)
    if not path.is_absolute():
        path = root / path
    try:
        return path.resolve().relative_to(root.resolve()).as_posix()
    except ValueError:
        return path.as_posix()


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
        raise CompileWarningBaselineError(
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


def warning_flags(demotions: list[str]) -> str:
    flags = ["-Werror"]
    flags.extend(f"-Wno-error={warning}" for warning in demotions)
    return " ".join(flags)


def configure_and_build(root: Path, build_dir: Path, demotions: list[str]) -> str:
    if build_dir.exists():
        shutil.rmtree(build_dir)
    flags = warning_flags(demotions)
    configure = [
        "cmake",
        "-S",
        str(root),
        "-B",
        str(build_dir),
        "-DBUILD_TESTING=OFF",
        "-DAMFLOW_WITH_GINAC=OFF",
        "-DAMFLOW_WITH_MPFR=OFF",
        "-DAMFLOW_WITH_YAML_CPP=OFF",
        f"-DCMAKE_CXX_FLAGS={flags}",
    ]
    run_command(configure, cwd=root)
    build = run_command(
        ["cmake", "--build", str(build_dir), "--parallel", "2"],
        cwd=root,
        allow_failure=True,
    )
    output = "\n".join(part for part in (build.stdout, build.stderr) if part)
    if build.returncode != 0:
        raise CompileWarningBaselineError(
            "compile-warning baseline build failed; this usually means a non-baselined "
            "-Werror diagnostic appeared\n"
            + output.strip()
        )
    return output


def parse_warnings(root: Path, output: str, scope: str) -> list[WarningDiagnostic]:
    prefix = scope.rstrip("/") + "/"
    warnings: list[WarningDiagnostic] = []
    for raw_line in output.splitlines():
        match = WARNING_RE.match(raw_line.strip())
        if match is None:
            continue
        path = relative_path(root, match.group("path"))
        if not path.startswith(prefix):
            continue
        warnings.append(
            WarningDiagnostic(
                path=path,
                option=(match.group("option") or "").strip(),
                message=match.group("message").strip(),
                line=int(match.group("line")),
                column=int(match.group("column")),
            )
        )
    return sorted(
        warnings,
        key=lambda item: (item.path, item.option, item.message, item.line, item.column),
    )


def grouped_warnings(warnings: list[WarningDiagnostic]) -> list[dict[str, Any]]:
    groups: dict[tuple[str, str, str], list[WarningDiagnostic]] = defaultdict(list)
    for warning in warnings:
        groups[warning.signature()].append(warning)

    entries: list[dict[str, Any]] = []
    for signature in sorted(groups):
        path, option, message = signature
        occurrences = sorted(
            (warning.location() for warning in groups[signature]),
            key=lambda item: (item["line"], item["column"]),
        )
        entries.append(
            {
                "path": path,
                "warning_option": option,
                "message": message,
                "occurrences": occurrences,
            }
        )
    return entries


def manifest_payload(
    *,
    baseline_commit: str,
    compiler: str,
    demotions: list[str],
    scope: str,
    warnings: list[WarningDiagnostic],
) -> dict[str, Any]:
    return {
        "schema_version": 1,
        "scope": scope,
        "tool": "compile-warning-baseline",
        "fallback_reason": (
            "clang-tidy 21.1.8 was present but could not complete the src/* scan on "
            "this cluster toolchain because clang rejected the system Boost "
            "multiprecision headers before analysis completed."
        ),
        "baseline_commit": baseline_commit,
        "baseline_compiler": compiler,
        "warning_surface": [
            "repo CMake target warnings: -Wall -Wextra -Wpedantic",
            "fallback gate flag: -Werror",
        ],
        "werror_demotions_for_baseline_comparison": demotions,
        "status": "clean" if not warnings else "diagnostics",
        "diagnostic_count": len(warnings),
        "comparison_key": ["path", "warning_option", "message", "occurrence_count"],
        "notes": (
            "The CTest gate rebuilds the repo-local src/* targets with BUILD_TESTING=OFF "
            "and optional dependency backends disabled. Demoted warning classes are still "
            "parsed and compared against this baseline; removed warnings are allowed."
        ),
        "diagnostics": grouped_warnings(warnings),
    }


def warnings_from_manifest(manifest: dict[str, Any]) -> Counter[tuple[str, str, str]]:
    raw_diagnostics = manifest.get("diagnostics")
    expect(isinstance(raw_diagnostics, list), "manifest diagnostics must be a list")
    counter: Counter[tuple[str, str, str]] = Counter()
    for index, item in enumerate(raw_diagnostics):
        expect(isinstance(item, dict), f"diagnostics[{index}] must be an object")
        path = item.get("path")
        option = item.get("warning_option")
        message = item.get("message")
        occurrences = item.get("occurrences")
        expect(isinstance(path, str) and path.strip(), f"diagnostics[{index}].path must be a string")
        expect(isinstance(option, str), f"diagnostics[{index}].warning_option must be a string")
        expect(isinstance(message, str) and message.strip(), f"diagnostics[{index}].message must be a string")
        expect(isinstance(occurrences, list), f"diagnostics[{index}].occurrences must be a list")
        expect(occurrences, f"diagnostics[{index}].occurrences must not be empty")
        for occurrence_index, occurrence in enumerate(occurrences):
            expect(
                isinstance(occurrence, dict),
                f"diagnostics[{index}].occurrences[{occurrence_index}] must be an object",
            )
            expect(isinstance(occurrence.get("line"), int), "warning occurrence line must be an integer")
            expect(isinstance(occurrence.get("column"), int), "warning occurrence column must be an integer")
        counter[(path.strip(), option.strip(), message.strip())] += len(occurrences)
    return counter


def load_manifest(path: Path, scope: str) -> tuple[Counter[tuple[str, str, str]], list[str]]:
    with path.open("r", encoding="utf-8") as stream:
        manifest = json.load(stream)
    expect(isinstance(manifest, dict), "baseline manifest must contain a JSON object")
    expect(manifest.get("schema_version") == 1, "baseline manifest schema_version must be 1")
    expect(manifest.get("tool") == "compile-warning-baseline", "baseline manifest tool mismatch")
    expect(manifest.get("scope") == scope, f"baseline manifest scope must be {scope}")
    demotions = manifest.get("werror_demotions_for_baseline_comparison")
    expect(isinstance(demotions, list), "baseline manifest demotions must be a list")
    normalized_demotions: list[str] = []
    for item in demotions:
        expect(isinstance(item, str) and item.strip(), "baseline demotion entries must be strings")
        normalized_demotions.append(item.strip())
    expected = warnings_from_manifest(manifest)
    diagnostic_count = manifest.get("diagnostic_count")
    expect(
        isinstance(diagnostic_count, int) and diagnostic_count == sum(expected.values()),
        "baseline manifest diagnostic_count does not match diagnostics",
    )
    status = manifest.get("status")
    expect(status in {"clean", "diagnostics"}, "baseline manifest status must be clean or diagnostics")
    expect((status == "clean") == (diagnostic_count == 0), "baseline manifest status disagrees with count")
    return expected, normalized_demotions


def counter_from_warnings(warnings: list[WarningDiagnostic]) -> Counter[tuple[str, str, str]]:
    return Counter(warning.signature() for warning in warnings)


def format_signature(signature: tuple[str, str, str]) -> str:
    path, option, message = signature
    option_suffix = f" [{option}]" if option else ""
    return f"{path}: warning: {message}{option_suffix}"


def regression_message(
    expected: Counter[tuple[str, str, str]],
    actual: Counter[tuple[str, str, str]],
) -> str | None:
    regressions: list[str] = []
    for signature in sorted(actual):
        excess = actual[signature] - expected.get(signature, 0)
        if excess > 0:
            regressions.append(f"  + {excess}x {format_signature(signature)}")
    if not regressions:
        return None
    return "\n".join(
        [
            "compile-warning baseline regression detected; update the baseline only after review",
            *regressions,
        ]
    )


def write_baseline(root: Path, build_dir: Path, baseline: Path, scope: str, demotions: list[str]) -> int:
    output = configure_and_build(root, build_dir, demotions)
    warnings = parse_warnings(root, output, scope)
    payload = manifest_payload(
        baseline_commit=git_head(root),
        compiler=compiler_version(root),
        demotions=demotions,
        scope=scope,
        warnings=warnings,
    )
    baseline.parent.mkdir(parents=True, exist_ok=True)
    baseline.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    print(
        f"wrote compile-warning baseline: {len(warnings)} diagnostics "
        f"({payload['status']}) for {scope}/*"
    )
    return 0


def verify_baseline(root: Path, build_dir: Path, baseline: Path, scope: str) -> int:
    expected, demotions = load_manifest(baseline, scope)
    output = configure_and_build(root, build_dir, demotions)
    warnings = parse_warnings(root, output, scope)
    actual = counter_from_warnings(warnings)
    message = regression_message(expected, actual)
    if message is not None:
        raise CompileWarningBaselineError(message)
    resolved = sum((expected - actual).values())
    print(
        f"compile-warning baseline verified: {sum(actual.values())} diagnostics "
        f"within pinned {sum(expected.values())} for {scope}/*"
    )
    if resolved:
        print(f"compile-warning baseline note: {resolved} pinned diagnostics are no longer present")
    return 0


def run_self_check() -> None:
    root = Path("/repo")
    output = "\n".join(
        [
            "/repo/src/a.cpp:10:3: warning: first warning [-Wunused-function]",
            "/repo/tests/a.cpp:2:1: warning: out of scope [-Wunused-function]",
        ]
    )
    warnings = parse_warnings(root, output, DEFAULT_SCOPE)
    expect(len(warnings) == 1, "parser should keep only scoped warnings")
    expected = counter_from_warnings(warnings)
    expect(regression_message(expected, expected) is None, "matching baseline should pass")
    extra = Counter(expected)
    extra[("src/a.cpp", "-Wunused-function", "first warning")] += 1
    expect(regression_message(expected, extra) is not None, "extra occurrence should fail")
    payload = manifest_payload(
        baseline_commit="0" * 40,
        compiler="fixture compiler",
        demotions=list(DEFAULT_WERROR_DEMOTIONS),
        scope=DEFAULT_SCOPE,
        warnings=warnings,
    )
    expect(sum(warnings_from_manifest(payload).values()) == 1, "manifest round trip should preserve count")


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", type=Path, default=DEFAULT_BASELINE)
    parser.add_argument("--build-dir", type=Path, required=False)
    parser.add_argument("--scope", default=DEFAULT_SCOPE)
    parser.add_argument("--write-baseline", action="store_true")
    parser.add_argument("--demote-warning", action="append", default=[])
    parser.add_argument("--self-check", action="store_true")
    args = parser.parse_args(argv)

    try:
        if args.self_check:
            run_self_check()
            print("compile-warning baseline verifier self-check passed")
            return 0

        root = repo_root()
        baseline = args.baseline if args.baseline.is_absolute() else root / args.baseline
        expect(args.build_dir is not None, "--build-dir is required")
        build_dir = args.build_dir if args.build_dir.is_absolute() else root / args.build_dir
        if args.write_baseline:
            demotions = args.demote_warning or list(DEFAULT_WERROR_DEMOTIONS)
            return write_baseline(root, build_dir, baseline, args.scope, demotions)
        return verify_baseline(root, build_dir, baseline, args.scope)
    except (CompileWarningBaselineError, OSError, json.JSONDecodeError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
