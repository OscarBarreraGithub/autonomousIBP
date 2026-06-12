#!/usr/bin/env python3
"""Verify b61n/b63n/b64ag boundary sidecar provenance and artifact pins."""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Any

from audit_b61n_publication_qualifier import (
    audit_sidecar as audit_b61n_publication_sidecar,
    default_publication_qualifier_sidecar_path,
)
from audit_b64ag_golden_recapture_readiness import (
    audit_b64ag_golden_recapture_readiness,
)
from verify_b61n_exact_rational_extension_stability import (
    DEFAULT_EXTENSION_FIXTURE as B61N_EXTENSION_FIXTURE,
    verify_paths as verify_b61n_extension_paths,
)
from verify_b61n_precision_uplift_monotonicity import (
    DEFAULT_PRECISION_EVIDENCE as B61N_PRECISION_EVIDENCE,
    verify_paths as verify_b61n_precision_paths,
)
from verify_b61n_reference_floor_parity import (
    DEFAULT_AMFLOW_GOLDEN as B61N_REFERENCE_FLOOR_GOLDEN,
    DEFAULT_CPP_RESULT as B61N_REFERENCE_FLOOR_CPP,
    DEFAULT_RETAINED_COMPARISON as B61N_REFERENCE_FLOOR_COMPARISON,
    verify_paths as verify_b61n_reference_floor_paths,
)
from verify_b63n_d246_evidence import (
    DEFAULT_SIDECAR as B63N_D246_SIDECAR,
    verify_sidecar as verify_b63n_d246_sidecar,
)
from verify_b63n_selected4_permutation_audit import (
    DEFAULT_COMPARE as B63N_SELECTED4_COMPARE,
    DEFAULT_CPP_RESULT as B63N_SELECTED4_CPP_RESULT,
    DEFAULT_EVIDENCE as B63N_SELECTED4_EVIDENCE,
    verify_paths as verify_b63n_selected4_paths,
)


SPECS_ROOT = Path("tools/reference-harness/specs")
LIVE_RERUN_PARTS = ("phase0", "live-rerun")
B64AG_READINESS = Path(
    "tools/reference-harness/specs/m6/lane1-next24/"
    "b64ag-golden-recapture-readiness.json"
)
B64AG_CPP_RESULT = Path(
    "tools/reference-harness/specs/m6/lane1-next24/"
    "linear_propagator.b64ag-full-packet-finite-part.cpp-result.json"
)
B64AG_COMPARE50 = Path(
    "tools/reference-harness/specs/m6/lane1-next24/"
    "linear_propagator.b64ag-full-packet-finite-part.compare50.json"
)
B64AG_AMFLOW_STATE = Path("tools/reference-harness/specs/phase0/linear_propagator.amflow-state.json")

BOUNDARY_TOKEN_RE = re.compile(r"(^|[-_/])(b61n|b63n|b64ag)([-_/]|$)")
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
SOURCE_PROVENANCE_DIGEST_FIELDS = frozenset(
    ("source_payload_sha256", "source_provenance_sha256")
)
PATH_PREFIXES = (
    "tools/",
    "src/",
    "include/",
    "tests/",
    "docs/",
    "cmake/",
    "CMakeLists.txt",
    "build/",
)
PATH_SUFFIXES = (
    ".json",
    ".txt",
    ".md",
    ".wl",
    ".m",
    ".cpp",
    ".hpp",
    ".py",
    ".cmake",
    ".sqlite",
    ".db",
)


class BoundarySidecarError(RuntimeError):
    """Raised when boundary sidecar provenance drifts."""


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def expect(condition: bool, message: str) -> None:
    if not condition:
        raise BoundarySidecarError(message)


def read_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as stream:
        payload = json.load(stream)
    expect(isinstance(payload, dict), f"{path} must contain a JSON object")
    return payload


