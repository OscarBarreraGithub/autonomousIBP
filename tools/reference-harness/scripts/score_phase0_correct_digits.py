#!/usr/bin/env python3
"""Score candidate phase-0 numerics against retained reference outputs."""

from __future__ import annotations

import argparse
import json
import re
import shutil
import tempfile
from dataclasses import dataclass
from decimal import Decimal, ROUND_FLOOR, localcontext
from pathlib import Path
from typing import Any

from compare_phase0_results_to_reference import (
    expect,
    expect_path_within_root,
    load_output_entries,
    load_phase0_scaffold,
    load_reference_packet,
    repo_root,
    rewrite_candidate_result_manifest_pointer,
    select_benchmark_ids,
    write_candidate_packet,
    write_json,
    write_reference_packet,
)
from freeze_phase0_goldens import load_json


APPROXIMATE_LITERAL_RE = re.compile(
    r"(?:\d+\.\d*|\d*\.\d+|\d+)`[0-9]+(?:\.[0-9]+)?(?:\*\^[+-]?\d+)?"
)
APPROXIMATE_LITERAL_PARSER_RE = re.compile(
    r"(?P<mantissa>(?:\d+\.\d*|\d*\.\d+|\d+))`"
    r"(?P<precision>[0-9]+(?:\.[0-9]+)?)"
    r"(?P<exponent>\*\^[+-]?\d+)?"
)

STRUCTURAL_ONLY = "exact-structural-only"
APPROXIMATE_NUMERIC = "approximate-numeric-literals"
DIGIT_THRESHOLD_MET = "digit-threshold-met"
DIGIT_THRESHOLD_FAILED = "digit-threshold-failed"


@dataclass(frozen=True)
class ApproximateLiteral:
    raw: str
    value: Decimal
    precision_digits: int
    start: int
    end: int


def floor_precision_digits(raw_precision: str) -> int:
    digits = int(Decimal(raw_precision).to_integral_value(rounding=ROUND_FLOOR))
    expect(digits >= 0, f"approximate literal precision must be nonnegative, got {raw_precision!r}")
    return digits


def parse_approximate_literal(match: re.Match[str]) -> ApproximateLiteral:
    raw = match.group(0)
    parsed = APPROXIMATE_LITERAL_PARSER_RE.fullmatch(raw)
    expect(parsed is not None, f"unsupported approximate literal token: {raw!r}")
    mantissa = parsed.group("mantissa")
    precision_digits = floor_precision_digits(parsed.group("precision"))
    exponent = parsed.group("exponent")
    value = Decimal(mantissa)
    if exponent is not None:
        value *= Decimal(10) ** int(exponent[2:])
    return ApproximateLiteral(
        raw=raw,
        value=value,
        precision_digits=precision_digits,
        start=match.start(),
        end=match.end(),
    )


def extract_approximate_literals(text: str) -> list[ApproximateLiteral]:
    return [parse_approximate_literal(match) for match in APPROXIMATE_LITERAL_RE.finditer(text)]


def approximate_literal_skeleton(text: str) -> str:
    return APPROXIMATE_LITERAL_RE.sub("<approx>", text)


def score_literal_pair(reference: ApproximateLiteral, candidate: ApproximateLiteral) -> int:
    available_digits = min(reference.precision_digits, candidate.precision_digits)
    if reference.value == candidate.value:
        return available_digits

    absolute_error = abs(candidate.value - reference.value)
    expect(absolute_error != 0, "non-identical literal pair must keep a nonzero absolute error")

    with localcontext() as context:
        context.prec = max(available_digits + 40, 200)
        if reference.value == 0:
            digits = int((-absolute_error.log10()).to_integral_value(rounding=ROUND_FLOOR))
        else:
            relative_error = absolute_error / abs(reference.value)
            digits = int((-relative_error.log10()).to_integral_value(rounding=ROUND_FLOOR))
    return max(0, min(available_digits, digits))


