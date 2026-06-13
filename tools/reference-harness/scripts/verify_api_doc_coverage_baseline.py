#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-autoIBP-TBD
"""CTest gate for the pinned C++ public API documentation baseline."""

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
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


DEFAULT_BASELINE = Path("tools/reference-harness/specs/diagnostics/api-doc-coverage-baseline.json")
HEADER_SUFFIXES = {".h", ".hh", ".hpp", ".hxx"}
PUBLIC_HEADER_ROOT = "include"
ACCESS_RE = re.compile(r"^\s*(public|private|protected)\s*:\s*$")
CLASS_OPEN_RE = re.compile(r"\b(class|struct)\s+([A-Za-z_]\w*)\b[^;{]*\{")
NAMESPACE_OPEN_RE = re.compile(r"\bnamespace\s+([A-Za-z_]\w*)\s*\{")
OPERATOR_NAME_RE = re.compile(
    r"(operator\s*(?:==|!=|<=|>=|<=>|[-+*/%&|^~!<>=,()[\]]+|[A-Za-z_]\w*))\s*$"
)
IDENT_NAME_RE = re.compile(r"(~?[A-Za-z_]\w*(?:::[~A-Za-z_]\w*)?)\s*$")
FORBIDDEN_DECLARATION_PREFIXES = (
    "#",
    "case ",
    "class ",
    "do ",
    "else ",
    "enum ",
    "for ",
    "if ",
    "namespace ",
    "return ",
    "static_assert",
    "struct ",
    "switch ",
    "template ",
    "typedef ",
    "using ",
    "while ",
)


class ApiDocCoverageBaselineError(RuntimeError):
    """Raised when public API documentation coverage regresses."""


@dataclass(frozen=True)
class ScopeFrame:
    name: str
    depth: int
    kind: str
    access: str


@dataclass(frozen=True)
class PublicFunction:
    path: str
    line: int
    qualified_name: str
    declaration: str
    documented: bool

    def signature(self) -> tuple[str, str, str]:
        return (self.path, self.qualified_name, self.declaration)

    def as_json(self) -> dict[str, Any]:
        return {
            "path": self.path,
            "line": self.line,
            "qualified_name": self.qualified_name,
            "declaration": self.declaration,
        }


@dataclass(frozen=True)
class ApiDocMeasurement:
    header_count: int
    functions: list[PublicFunction]

    @property
    def documented_count(self) -> int:
        return sum(1 for function in self.functions if function.documented)

    @property
    def undocumented_count(self) -> int:
        return len(self.functions) - self.documented_count

    @property
    def coverage_percent(self) -> float:
        if not self.functions:
            return 100.0
        return round((self.documented_count / len(self.functions)) * 100.0, 2)


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise ApiDocCoverageBaselineError(message)


