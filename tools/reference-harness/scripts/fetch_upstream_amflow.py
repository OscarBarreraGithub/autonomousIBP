#!/usr/bin/env python3
"""Fetch or inspect upstream AMFlow inputs for the reference harness."""

from __future__ import annotations

import argparse
import hashlib
import io
import json
import shutil
import subprocess
import tarfile
import tempfile
import urllib.parse
import urllib.request
import zipfile
from pathlib import Path
from typing import Any


def ensure_dir(path: Path, dry_run: bool) -> None:
    if dry_run:
        return
    path.mkdir(parents=True, exist_ok=True)


def write_json(path: Path, payload: dict[str, Any], dry_run: bool) -> None:
    if dry_run:
        return
    path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def run_checked(command: list[str]) -> None:
    subprocess.run(command, check=True)


def command_succeeds(command: list[str]) -> bool:
    return subprocess.run(command, check=False, capture_output=True).returncode == 0


def read_stdout(command: list[str]) -> str:
    completed = subprocess.run(command, check=True, capture_output=True, text=True)
    return completed.stdout.strip()


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def validate_archive_target(member_name: str, destination: Path) -> Path:
    resolved_destination = destination.resolve()
    if member_name.startswith(("/", "\\")):
        raise RuntimeError(f"refusing to extract archive member with absolute path: {member_name}")
    target = (destination / member_name).resolve()
    if resolved_destination not in target.parents and target != resolved_destination:
        raise RuntimeError(f"refusing to extract archive member outside destination: {member_name}")
    return target


def safe_extract_tar(archive: tarfile.TarFile, destination: Path) -> None:
    validated_targets: list[tuple[tarfile.TarInfo, Path]] = []
    for member in archive.getmembers():
        target = validate_archive_target(member.name, destination)
        if member.issym():
            raise RuntimeError(f"refusing to extract symlink tar member: {member.name}")
        if member.islnk():
            raise RuntimeError(f"refusing to extract hardlink tar member: {member.name}")
        if member.ischr() or member.isblk() or member.isfifo():
            raise RuntimeError(f"refusing to extract special-device tar member: {member.name}")
        if member.isdir():
            continue
        if not member.isreg():
            raise RuntimeError(f"refusing to extract unsupported tar member type: {member.name}")
        validated_targets.append((member, target))

    for member, target in validated_targets:
        target.parent.mkdir(parents=True, exist_ok=True)
        source = archive.extractfile(member)
        if source is None:
            raise RuntimeError(f"failed to read regular tar member: {member.name}")
        with source, target.open("wb") as handle:
            shutil.copyfileobj(source, handle)

    for member in archive.getmembers():
        if member.isdir():
            validate_archive_target(member.name, destination).mkdir(parents=True, exist_ok=True)


def safe_extract_zip(archive: zipfile.ZipFile, destination: Path) -> None:
    for member in archive.namelist():
        validate_archive_target(member, destination)
    archive.extractall(destination)


def recreate_directory(path: Path, dry_run: bool) -> None:
    if dry_run:
        return
    if path.exists():
        if path.is_dir():
            shutil.rmtree(path)
        else:
            path.unlink()
    path.mkdir(parents=True, exist_ok=True)


def extract_archive(archive_path: Path, destination: Path, dry_run: bool) -> dict[str, Any]:
    extraction = {
        "archive_path": str(archive_path),
        "extracted_path": str(destination),
        "archive_kind": "unknown",
    }
    if dry_run:
        extraction["status"] = "planned"
        return extraction

    recreate_directory(destination, False)
    if zipfile.is_zipfile(archive_path):
        with zipfile.ZipFile(archive_path) as handle:
            safe_extract_zip(handle, destination)
        extraction["archive_kind"] = "zip"
    elif tarfile.is_tarfile(archive_path):
        with tarfile.open(archive_path) as handle:
            safe_extract_tar(handle, destination)
        extraction["archive_kind"] = "tar"
    else:
        extraction["status"] = "not-an-archive"
        return extraction

    extraction["status"] = "ready"
    return extraction


def archive_name_from_url(url: str) -> str:
    parsed = urllib.parse.urlparse(url)
    candidate = Path(parsed.path).name
    return candidate or "amflow-cpc-archive"


