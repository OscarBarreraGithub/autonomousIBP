#!/usr/bin/env python3
"""Emit fresh b61n publication audit fingerprints from the C++ runtime."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any

from verify_b61n_publication_audit_trail import (
    compute_artifact_fingerprint,
    expect,
    parse_audit_text,
    repo_root,
    resolve_test_binary,
    run_emitter,
    validate_payload as validate_audit_trail_payload,
)


EXPECTED_KIND = "b61n-publication-audit-fingerprint-regeneration"
EXPECTED_LABELS = ["published-lane142-primitive-bubble"]
EXPECTED_CATEGORIES = ["b61n-publication-contour-evaluation"]
EXPECTED_PINS = {
    "published-lane142-primitive-bubble": {
        "category": "b61n-publication-contour-evaluation",
        "pinned_fingerprint": "fnv1a64:92f403f7f2c701d5",
    },
}


def require_string(raw: Any, label: str) -> str:
    expect(isinstance(raw, str) and raw, f"{label} must be a non-empty string")
    return raw


def require_bool(raw: Any, label: str) -> bool:
    expect(isinstance(raw, bool), f"{label} must be a bool")
    return raw


def require_list(raw: Any, label: str) -> list[Any]:
    expect(isinstance(raw, list), f"{label} must be a list")
    return raw


def build_regeneration_payload(test_binary: Path, root: Path) -> dict[str, Any]:
    source_payload = run_emitter(test_binary, root)
    validate_audit_trail_payload(source_payload)
    source_entries = require_list(source_payload.get("entries"), "source entries")

    by_label: dict[str, dict[str, Any]] = {}
    for index, raw_entry in enumerate(source_entries):
        expect(isinstance(raw_entry, dict), f"source entries[{index}] must be an object")
        label = require_string(raw_entry.get("label"), f"source entries[{index}].label")
        expect(label not in by_label, f"duplicate b61n audit label {label}")
        by_label[label] = raw_entry

    entries: list[dict[str, Any]] = []
    for label in EXPECTED_LABELS:
        expect(label in by_label, f"missing b61n audit label {label}")
        source_entry = by_label[label]
        audit = require_string(source_entry.get("audit"), f"{label}.audit")
        fields = parse_audit_text(audit)
        category = require_string(fields.get("kind"), f"{label}.audit kind")
        pin = EXPECTED_PINS[label]
        expect(
            category == pin["category"],
            f"{label} b61n audit category drifted: expected "
            f"{pin['category']}, got {category}",
        )

        fresh_fingerprint = compute_artifact_fingerprint(audit)
        emitted_fingerprint = require_string(
            source_entry.get("audit_fingerprint"),
            f"{label}.audit_fingerprint",
        )
        expect(
            emitted_fingerprint == fresh_fingerprint,
            f"{label} emitted audit fingerprint drifted from canonical audit text",
        )
        entries.append(
            {
                "label": label,
                "category": category,
                "fresh_fingerprint": fresh_fingerprint,
                "pinned_fingerprint": pin["pinned_fingerprint"],
                "matches_pin": fresh_fingerprint == pin["pinned_fingerprint"],
            }
        )

    return {
        "kind": EXPECTED_KIND,
        "entry_count": len(entries),
        "published_categories": EXPECTED_CATEGORIES,
        "entries": entries,
    }


def validate_payload(payload: dict[str, Any], *, require_matches: bool) -> dict[str, Any]:
    expect(payload.get("kind") == EXPECTED_KIND, "unexpected b61n fingerprint payload kind")
    categories = require_list(payload.get("published_categories"), "published_categories")
    expect(
        categories == EXPECTED_CATEGORIES,
        "published b61n publication audit categories drifted",
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
        fresh = require_string(
            raw_entry.get("fresh_fingerprint"),
            f"entries[{index}].fresh_fingerprint",
        )
        pinned = require_string(
            raw_entry.get("pinned_fingerprint"),
            f"entries[{index}].pinned_fingerprint",
        )
        matches = require_bool(raw_entry.get("matches_pin"), f"entries[{index}].matches_pin")
        expect(category in EXPECTED_CATEGORIES, f"{label} has unexpected category {category}")
        expect(fresh.startswith("fnv1a64:"), f"{label} fresh fingerprint must be fnv1a64")
        expect(pinned.startswith("fnv1a64:"), f"{label} pinned fingerprint must be fnv1a64")
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

    expect(labels == EXPECTED_LABELS, "b61n publication audit fingerprint labels drifted")
    expect(
        len(set(fresh_fingerprints)) == len(fresh_fingerprints),
        "fresh b61n publication audit fingerprints must remain unique",
    )
    if require_matches:
        expect(
            not mismatches,
            "b61n publication audit fingerprints drifted; run without "
            "--self-check to print fresh hashes",
        )
    return {"mismatches": mismatches, "entry_count": len(entries)}


def render_plain(payload: dict[str, Any]) -> str:
    lines = ["# fresh b61n publication audit fingerprints"]
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
            "Regenerate the b61n publication audit fingerprint hashes from "
            "the built singular-runtime-lane-tests binary."
        )
    )
    parser.add_argument(
        "--test-binary",
        help="Path to the built singular-runtime-lane-tests executable.",
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
    payload = build_regeneration_payload(test_binary, root)
    summary = validate_payload(payload, require_matches=args.self_check)

    if args.self_check:
        print(
            json.dumps(
                {
                    "kind": "b61n-publication-audit-fingerprint-self-check",
                    "entry_count": summary["entry_count"],
                    "passed": True,
                },
                indent=2,
                sort_keys=True,
            )
        )
        return 0

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
