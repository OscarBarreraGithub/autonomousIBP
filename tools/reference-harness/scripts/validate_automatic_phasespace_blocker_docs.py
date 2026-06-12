#!/usr/bin/env python3
"""Validate automatic_phasespace D246 blocker documentation consistency."""

from __future__ import annotations

import argparse
import json
import sys
import tempfile
from pathlib import Path
from typing import Any


RELEASE_KNOWN_GAPS = Path("docs/release/known-gaps.md")
AUTOMATIC_PHASESPACE_LIVE_DOC = Path(
    "docs/release/amflow-live-rerun-automatic_phasespace.md"
)
D246_BLOCKER_DOC = Path("docs/milestones/m7-b63n-d246-mathematica-attempt.md")
D246_SIDECAR = Path(
    "tools/reference-harness/specs/m6/lane146/"
    "b63n-d246-weighted-residue-reference-evidence.json"
)
D246_BLOCKER_RECORD_COMMIT = "a8f0772"
KNOWN_GAPS_BLOCKER_LINK = "../milestones/m7-b63n-d246-mathematica-attempt.md"
FULL_WEIGHTED_TARGET = "j[phase,1,2,1,1,1,1,1]"
EXPECTED_SOURCE_HASHES = {
    "automatic_phasespace_run_wl": "f9f021e584334cc21fb682526eaf9f55de95b6f2a58b2971ae46a445fd46e2bf",
    "amflow_m": "6fd47002b36399ee71c38e3e43e5e75541d1f2641966ca103fc8b8ce37dc7add",
    "desolver_m": "22c63b2aa4a4c8236a9593d39ba7ae8283efa12cb7730401e640ff1b43875585",
}
EXPECTED_WEIGHT_IDS = ["D2", "D4", "D6"]
EXPECTED_PUBLICATION_BLOCKERS = [
    "precision_requested_digits unset; rerun or recapture the upstream Mathematica surface at reviewed high precision",
    "eps_order_requested unset; publish a contiguous epsilon range matching the runtime scope",
    "run_command, run_log, raw_output, and raw_output_sha256 unset",
    "D2, D4, and D6 coefficient arrays are empty for the full j[phase,1,2,1,1,1,1,1] target",
    "independent comparison artifacts and 50-digit agreement claims are absent",
]

KNOWN_GAPS_ROW_FRAGMENTS = (
    "`automatic_phasespace`",
    "`b63n`",
    "weighted residue evaluation",
    "qualified high-precision AMFlow packet",
)
KNOWN_GAPS_BLOCKER_FRAGMENTS = (
    D246_BLOCKER_RECORD_COMMIT,
    KNOWN_GAPS_BLOCKER_LINK,
    "D246 blocker record",
    "fresh Mathematica/AMFlow invocation",
    "did not produce labeled D2/D4/D6 weighted-residue coefficients",
    "skeleton-only",
    "must not be treated as a qualified high-precision AMFlow packet",
    "C++ `b63n` live runtime reproduction",
)
KNOWN_GAPS_OVERCLAIMS = (
    "automatic_phasespace` | no remaining gap",
    "automatic_phasespace` | fully reproduced",
    "automatic_phasespace` | live reproduced",
    "D246 blocker closed",
    "D2/D4/D6 weighted-residue coefficients are published",
)
BLOCKER_DOC_FRAGMENTS = (
    "Status: blocked.",
    "successful fresh Mathematica/AMFlow invocation",
    "automatic_phasespace",
    "D2/D4/D6 weighted-residue coefficients",
    "examples/automatic_phasespace/run.wl",
    "family `phase`",
    "cut vector `{1, 0, 1, 0, 1, 0, 0}`",
    FULL_WEIGHTED_TARGET,
    "AMFlow 1.1",
    "Mathematica: 13.3.0",
    "AMFlow invocation succeeded",
    "It does not label or decompose selected-weight coefficient series for D2, D4, and D6.",
    "No such per-weight extraction element appears in the raw AMFlow result.",
    "no endpoint Laurent coefficient evaluated or published",
    "Reusing those D2/D4/D6 seeds would fake the requested reference data.",
    "D246 sidecar remains a skeleton",
    "`passed` remains false",
    "A coefficient-producing lane needs one of the following",
)


class ValidationError(RuntimeError):
    """Raised when fixture self-check setup is invalid."""


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise ValidationError(message)


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def read_text(root: Path, path: Path, errors: list[str]) -> str:
    full_path = root / path
    if not full_path.is_file():
        errors.append(f"{path} is missing")
        return ""
    return full_path.read_text(encoding="utf-8")