def score_output_texts(
    *,
    benchmark_id: str,
    output_name: str,
    reference_text: str,
    candidate_text: str,
    required_digits: int,
) -> dict[str, Any]:
    reference_literals = extract_approximate_literals(reference_text)
    candidate_literals = extract_approximate_literals(candidate_text)

    if not reference_literals and not candidate_literals:
        expect(
            candidate_text == reference_text,
            f"candidate canonical text for {benchmark_id}:{output_name} must match the retained "
            "reference exactly when the output has no approximate numeric literals",
        )
        return {
            "name": output_name,
            "scoring_mode": STRUCTURAL_ONLY,
            "approximate_literal_count": 0,
            "skeleton_matches_reference": True,
            "minimum_observed_correct_digits": None,
            "required_minimum_correct_digits": required_digits,
            "digit_threshold_met": None,
            "candidate_text_matches_reference": True,
        }

    expect(
        len(candidate_literals) == len(reference_literals),
        f"candidate canonical text for {benchmark_id}:{output_name} must keep the retained "
        "approximate literal count",
    )
    expect(
        approximate_literal_skeleton(candidate_text) == approximate_literal_skeleton(reference_text),
        f"candidate canonical text for {benchmark_id}:{output_name} must preserve the retained "
        "nonnumeric skeleton",
    )

    literal_scores = [
        score_literal_pair(reference_literal, candidate_literal)
        for reference_literal, candidate_literal in zip(reference_literals, candidate_literals)
    ]
    minimum_correct_digits = min(literal_scores)
    return {
        "name": output_name,
        "scoring_mode": APPROXIMATE_NUMERIC,
        "approximate_literal_count": len(reference_literals),
        "skeleton_matches_reference": True,
        "minimum_observed_correct_digits": minimum_correct_digits,
        "required_minimum_correct_digits": required_digits,
        "digit_threshold_met": minimum_correct_digits >= required_digits,
        "candidate_text_matches_reference": candidate_text == reference_text,
        "literal_correct_digits": literal_scores,
    }


