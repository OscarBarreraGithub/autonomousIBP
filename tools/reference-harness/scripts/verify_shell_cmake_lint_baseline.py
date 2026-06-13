#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-autoIBP-TBD
"""CTest gate for the pinned shell and CMake lint baseline."""

from __future__ import annotations

import argparse
import json
import re
import shutil
import subprocess
import sys
import tempfile
from collections import Counter
from dataclasses import dataclass
from pathlib import Path
from typing import Any


DEFAULT_BASELINE = Path(
    "tools/reference-harness/specs/diagnostics/shell-cmake-lint-baseline.json"
)
SHELL_ROOTS = ("tools", "scripts")
CMAKE_CONFIGURE_OPTIONS = (
    "-DBUILD_TESTING=OFF",
    "-DAMFLOW_WITH_GINAC=OFF",
    "-DAMFLOW_WITH_MPFR=OFF",
    "-DAMFLOW_WITH_YAML_CPP=OFF",
)
SHELL_COMMANDS = ("zsh", "bash", "dash", "ksh", "sh")


class ShellCMakeLintError(RuntimeError):
    """Raised when shell/CMake lint diagnostics exceed the pinned baseline."""


@dataclass(frozen=True)
class Diagnostic:
    category: str
    path: str
    kind: str
    message: str
    line: int | None = None

    def signature(self) -> tuple[str, str, str, str]:
        return (self.category, self.path, self.kind, self.message)

    def location(self) -> dict[str, int]:
        if self.line is None:
            return {}
        return {"line": self.line}


@dataclass(frozen=True)
class ToolProbe:
    command: str
    status: str
    resolved_path: str | None = None

    def payload(self) -> dict[str, Any]:
        item: dict[str, Any] = {"command": self.command, "status": self.status}
        if self.resolved_path is not None:
            item["resolved_path"] = self.resolved_path
        return item


@dataclass(frozen=True)
class ScanResult:
    shell_scripts: list[str]
    cmake_files: list[str]
    cmake_lists: list[str]
    shellcheck_probe: ToolProbe
    cmake_format_probe: ToolProbe
    cmake_lint_probe: ToolProbe
    cmake_probe: ToolProbe
    shell_syntax_checked: int
    shell_syntax_passing: int
    cmake_syntax_checked: int
    cmake_syntax_passing: int
    diagnostics: list[Diagnostic]


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise ShellCMakeLintError(message)


def run_command(
    command: list[str],
    *,
    cwd: Path,
) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        command,
        cwd=cwd,
        check=False,
        capture_output=True,
        text=True,
    )


