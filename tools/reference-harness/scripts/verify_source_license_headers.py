#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-autoIBP-TBD
"""CTest gate for source license and copyright header coverage."""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path


DEFAULT_BASELINE = Path(
    "tools/reference-harness/specs/license/source-license-header-baseline.txt"
)
BASELINE_SCHEMA_LINE = "# schema: amflow-source-license-header-baseline-v1"
HEADER_SCAN_LINES = 40
CXX_SOURCE_SUFFIXES = {
    ".c",
    ".cc",
    ".cpp",
    ".cxx",
    ".h",
    ".hh",
    ".hpp",
    ".hxx",
    ".ipp",
    ".ixx",
}
CXX_SOURCE_ROOTS = {"lib", "include", "src", "tools"}
ROOT_LICENSE_FILENAMES = {
    "license",
    "license.md",
    "license.txt",
    "copying",
    "copying.md",
    "copying.txt",
    "notice",
    "notice.md",
    "notice.txt",
}
HEADER_PATTERNS = (
    re.compile(r"\bSPDX-License-Identifier\s*:"),
    re.compile(r"\bCopyright\b", re.IGNORECASE),
    re.compile(r"\bLicensed under\b", re.IGNORECASE),
    re.compile(r"\bDistributed under\b", re.IGNORECASE),
)


class LicenseHeaderError(RuntimeError):
    """Raised when source license header coverage differs from the pin."""


@dataclass(frozen=True)
class HeaderAudit:
    scoped_paths: list[str]
    header_paths: list[str]
    missing_header_paths: list[str]
    root_license_files: list[str]


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def resolve_path(root: Path, path: Path) -> Path:
    return path if path.is_absolute() else root / path


