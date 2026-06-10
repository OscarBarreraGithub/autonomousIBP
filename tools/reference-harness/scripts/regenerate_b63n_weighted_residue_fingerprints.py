#!/usr/bin/env python3
"""Emit fresh b63n weighted-residue audit fingerprints from the C++ runtime."""

from __future__ import annotations

import argparse
import copy
import json
import os
import re
import subprocess
import sys
from pathlib import Path
from typing import Any


EMIT_FLAG = "--emit-b63n-weighted-residue-fingerprints"
EXPECTED_KIND = "b63n-weighted-residue-audit-fingerprint-regeneration"
EXPECTED_LABELS = [
    "evaluation-plan",
    "D7-moment-seed",
    "moment-seed-packet",
    "moment-cross-validation-gate",
    "blocked-D2-scoped-weighted-residue",
    "published-D7-scoped-weighted-residue",
]
EXPECTED_CATEGORIES = [
    "b63n-cutkosky-weighted-residue-evaluation-plan",
    "b63n-automatic-phasespace-weighted-residue-moment-seed",
    "b63n-automatic-phasespace-weighted-residue-moment-seed-packet",
    "b63n-weighted-residue-moment-cross-validation-gate",
    "b63n-scoped-weighted-residue-evaluation",
]
FNV1A64_RE = re.compile(r"^fnv1a64:[0-9a-f]{16}$")


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def common_binary_candidates(root: Path) -> list[Path]:
    return [
        root / "build" / "cutkosky-weighted-residue-tests",
        root / "build-debug" / "cutkosky-weighted-residue-tests",
        root / "cmake-build-debug" / "cutkosky-weighted-residue-tests",
        root / "cmake-build-release" / "cutkosky-weighted-residue-tests",
        root / "out" / "build" / "cutkosky-weighted-residue-tests",
    ]


def resolve_test_binary(raw_path: str | None, root: Path) -> Path:
    candidates: list[Path] = []
    if raw_path:
        candidates.append(Path(raw_path))
    env_path = os.environ.get("B63N_WEIGHTED_RESIDUE_TEST_BINARY")
    if env_path:
        candidates.append(Path(env_path))
    candidates.extend(common_binary_candidates(root))

    checked: list[str] = []
    for candidate in candidates:
        path = candidate if candidate.is_absolute() else root / candidate
        checked.append(path.as_posix())
        if path.is_file():
            return path

    raise RuntimeError(
        "unable to find cutkosky-weighted-residue-tests; pass --test-binary. "
        "checked: "
        + ", ".join(checked)
    )


