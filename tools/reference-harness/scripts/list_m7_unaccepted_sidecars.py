#!/usr/bin/env python3
"""Print the committed M7 unaccepted-sidecar review queue."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Callable

from audit_m7_sidecar_inventory import (
    InventoryEntry,
    InventoryError,
    build_inventory,
    repo_root,
)
from validate_m7_release_sidecar_schemas import M7_ROOT, SchemaError


class QueueError(RuntimeError):
    """Raised when the unaccepted sidecar queue is inconsistent."""


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise QueueError(message)


def unaccepted_entries(entries: list[InventoryEntry]) -> list[InventoryEntry]:
    return [entry for entry in entries if entry.status == "unaccepted"]


def render_text(entries: list[InventoryEntry], *, summary_only: bool) -> str:
    lines = [f"M7 unaccepted sidecar queue: count={len(entries)}"]
    if summary_only:
        return "\n".join(lines)
    for entry in entries:
        lines.append(f"{entry.schema:64} {entry.path} ({entry.basis})")
    return "\n".join(lines)


def render_json(entries: list[InventoryEntry]) -> str:
    payload = {
        "schema_version": 1,
        "queue_count": len(entries),
        "review_queue": [entry.__dict__ for entry in entries],
    }
    return json.dumps(payload, indent=2, sort_keys=True)


def verify_queue(all_entries: list[InventoryEntry], queue: list[InventoryEntry]) -> None:
    queued_paths = [entry.path for entry in queue]
    if len(queued_paths) != len(set(queued_paths)):
        raise QueueError("unaccepted sidecar queue must not contain duplicate paths")
    if any(entry.status != "unaccepted" for entry in queue):
        raise QueueError("unaccepted sidecar queue included an accepted sidecar")
    expected = sum(1 for entry in all_entries if entry.status == "unaccepted")
    if len(queue) != expected:
        raise QueueError(
            "unaccepted sidecar queue count mismatch: "
            f"queue={len(queue)} inventory={expected}"
        )


def expect_queue_error(label: str, expected: str, action: Callable[[], None]) -> None:
    try:
        action()
    except (InventoryError, QueueError, SchemaError) as error:
        expect(expected in str(error), f"{label} failed for the wrong reason: {error}")
        return
    raise QueueError(f"{label} unexpectedly passed")


def self_check(root: Path) -> None:
    entries = build_inventory(root, M7_ROOT)
    queue = unaccepted_entries(entries)
    verify_queue(entries, queue)

    summary = render_text(queue, summary_only=True)
    expect(
        summary == f"M7 unaccepted sidecar queue: count={len(queue)}",
        "summary-only queue text drifted",
    )
    text_lines = render_text(queue, summary_only=False).splitlines()
    expect(text_lines[:1] == [summary], "queue text header drifted")
    expect(
        len(text_lines) == len(queue) + 1,
        "queue text must include one row per unaccepted sidecar",
    )

    payload = json.loads(render_json(queue))
    expect(payload.get("schema_version") == 1, "queue JSON schema_version must be 1")
    expect(payload.get("queue_count") == len(queue), "queue JSON count mismatch")
    review_queue = payload.get("review_queue")
    expect(isinstance(review_queue, list), "queue JSON review_queue must be a list")
    expect(len(review_queue) == len(queue), "queue JSON review_queue length mismatch")
    expect(
        all(isinstance(item, dict) for item in review_queue),
        "queue JSON review_queue entries must be objects",
    )
    expect(
        all(item.get("status") == "unaccepted" for item in review_queue),
        "queue JSON included an accepted sidecar",
    )

    accepted = next((entry for entry in entries if entry.status == "accepted"), None)
    expect(accepted is not None, "self-check requires at least one accepted sidecar")
    expect(queue, "self-check requires at least one unaccepted sidecar")
    expect_queue_error(
        "duplicate queue path guard",
        "duplicate paths",
        lambda: verify_queue(entries, [queue[0], queue[0]]),
    )
    expect_queue_error(
        "accepted sidecar guard",
        "included an accepted sidecar",
        lambda: verify_queue(entries, [*queue, accepted]),
    )
    expect_queue_error(
        "queue count guard",
        "count mismatch",
        lambda: verify_queue(entries, queue[:-1]),
    )
    expect_queue_error(
        "absolute m7 root guard",
        "m7 root must be repository-relative",
        lambda: build_inventory(root, root / M7_ROOT),
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--m7-root",
        default=str(M7_ROOT),
        help="Repository-relative M7 sidecar root to inventory.",
    )
    parser.add_argument(
        "--format",
        choices=("text", "json"),
        default="text",
        help="Output format.",
    )
    parser.add_argument(
        "--summary-only",
        action="store_true",
        help="Only print the queue count in text mode.",
    )
    parser.add_argument(
        "--verify",
        action="store_true",
        help="Fail if the queue no longer matches the unaccepted inventory subset.",
    )
    parser.add_argument(
        "--self-check",
        action="store_true",
        help="Run positive and negative checks for unaccepted queue invariants.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    root = repo_root()
    try:
        if args.self_check:
            self_check(root)
            print("M7 unaccepted sidecar queue self-check passed")
            return 0
        entries = build_inventory(root, Path(args.m7_root))
        queue = unaccepted_entries(entries)
        if args.verify:
            verify_queue(entries, queue)
    except (InventoryError, QueueError, SchemaError) as error:
        print(str(error), file=sys.stderr)
        return 1

    if args.format == "json":
        print(render_json(queue))
    else:
        print(render_text(queue, summary_only=args.summary_only))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