def score_selected_benchmarks(
    *,
    reference_packet: dict[str, Any],
    candidate_root: Path,
    benchmark_ids: list[str],
) -> list[dict[str, Any]]:
    expect(candidate_root.is_dir(), f"candidate root must exist: {candidate_root}")
    benchmark_summaries: list[dict[str, Any]] = []
    for benchmark_id in benchmark_ids:
        reference_benchmark = reference_packet["benchmarks"][benchmark_id]
        scaffold_entry = reference_benchmark["scaffold"]
        candidate_result_manifest = (
            candidate_root / "results" / "phase0" / benchmark_id / "result-manifest.json"
        )
        expect(
            candidate_result_manifest.exists(),
            f"candidate result manifest missing for {benchmark_id} in {candidate_root}",
        )
        expect_path_within_root(
            candidate_result_manifest,
            candidate_root,
            f"candidate result manifest path for {benchmark_id}",
        )
        result_manifest = load_json(candidate_result_manifest)
        expect(
            str(result_manifest.get("benchmark_id", "")).strip() == benchmark_id,
            f"candidate result manifest benchmark_id must match {benchmark_id}",
        )
        primary_run_manifest = Path(str(result_manifest.get("primary_run_manifest", "")))
        expect(
            primary_run_manifest.exists(),
            f"candidate primary run manifest missing for {benchmark_id} in {candidate_root}",
        )
        expect_path_within_root(
            primary_run_manifest,
            candidate_root,
            f"candidate primary run manifest path for {benchmark_id}",
        )
        run_manifest = load_json(primary_run_manifest)
        expect(
            str(run_manifest.get("benchmark_id", "")).strip() == benchmark_id,
            f"candidate primary run manifest benchmark_id must match {benchmark_id}",
        )
        candidate_outputs = load_output_entries(
            run_manifest.get("outputs", []),
            label=f"candidate outputs for {benchmark_id}",
        )
        candidate_output_names = set(candidate_outputs)
        reference_output_names = set(reference_benchmark["outputs"])
        expect(
            candidate_output_names == reference_output_names,
            f"candidate outputs for {benchmark_id} must match the retained reference output set",
        )

        output_summaries: list[dict[str, Any]] = []
        for output_name in sorted(reference_benchmark["outputs"]):
            candidate_output = candidate_outputs[output_name]
            reference_output = reference_benchmark["outputs"][output_name]
            expect_path_within_root(
                candidate_output["path"],
                candidate_root,
                f"candidate output path for {benchmark_id}:{output_name}",
            )
            expect(
                candidate_output["path"].exists(),
                f"candidate output path must exist for {benchmark_id}:{output_name}",
            )
            expect_path_within_root(
                candidate_output["canonical_text"],
                candidate_root,
                f"candidate canonical text path for {benchmark_id}:{output_name}",
            )
            expect(
                candidate_output["canonical_text"].exists(),
                f"candidate canonical text path must exist for {benchmark_id}:{output_name}",
            )
            reference_text = reference_output["canonical_text"].read_text(encoding="utf-8")
            candidate_text = candidate_output["canonical_text"].read_text(encoding="utf-8")
            output_summary = score_output_texts(
                benchmark_id=benchmark_id,
                output_name=output_name,
                reference_text=reference_text,
                candidate_text=candidate_text,
                required_digits=scaffold_entry["minimum_correct_digits"],
            )
            output_summary["reference_canonical_text"] = str(reference_output["canonical_text"])
            output_summary["candidate_canonical_text"] = str(candidate_output["canonical_text"])
            output_summaries.append(output_summary)

        numeric_outputs = [
            output_summary
            for output_summary in output_summaries
            if output_summary["scoring_mode"] == APPROXIMATE_NUMERIC
        ]
        expect(
            numeric_outputs,
            f"candidate outputs for {benchmark_id} must expose at least one approximate numeric "
            "canonical output on the reviewed scorer path",
        )
        minimum_correct_digits = min(
            int(output_summary["minimum_observed_correct_digits"])
            for output_summary in numeric_outputs
        )
        all_numeric_outputs_meet_threshold = all(
            bool(output_summary["digit_threshold_met"]) for output_summary in numeric_outputs
        )
        benchmark_summaries.append(
            {
                "benchmark_id": benchmark_id,
                "status": (
                    DIGIT_THRESHOLD_MET
                    if all_numeric_outputs_meet_threshold
                    else DIGIT_THRESHOLD_FAILED
                ),
                "reference_packet_label": reference_packet["packet_label"],
                "reference_capture_state": reference_packet["capture_state"],
                "required_capture": scaffold_entry["required_capture"],
                "current_evidence_state": scaffold_entry["current_evidence_state"],
                "optional_capture_packet": scaffold_entry["optional_capture_packet"],
                "next_runtime_lane": scaffold_entry["next_runtime_lane"],
                "digit_threshold_profile": scaffold_entry["digit_threshold_profile"],
                "minimum_correct_digits": scaffold_entry["minimum_correct_digits"],
                "failure_code_profile": scaffold_entry["failure_code_profile"],
                "required_failure_codes": scaffold_entry["required_failure_codes"],
                "regression_profile": scaffold_entry["regression_profile"],
                "known_regression_families": scaffold_entry["known_regression_families"],
                "reference_golden_manifest": str(reference_benchmark["golden_manifest"]),
                "candidate_result_manifest": str(candidate_result_manifest),
                "candidate_primary_run_manifest": str(primary_run_manifest),
                "reference_output_names": sorted(reference_benchmark["outputs"]),
                "candidate_output_names": sorted(candidate_outputs),
                "numeric_output_names": [
                    output_summary["name"]
                    for output_summary in numeric_outputs
                ],
                "structural_only_output_names": [
                    output_summary["name"]
                    for output_summary in output_summaries
                    if output_summary["scoring_mode"] == STRUCTURAL_ONLY
                ],
                "numeric_output_count": len(numeric_outputs),
                "structural_only_output_count": len(output_summaries) - len(numeric_outputs),
                "minimum_observed_correct_digits": minimum_correct_digits,
                "all_numeric_outputs_meet_threshold": all_numeric_outputs_meet_threshold,
                "outputs": output_summaries,
            }
        )
    return benchmark_summaries


def score_phase0_correct_digits(
    *,
    reference_root: Path,
    candidate_root: Path,
    benchmark_ids: list[str] | None,
    qualification_path: Path,
) -> dict[str, Any]:
    scaffold_entries = load_phase0_scaffold(qualification_path)
    reference_packet = load_reference_packet(
        reference_root=reference_root,
        scaffold_entries=scaffold_entries,
    )
    selected_benchmark_ids = select_benchmark_ids(reference_packet, benchmark_ids)
    benchmark_summaries = score_selected_benchmarks(
        reference_packet=reference_packet,
        candidate_root=candidate_root,
        benchmark_ids=selected_benchmark_ids,
    )
    return {
        "schema_version": 1,
        "qualification_path": str(qualification_path),
        "reference_root": str(reference_root),
        "reference_packet_label": reference_packet["packet_label"],
        "reference_capture_state": reference_packet["capture_state"],
        "candidate_root": str(candidate_root),
        "selected_benchmark_ids": selected_benchmark_ids,
        "reference_benchmarks_pass_retained_capture_checks": True,
        "candidate_result_manifests_exist": True,
        "candidate_primary_run_manifests_exist": True,
        "candidate_output_names_match_reference": True,
        "candidate_numeric_literal_skeletons_match_reference": True,
        "digit_threshold_profiles_reported": True,
        "required_failure_code_profiles_reported": True,
        "regression_profiles_reported": True,
        "all_selected_benchmarks_meet_digit_thresholds": all(
            benchmark["all_numeric_outputs_meet_threshold"] for benchmark in benchmark_summaries
        ),
        "benchmarks": benchmark_summaries,
    }


