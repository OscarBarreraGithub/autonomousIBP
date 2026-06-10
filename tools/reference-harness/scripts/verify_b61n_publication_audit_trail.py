#!/usr/bin/env python3
"""Verify that the b61n publication audit trail is queryable from C++."""

from __future__ import annotations

import argparse
import copy
import json
import os
import subprocess
import sys
from pathlib import Path
from typing import Any


EMIT_FLAG = "--emit-b61n-publication-audit-trail"
EXPECTED_KIND = "b61n-publication-audit-trail-query"
EXPECTED_LABELS = [
    "published-lane142-primitive-bubble",
    "blocked-off-axis-publication-contour",
]
EXPECTED_AUDIT_KEYS = [
    "kind",
    "success",
    "endpoint_integral_id",
    "matrix_fingerprint",
    "contour_fingerprint",
    "endpoint_local_model_kind",
    "transport_scope",
    "ode_propagation_applied",
    "coefficient_publication",
    "endpoint_extraction_applied",
    "retained_solution_samples_used",
    "full_eta_zero_contour_applied",
    "eta_zero_endpoint_reached",
    "failure_code",
    "summary",
]
EXPECTED_ENDPOINT = "box[1,0,1,0]"
EXPECTED_MATRIX_FINGERPRINT = "lane142-b61n-selected5-primitive-bubble-v1"
EXPECTED_LOCAL_MODEL = "b61n-primitive-bubble-regular-taylor-r0"
FNV1A64_OFFSET = 14695981039346656037
FNV1A64_PRIME = 1099511628211
FNV1A64_MASK = (1 << 64) - 1


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise RuntimeError(message)


def compute_artifact_fingerprint(text: str) -> str:
    value = FNV1A64_OFFSET
    for byte in text.encode("utf-8"):
        value ^= byte
        value = (value * FNV1A64_PRIME) & FNV1A64_MASK
    return f"fnv1a64:{value:016x}"


def common_binary_candidates(root: Path) -> list[Path]:
    return [
        root / "build" / "singular-runtime-lane-tests",
        root / "build-debug" / "singular-runtime-lane-tests",
        root / "cmake-build-debug" / "singular-runtime-lane-tests",
        root / "cmake-build-release" / "singular-runtime-lane-tests",
        root / "out" / "build" / "singular-runtime-lane-tests",
    ]


def resolve_test_binary(raw_path: str | None, root: Path) -> Path:
    candidates: list[Path] = []
    if raw_path:
        candidates.append(Path(raw_path))
    env_path = os.environ.get("B61N_SINGULAR_RUNTIME_TEST_BINARY")
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
        "unable to find singular-runtime-lane-tests; pass --test-binary. checked: "
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
    expect(isinstance(payload, dict), "b61n audit emitter must return a JSON object")
    return payload


def require_string(raw: Any, label: str) -> str:
    expect(isinstance(raw, str) and raw, f"{label} must be a non-empty string")
    return raw


def require_optional_string(raw: Any, label: str) -> str:
    expect(isinstance(raw, str), f"{label} must be a string")
    return raw


def require_bool(raw: Any, label: str) -> bool:
    expect(isinstance(raw, bool), f"{label} must be a bool")
    return raw


def require_int(raw: Any, label: str) -> int:
    expect(isinstance(raw, int) and not isinstance(raw, bool), f"{label} must be an int")
    return raw


def require_list(raw: Any, label: str) -> list[Any]:
    expect(isinstance(raw, list), f"{label} must be a list")
    return raw


def require_entry_field(entry: dict[str, Any], key: str, label: str) -> Any:
    expect(key in entry, f"{label} is missing")
    return entry[key]


def parse_audit_text(text: str) -> dict[str, str]:
    fields: dict[str, str] = {}
    for line_number, raw_line in enumerate(text.splitlines(), start=1):
        line = raw_line.strip()
        if not line:
            continue
        expect("=" in line, f"audit line {line_number} must be key=value")
        key, value = line.split("=", 1)
        expect(key, f"audit line {line_number} has an empty key")
        expect(key not in fields, f"audit repeats key {key}")
        fields[key] = value
    return fields


def require_field(fields: dict[str, str], key: str) -> str:
    expect(key in fields, f"audit missing {key}")
    return fields[key]


def bool_text(value: bool) -> str:
    return "true" if value else "false"


