#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-autoIBP-TBD
"""CTest gate for the pinned amflow-cli binary size."""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from decimal import Decimal, InvalidOperation, ROUND_CEILING, ROUND_FLOOR
from pathlib import Path
from typing import Any


DEFAULT_BASELINE = Path("tools/reference-harness/specs/release/amflow-cli-size-baseline.json")
SCOPE = "amflow-cli-binary-size-baseline"


class BinarySizeError(RuntimeError):
    """Raised when amflow-cli leaves the pinned binary-size window."""


@dataclass(frozen=True)
class BinarySizeBaseline:
    binary_target: str
    unstripped_size_bytes: int
    stripped_size_bytes: int
    tolerance_percent: Decimal
    min_unstripped_size_bytes: int
    max_unstripped_size_bytes: int
    min_stripped_size_bytes: int
    max_stripped_size_bytes: int


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise BinarySizeError(message)


def read_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as stream:
        payload = json.load(stream)
    expect(isinstance(payload, dict), f"{path} must contain a JSON object")
    return payload


def require_string(payload: dict[str, Any], field: str) -> str:
    value = payload.get(field)
    if not isinstance(value, str) or not value.strip():
        raise BinarySizeError(f"{field} must be a non-empty string")
    expect(value == value.strip(), f"{field} must not carry surrounding whitespace")
    return value


def require_positive_int(payload: dict[str, Any], field: str) -> int:
    value = payload.get(field)
    if type(value) is not int or value <= 0:
        raise BinarySizeError(f"{field} must be a positive integer")
    return value


def tolerance_percent(payload: dict[str, Any]) -> Decimal:
    raw = payload.get("tolerance_percent")
    expect(isinstance(raw, (int, float, str)), "tolerance_percent must be numeric")
    try:
        value = Decimal(str(raw))
    except InvalidOperation as error:
        raise BinarySizeError("tolerance_percent must be a valid decimal") from error
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


def load_baseline(path: Path) -> BinarySizeBaseline:
    payload = read_json(path)
    expect(payload.get("schema_version") == 1, "schema_version must be 1")
    expect(payload.get("scope") == SCOPE, f"scope must be {SCOPE}")

    binary_target = require_string(payload, "binary_target")
    expect(binary_target == "amflow-cli", "binary_target must be amflow-cli")
    unstripped_size = require_positive_int(payload, "unstripped_size_bytes")
    stripped_size = require_positive_int(payload, "stripped_size_bytes")
    tolerance = tolerance_percent(payload)

    expected_min_unstripped, expected_max_unstripped = expected_window(
        unstripped_size,
        tolerance,
    )
    expected_min_stripped, expected_max_stripped = expected_window(
        stripped_size,
        tolerance,
    )
    pinned_min_unstripped = require_positive_int(payload, "min_unstripped_size_bytes")
    pinned_max_unstripped = require_positive_int(payload, "max_unstripped_size_bytes")
    pinned_min_stripped = require_positive_int(payload, "min_stripped_size_bytes")
    pinned_max_stripped = require_positive_int(payload, "max_stripped_size_bytes")

    expect(
        pinned_min_unstripped == expected_min_unstripped,
        "min_unstripped_size_bytes does not match unstripped_size_bytes/tolerance_percent",
    )
    expect(
        pinned_max_unstripped == expected_max_unstripped,
        "max_unstripped_size_bytes does not match unstripped_size_bytes/tolerance_percent",
    )
    expect(
        pinned_min_stripped == expected_min_stripped,
        "min_stripped_size_bytes does not match stripped_size_bytes/tolerance_percent",
    )
    expect(
        pinned_max_stripped == expected_max_stripped,
        "max_stripped_size_bytes does not match stripped_size_bytes/tolerance_percent",
    )
    expect(
        pinned_min_unstripped <= unstripped_size <= pinned_max_unstripped,
        "unstripped baseline size must fall within its own window",
    )
    expect(
        pinned_min_stripped <= stripped_size <= pinned_max_stripped,
        "stripped baseline size must fall within its own window",
    )

    return BinarySizeBaseline(
        binary_target=binary_target,
        unstripped_size_bytes=unstripped_size,
        stripped_size_bytes=stripped_size,
        tolerance_percent=tolerance,
        min_unstripped_size_bytes=pinned_min_unstripped,
        max_unstripped_size_bytes=pinned_max_unstripped,
        min_stripped_size_bytes=pinned_min_stripped,
        max_stripped_size_bytes=pinned_max_stripped,
    )


def strip_copy_size(binary_path: Path) -> int:
    strip_tool = shutil.which("strip")
    if strip_tool is None:
        raise BinarySizeError("strip tool not found on PATH")
    with tempfile.TemporaryDirectory(prefix="amflow-cli-size-") as temp_dir:
        stripped_path = Path(temp_dir) / binary_path.name
        shutil.copy2(binary_path, stripped_path)
        completed = subprocess.run(
            [strip_tool, "--strip-all", str(stripped_path)],
            text=True,
            capture_output=True,
            check=False,
        )
        if completed.returncode != 0:
            output = "\n".join(
                part for part in (completed.stdout, completed.stderr) if part
            ).strip()
            raise BinarySizeError(
                f"strip --strip-all failed with exit {completed.returncode}"
                + (f"\n{output}" if output else "")
            )
        return stripped_path.stat().st_size