def write_json(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def source_provenance_sha256(payload: dict[str, Any]) -> str:
    encoded = json.dumps(
        {
            key: value
            for key, value in payload.items()
            if key not in SOURCE_PROVENANCE_DIGEST_FIELDS
        },
        ensure_ascii=False,
        separators=(",", ":"),
        sort_keys=True,
    ).encode("utf-8")
    return hashlib.sha256(encoded).hexdigest()


def is_live_rerun_path(path: Path) -> bool:
    parts = path.parts
    return any(
        tuple(parts[index : index + len(LIVE_RERUN_PARTS)]) == LIVE_RERUN_PARTS
        for index in range(0, len(parts) - len(LIVE_RERUN_PARTS) + 1)
    )


def path_has_boundary_token(path: Path) -> bool:
    return BOUNDARY_TOKEN_RE.search(path.as_posix()) is not None


def boundary_seed_sidecars(root: Path) -> list[Path]:
    specs = root / SPECS_ROOT
    paths = [
        path
        for path in specs.rglob("*.json")
        if path_has_boundary_token(path.relative_to(root)) and not is_live_rerun_path(path.relative_to(root))
    ]
    expect(paths, "no b61n/b63n/b64ag JSON sidecars were found")
    return sorted(paths)


def clean_path_text(raw: str) -> str | None:
    text = raw.strip()
    if not text or text != raw:
        return None
    if "\n" in text or "\r" in text or "\t" in text:
        return None
    if " " in text and not Path(text).is_absolute():
        return None
    if text.startswith("fnv1a64:") or text.startswith("sha256:"):
        return None
    if ":" in text and not re.search(r":\d+$", text) and not Path(text).is_absolute():
        return None
    if re.search(r":\d+$", text):
        text = text.rsplit(":", 1)[0]
    return text


def looks_like_path(text: str) -> bool:
    path = Path(text)
    if path.is_absolute():
        return True
    return text.startswith(PATH_PREFIXES) or text.endswith(PATH_SUFFIXES)


def resolve_candidate(root: Path, raw: Any, *, base_file: Path | None = None) -> Path | None:
    if not isinstance(raw, str):
        return None
    text = clean_path_text(raw)
    if text is None or not looks_like_path(text):
        return None
    path = Path(text)
    if path.is_absolute():
        return path
    if base_file is not None and path.parts and path.parts[0] == "..":
        return base_file.parent / path
    return root / path


def require_sha(raw: Any, label: str) -> str:
    expect(isinstance(raw, str), f"{label} must be a string")
    expect(SHA256_RE.fullmatch(raw) is not None, f"{label} must be a lowercase sha256")
    return raw


def require_existing_path(
    root: Path,
    raw: Any,
    label: str,
    *,
    base_file: Path | None = None,
) -> Path | None:
    path = resolve_candidate(root, raw, base_file=base_file)
    if path is None:
        return None
    if not path.is_absolute():
        path = root / path
    try:
        relative = path.resolve(strict=False).relative_to(root.resolve(strict=True))
    except ValueError:
        expect(path.exists(), f"{label} absolute path is missing: {path}")
        return path
    if relative.parts and relative.parts[0] == "build" and not path.exists():
        return None
    expect(path.exists(), f"{label} referenced path is missing: {relative.as_posix()}")
    return path


def is_noncommitted_build_path(raw: Any) -> bool:
    if not isinstance(raw, str):
        return False
    text = clean_path_text(raw)
    if text is None:
        return False
    path = Path(text)
    return not path.is_absolute() and bool(path.parts) and path.parts[0] == "build"


def iter_objects(value: Any) -> list[dict[str, Any]]:
    objects: list[dict[str, Any]] = []

    def visit(item: Any) -> None:
        if isinstance(item, dict):
            objects.append(item)
            for nested in item.values():
                visit(nested)
        elif isinstance(item, list):
            for nested in item:
                visit(nested)

    visit(value)
    return objects


def iter_string_items(value: Any, prefix: str = "") -> list[tuple[str, str]]:
    items: list[tuple[str, str]] = []
    if isinstance(value, dict):
        for key, nested in value.items():
            items.extend(iter_string_items(nested, f"{prefix}.{key}" if prefix else str(key)))
    elif isinstance(value, list):
        for index, nested in enumerate(value):
            items.extend(iter_string_items(nested, f"{prefix}[{index}]"))
    elif isinstance(value, str):
        items.append((prefix, value))
    return items


def verify_source_provenance(root: Path, path: Path, payload: dict[str, Any]) -> int:
    if "source_provenance_sha256" not in payload:
        return 0
    label = path.relative_to(root).as_posix()
    digest = require_sha(payload.get("source_provenance_sha256"), f"{label}.source_provenance_sha256")
    expected = source_provenance_sha256(payload)
    expect(digest == expected, f"{label} source_provenance_sha256 drifted")
    commit = payload.get("source_commit")
    if commit is not None:
        expect(
            isinstance(commit, str) and re.fullmatch(r"[0-9a-f]{40}", commit) is not None,
            f"{label}.source_commit must be a full lowercase git SHA",
        )
        completed = subprocess.run(
            ["git", "cat-file", "-e", f"{commit}^{{commit}}"],
            cwd=root,
            text=True,
            capture_output=True,
            check=False,
        )
        expect(completed.returncode == 0, f"{label}.source_commit is not known to git: {commit}")
    return 1


def verify_paired_sha_fields(root: Path, path: Path, payload: dict[str, Any]) -> tuple[int, int]:
    checked = 0
    skipped = 0
    label = path.relative_to(root).as_posix()
    for obj in iter_objects(payload):
        if "path" in obj and "sha256" in obj:
            pinned = require_sha(obj["sha256"], f"{label}.sha256")
            target = require_existing_path(root, obj["path"], f"{label}.path", base_file=path)
            if target is not None:
                expect(sha256_file(target) == pinned, f"{label}.path sha256 drifted: {obj['path']}")
                checked += 1
            continue
        for key, value in obj.items():
            if (
                not key.endswith("_sha256")
                or value is None
                or key == "source_provenance_sha256"
                or (key == "artifact_sha256" and isinstance(value, dict))
            ):
                continue
            pinned = require_sha(value, f"{label}.{key}")
            base_key = key[: -len("_sha256")]
            raw_path = obj.get(base_key)
            if raw_path is None:
                continue
            if is_noncommitted_build_path(raw_path):
                skipped += 1
                continue
            target = require_existing_path(root, raw_path, f"{label}.{base_key}", base_file=path)
            if target is None:
                skipped += 1
                continue
            expect(sha256_file(target) == pinned, f"{label}.{key} drifted for {raw_path}")
            checked += 1
    artifact_sha = payload.get("artifact_sha256")
    if isinstance(artifact_sha, dict):
        for raw_path, raw_sha in artifact_sha.items():
            pinned = require_sha(raw_sha, f"{label}.artifact_sha256[{raw_path}]")
            if is_noncommitted_build_path(raw_path):
                skipped += 1
                continue
            target = require_existing_path(
                root,
                raw_path,
                f"{label}.artifact_sha256[{raw_path}]",
                base_file=path,
            )
            if target is None:
                skipped += 1
                continue
            expect(sha256_file(target) == pinned, f"{label}.artifact_sha256 drifted for {raw_path}")
            checked += 1
    return checked, skipped


def verify_path_references(root: Path, path: Path, payload: dict[str, Any]) -> int:
    checked = 0
    label = path.relative_to(root).as_posix()
    for field, value in iter_string_items(payload):
        target = require_existing_path(root, value, f"{label}.{field}", base_file=path)
        if target is not None:
            checked += 1
    return checked


def referenced_spec_jsons(root: Path, path: Path, payload: dict[str, Any]) -> list[Path]:
    refs: list[Path] = []
    specs = (root / SPECS_ROOT).resolve(strict=True)
    for _field, value in iter_string_items(payload):
        target = resolve_candidate(root, value, base_file=path)
        if target is None or target.suffix != ".json":
            continue
        resolved = target.resolve(strict=False)
        try:
            resolved.relative_to(specs)
        except ValueError:
            continue
        relative = resolved.relative_to(root.resolve(strict=True))
        if is_live_rerun_path(relative):
            continue
        if resolved.exists():
            refs.append(resolved)
    return refs


def collect_sidecar_graph(root: Path) -> list[Path]:
    seen: set[Path] = set()
    queue = boundary_seed_sidecars(root)
    while queue:
        path = queue.pop(0).resolve(strict=True)
        if path in seen:
            continue
        seen.add(path)
        payload = read_json(path)
        for ref in referenced_spec_jsons(root, path, payload):
            if ref not in seen:
                queue.append(ref)
    return sorted(seen)


def require_object(raw: Any, label: str) -> dict[str, Any]:
    expect(isinstance(raw, dict), f"{label} must be an object")
    return raw


def require_list(raw: Any, label: str) -> list[Any]:
    expect(isinstance(raw, list), f"{label} must be a list")
    return raw


def require_bool(raw: Any, label: str) -> bool:
    expect(isinstance(raw, bool), f"{label} must be a bool")
    return raw


def sidecar_path(root: Path, raw: Any, label: str) -> Path:
    target = require_existing_path(root, raw, label)
    expect(target is not None, f"{label} must point to a retained artifact")
    return target


def integral_names(rows: Any, label: str) -> list[str]:
    names: list[str] = []
    for index, raw_row in enumerate(require_list(rows, label)):
        row = require_object(raw_row, f"{label}[{index}]")
        name = row.get("integral")
        expect(isinstance(name, str) and name, f"{label}[{index}].integral must be a string")
        names.append(name)
    expect(len(names) == len(set(names)), f"{label} integral names must be unique")
    return names


def verify_selected_endpoint_evidence(root: Path, path: Path, payload: dict[str, Any]) -> int:
    if "eta_zero_endpoint_transported_integrals" not in payload:
        return 0
    label = path.relative_to(root).as_posix()
    transported = payload.get("eta_zero_endpoint_transported_integrals")
    expect(isinstance(transported, list), f"{label}.eta_zero_endpoint_transported_integrals must be a list")
    expect(
        all(isinstance(item, str) and item for item in transported),
        f"{label}.eta_zero_endpoint_transported_integrals entries must be strings",
    )
    expect(
        len(transported) == len(set(transported)),
        f"{label}.eta_zero_endpoint_transported_integrals entries must be unique",
    )

    cpp_path = sidecar_path(root, payload.get("cpp_result"), f"{label}.cpp_result")
    cpp = read_json(cpp_path)
    continuation = require_object(cpp.get("continuation"), f"{label} cpp_result.continuation")
    expect(
        continuation.get("eta_zero_endpoint_transported_integrals") == transported,
        f"{label} transported integral witness array drifted from cpp_result.continuation",
    )
    for field in ("transport_scope", "eta_zero_endpoint_transport_applied", "full_eta_zero_contour_applied"):
        if field in payload:
            expect(
                payload.get(field) == continuation.get(field),
                f"{label}.{field} drifted from cpp_result.continuation",
            )
    boundary_state = require_object(cpp.get("boundary_state"), f"{label} cpp_result.boundary_state")
    if "runtime_boundary_provider" in payload:
        expect(
            payload.get("runtime_boundary_provider") == boundary_state.get("runtime_boundary_provider"),
            f"{label}.runtime_boundary_provider drifted from cpp_result.boundary_state",
        )
    if "runtime_application" in payload:
        expect(
            payload.get("runtime_application") == continuation.get("runtime_application"),
            f"{label}.runtime_application drifted from cpp_result.continuation",
        )

    compare_path_text = None
    comparator = payload.get("comparator")
    if isinstance(comparator, dict):
        compare_path_text = comparator.get("path")
    compare30 = payload.get("compare30")
    if isinstance(compare30, dict):
        compare_path_text = compare30.get("path")
        comparator = compare30
    if compare_path_text is not None:
        compare_path = sidecar_path(root, compare_path_text, f"{label}.comparator.path")
        compare = read_json(compare_path)
        expect(set(integral_names(compare.get("integrals"), f"{label} comparator.integrals")) == set(transported),
               f"{label} comparator integral witness set drifted")
        if isinstance(comparator, dict):
            for field in (
                "compared_coefficient_count",
                "passed_coefficient_count",
                "matched_integral_count",
                "minimum_digit_agreement",
                "tolerance_digits",
                "passed",
            ):
                if field in comparator:
                    expect(
                        comparator.get(field) == compare.get(field),
                        f"{label}.comparator.{field} drifted from comparator sidecar",
                    )
    if "final_solution_samples_used_as_input" in payload:
        expect(
            payload.get("final_solution_samples_used_as_input") is False,
            f"{label}.final_solution_samples_used_as_input must remain false",
        )
    return 1


def verify_b64ag_readiness(root: Path) -> None:
    committed = read_json(root / B64AG_READINESS)
    rerun = audit_b64ag_golden_recapture_readiness(
        root / B64AG_CPP_RESULT,
        root / B64AG_COMPARE50,
        root / B64AG_AMFLOW_STATE,
    )
    rerun["inputs"] = {
        "cpp_result": str(B64AG_CPP_RESULT),
        "comparison_summary": str(B64AG_COMPARE50),
        "amflow_state": str(B64AG_AMFLOW_STATE),
    }
    expect(committed == rerun, f"{B64AG_READINESS} is not reproducible from current inputs")


def run_semantic_verifiers(root: Path) -> dict[str, str]:
    summaries: dict[str, str] = {}
    verify_b61n_reference_floor_paths(
        root / B61N_REFERENCE_FLOOR_CPP,
        root / B61N_REFERENCE_FLOOR_GOLDEN,
        root / B61N_REFERENCE_FLOOR_COMPARISON,
    )
    summaries["b61n_reference_floor"] = "passed"
    verify_b61n_precision_paths(root / B61N_PRECISION_EVIDENCE)
    summaries["b61n_precision_uplift"] = "passed"
    verify_b61n_extension_paths(root / B61N_EXTENSION_FIXTURE)
    summaries["b61n_exact_rational_extension"] = "passed"
    audit_b61n_publication_sidecar(root / default_publication_qualifier_sidecar_path())
    summaries["b61n_publication_qualifier"] = "passed"
    verify_b63n_d246_sidecar(root / B63N_D246_SIDECAR)
    summaries["b63n_d246"] = "passed"
    verify_b63n_selected4_paths(
        None,
        root / B63N_SELECTED4_EVIDENCE,
        root / B63N_SELECTED4_COMPARE,
        root / B63N_SELECTED4_CPP_RESULT,
    )
    summaries["b63n_selected4"] = "passed"
    verify_b64ag_readiness(root)
    summaries["b64ag_readiness"] = "passed"
    return summaries


def verify(root: Path, *, semantic: bool = True) -> dict[str, Any]:
    root = root.resolve(strict=True)
    sidecars = collect_sidecar_graph(root)
    path_reference_count = 0
    sha_pin_count = 0
    skipped_sha_pin_count = 0
    source_provenance_count = 0
    selected_endpoint_count = 0
    for path in sidecars:
        payload = read_json(path)
        path_reference_count += verify_path_references(root, path, payload)
        checked, skipped = verify_paired_sha_fields(root, path, payload)
        sha_pin_count += checked
        skipped_sha_pin_count += skipped
        source_provenance_count += verify_source_provenance(root, path, payload)
        selected_endpoint_count += verify_selected_endpoint_evidence(root, path, payload)
    semantic_summaries = run_semantic_verifiers(root) if semantic else {}
    return {
        "schema_version": 1,
        "status": "boundary-sidecar-provenance-clean",
        "seed_sidecar_count": len(boundary_seed_sidecars(root)),
        "sidecar_graph_json_count": len(sidecars),
        "path_reference_count": path_reference_count,
        "sha256_pin_count": sha_pin_count,
        "skipped_noncommitted_build_sha256_pin_count": skipped_sha_pin_count,
        "source_provenance_digest_count": source_provenance_count,
        "selected_endpoint_witness_sidecar_count": selected_endpoint_count,
        "semantic_verifiers": semantic_summaries,
        "live_rerun_docs_touched": False,
    }


def run_self_check() -> dict[str, Any]:
    with tempfile.TemporaryDirectory(prefix="boundary-sidecar-provenance-") as raw_tmp:
        root = Path(raw_tmp)
        sidecar = root / SPECS_ROOT / "m6/lane-self/b61n-self-evidence.json"
        artifact = root / SPECS_ROOT / "m6/lane-self/artifact.txt"
        artifact.parent.mkdir(parents=True, exist_ok=True)
        artifact.write_text("retained artifact\n", encoding="utf-8")
        payload = {
            "schema_version": 1,
            "benchmark_id": "self",
            "runtime_lane": "b61n",
            "source_artifacts": {
                "artifact": artifact.relative_to(root).as_posix(),
                "artifact_sha256": sha256_file(artifact),
            },
        }
        write_json(sidecar, payload)
        clean = verify(root, semantic=False)

        bad_sha = copy.deepcopy(payload)
        bad_sha["source_artifacts"]["artifact_sha256"] = "0" * 64
        write_json(sidecar, bad_sha)
        bad_sha_rejected = False
        try:
            verify(root, semantic=False)
        except BoundarySidecarError:
            bad_sha_rejected = True

        missing_path = copy.deepcopy(payload)
        missing_path["source_artifacts"]["artifact"] = (
            root / "missing-boundary-artifact.json"
        ).as_posix()
        missing_path["source_artifacts"]["artifact_sha256"] = "1" * 64
        write_json(sidecar, missing_path)
        missing_path_rejected = False
        try:
            verify(root, semantic=False)
        except BoundarySidecarError:
            missing_path_rejected = True

        expect(bad_sha_rejected, "self-check did not reject a stale sha256 pin")
        expect(missing_path_rejected, "self-check did not reject a missing absolute path")
        return {
            "schema_version": 1,
            "status": "boundary-sidecar-provenance-self-check-clean",
            "clean_sidecar_graph_json_count": clean["sidecar_graph_json_count"],
            "bad_sha_rejected": bad_sha_rejected,
            "missing_path_rejected": missing_path_rejected,
        }


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=repo_root(), help="repository root")
    parser.add_argument("--self-check", action="store_true", help="run synthetic tamper checks")
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    try:
        summary = run_self_check() if args.self_check else verify(args.root)
        print(json.dumps(summary, indent=2, sort_keys=True))
        return 0
    except Exception as error:  # noqa: BLE001 - CTest should show the first blocker.
        print(
            json.dumps(
                {
                    "schema_version": 1,
                    "status": "boundary-sidecar-provenance-drift",
                    "blocking_reasons": [str(error)],
                    "live_rerun_docs_touched": False,
                },
                indent=2,
                sort_keys=True,
            ),
            file=sys.stderr,
        )
        return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
