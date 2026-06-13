#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-autoIBP-TBD
"""CTest gate for documented build/runtime environment-variable reads."""

from __future__ import annotations

import argparse
import ast
import json
import re
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Any


DEFAULT_REGISTRY = Path("tools/reference-harness/specs/env-var-registry.json")
ENV_NAME_RE = re.compile(r"^[A-Z_][A-Z0-9_]*$")
CMAKE_ENV_EXPANSION_RE = re.compile(r"\$ENV\{(?P<name>[^}]+)\}")
CMAKE_DEFINED_ENV_RE = re.compile(r"\bDEFINED\s+ENV\{(?P<name>[^}]+)\}")
CPP_ENV_CALL_RE = re.compile(
    r"\b(?P<api>(?:::)?(?:std::)?getenv|secure_getenv)\s*\((?P<argument>[^)]*)\)"
)
CPP_STRING_ARGUMENT_RE = re.compile(r'^\s*"(?P<name>[A-Z_][A-Z0-9_]*)"\s*$')
CPP_ENVIRON_DECL_RE = re.compile(r"^\s*extern\s+char\s*\*\*\s*environ\s*;\s*$")
CMAKE_SCOPED_ROOTS = {"cmake"}
PYTHON_SCOPED_ROOTS = {"tests", "tools"}
CPP_SCOPED_ROOTS = {"include", "src", "tests", "tools"}
CPP_SUFFIXES = {".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx"}


class EnvRegistryError(RuntimeError):
    """Raised when explicit env-var reads are not documented in the registry."""


@dataclass(frozen=True)
class EnvRead:
    name: str
    path: str
    line: int
    surface: str
    api: str

    def location(self) -> str:
        return f"{self.path}:{self.line} ({self.surface}, {self.api})"


@dataclass(frozen=True)
class DynamicEnvRead:
    path: str
    line: int
    surface: str
    api: str
    expression: str

    def location(self) -> str:
        return f"{self.path}:{self.line} ({self.surface}, {self.api}: {self.expression})"


@dataclass(frozen=True)
class WholeEnvironmentRead:
    path: str
    line: int
    surface: str
    api: str

    def location(self) -> str:
        return f"{self.path}:{self.line} ({self.surface}, {self.api})"

    def registry_key(self) -> tuple[str, str]:
        return (self.path, self.api)


