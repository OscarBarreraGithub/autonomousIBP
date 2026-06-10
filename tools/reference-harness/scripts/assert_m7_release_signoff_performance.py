#!/usr/bin/env python3
"""CTest performance baseline for the accepted M7 release-readiness replay."""

from __future__ import annotations

import argparse
import json
import statistics
import subprocess
import sys
import tempfile
import time
from pathlib import Path
from typing import Any

from assert_m7_release_signoff_ready import (
    ACCEPTED_READINESS_SIDECAR,
    build_readiness_command,
    read_json,
    repo_root,
    summarize_failure,
)


DEFAULT_SAMPLE_COUNT = 3
DEFAULT_MAX_MEDIAN_REPLAY_SECONDS = 5.0
DEFAULT_MAX_SINGLE_REPLAY_SECONDS = 10.0

WITHHELD_CLAIMS: tuple[str, ...] = (
    "This gate measures release_signoff_readiness.py orchestration overhead only.",
    "This gate does not benchmark AMFlow runtime numerics.",
    "This gate does not claim release readiness independently of accepted sidecars.",
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--samples",
        type=int,
        default=DEFAULT_SAMPLE_COUNT,
        help="Number of accepted-input replay samples to collect.",
    )
    parser.add_argument(
        "--max-median-seconds",
        type=float,
        default=DEFAULT_MAX_MEDIAN_REPLAY_SECONDS,
        help="Maximum allowed median wall-clock seconds across replay samples.",
    )
    parser.add_argument(
        "--max-single-seconds",
        type=float,
        default=DEFAULT_MAX_SINGLE_REPLAY_SECONDS,
        help="Maximum allowed wall-clock seconds for any single replay sample.",
    )
    return parser.parse_args()


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def run_replay_sample(
    root: Path,
    accepted_summary: dict[str, Any],
    output_path: Path,
) -> tuple[float, dict[str, Any]]:
    command = build_readiness_command(root, accepted_summary, output_path)
    start = time.perf_counter()
    completed = subprocess.run(
        command,
        cwd=root,
        text=True,
        capture_output=True,
        check=False,
    )
    elapsed_seconds = time.perf_counter() - start
    if completed.returncode != 0:
        print("release_signoff_readiness.py failed during the M7 performance gate", file=sys.stderr)
        if completed.stdout:
            print("stdout:", file=sys.stderr)
            print(completed.stdout, file=sys.stderr)
        if completed.stderr:
            print("stderr:", file=sys.stderr)
            print(completed.stderr, file=sys.stderr)
        raise RuntimeError("accepted-input release-readiness replay failed")

    summary = read_json(output_path)
    if summary.get("release_signoff_ready") is not True or summary.get("release_signoff_blockers") != []:
        print(
            "M7 release readiness performance gate measured a non-ready accepted-input replay",
            file=sys.stderr,
        )
        print(json.dumps(summarize_failure(summary), indent=2, sort_keys=True), file=sys.stderr)
        raise RuntimeError("accepted-input release-readiness replay was not release-ready")

    return elapsed_seconds, summary


def main() -> int:
    args = parse_args()
    expect(args.samples > 0, "--samples must be positive")
    expect(args.max_median_seconds > 0, "--max-median-seconds must be positive")
    expect(args.max_single_seconds > 0, "--max-single-seconds must be positive")

    root = repo_root()
    accepted_summary = read_json(root / ACCEPTED_READINESS_SIDECAR)
    elapsed_samples: list[float] = []
    final_summary: dict[str, Any] | None = None
    with tempfile.TemporaryDirectory(prefix="m7-release-readiness-performance-") as temp_dir:
        temp_root = Path(temp_dir)
        for sample_index in range(args.samples):
            elapsed_seconds, final_summary = run_replay_sample(
                root,
                accepted_summary,
                temp_root / f"release-readiness-{sample_index}.json",
            )
            elapsed_samples.append(elapsed_seconds)

    median_seconds = statistics.median(elapsed_samples)
    slowest_seconds = max(elapsed_samples)
    result = {
        "schema_version": 1,
        "scope": "m7-release-signoff-readiness-performance-baseline",
        "sample_count": args.samples,
        "elapsed_seconds": [round(value, 6) for value in elapsed_samples],
        "median_replay_seconds": round(median_seconds, 6),
        "slowest_replay_seconds": round(slowest_seconds, 6),
        "max_median_replay_seconds": args.max_median_seconds,
        "max_single_replay_seconds": args.max_single_seconds,
        "release_signoff_ready": final_summary.get("release_signoff_ready") if final_summary else None,
        "release_signoff_blockers": final_summary.get("release_signoff_blockers") if final_summary else None,
        "withheld_claims": list(WITHHELD_CLAIMS),
    }

    if median_seconds > args.max_median_seconds or slowest_seconds > args.max_single_seconds:
        print("M7 release readiness performance baseline regressed", file=sys.stderr)
        print(json.dumps(result, indent=2, sort_keys=True), file=sys.stderr)
        return 1

    print(json.dumps(result, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