def canonicalize_git_url(url: str) -> str:
    candidate = url.strip().rstrip("/")
    if not candidate:
        return ""
    if "://" in candidate:
        parsed = urllib.parse.urlparse(candidate)
        host = (parsed.hostname or "").lower()
        path = parsed.path.rstrip("/")
        if path.endswith(".git"):
            path = path[:-4]
        return f"{host}{path}"
    if "@" in candidate and ":" in candidate.split("@", 1)[1]:
        host_part, path_part = candidate.split(":", 1)
        host = host_part.split("@", 1)[1].lower()
        path = "/" + path_part.rstrip("/")
        if path.endswith(".git"):
            path = path[:-4]
        return f"{host}{path}"
    if candidate.endswith(".git"):
        candidate = candidate[:-4]
    return candidate


def read_origin_remote_url(path: Path) -> str:
    return read_stdout(["git", "-C", str(path), "remote", "get-url", "origin"])


def verify_checkout_origin(path: Path, requested_url: str) -> str:
    remote_url = read_origin_remote_url(path)
    if canonicalize_git_url(remote_url) != canonicalize_git_url(requested_url):
        raise RuntimeError(
            "existing AMFlow checkout origin does not match requested URL: "
            f"found {remote_url!r}, expected {requested_url!r}"
        )
    return remote_url


def git_ref_exists(path: Path, ref: str) -> bool:
    return command_succeeds(
        ["git", "-C", str(path), "show-ref", "--verify", "--quiet", ref]
    )


def refresh_checkout(path: Path, requested_ref: str | None) -> str:
    run_checked(["git", "-C", str(path), "fetch", "--tags", "--prune", "origin"])
    if not requested_ref:
        return read_stdout(["git", "-C", str(path), "rev-parse", "HEAD"])

    command_succeeds(["git", "-C", str(path), "fetch", "--tags", "origin", requested_ref])
    remote_branch_ref = f"refs/remotes/origin/{requested_ref}"
    tag_ref = f"refs/tags/{requested_ref}"
    if git_ref_exists(path, remote_branch_ref):
        run_checked(["git", "-C", str(path), "checkout", "-B", requested_ref, f"origin/{requested_ref}"])
        return read_stdout(["git", "-C", str(path), "rev-parse", f"origin/{requested_ref}"])
    if git_ref_exists(path, tag_ref):
        run_checked(["git", "-C", str(path), "checkout", "--detach", tag_ref])
        return read_stdout(["git", "-C", str(path), "rev-parse", tag_ref])

    if not command_succeeds(["git", "-C", str(path), "rev-parse", "--verify", requested_ref]):
        raise RuntimeError(f"requested AMFlow ref could not be resolved after fetch: {requested_ref}")
    run_checked(["git", "-C", str(path), "checkout", "--detach", requested_ref])
    return read_stdout(["git", "-C", str(path), "rev-parse", requested_ref])


def inspect_existing_checkout(path: Path) -> dict[str, Any]:
    if not path.exists():
        raise FileNotFoundError(f"AMFlow path does not exist: {path}")

    metadata = {
        "kind": "local",
        "path": str(path.resolve()),
        "requested_ref": "",
        "resolved_commit": "",
        "status": "ready",
    }
    if (path / ".git").exists():
        metadata["kind"] = "git"
        metadata["resolved_commit"] = read_stdout(["git", "-C", str(path), "rev-parse", "HEAD"])
    return metadata


def clone_amflow_checkout(
    *,
    url: str,
    destination: Path,
    requested_ref: str | None,
    dry_run: bool,
) -> dict[str, Any]:
    metadata = {
        "kind": "git",
        "url": url,
        "path": str(destination.resolve()),
        "requested_ref": requested_ref or "",
        "resolved_commit": "",
        "origin_url": "",
        }
    if dry_run:
        metadata["status"] = "planned"
        return metadata

    ensure_dir(destination.parent, False)
    if destination.exists():
        if not (destination / ".git").exists():
            raise RuntimeError(f"destination exists but is not a git checkout: {destination}")
        metadata["origin_url"] = verify_checkout_origin(destination, url)
    else:
        run_checked(["git", "clone", url, str(destination)])
        metadata["origin_url"] = read_origin_remote_url(destination)

    metadata["resolved_commit"] = refresh_checkout(destination, requested_ref)
    metadata["status"] = "ready"
    return metadata


