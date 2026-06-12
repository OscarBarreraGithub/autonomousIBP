#!/usr/bin/env python3
"""Validate the pinned upstream AMFlow example inventory registry."""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Any


REGISTRY_PATH = Path(
    "tools/reference-harness/specs/release/amflow-upstream-example-inventory.registry.json"
)
ENTRY_SUFFIXES = (".wl", ".nb")


class InventoryError(RuntimeError):
    """Raised when the AMFlow example inventory registry is inconsistent."""


@dataclass(frozen=True)
class EntryHash:
    path: str
    sha256: str


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise InventoryError(message)


def require_mapping(value: Any, label: str) -> dict[str, Any]:
    expect(isinstance(value, dict), f"{label} must be an object")
    return value


def require_list(value: Any, label: str) -> list[Any]:
    expect(isinstance(value, list), f"{label} must be a list")
    return value


def require_string(value: Any, label: str) -> str:
    expect(isinstance(value, str), f"{label} must be a string")
    expect(value.strip() == value, f"{label} must not contain surrounding whitespace")
    expect(value != "", f"{label} must not be empty")
    return value


def load_json(path: Path) -> dict[str, Any]:
    try:
        payload = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as exc:
        raise InventoryError(f"{path} is not valid JSON: {exc}") from exc
    return require_mapping(payload, str(path))


def resolve_registry_path(root: Path, path: Path | None) -> Path:
    if path is None:
        return root / REGISTRY_PATH
    if path.is_absolute():
        return path
    return root / path


def digest_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def digest_file(path: Path) -> str:
    return digest_bytes(path.read_bytes())


def digest_path_set(paths: list[str]) -> str:
    return digest_bytes(("".join(f"{path}\n" for path in paths)).encode("utf-8"))


def digest_manifest(entries: list[EntryHash]) -> str:
    text = "".join(f"{entry.path}\t{entry.sha256}\n" for entry in entries)
    return digest_bytes(text.encode("utf-8"))


def run_git(root: Path, args: list[str], *, input_text: str | None = None) -> str:
    result = subprocess.run(
        ["git", "-C", str(root), *args],
        check=False,
        input=input_text,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )
    if result.returncode != 0:
        raise InventoryError(
            f"git {' '.join(args)} failed in {root}: {result.stderr.strip()}"
        )
    return result.stdout


