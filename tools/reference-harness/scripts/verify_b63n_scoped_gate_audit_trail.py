#!/usr/bin/env python3
"""Verify that the b63n scoped gate audit trail is queryable from Python."""

from __future__ import annotations

import argparse
import copy
import json
import os
import subprocess
import sys
from pathlib import Path
from typing import Any


EMIT_FLAG = "--emit-b63n-scoped-gate-audit-trail"
EXPECTED_KIND = "b63n-scoped-gate-audit-trail-query"
EXPECTED_LABELS = [
    "blocked-D2-scoped-weighted-residue",
    "published-D7-scoped-weighted-residue",
]
EXPECTED_SEED_DENOMINATOR_IDENTITIES = [
    "D2;index=1;power=2;role=D2=(l1+p1)^2 angular weight with power 2;form=inverse_denominator_weight[D2(q2,cos_theta_a)]",
    "D4;index=3;power=1;role=D4=(l1+l2+p1)^2 angular weight;form=inverse_denominator_weight[D4(q2,cos_theta_a,cos_theta_b)]",
    "D6;index=5;power=1;role=D6=(l1+l2+p2)^2 angular weight;form=inverse_denominator_weight[D6(q2,cos_theta_a,cos_theta_b)]",
    "D7;index=6;power=1;role=D7=(l1+p2)^2 angular weight;form=inverse_denominator_weight[D7(q2,cos_theta_a)]",
]
EXPECTED_SEED_PROVENANCE = [
    "D2;series=automatic_phasespace::weighted-moment-seed::D2::prefactor;eta_zero_label=automatic_phasespace_D2_weighted_moment_seed;coefficient_label=automatic_phasespace_D2_weighted_moment_seed;source=reviewed automatic_phasespace symbolic integrand; not AMFlow final solution samples;fixture=lane3-next2-automatic-phasespace-D2-weighted-moment-seed;synthetic=true;retained_solution_samples_used=false;coefficient_published=false",
    "D4;series=automatic_phasespace::weighted-moment-seed::D4::prefactor;eta_zero_label=automatic_phasespace_D4_weighted_moment_seed;coefficient_label=automatic_phasespace_D4_weighted_moment_seed;source=reviewed automatic_phasespace symbolic integrand; not AMFlow final solution samples;fixture=lane3-next2-automatic-phasespace-D4-weighted-moment-seed;synthetic=true;retained_solution_samples_used=false;coefficient_published=false",
    "D6;series=automatic_phasespace::weighted-moment-seed::D6::prefactor;eta_zero_label=automatic_phasespace_D6_weighted_moment_seed;coefficient_label=automatic_phasespace_D6_weighted_moment_seed;source=reviewed automatic_phasespace symbolic integrand; not AMFlow final solution samples;fixture=lane3-next2-automatic-phasespace-D6-weighted-moment-seed;synthetic=true;retained_solution_samples_used=false;coefficient_published=false",
    "D7;series=automatic_phasespace::weighted-moment-seed::D7::prefactor;eta_zero_label=automatic_phasespace_D7_weighted_moment_seed;coefficient_label=automatic_phasespace_D7_weighted_moment_seed;source=reviewed automatic_phasespace symbolic integrand; not AMFlow final solution samples;fixture=lane3-next2-automatic-phasespace-D7-weighted-moment-seed;synthetic=true;retained_solution_samples_used=false;coefficient_published=false",
]
EXPECTED_CANDIDATE_PROVENANCE = {
    "blocked-D2-scoped-weighted-residue": "D2;series=automatic_phasespace::weighted-moment-seed::D2::prefactor;eta_zero_label=automatic_phasespace_D2_weighted_moment_seed;coefficient_label=automatic_phasespace_D2_weighted_moment_seed;source=reviewed automatic_phasespace symbolic integrand; not AMFlow final solution samples;fixture=lane3-next2-automatic-phasespace-D2-weighted-moment-seed;synthetic=true;retained_solution_samples_used=false;coefficient_published=false",
    "published-D7-scoped-weighted-residue": "D7;series=automatic_phasespace::reviewed-weighted-residue::D7::eps0-eps3;eta_zero_label=automatic_phasespace_D7_weighted_residue_eps0;coefficient_label=automatic_phasespace_D7_weighted_residue_eps0;source=tools/reference-harness/specs/m6/lane146/automatic_phasespace.selected4-cutkosky.compare30.json;fixture=lane146-reviewed-automatic-phasespace-D7-weighted-residue-eps0;synthetic=false;retained_solution_samples_used=false;coefficient_published=true",
}
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
    expect(isinstance(payload, dict), "scoped gate emitter must return a JSON object")
    return payload


def require_string(raw: Any, label: str) -> str:
    expect(isinstance(raw, str) and raw, f"{label} must be a non-empty string")
    return raw


def require_entry_field(entry: dict[str, Any], key: str, label: str) -> Any:
    expect(key in entry, f"{label} is missing")
    return entry[key]