def materialize_cpc_archive(
    *,
    url: str | None,
    source_path: Path | None,
    archive_destination: Path,
    extraction_destination: Path,
    dry_run: bool,
) -> dict[str, Any]:
    metadata: dict[str, Any] = {
        "kind": "unset",
        "url": url or "",
        "source_path": str(source_path.resolve()) if source_path else "",
        "archive_path": "",
        "extracted_path": "",
        "sha256": "",
        "status": "unconfigured",
    }

    if url:
        metadata["kind"] = "url"
        archive_path = archive_destination / archive_name_from_url(url)
        metadata["archive_path"] = str(archive_path)
        metadata["extracted_path"] = str(extraction_destination)
        if dry_run:
            metadata["status"] = "planned"
            return metadata

        ensure_dir(archive_destination, False)
        urllib.request.urlretrieve(url, archive_path)
        metadata["sha256"] = sha256_file(archive_path)
        extraction = extract_archive(archive_path, extraction_destination, False)
        metadata["status"] = extraction["status"]
        metadata["archive_kind"] = extraction["archive_kind"]
        return metadata

    if source_path:
        if not source_path.exists():
            raise FileNotFoundError(f"CPC archive path does not exist: {source_path}")

        if source_path.is_dir():
            metadata["kind"] = "local-directory"
            metadata["extracted_path"] = str(source_path.resolve())
            metadata["status"] = "ready"
            return metadata

        metadata["kind"] = "local-archive"
        metadata["archive_path"] = str(source_path.resolve())
        metadata["extracted_path"] = str(extraction_destination)
        if dry_run:
            metadata["status"] = "planned"
            return metadata

        metadata["sha256"] = sha256_file(source_path)
        extraction = extract_archive(source_path, extraction_destination, False)
        metadata["status"] = extraction["status"]
        metadata["archive_kind"] = extraction["archive_kind"]
        return metadata

    return metadata


def fetch_upstream_inputs(
    *,
    root: Path,
    amflow_url: str | None,
    amflow_ref: str | None,
    amflow_path: Path | None,
    cpc_url: str | None,
    cpc_archive_path: Path | None,
    dry_run: bool,
) -> dict[str, Any]:
    if amflow_url and amflow_path:
        raise ValueError("pass either --amflow-url or --amflow-path, not both")
    if cpc_url and cpc_archive_path:
        raise ValueError("pass either --cpc-url or --cpc-archive-path, not both")

    inputs_root = root / "inputs"
    upstream_dir = inputs_root / "upstream"
    downloads_dir = inputs_root / "downloads"
    extracted_dir = inputs_root / "extracted"

    summary = {
        "amflow": {
            "kind": "unset",
            "url": amflow_url or "",
            "path": "",
            "requested_ref": amflow_ref or "",
            "resolved_commit": "",
            "status": "unconfigured",
        },
        "cpc_archive": {
            "kind": "unset",
            "url": cpc_url or "",
            "source_path": str(cpc_archive_path.resolve()) if cpc_archive_path else "",
            "archive_path": "",
            "extracted_path": "",
            "sha256": "",
            "status": "unconfigured",
        },
    }

    if amflow_path:
        summary["amflow"] = inspect_existing_checkout(amflow_path)
    elif amflow_url:
        summary["amflow"] = clone_amflow_checkout(
            url=amflow_url,
            destination=upstream_dir / "amflow",
            requested_ref=amflow_ref,
            dry_run=dry_run,
        )

    summary["cpc_archive"] = materialize_cpc_archive(
        url=cpc_url,
        source_path=cpc_archive_path,
        archive_destination=downloads_dir,
        extraction_destination=extracted_dir / "cpc",
        dry_run=dry_run,
    )
    return summary