def run_emitter(test_binary: Path, root: Path) -> dict[str, Any]:
    completed = subprocess.run(
        [test_binary.as_posix(), EMIT_FLAG],
        cwd=root,
        check=False,
        encoding="utf-8",
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    if completed.returncode != 0:
        raise RuntimeError(
            f"{test_binary} {EMIT_FLAG} failed with exit code "
            f"{completed.returncode}:\n{completed.stderr}"
        )
    try:
        payload = json.loads(completed.stdout)
    except json.JSONDecodeError as error:
        raise RuntimeError(
            f"{test_binary} did not emit valid JSON:\n{completed.stdout}"
        ) from error
    expect(isinstance(payload, dict), "fingerprint emitter must return a JSON object")
    return payload


def require_string(raw: Any, label: str) -> str:
    expect(isinstance(raw, str) and raw, f"{label} must be a non-empty string")
    return raw


def require_fingerprint(raw: Any, label: str) -> str:
    value = require_string(raw, label)
    expect(
        FNV1A64_RE.fullmatch(value) is not None,
        f"{label} must be fnv1a64:<16 lowercase hex>",
    )
    return value


def require_bool(raw: Any, label: str) -> bool:
    expect(isinstance(raw, bool), f"{label} must be a bool")
    return raw


def require_list(raw: Any, label: str) -> list[Any]:
    expect(isinstance(raw, list), f"{label} must be a list")
    return raw


def validate_payload(payload: dict[str, Any], *, require_matches: bool) -> dict[str, Any]:
    expect(payload.get("kind") == EXPECTED_KIND, "unexpected fingerprint payload kind")
    categories = require_list(payload.get("published_categories"), "published_categories")
    expect(
        categories == EXPECTED_CATEGORIES,
        "published b63n weighted-residue audit categories drifted",
    )
    entries = require_list(payload.get("entries"), "entries")
    expect(payload.get("entry_count") == len(entries), "entry_count must match entries")

    labels: list[str] = []
    fresh_fingerprints: list[str] = []
    mismatches: list[dict[str, str]] = []
    for index, raw_entry in enumerate(entries):
        expect(isinstance(raw_entry, dict), f"entries[{index}] must be an object")
        label = require_string(raw_entry.get("label"), f"entries[{index}].label")
        category = require_string(raw_entry.get("category"), f"entries[{index}].category")
        fresh = require_fingerprint(
            raw_entry.get("fresh_fingerprint"),
            f"entries[{index}].fresh_fingerprint",
        )
        pinned = require_fingerprint(
            raw_entry.get("pinned_fingerprint"),
            f"entries[{index}].pinned_fingerprint",
        )
        matches = require_bool(raw_entry.get("matches_pin"), f"entries[{index}].matches_pin")
        expect(category in EXPECTED_CATEGORIES, f"{label} has unexpected category {category}")
        expect(
            matches == (fresh == pinned),
            f"{label} matches_pin must reflect fresh/pinned fingerprint equality",
        )
        labels.append(label)
        fresh_fingerprints.append(fresh)
        if not matches:
            mismatches.append(
                {
                    "label": label,
                    "category": category,
                    "fresh_fingerprint": fresh,
                    "pinned_fingerprint": pinned,
                }
            )

    expect(labels == EXPECTED_LABELS, "b63n weighted-residue fingerprint labels drifted")
    expect(
        len(set(fresh_fingerprints)) == len(fresh_fingerprints),
        "fresh b63n weighted-residue audit fingerprints must remain unique",
    )
    if require_matches:
        expect(
            not mismatches,
            "b63n weighted-residue audit fingerprints drifted; run without "
            "--self-check to print fresh hashes",
        )
    return {"mismatches": mismatches, "entry_count": len(entries)}


def rejected(
    payload: dict[str, Any],
    expected_message: str,
    *,
    require_matches: bool = True,
) -> bool:
    try:
        validate_payload(payload, require_matches=require_matches)
    except Exception as error:  # noqa: BLE001 - self-check intentionally probes failures.
        return expected_message in str(error)
    return False


def run_self_check(payload: dict[str, Any]) -> dict[str, Any]:
    accepted = validate_payload(payload, require_matches=True)

    stale_categories = copy.deepcopy(payload)
    stale_categories["published_categories"] = []

    stale_entry_count = copy.deepcopy(payload)
    stale_entry_count["entry_count"] = accepted["entry_count"] + 1

    stale_label = copy.deepcopy(payload)
    stale_label["entries"][0]["label"] = "synthetic-stale-b63n-label"

    malformed_fresh_fingerprint = copy.deepcopy(payload)
    malformed_fresh_fingerprint["entries"][0]["fresh_fingerprint"] = "fnv1a64:nothex"

    malformed_pinned_fingerprint = copy.deepcopy(payload)
    malformed_pinned_fingerprint["entries"][0]["pinned_fingerprint"] = "fnv1a64:00000000000000000"

    false_match_flag = copy.deepcopy(payload)
    false_match_flag["entries"][0]["fresh_fingerprint"] = "fnv1a64:0000000000000000"
    false_match_flag["entries"][0]["matches_pin"] = True

    runtime_fingerprint_drift = copy.deepcopy(payload)
    runtime_fingerprint_drift["entries"][0]["fresh_fingerprint"] = "fnv1a64:0000000000000000"
    runtime_fingerprint_drift["entries"][0]["matches_pin"] = False

    checks = {
        "accepts_runtime_fingerprint_payload": accepted["entry_count"] == len(EXPECTED_LABELS),
        "rejects_published_category_drift": rejected(
            stale_categories,
            "published b63n weighted-residue audit categories drifted",
        ),
        "rejects_stale_entry_count": rejected(
            stale_entry_count,
            "entry_count must match entries",
        ),
        "rejects_label_drift": rejected(
            stale_label,
            "b63n weighted-residue fingerprint labels drifted",
        ),
        "rejects_malformed_fresh_fingerprint": rejected(
            malformed_fresh_fingerprint,
            "fresh_fingerprint must be fnv1a64:<16 lowercase hex>",
        ),
        "rejects_malformed_pinned_fingerprint": rejected(
            malformed_pinned_fingerprint,
            "pinned_fingerprint must be fnv1a64:<16 lowercase hex>",
        ),
        "rejects_false_match_flag": rejected(
            false_match_flag,
            "matches_pin must reflect fresh/pinned fingerprint equality",
        ),
        "rejects_runtime_fingerprint_drift": rejected(
            runtime_fingerprint_drift,
            "b63n weighted-residue audit fingerprints drifted",
        ),
    }
    expect(all(checks.values()), "b63n weighted-residue fingerprint self-check failed")
    return {
        "kind": "b63n-weighted-residue-fingerprint-self-check",
        "entry_count": accepted["entry_count"],
        "passed": True,
        "checks": checks,
    }


def render_plain(payload: dict[str, Any]) -> str:
    lines = [
        "# fresh b63n weighted-residue audit fingerprints",
    ]
    for entry in payload["entries"]:
        lines.append(
            "{label}\t{category}\tfresh={fresh}\tpinned={pinned}\tmatches_pin={matches}".format(
                label=entry["label"],
                category=entry["category"],
                fresh=entry["fresh_fingerprint"],
                pinned=entry["pinned_fingerprint"],
                matches=str(entry["matches_pin"]).lower(),
            )
        )
    return "\n".join(lines) + "\n"


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Regenerate the b63n weighted-residue audit fingerprint hashes from "
            "the built cutkosky-weighted-residue-tests binary."
        )
    )
    parser.add_argument(
        "--test-binary",
        help="Path to the built cutkosky-weighted-residue-tests executable.",
    )
    parser.add_argument(
        "--format",
        choices=("json", "plain"),
        default="json",
        help="Output format for the fresh fingerprints.",
    )
    parser.add_argument(
        "--self-check",
        action="store_true",
        help="Fail unless fresh fingerprints match the currently pinned values.",
    )
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    root = repo_root()
    test_binary = resolve_test_binary(args.test_binary, root)
    payload = run_emitter(test_binary, root)

    if args.self_check:
        print(json.dumps(run_self_check(payload), indent=2, sort_keys=True))
        return 0

    validate_payload(payload, require_matches=False)

    if args.format == "plain":
        print(render_plain(payload), end="")
    else:
        print(json.dumps(payload, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main(sys.argv[1:]))
    except Exception as error:
        print(f"error: {error}", file=sys.stderr)
        raise SystemExit(1)