def utc_now() -> str:
    return datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def run_command(command: list[str], *, cwd: Path, allow_failure: bool = False) -> subprocess.CompletedProcess[str]:
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
        raise ApiDocCoverageBaselineError(
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
        raise ApiDocCoverageBaselineError(
            f"git {' '.join(args)} failed in {root}: "
            f"{completed.stderr.decode('utf-8', errors='replace').strip()}"
        )
    return completed.stdout


def git_head(root: Path) -> str:
    return run_git(root, ["rev-parse", "HEAD"]).decode("utf-8").strip()


def tracked_files(root: Path) -> list[str]:
    output = run_git(root, ["ls-files", "-z"])
    return sorted(item.decode("utf-8") for item in output.split(b"\0") if item)


def tracked_public_headers(root: Path) -> list[str]:
    headers: list[str] = []
    for path_text in tracked_files(root):
        path = Path(path_text)
        if path.parts and path.parts[0] == PUBLIC_HEADER_ROOT and path.suffix.lower() in HEADER_SUFFIXES:
            headers.append(path_text)
    return headers


def doxygen_probe(root: Path) -> dict[str, Any]:
    executable = shutil.which("doxygen")
    if executable is None:
        return {
            "status": "unavailable",
            "checked_command": "doxygen",
            "notes": (
                "No doxygen executable was found in PATH at baseline capture; "
                "the gate uses the requested leading-comment fallback."
            ),
        }
    version = run_command([executable, "--version"], cwd=root, allow_failure=True)
    output = "\n".join(part for part in (version.stdout, version.stderr) if part).strip()
    return {
        "status": "available",
        "checked_command": "doxygen",
        "resolved_path": executable,
        "version": output.splitlines()[0].strip() if output.splitlines() else "unknown",
        "notes": (
            "Doxygen is available, but this portable CTest gate compares the "
            "leading-comment fallback surface captured in the baseline."
        ),
    }


def strip_comments(line: str, in_block_comment: bool) -> tuple[str, bool]:
    output: list[str] = []
    index = 0
    while index < len(line):
        if in_block_comment:
            end = line.find("*/", index)
            if end == -1:
                return "".join(output), True
            index = end + 2
            in_block_comment = False
            continue

        line_comment = line.find("//", index)
        block_comment = line.find("/*", index)
        if line_comment == -1 and block_comment == -1:
            output.append(line[index:])
            break
        if line_comment != -1 and (block_comment == -1 or line_comment < block_comment):
            output.append(line[index:line_comment])
            break
        output.append(line[index:block_comment])
        end = line.find("*/", block_comment + 2)
        if end == -1:
            in_block_comment = True
            break
        index = end + 2
    return "".join(output), in_block_comment


def mask_string_literals(line: str) -> str:
    output: list[str] = []
    quote: str | None = None
    escaped = False
    for char in line:
        if quote is not None:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == quote:
                quote = None
            output.append(" ")
            continue
        if char in {'"', "'"}:
            quote = char
            output.append(" ")
            continue
        output.append(char)
    return "".join(output)


def code_lines(lines: list[str]) -> list[str]:
    stripped_lines: list[str] = []
    in_block_comment = False
    for line in lines:
        without_comments, in_block_comment = strip_comments(line, in_block_comment)
        stripped_lines.append(mask_string_literals(without_comments))
    return stripped_lines


def doxygen_comment_lines(lines: list[str]) -> set[int]:
    documented: set[int] = set()
    in_doc_block = False
    for index, line in enumerate(lines):
        stripped = line.lstrip()
        if in_doc_block:
            documented.add(index)
            if "*/" in stripped:
                in_doc_block = False
            continue
        if stripped.startswith("///"):
            documented.add(index)
            continue
        if stripped.startswith("/**"):
            documented.add(index)
            if "*/" not in stripped:
                in_doc_block = True
    return documented


def has_leading_doxygen(doc_lines: set[int], start_index: int) -> bool:
    previous = start_index - 1
    return previous in doc_lines


def split_return_prefix(lines: list[str], index: int) -> bool:
    if index < 0:
        return False
    stripped = lines[index].strip()
    if not stripped:
        return False
    if any(token in stripped for token in (";", "{", "}", "(", ")", "=")):
        return False
    if ACCESS_RE.match(stripped):
        return False
    return not stripped.startswith(FORBIDDEN_DECLARATION_PREFIXES)


def trim_function_body(text: str) -> str:
    balance = 0
    for index, char in enumerate(text):
        if char == "(":
            balance += 1
        elif char == ")":
            balance -= 1
        elif char == "{" and balance == 0:
            return text[:index].rstrip()
    return text


def normalized_declaration(statement_lines: list[str]) -> str:
    text = " ".join(part.strip() for part in statement_lines if part.strip())
    text = re.sub(r"\s+", " ", text).strip()
    return trim_function_body(text)


def paren_balance(text: str) -> int:
    balance = 0
    for char in text:
        if char == "(":
            balance += 1
        elif char == ")":
            balance -= 1
    return balance


def declaration_complete(statement_lines: list[str]) -> bool:
    text = normalized_declaration(statement_lines)
    if not text or paren_balance(text) != 0:
        return False
    return ";" in text or "{" in " ".join(statement_lines)


def declaration_name(statement: str) -> str | None:
    first_paren = statement.find("(")
    if first_paren == -1:
        return None
    prefix = statement[:first_paren].strip()
    if not prefix:
        return None
    if "=" in prefix:
        return None
    if prefix.startswith(FORBIDDEN_DECLARATION_PREFIXES):
        return None
    operator_match = OPERATOR_NAME_RE.search(prefix)
    if operator_match is not None:
        return re.sub(r"\s+", "", operator_match.group(1))
    identifier_match = IDENT_NAME_RE.search(prefix)
    if identifier_match is None:
        return None
    name = identifier_match.group(1)
    if name in {"if", "for", "switch", "while"}:
        return None
    return name


def is_public_scope(class_stack: list[ScopeFrame]) -> bool:
    return not class_stack or class_stack[-1].access == "public"


def namespace_names(namespace_stack: list[ScopeFrame]) -> list[str]:
    return [frame.name for frame in namespace_stack]


def class_names(class_stack: list[ScopeFrame]) -> list[str]:
    return [frame.name for frame in class_stack]


def qualified_name(name: str, namespace_stack: list[ScopeFrame], class_stack: list[ScopeFrame]) -> str:
    scope = [*namespace_names(namespace_stack), *class_names(class_stack)]
    return "::".join([*scope, name]) if scope else name


def update_access(class_stack: list[ScopeFrame], access: str) -> None:
    current = class_stack[-1]
    class_stack[-1] = ScopeFrame(
        name=current.name,
        depth=current.depth,
        kind=current.kind,
        access=access,
    )


def scan_header(root: Path, path_text: str) -> list[PublicFunction]:
    path = root / path_text
    lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    code = code_lines(lines)
    doc_lines = doxygen_comment_lines(lines)
    functions: list[PublicFunction] = []
    namespace_stack: list[ScopeFrame] = []
    class_stack: list[ScopeFrame] = []
    brace_depth = 0
    pending_lines: list[str] = []
    pending_start_index = 0
    pending_documented = False

    for index, line in enumerate(code):
        stripped = line.strip()
        access_match = ACCESS_RE.match(stripped)
        if access_match is not None and class_stack:
            update_access(class_stack, access_match.group(1))

        if pending_lines:
            pending_lines.append(stripped)
            if declaration_complete(pending_lines):
                declaration = normalized_declaration(pending_lines).rstrip(";").strip()
                name = declaration_name(declaration)
                if name is not None:
                    functions.append(
                        PublicFunction(
                            path=path_text,
                            line=pending_start_index + 1,
                            qualified_name=qualified_name(name, namespace_stack, class_stack),
                            declaration=declaration,
                            documented=pending_documented,
                        )
                    )
                pending_lines = []
        elif "(" in stripped and is_public_scope(class_stack):
            first_paren = stripped.find("(")
            first_equal = stripped.find("=")
            if first_equal == -1 or first_equal > first_paren:
                start_index = index - 1 if split_return_prefix(code, index - 1) else index
                statement_lines = code[start_index : index + 1] if start_index < index else [stripped]
                declaration = normalized_declaration(statement_lines)
                if declaration_name(declaration) is not None:
                    pending_lines = statement_lines
                    pending_start_index = start_index
                    pending_documented = has_leading_doxygen(doc_lines, start_index)
                    if declaration_complete(pending_lines):
                        finished = normalized_declaration(pending_lines).rstrip(";").strip()
                        name = declaration_name(finished)
                        if name is not None:
                            functions.append(
                                PublicFunction(
                                    path=path_text,
                                    line=pending_start_index + 1,
                                    qualified_name=qualified_name(name, namespace_stack, class_stack),
                                    declaration=finished,
                                    documented=pending_documented,
                                )
                            )
                        pending_lines = []

        namespace_match = NAMESPACE_OPEN_RE.search(stripped)
        class_match = CLASS_OPEN_RE.search(stripped)
        opens = stripped.count("{")
        closes = stripped.count("}")
        if namespace_match is not None:
            namespace_stack.append(
                ScopeFrame(
                    name=namespace_match.group(1),
                    depth=brace_depth + opens,
                    kind="namespace",
                    access="public",
                )
            )
        if class_match is not None:
            kind = class_match.group(1)
            class_stack.append(
                ScopeFrame(
                    name=class_match.group(2),
                    depth=brace_depth + opens,
                    kind=kind,
                    access="public" if kind == "struct" else "private",
                )
            )

        brace_depth += opens - closes
        while class_stack and brace_depth < class_stack[-1].depth:
            class_stack.pop()
        while namespace_stack and brace_depth < namespace_stack[-1].depth:
            namespace_stack.pop()

    if pending_lines:
        raise ApiDocCoverageBaselineError(f"unterminated public function declaration in {path_text}")
    return functions


def scan_headers(root: Path, headers: list[str]) -> ApiDocMeasurement:
    functions: list[PublicFunction] = []
    for path_text in sorted(headers):
        functions.extend(scan_header(root, path_text))
    return ApiDocMeasurement(header_count=len(headers), functions=sorted(functions, key=lambda item: item.signature()))


def grouped_functions(functions: list[PublicFunction]) -> list[dict[str, Any]]:
    groups: dict[tuple[str, str, str], list[PublicFunction]] = {}
    for function in functions:
        groups.setdefault(function.signature(), []).append(function)

    entries: list[dict[str, Any]] = []
    for signature in sorted(groups):
        path, name, declaration = signature
        occurrences = sorted(
            ({"line": function.line} for function in groups[signature]),
            key=lambda item: item["line"],
        )
        entries.append(
            {
                "path": path,
                "qualified_name": name,
                "declaration": declaration,
                "occurrences": occurrences,
            }
        )
    return entries


def manifest_payload(
    root: Path,
    measurement: ApiDocMeasurement,
    *,
    baseline_commit: str | None = None,
    captured_at_utc: str | None = None,
    doxygen: dict[str, Any] | None = None,
) -> dict[str, Any]:
    documented = [function for function in measurement.functions if function.documented]
    undocumented = [function for function in measurement.functions if not function.documented]
    return {
        "schema_version": 1,
        "scope": "cpp-public-api-doc-coverage-baseline",
        "tool": "leading-doxygen-comment-fallback",
        "baseline_commit": baseline_commit or git_head(root),
        "baseline_captured_at_utc": captured_at_utc or utc_now(),
        "source_surface": {
            "root": PUBLIC_HEADER_ROOT,
            "tracked_header_count": measurement.header_count,
            "header_suffixes": sorted(HEADER_SUFFIXES),
        },
        "doxygen_probe": doxygen or doxygen_probe(root),
        "documentation_rule": (
            "A public include-surface function is documented only when the declaration is "
            "immediately preceded by a Doxygen-style /// line or /** block."
        ),
        "coverage": {
            "documented_functions": measurement.documented_count,
            "undocumented_functions": measurement.undocumented_count,
            "total_public_functions": len(measurement.functions),
            "percent": measurement.coverage_percent,
        },
        "comparison": {
            "undocumented_functions": (
                "existing undocumented public functions are pinned as diagnostics; "
                "resolved entries are allowed, but new undocumented entries fail"
            ),
            "percent": "actual documented function percent must not fall below the pinned percent",
        },
        "documented_symbols": grouped_functions(documented),
        "undocumented_symbols": grouped_functions(undocumented),
    }


def validate_grouped_functions(raw_functions: Any, key: str) -> Counter[tuple[str, str, str]]:
    expect(isinstance(raw_functions, list), f"{key} must be a list")
    counter: Counter[tuple[str, str, str]] = Counter()
    for index, item in enumerate(raw_functions):
        expect(isinstance(item, dict), f"{key}[{index}] must be an object")
        path = item.get("path")
        name = item.get("qualified_name")
        declaration = item.get("declaration")
        occurrences = item.get("occurrences")
        expect(isinstance(path, str) and path.strip(), f"{key}[{index}].path must be a string")
        expect(isinstance(name, str) and name.strip(), f"{key}[{index}].qualified_name must be a string")
        expect(
            isinstance(declaration, str) and declaration.strip(),
            f"{key}[{index}].declaration must be a string",
        )
        expect(isinstance(occurrences, list) and occurrences, f"{key}[{index}].occurrences must be non-empty")
        for occurrence_index, occurrence in enumerate(occurrences):
            expect(
                isinstance(occurrence, dict),
                f"{key}[{index}].occurrences[{occurrence_index}] must be an object",
            )
            expect(isinstance(occurrence.get("line"), int), "API doc occurrence line must be an integer")
        counter[(path.strip(), name.strip(), declaration.strip())] += len(occurrences)
    return counter


def load_manifest(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as stream:
        manifest = json.load(stream)
    expect(isinstance(manifest, dict), "baseline manifest must contain a JSON object")
    expect(manifest.get("schema_version") == 1, "baseline schema_version must be 1")
    expect(
        manifest.get("scope") == "cpp-public-api-doc-coverage-baseline",
        "baseline scope mismatch",
    )
    expect(manifest.get("tool") == "leading-doxygen-comment-fallback", "baseline tool mismatch")
    coverage = manifest.get("coverage")
    expect(isinstance(coverage, dict), "coverage must be an object")
    for key in ("documented_functions", "undocumented_functions", "total_public_functions"):
        expect(isinstance(coverage.get(key), int), f"coverage.{key} must be an integer")
    expect(isinstance(coverage.get("percent"), (int, float)), "coverage.percent must be numeric")
    documented = validate_grouped_functions(manifest.get("documented_symbols"), "documented_symbols")
    undocumented = validate_grouped_functions(manifest.get("undocumented_symbols"), "undocumented_symbols")
    expect(
        coverage["documented_functions"] == sum(documented.values()),
        "coverage.documented_functions does not match documented_symbols",
    )
    expect(
        coverage["undocumented_functions"] == sum(undocumented.values()),
        "coverage.undocumented_functions does not match undocumented_symbols",
    )
    expect(
        coverage["total_public_functions"] == coverage["documented_functions"] + coverage["undocumented_functions"],
        "coverage total does not match documented plus undocumented counts",
    )
    return manifest


def undocumented_counter(functions: list[PublicFunction]) -> Counter[tuple[str, str, str]]:
    return Counter(function.signature() for function in functions if not function.documented)


def format_signature(signature: tuple[str, str, str]) -> str:
    path, name, declaration = signature
    return f"{path}: {name}: {declaration}"


def verify_against_manifest(manifest: dict[str, Any], measurement: ApiDocMeasurement) -> list[str]:
    problems: list[str] = []
    expected_undocumented = validate_grouped_functions(
        manifest.get("undocumented_symbols"),
        "undocumented_symbols",
    )
    actual_undocumented = undocumented_counter(measurement.functions)
    regressions: list[str] = []
    for signature in sorted(actual_undocumented):
        excess = actual_undocumented[signature] - expected_undocumented.get(signature, 0)
        if excess > 0:
            regressions.append(f"  + {excess}x {format_signature(signature)}")
    if regressions:
        problems.append(
            "\n".join(
                [
                    "C++ public API documentation baseline regression detected; "
                    "new undocumented include-surface functions appeared",
                    *regressions,
                ]
            )
        )

    expected_percent = float(manifest["coverage"]["percent"])
    if measurement.coverage_percent + 1e-9 < expected_percent:
        problems.append(
            f"C++ public API documentation coverage regression: "
            f"{measurement.coverage_percent:.2f}% is below baseline {expected_percent:.2f}%"
        )
    return problems


def write_baseline(root: Path, baseline: Path) -> int:
    measurement = scan_headers(root, tracked_public_headers(root))
    payload = manifest_payload(root, measurement)
    baseline.parent.mkdir(parents=True, exist_ok=True)
    baseline.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    print(
        "wrote C++ API doc coverage baseline: "
        f"{measurement.documented_count} documented, "
        f"{measurement.undocumented_count} undocumented, "
        f"{measurement.coverage_percent:.2f}% coverage"
    )
    print(f"doxygen probe: {payload['doxygen_probe']['status']}")
    return 0


def verify_baseline(root: Path, baseline: Path) -> int:
    manifest = load_manifest(baseline)
    measurement = scan_headers(root, tracked_public_headers(root))
    problems = verify_against_manifest(manifest, measurement)
    if problems:
        raise ApiDocCoverageBaselineError("\n".join(problems))
    expected_undocumented = int(manifest["coverage"]["undocumented_functions"])
    resolved = expected_undocumented - measurement.undocumented_count
    print(
        "C++ API doc coverage baseline verified: "
        f"{measurement.documented_count} documented, "
        f"{measurement.undocumented_count} undocumented "
        f"(baseline {expected_undocumented}), "
        f"{measurement.coverage_percent:.2f}% coverage "
        f"(baseline {float(manifest['coverage']['percent']):.2f}%)"
    )
    if resolved > 0:
        print(f"C++ API doc coverage note: {resolved} pinned undocumented entries are no longer present")
    return 0


def run_self_check() -> None:
    with tempfile.TemporaryDirectory(prefix="autoibp-api-doc-fixture-") as tmpdir:
        root = Path(tmpdir)
        header = root / "include" / "amflow" / "demo.hpp"
        header.parent.mkdir(parents=True)
        header.write_text(
            "\n".join(
                [
                    "#pragma once",
                    "#include <vector>",
                    "namespace demo {",
                    "/// Documented free function.",
                    "int Documented(int value);",
                    "class Widget {",
                    " public:",
                    "  /** Documented method. */",
                    "  void BlockDocumented() const;",
                    "  void Missing();",
                    " private:",
                    "  void PrivateMissing();",
                    "};",
                    "struct Bag {",
                    "  /// Documented split return.",
                    "  std::vector<int>",
                    "  Values() const;",
                    "  int MissingInline() const {",
                    "    return 0;",
                    "  }",
                    "};",
                    "}",
                    "",
                ]
            ),
            encoding="utf-8",
        )
        measurement = scan_headers(root, ["include/amflow/demo.hpp"])
        expect(len(measurement.functions) == 5, "self-check should find five public functions")
        expect(measurement.documented_count == 3, "self-check should count three documented functions")
        expect(measurement.undocumented_count == 2, "self-check should count two undocumented functions")
        expect(
            not any("PrivateMissing" in function.qualified_name for function in measurement.functions),
            "self-check should ignore private methods",
        )
        payload = manifest_payload(
            root,
            measurement,
            baseline_commit="0" * 40,
            captured_at_utc="2000-01-01T00:00:00Z",
            doxygen={"status": "unavailable", "checked_command": "doxygen"},
        )
        validate_grouped_functions(payload["documented_symbols"], "documented_symbols")
        validate_grouped_functions(payload["undocumented_symbols"], "undocumented_symbols")
        expect(not verify_against_manifest(payload, measurement), "matching baseline should pass")
        regressed = ApiDocMeasurement(
            header_count=measurement.header_count,
            functions=[
                *measurement.functions,
                PublicFunction(
                    path="include/amflow/demo.hpp",
                    line=99,
                    qualified_name="demo::NewUndocumented",
                    declaration="int NewUndocumented()",
                    documented=False,
                ),
            ],
        )
        expect(verify_against_manifest(payload, regressed), "new undocumented function should fail")


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", type=Path, default=DEFAULT_BASELINE)
    parser.add_argument("--write-baseline", action="store_true")
    parser.add_argument("--self-check", action="store_true")
    args = parser.parse_args(argv)

    try:
        if args.self_check:
            run_self_check()
            print("C++ API doc coverage baseline verifier self-check passed")
            return 0

        root = repo_root()
        baseline = args.baseline if args.baseline.is_absolute() else root / args.baseline
        if args.write_baseline:
            return write_baseline(root, baseline)
        return verify_baseline(root, baseline)
    except (ApiDocCoverageBaselineError, OSError, json.JSONDecodeError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
