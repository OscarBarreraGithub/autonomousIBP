#!/usr/bin/env python3
"""CTest gate for the pinned public C++ symbol surface."""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


DEFAULT_MANIFEST = Path("tools/reference-harness/specs/abi/amflow-public-symbols.txt")
MANIFEST_SCHEMA = "amflow-public-symbols-v1"
PROJECT_DEMANGLE_PREFIXES = (
    "amflow::",
    "vtable for amflow::",
    "typeinfo for amflow::",
    "typeinfo name for amflow::",
    "VTT for amflow::",
    "construction vtable for amflow::",
    "non-virtual thunk to amflow::",
    "virtual thunk to amflow::",
    "covariant return thunk to amflow::",
)
RAW_PROJECT_PREFIXES = (
    "_ZN6amflow",
    "_ZNK6amflow",
    "_ZTVN6amflow",
    "_ZTIN6amflow",
    "_ZTSN6amflow",
    "_ZTTN6amflow",
)


class VerificationError(RuntimeError):
    """Raised when the public symbol surface differs from the pin."""


@dataclass(frozen=True)
class SymbolEntry:
    kind: str
    name: str

    def line(self) -> str:
        return f"{self.kind} {self.name}"


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def resolve_path(root: Path, path: Path) -> Path:
    return path if path.is_absolute() else root / path


def sort_entries(entries: set[SymbolEntry] | list[SymbolEntry]) -> list[SymbolEntry]:
    return sorted(entries, key=lambda entry: (entry.name, entry.kind))


def is_archive(path: Path) -> bool:
    try:
        with path.open("rb") as stream:
            return stream.read(8) == b"!<arch>\n"
    except OSError as exc:
        raise VerificationError(f"cannot read symbol target {path}: {exc}") from exc


def run_command(
    command: list[str], *, allow_failure: bool = False
) -> subprocess.CompletedProcess[str]:
    completed = subprocess.run(
        command,
        check=False,
        capture_output=True,
        text=True,
    )
    if not allow_failure and completed.returncode != 0:
        detail = completed.stderr.strip() or completed.stdout.strip()
        raise VerificationError(
            f"command failed ({completed.returncode}): {' '.join(command)}"
            + (f"\n{detail}" if detail else "")
        )
    return completed


def parse_nm_posix(stdout: str) -> list[SymbolEntry]:
    entries: list[SymbolEntry] = []
    for raw_line in stdout.splitlines():
        line = raw_line.strip()
        if not line or line.endswith(":"):
            continue
        parts = line.split()
        if len(parts) < 2:
            continue
        name = parts[0]
        kind = parts[1]
        if len(kind) != 1 or kind == "U":
            continue
        entries.append(SymbolEntry(kind=kind, name=name))
    return entries


def nm_defined_symbols(path: Path) -> tuple[list[SymbolEntry], str]:
    nm = shutil.which("nm")
    if nm is None:
        raise VerificationError("nm is required to verify the public symbol surface")

    if is_archive(path):
        completed = run_command(
            [nm, "-g", "--defined-only", "--format=posix", str(path)]
        )
        return parse_nm_posix(completed.stdout), "nm -g --defined-only"

    dynamic = run_command(
        [nm, "-D", "--defined-only", "--extern-only", "--format=posix", str(path)],
        allow_failure=True,
    )
    dynamic_entries = parse_nm_posix(dynamic.stdout) if dynamic.returncode == 0 else []
    if dynamic_entries:
        return dynamic_entries, "nm -D --defined-only --extern-only"

    completed = run_command(
        [nm, "-g", "--defined-only", "--format=posix", str(path)]
    )
    return parse_nm_posix(completed.stdout), "nm -g --defined-only"


def demangle_names(names: list[str]) -> dict[str, str]:
    cxxfilt = shutil.which("c++filt")
    if cxxfilt is None:
        return {name: name for name in names}

    completed = subprocess.run(
        [cxxfilt],
        input="\n".join(names) + "\n",
        check=False,
        capture_output=True,
        text=True,
    )
    if completed.returncode != 0:
        return {name: name for name in names}
    demangled = completed.stdout.splitlines()
    if len(demangled) != len(names):
        return {name: name for name in names}
    return dict(zip(names, demangled))


def is_project_symbol(name: str, demangled: str) -> bool:
    if any(demangled.startswith(prefix) for prefix in PROJECT_DEMANGLE_PREFIXES):
        return True
    return any(name.startswith(prefix) for prefix in RAW_PROJECT_PREFIXES)


def project_symbol_surface(entries: list[SymbolEntry]) -> list[SymbolEntry]:
    unique_entries = sort_entries(set(entries))
    demangled = demangle_names([entry.name for entry in unique_entries])
    selected = [
        entry
        for entry in unique_entries
        if is_project_symbol(entry.name, demangled.get(entry.name, entry.name))
    ]
    return sort_entries(set(selected))


def manifest_header() -> str:
    return "\n".join(
        [
            "# amflow public C++ symbol surface manifest",
            f"# schema: {MANIFEST_SCHEMA}",
            "# target: amflow",
            "# entry-format: <nm-kind> <mangled-symbol-name>",
            (
                "# extraction: defined external symbols from nm, filtered to top-level "
                "amflow C++ symbols"
            ),
            (
                "# note: initial gate pin; this tooling change does not add or remove "
                "amflow library symbols"
            ),
        ]
    )