def run_git(root: Path, args: list[str]) -> bytes:
    completed = subprocess.run(
        ["git", "-C", str(root), *args],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if completed.returncode != 0:
        raise ShellCMakeLintError(
            f"git {' '.join(args)} failed in {root}: "
            f"{completed.stderr.decode('utf-8', errors='replace').strip()}"
        )
    return completed.stdout


def git_head(root: Path) -> str:
    return run_git(root, ["rev-parse", "HEAD"]).decode("utf-8").strip()


def tracked_files(root: Path) -> list[str]:
    output = run_git(root, ["ls-files", "-z"])
    return sorted(item.decode("utf-8") for item in output.split(b"\0") if item)


def command_probe(command: str) -> ToolProbe:
    resolved = shutil.which(command)
    if resolved is None:
        return ToolProbe(command=command, status="unavailable")
    return ToolProbe(command=command, status="available", resolved_path=resolved)


def first_line(root: Path, path_text: str) -> str:
    try:
        with (root / path_text).open("r", encoding="utf-8", errors="replace") as stream:
            return stream.readline().strip()
    except OSError:
        return ""


def has_shell_shebang(root: Path, path_text: str) -> bool:
    line = first_line(root, path_text)
    if not line.startswith("#!"):
        return False
    return any(re.search(rf"(^|[/\s]){re.escape(command)}(\s|$)", line) for command in SHELL_COMMANDS)


def is_shell_script(root: Path, path_text: str) -> bool:
    path = Path(path_text)
    if path.suffix == ".sh":
        return True
    return bool(path.parts) and path.parts[0] in SHELL_ROOTS and has_shell_shebang(root, path_text)


def is_cmake_file(path_text: str) -> bool:
    path = Path(path_text)
    return path.name == "CMakeLists.txt" or path.suffix == ".cmake"


def discover_files(root: Path, source_paths: list[str]) -> tuple[list[str], list[str], list[str]]:
    shell_scripts = sorted(path for path in source_paths if is_shell_script(root, path))
    cmake_files = sorted(path for path in source_paths if is_cmake_file(path))
    cmake_lists = sorted(path for path in cmake_files if Path(path).name == "CMakeLists.txt")
    return shell_scripts, cmake_files, cmake_lists


def normalize_output(root: Path, output: str) -> str:
    normalized = output.replace(str(root.resolve()) + "/", "")
    return normalized.strip()


def command_output(root: Path, completed: subprocess.CompletedProcess[str]) -> str:
    return normalize_output(root, "\n".join(part for part in (completed.stdout, completed.stderr) if part))


def shell_interpreter(root: Path, path_text: str) -> str:
    line = first_line(root, path_text)
    for command in SHELL_COMMANDS:
        if re.search(rf"(^|[/\s]){re.escape(command)}(\s|$)", line):
            return command
    return "sh"


def run_shellcheck(root: Path, shell_scripts: list[str]) -> tuple[ToolProbe, list[Diagnostic]]:
    probe = command_probe("shellcheck")
    if probe.status == "unavailable":
        return probe, []

    diagnostics: list[Diagnostic] = []
    for path_text in shell_scripts:
        completed = run_command([probe.resolved_path or probe.command, str(root / path_text)], cwd=root)
        if completed.returncode != 0:
            diagnostics.append(
                Diagnostic(
                    category="shellcheck",
                    path=path_text,
                    kind="shellcheck",
                    message=command_output(root, completed) or f"shellcheck exited {completed.returncode}",
                )
            )
    return probe, diagnostics


def run_shell_syntax(root: Path, shell_scripts: list[str]) -> tuple[int, int, list[Diagnostic]]:
    diagnostics: list[Diagnostic] = []
    checked = 0
    failed_paths: set[str] = set()
    for path_text in shell_scripts:
        interpreter = shell_interpreter(root, path_text)
        resolved = shutil.which(interpreter)
        checked += 1
        if resolved is None:
            failed_paths.add(path_text)
            diagnostics.append(
                Diagnostic(
                    category="shell-syntax",
                    path=path_text,
                    kind="interpreter-unavailable",
                    message=f"{interpreter} is not available for noexec syntax check",
                )
            )
            continue
        completed = run_command([resolved, "-n", str(root / path_text)], cwd=root)
        if completed.returncode != 0:
            failed_paths.add(path_text)
            diagnostics.append(
                Diagnostic(
                    category="shell-syntax",
                    path=path_text,
                    kind=f"{interpreter}-noexec",
                    message=command_output(root, completed) or f"{interpreter} -n exited {completed.returncode}",
                )
            )
    return checked, checked - len(failed_paths), diagnostics


def run_cmake_format(root: Path, cmake_lists: list[str]) -> tuple[ToolProbe, list[Diagnostic]]:
    probe = command_probe("cmake-format")
    if probe.status == "unavailable":
        return probe, []

    diagnostics: list[Diagnostic] = []
    for path_text in cmake_lists:
        completed = run_command([probe.resolved_path or probe.command, "--check", str(root / path_text)], cwd=root)
        if completed.returncode != 0:
            diagnostics.append(
                Diagnostic(
                    category="cmake-format",
                    path=path_text,
                    kind="format",
                    message=command_output(root, completed) or f"cmake-format exited {completed.returncode}",
                )
            )
    return probe, diagnostics


def run_cmake_lint(root: Path, cmake_lists: list[str]) -> tuple[ToolProbe, list[Diagnostic]]:
    probe = command_probe("cmake-lint")
    if probe.status == "unavailable":
        return probe, []

    diagnostics: list[Diagnostic] = []
    for path_text in cmake_lists:
        completed = run_command([probe.resolved_path or probe.command, str(root / path_text)], cwd=root)
        if completed.returncode != 0:
            diagnostics.append(
                Diagnostic(
                    category="cmake-lint",
                    path=path_text,
                    kind="lint",
                    message=command_output(root, completed) or f"cmake-lint exited {completed.returncode}",
                )
            )
    return probe, diagnostics


def run_cmake_syntax(
    root: Path,
    build_dir: Path,
    cmake_files: list[str],
    cmake_lists: list[str],
) -> tuple[ToolProbe, int, int, list[Diagnostic]]:
    probe = command_probe("cmake")
    diagnostics: list[Diagnostic] = []
    failed_paths: set[str] = set()
    checked = len(cmake_files)
    if probe.status == "unavailable":
        for path_text in cmake_files:
            failed_paths.add(path_text)
            diagnostics.append(
                Diagnostic(
                    category="cmake-syntax",
                    path=path_text,
                    kind="cmake-unavailable",
                    message="cmake is not available for fallback syntax checks",
                )
            )
        return probe, checked, 0, diagnostics

    cmake_command = probe.resolved_path or probe.command
    for path_text in cmake_files:
        if Path(path_text).name == "CMakeLists.txt":
            continue
        completed = run_command([cmake_command, "-P", str(root / path_text)], cwd=root)
        if completed.returncode != 0:
            failed_paths.add(path_text)
            diagnostics.append(
                Diagnostic(
                    category="cmake-syntax",
                    path=path_text,
                    kind="script-mode",
                    message=command_output(root, completed) or f"cmake -P exited {completed.returncode}",
                )
            )

    if cmake_lists:
        if build_dir.exists():
            shutil.rmtree(build_dir)
        configure = [
            cmake_command,
            "-S",
            str(root),
            "-B",
            str(build_dir),
            *CMAKE_CONFIGURE_OPTIONS,
        ]
        completed = run_command(configure, cwd=root)
        if completed.returncode != 0:
            for path_text in cmake_lists:
                failed_paths.add(path_text)
                diagnostics.append(
                    Diagnostic(
                        category="cmake-syntax",
                        path=path_text,
                        kind="configure",
                        message=command_output(root, completed) or f"cmake configure exited {completed.returncode}",
                    )
                )

    return probe, checked, checked - len(failed_paths), diagnostics


def scan(root: Path, build_dir: Path, source_paths: list[str] | None = None) -> ScanResult:
    paths = tracked_files(root) if source_paths is None else sorted(source_paths)
    shell_scripts, cmake_files, cmake_lists = discover_files(root, paths)
    shellcheck_probe, shellcheck_diagnostics = run_shellcheck(root, shell_scripts)
    shell_syntax_checked, shell_syntax_passing, shell_syntax_diagnostics = run_shell_syntax(root, shell_scripts)
    cmake_format_probe, cmake_format_diagnostics = run_cmake_format(root, cmake_lists)
    cmake_lint_probe, cmake_lint_diagnostics = run_cmake_lint(root, cmake_lists)
    cmake_probe, cmake_syntax_checked, cmake_syntax_passing, cmake_syntax_diagnostics = run_cmake_syntax(
        root,
        build_dir,
        cmake_files,
        cmake_lists,
    )
    diagnostics = sorted(
        [
            *shellcheck_diagnostics,
            *shell_syntax_diagnostics,
            *cmake_format_diagnostics,
            *cmake_lint_diagnostics,
            *cmake_syntax_diagnostics,
        ],
        key=lambda item: (item.category, item.path, item.kind, item.message),
    )
    return ScanResult(
        shell_scripts=shell_scripts,
        cmake_files=cmake_files,
        cmake_lists=cmake_lists,
        shellcheck_probe=shellcheck_probe,
        cmake_format_probe=cmake_format_probe,
        cmake_lint_probe=cmake_lint_probe,
        cmake_probe=cmake_probe,
        shell_syntax_checked=shell_syntax_checked,
        shell_syntax_passing=shell_syntax_passing,
        cmake_syntax_checked=cmake_syntax_checked,
        cmake_syntax_passing=cmake_syntax_passing,
        diagnostics=diagnostics,
    )


def grouped_diagnostics(diagnostics: list[Diagnostic]) -> list[dict[str, Any]]:
    groups: dict[tuple[str, str, str, str], list[Diagnostic]] = {}
    for diagnostic in diagnostics:
        groups.setdefault(diagnostic.signature(), []).append(diagnostic)

    entries: list[dict[str, Any]] = []
    for signature in sorted(groups):
        category, path, kind, message = signature
        occurrences = sorted(
            (diagnostic.location() for diagnostic in groups[signature]),
            key=lambda item: item.get("line", 0),
        )
        entries.append(
            {
                "category": category,
                "path": path,
                "kind": kind,
                "message": message,
                "occurrences": occurrences,
            }
        )
    return entries


def manifest_payload(*, baseline_commit: str, result: ScanResult) -> dict[str, Any]:
    return {
        "schema_version": 1,
        "tool": "shell-cmake-lint-baseline",
        "baseline_commit": baseline_commit,
        "baseline_posture": (
            "Existing diagnostics are pinned as accepted findings. The CTest gate "
            "allows accepted diagnostics to be resolved, but fails on new or "
            "additional diagnostics unless the baseline is intentionally updated."
        ),
        "status": "clean" if not result.diagnostics else "diagnostics",
        "diagnostic_count": len(result.diagnostics),
        "comparison_key": [
            "category",
            "path",
            "kind",
            "message",
            "occurrence_count",
        ],
        "discovered_files": {
            "shell_scripts": result.shell_scripts,
            "cmake_files": result.cmake_files,
            "cmake_lists": result.cmake_lists,
        },
        "preferred_tools": {
            "shellcheck": result.shellcheck_probe.payload(),
            "cmake-format": result.cmake_format_probe.payload(),
            "cmake-lint": result.cmake_lint_probe.payload(),
        },
        "fallback_checks": {
            "reason": (
                "Preferred shell/CMake lint tools are optional in this environment; "
                "when unavailable, the gate falls back to interpreter noexec checks "
                "for shell scripts plus CMake script/configure parse checks."
            ),
            "shell_syntax": {
                "file_count": result.shell_syntax_checked,
                "passing_file_count": result.shell_syntax_passing,
            },
            "cmake_syntax": {
                "cmake": result.cmake_probe.payload(),
                "file_count": result.cmake_syntax_checked,
                "passing_file_count": result.cmake_syntax_passing,
                "configure_options": list(CMAKE_CONFIGURE_OPTIONS),
            },
        },
        "diagnostics": grouped_diagnostics(result.diagnostics),
    }


def diagnostics_from_manifest(manifest: dict[str, Any]) -> Counter[tuple[str, str, str, str]]:
    raw_diagnostics = manifest.get("diagnostics")
    expect(isinstance(raw_diagnostics, list), "manifest diagnostics must be a list")
    counter: Counter[tuple[str, str, str, str]] = Counter()
    for index, item in enumerate(raw_diagnostics):
        expect(isinstance(item, dict), f"diagnostics[{index}] must be an object")
        category = item.get("category")
        path = item.get("path")
        kind = item.get("kind")
        message = item.get("message")
        occurrences = item.get("occurrences")
        expect(isinstance(category, str) and category.strip(), f"diagnostics[{index}].category must be a string")
        expect(isinstance(path, str) and path.strip(), f"diagnostics[{index}].path must be a string")
        expect(isinstance(kind, str) and kind.strip(), f"diagnostics[{index}].kind must be a string")
        expect(isinstance(message, str) and message.strip(), f"diagnostics[{index}].message must be a string")
        expect(isinstance(occurrences, list), f"diagnostics[{index}].occurrences must be a list")
        expect(occurrences, f"diagnostics[{index}].occurrences must not be empty")
        for occurrence_index, occurrence in enumerate(occurrences):
            expect(
                isinstance(occurrence, dict),
                f"diagnostics[{index}].occurrences[{occurrence_index}] must be an object",
            )
            if "line" in occurrence:
                expect(isinstance(occurrence["line"], int), "diagnostic occurrence line must be an integer")
        counter[(category.strip(), path.strip(), kind.strip(), message.strip())] += len(occurrences)
    return counter


def load_manifest(path: Path) -> Counter[tuple[str, str, str, str]]:
    with path.open("r", encoding="utf-8") as stream:
        manifest = json.load(stream)
    expect(isinstance(manifest, dict), "baseline manifest must contain a JSON object")
    expect(manifest.get("schema_version") == 1, "baseline manifest schema_version must be 1")
    expect(manifest.get("tool") == "shell-cmake-lint-baseline", "baseline manifest tool mismatch")
    expected = diagnostics_from_manifest(manifest)
    diagnostic_count = manifest.get("diagnostic_count")
    expect(
        isinstance(diagnostic_count, int) and diagnostic_count == sum(expected.values()),
        "baseline manifest diagnostic_count does not match diagnostics",
    )
    status = manifest.get("status")
    expect(status in {"clean", "diagnostics"}, "baseline manifest status must be clean or diagnostics")
    expect((status == "clean") == (diagnostic_count == 0), "baseline manifest status disagrees with count")
    discovered_files = manifest.get("discovered_files")
    expect(isinstance(discovered_files, dict), "baseline manifest discovered_files must be an object")
    for key in ("shell_scripts", "cmake_files", "cmake_lists"):
        expect(isinstance(discovered_files.get(key), list), f"baseline manifest {key} must be a list")
    return expected


def counter_from_diagnostics(diagnostics: list[Diagnostic]) -> Counter[tuple[str, str, str, str]]:
    return Counter(diagnostic.signature() for diagnostic in diagnostics)


def format_signature(signature: tuple[str, str, str, str]) -> str:
    category, path, kind, message = signature
    return f"{path}: {category}/{kind}: {message}"


def regression_message(
    expected: Counter[tuple[str, str, str, str]],
    actual: Counter[tuple[str, str, str, str]],
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
            "shell/CMake lint baseline regression detected; update the baseline only after review",
            *regressions,
        ]
    )