def git_blob(root: Path, ref: str, path: str) -> bytes:
    result = subprocess.run(
        ["git", "-C", str(root), "show", f"{ref}:{path}"],
        check=False,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if result.returncode != 0:
        raise InventoryError(
            f"git show {ref}:{path} failed in {root}: "
            f"{result.stderr.decode('utf-8', errors='replace').strip()}"
        )
    return result.stdout


def entry_files_from_fs(amflow_root: Path) -> list[EntryHash]:
    examples_root = amflow_root / "examples"
    expect(examples_root.is_dir(), f"missing examples directory: {examples_root}")
    entries: list[EntryHash] = []
    for path in sorted(examples_root.rglob("*")):
        if not path.is_file() or path.suffix not in ENTRY_SUFFIXES:
            continue
        relative_path = path.relative_to(examples_root).as_posix()
        entries.append(EntryHash(path=relative_path, sha256=digest_file(path)))
    return entries


def entry_files_from_git_ref(amflow_root: Path, ref: str) -> list[EntryHash]:
    listing = run_git(amflow_root, ["ls-tree", "-r", "--name-only", ref, "examples"])
    paths = sorted(
        path
        for path in listing.splitlines()
        if path.endswith(ENTRY_SUFFIXES) and path.startswith("examples/")
    )
    entries: list[EntryHash] = []
    for path in paths:
        entries.append(
            EntryHash(
                path=path.removeprefix("examples/"),
                sha256=digest_bytes(git_blob(amflow_root, ref, path)),
            )
        )
    return entries


def parse_registry_entries(value: Any, label: str) -> list[EntryHash]:
    entries: list[EntryHash] = []
    seen_paths: set[str] = set()
    for index, item in enumerate(require_list(value, label)):
        item_label = f"{label}[{index}]"
        item_map = require_mapping(item, item_label)
        path = require_string(item_map.get("path"), f"{item_label}.path")
        sha256 = require_string(item_map.get("sha256"), f"{item_label}.sha256")
        expect(path not in seen_paths, f"{item_label}.path duplicates {path}")
        expect(len(sha256) == 64, f"{item_label}.sha256 must be a SHA-256 hex digest")
        seen_paths.add(path)
        entries.append(EntryHash(path=path, sha256=sha256))
    return sorted(entries, key=lambda entry: entry.path)


def validate_entry_inventory(
    entries: list[EntryHash],
    *,
    expected_path_digest: str,
    expected_manifest_digest: str,
    label: str,
) -> None:
    paths = [entry.path for entry in entries]
    expect(digest_path_set(paths) == expected_path_digest, f"{label} path-set digest drifted")
    expect(
        digest_manifest(entries) == expected_manifest_digest,
        f"{label} file-manifest digest drifted",
    )


def validate_root(root: Path, registry_path: Path) -> dict[str, Any]:
    payload = load_json(registry_path)
    expect(payload.get("schema_version") == 1, "schema_version must be 1")
    expect(
        payload.get("registry_kind") == "amflow_upstream_example_inventory",
        "registry_kind must be amflow_upstream_example_inventory",
    )

    decision = require_mapping(payload.get("decision"), "decision")
    expect(
        decision.get("coverage_complete_for_stock_upstream_examples") is True,
        "decision.coverage_complete_for_stock_upstream_examples must be true",
    )
    expect(
        require_list(decision.get("missed_upstream_example_dirs"), "missed_upstream_example_dirs")
        == [],
        "missed_upstream_example_dirs must be empty for a complete inventory",
    )

    checkout = require_mapping(payload.get("canonical_cluster_checkout"), "canonical_cluster_checkout")
    checkout_path = Path(require_string(checkout.get("path"), "canonical_cluster_checkout.path"))
    if not checkout_path.is_absolute():
        checkout_path = root / checkout_path
    expect(checkout_path.is_dir(), f"canonical checkout is missing: {checkout_path}")

    package_file = checkout_path / require_string(
        checkout.get("package_file"), "canonical_cluster_checkout.package_file"
    )
    expect(package_file.is_file(), f"package file is missing: {package_file}")
    expected_package_sha = require_string(
        checkout.get("package_sha256"), "canonical_cluster_checkout.package_sha256"
    )
    expect(digest_file(package_file) == expected_package_sha, "canonical AMFlow.m hash drifted")

    expected_path_digest = require_string(
        checkout.get("entry_path_set_sha256"),
        "canonical_cluster_checkout.entry_path_set_sha256",
    )
    expected_manifest_digest = require_string(
        checkout.get("entry_file_manifest_sha256"),
        "canonical_cluster_checkout.entry_file_manifest_sha256",
    )
    fs_entries = entry_files_from_fs(checkout_path)
    validate_entry_inventory(
        fs_entries,
        expected_path_digest=expected_path_digest,
        expected_manifest_digest=expected_manifest_digest,
        label="canonical checkout filesystem inventory",
    )

    registry_entries = parse_registry_entries(
        payload.get("installed_ref_entry_files"), "installed_ref_entry_files"
    )
    expect(registry_entries == fs_entries, "installed_ref_entry_files do not match filesystem hashes")

    stock_dirs = [
        require_string(item, f"stock_example_dirs[{index}]")
        for index, item in enumerate(require_list(payload.get("stock_example_dirs"), "stock_example_dirs"))
    ]
    expect(len(stock_dirs) == len(set(stock_dirs)), "stock_example_dirs must be unique")
    expect(
        len(stock_dirs) == decision.get("covered_stock_example_dir_count"),
        "covered_stock_example_dir_count does not match stock_example_dirs",
    )
    run_dirs = sorted(
        entry.path.removesuffix("/run.wl") for entry in fs_entries if entry.path.endswith("/run.wl")
    )
    expect(sorted(stock_dirs) == run_dirs, "stock_example_dirs do not match run.wl directories")
    expect(
        len(fs_entries) == decision.get("stock_entry_file_count"),
        "stock_entry_file_count does not match .wl/.nb inventory",
    )
    notebook_count = sum(1 for entry in fs_entries if entry.path.endswith(".nb"))
    expect(
        notebook_count == decision.get("stock_notebook_count"),
        "stock_notebook_count does not match .nb inventory",
    )

    git_tree = run_git(checkout_path, ["rev-parse", "HEAD:examples"]).strip()
    expected_tree = require_string(
        checkout.get("examples_tree_git_object"),
        "canonical_cluster_checkout.examples_tree_git_object",
    )
    expect(git_tree == expected_tree, "canonical examples Git tree object drifted")

    for index, ref_payload in enumerate(
        require_list(payload.get("same_path_cross_check_refs"), "same_path_cross_check_refs")
    ):
        ref_label = f"same_path_cross_check_refs[{index}]"
        ref_map = require_mapping(ref_payload, ref_label)
        ref = require_string(ref_map.get("ref"), f"{ref_label}.ref")
        resolved_commit = run_git(checkout_path, ["rev-parse", f"{ref}^{{commit}}"]).strip()
        expect(
            resolved_commit == require_string(ref_map.get("resolved_commit"), f"{ref_label}.resolved_commit"),
            f"{ref_label}.resolved_commit drifted",
        )
        tag_object = ref_map.get("annotated_tag_object")
        if tag_object is not None:
            expect(
                run_git(checkout_path, ["rev-parse", f"{ref}^{{tag}}"]).strip()
                == require_string(tag_object, f"{ref_label}.annotated_tag_object"),
                f"{ref_label}.annotated_tag_object drifted",
            )
        ref_package_sha = digest_bytes(git_blob(checkout_path, ref, "AMFlow.m"))
        expect(
            ref_package_sha == require_string(ref_map.get("package_sha256"), f"{ref_label}.package_sha256"),
            f"{ref_label}.package_sha256 drifted",
        )
        ref_entries = entry_files_from_git_ref(checkout_path, ref)
        validate_entry_inventory(
            ref_entries,
            expected_path_digest=require_string(
                ref_map.get("entry_path_set_sha256"),
                f"{ref_label}.entry_path_set_sha256",
            ),
            expected_manifest_digest=require_string(
                ref_map.get("entry_file_manifest_sha256"),
                f"{ref_label}.entry_file_manifest_sha256",
            ),
            label=f"{ref_label} inventory",
        )
        expect(
            digest_path_set([entry.path for entry in ref_entries]) == expected_path_digest,
            f"{ref_label} path set no longer matches canonical inventory",
        )

    return {
        "status": "pass",
        "entry_file_count": len(fs_entries),
        "stock_example_dir_count": len(stock_dirs),
        "notebook_count": notebook_count,
        "entry_path_set_sha256": expected_path_digest,
        "entry_file_manifest_sha256": expected_manifest_digest,
    }


def write_file(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def init_fixture_repo(root: Path) -> dict[str, Any]:
    upstream = root / "upstream"
    write_file(upstream / "AMFlow.m", "$PackageInfo = {\"test\", \"today\"};\n")
    write_file(upstream / "examples" / "alpha" / "run.wl", "Print[alpha]\n")
    write_file(upstream / "examples" / "beta" / "run.wl", "Print[beta]\n")
    write_file(upstream / "examples" / "beta" / "diffeq.wl", "Print[diffeq]\n")
    run_git(upstream, ["init"])
    run_git(upstream, ["config", "user.email", "test@example.invalid"])
    run_git(upstream, ["config", "user.name", "Example Test"])
    run_git(upstream, ["add", "."])
    run_git(upstream, ["commit", "-m", "fixture"])
    run_git(upstream, ["tag", "-a", "fixture-1", "-m", "fixture-1"])

    entries = entry_files_from_fs(upstream)
    registry = {
        "schema_version": 1,
        "registry_kind": "amflow_upstream_example_inventory",
        "decision": {
            "coverage_complete_for_stock_upstream_examples": True,
            "missed_upstream_example_dirs": [],
            "covered_stock_example_dir_count": 2,
            "stock_entry_file_count": 3,
            "stock_notebook_count": 0,
        },
        "canonical_cluster_checkout": {
            "path": str(upstream),
            "package_file": "AMFlow.m",
            "package_sha256": digest_file(upstream / "AMFlow.m"),
            "examples_tree_git_object": run_git(upstream, ["rev-parse", "HEAD:examples"]).strip(),
            "entry_path_set_sha256": digest_path_set([entry.path for entry in entries]),
            "entry_file_manifest_sha256": digest_manifest(entries),
        },
        "same_path_cross_check_refs": [
            {
                "ref": "fixture-1",
                "annotated_tag_object": run_git(upstream, ["rev-parse", "fixture-1^{tag}"]).strip(),
                "resolved_commit": run_git(upstream, ["rev-parse", "fixture-1^{commit}"]).strip(),
                "package_sha256": digest_bytes(git_blob(upstream, "fixture-1", "AMFlow.m")),
                "entry_path_set_sha256": digest_path_set([entry.path for entry in entries]),
                "entry_file_manifest_sha256": digest_manifest(entries),
            }
        ],
        "stock_example_dirs": ["alpha", "beta"],
        "installed_ref_entry_files": [
            {"path": entry.path, "sha256": entry.sha256} for entry in entries
        ],
    }
    return registry


def expect_failure(root: Path, registry_path: Path, expected_substring: str) -> None:
    try:
        validate_root(root, registry_path)
    except InventoryError as exc:
        expect(
            expected_substring in str(exc),
            f"expected failure containing {expected_substring!r}, got {exc}",
        )
        return
    raise InventoryError(f"expected validation failure containing {expected_substring!r}")


def self_check() -> None:
    with tempfile.TemporaryDirectory(prefix="amflow-example-inventory-") as tmp_text:
        tmp = Path(tmp_text)
        registry = init_fixture_repo(tmp)
        registry_path = tmp / "registry.json"
        registry_path.write_text(json.dumps(registry, indent=2) + "\n", encoding="utf-8")

        result = validate_root(tmp, registry_path)
        expect(result["entry_file_count"] == 3, "positive self-check returned wrong count")

        upstream = Path(registry["canonical_cluster_checkout"]["path"])
        write_file(upstream / "examples" / "gamma" / "run.wl", "Print[gamma]\n")
        expect_failure(tmp, registry_path, "path-set digest drifted")

        write_file(upstream / "examples" / "gamma" / "run.wl", "")
        registry["decision"]["missed_upstream_example_dirs"] = ["gamma"]
        registry_path.write_text(json.dumps(registry, indent=2) + "\n", encoding="utf-8")
        expect_failure(tmp, registry_path, "missed_upstream_example_dirs must be empty")


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Validate the pinned upstream AMFlow example inventory registry."
    )
    parser.add_argument("--verify", action="store_true", help="validate the committed registry")
    parser.add_argument("--self-check", action="store_true", help="run synthetic self-checks")
    parser.add_argument("--registry", type=Path, help="override the registry path")
    args = parser.parse_args()

    if args.self_check:
        self_check()
        return 0

    root = repo_root()
    registry_path = resolve_registry_path(root, args.registry)
    result = validate_root(root, registry_path)
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except InventoryError as exc:
        print(f"error: {exc}", file=sys.stderr)
        raise SystemExit(1)