def overwrite_reference_output_text(
    *,
    root: Path,
    benchmark_id: str,
    output_name: str,
    text: str,
) -> None:
    path = root / "goldens" / "phase0" / benchmark_id / "captured" / "canonical" / (
        output_name + ".canonical.txt"
    )
    path.write_text(text + "\n", encoding="utf-8")


def overwrite_candidate_output_text(
    *,
    root: Path,
    benchmark_id: str,
    output_name: str,
    text: str,
) -> None:
    path = root / "results" / "phase0" / benchmark_id / "primary" / "canonical" / (
        output_name + ".canonical.txt"
    )
    path.write_text(text + "\n", encoding="utf-8")


def require_reference_root(reference_root: Path | None) -> Path:
    expect(reference_root is not None, "--reference-root is required unless --self-check is set")
    return reference_root


def require_candidate_root(candidate_root: Path | None) -> Path:
    expect(candidate_root is not None, "--candidate-root is required unless --self-check is set")
    return candidate_root


def run_self_check(qualification_path: Path) -> dict[str, Any]:
    with tempfile.TemporaryDirectory(prefix="amflow-phase0-correct-digits-self-check-") as tmp:
        temp_root = Path(tmp)
        reference_root = temp_root / "phase0-reference-captured-20260419-required-set"
        matching_candidate_root = temp_root / "candidate-packet-match"
        summary_path = temp_root / "correct-digit-summary.json"
        benchmark_id = "automatic_loop"
        output_names = ["sol1", "diffeq"]
        output_hashes = {
            "sol1": "1111111111111111111111111111111111111111111111111111111111111111",
            "diffeq": "2222222222222222222222222222222222222222222222222222222222222222",
        }

        write_reference_packet(
            root=reference_root,
            benchmark_id=benchmark_id,
            output_names=output_names,
            output_hashes=output_hashes,
        )
        write_candidate_packet(
            root=matching_candidate_root,
            benchmark_id=benchmark_id,
            output_names=output_names,
            output_hashes=output_hashes,
        )

        numeric_reference = (
            "{sol1 -> "
            "1.1111111111111111111111111111111111111111111111111111111111111111`70.}"
        )
        structural_reference = "{diffeq -> {{(1 - 2*eps)/s, -s^(-1)}}}"
        overwrite_reference_output_text(
            root=reference_root,
            benchmark_id=benchmark_id,
            output_name="sol1",
            text=numeric_reference,
        )
        overwrite_reference_output_text(
            root=reference_root,
            benchmark_id=benchmark_id,
            output_name="diffeq",
            text=structural_reference,
        )
        overwrite_candidate_output_text(
            root=matching_candidate_root,
            benchmark_id=benchmark_id,
            output_name="sol1",
            text=numeric_reference,
        )
        overwrite_candidate_output_text(
            root=matching_candidate_root,
            benchmark_id=benchmark_id,
            output_name="diffeq",
            text=structural_reference,
        )

        matching_summary = score_phase0_correct_digits(
            reference_root=reference_root,
            candidate_root=matching_candidate_root,
            benchmark_ids=None,
            qualification_path=qualification_path,
        )
        write_json(summary_path, matching_summary)

        small_drift_root = temp_root / "candidate-packet-small-drift"
        shutil.copytree(matching_candidate_root, small_drift_root)
        rewrite_candidate_result_manifest_pointer(small_drift_root, benchmark_id)
        overwrite_candidate_output_text(
            root=small_drift_root,
            benchmark_id=benchmark_id,
            output_name="sol1",
            text=(
                "{sol1 -> "
                "1.1111111111111111111111111111111111111111111111111111111111111112`70.}"
            ),
        )
        small_drift_summary = score_phase0_correct_digits(
            reference_root=reference_root,
            candidate_root=small_drift_root,
            benchmark_ids=None,
            qualification_path=qualification_path,
        )

        threshold_failure_root = temp_root / "candidate-packet-below-threshold"
        shutil.copytree(matching_candidate_root, threshold_failure_root)
        rewrite_candidate_result_manifest_pointer(threshold_failure_root, benchmark_id)
        overwrite_candidate_output_text(
            root=threshold_failure_root,
            benchmark_id=benchmark_id,
            output_name="sol1",
            text=(
                "{sol1 -> "
                "1.1111111111111111111111111111211111111111111111111111111111111111`70.}"
            ),
        )
        threshold_failure_summary = score_phase0_correct_digits(
            reference_root=reference_root,
            candidate_root=threshold_failure_root,
            benchmark_ids=None,
            qualification_path=qualification_path,
        )

        skeleton_mismatch_root = temp_root / "candidate-packet-skeleton-mismatch"
        shutil.copytree(matching_candidate_root, skeleton_mismatch_root)
        rewrite_candidate_result_manifest_pointer(skeleton_mismatch_root, benchmark_id)
        overwrite_candidate_output_text(
            root=skeleton_mismatch_root,
            benchmark_id=benchmark_id,
            output_name="sol1",
            text=(
                "{sol1 -> {"
                "1.1111111111111111111111111111111111111111111111111111111111111111`70.}}"
            ),
        )

        missing_reference_root_rejected = False
        try:
            require_reference_root(None)
        except RuntimeError as error:
            missing_reference_root_rejected = "--reference-root is required" in str(error)

        missing_candidate_root_rejected = False
        try:
            require_candidate_root(None)
        except RuntimeError as error:
            missing_candidate_root_rejected = "--candidate-root is required" in str(error)

        subunit_precision_output = score_output_texts(
            benchmark_id=benchmark_id,
            output_name="sol1",
            reference_text="{sol1 -> 1.234`0.5}",
            candidate_text="{sol1 -> 1.234`0.5}",
            required_digits=1,
        )

        skeleton_mismatch_rejected = False
        try:
            score_phase0_correct_digits(
                reference_root=reference_root,
                candidate_root=skeleton_mismatch_root,
                benchmark_ids=None,
                qualification_path=qualification_path,
            )
        except RuntimeError as error:
            skeleton_mismatch_rejected = "must preserve the retained nonnumeric skeleton" in str(
                error
            )

        escaping_run_manifest_root = temp_root / "candidate-packet-escaping-run-manifest"
        shutil.copytree(matching_candidate_root, escaping_run_manifest_root)
        rewrite_candidate_result_manifest_pointer(escaping_run_manifest_root, benchmark_id)
        escaping_run_manifest = temp_root / "escaping-run-manifest.json"
        write_json(
            escaping_run_manifest,
            load_json(
                escaping_run_manifest_root
                / "results"
                / "phase0"
                / benchmark_id
                / "primary"
                / "run-manifest.json"
            ),
        )
        escaping_result_manifest_path = (
            escaping_run_manifest_root / "results" / "phase0" / benchmark_id / "result-manifest.json"
        )
        escaping_result_manifest = load_json(escaping_result_manifest_path)
        escaping_result_manifest["primary_run_manifest"] = str(escaping_run_manifest)
        write_json(escaping_result_manifest_path, escaping_result_manifest)
        escaping_run_manifest_rejected = False
        try:
            score_phase0_correct_digits(
                reference_root=reference_root,
                candidate_root=escaping_run_manifest_root,
                benchmark_ids=None,
                qualification_path=qualification_path,
            )
        except RuntimeError as error:
            escaping_run_manifest_rejected = (
                "candidate primary run manifest path" in str(error)
                and "must stay under" in str(error)
            )

        escaping_canonical_text_root = temp_root / "candidate-packet-escaping-canonical-text"
        shutil.copytree(matching_candidate_root, escaping_canonical_text_root)
        rewrite_candidate_result_manifest_pointer(escaping_canonical_text_root, benchmark_id)
        escaping_run_manifest_path = (
            escaping_canonical_text_root
            / "results"
            / "phase0"
            / benchmark_id
            / "primary"
            / "run-manifest.json"
        )
        escaping_run_payload = load_json(escaping_run_manifest_path)
        escaping_canonical_text = temp_root / "escaping-sol1.canonical.txt"
        escaping_canonical_text.write_text(numeric_reference + "\n", encoding="utf-8")
        for output in escaping_run_payload.get("outputs", []):
            if str(output.get("name", "")).strip() == "sol1":
                output["canonical_text"] = str(escaping_canonical_text)
        write_json(escaping_run_manifest_path, escaping_run_payload)
        escaping_canonical_text_rejected = False
        try:
            score_phase0_correct_digits(
                reference_root=reference_root,
                candidate_root=escaping_canonical_text_root,
                benchmark_ids=None,
                qualification_path=qualification_path,
            )
        except RuntimeError as error:
            escaping_canonical_text_rejected = (
                "candidate canonical text path" in str(error)
                and "must stay under" in str(error)
            )

        benchmark_summary = matching_summary["benchmarks"][0]
        numeric_output = next(
            output for output in benchmark_summary["outputs"] if output["name"] == "sol1"
        )
        structural_output = next(
            output for output in benchmark_summary["outputs"] if output["name"] == "diffeq"
        )
        return {
            "matching_candidate_meets_thresholds": (
                matching_summary["all_selected_benchmarks_meet_digit_thresholds"]
                and benchmark_summary["status"] == DIGIT_THRESHOLD_MET
            ),
            "profiles_reported_from_scaffold": (
                benchmark_summary["digit_threshold_profile"] == "core-package-family-default"
                and benchmark_summary["minimum_correct_digits"] == 50
                and "insufficient_precision" in benchmark_summary["required_failure_codes"]
                and "unexpected master sets in Kira interface"
                in benchmark_summary["known_regression_families"]
            ),
            "small_numeric_drift_meets_threshold": (
                small_drift_summary["all_selected_benchmarks_meet_digit_thresholds"]
                and small_drift_summary["benchmarks"][0]["minimum_observed_correct_digits"] >= 50
            ),
            "threshold_failure_detected": (
                not threshold_failure_summary["all_selected_benchmarks_meet_digit_thresholds"]
                and threshold_failure_summary["benchmarks"][0]["status"] == DIGIT_THRESHOLD_FAILED
            ),
            "structural_only_output_preserved": (
                structural_output["scoring_mode"] == STRUCTURAL_ONLY
                and structural_output["candidate_text_matches_reference"]
            ),
            "numeric_output_scored": (
                numeric_output["scoring_mode"] == APPROXIMATE_NUMERIC
                and numeric_output["minimum_observed_correct_digits"] == 70
            ),
            "subunit_precision_literal_scored": (
                subunit_precision_output["scoring_mode"] == APPROXIMATE_NUMERIC
                and subunit_precision_output["minimum_observed_correct_digits"] == 0
                and not subunit_precision_output["digit_threshold_met"]
            ),
            "skeleton_mismatch_rejected": skeleton_mismatch_rejected,
            "missing_reference_root_rejected": missing_reference_root_rejected,
            "missing_candidate_root_rejected": missing_candidate_root_rejected,
            "escaping_run_manifest_rejected": escaping_run_manifest_rejected,
            "escaping_canonical_text_rejected": escaping_canonical_text_rejected,
            "summary_written": summary_path.exists(),
        }


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--reference-root",
        type=Path,
        help="Retained reference packet root that publishes goldens and comparison summaries",
    )
    parser.add_argument(
        "--candidate-root",
        type=Path,
        help="Candidate packet root that publishes result-manifest and primary run-manifest files",
    )
    parser.add_argument(
        "--benchmark-id",
        action="append",
        help="Optional benchmark id to score; defaults to all retained benchmarks in the reference packet",
    )
    parser.add_argument(
        "--qualification-path",
        type=Path,
        help="Qualification scaffold JSON path",
    )
    parser.add_argument(
        "--summary-path",
        type=Path,
        help="Optional output file for the correct-digit summary",
    )
    parser.add_argument(
        "--self-check",
        action="store_true",
        help="Run a synthetic correct-digit self-check",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    qualification_path = (
        args.qualification_path
        if args.qualification_path is not None
        else repo_root() / "tools" / "reference-harness" / "templates" / "qualification-benchmarks.json"
    )

    if args.self_check:
        summary = run_self_check(qualification_path)
        if args.summary_path is not None:
            write_json(args.summary_path, summary)
        print(json.dumps(summary, indent=2, sort_keys=True))
        return 0

    summary = score_phase0_correct_digits(
        reference_root=require_reference_root(args.reference_root),
        candidate_root=require_candidate_root(args.candidate_root),
        benchmark_ids=args.benchmark_id,
        qualification_path=qualification_path,
    )
    if args.summary_path is not None:
        write_json(args.summary_path, summary)
    print(json.dumps(summary, indent=2, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