def write_baseline(root: Path, build_dir: Path, baseline: Path) -> int:
    result = scan(root, build_dir)
    payload = manifest_payload(baseline_commit=git_head(root), result=result)
    baseline.parent.mkdir(parents=True, exist_ok=True)
    baseline.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    print(
        "wrote shell/CMake lint baseline: "
        f"{len(result.diagnostics)} diagnostics; "
        f"shell syntax {result.shell_syntax_passing}/{result.shell_syntax_checked}; "
        f"CMake syntax {result.cmake_syntax_passing}/{result.cmake_syntax_checked}"
    )
    return 0


def verify_baseline(root: Path, build_dir: Path, baseline: Path) -> int:
    expected = load_manifest(baseline)
    result = scan(root, build_dir)
    actual = counter_from_diagnostics(result.diagnostics)
    message = regression_message(expected, actual)
    if message is not None:
        raise ShellCMakeLintError(message)
    resolved = sum((expected - actual).values())
    print(
        "shell/CMake lint baseline verified: "
        f"{sum(actual.values())} diagnostics within pinned {sum(expected.values())}; "
        f"shell syntax {result.shell_syntax_passing}/{result.shell_syntax_checked}; "
        f"CMake syntax {result.cmake_syntax_passing}/{result.cmake_syntax_checked}"
    )
    if resolved:
        print(f"shell/CMake lint baseline note: {resolved} pinned diagnostics are no longer present")
    return 0