def require_fragments(
    text: str,
    fragments: tuple[str, ...],
    label: str,
    errors: list[str],
) -> None:
    normalized_text = " ".join(text.split())
    missing = [
        fragment
        for fragment in fragments
        if fragment not in text and " ".join(fragment.split()) not in normalized_text
    ]
    if missing:
        errors.append(f"{label} missing required fragment(s): {missing!r}")


def automatic_phasespace_known_gaps_row(text: str) -> tuple[int, str] | None:
    for line_number, line in enumerate(text.splitlines(), start=1):
        if line.startswith("| `automatic_phasespace` |"):
            return line_number, line
    return None


def validate_known_gaps(root: Path, errors: list[str]) -> dict[str, object]:
    text = read_text(root, RELEASE_KNOWN_GAPS, errors)
    if not text:
        return {"known_gaps_found": False}

    row = automatic_phasespace_known_gaps_row(text)
    if row is None:
        errors.append(f"{RELEASE_KNOWN_GAPS} missing `automatic_phasespace` gap row")
    else:
        line_number, row_text = row
        require_fragments(
            row_text,
            KNOWN_GAPS_ROW_FRAGMENTS,
            f"{RELEASE_KNOWN_GAPS}:{line_number} automatic_phasespace row",
            errors,
        )

    require_fragments(
        text,
        KNOWN_GAPS_BLOCKER_FRAGMENTS,
        f"{RELEASE_KNOWN_GAPS} automatic_phasespace D246 blocker paragraph",
        errors,
    )
    for phrase in KNOWN_GAPS_OVERCLAIMS:
        if phrase in text:
            errors.append(f"{RELEASE_KNOWN_GAPS} contains automatic_phasespace overclaim: {phrase!r}")

    if (root / AUTOMATIC_PHASESPACE_LIVE_DOC).exists():
        errors.append(
            f"{AUTOMATIC_PHASESPACE_LIVE_DOC} must not exist while the D246 blocker remains open"
        )

    return {"known_gaps_found": True, "known_gaps_row_line": row[0] if row is not None else None}


def validate_blocker_doc(root: Path, errors: list[str]) -> dict[str, object]:
    text = read_text(root, D246_BLOCKER_DOC, errors)
    if not text:
        return {"blocker_doc_found": False}

    require_fragments(
        text,
        BLOCKER_DOC_FRAGMENTS,
        f"{D246_BLOCKER_DOC}",
        errors,
    )
    for key, sha256 in EXPECTED_SOURCE_HASHES.items():
        if sha256 not in text:
            errors.append(f"{D246_BLOCKER_DOC} missing pinned source hash for {key}")

    return {"blocker_doc_found": True}


