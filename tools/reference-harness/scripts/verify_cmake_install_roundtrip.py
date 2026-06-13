#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-autoIBP-TBD
"""CTest gate for CMake install/uninstall inventory integrity."""

from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path


def fail(message: str) -> None:
    raise SystemExit(message)


def run(command: list[str]) -> None:
    completed = subprocess.run(
        command,
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if completed.returncode == 0:
        return
    print("$ " + " ".join(command), file=sys.stderr)
    if completed.stdout:
        print(completed.stdout, file=sys.stderr, end="")
    if completed.stderr:
        print(completed.stderr, file=sys.stderr, end="")
    fail(f"command failed with exit code {completed.returncode}")


def is_relative_to(path: Path, parent: Path) -> bool:
    try:
        path.relative_to(parent)
    except ValueError:
        return False
    return True


def normal_path(path: Path) -> Path:
    return Path(os.path.abspath(path))


def file_inventory(root: Path) -> set[Path]:
    if not root.exists():
        return set()
    return {
        normal_path(path)
        for path in root.rglob("*")
        if path.is_file() or path.is_symlink()
    }


def relative_inventory(paths: set[Path], prefix: Path) -> set[Path]:
    return {path.relative_to(prefix) for path in paths}


def parse_manifest(manifest: Path) -> set[Path]:
    if not manifest.exists():
        fail(f"install manifest was not written: {manifest}")
    entries = {
        normal_path(Path(line))
        for line in manifest.read_text(encoding="utf-8").splitlines()
        if line.strip()
    }
    if not entries:
        fail(f"install manifest is empty: {manifest}")
    return entries


def expected_inventory(args: argparse.Namespace, prefix: Path) -> set[Path]:
    bindir = Path(args.bindir)
    libdir = Path(args.libdir)
    includedir = Path(args.includedir)
    for name, install_dir in (
        ("bindir", bindir),
        ("libdir", libdir),
        ("includedir", includedir),
    ):
        if install_dir.is_absolute():
            fail(f"{name} must be relative to the install prefix: {install_dir}")

    source_include = Path(args.source_dir).resolve() / "include"
    headers = {
        includedir / header.relative_to(source_include)
        for header in source_include.rglob("*.hpp")
    }
    if not headers:
        fail(f"no public headers found under {source_include}")

    package_dir = libdir / "cmake" / "amflow"
    export_fragments = sorted((prefix / package_dir).glob("amflowTargets-*.cmake"))
    if len(export_fragments) != 1:
        found = ", ".join(str(path.relative_to(prefix)) for path in export_fragments)
        fail(f"expected one amflowTargets-*.cmake export fragment, found: {found}")

    expected = set(headers)
    expected.update(
        {
            bindir / args.cli_filename,
            libdir / args.library_filename,
            package_dir / "amflowConfig.cmake",
            package_dir / "amflowConfigVersion.cmake",
            package_dir / "amflowTargets.cmake",
            export_fragments[0].relative_to(prefix),
        }
    )
    return expected


def format_paths(paths: set[Path]) -> str:
    return "\n".join(f"  {path.as_posix()}" for path in sorted(paths))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--cmake", required=True)
    parser.add_argument("--build-dir", required=True)
    parser.add_argument("--source-dir", required=True)
    parser.add_argument("--install-root", required=True)
    parser.add_argument("--bindir", required=True)
    parser.add_argument("--libdir", required=True)
    parser.add_argument("--includedir", required=True)
    parser.add_argument("--library-filename", required=True)
    parser.add_argument("--cli-filename", required=True)
    parser.add_argument("--config", default="")
    args = parser.parse_args()

    build_dir = normal_path(Path(args.build_dir))
    install_root = normal_path(Path(args.install_root))
    prefix = install_root / "prefix"
    manifest = build_dir / "install_manifest.txt"

    if install_root.exists():
        shutil.rmtree(install_root)
    install_root.mkdir(parents=True)
    if manifest.exists():
        manifest.unlink()

    install_command = [args.cmake, "--install", str(build_dir), "--prefix", str(prefix)]
    if args.config:
        install_command.extend(["--config", args.config])
    run(install_command)

    root_children = {normal_path(path) for path in install_root.iterdir()}
    if root_children != {prefix}:
        unexpected = root_children - {prefix}
        missing = {prefix} - root_children
        details = []
        if unexpected:
            details.append(
                "install wrote entries outside the requested prefix:\n"
                + format_paths({path.relative_to(install_root) for path in unexpected})
            )
        if missing:
            details.append("install did not create the requested prefix")
        fail(
            "\n".join(details)
        )

    manifest_entries = parse_manifest(manifest)
    outside_prefix = {
        path for path in manifest_entries if not is_relative_to(path, prefix)
    }
    if outside_prefix:
        fail("install manifest contains paths outside prefix:\n" + format_paths(outside_prefix))

    installed_files = file_inventory(prefix)
    missing_from_tree = manifest_entries - installed_files
    missing_from_manifest = installed_files - manifest_entries
    if missing_from_tree or missing_from_manifest:
        details = []
        if missing_from_tree:
            details.append("manifest entries missing from install tree:\n" + format_paths(missing_from_tree))
        if missing_from_manifest:
            details.append("installed files missing from manifest:\n" + format_paths(missing_from_manifest))
        fail("\n".join(details))

    actual_relative = relative_inventory(installed_files, prefix)
    expected_relative = expected_inventory(args, prefix)
    unexpected_files = actual_relative - expected_relative
    missing_files = expected_relative - actual_relative
    if unexpected_files or missing_files:
        details = []
        if unexpected_files:
            details.append("unexpected installed files:\n" + format_paths(unexpected_files))
        if missing_files:
            details.append("expected installed files were not installed:\n" + format_paths(missing_files))
        fail("\n".join(details))

    uninstall_command = [args.cmake, "--build", str(build_dir), "--target", "uninstall"]
    if args.config:
        uninstall_command.extend(["--config", args.config])
    run(uninstall_command)

    if prefix.exists():
        leftovers = {path.relative_to(prefix) for path in prefix.rglob("*")}
        if leftovers:
            fail("uninstall left files or directories under prefix:\n" + format_paths(leftovers))
        fail(f"uninstall left the empty prefix directory behind: {prefix}")

    remaining_root_entries = {path.relative_to(install_root) for path in install_root.rglob("*")}
    if remaining_root_entries:
        fail("uninstall left scratch entries behind:\n" + format_paths(remaining_root_entries))

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