def run_self_check() -> None:
    with tempfile.TemporaryDirectory() as tmpdir:
        root = Path(tmpdir)
        (root / "tools").mkdir()
        (root / "scripts").mkdir()
        (root / "references").mkdir()
        (root / "references" / "fetch.sh").write_text("#!/usr/bin/env bash\ntrue\n", encoding="utf-8")
        (root / "tools" / "helper").write_text("#!/bin/sh\ntrue\n", encoding="utf-8")
        (root / "tools" / "helper.py").write_text("#!/usr/bin/env python3\n", encoding="utf-8")
        shell_scripts, cmake_files, cmake_lists = discover_files(
            root,
            [
                "references/fetch.sh",
                "tools/helper",
                "tools/helper.py",
                "CMakeLists.txt",
                "cmake/Dependencies.cmake",
            ],
        )
        expect(shell_scripts == ["references/fetch.sh", "tools/helper"], "shell discovery should match .sh and shell shebangs")
        expect(cmake_files == ["CMakeLists.txt", "cmake/Dependencies.cmake"], "CMake discovery should include lists and modules")
        expect(cmake_lists == ["CMakeLists.txt"], "CMakeLists discovery should be narrowed")

    diagnostic = Diagnostic(
        category="shell-syntax",
        path="bad.sh",
        kind="bash-noexec",
        message="syntax error",
    )
    clean: Counter[tuple[str, str, str, str]] = Counter()
    dirty = Counter({diagnostic.signature(): 1})
    expect(regression_message(clean, clean) is None, "matching clean baseline should pass")
    expect(regression_message(clean, dirty) is not None, "new diagnostic should fail")
    payload = manifest_payload(
        baseline_commit="0" * 40,
        result=ScanResult(
            shell_scripts=["good.sh"],
            cmake_files=["CMakeLists.txt"],
            cmake_lists=["CMakeLists.txt"],
            shellcheck_probe=ToolProbe("shellcheck", "unavailable"),
            cmake_format_probe=ToolProbe("cmake-format", "unavailable"),
            cmake_lint_probe=ToolProbe("cmake-lint", "unavailable"),
            cmake_probe=ToolProbe("cmake", "available", "/usr/bin/cmake"),
            shell_syntax_checked=1,
            shell_syntax_passing=1,
            cmake_syntax_checked=1,
            cmake_syntax_passing=1,
            diagnostics=[],
        ),
    )
    expect(sum(diagnostics_from_manifest(payload).values()) == 0, "clean manifest should round trip")


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", type=Path, default=DEFAULT_BASELINE)
    parser.add_argument("--build-dir", type=Path)
    parser.add_argument("--write-baseline", action="store_true")
    parser.add_argument("--self-check", action="store_true")
    args = parser.parse_args(argv)

    try:
        if args.self_check:
            run_self_check()
            print("shell/CMake lint baseline verifier self-check passed")
            return 0

        root = repo_root()
        baseline = args.baseline if args.baseline.is_absolute() else root / args.baseline
        expect(args.build_dir is not None, "--build-dir is required")
        build_dir = args.build_dir if args.build_dir.is_absolute() else root / args.build_dir
        if args.write_baseline:
            return write_baseline(root, build_dir, baseline)
        return verify_baseline(root, build_dir, baseline)
    except (ShellCMakeLintError, OSError, json.JSONDecodeError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
