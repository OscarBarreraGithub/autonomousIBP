#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-autoIBP-TBD
"""CTest gate for the pinned M7 release evidence tarball size."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from decimal import Decimal, InvalidOperation, ROUND_CEILING, ROUND_FLOOR
from pathlib import Path
from typing import Any


DEFAULT_BASELINE = Path("tools/reference-harness/specs/release/m7-tarball-size-baseline.json")
DEFAULT_TARBALL_NAME = "m7-release-evidence.tar.gz"
SCOPE = "m7-release-evidence-tarball-size-baseline"


class TarballSizeError(RuntimeError):
    """Raised when the release evidence tarball size leaves the pinned window."""


@dataclass(frozen=True)
class SizeBaseline:
    package_script: str
    readiness_sidecar: str
    bundle_root: str
    compressed_size_bytes: int
    tolerance_percent: Decimal
    min_compressed_size_bytes: int
    max_compressed_size_bytes: int


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise TarballSizeError(message)


def read_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as stream:
        payload = json.load(stream)
    expect(isinstance(payload, dict), f"{path} must contain a JSON object")
    return payload


def require_string(payload: dict[str, Any], field: str) -> str:
    value = payload.get(field)
    if not isinstance(value, str) or not value.strip():
        raise TarballSizeError(f"{field} must be a non-empty string")
    expect(value == value.strip(), f"{field} must not carry surrounding whitespace")
    return value


def require_relative_posix_path(value: str, field: str) -> str:
    path = Path(value)
    expect(not path.is_absolute(), f"{field} must be repository-relative")
    expect("\\" not in value, f"{field} must use POSIX path separators")
    expect(
        all(part not in {"", ".", ".."} for part in value.split("/")),
        f"{field} must be a safe relative POSIX path",
    )
    return value


def require_positive_int(payload: dict[str, Any], field: str) -> int:
    value = payload.get(field)
    if type(value) is not int or value <= 0:
        raise TarballSizeError(f"{field} must be a positive integer")
    return value


def tolerance_percent(payload: dict[str, Any]) -> Decimal:
    raw = payload.get("tolerance_percent")
    expect(isinstance(raw, (int, float, str)), "tolerance_percent must be numeric")
    try:
        value = Decimal(str(raw))
    except InvalidOperation as error:
        raise TarballSizeError("tolerance_percent must be a valid decimal") from error
    expect(value >= 0, "tolerance_percent must be nonnegative")
    expect(value < 100, "tolerance_percent must be less than 100")
    return value


def expected_window(size: int, tolerance: Decimal) -> tuple[int, int]:
    scale = Decimal(100)
    lower = (Decimal(size) * (scale - tolerance) / scale).to_integral_value(
        rounding=ROUND_CEILING
    )
    upper = (Decimal(size) * (scale + tolerance) / scale).to_integral_value(
        rounding=ROUND_FLOOR
    )
    return int(lower), int(upper)


def load_baseline(path: Path) -> SizeBaseline:
    payload = read_json(path)
    expect(payload.get("schema_version") == 1, "schema_version must be 1")
    expect(payload.get("scope") == SCOPE, f"scope must be {SCOPE}")

    package_script = require_relative_posix_path(require_string(payload, "package_script"), "package_script")
    readiness_sidecar = require_relative_posix_path(
        require_string(payload, "readiness_sidecar"),
        "readiness_sidecar",
    )
    bundle_root = require_relative_posix_path(require_string(payload, "bundle_root"), "bundle_root")
    size = require_positive_int(payload, "compressed_size_bytes")
    tolerance = tolerance_percent(payload)
    expected_min, expected_max = expected_window(size, tolerance)
    pinned_min = require_positive_int(payload, "min_compressed_size_bytes")
    pinned_max = require_positive_int(payload, "max_compressed_size_bytes")

    expect(
        pinned_min == expected_min,
        "min_compressed_size_bytes does not match compressed_size_bytes/tolerance_percent",
    )
    expect(
        pinned_max == expected_max,
        "max_compressed_size_bytes does not match compressed_size_bytes/tolerance_percent",
    )
    expect(pinned_min <= size <= pinned_max, "baseline size must fall within its own window")

    return SizeBaseline(
        package_script=package_script,
        readiness_sidecar=readiness_sidecar,
        bundle_root=bundle_root,
        compressed_size_bytes=size,
        tolerance_percent=tolerance,
        min_compressed_size_bytes=pinned_min,
        max_compressed_size_bytes=pinned_max,
    )


def package_release_evidence(
    root: Path,
    baseline: SizeBaseline,
    output_path: Path,
) -> dict[str, Any]:
    package_script = root / baseline.package_script
    expect(package_script.is_file(), f"package script does not exist: {baseline.package_script}")
    command = [
        sys.executable,
        str(package_script),
        "--readiness-sidecar",
        baseline.readiness_sidecar,
        "--bundle-root",
        baseline.bundle_root,
        "--output",
        str(output_path),
    ]
    completed = subprocess.run(
        command,
        cwd=root,
        text=True,
        capture_output=True,
        check=False,
    )
    if completed.returncode != 0:
        output = "\n".join(
            part for part in (completed.stdout, completed.stderr) if part
        ).strip()
        raise TarballSizeError(
            f"package_m7_release_evidence.py failed with exit {completed.returncode}"
            + (f"\n{output}" if output else "")
        )
    try:
        metadata = json.loads(completed.stdout)
    except json.JSONDecodeError as error:
        raise TarballSizeError("package_m7_release_evidence.py did not emit JSON metadata") from error
    expect(isinstance(metadata, dict), "package metadata must be a JSON object")
    expect(output_path.is_file(), f"package script did not create {output_path}")
    return metadata


def verify_size(actual_size: int, baseline: SizeBaseline) -> None:
    if not (
        baseline.min_compressed_size_bytes
        <= actual_size
        <= baseline.max_compressed_size_bytes
    ):
        raise TarballSizeError(
            "M7 release evidence tarball size outside pinned window: "
            f"actual={actual_size} bytes, "
            f"baseline={baseline.compressed_size_bytes} bytes, "
            f"window=[{baseline.min_compressed_size_bytes}, "
            f"{baseline.max_compressed_size_bytes}] bytes, "
            f"tolerance={baseline.tolerance_percent}%"
        )


def verify_baseline(root: Path, baseline_path: Path) -> tuple[int, dict[str, Any], SizeBaseline]:
    baseline = load_baseline(baseline_path)
    with tempfile.TemporaryDirectory(prefix="m7-release-evidence-size-") as temp_dir:
        output_path = Path(temp_dir) / DEFAULT_TARBALL_NAME
        metadata = package_release_evidence(root, baseline, output_path)
        actual_size = output_path.stat().st_size
    verify_size(actual_size, baseline)
    return actual_size, metadata, baseline


def expect_failure(action: Any, expected: str) -> None:
    try:
        action()
    except TarballSizeError as exc:
        expect(expected in str(exc), f"unexpected self-check error: {exc}")
        return
    raise TarballSizeError("self-check fixture unexpectedly passed")


def run_self_check() -> None:
    payload = {
        "schema_version": 1,
        "scope": SCOPE,
        "package_script": "tools/reference-harness/scripts/package_m7_release_evidence.py",
        "readiness_sidecar": (
            "tools/reference-harness/specs/m7/lane3/"
            "release-readiness.m5-accepted.full-output.json"
        ),
        "bundle_root": "m7-release-evidence",
        "compressed_size_bytes": 1000,
        "tolerance_percent": 10,
        "min_compressed_size_bytes": 900,
        "max_compressed_size_bytes": 1100,
    }
    with tempfile.TemporaryDirectory(prefix="m7-release-evidence-size-self-check-") as temp_dir:
        baseline_path = Path(temp_dir) / "baseline.json"
        baseline_path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
        baseline = load_baseline(baseline_path)
        for size in (900, 1000, 1100):
            verify_size(size, baseline)
        expect_failure(lambda: verify_size(899, baseline), "outside pinned window")
        expect_failure(lambda: verify_size(1101, baseline), "outside pinned window")

        bad_min = dict(payload)
        bad_min["min_compressed_size_bytes"] = 899
        baseline_path.write_text(json.dumps(bad_min, indent=2) + "\n", encoding="utf-8")
        expect_failure(lambda: load_baseline(baseline_path), "min_compressed_size_bytes")

        bad_root = dict(payload)
        bad_root["bundle_root"] = "../m7-release-evidence"
        baseline_path.write_text(json.dumps(bad_root, indent=2) + "\n", encoding="utf-8")
        expect_failure(lambda: load_baseline(baseline_path), "bundle_root")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--baseline",
        type=Path,
        default=DEFAULT_BASELINE,
        help="Pinned tarball-size baseline JSON.",
    )
    parser.add_argument(
        "--self-check",
        action="store_true",
        help="Run synthetic positive and negative checks for this verifier.",
    )
    args = parser.parse_args(argv)

    try:
        if args.self_check:
            run_self_check()
            print("M7 release tarball size verifier self-check passed")
            return 0

        root = repo_root()
        baseline_path = args.baseline if args.baseline.is_absolute() else root / args.baseline
        actual_size, metadata, baseline = verify_baseline(root, baseline_path)
        print(
            "M7 release evidence tarball size verified: "
            f"{actual_size} bytes within "
            f"[{baseline.min_compressed_size_bytes}, {baseline.max_compressed_size_bytes}] "
            f"bytes; file_count={metadata.get('file_count')}, "
            f"source_commit={metadata.get('source_commit')}"
        )
        return 0
    except (TarballSizeError, OSError, json.JSONDecodeError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