def load_json(root: Path, path: Path, errors: list[str]) -> dict[str, Any]:
    full_path = root / path
    if not full_path.is_file():
        errors.append(f"{path} is missing")
        return {}
    try:
        payload = json.loads(full_path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as error:
        errors.append(f"{path} is not valid JSON: {error}")
        return {}
    if not isinstance(payload, dict):
        errors.append(f"{path} must contain a JSON object")
        return {}
    return payload


def require_equal(actual: Any, expected: Any, label: str, errors: list[str]) -> None:
    if actual != expected:
        errors.append(f"{label} must be {expected!r}, got {actual!r}")


def require_false(actual: Any, label: str, errors: list[str]) -> None:
    if actual is not False:
        errors.append(f"{label} must be false, got {actual!r}")


def require_null(actual: Any, label: str, errors: list[str]) -> None:
    if actual is not None:
        errors.append(f"{label} must be null, got {actual!r}")


def validate_d246_sidecar(root: Path, errors: list[str]) -> dict[str, object]:
    payload = load_json(root, D246_SIDECAR, errors)
    if not payload:
        return {"sidecar_found": False}

    require_equal(payload.get("runtime_lane"), "b63n", "D246 runtime_lane", errors)
    require_equal(payload.get("benchmark_id"), "automatic_phasespace", "D246 benchmark_id", errors)
    require_equal(
        payload.get("surface_label"),
        "phase[1,2,1,1,1,1,1]",
        "D246 surface_label",
        errors,
    )
    require_equal(
        payload.get("status"),
        "skeleton-pending-amflow-reference-run",
        "D246 status",
        errors,
    )
    require_equal(payload.get("skeleton"), True, "D246 skeleton", errors)
    require_false(payload.get("passed"), "D246 passed", errors)

    source_files = payload.get("source_files")
    if not isinstance(source_files, dict):
        errors.append("D246 source_files must be an object")
    else:
        require_equal(set(source_files), set(EXPECTED_SOURCE_HASHES), "D246 source file keys", errors)
        for key, sha256 in EXPECTED_SOURCE_HASHES.items():
            entry = source_files.get(key)
            if not isinstance(entry, dict):
                errors.append(f"D246 source_files.{key} must be an object")
                continue
            require_equal(entry.get("sha256"), sha256, f"D246 source_files.{key}.sha256", errors)

    parameters = payload.get("amflow_parameter_set")
    if not isinstance(parameters, dict):
        errors.append("D246 amflow_parameter_set must be an object")
    else:
        require_equal(parameters.get("target"), FULL_WEIGHTED_TARGET, "D246 target", errors)
        require_equal(parameters.get("cut"), [1, 0, 1, 0, 1, 0, 0], "D246 cut", errors)
        for field in (
            "precision_requested_digits",
            "eps_order_requested",
            "working_precision_digits",
            "run_command",
            "run_log",
            "raw_output",
            "raw_output_sha256",
        ):
            require_null(parameters.get(field), f"D246 amflow_parameter_set.{field}", errors)

    weights = payload.get("weights")
    if not isinstance(weights, list):
        errors.append("D246 weights must be a list")
        weight_ids: list[str] = []
    else:
        weight_ids = [entry.get("denominator_id") for entry in weights if isinstance(entry, dict)]
        require_equal(weight_ids, EXPECTED_WEIGHT_IDS, "D246 weight scope", errors)
        for entry in weights:
            if not isinstance(entry, dict):
                errors.append("D246 weight entry must be an object")
                continue
            denominator_id = entry.get("denominator_id")
            label = f"D246 weight {denominator_id}"
            require_equal(entry.get("coefficients"), [], f"{label} coefficients", errors)
            validation = entry.get("reference_validation")
            if not isinstance(validation, dict):
                errors.append(f"{label} reference_validation must be an object")
                continue
            require_false(validation.get("passed"), f"{label} reference_validation.passed", errors)
            require_false(
                validation.get("coefficient_published"),
                f"{label} reference_validation.coefficient_published",
                errors,
            )
            require_false(
                validation.get("final_solution_samples_used_as_input"),
                f"{label} reference_validation.final_solution_samples_used_as_input",
                errors,
            )
            require_null(
                validation.get("minimum_digit_agreement"),
                f"{label} reference_validation.minimum_digit_agreement",
                errors,
            )

    anti_fake = payload.get("anti_fake")
    if not isinstance(anti_fake, dict):
        errors.append("D246 anti_fake must be an object")
    else:
        require_false(
            anti_fake.get("retained_solution_samples_used_as_input"),
            "D246 anti_fake.retained_solution_samples_used_as_input",
            errors,
        )
        require_false(anti_fake.get("synthetic_fixture"), "D246 anti_fake.synthetic_fixture", errors)
        require_false(
            anti_fake.get("full_eta_zero_contour_applied"),
            "D246 anti_fake.full_eta_zero_contour_applied",
            errors,
        )
        notes = anti_fake.get("notes")
        if not isinstance(notes, str) or "Skeleton only" not in notes or "must not promote b63n" not in notes:
            errors.append("D246 anti_fake.notes must preserve the skeleton non-promotion boundary")

    require_equal(
        payload.get("publication_blockers"),
        EXPECTED_PUBLICATION_BLOCKERS,
        "D246 publication_blockers",
        errors,
    )

    return {"sidecar_found": True, "weight_ids": weight_ids}


def validate_root(root: Path) -> tuple[dict[str, object], list[str]]:
    errors: list[str] = []
    summary: dict[str, object] = {
        "blocker_record_commit": D246_BLOCKER_RECORD_COMMIT,
        "known_gaps": validate_known_gaps(root, errors),
        "blocker_doc": validate_blocker_doc(root, errors),
        "sidecar": validate_d246_sidecar(root, errors),
    }
    return summary, errors


def blocker_doc_fixture() -> str:
    return f"""# M7 B63n D246 Mathematica Attempt

Status: blocked. This report records a successful fresh Mathematica/AMFlow invocation for the `automatic_phasespace` target, but it does not publish D2/D4/D6 weighted-residue coefficients and does not modify the D246 evidence sidecar.

## AMFlow Inventory

- `examples/automatic_phasespace/run.wl`
- family `phase`
- cut vector `{{1, 0, 1, 0, 1, 0, 0}}`
- target `{FULL_WEIGHTED_TARGET}`

```text
{EXPECTED_SOURCE_HASHES["automatic_phasespace_run_wl"]}  examples/automatic_phasespace/run.wl
{EXPECTED_SOURCE_HASHES["amflow_m"]}  AMFlow.m
{EXPECTED_SOURCE_HASHES["desolver_m"]}  diffeq_solver/DESolver.m
```

AMFlow 1.1
Mathematica: 13.3.0

## Blocker

AMFlow invocation succeeded. It does not label or decompose selected-weight coefficient series for D2, D4, and D6. No such per-weight extraction element appears in the raw AMFlow result. The current runtime path has no endpoint Laurent coefficient evaluated or published. Reusing those D2/D4/D6 seeds would fake the requested reference data.

Therefore the D246 sidecar remains a skeleton, `passed` remains false, and no promotion is justified.

## Required Follow-Up

A coefficient-producing lane needs one of the following before the D246 sidecar can be populated.
"""


def sidecar_fixture() -> dict[str, Any]:
    source_root = "/n/holylabs/schwartz_lab/Lab/obarrera/reference-inputs/autonomousIBP/cpc/amflow-gitlab-1.1-extracted"
    return {
        "runtime_lane": "b63n",
        "benchmark_id": "automatic_phasespace",
        "surface_label": "phase[1,2,1,1,1,1,1]",
        "status": "skeleton-pending-amflow-reference-run",
        "skeleton": True,
        "source_files": {
            "automatic_phasespace_run_wl": {
                "path": f"{source_root}/examples/automatic_phasespace/run.wl",
                "sha256": EXPECTED_SOURCE_HASHES["automatic_phasespace_run_wl"],
            },
            "amflow_m": {
                "path": f"{source_root}/AMFlow.m",
                "sha256": EXPECTED_SOURCE_HASHES["amflow_m"],
            },
            "desolver_m": {
                "path": f"{source_root}/diffeq_solver/DESolver.m",
                "sha256": EXPECTED_SOURCE_HASHES["desolver_m"],
            },
        },
        "amflow_parameter_set": {
            "target": FULL_WEIGHTED_TARGET,
            "cut": [1, 0, 1, 0, 1, 0, 0],
            "precision_requested_digits": None,
            "eps_order_requested": None,
            "working_precision_digits": None,
            "run_command": None,
            "run_log": None,
            "raw_output": None,
            "raw_output_sha256": None,
        },
        "weights": [
            {
                "denominator_id": denominator_id,
                "coefficients": [],
                "reference_validation": {
                    "passed": False,
                    "coefficient_published": False,
                    "final_solution_samples_used_as_input": False,
                    "minimum_digit_agreement": None,
                },
            }
            for denominator_id in EXPECTED_WEIGHT_IDS
        ],
        "anti_fake": {
            "retained_solution_samples_used_as_input": False,
            "synthetic_fixture": False,
            "full_eta_zero_contour_applied": False,
            "notes": "Skeleton only. This file publishes no AMFlow coefficient values and must not promote b63n.",
        },
        "publication_blockers": EXPECTED_PUBLICATION_BLOCKERS,
        "passed": False,
    }


def known_gaps_fixture() -> str:
    return f"""# Release Known Gaps

## Not Fully Reproduced Rows

| AMFlow example | Remaining gap |
| --- | --- |
| `automatic_phasespace` | Needs full `b63n` live Cutkosky phase-space boundary reconstruction, weighted residue evaluation, endpoint propagation, and a qualified high-precision AMFlow packet. |

For the `automatic_phasespace` row, the durable D246 blocker record introduced
at `{D246_BLOCKER_RECORD_COMMIT}` remains the release-facing reference:
[`m7-b63n-d246-mathematica-attempt.md`]({KNOWN_GAPS_BLOCKER_LINK}).
It records a fresh Mathematica/AMFlow invocation that succeeded, but did not
produce labeled D2/D4/D6 weighted-residue coefficients. The D246 sidecar
therefore remains skeleton-only and must not be treated as a qualified
high-precision AMFlow packet or a C++ `b63n` live runtime reproduction.
"""


def write_fixture_root(root: Path) -> None:
    (root / RELEASE_KNOWN_GAPS).parent.mkdir(parents=True, exist_ok=True)
    (root / D246_BLOCKER_DOC).parent.mkdir(parents=True, exist_ok=True)
    (root / D246_SIDECAR).parent.mkdir(parents=True, exist_ok=True)
    (root / RELEASE_KNOWN_GAPS).write_text(known_gaps_fixture(), encoding="utf-8")
    (root / D246_BLOCKER_DOC).write_text(blocker_doc_fixture(), encoding="utf-8")
    (root / D246_SIDECAR).write_text(
        json.dumps(sidecar_fixture(), indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )


def run_fixture_validation(mutator=None) -> tuple[dict[str, object], list[str]]:
    with tempfile.TemporaryDirectory(prefix="automatic-phasespace-blocker-docs-") as temp_dir:
        root = Path(temp_dir)
        write_fixture_root(root)
        if mutator is not None:
            mutator(root)
        return validate_root(root)


def expect_self_check_failure(label: str, mutator, expected: str) -> bool:
    _, errors = run_fixture_validation(mutator)
    expect(errors, f"{label} unexpectedly passed")
    detail = "\n".join(errors)
    expect(expected in detail, f"{label} failed for the wrong reason: {detail}")
    return True


def mutate_json(path: Path, mutator) -> None:
    payload = json.loads(path.read_text(encoding="utf-8"))
    mutator(payload)
    path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def run_self_check() -> dict[str, object]:
    summary, errors = run_fixture_validation()
    expect(not errors, "valid automatic_phasespace blocker fixture failed: " + "; ".join(errors))

    checks = {
        "valid_fixture": True,
        "missing_known_gaps_link_rejected": expect_self_check_failure(
            "missing-known-gaps-link",
            lambda root: (root / RELEASE_KNOWN_GAPS).write_text(
                known_gaps_fixture().replace(KNOWN_GAPS_BLOCKER_LINK, "missing.md"),
                encoding="utf-8",
            ),
            "D246 blocker paragraph",
        ),
        "blocker_doc_overclaim_rejected": expect_self_check_failure(
            "blocker-doc-overclaim",
            lambda root: (root / D246_BLOCKER_DOC).write_text(
                blocker_doc_fixture().replace(
                    "It does not label or decompose selected-weight coefficient series for D2, D4, and D6.",
                    "It labels and decomposes selected-weight coefficient series for D2, D4, and D6.",
                ),
                encoding="utf-8",
            ),
            "It does not label or decompose",
        ),
        "published_sidecar_rejected": expect_self_check_failure(
            "published-sidecar",
            lambda root: mutate_json(
                root / D246_SIDECAR,
                lambda payload: payload.update({"passed": True, "skeleton": False}),
            ),
            "D246 skeleton",
        ),
        "coefficient_publication_rejected": expect_self_check_failure(
            "coefficient-publication",
            lambda root: mutate_json(
                root / D246_SIDECAR,
                lambda payload: payload["weights"][0]["reference_validation"].update(
                    {"passed": True, "coefficient_published": True, "minimum_digit_agreement": "50.0"}
                ),
            ),
            "reference_validation.passed",
        ),
        "automatic_phasespace_live_doc_rejected": expect_self_check_failure(
            "automatic-phasespace-live-doc",
            lambda root: (root / AUTOMATIC_PHASESPACE_LIVE_DOC).write_text(
                "# AMFlow Live Rerun: automatic_phasespace\n",
                encoding="utf-8",
            ),
            "must not exist while the D246 blocker remains open",
        ),
    }
    expect(all(checks.values()), "automatic_phasespace blocker doc self-check failed")
    return {
        "self_check_passed": True,
        "checks": checks,
        "valid_summary": summary,
    }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--verify",
        action="store_true",
        help="Validate committed automatic_phasespace blocker docs. This is the default unless --self-check is used.",
    )
    parser.add_argument(
        "--self-check",
        action="store_true",
        help="Run fixture-backed positive and negative checks for this validator.",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        if args.self_check:
            print(json.dumps(run_self_check(), indent=2, sort_keys=True))
            return 0
        summary, errors = validate_root(repo_root())
    except ValidationError as error:
        print(f"automatic_phasespace blocker doc validation failed: {error}", file=sys.stderr)
        return 1

    if errors:
        print("automatic_phasespace blocker doc validation failed:", file=sys.stderr)
        for error in errors:
            print(f"  - {error}", file=sys.stderr)
        return 1

    sidecar = summary["sidecar"]
    weight_ids = sidecar["weight_ids"] if isinstance(sidecar, dict) else []
    print(
        "automatic_phasespace blocker doc validation passed: "
        f"D246 record {D246_BLOCKER_RECORD_COMMIT}, weights={weight_ids}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