@dataclass(frozen=True)
class EnvAudit:
    reads: list[EnvRead]
    dynamic_reads: list[DynamicEnvRead]
    whole_environment_reads: list[WholeEnvironmentRead]


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def run_git(root: Path, args: list[str]) -> bytes:
    completed = subprocess.run(
        ["git", "-C", str(root), *args],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if completed.returncode != 0:
        raise EnvRegistryError(
            f"git {' '.join(args)} failed in {root}: "
            f"{completed.stderr.decode('utf-8', errors='replace').strip()}"
        )
    return completed.stdout


def tracked_files(root: Path) -> list[str]:
    try:
        output = run_git(root, ["ls-files", "-z"])
    except EnvRegistryError:
        return sorted(
            path.relative_to(root).as_posix()
            for path in root.rglob("*")
            if path.is_file() and ".git" not in path.relative_to(root).parts
        )
    return sorted(item.decode("utf-8") for item in output.split(b"\0") if item)


def path_parts(path_text: str) -> tuple[str, ...]:
    return Path(path_text).parts


def is_cmake_source(path_text: str) -> bool:
    path = Path(path_text)
    parts = path_parts(path_text)
    return (
        path.name == "CMakeLists.txt"
        or path.suffix == ".cmake"
        or (bool(parts) and parts[0] in CMAKE_SCOPED_ROOTS and path.suffix == ".cmake")
    )


def is_python_source(path_text: str) -> bool:
    parts = path_parts(path_text)
    return bool(parts) and parts[0] in PYTHON_SCOPED_ROOTS and Path(path_text).suffix == ".py"


def is_cpp_source(path_text: str) -> bool:
    parts = path_parts(path_text)
    return (
        bool(parts)
        and parts[0] in CPP_SCOPED_ROOTS
        and Path(path_text).suffix.lower() in CPP_SUFFIXES
    )


def validate_env_name(name: str) -> bool:
    return ENV_NAME_RE.fullmatch(name) is not None


def scan_cmake_file(root: Path, path_text: str) -> EnvAudit:
    path = root / path_text
    reads: list[EnvRead] = []
    dynamic_reads: list[DynamicEnvRead] = []
    lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    for line_number, line in enumerate(lines, start=1):
        for pattern, api in (
            (CMAKE_ENV_EXPANSION_RE, "$ENV{}"),
            (CMAKE_DEFINED_ENV_RE, "DEFINED ENV{}"),
        ):
            for match in pattern.finditer(line):
                raw_name = match.group("name").strip()
                if validate_env_name(raw_name):
                    reads.append(EnvRead(raw_name, path_text, line_number, "cmake", api))
                else:
                    dynamic_reads.append(
                        DynamicEnvRead(path_text, line_number, "cmake", api, raw_name)
                    )
    return EnvAudit(reads=reads, dynamic_reads=dynamic_reads, whole_environment_reads=[])


def ast_call_name(node: ast.AST) -> str | None:
    if isinstance(node, ast.Attribute):
        parent = ast_call_name(node.value)
        if parent:
            return parent + "." + node.attr
        return node.attr
    if isinstance(node, ast.Name):
        return node.id
    return None


def is_os_environ(node: ast.AST) -> bool:
    return ast_call_name(node) == "os.environ"


def constant_string(node: ast.AST) -> str | None:
    if isinstance(node, ast.Constant) and isinstance(node.value, str):
        return node.value
    return None


def subscript_slice(node: ast.Subscript) -> ast.AST:
    return node.slice


class PythonEnvVisitor(ast.NodeVisitor):
    def __init__(self, path_text: str) -> None:
        self.path_text = path_text
        self.reads: list[EnvRead] = []
        self.dynamic_reads: list[DynamicEnvRead] = []
        self.whole_environment_reads: list[WholeEnvironmentRead] = []

    def add_name(self, name: str | None, node: ast.AST, api: str, expression: str = "") -> None:
        line = getattr(node, "lineno", 0)
        if name and validate_env_name(name):
            self.reads.append(EnvRead(name, self.path_text, line, "python", api))
            return
        self.dynamic_reads.append(
            DynamicEnvRead(
                self.path_text,
                line,
                "python",
                api,
                expression or ast.unparse(node),
            )
        )

    def add_whole_environment_read(self, node: ast.AST, api: str) -> None:
        self.whole_environment_reads.append(
            WholeEnvironmentRead(
                self.path_text,
                getattr(node, "lineno", 0),
                "python",
                api,
            )
        )

    def visit_Call(self, node: ast.Call) -> None:
        call_name = ast_call_name(node.func)
        if call_name in {"os.getenv", "os.environ.get", "os.environ.pop", "os.environ.setdefault"}:
            if node.args:
                self.add_name(constant_string(node.args[0]), node, call_name, ast.unparse(node.args[0]))
            else:
                self.add_name(None, node, call_name, "missing first argument")
        if call_name == "dict" and node.args and is_os_environ(node.args[0]):
            self.add_whole_environment_read(node, "dict(os.environ)")
        if call_name in {
            "os.environ.copy",
            "os.environ.items",
            "os.environ.keys",
            "os.environ.values",
        }:
            self.add_whole_environment_read(node, call_name)
        self.generic_visit(node)

    def visit_Subscript(self, node: ast.Subscript) -> None:
        if is_os_environ(node.value):
            slice_node = subscript_slice(node)
            self.add_name(constant_string(slice_node), node, "os.environ[]", ast.unparse(slice_node))
        self.generic_visit(node)

    def visit_Compare(self, node: ast.Compare) -> None:
        for operator, comparator in zip(node.ops, node.comparators):
            if isinstance(operator, ast.In) and is_os_environ(comparator):
                self.add_name(constant_string(node.left), node, "in os.environ", ast.unparse(node.left))
        self.generic_visit(node)


def scan_python_file(root: Path, path_text: str) -> EnvAudit:
    path = root / path_text
    source = path.read_text(encoding="utf-8", errors="replace")
    try:
        tree = ast.parse(source, filename=path_text)
    except SyntaxError as exc:
        raise EnvRegistryError(f"cannot parse Python source {path_text}: {exc}") from exc
    visitor = PythonEnvVisitor(path_text)
    visitor.visit(tree)
    return EnvAudit(
        reads=visitor.reads,
        dynamic_reads=visitor.dynamic_reads,
        whole_environment_reads=visitor.whole_environment_reads,
    )


def scan_cpp_file(root: Path, path_text: str) -> EnvAudit:
    path = root / path_text
    reads: list[EnvRead] = []
    dynamic_reads: list[DynamicEnvRead] = []
    whole_environment_reads: list[WholeEnvironmentRead] = []
    lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    for line_number, line in enumerate(lines, start=1):
        for match in CPP_ENV_CALL_RE.finditer(line):
            api = match.group("api").lstrip(":")
            argument = match.group("argument").strip()
            literal = CPP_STRING_ARGUMENT_RE.fullmatch(argument)
            if literal:
                reads.append(EnvRead(literal.group("name"), path_text, line_number, "cpp", api))
            else:
                dynamic_reads.append(
                    DynamicEnvRead(path_text, line_number, "cpp", api, argument)
                )
        if "environ" in line and not CPP_ENVIRON_DECL_RE.fullmatch(line):
            if re.search(r"\benviron\b", line):
                whole_environment_reads.append(
                    WholeEnvironmentRead(
                        path_text,
                        line_number,
                        "cpp",
                        "process environ traversal",
                    )
                )
    return EnvAudit(
        reads=reads,
        dynamic_reads=dynamic_reads,
        whole_environment_reads=whole_environment_reads,
    )


def scan_sources(root: Path, source_paths: list[str]) -> EnvAudit:
    reads: list[EnvRead] = []
    dynamic_reads: list[DynamicEnvRead] = []
    whole_environment_reads: list[WholeEnvironmentRead] = []
    for path_text in source_paths:
        if is_cmake_source(path_text):
            audit = scan_cmake_file(root, path_text)
        elif is_python_source(path_text):
            audit = scan_python_file(root, path_text)
        elif is_cpp_source(path_text):
            audit = scan_cpp_file(root, path_text)
        else:
            continue
        reads.extend(audit.reads)
        dynamic_reads.extend(audit.dynamic_reads)
        whole_environment_reads.extend(audit.whole_environment_reads)
    return EnvAudit(
        reads=sorted(reads, key=lambda item: (item.name, item.path, item.line, item.api)),
        dynamic_reads=sorted(
            dynamic_reads,
            key=lambda item: (item.path, item.line, item.surface, item.api, item.expression),
        ),
        whole_environment_reads=sorted(
            whole_environment_reads,
            key=lambda item: (item.path, item.line, item.surface, item.api),
        ),
    )


def load_json(path: Path) -> dict[str, Any]:
    try:
        payload = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        raise EnvRegistryError(f"{path} is not valid JSON: {exc}") from exc
    if not isinstance(payload, dict):
        raise EnvRegistryError(f"{path} must contain a JSON object")
    return payload


def registry_entries(registry: dict[str, Any]) -> dict[str, dict[str, Any]]:
    if registry.get("schema_version") != 1:
        raise EnvRegistryError("env-var registry schema_version must be 1")
    entries = registry.get("environment_variables")
    if not isinstance(entries, list):
        raise EnvRegistryError("env-var registry environment_variables must be a list")

    by_name: dict[str, dict[str, Any]] = {}
    for index, raw_entry in enumerate(entries):
        if not isinstance(raw_entry, dict):
            raise EnvRegistryError(f"environment_variables[{index}] must be an object")
        name = raw_entry.get("name")
        if not isinstance(name, str) or not validate_env_name(name):
            raise EnvRegistryError(f"environment_variables[{index}].name is invalid: {name!r}")
        if name in by_name:
            raise EnvRegistryError(f"duplicate env-var registry entry: {name}")
        required = raw_entry.get("required")
        if not isinstance(required, bool):
            raise EnvRegistryError(f"{name}: required must be a boolean")
        for field in ("default", "description", "default_behavior", "failure_mode"):
            value = raw_entry.get(field)
            if not isinstance(value, str) or not value.strip():
                raise EnvRegistryError(f"{name}: {field} must be a non-empty string")
        readers = raw_entry.get("readers")
        if not isinstance(readers, list) or not readers:
            raise EnvRegistryError(f"{name}: readers must be a non-empty list")
        for reader_index, reader in enumerate(readers):
            if not isinstance(reader, dict):
                raise EnvRegistryError(f"{name}: readers[{reader_index}] must be an object")
            path = reader.get("path")
            api = reader.get("api")
            if not isinstance(path, str) or not path.strip():
                raise EnvRegistryError(f"{name}: readers[{reader_index}].path must be non-empty")
            if not isinstance(api, str) or not api.strip():
                raise EnvRegistryError(f"{name}: readers[{reader_index}].api must be non-empty")
        by_name[name] = raw_entry
    return by_name


def whole_environment_registry_entries(registry: dict[str, Any]) -> set[tuple[str, str]]:
    raw_entries = registry.get("non_dependency_environment_uses", [])
    if not isinstance(raw_entries, list):
        raise EnvRegistryError("env-var registry non_dependency_environment_uses must be a list")

    entries: set[tuple[str, str]] = set()
    for index, raw_entry in enumerate(raw_entries):
        if not isinstance(raw_entry, dict):
            raise EnvRegistryError(f"non_dependency_environment_uses[{index}] must be an object")
        path = raw_entry.get("path")
        api = raw_entry.get("api")
        description = raw_entry.get("description")
        if not isinstance(path, str) or not path.strip():
            raise EnvRegistryError(
                f"non_dependency_environment_uses[{index}].path must be non-empty"
            )
        if not isinstance(api, str) or not api.strip():
            raise EnvRegistryError(
                f"non_dependency_environment_uses[{index}].api must be non-empty"
            )
        if not isinstance(description, str) or not description.strip():
            raise EnvRegistryError(
                f"non_dependency_environment_uses[{index}].description must be non-empty"
            )
        key = (path, api)
        if key in entries:
            raise EnvRegistryError(
                "duplicate non-dependency environment use: " + f"{path} ({api})"
            )
        entries.add(key)
    return entries


def verify_registry(root: Path, registry_path: Path) -> EnvAudit:
    registry = load_json(registry_path)
    entries = registry_entries(registry)
    whole_environment_entries = whole_environment_registry_entries(registry)
    audit = scan_sources(root, tracked_files(root))

    if audit.dynamic_reads:
        details = "\n".join(f"  - {item.location()}" for item in audit.dynamic_reads)
        raise EnvRegistryError(
            "dynamic environment-variable reads are not allowed by this gate; "
            "use a literal variable name and document it in the registry:\n" + details
        )

    found_names = {read.name for read in audit.reads}
    registry_names = set(entries)
    found_whole_environment = {read.registry_key() for read in audit.whole_environment_reads}
    missing = sorted(found_names - registry_names)
    stale = sorted(registry_names - found_names)
    missing_whole_environment = sorted(found_whole_environment - whole_environment_entries)
    stale_whole_environment = sorted(whole_environment_entries - found_whole_environment)
    errors: list[str] = []
    if missing:
        details = "\n".join(
            f"  - {name}: "
            + ", ".join(read.location() for read in audit.reads if read.name == name)
            for name in missing
        )
        errors.append("Env vars read from source but missing from registry:\n" + details)
    if stale:
        details = "\n".join(f"  - {name}" for name in stale)
        errors.append("Env vars listed in registry but not read from source:\n" + details)
    if missing_whole_environment:
        details = "\n".join(
            f"  - {path} ({api}): "
            + ", ".join(
                read.location()
                for read in audit.whole_environment_reads
                if read.registry_key() == (path, api)
            )
            for path, api in missing_whole_environment
        )
        errors.append(
            "Whole-environment reads/pass-throughs missing from "
            "non_dependency_environment_uses:\n" + details
        )
    if stale_whole_environment:
        details = "\n".join(f"  - {path} ({api})" for path, api in stale_whole_environment)
        errors.append(
            "Whole-environment non-dependency entries listed but not read from source:\n"
            + details
        )
    if errors:
        raise EnvRegistryError("\n\n".join(errors))
    return audit


def write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def self_check() -> None:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        write_text(root / "CMakeLists.txt", 'if(DEFINED ENV{CMAKE_ONLY})\nendif()\n')
        write_text(
            root / "tools" / "sample.py",
            'import os\nVALUE = os.environ.get("PY_TOOL")\nENV = dict(os.environ)\n',
        )
        write_text(
            root / "src" / "sample.cpp",
            '#include <cstdlib>\nextern char** environ;\n'
            'const char* value = std::getenv("CPP_RUNTIME");\n'
            'void copy_env() { for (char** entry = environ; *entry != nullptr; ++entry) {} }\n',
        )
        registry_path = root / "tools" / "reference-harness" / "specs" / "env-var-registry.json"
        synthetic_entries: list[dict[str, Any]] = [
            {
                "name": name,
                "required": False,
                "default": "unset",
                "description": f"Synthetic {name} description.",
                "default_behavior": "Synthetic default behavior.",
                "failure_mode": "Synthetic failure mode.",
                "readers": [{"path": "synthetic", "api": "synthetic"}],
            }
            for name in ("CMAKE_ONLY", "PY_TOOL", "CPP_RUNTIME")
        ]
        registry: dict[str, Any] = {
            "schema_version": 1,
            "environment_variables": synthetic_entries,
            "non_dependency_environment_uses": [
                {
                    "path": "tools/sample.py",
                    "api": "dict(os.environ)",
                    "description": "Synthetic Python whole-environment pass-through.",
                },
                {
                    "path": "src/sample.cpp",
                    "api": "process environ traversal",
                    "description": "Synthetic C++ whole-environment pass-through.",
                },
            ],
        }
        write_text(registry_path, json.dumps(registry, indent=2) + "\n")
        audit = verify_registry(root, registry_path)
        names = {read.name for read in audit.reads}
        if names != {"CMAKE_ONLY", "PY_TOOL", "CPP_RUNTIME"}:
            raise EnvRegistryError(f"self-check scanner missed env vars: {sorted(names)}")
        whole_environment = {read.registry_key() for read in audit.whole_environment_reads}
        expected_whole_environment = {
            ("tools/sample.py", "dict(os.environ)"),
            ("src/sample.cpp", "process environ traversal"),
        }
        if whole_environment != expected_whole_environment:
            raise EnvRegistryError(
                "self-check scanner missed whole-environment reads: "
                f"{sorted(whole_environment)}"
            )

        bad_registry = dict(registry)
        bad_registry["environment_variables"] = [
            entry for entry in synthetic_entries if entry["name"] != "PY_TOOL"
        ]
        write_text(registry_path, json.dumps(bad_registry, indent=2) + "\n")
        try:
            verify_registry(root, registry_path)
        except EnvRegistryError as exc:
            if "PY_TOOL" not in str(exc):
                raise EnvRegistryError("self-check missing-entry failure did not name PY_TOOL") from exc
        else:
            raise EnvRegistryError("self-check expected an undocumented env-var failure")

        bad_registry = dict(registry)
        bad_registry["non_dependency_environment_uses"] = [
            entry
            for entry in registry["non_dependency_environment_uses"]
            if entry["api"] != "dict(os.environ)"
        ]
        write_text(registry_path, json.dumps(bad_registry, indent=2) + "\n")
        try:
            verify_registry(root, registry_path)
        except EnvRegistryError as exc:
            if "dict(os.environ)" not in str(exc):
                raise EnvRegistryError(
                    "self-check whole-environment failure did not name dict(os.environ)"
                ) from exc
        else:
            raise EnvRegistryError(
                "self-check expected an undocumented whole-environment failure"
            )


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--registry",
        type=Path,
        default=repo_root() / DEFAULT_REGISTRY,
        help="Path to the env-var registry JSON.",
    )
    parser.add_argument("--verify", action="store_true", help="Verify registry against source reads.")
    parser.add_argument("--self-check", action="store_true", help="Run scanner failure-mode checks.")
    args = parser.parse_args(argv)

    try:
        if args.self_check:
            self_check()
            print("env-var registry verifier self-check passed")
            return 0

        audit = verify_registry(repo_root(), args.registry)
        print(
            "env-var registry verified: "
            f"{len({read.name for read in audit.reads})} variables, "
            f"{len(audit.reads)} read locations, "
            f"{len({read.registry_key() for read in audit.whole_environment_reads})} "
            "whole-environment pass-throughs"
        )
        return 0
    except EnvRegistryError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