def load_manifest(path: Path) -> list[SymbolEntry]:
    try:
        lines = path.read_text(encoding="utf-8").splitlines()
    except OSError as exc:
        raise VerificationError(f"cannot read manifest {path}: {exc}") from exc

    entries: list[SymbolEntry] = []
    for index, raw_line in enumerate(lines, start=1):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split()
        if len(parts) != 2 or len(parts[0]) != 1:
            raise VerificationError(f"{path}:{index}: invalid symbol manifest entry")
        entries.append(SymbolEntry(kind=parts[0], name=parts[1]))

    canonical = sort_entries(set(entries))
    if entries != canonical:
        raise VerificationError(f"{path} must contain sorted unique symbol entries")
    if not entries:
        raise VerificationError(f"{path} must contain at least one symbol entry")
    return entries


def write_manifest(path: Path, entries: list[SymbolEntry]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    body = "\n".join(entry.line() for entry in sort_entries(set(entries)))
    path.write_text(f"{manifest_header()}\n{body}\n", encoding="utf-8")


def diff_message(expected: list[SymbolEntry], actual: list[SymbolEntry]) -> str | None:
    expected_set = set(expected)
    actual_set = set(actual)
    if expected_set == actual_set:
        return None

    removed = sort_entries(expected_set - actual_set)
    added = sort_entries(actual_set - expected_set)
    lines = [
        "public symbol surface changed; update the manifest only with explicit ABI review"
    ]
    if removed:
        lines.append("removed symbols:")
        lines.extend(f"  - {entry.line()}" for entry in removed[:40])
        if len(removed) > 40:
            lines.append(f"  ... {len(removed) - 40} more removed symbols")
    if added:
        lines.append("added symbols:")
        lines.extend(f"  + {entry.line()}" for entry in added[:40])
        if len(added) > 40:
            lines.append(f"  ... {len(added) - 40} more added symbols")
    return "\n".join(lines)


def verify_surface(target: Path, manifest: Path) -> int:
    if not target.is_file():
        raise VerificationError(f"symbol target is missing: {target}")
    raw_entries, extraction = nm_defined_symbols(target)
    actual = project_symbol_surface(raw_entries)
    expected = load_manifest(manifest)
    message = diff_message(expected, actual)
    if message is not None:
        raise VerificationError(message)
    print(
        f"public symbol surface verified: {len(actual)} symbols "
        f"({target.name}, {extraction})"
    )
    return 0


def write_current_surface(target: Path, manifest: Path) -> int:
    if not target.is_file():
        raise VerificationError(f"symbol target is missing: {target}")
    raw_entries, extraction = nm_defined_symbols(target)
    actual = project_symbol_surface(raw_entries)
    write_manifest(manifest, actual)
    print(
        f"wrote public symbol manifest: {manifest} "
        f"({len(actual)} symbols, {target.name}, {extraction})"
    )
    return 0


def run_self_check() -> int:
    fake_entries = parse_nm_posix(
        "\n".join(
            [
                "_ZN6amflow3FooEv T 00000000 00000010",
                "_ZNK6amflow3Foo3BarEv T 00000000 00000010",
                "_ZTVN6amflow3FooE V 00000000 00000010",
                (
                    "_ZNSt6vectorIN6amflow3FooESaIS1_EE7reserveEm "
                    "W 00000000 00000010"
                ),
            ]
        )
    )
    selected = project_symbol_surface(fake_entries)
    selected_names = {entry.name for entry in selected}
    if "_ZN6amflow3FooEv" not in selected_names:
        raise VerificationError("self-check failed to include namespace function")
    if "_ZNK6amflow3Foo3BarEv" not in selected_names:
        raise VerificationError("self-check failed to include const member function")
    if "_ZTVN6amflow3FooE" not in selected_names:
        raise VerificationError("self-check failed to include vtable symbol")
    if "_ZNSt6vectorIN6amflow3FooESaIS1_EE7reserveEm" in selected_names:
        raise VerificationError("self-check failed to exclude std template instantiation")

    expected = [SymbolEntry("T", "_ZN6amflow3FooEv")]
    actual = [
        SymbolEntry("T", "_ZN6amflow3FooEv"),
        SymbolEntry("T", "_ZN6amflow3NewEv"),
    ]
    message = diff_message(expected, actual)
    if message is None or "_ZN6amflow3NewEv" not in message:
        raise VerificationError("self-check failed to report added symbols")

    print("public symbol surface verifier self-check passed")
    return 0


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--target", type=Path, help="Library or executable to inspect.")
    parser.add_argument(
        "--manifest",
        type=Path,
        default=DEFAULT_MANIFEST,
        help="Pinned symbol manifest path.",
    )
    parser.add_argument(
        "--write-manifest",
        action="store_true",
        help="Write the current target surface to the manifest path.",
    )
    parser.add_argument(
        "--self-check",
        action="store_true",
        help="Run parser and comparison self-checks.",
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    root = repo_root()

    try:
        if args.self_check:
            return run_self_check()
        if args.target is None:
            raise VerificationError("--target is required unless --self-check is used")
        target = resolve_path(root, args.target)
        manifest = resolve_path(root, args.manifest)
        if args.write_manifest:
            return write_current_surface(target, manifest)
        return verify_surface(target, manifest)
    except VerificationError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