def require_bool(raw: Any, label: str) -> bool:
    expect(isinstance(raw, bool), f"{label} must be a bool")
    return raw


def require_int(raw: Any, label: str) -> int:
    expect(isinstance(raw, int) and not isinstance(raw, bool), f"{label} must be an int")
    return raw


def require_list(raw: Any, label: str) -> list[Any]:
    expect(isinstance(raw, list), f"{label} must be a list")
    return raw


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
    expect(
        label in EXPECTED_CANDIDATE_PROVENANCE,
        f"unexpected scoped gate label {label}",
    )
    selected_weight = require_string(
        require_entry_field(
            raw_entry,
            "selected_weight",
            f"entries[{index}].selected_weight",
        ),
        f"entries[{index}].selected_weight",
    )
    selected_weight_index = require_int(
        require_entry_field(
            raw_entry,
            "selected_weight_index",
            f"entries[{index}].selected_weight_index",
        ),
        f"entries[{index}].selected_weight_index",
    )
    publication_gate_checked = require_bool(
        require_entry_field(
            raw_entry,
            "publication_gate_checked",
            f"entries[{index}].publication_gate_checked",
        ),
        f"entries[{index}].publication_gate_checked",
    )
    publication_gate_passed = require_bool(
        require_entry_field(
            raw_entry,
            "publication_gate_passed",
            f"entries[{index}].publication_gate_passed",
        ),
        f"entries[{index}].publication_gate_passed",
    )
    live_coefficients_available = require_bool(
        require_entry_field(
            raw_entry,
            "live_coefficients_available",
            f"entries[{index}].live_coefficients_available",
        ),
        f"entries[{index}].live_coefficients_available",
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
    expect(
        fingerprint.startswith("fnv1a64:"),
        f"{label} audit fingerprint must be fnv1a64",
    )
    expected_fingerprint = compute_artifact_fingerprint(audit)
    expect(
        fingerprint == expected_fingerprint,
        f"{label} audit fingerprint must match audit text: "
        f"expected {expected_fingerprint}, got {fingerprint}",
    )

    fields = parse_audit_text(audit)
    required_exact = {
        "kind": "b63n-scoped-weighted-residue-evaluation",
        "reviewed_surface": "true",
        "evaluation_attempted": "true",
        "surface": "phase[1,2,1,1,1,1,1]",
        "residue_model_kind": "automatic_phasespace::one-mass-three-body-residue",
        "selected_weight": selected_weight,
        "selected_weight_index": str(selected_weight_index),
        "publication_gate_checked": bool_text(publication_gate_checked),
        "publication_gate_passed": bool_text(publication_gate_passed),
        "live_coefficients_available": bool_text(live_coefficients_available),
        "retained_solution_samples_used": bool_text(retained_solution_samples_used),
        "full_eta_zero_contour_applied": bool_text(full_eta_zero_contour_applied),
        "moment_cross_validation": "passed",
        "moment_cross_validation_failure_count": "0",
        "candidate_provenance": EXPECTED_CANDIDATE_PROVENANCE[label],
    }
    for key, expected in required_exact.items():
        actual = require_field(fields, key)
        expect(actual == expected, f"{label} audit {key} must be {expected!r}, got {actual!r}")

    nested_gate = require_field(fields, "moment_cross_validation_publication_gate")
    expect(
        nested_gate.startswith("blocked-by-publication-gate"),
        f"{label} nested gate must preserve publication blocking",
    )
    expect("synthetic" in nested_gate, f"{label} nested gate must identify synthetic seeds")

    seed_identities = [
        require_field(fields, f"moment_cross_validation_seed_denominator_identity[{seed_index}]")
        for seed_index in range(len(EXPECTED_SEED_DENOMINATOR_IDENTITIES))
    ]
    expect(
        seed_identities == EXPECTED_SEED_DENOMINATOR_IDENTITIES,
        f"{label} nested seed identities must be queryable in canonical D2,D4,D6,D7 order",
    )
    seed_provenance = [
        require_field(fields, f"moment_cross_validation_seed_provenance[{seed_index}]")
        for seed_index in range(len(EXPECTED_SEED_PROVENANCE))
    ]
    expect(
        seed_provenance == EXPECTED_SEED_PROVENANCE,
        f"{label} nested seed provenance must be queryable in canonical D2,D4,D6,D7 order",
    )

    if label == "blocked-D2-scoped-weighted-residue":
        expect(selected_weight == "D2", "blocked scoped gate entry must query D2")
        expect(selected_weight_index == 1, "blocked scoped gate entry must query D2 index 1")
        expect(not publication_gate_passed, "blocked D2 scoped gate must remain blocked")
        expect(not live_coefficients_available, "blocked D2 scoped gate must not publish")
        expect(
            require_field(fields, "failure_code") == "boundary_unsolved",
            "blocked D2 scoped gate must expose boundary_unsolved",
        )
    elif label == "published-D7-scoped-weighted-residue":
        expect(selected_weight == "D7", "published scoped gate entry must query D7")
        expect(selected_weight_index == 6, "published scoped gate entry must query D7 index 6")
        expect(publication_gate_passed, "published D7 scoped gate must pass publication")
        expect(live_coefficients_available, "published D7 scoped gate must expose live coefficient availability")
        expect(
            require_field(fields, "reference_validation_passed") == "true",
            "published D7 scoped gate must retain AMFlow reference validation",
        )
        expect(require_field(fields, "failure_code") == "", "published D7 scoped gate must have no failure code")
    else:
        raise RuntimeError(f"unexpected scoped gate label {label}")

    expect(not retained_solution_samples_used, f"{label} must not consume retained final samples")
    expect(not full_eta_zero_contour_applied, f"{label} must not claim full eta-zero contour")
    return {"label": label, "selected_weight": selected_weight, "audit_fingerprint": fingerprint}


def validate_payload(payload: dict[str, Any]) -> dict[str, Any]:
    expect(payload.get("kind") == EXPECTED_KIND, "unexpected scoped gate payload kind")
    entries = require_list(payload.get("entries"), "entries")
    expect(payload.get("entry_count") == len(entries), "entry_count must match entries")
    validated = [validate_entry(raw_entry, index) for index, raw_entry in enumerate(entries)]
    labels = [entry["label"] for entry in validated]
    expect(labels == EXPECTED_LABELS, "scoped gate audit entries must remain in D2,D7 query order")
    expect(
        len({entry["audit_fingerprint"] for entry in validated}) == len(validated),
        "scoped gate audit fingerprints must remain distinct",
    )
    return {
        "schema_version": 1,
        "verifier": "b63n-scoped-gate-audit-trail-query-v1",
        "scoped_gate_audit_query_passed": True,
        "entry_count": len(validated),
        "queried_labels": labels,
        "queried_weights": [entry["selected_weight"] for entry in validated],
        "m6_closure_claimed": False,
        "m7_closure_claimed": False,
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
    missing_entry_gate_field = copy.deepcopy(payload)
    del missing_entry_gate_field["entries"][0]["publication_gate_checked"]
    missing_nested_gate = copy.deepcopy(payload)
    replace_entry_audit_line(
        missing_nested_gate["entries"][0],
        "moment_cross_validation_publication_gate=",
        None,
    )
    stale_seed_identity = copy.deepcopy(payload)
    replace_entry_audit_line(
        stale_seed_identity["entries"][1],
        "moment_cross_validation_seed_denominator_identity[3]=",
        "moment_cross_validation_seed_denominator_identity[3]=D7;stale=true",
    )
    unexpected_entry_label = copy.deepcopy(payload)
    unexpected_entry_label["entries"][0]["label"] = "synthetic-stale-scoped-label"
    full_contour_overclaim = copy.deepcopy(payload)
    full_contour_overclaim["entries"][0]["full_eta_zero_contour_applied"] = True
    replace_entry_audit_line(
        full_contour_overclaim["entries"][0],
        "full_eta_zero_contour_applied=",
        "full_eta_zero_contour_applied=true",
    )
    stale_audit_fingerprint = copy.deepcopy(payload)
    stale_audit_fingerprint["entries"][1]["audit_fingerprint"] = (
        "fnv1a64:0000000000000000"
    )
    checks = {
        "accepts_runtime_scoped_gate_emitter": accepted["scoped_gate_audit_query_passed"],
        "rejects_missing_entry_publication_gate_checked": rejected(
            missing_entry_gate_field,
            "entries[0].publication_gate_checked is missing",
        ),
        "rejects_missing_nested_gate_status": rejected(missing_nested_gate, "audit missing moment_cross_validation_publication_gate"),
        "rejects_stale_nested_seed_identity": rejected(stale_seed_identity, "nested seed identities"),
        "rejects_unexpected_entry_label": rejected(
            unexpected_entry_label,
            "unexpected scoped gate label synthetic-stale-scoped-label",
        ),
        "rejects_full_contour_overclaim": rejected(full_contour_overclaim, "must not claim full eta-zero contour"),
        "rejects_stale_audit_fingerprint": rejected(
            stale_audit_fingerprint,
            "audit fingerprint must match audit text",
        ),
    }
    expect(all(checks.values()), "b63n scoped gate audit trail verifier self-check failed")
    accepted["self_check_passed"] = True
    accepted["checks"] = checks
    return accepted


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--test-binary",
        help="Path to the built cutkosky-weighted-residue-tests executable.",
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
            "verifier": "b63n-scoped-gate-audit-trail-query-v1",
            "scoped_gate_audit_query_passed": False,
            "blocking_reasons": [str(error)],
            "m6_closure_claimed": False,
            "m7_closure_claimed": False,
        }
        if args.summary_path is not None:
            write_json(args.summary_path, summary)
        print_json(summary)
        return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
