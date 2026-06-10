#!/usr/bin/env python3
"""Verify one b61n publication audit JSON field addressed by path."""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path
from typing import Any

from audit_b61n_publication_qualifier import (
    audit_sidecar,
    default_publication_qualifier_sidecar_path,
)


_UNSET = object()


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def load_json(path: Path) -> Any:
    with path.open("r", encoding="utf-8") as handle:
        return json.load(handle)


def write_json(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def parse_field_path(path: str) -> list[str | int]:
    expect(path.strip() == path and path, "field path must be a non-empty trimmed string")
    tokens: list[str | int] = []
    for segment in path.split("."):
        expect(segment, f"field path {path!r} contains an empty segment")
        cursor = 0
        if segment[0] != "[":
            while cursor < len(segment) and segment[cursor] != "[":
                cursor += 1
            key = segment[:cursor]
            expect(key, f"field path {path!r} contains an empty object key")
            tokens.append(key)
        while cursor < len(segment):
            expect(segment[cursor] == "[", f"field path segment {segment!r} is malformed")
            close = segment.find("]", cursor + 1)
            expect(close != -1, f"field path segment {segment!r} is missing ']'")
            raw_index = segment[cursor + 1 : close]
            expect(raw_index.isdigit(), f"field path segment {segment!r} has a non-numeric index")
            tokens.append(int(raw_index))
            cursor = close + 1
    return tokens


def format_path(tokens: list[str | int]) -> str:
    text = ""
    for token in tokens:
        if isinstance(token, str):
            text = token if not text else f"{text}.{token}"
        else:
            text = f"{text}[{token}]"
    return text


def query_field(payload: Any, field_path: str) -> Any:
    tokens = parse_field_path(field_path)
    current = payload
    traversed: list[str | int] = []
    for token in tokens:
        parent_path = format_path(traversed) or "<root>"
        if isinstance(token, str):
            if not isinstance(current, dict):
                raise RuntimeError(
                    f"cannot read key {token!r} below {parent_path}: "
                    f"found {type(current).__name__}"
                )
            if token not in current:
                raise RuntimeError(f"missing object key {token!r} below {parent_path}")
            current = current[token]
        else:
            if not isinstance(current, list):
                raise RuntimeError(
                    f"cannot read index {token} below {parent_path}: "
                    f"found {type(current).__name__}"
                )
            if token >= len(current):
                raise RuntimeError(
                    f"array index {token} below {parent_path} is out of range "
                    f"for length {len(current)}"
                )
            current = current[token]
        traversed.append(token)
    return current


def audit_payload(args: argparse.Namespace) -> tuple[dict[str, Any], str]:
    if args.audit_json is not None:
        payload = load_json(args.audit_json)
        expect(isinstance(payload, dict), "audit JSON root must be an object")
        return payload, args.audit_json.as_posix()

    sidecar_path = (
        args.sidecar_path
        if args.sidecar_path is not None
        else default_publication_qualifier_sidecar_path()
    )
    return audit_sidecar(sidecar_path), sidecar_path.as_posix()


def verify_field(
    payload: dict[str, Any],
    *,
    field_path: str,
    expected_value: Any = _UNSET,
    source: str,
) -> dict[str, Any]:
    value = query_field(payload, field_path)
    matched = expected_value is _UNSET or value == expected_value
    if expected_value is not _UNSET:
        expect(
            matched,
            f"field {field_path!r} value mismatch: expected {expected_value!r}, got {value!r}",
        )
    return {
        "schema_version": 1,
        "verifier": "b61n-publication-audit-field-path-v1",
        "audit_query_passed": True,
        "audit_json_source": source,
        "field_path": field_path,
        "field_value": value,
        "expected_value": None if expected_value is _UNSET else expected_value,
        "expected_value_checked": expected_value is not _UNSET,
        "m6_closure_claimed": False,
        "m7_closure_claimed": False,
        "release_readiness_claimed": False,
    }


def rejected(payload: dict[str, Any], field_path: str, message: str, expected: Any = _UNSET) -> bool:
    try:
        verify_field(payload, field_path=field_path, expected_value=expected, source="self-check")
    except Exception as error:  # noqa: BLE001 - self-check intentionally probes failure paths.
        return message in str(error)
    return False


def run_self_check() -> dict[str, Any]:
    payload = audit_sidecar(default_publication_qualifier_sidecar_path())
    contour = verify_field(
        payload,
        field_path="source_contour_fingerprint",
        expected_value="fnv1a64:a8dd3d0427fbf52b",
        source="default-publication-sidecar",
    )
    first_endpoint = verify_field(
        payload,
        field_path="reviewed_endpoint_integrals[0]",
        expected_value=payload["reviewed_endpoint_integrals"][0],
        source="default-publication-sidecar",
    )
    blocked_gate = verify_field(
        payload,
        field_path="amflow_cross_comparator_publication_gate_passed",
        expected_value=False,
        source="default-publication-sidecar",
    )
    missing_key_rejected = rejected(
        payload,
        "precision_evidence_source_cpp_result.missing",
        "cannot read key",
    )
    out_of_range_rejected = rejected(
        payload,
        "reviewed_endpoint_integrals[99]",
        "out of range",
    )
    wrong_value_rejected = rejected(
        payload,
        "amflow_cross_comparator_publication_gate_passed",
        "value mismatch",
        expected=True,
    )
    checks = {
        "queries_string_field": contour["field_value"] == "fnv1a64:a8dd3d0427fbf52b",
        "queries_list_index": first_endpoint["field_value"]
        == payload["reviewed_endpoint_integrals"][0],
        "queries_bool_field": blocked_gate["field_value"] is False,
        "rejects_missing_nested_key": missing_key_rejected,
        "rejects_out_of_range_index": out_of_range_rejected,
        "rejects_wrong_expected_value": wrong_value_rejected,
    }
    expect(all(checks.values()), "b61n publication audit field verifier self-check failed")
    return {
        "schema_version": 1,
        "verifier": "b61n-publication-audit-field-path-v1",
        "self_check_passed": True,
        "audit_query_passed": True,
        "queried_paths": [
            contour["field_path"],
            first_endpoint["field_path"],
            blocked_gate["field_path"],
        ],
        "checks": checks,
        "m6_closure_claimed": False,
        "m7_closure_claimed": False,
        "release_readiness_claimed": False,
    }


def parse_expected_value(args: argparse.Namespace) -> Any:
    if args.expect_json is not None:
        try:
            return json.loads(args.expect_json)
        except json.JSONDecodeError as error:
            raise RuntimeError(f"--expect-json is not valid JSON: {error}") from error
    if args.expect_string is not None:
        return args.expect_string
    return _UNSET


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--audit-json",
        type=Path,
        help="Existing b61n publication audit summary JSON. Defaults to generating one.",
    )
    parser.add_argument(
        "--sidecar-path",
        type=Path,
        help="Publication qualifier sidecar used when --audit-json is omitted.",
    )
    parser.add_argument("--field-path", help="Dotted/bracket path, for example a.b[0].c")
    expected = parser.add_mutually_exclusive_group()
    expected.add_argument("--expect-json", help="Expected field value as a JSON literal")
    expected.add_argument("--expect-string", help="Expected field value as an exact string")
    parser.add_argument("--summary-path", type=Path, help="Optional verifier summary path")
    parser.add_argument(
        "--self-check",
        action="store_true",
        help="Run accepted and rejected path-query checks against the default b61n audit.",
    )
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    try:
        if args.self_check:
            summary = run_self_check()
        else:
            expect(args.field_path is not None, "--field-path is required outside --self-check")
            payload, source = audit_payload(args)
            summary = verify_field(
                payload,
                field_path=args.field_path,
                expected_value=parse_expected_value(args),
                source=source,
            )
        if args.summary_path is not None:
            write_json(args.summary_path, summary)
        print(json.dumps(summary, indent=2, sort_keys=True))
        return 0
    except Exception as error:  # noqa: BLE001 - command-line verifier should fail closed.
        summary = {
            "schema_version": 1,
            "verifier": "b61n-publication-audit-field-path-v1",
            "audit_query_passed": False,
            "blocking_reasons": [str(error)],
            "m6_closure_claimed": False,
            "m7_closure_claimed": False,
            "release_readiness_claimed": False,
        }
        if args.summary_path is not None:
            write_json(args.summary_path, summary)
        print(json.dumps(summary, indent=2, sort_keys=True))
        return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