def run_git(root: Path, args: list[str]) -> bytes:
    result = subprocess.run(
        ["git", "-C", str(root), *args],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if result.returncode != 0:
        raise LicenseHeaderError(
            f"git {' '.join(args)} failed in {root}: "
            f"{result.stderr.decode('utf-8', errors='replace').strip()}"
        )
    return result.stdout


def tracked_files(root: Path) -> list[str]:
    output = run_git(root, ["ls-files", "-z"])
    return sorted(
        item.decode("utf-8")
        for item in output.split(b"\0")
        if item
    )


def is_scoped_source(path_text: str) -> bool:
    path = Path(path_text)
    parts = path.parts
    if not parts:
        return False
    if path.name == "CMakeLists.txt" or path.suffix == ".cmake":
        return True
    if parts[0] in CXX_SOURCE_ROOTS and path.suffix.lower() in CXX_SOURCE_SUFFIXES:
        return True
    return parts[0] == "tools" and path.suffix == ".py"


def expected_header_present(path: Path) -> bool:
    try:
        lines = path.read_text(encoding="utf-8", errors="replace").splitlines()
    except OSError as exc:
        raise LicenseHeaderError(f"cannot read scoped source file {path}: {exc}") from exc
    head = "\n".join(lines[:HEADER_SCAN_LINES])
    return any(pattern.search(head) for pattern in HEADER_PATTERNS)


def root_license_files(root: Path) -> list[str]:
    return sorted(
        path.name
        for path in root.iterdir()
        if path.is_file() and path.name.lower() in ROOT_LICENSE_FILENAMES
    )


def load_missing_header_baseline(path: Path) -> set[str]:
    try:
        lines = path.read_text(encoding="utf-8").splitlines()
    except OSError as exc:
        raise LicenseHeaderError(f"cannot read missing-header baseline {path}: {exc}") from exc

    if not any(line.strip() == BASELINE_SCHEMA_LINE for line in lines[:12]):
        raise LicenseHeaderError(
            f"{path} is missing required schema line: {BASELINE_SCHEMA_LINE}"
        )

    entries: set[str] = set()
    for line_number, raw_line in enumerate(lines, start=1):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        if raw_line != line:
            raise LicenseHeaderError(
                f"{path}:{line_number} baseline paths must not have surrounding whitespace"
            )
        parsed = Path(line)
        if parsed.is_absolute() or ".." in parsed.parts:
            raise LicenseHeaderError(
                f"{path}:{line_number} baseline path must be repo-relative: {line}"
            )
        if line in entries:
            raise LicenseHeaderError(
                f"{path}:{line_number} duplicate missing-header baseline path: {line}"
            )
        entries.add(line)
    return entries


def audit_paths(root: Path, source_paths: list[str]) -> HeaderAudit:
    scoped_paths = sorted(path for path in source_paths if is_scoped_source(path))
    header_paths: list[str] = []
    missing_header_paths: list[str] = []
    for path_text in scoped_paths:
        if expected_header_present(root / path_text):
            header_paths.append(path_text)
        else:
            missing_header_paths.append(path_text)
    return HeaderAudit(
        scoped_paths=scoped_paths,
        header_paths=header_paths,
        missing_header_paths=missing_header_paths,
        root_license_files=root_license_files(root),
    )


def format_path_list(title: str, paths: list[str]) -> str:
    if not paths:
        return ""
    rendered = "\n".join(f"  - {path}" for path in paths)
    return f"{title}\n{rendered}"


def verify(root: Path, baseline_path: Path) -> HeaderAudit:
    baseline = load_missing_header_baseline(baseline_path)
    audit = audit_paths(root, tracked_files(root))
    scoped = set(audit.scoped_paths)
    missing = set(audit.missing_header_paths)
    with_header = set(audit.header_paths)

    orphaned_baseline = sorted(baseline - scoped)
    stale_baseline = sorted(baseline & with_header)
    unpinned_missing = sorted(missing - baseline)

    errors = [
        format_path_list(
            "Scoped files missing the expected header and absent from the baseline:",
            unpinned_missing,
        ),
        format_path_list(
            "Baseline entries that now have an expected header; remove them from the pin:",
            stale_baseline,
        ),
        format_path_list(
            "Baseline entries that are no longer tracked scoped files:",
            orphaned_baseline,
        ),
    ]
    errors = [error for error in errors if error]
    if errors:
        posture = (
            "Root license files: " + ", ".join(audit.root_license_files)
            if audit.root_license_files
            else "Root license files: none found; this remains a documented known gap."
        )
        raise LicenseHeaderError(
            "\n\n".join(errors)
            + "\n\n"
            + posture
            + "\nExpected header markers: SPDX-License-Identifier, Copyright, "
            "Licensed under, or Distributed under in the first "
            f"{HEADER_SCAN_LINES} lines."
        )
    return audit


def self_check() -> None:
    with tempfile.TemporaryDirectory() as tmpdir:
        root = Path(tmpdir)
        (root / "tools").mkdir()
        missing = root / "tools" / "new_tool.py"
        missing.write_text(
            "#!/usr/bin/env python3\n"
            '"""A new Python tool without license metadata."""\n',
            encoding="utf-8",
        )
        spdx = root / "tools" / "new_tool_with_header.py"
        spdx.write_text(
            "#!/usr/bin/env python3\n"
            "# SPDX-License-Identifier: LicenseRef-PROJECT-TBD\n",
            encoding="utf-8",
        )
        audit = audit_paths(
            root,
            ["tools/new_tool.py", "tools/new_tool_with_header.py"],
        )
        if audit.missing_header_paths != ["tools/new_tool.py"]:
            raise LicenseHeaderError(
                "self-check failed to classify the unheadered new Python tool"
            )
        if audit.header_paths != ["tools/new_tool_with_header.py"]:
            raise LicenseHeaderError(
                "self-check failed to accept the SPDX-marked Python tool"
            )


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--baseline",
        type=Path,
        default=DEFAULT_BASELINE,
        help="Repo-relative or absolute missing-header baseline path.",
    )
    parser.add_argument(
        "--self-check",
        action="store_true",
        help="Run the verifier's internal classification check.",
    )
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    try:
        if args.self_check:
            self_check()
            print("source license header verifier self-check passed")
            return 0

        root = repo_root()
        baseline_path = resolve_path(root, args.baseline)
        audit = verify(root, baseline_path)
        posture = (
            "root license files: " + ", ".join(audit.root_license_files)
            if audit.root_license_files
            else "root license files: none found; documented known gap"
        )
        print(
            "source license header gate passed: "
            f"{len(audit.scoped_paths)} scoped files, "
            f"{len(audit.header_paths)} with expected headers, "
            f"{len(audit.missing_header_paths)} pinned missing-header files; "
            f"{posture}"
        )
    except LicenseHeaderError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