def verify_size(
    label: str,
    actual_size: int,
    baseline_size: int,
    min_size: int,
    max_size: int,
    tolerance: Decimal,
) -> None:
    if not (min_size <= actual_size <= max_size):
        raise BinarySizeError(
            f"amflow-cli {label} size outside pinned window: "
            f"actual={actual_size} bytes, "
            f"baseline={baseline_size} bytes, "
            f"window=[{min_size}, {max_size}] bytes, "
            f"tolerance={tolerance}%"
        )


def verify_baseline(binary_path: Path, baseline_path: Path) -> tuple[int, int, BinarySizeBaseline]:
    baseline = load_baseline(baseline_path)
    expect(binary_path.is_file(), f"binary does not exist: {binary_path}")
    unstripped_size = binary_path.stat().st_size
    stripped_size = strip_copy_size(binary_path)

    verify_size(
        "unstripped",
        unstripped_size,
        baseline.unstripped_size_bytes,
        baseline.min_unstripped_size_bytes,
        baseline.max_unstripped_size_bytes,
        baseline.tolerance_percent,
    )
    verify_size(
        "stripped",
        stripped_size,
        baseline.stripped_size_bytes,
        baseline.min_stripped_size_bytes,
        baseline.max_stripped_size_bytes,
        baseline.tolerance_percent,
    )
    return unstripped_size, stripped_size, baseline


def expect_failure(action: Any, expected: str) -> None:
    try:
        action()
    except BinarySizeError as exc:
        expect(expected in str(exc), f"unexpected self-check error: {exc}")
        return
    raise BinarySizeError("self-check fixture unexpectedly passed")


def run_self_check() -> None:
    payload = {
        "schema_version": 1,
        "scope": SCOPE,
        "binary_target": "amflow-cli",
        "unstripped_size_bytes": 1000,
        "stripped_size_bytes": 800,
        "tolerance_percent": 15,
        "min_unstripped_size_bytes": 850,
        "max_unstripped_size_bytes": 1150,
        "min_stripped_size_bytes": 680,
        "max_stripped_size_bytes": 920,
    }
    with tempfile.TemporaryDirectory(prefix="amflow-cli-size-self-check-") as temp_dir:
        baseline_path = Path(temp_dir) / "baseline.json"
        baseline_path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
        baseline = load_baseline(baseline_path)
        for size in (850, 1000, 1150):
            verify_size(
                "unstripped",
                size,
                baseline.unstripped_size_bytes,
                baseline.min_unstripped_size_bytes,
                baseline.max_unstripped_size_bytes,
                baseline.tolerance_percent,
            )
        for size in (680, 800, 920):
            verify_size(
                "stripped",
                size,
                baseline.stripped_size_bytes,
                baseline.min_stripped_size_bytes,
                baseline.max_stripped_size_bytes,
                baseline.tolerance_percent,
            )
        expect_failure(
            lambda: verify_size(
                "unstripped",
                849,
                baseline.unstripped_size_bytes,
                baseline.min_unstripped_size_bytes,
                baseline.max_unstripped_size_bytes,
                baseline.tolerance_percent,
            ),
            "outside pinned window",
        )
        expect_failure(
            lambda: verify_size(
                "stripped",
                921,
                baseline.stripped_size_bytes,
                baseline.min_stripped_size_bytes,
                baseline.max_stripped_size_bytes,
                baseline.tolerance_percent,
            ),
            "outside pinned window",
        )

        bad_min = dict(payload)
        bad_min["min_unstripped_size_bytes"] = 849
        baseline_path.write_text(json.dumps(bad_min, indent=2) + "\n", encoding="utf-8")
        expect_failure(lambda: load_baseline(baseline_path), "min_unstripped_size_bytes")

        bad_target = dict(payload)
        bad_target["binary_target"] = "other-cli"
        baseline_path.write_text(json.dumps(bad_target, indent=2) + "\n", encoding="utf-8")
        expect_failure(lambda: load_baseline(baseline_path), "binary_target")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "binary",
        nargs="?",
        type=Path,
        help="Path to the built amflow-cli executable.",
    )
    parser.add_argument(
        "--baseline",
        type=Path,
        default=DEFAULT_BASELINE,
        help="Pinned amflow-cli binary-size baseline JSON.",
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
            print("amflow-cli size verifier self-check passed")
            return 0

        if args.binary is None:
            raise BinarySizeError("binary path is required unless --self-check is used")
        baseline_path = args.baseline.resolve()
        unstripped_size, stripped_size, baseline = verify_baseline(
            args.binary.resolve(),
            baseline_path,
        )
        print(
            "amflow-cli binary size verified: "
            f"unstripped={unstripped_size} bytes within "
            f"[{baseline.min_unstripped_size_bytes}, {baseline.max_unstripped_size_bytes}], "
            f"stripped={stripped_size} bytes within "
            f"[{baseline.min_stripped_size_bytes}, {baseline.max_stripped_size_bytes}]"
        )
        return 0
    except (BinarySizeError, OSError, json.JSONDecodeError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