def validate_entry(raw_entry: Any, index: int) -> dict[str, Any]:
    expect(isinstance(raw_entry, dict), f"entries[{index}] must be an object")
    label = require_string(
        require_entry_field(raw_entry, "label", f"entries[{index}].label"),
        f"entries[{index}].label",
    )
    endpoint_integral_id = require_string(
        require_entry_field(
            raw_entry,
            "endpoint_integral_id",
            f"entries[{index}].endpoint_integral_id",
        ),
        f"entries[{index}].endpoint_integral_id",
    )
    matrix_fingerprint = require_string(
        require_entry_field(
            raw_entry,
            "matrix_fingerprint",
            f"entries[{index}].matrix_fingerprint",
        ),
        f"entries[{index}].matrix_fingerprint",
    )
    contour_fingerprint = require_string(
        require_entry_field(
            raw_entry,
            "contour_fingerprint",
            f"entries[{index}].contour_fingerprint",
        ),
        f"entries[{index}].contour_fingerprint",
    )
    endpoint_local_model_kind = require_string(
        require_entry_field(
            raw_entry,
            "endpoint_local_model_kind",
            f"entries[{index}].endpoint_local_model_kind",
        ),
        f"entries[{index}].endpoint_local_model_kind",
    )
    transport_scope = require_string(
        require_entry_field(
            raw_entry,
            "transport_scope",
            f"entries[{index}].transport_scope",
        ),
        f"entries[{index}].transport_scope",
    )
    ode_propagation_applied = require_bool(
        require_entry_field(
            raw_entry,
            "ode_propagation_applied",
            f"entries[{index}].ode_propagation_applied",
        ),
        f"entries[{index}].ode_propagation_applied",
    )
    coefficient_publication = require_bool(
        require_entry_field(
            raw_entry,
            "coefficient_publication",
            f"entries[{index}].coefficient_publication",
        ),
        f"entries[{index}].coefficient_publication",
    )
    endpoint_extraction_applied = require_bool(
        require_entry_field(
            raw_entry,
            "endpoint_extraction_applied",
            f"entries[{index}].endpoint_extraction_applied",
        ),
        f"entries[{index}].endpoint_extraction_applied",
    )
    retained_solution_samples_used = require_bool(
        require_entry_field(
            raw_entry,
            "retained_solution_samples_used",
            f"entries[{index}].retained_solution_samples_used",
        ),
        f"entries[{index}].retained_solution_samples_used",
    )
    full_eta_zero_contour_applied = require_bool(
        require_entry_field(
            raw_entry,
            "full_eta_zero_contour_applied",
            f"entries[{index}].full_eta_zero_contour_applied",
        ),
        f"entries[{index}].full_eta_zero_contour_applied",
    )
    eta_zero_endpoint_reached = require_bool(
        require_entry_field(
            raw_entry,
            "eta_zero_endpoint_reached",
            f"entries[{index}].eta_zero_endpoint_reached",
        ),
        f"entries[{index}].eta_zero_endpoint_reached",
    )
    failure_code = require_optional_string(
        require_entry_field(raw_entry, "failure_code", f"entries[{index}].failure_code"),
        f"entries[{index}].failure_code",
    )
    fingerprint = require_string(
        require_entry_field(
            raw_entry,
            "audit_fingerprint",
            f"entries[{index}].audit_fingerprint",
        ),
        f"entries[{index}].audit_fingerprint",
    )
    audit = require_string(
        require_entry_field(raw_entry, "audit", f"entries[{index}].audit"),
        f"entries[{index}].audit",
    )

    expect(endpoint_integral_id == EXPECTED_ENDPOINT, f"{label} endpoint drifted")
    expect(
        matrix_fingerprint == EXPECTED_MATRIX_FINGERPRINT,
        f"{label} matrix fingerprint drifted",
    )
    expect(
        endpoint_local_model_kind == EXPECTED_LOCAL_MODEL,
        f"{label} endpoint local model drifted",
    )
    expect(contour_fingerprint.startswith("fnv1a64:"), f"{label} contour must be fingerprinted")
    expect(fingerprint.startswith("fnv1a64:"), f"{label} audit fingerprint must be fnv1a64")
    expected_fingerprint = compute_artifact_fingerprint(audit)
    expect(
        fingerprint == expected_fingerprint,
        f"{label} audit fingerprint must match audit text: "
        f"expected {expected_fingerprint}, got {fingerprint}",
    )

    fields = parse_audit_text(audit)
    actual_keys = list(fields)
    expect(
        actual_keys == EXPECTED_AUDIT_KEYS,
        f"{label} audit keys must remain {EXPECTED_AUDIT_KEYS!r}, got {actual_keys!r}",
    )
    exact_fields = {
        "kind": "b61n-publication-contour-evaluation",
        "success": "true",
        "endpoint_integral_id": endpoint_integral_id,
        "matrix_fingerprint": matrix_fingerprint,
        "contour_fingerprint": contour_fingerprint,
        "endpoint_local_model_kind": endpoint_local_model_kind,
        "transport_scope": transport_scope,
        "ode_propagation_applied": bool_text(ode_propagation_applied),
        "coefficient_publication": bool_text(coefficient_publication),
        "endpoint_extraction_applied": bool_text(endpoint_extraction_applied),
        "retained_solution_samples_used": bool_text(retained_solution_samples_used),
        "full_eta_zero_contour_applied": bool_text(full_eta_zero_contour_applied),
        "eta_zero_endpoint_reached": bool_text(eta_zero_endpoint_reached),
        "failure_code": failure_code,
    }
    for key, expected in exact_fields.items():
        actual = require_field(fields, key)
        expect(actual == expected, f"{label} audit {key} must be {expected!r}, got {actual!r}")

    summary = require_field(fields, "summary")
    for required_text in (
        f"endpoint_integral_id={EXPECTED_ENDPOINT}",
        f"matrix_fingerprint={EXPECTED_MATRIX_FINGERPRINT}",
        f"endpoint_local_model_kind={EXPECTED_LOCAL_MODEL}",
        f"coefficient_publication={bool_text(coefficient_publication)}",
        f"endpoint_extraction_applied={bool_text(endpoint_extraction_applied)}",
        "final_solution_samples_used_as_input=false",
        "full_eta_zero_contour_applied=false",
    ):
        expect(required_text in summary, f"{label} summary must include {required_text!r}")

    expect(ode_propagation_applied, f"{label} must come from live ODE propagation")
    expect(not retained_solution_samples_used, f"{label} must not consume retained final samples")
    expect(not full_eta_zero_contour_applied, f"{label} must not claim full eta-zero contour")
    expect(eta_zero_endpoint_reached, f"{label} must reach eta=0 before publication auditing")
    expect(failure_code == "", f"{label} publication audit fixture should not be a runtime failure")

    if label == "published-lane142-primitive-bubble":
        expect(coefficient_publication, "published b61n entry must pass coefficient publication")
        expect(endpoint_extraction_applied, "published b61n entry must mark endpoint extraction")
    elif label == "blocked-off-axis-publication-contour":
        expect(not coefficient_publication, "blocked b61n entry must withhold publication")
        expect(
            not endpoint_extraction_applied,
            "blocked b61n entry must withhold endpoint extraction",
        )
        expect(
            "coefficient_publication=false" in summary,
            "blocked b61n entry must expose the publication gate result",
        )
    else:
        raise RuntimeError(f"unexpected b61n audit label {label}")

    return {
        "label": label,
        "coefficient_publication": coefficient_publication,
        "audit_fingerprint": fingerprint,
        "contour_fingerprint": contour_fingerprint,
        "audit_keys": actual_keys,
    }


