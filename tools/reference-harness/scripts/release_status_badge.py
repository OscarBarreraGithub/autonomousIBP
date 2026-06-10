#!/usr/bin/env python3
"""Render a Shields-compatible M7 release status badge from release health."""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path
from typing import Any


HEALTH_SUMMARY_SCRIPT = Path("tools/reference-harness/scripts/release_health_summary.py")


class BadgeError(RuntimeError):
    """Raised when release health cannot be rendered as a badge."""


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def read_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as stream:
        payload = json.load(stream)
    if not isinstance(payload, dict):
        raise BadgeError(f"{path} must contain a JSON object")
    return payload


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise BadgeError(message)


def require_object(payload: dict[str, Any], field: str) -> dict[str, Any]:
    value = payload.get(field)
    expect(isinstance(value, dict), f"{field} must be an object")
    return value


def require_int(payload: dict[str, Any], field: str) -> int:
    value = payload.get(field)
    expect(type(value) is int and value >= 0, f"{field} must be a nonnegative integer")
    return value


def run_release_health(root: Path, *, verify: bool) -> dict[str, Any]:
    command = [
        sys.executable,
        str(root / HEALTH_SUMMARY_SCRIPT),
        "--format",
        "json",
    ]
    if verify:
        command.append("--verify")
    completed = subprocess.run(
        command,
        cwd=root,
        text=True,
        capture_output=True,
        check=False,
    )
    if completed.returncode != 0:
        details = completed.stderr.strip() or completed.stdout.strip()
        raise BadgeError(f"release health summary failed: {details}")
    try:
        payload = json.loads(completed.stdout)
    except json.JSONDecodeError as error:
        raise BadgeError(f"release health summary did not emit JSON: {error}") from error
    if not isinstance(payload, dict):
        raise BadgeError("release health summary JSON must be an object")
    return payload


def badge_payload(health: dict[str, Any]) -> dict[str, Any]:
    expect(health.get("schema_version") == 1, "release health schema_version must be 1")
    status = health.get("status")
    expect(status in {"ready", "blocked"}, "release health status must be ready or blocked")

    readiness = require_object(health, "readiness")
    inventory = require_object(health, "inventory")
    ready = readiness.get("release_signoff_ready")
    expect(isinstance(ready, bool), "readiness.release_signoff_ready must be boolean")
    blocker_count = require_int(readiness, "blocker_count")
    expected_status = "ready" if ready and blocker_count == 0 else "blocked"
    expect(
        status == expected_status,
        (
            "release health status is inconsistent with readiness fields: "
            f"status={status} expected={expected_status}"
        ),
    )
    sidecar_count = require_int(inventory, "sidecar_count")
    accepted_count = require_int(inventory, "accepted_count")
    unaccepted_count = require_int(inventory, "unaccepted_count")
    expect(accepted_count <= sidecar_count, "inventory.accepted_count exceeds sidecar_count")
    expect(
        accepted_count + unaccepted_count == sidecar_count,
        "inventory accepted/unaccepted counts do not sum to sidecar_count",
    )

    if status == "ready" and ready and blocker_count == 0:
        message = f"ready; {accepted_count}/{sidecar_count} accepted"
        color = "brightgreen"
    else:
        message = f"blocked; {blocker_count} blockers; {unaccepted_count} unaccepted"
        color = "red"

    return {
        "schemaVersion": 1,
        "label": "M7 release",
        "message": message,
        "color": color,
    }


def render_shields_json(payload: dict[str, Any]) -> str:
    return json.dumps(payload, indent=2, sort_keys=True)


def synthetic_health(
    *,
    status: str,
    ready: bool,
    blocker_count: int,
    sidecar_count: int,
    accepted_count: int,
    unaccepted_count: int,
) -> dict[str, Any]:
    return {
        "schema_version": 1,
        "status": status,
        "readiness": {
            "release_signoff_ready": ready,
            "blocker_count": blocker_count,
        },
        "inventory": {
            "sidecar_count": sidecar_count,
            "accepted_count": accepted_count,
            "unaccepted_count": unaccepted_count,
        },
    }


def expect_badge_error(label: str, health: dict[str, Any], expected: str) -> None:
    try:
        badge_payload(health)
    except BadgeError as error:
        expect(expected in str(error), f"{label} failed for the wrong reason: {error}")
        return
    raise BadgeError(f"{label} unexpectedly passed")


def self_check() -> None:
    ready_badge = badge_payload(
        synthetic_health(
            status="ready",
            ready=True,
            blocker_count=0,
            sidecar_count=77,
            accepted_count=56,
            unaccepted_count=21,
        )
    )
    expect(
        ready_badge
        == {
            "schemaVersion": 1,
            "label": "M7 release",
            "message": "ready; 56/77 accepted",
            "color": "brightgreen",
        },
        "ready badge payload drifted",
    )

    blocked_badge = badge_payload(
        synthetic_health(
            status="blocked",
            ready=False,
            blocker_count=2,
            sidecar_count=11,
            accepted_count=4,
            unaccepted_count=7,
        )
    )
    expect(
        blocked_badge
        == {
            "schemaVersion": 1,
            "label": "M7 release",
            "message": "blocked; 2 blockers; 7 unaccepted",
            "color": "red",
        },
        "blocked badge payload drifted",
    )

    expect_badge_error(
        "inconsistent inventory count check",
        synthetic_health(
            status="ready",
            ready=True,
            blocker_count=0,
            sidecar_count=10,
            accepted_count=8,
            unaccepted_count=1,
        ),
        "inventory accepted/unaccepted counts do not sum",
    )
    expect_badge_error(
        "status/readiness coherence check",
        synthetic_health(
            status="ready",
            ready=False,
            blocker_count=0,
            sidecar_count=10,
            accepted_count=8,
            unaccepted_count=2,
        ),
        "status is inconsistent with readiness fields",
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--health-json",
        type=Path,
        help="Existing release_health_summary.py JSON output to render instead of running it.",
    )
    parser.add_argument(
        "--format",
        choices=("shields-json",),
        default="shields-json",
        help="Badge output format.",
    )
    parser.add_argument(
        "--verify",
        action="store_true",
        help="Pass --verify through to release_health_summary.py when reading live health.",
    )
    parser.add_argument(
        "--self-check",
        action="store_true",
        help="Run synthetic badge rendering checks.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        if args.self_check:
            self_check()
            print("M7 release status badge self-check passed")
            return 0

        root = repo_root()
        if args.health_json is None:
            health = run_release_health(root, verify=args.verify)
        else:
            health_path = args.health_json
            if not health_path.is_absolute():
                health_path = root / health_path
            health = read_json(health_path)

        print(render_shields_json(badge_payload(health)))
        return 0
    except BadgeError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