def run_self_check() -> dict[str, Any]:
    with tempfile.TemporaryDirectory(prefix="amflow-fetch-self-check-") as tmp:
        temp_root = Path(tmp)
        remote_bare = temp_root / "remote.git"
        seed_checkout = temp_root / "seed"
        mismatch_remote = temp_root / "mismatch.git"
        archive_one = temp_root / "archive-one.zip"
        archive_two = temp_root / "archive-two.zip"
        symlink_tar = temp_root / "bad-symlink.tar"
        hardlink_tar = temp_root / "bad-hardlink.tar"
        device_tar = temp_root / "bad-device.tar"
        absolute_tar = temp_root / "bad-absolute.tar"
        escape_tar = temp_root / "bad-escape.tar"
        mixed_tar = temp_root / "mixed.tar"
        harness_root = temp_root / "harness"

        run_checked(["git", "init", "--bare", str(remote_bare)])
        run_checked(["git", "clone", str(remote_bare), str(seed_checkout)])
        run_checked(["git", "-C", str(seed_checkout), "config", "user.name", "AMFlow Self Check"])
        run_checked(
            ["git", "-C", str(seed_checkout), "config", "user.email", "amflow-self-check@example.invalid"]
        )
        (seed_checkout / "README.md").write_text("seed\n", encoding="utf-8")
        run_checked(["git", "-C", str(seed_checkout), "add", "README.md"])
        run_checked(["git", "-C", str(seed_checkout), "commit", "-m", "seed"])
        run_checked(["git", "-C", str(seed_checkout), "branch", "-M", "main"])
        run_checked(["git", "-C", str(seed_checkout), "push", "-u", "origin", "main"])

        first_fetch = fetch_upstream_inputs(
            root=harness_root,
            amflow_url=str(remote_bare),
            amflow_ref="main",
            amflow_path=None,
            cpc_url=None,
            cpc_archive_path=None,
            dry_run=False,
        )

        (seed_checkout / "README.md").write_text("seed\nsecond\n", encoding="utf-8")
        run_checked(["git", "-C", str(seed_checkout), "commit", "-am", "second"])
        run_checked(["git", "-C", str(seed_checkout), "push", "origin", "main"])
        expected_commit = read_stdout(["git", "-C", str(seed_checkout), "rev-parse", "HEAD"])

        second_fetch = fetch_upstream_inputs(
            root=harness_root,
            amflow_url=str(remote_bare),
            amflow_ref="main",
            amflow_path=None,
            cpc_url=None,
            cpc_archive_path=None,
            dry_run=False,
        )

        run_checked(["git", "init", "--bare", str(mismatch_remote)])
        mismatch_detected = False
        mismatch_message = ""
        try:
            fetch_upstream_inputs(
                root=harness_root,
                amflow_url=str(mismatch_remote),
                amflow_ref="main",
                amflow_path=None,
                cpc_url=None,
                cpc_archive_path=None,
                dry_run=False,
            )
        except RuntimeError as error:
            mismatch_detected = "origin does not match requested URL" in str(error)
            mismatch_message = str(error)

        with zipfile.ZipFile(archive_one, "w") as handle:
            handle.writestr("old.txt", "old\n")
        with zipfile.ZipFile(archive_two, "w") as handle:
            handle.writestr("new.txt", "new\n")

        extraction_destination = harness_root / "inputs" / "extracted" / "cpc"
        materialize_cpc_archive(
            url=None,
            source_path=archive_one,
            archive_destination=harness_root / "inputs" / "downloads",
            extraction_destination=extraction_destination,
            dry_run=False,
        )
        materialize_cpc_archive(
            url=None,
            source_path=archive_two,
            archive_destination=harness_root / "inputs" / "downloads",
            extraction_destination=extraction_destination,
            dry_run=False,
        )

        def write_tar_with_member(archive_path: Path, member: tarfile.TarInfo, payload: bytes | None = None) -> None:
            with tarfile.open(archive_path, "w") as handle:
                stream = io.BytesIO(payload) if payload is not None else None
                handle.addfile(member, stream)

        symlink_member = tarfile.TarInfo("bad-link")
        symlink_member.type = tarfile.SYMTYPE
        symlink_member.linkname = "target.txt"
        write_tar_with_member(symlink_tar, symlink_member)

        hardlink_member = tarfile.TarInfo("bad-hardlink")
        hardlink_member.type = tarfile.LNKTYPE
        hardlink_member.linkname = "target.txt"
        write_tar_with_member(hardlink_tar, hardlink_member)

        device_member = tarfile.TarInfo("bad-device")
        device_member.type = tarfile.CHRTYPE
        device_member.devmajor = 1
        device_member.devminor = 3
        write_tar_with_member(device_tar, device_member)

        absolute_member = tarfile.TarInfo("/absolute.txt")
        absolute_payload = b"absolute\n"
        absolute_member.size = len(absolute_payload)
        write_tar_with_member(absolute_tar, absolute_member, absolute_payload)

        escape_member = tarfile.TarInfo("../escape.txt")
        escape_payload = b"escape\n"
        escape_member.size = len(escape_payload)
        write_tar_with_member(escape_tar, escape_member, escape_payload)

        with tarfile.open(mixed_tar, "w") as handle:
            safe_member = tarfile.TarInfo("safe.txt")
            safe_payload = b"safe\n"
            safe_member.size = len(safe_payload)
            handle.addfile(safe_member, io.BytesIO(safe_payload))
            handle.addfile(symlink_member)

        tar_policy_checks: dict[str, bool] = {}
        for label, archive_path in {
            "tar_symlink_rejected": symlink_tar,
            "tar_hardlink_rejected": hardlink_tar,
            "tar_device_rejected": device_tar,
            "tar_absolute_path_rejected": absolute_tar,
            "tar_escape_rejected": escape_tar,
        }.items():
            try:
                extract_archive(archive_path, harness_root / "inputs" / "extracted" / label, False)
            except RuntimeError:
                tar_policy_checks[label] = True
            else:
                tar_policy_checks[label] = False
        mixed_destination = harness_root / "inputs" / "extracted" / "tar-mixed"
        try:
            extract_archive(mixed_tar, mixed_destination, False)
        except RuntimeError:
            tar_policy_checks["tar_mixed_archive_rejected_before_write"] = not (
                mixed_destination / "safe.txt"
            ).exists()
        else:
            tar_policy_checks["tar_mixed_archive_rejected_before_write"] = False

        return {
            "updated_commit_matches_remote": second_fetch["amflow"]["resolved_commit"] == expected_commit,
            "existing_checkout_was_refreshed": first_fetch["amflow"]["resolved_commit"]
            != second_fetch["amflow"]["resolved_commit"],
            "mismatch_detected": mismatch_detected,
            "mismatch_message": mismatch_message,
            "cpc_extraction_cleaned": not (extraction_destination / "old.txt").exists()
            and (extraction_destination / "new.txt").exists(),
            **tar_policy_checks,
        }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True, help="Reference harness root")
    parser.add_argument("--amflow-url", help="Public AMFlow Git URL to clone")
    parser.add_argument("--amflow-ref", help="Optional git ref to check out after clone")
    parser.add_argument("--amflow-path", type=Path, help="Existing local AMFlow checkout to record")
    parser.add_argument("--cpc-url", help="CPC archive URL to download")
    parser.add_argument(
        "--cpc-archive-path",
        type=Path,
        help="Existing local CPC archive or extracted directory to record",
    )
    parser.add_argument("--summary-json", type=Path, help="Optional path for the JSON summary")
    parser.add_argument("--dry-run", action="store_true", help="Plan fetch actions without writing")
    parser.add_argument(
        "--self-check",
        action="store_true",
        help=(
            "Run a local no-network regression check for remote verification, clean extraction "
            "behavior, and tar archive policy enforcement"
        ),
    )
    args = parser.parse_args()

    if args.self_check:
        print(json.dumps(run_self_check(), indent=2, sort_keys=True))
        return 0

    summary = fetch_upstream_inputs(
        root=args.root,
        amflow_url=args.amflow_url,
        amflow_ref=args.amflow_ref,
        amflow_path=args.amflow_path,
        cpc_url=args.cpc_url,
        cpc_archive_path=args.cpc_archive_path,
        dry_run=args.dry_run,
    )

    if args.summary_json:
        ensure_dir(args.summary_json.parent, args.dry_run)
        write_json(args.summary_json, summary, args.dry_run)

    print(json.dumps(summary, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