def validate_payload(payload: dict[str, Any]) -> dict[str, Any]:
    expect(payload.get("kind") == EXPECTED_KIND, "unexpected b61n audit payload kind")
    entries = require_list(payload.get("entries"), "entries")
    expect(require_int(payload.get("entry_count"), "entry_count") == len(entries), "entry_count must match entries")
    validated = [validate_entry(raw_entry, index) for index, raw_entry in enumerate(entries)]
    labels = [entry["label"] for entry in validated]
    expect(labels == EXPECTED_LABELS, "b61n audit entries must remain in published,blocked order")
    expect(
        len({entry["audit_fingerprint"] for entry in validated}) == len(validated),
        "b61n audit fingerprints must remain distinct",
    )
    expect(
        len({entry["contour_fingerprint"] for entry in validated}) == len(validated),
        "b61n contour fingerprints must distinguish published and off-axis probes",
    )
    return {
        "schema_version": 1,
        "verifier": "b61n-publication-audit-trail-query-v1",
        "publication_audit_trail_query_passed": True,
        "entry_count": len(validated),
        "queried_labels": labels,
        "audit_keys_by_label": {
            entry["label"]: entry["audit_keys"] for entry in validated
        },
        "published_entry_count": sum(
            1 for entry in validated if entry["coefficient_publication"]
        ),
        "m6_closure_claimed": False,
        "m7_closure_claimed": False,
        "release_readiness_claimed": False,
    }


