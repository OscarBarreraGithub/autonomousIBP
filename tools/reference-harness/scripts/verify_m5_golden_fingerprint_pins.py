#!/usr/bin/env python3
"""Fail-closed verifier for committed M5 golden fingerprint pins."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import tempfile
from pathlib import Path
from typing import Any, Callable


DEFAULT_EVIDENCE_SUMMARY = Path(
    "tools/reference-harness/specs/m5/m5-feature-surface-lane50.json"
)

PINNED_M5_GOLDENS: tuple[dict[str, str], ...] = (
    {
        "path": (
            "tools/reference-harness/specs/m5/lane50/goldens/"
            "spacetime_dimension.direct-nondefault-d.amflow-golden.txt"
        ),
        "sha256": "9e2c7d8564375dba3a80d423d11f4a6b95b6dbdae64657098e20aee1125674de",
    },
    {
        "path": (
            "tools/reference-harness/specs/m5/lane50/goldens/"
            "user_defined_amfmode.box1_m2_1_1_2.scoped-golden.txt"
        ),
        "sha256": "88d3e51cf6ac07b68017b6dfe33d07b34f8bc156beaea82ce5a32a022163ffa0",
    },
    {
        "path": (
            "tools/reference-harness/specs/m5/lane50/goldens/"
            "user_defined_ending.final_usr.box1_m2_1_1_2.scoped-golden.txt"
        ),
        "sha256": "88d3e51cf6ac07b68017b6dfe33d07b34f8bc156beaea82ce5a32a022163ffa0",
    },
)

SHA256_RE = re.compile(r"^[0-9a-f]{64}$")


def repo_root() -> Path:
    return Path(__file__).resolve().parents[3]


def load_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as stream:
        payload = json.load(stream)
    if not isinstance(payload, dict):
        raise RuntimeError(f"{path} must contain a JSON object")
    return payload


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(65536), b""):
            digest.update(chunk)
    return digest.hexdigest()


def repo_relative(path: Path, root: Path) -> str | None:
    try:
        return path.resolve(strict=False).relative_to(root.resolve(strict=False)).as_posix()
    except ValueError:
        return None


def resolve_repo_or_base_relative(raw: Any, *, root: Path, base_dir: Path, label: str) -> Path:
    if not isinstance(raw, str) or not raw.strip():
        raise RuntimeError(f"{label} must be a non-empty string path")
    value = raw.strip()
    path = Path(value)
    if path.is_absolute():
        return path
    if path.parts[:1] == ("tools",):
        return root / path
    return base_dir / path


def is_committed_m5_golden_path(relative_path: str) -> bool:
    parts = Path(relative_path).parts
    return (
        len(parts) >= 7
        and parts[0:4] == ("tools", "reference-harness", "specs", "m5")
        and parts[4].startswith("lane")
        and parts[5] == "goldens"
    )


def normalize_pins(raw_pins: tuple[dict[str, str], ...] | list[dict[str, str]]) -> dict[str, str]:
    pins: dict[str, str] = {}
    errors: list[str] = []
    for index, pin in enumerate(raw_pins):
        path = str(pin.get("path", "")).strip()
        digest = str(pin.get("sha256", "")).strip()
        if not path:
            errors.append(f"pin[{index}].path must not be empty")
            continue
        if path in pins:
            errors.append(f"duplicate M5 golden fingerprint pin path: {path}")
        if not is_committed_m5_golden_path(path):
            errors.append(f"M5 golden fingerprint pin path is outside m5/lane*/goldens: {path}")
        if SHA256_RE.fullmatch(digest) is None:
            errors.append(f"M5 golden fingerprint pin must be lowercase sha256 for {path}")
        pins[path] = digest
    if errors:
        raise RuntimeError("\n".join(errors))
    return pins


def comparison_paths_from_evidence(evidence_summary_path: Path, *, root: Path) -> list[Path]:
    evidence = load_json(evidence_summary_path)
    if evidence.get("schema_version") != 1:
        raise RuntimeError("M5 feature evidence schema_version must be 1")
    if evidence.get("scope") != "m5-feature-surface":
        raise RuntimeError("M5 feature evidence scope must be m5-feature-surface")

    examples = evidence.get("examples")
    if not isinstance(examples, list):
        raise RuntimeError("M5 feature evidence examples must be a list")

    paths: list[Path] = []
    seen_examples: set[str] = set()
    for index, entry in enumerate(examples):
        if not isinstance(entry, dict):
            raise RuntimeError(f"M5 feature evidence examples[{index}] must be an object")
        example_id = str(entry.get("id", "")).strip()
        if not example_id:
            raise RuntimeError(f"M5 feature evidence examples[{index}].id must not be empty")
        if example_id in seen_examples:
            raise RuntimeError(f"duplicate M5 feature evidence example id: {example_id}")
        seen_examples.add(example_id)
        comparison_path = resolve_repo_or_base_relative(
            entry.get("comparison_summary"),
            root=root,
            base_dir=evidence_summary_path.parent,
            label=f"M5 feature evidence {example_id} comparison_summary",
        )
        paths.append(comparison_path)
    return paths


def current_m5_golden_surface(root: Path, evidence_summary_path: Path) -> set[str]:
    surface: set[str] = set()
    for comparison_path in comparison_paths_from_evidence(evidence_summary_path, root=root):
        comparison = load_json(comparison_path)
        raw_golden = comparison.get("amflow_golden") or comparison.get("reference_golden")
        if raw_golden is None:
            continue
        golden_path = resolve_repo_or_base_relative(
            raw_golden,
            root=root,
            base_dir=comparison_path.parent,
            label=f"{comparison_path} amflow_golden",
        )
        relative = repo_relative(golden_path, root)
        if relative is not None and is_committed_m5_golden_path(relative):
            surface.add(relative)
    return surface


def committed_m5_golden_files(root: Path) -> set[str]:
    m5_root = root / "tools/reference-harness/specs/m5"
    return {
        path.relative_to(root).as_posix()
        for path in m5_root.glob("lane*/goldens/*")
        if path.is_file()
    }


def format_set(values: set[str]) -> str:
    return ", ".join(sorted(values)) if values else "<none>"


def verify_m5_golden_pins(
    *,
    root: Path,
    evidence_summary: Path,
    raw_pins: tuple[dict[str, str], ...] | list[dict[str, str]] = PINNED_M5_GOLDENS,
) -> dict[str, Any]:
    evidence_path = evidence_summary if evidence_summary.is_absolute() else root / evidence_summary
    pins = normalize_pins(raw_pins)
    surface = current_m5_golden_surface(root, evidence_path)
    committed = committed_m5_golden_files(root)
    pinned_paths = set(pins)

    errors: list[str] = []
    if committed != surface:
        errors.append(
            "committed m5/lane*/goldens files must match the current M5 release surface exactly; "
            f"not referenced by surface: {format_set(committed - surface)}; "
            f"referenced but not committed: {format_set(surface - committed)}"
        )
    if pinned_paths != surface:
        errors.append(
            "M5 golden fingerprint pin table must match the current release surface exactly; "
            f"missing pins: {format_set(surface - pinned_paths)}; "
            f"extra pins: {format_set(pinned_paths - surface)}"
        )

    entries: list[dict[str, Any]] = []
    for relative_path in sorted(pinned_paths):
        path = root / relative_path
        expected = pins[relative_path]
        if not path.is_file():
            errors.append(f"M5 golden fingerprint pinned file is missing: {relative_path}")
            continue
        actual = sha256_file(path)
        entries.append(
            {
                "path": relative_path,
                "pinned_sha256": expected,
                "actual_sha256": actual,
                "matches_pin": actual == expected,
            }
        )
        if actual != expected:
            errors.append(
                "M5 golden fingerprint drift: "
                f"{relative_path} expected {expected}, got {actual}"
            )

    if errors:
        raise RuntimeError("\n".join(errors))

    return {
        "schema_version": 1,
        "scope": "m5-golden-fingerprint-pins",
        "evidence_summary": repo_relative(evidence_path, root) or str(evidence_path),
        "entry_count": len(entries),
        "pins_match": True,
        "entries": entries,
        "withheld_claims": [
            "This verifier hashes committed M5 golden files only.",
            "This verifier does not run AMFlow numerics or create new retained captures.",
            "This verifier does not widen M5, M6, M7, or release-readiness claims.",
        ],
    }


def write_minimal_surface(
    root: Path,
    *,
    files: dict[str, bytes],
    comparison_to_golden: dict[str, str],
) -> Path:
    for relative_path, content in files.items():
        path = root / relative_path
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content)

    comparisons: list[str] = []
    for index, (comparison_relative, golden_relative) in enumerate(comparison_to_golden.items()):
        comparison_path = root / comparison_relative
        comparison_path.parent.mkdir(parents=True, exist_ok=True)
        comparison_path.write_text(
            json.dumps(
                {
                    "schema_version": 1,
                    "passed": True,
                    "amflow_golden": golden_relative,
                    "compared_coefficient_count": 1,
                    "passed_coefficient_count": 1,
                    "tolerance_digits": 30,
                    "failures": [],
                },
                indent=2,
                sort_keys=True,
            )
            + "\n",
            encoding="utf-8",
        )
        comparisons.append(comparison_relative)

    evidence_path = root / DEFAULT_EVIDENCE_SUMMARY
    evidence_path.parent.mkdir(parents=True, exist_ok=True)
    evidence_path.write_text(
        json.dumps(
            {
                "schema_version": 1,
                "scope": "m5-feature-surface",
                "m0b_accepted": True,
                "examples": [
                    {
                        "id": f"example_{index}",
                        "promoted_golden": True,
                        "coefficient_bearing": True,
                        "evidence_kind": "live-solver-path",
                        "comparison_summary": Path(comparison).relative_to(
                            "tools/reference-harness/specs/m5"
                        ).as_posix(),
                        "blockers": [],
                    }
                    for index, comparison in enumerate(comparisons)
                ],
                "runtime_features": [],
            },
            indent=2,
            sort_keys=True,
        )
        + "\n",
        encoding="utf-8",
    )
    return evidence_path


def rejected(callable_obj: Callable[[], Any], expected: str) -> bool:
    try:
        callable_obj()
    except RuntimeError as error:
        return expected in str(error)
    return False


def run_self_check() -> None:
    with tempfile.TemporaryDirectory(prefix="amflow-m5-golden-pins-self-check-") as tmp:
        root = Path(tmp)
        golden_a = (
            "tools/reference-harness/specs/m5/lane1/goldens/"
            "alpha.scoped-golden.txt"
        )
        golden_b = (
            "tools/reference-harness/specs/m5/lane2/goldens/"
            "beta.scoped-golden.txt"
        )
        evidence_path = write_minimal_surface(
            root,
            files={golden_a: b"alpha\n", golden_b: b"beta\n"},
            comparison_to_golden={
                "tools/reference-harness/specs/m5/comparisons/lane1/alpha.compare.json": golden_a,
                "tools/reference-harness/specs/m5/comparisons/lane2/beta.compare.json": golden_b,
            },
        )
        valid_pins = [
            {"path": golden_a, "sha256": hashlib.sha256(b"alpha\n").hexdigest()},
            {"path": golden_b, "sha256": hashlib.sha256(b"beta\n").hexdigest()},
        ]
        summary = verify_m5_golden_pins(
            root=root,
            evidence_summary=evidence_path,
            raw_pins=valid_pins,
        )
        if summary["entry_count"] != 2 or not summary["pins_match"]:
            raise RuntimeError("valid M5 golden pin self-check should pass")

        stale_pins = [dict(pin) for pin in valid_pins]
        stale_pins[0]["sha256"] = "0" * 64
        if not rejected(
            lambda: verify_m5_golden_pins(
                root=root,
                evidence_summary=evidence_path,
                raw_pins=stale_pins,
            ),
            "M5 golden fingerprint drift",
        ):
            raise RuntimeError("stale M5 golden digest self-check should fail")

        if not rejected(
            lambda: verify_m5_golden_pins(
                root=root,
                evidence_summary=evidence_path,
                raw_pins=valid_pins[:1],
            ),
            "missing pins",
        ):
            raise RuntimeError("missing M5 golden pin self-check should fail")

        extra_pins = valid_pins + [
            {
                "path": (
                    "tools/reference-harness/specs/m5/lane3/goldens/"
                    "extra.scoped-golden.txt"
                ),
                "sha256": hashlib.sha256(b"extra\n").hexdigest(),
            }
        ]
        if not rejected(
            lambda: verify_m5_golden_pins(
                root=root,
                evidence_summary=evidence_path,
                raw_pins=extra_pins,
            ),
            "extra pins",
        ):
            raise RuntimeError("extra M5 golden pin self-check should fail")

        unreferenced = (
            root
            / "tools/reference-harness/specs/m5/lane4/goldens/"
            "unreferenced.scoped-golden.txt"
        )
        unreferenced.parent.mkdir(parents=True, exist_ok=True)
        unreferenced.write_text("unreferenced\n", encoding="utf-8")
        if not rejected(
            lambda: verify_m5_golden_pins(
                root=root,
                evidence_summary=evidence_path,
                raw_pins=valid_pins,
            ),
            "not referenced by surface",
        ):
            raise RuntimeError("unreferenced committed M5 golden self-check should fail")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=repo_root(), help="Repository root")
    parser.add_argument(
        "--evidence-summary",
        type=Path,
        default=DEFAULT_EVIDENCE_SUMMARY,
        help="Current M5 feature evidence summary",
    )
    parser.add_argument("--verify", action="store_true", help="Verify committed pins")
    parser.add_argument("--self-check", action="store_true", help="Run synthetic failure checks")
    parser.add_argument("--json", action="store_true", help="Print the verifier summary as JSON")
    args = parser.parse_args()

    if args.self_check:
        run_self_check()

    if args.verify or not args.self_check:
        summary = verify_m5_golden_pins(
            root=args.root,
            evidence_summary=args.evidence_summary,
        )
        if args.json:
            print(json.dumps(summary, indent=2, sort_keys=True))
        else:
            print(
                "M5 golden fingerprint pins verified: "
                f"{summary['entry_count']} committed golden file(s)"
            )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