def replace_audit_line(audit: str, prefix: str, replacement: str | None) -> str:
    lines = audit.splitlines()
    replaced = False
    output: list[str] = []
    for line in lines:
        if line.startswith(prefix):
            replaced = True
            if replacement is not None:
                output.append(replacement)
        else:
            output.append(line)
    expect(replaced, f"synthetic mutation could not find audit line {prefix}")
    return "\n".join(output) + "\n"


def replace_entry_audit_line(
    entry: dict[str, Any],
    prefix: str,
    replacement: str | None,
) -> None:
    audit = replace_audit_line(entry["audit"], prefix, replacement)
    entry["audit"] = audit
    entry["audit_fingerprint"] = compute_artifact_fingerprint(audit)


def rejected(payload: dict[str, Any], expected_message: str) -> bool:
    try:
        validate_payload(payload)
    except Exception as error:  # noqa: BLE001 - self-check intentionally probes failure paths.
        return expected_message in str(error)
    return False


def run_self_check(payload: dict[str, Any]) -> dict[str, Any]:
    accepted = validate_payload(payload)
    missing_publication_flag = copy.deepcopy(payload)
    del missing_publication_flag["entries"][0]["coefficient_publication"]
    stale_matrix = copy.deepcopy(payload)
    stale_matrix["entries"][0]["matrix_fingerprint"] = "synthetic-stale-b61n-matrix"
    full_contour_overclaim = copy.deepcopy(payload)
    full_contour_overclaim["entries"][0]["full_eta_zero_contour_applied"] = True
    replace_entry_audit_line(
        full_contour_overclaim["entries"][0],
        "full_eta_zero_contour_applied=",
        "full_eta_zero_contour_applied=true",
    )
    missing_endpoint_key = copy.deepcopy(payload)
    replace_entry_audit_line(
        missing_endpoint_key["entries"][0],
        "endpoint_integral_id=",
        None,
    )
    missing_summary = copy.deepcopy(payload)
    replace_entry_audit_line(missing_summary["entries"][1], "summary=", None)
    stale_audit_fingerprint = copy.deepcopy(payload)
    stale_audit_fingerprint["entries"][1]["audit_fingerprint"] = (
        "fnv1a64:0000000000000000"
    )
    checks = {
        "accepts_runtime_publication_audit_emitter": accepted[
            "publication_audit_trail_query_passed"
        ],
        "rejects_missing_entry_publication_flag": rejected(
            missing_publication_flag,
            "entries[0].coefficient_publication is missing",
        ),
        "rejects_stale_matrix_fingerprint": rejected(
            stale_matrix,
            "matrix fingerprint drifted",
        ),
        "rejects_full_contour_overclaim": rejected(
            full_contour_overclaim,
            "must not claim full eta-zero contour",
        ),
        "rejects_missing_expected_audit_key": rejected(
            missing_endpoint_key,
            "audit keys must remain",
        ),
        "rejects_missing_summary_key": rejected(
            missing_summary,
            "audit keys must remain",
        ),
        "rejects_stale_audit_fingerprint": rejected(
            stale_audit_fingerprint,
            "audit fingerprint must match audit text",
        ),
    }
    expect(all(checks.values()), "b61n publication audit trail verifier self-check failed")
    accepted["self_check_passed"] = True
    accepted["checks"] = checks
    return accepted


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--test-binary",
        help="Path to the built singular-runtime-lane-tests executable.",
    )
    parser.add_argument("--summary-path", type=Path, help="Optional JSON output path")
    parser.add_argument(
        "--self-check",
        action="store_true",
        help="Also probe fail-closed mutations after querying the runtime emitter.",
    )
    return parser.parse_args(argv)


def write_json(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def print_json(payload: dict[str, Any]) -> None:
    print(json.dumps(payload, indent=2, sort_keys=True))


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    try:
        root = repo_root()
        test_binary = resolve_test_binary(args.test_binary, root)
        payload = run_emitter(test_binary, root)
        summary = run_self_check(payload) if args.self_check else validate_payload(payload)
        if args.summary_path is not None:
            write_json(args.summary_path, summary)
        print_json(summary)
        return 0
    except Exception as error:  # noqa: BLE001 - command-line verifier should fail closed.
        summary = {
            "schema_version": 1,
            "verifier": "b61n-publication-audit-trail-query-v1",
            "publication_audit_trail_query_passed": False,
            "blocking_reasons": [str(error)],
            "m6_closure_claimed": False,
            "m7_closure_claimed": False,
            "release_readiness_claimed": False,
        }
        if args.summary_path is not None:
            write_json(args.summary_path, summary)
        print_json(summary)
        return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
