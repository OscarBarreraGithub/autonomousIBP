#!/usr/bin/env python3
"""Diagnose the four b61n row 5/6 comparator targets.

The diagnostic is intentionally read-only: it consumes one C++ solve-series
result and one cpp-vs-AMFlow comparator summary, then reports whether the
current evidence points at the sample-space matcher, coefficient-state
publication gate, transport precision, or AMFlow reference precision.
"""

from __future__ import annotations

import argparse
import copy
import json
import re
import sys
import tempfile
from pathlib import Path
from typing import Any


TARGETS: tuple[tuple[str, int], ...] = (
    ("box[1,0,1,1]", 0),
    ("box[1,1,1,1]", -2),
    ("box[1,1,1,1]", -1),
    ("box[1,1,1,1]", 0),
)


def expect(condition: bool, message: str) -> None:
  if not condition:
    raise RuntimeError(message)


def load_json(path: Path) -> dict[str, Any]:
  with path.open("r", encoding="utf-8") as stream:
    payload = json.load(stream)
  expect(isinstance(payload, dict), f"{path} must contain a JSON object")
  return payload


def write_json(payload: dict[str, Any], path: Path | None) -> None:
  raw = json.dumps(payload, indent=2, sort_keys=True)
  if path is None:
    print(raw)
    return
  path.parent.mkdir(parents=True, exist_ok=True)
  path.write_text(raw + "\n", encoding="utf-8")


def comparison_lookup(comparison: dict[str, Any]) -> dict[tuple[str, int], dict[str, Any]]:
  lookup: dict[tuple[str, int], dict[str, Any]] = {}
  integrals = comparison.get("integrals")
  expect(isinstance(integrals, list), "comparison integrals must be a list")
  for integral_entry in integrals:
    expect(isinstance(integral_entry, dict), "comparison integral entry must be an object")
    integral = integral_entry.get("integral")
    expect(isinstance(integral, str), "comparison integral entry must name an integral")
    coefficients = integral_entry.get("coefficients")
    expect(isinstance(coefficients, list), f"{integral} coefficients must be a list")
    for coefficient in coefficients:
      expect(isinstance(coefficient, dict), f"{integral} coefficient must be an object")
      order = coefficient.get("order")
      expect(isinstance(order, int), f"{integral} coefficient order must be an integer")
      lookup[(integral, order)] = coefficient
  return lookup


def cpp_lookup(cpp_result: dict[str, Any]) -> dict[tuple[str, int], dict[str, Any]]:
  lookup: dict[tuple[str, int], dict[str, Any]] = {}
  results = cpp_result.get("results")
  expect(isinstance(results, list), "C++ result results must be a list")
  for result in results:
    expect(isinstance(result, dict), "C++ result entry must be an object")
    integral = result.get("integral")
    expect(isinstance(integral, str), "C++ result entry must name an integral")
    epsilon_orders = result.get("epsilon_orders")
    expect(isinstance(epsilon_orders, list), f"{integral} epsilon_orders must be a list")
    for coefficient in epsilon_orders:
      expect(isinstance(coefficient, dict), f"{integral} epsilon coefficient must be an object")
      order = coefficient.get("order")
      expect(isinstance(order, int), f"{integral} epsilon order must be an integer")
      lookup[(integral, order)] = coefficient
  return lookup


def assignment_values(summary: str, key: str) -> list[str]:
  return [match.group(1).strip() for match in re.finditer(
      rf"(?:^|[; ]){re.escape(key)}=([^;}}]+)", summary)]


def first_assignment(summary: str, key: str) -> str | None:
  values = assignment_values(summary, key)
  return values[0] if values else None


def contains(summary: str, needle: str) -> bool:
  return needle in summary


def bool_field(payload: dict[str, Any], field: str, target_label: str) -> bool:
  value = payload.get(field)
  expect(isinstance(value, bool), f"{target_label} {field} must be boolean")
  return value


def validate_reference_floor_metadata(
    comparison_entry: dict[str, Any],
    integral: str,
    order: int,
    matched_to_reference_floor: bool,
    matched_to_tolerance_digits: bool,
    verdict: Any,
) -> None:
  target_label = f"{integral} eps^{order}"
  if not matched_to_reference_floor:
    return
  expect(
      matched_to_tolerance_digits is False,
      f"{target_label} reference-floor match must not also be a tolerance match")
  expect(
      verdict == "matched-to-reference-floor",
      f"{target_label} reference-floor match must use matched-to-reference-floor verdict")
  for field in ("reference_floor_real_digits", "reference_floor_imag_digits"):
    value = comparison_entry.get(field)
    expect(
        isinstance(value, int),
        f"{target_label} reference-floor match must include integer {field}")
  for field in ("reference_floor_id", "reference_floor_reason"):
    value = comparison_entry.get(field)
    expect(
        isinstance(value, str) and value.strip(),
        f"{target_label} reference-floor match must include non-empty {field}")


def summarize_target(
    target: tuple[str, int],
    comparison_by_target: dict[tuple[str, int], dict[str, Any]],
    cpp_by_target: dict[tuple[str, int], dict[str, Any]],
) -> dict[str, Any]:
  integral, order = target
  comparison_entry = comparison_by_target.get(target)
  expect(comparison_entry is not None, f"comparison is missing {integral} eps^{order}")
  cpp_entry = cpp_by_target.get(target)
  real_digits = comparison_entry.get("real_agreement_digits")
  imag_digits = comparison_entry.get("imag_agreement_digits")
  expect(isinstance(real_digits, int), f"{integral} eps^{order} real digits must be integer")
  expect(isinstance(imag_digits, int), f"{integral} eps^{order} imag digits must be integer")
  target_label = f"{integral} eps^{order}"
  matched_to_reference_floor = bool_field(
      comparison_entry, "matched_to_reference_floor", target_label)
  matched_to_tolerance_digits = bool_field(
      comparison_entry, "matched_to_tolerance_digits", target_label)
  verdict = comparison_entry.get("verdict")
  validate_reference_floor_metadata(
      comparison_entry,
      integral,
      order,
      matched_to_reference_floor,
      matched_to_tolerance_digits,
      verdict)
  return {
      "integral": integral,
      "eps_order": order,
      "passed": bool(comparison_entry.get("passed")),
      "matched_to_tolerance_digits": matched_to_tolerance_digits,
      "matched_to_reference_floor": matched_to_reference_floor,
      "verdict": verdict,
      "real_agreement_digits": real_digits,
      "imag_agreement_digits": imag_digits,
      "minimum_component_agreement_digits": min(real_digits, imag_digits),
      "reference_floor_real_digits": comparison_entry.get("reference_floor_real_digits"),
      "reference_floor_imag_digits": comparison_entry.get("reference_floor_imag_digits"),
      "reference_floor_id": comparison_entry.get("reference_floor_id"),
      "reference_floor_reason": comparison_entry.get("reference_floor_reason"),
      "cpp_present_in_comparator": bool(comparison_entry.get("cpp_present")),
      "cpp_emitted_order_in_result": cpp_entry is not None,
      "cpp_real": comparison_entry.get("cpp_real"),
      "cpp_imag": comparison_entry.get("cpp_imag"),
      "amflow_real": comparison_entry.get("amflow_real"),
      "amflow_imag": comparison_entry.get("amflow_imag"),
      "relative_error_abs": comparison_entry.get("relative_error_abs"),
      "real_relative_error_abs": comparison_entry.get("real_relative_error_abs"),
      "imag_relative_error_abs": comparison_entry.get("imag_relative_error_abs"),
  }


def diagnose(cpp_result: dict[str, Any], comparison: dict[str, Any]) -> dict[str, Any]:
  summary = cpp_result.get("summary", "")
  expect(isinstance(summary, str), "C++ result summary must be a string")
  comparison_by_target = comparison_lookup(comparison)
  cpp_by_target = cpp_lookup(cpp_result)
  target_summaries = [
      summarize_target(target, comparison_by_target, cpp_by_target)
      for target in TARGETS
  ]

  all_targets_failed = all(not target["passed"] for target in target_summaries)
  all_targets_matched_reference_floor = all(
      target["matched_to_reference_floor"] for target in target_summaries)
  reference_floor_target_count = sum(
      1 for target in target_summaries if target["matched_to_reference_floor"])
  target_min_digits = min(
      target["minimum_component_agreement_digits"] for target in target_summaries)

  coefficient_state_publication_seen = contains(
      summary, "b61n coefficient-state target publication")
  coefficient_state_publication_blocked = contains(
      summary, "b61n coefficient-state target publication blocked")
  coefficient_state_publication_failure_codes = assignment_values(summary, "failure_code")

  sample_space_matcher_applied = contains(
      summary, "coupled_frobenius_endpoint_matcher_applied=true")
  sample_space_matcher_solve = first_assignment(
      summary, "coupled_frobenius_boundary_condition_solve")
  free_constant_rows = first_assignment(summary, "coupled_frobenius_free_constant_rows")
  publication_materialized_count = first_assignment(summary, "materialized_node_count")
  finite_start_constructed = first_assignment(
      summary, "coefficient_state_finite_start_constructed")
  requested_public_target_count = first_assignment(summary, "requested_public_target_count")
  published_coefficient_count = first_assignment(summary, "published_coefficient_count")
  coefficient_target_graph_node_count = first_assignment(
      summary, "coefficient_target_graph_node_count")
  coefficient_target_graph_edge_count = first_assignment(
      summary, "coefficient_target_graph_edge_count")
  coefficient_target_graph_blocked_edge_count = first_assignment(
      summary, "coefficient_target_graph_blocked_edge_count")
  blocked_out_of_range_edge_count = first_assignment(
      summary, "blocked_out_of_range_edge_count")
  source_anchored_evolution_rows = first_assignment(
      summary, "source_anchored_evolution_rows")

  coeff_state_transport_values = assignment_values(summary, "coefficient_state_transport_applied")
  coeff_state_endpoint_values = assignment_values(
      summary, "coefficient_state_endpoint_matcher_applied")
  coeff_publication_values = assignment_values(
      summary, "target_coefficients_published_from_coefficient_state")
  sample_reconstruction_values = assignment_values(
      summary, "target_coefficients_reconstructed_from_epsilon_samples")

  row6_missing_laurent_orders = [
      target["eps_order"] for target in target_summaries
      if target["integral"] == "box[1,1,1,1]"
      and not target["cpp_emitted_order_in_result"]
  ]
  direct_targets_still_sample_reconstructed = "true" in sample_reconstruction_values

  publication_failure = (
      coefficient_state_publication_failure_codes[0]
      if coefficient_state_publication_failure_codes else None)

  active_blocker = (
      "coefficient-state publication gate blocks before direct target upsert"
      if coefficient_state_publication_blocked else
      "sample-space/post-endpoint coefficient path remains active for row 5/6 targets"
  )
  if publication_failure == "coefficient-state-publication-unclosed-target-graph":
    active_blocker += "; coefficient target graph still has blocked dependency edges"
  if coefficient_target_graph_blocked_edge_count is not None:
    active_blocker += (
        f"; blocked_target_graph_edges={coefficient_target_graph_blocked_edge_count}")
  if direct_targets_still_sample_reconstructed:
    active_blocker += "; emitted row 5/6 target coefficients are sample-reconstructed"
  if row6_missing_laurent_orders:
    active_blocker += (
        "; C++ result does not emit row 6 orders "
        + ", ".join(f"eps^{order}" for order in row6_missing_laurent_orders)
    )
  if all_targets_matched_reference_floor:
    active_blocker += (
        "; row 5/6 comparator verdict is matched-to-reference-floor, not "
        "matched-to-50-digit"
    )

  if all_targets_matched_reference_floor:
    amflow_reference_floor_classification = (
        "supported for these four row 5/6 targets: the comparator accepted them "
        "only through declared retained AMFlow reference floors, with component "
        "floors row5 eps^0=11/11, row6 eps^-2=46/46, row6 eps^-1=12/13, "
        "row6 eps^0=12/12")
    current_evidence_points_to = (
        "retained AMFlow reference-floor convergence for the four row 5/6 "
        "specific targets, while current emitted row 5/6 coefficients still come "
        "from sample reconstruction because direct coefficient-state publication "
        "is not active")
  else:
    amflow_reference_floor_classification = (
        "not supported by this comparison: no declared retained-reference floor "
        "matched every row 5/6 target")
    current_evidence_points_to = (
        "unclosed coefficient target graph plus sample-reconstructed row 5/6 "
        "target coefficients before direct coefficient-state publication")

  return {
      "schema_version": 1,
      "diagnostic_id": "b61n-row56-specific-target-diagnostic",
      "benchmark_id": comparison.get("benchmark_id", cpp_result.get("benchmark_id")),
      "cpp_result": comparison.get("cpp_result"),
      "amflow_golden": comparison.get("amflow_golden"),
      "tolerance_digits": comparison.get("tolerance_digits"),
      "comparison_summary": {
          "passed": comparison.get("passed"),
          "comparison_verdict": comparison.get("comparison_verdict"),
          "compared_coefficient_count": comparison.get("compared_coefficient_count"),
          "passed_coefficient_count": comparison.get("passed_coefficient_count"),
          "accepted_coefficient_count": comparison.get("accepted_coefficient_count"),
          "reference_floor_matched_coefficient_count": (
              comparison.get("reference_floor_matched_coefficient_count")),
          "minimum_digit_agreement": comparison.get("minimum_digit_agreement"),
          "all_row56_specific_targets_failed": all_targets_failed,
          "all_row56_specific_targets_matched_reference_floor": (
              all_targets_matched_reference_floor),
          "row56_specific_target_reference_floor_count": (
              reference_floor_target_count),
          "row56_specific_target_minimum_digit_agreement": target_min_digits,
      },
      "target_diagnostics": target_summaries,
      "runtime_path": {
          "sample_space_coupled_row_matcher_applied": sample_space_matcher_applied,
          "sample_space_boundary_condition_solve": sample_space_matcher_solve,
          "sample_space_free_constant_rows": free_constant_rows,
          "sample_space_source_anchored_evolution_rows": source_anchored_evolution_rows,
          "coefficient_state_publication_attempt_seen": coefficient_state_publication_seen,
          "coefficient_state_publication_blocked": coefficient_state_publication_blocked,
          "coefficient_state_publication_failure_codes": (
              coefficient_state_publication_failure_codes),
          "coefficient_state_finite_start_constructed": finite_start_constructed,
          "coefficient_state_publication_requested_public_target_count": (
              requested_public_target_count),
          "coefficient_state_publication_published_coefficient_count": (
              published_coefficient_count),
          "coefficient_state_publication_materialized_node_count": (
              publication_materialized_count),
          "coefficient_target_graph_node_count": coefficient_target_graph_node_count,
          "coefficient_target_graph_edge_count": coefficient_target_graph_edge_count,
          "coefficient_target_graph_blocked_edge_count": (
              coefficient_target_graph_blocked_edge_count),
          "blocked_out_of_range_edge_count": blocked_out_of_range_edge_count,
          "coefficient_state_transport_applied_values_seen": coeff_state_transport_values,
          "coefficient_state_endpoint_matcher_applied_values_seen": coeff_state_endpoint_values,
          "target_coefficients_published_from_coefficient_state_values_seen": (
              coeff_publication_values),
          "target_coefficients_reconstructed_from_epsilon_samples_values_seen": (
              sample_reconstruction_values),
          "direct_targets_still_sample_reconstructed": (
              direct_targets_still_sample_reconstructed),
          "row6_missing_laurent_orders_in_cpp_result": row6_missing_laurent_orders,
      },
      "classification": {
          "active_blocker": active_blocker,
          "matcher_converges_to_wrong_free_constants": (
              "plausible for the emitted sample-reconstructed row 5/6 coefficients, "
              "but not isolated as the direct coefficient-state route never reaches "
              "target publication"),
          "matcher_converges_correctly_but_transport_loses_precision": (
              "not the primary current classification: the public row 5/6 targets are "
              "still sample-reconstructed and the coefficient-state publication route "
              "is blocked by the target graph before a direct endpoint value is upserted"),
          "amflow_reference_only_11_digits": amflow_reference_floor_classification,
          "amflow_reference_floor_for_row56_targets": (
              amflow_reference_floor_classification),
          "current_evidence_points_to": current_evidence_points_to,
      },
      "source_references": {
          "post_endpoint_fit": "src/cli/main.cpp:11261",
          "publication_attempt": "src/cli/main.cpp:6486",
          "publication_gate": "src/runtime/b61n_coefficient_state_transport.cpp:526",
          "row56_target_graph": "src/runtime/b61n_coefficient_target_graph.cpp:633",
      },
  }


def run_self_check() -> None:
  with tempfile.TemporaryDirectory() as temp_raw:
    temp = Path(temp_raw)
    cpp_path = temp / "cpp.json"
    compare_path = temp / "compare.json"
    cpp_payload = {
        "schema_version": 1,
        "benchmark_id": "complex_kinematics",
        "summary": (
            "b61n coupled-row live contour propagation executed; "
            "coupled_frobenius_endpoint_matcher_applied=true; "
            "coupled_frobenius_boundary_condition_solve=2x2-match; "
            "coupled_frobenius_free_constant_rows=[5, 6]; "
            "b61n coefficient-state target publication blocked: diagnostic; "
            "failure_code=coefficient-state-publication-finite-start-missing; "
            "materialized_node_count=0; "
            "coefficient_state_finite_start_constructed=false; "
            "coefficient_state_transport_applied=false; "
            "coefficient_state_endpoint_matcher_applied=false; "
            "target_coefficients_published_from_coefficient_state=false; "
            "target_coefficients_reconstructed_from_epsilon_samples=true"
        ),
        "results": [
            {"integral": "box[1,0,1,1]",
             "epsilon_orders": [{"order": 0, "real_digits": "1.0", "imag_digits": "2.0"}]},
            {"integral": "box[1,1,1,1]",
             "epsilon_orders": [
                 {"order": -2, "real_digits": "1.0", "imag_digits": "1.0"},
                 {"order": -1, "real_digits": "2.0", "imag_digits": "2.0"},
                 {"order": 0, "real_digits": "3.0", "imag_digits": "4.0"},
             ]},
        ],
    }
    compare_payload = {
        "schema_version": 1,
        "benchmark_id": "complex_kinematics",
        "cpp_result": str(cpp_path),
        "amflow_golden": "golden.json",
        "tolerance_digits": 50,
        "passed": True,
        "comparison_verdict": "matched-to-reference-floor",
        "compared_coefficient_count": 14,
        "passed_coefficient_count": 10,
        "accepted_coefficient_count": 14,
        "reference_floor_matched_coefficient_count": 4,
        "minimum_digit_agreement": 11,
        "integrals": [
            {
                "integral": "box[1,0,1,1]",
                "coefficients": [
                    {"order": 0, "passed": True, "cpp_present": True,
                     "matched_to_reference_floor": True,
                     "matched_to_tolerance_digits": False,
                     "verdict": "matched-to-reference-floor",
                     "reference_floor_id": "b61n-row5-eps0-retained-amflow-floor",
                     "reference_floor_real_digits": 11,
                     "reference_floor_imag_digits": 11,
                     "reference_floor_reason": "synthetic retained AMFlow floor",
                     "real_agreement_digits": 11, "imag_agreement_digits": 11,
                     "cpp_real": "1", "cpp_imag": "2",
                     "amflow_real": "1.1", "amflow_imag": "2.1"},
                ],
            },
            {
                "integral": "box[1,1,1,1]",
                "coefficients": [
                    {"order": -2, "passed": True, "cpp_present": True,
                     "matched_to_reference_floor": True,
                     "matched_to_tolerance_digits": False,
                     "verdict": "matched-to-reference-floor",
                     "reference_floor_id": "b61n-row6-eps-2-retained-amflow-floor",
                     "reference_floor_real_digits": 46,
                     "reference_floor_imag_digits": 46,
                     "reference_floor_reason": "synthetic retained AMFlow floor",
                     "real_agreement_digits": 46, "imag_agreement_digits": 46,
                     "cpp_real": "1", "cpp_imag": "1",
                     "amflow_real": "1", "amflow_imag": "1"},
                    {"order": -1, "passed": True, "cpp_present": True,
                     "matched_to_reference_floor": True,
                     "matched_to_tolerance_digits": False,
                     "verdict": "matched-to-reference-floor",
                     "reference_floor_id": "b61n-row6-eps-1-retained-amflow-floor",
                     "reference_floor_real_digits": 12,
                     "reference_floor_imag_digits": 13,
                     "reference_floor_reason": "synthetic retained AMFlow floor",
                     "real_agreement_digits": 12, "imag_agreement_digits": 13,
                     "cpp_real": "2", "cpp_imag": "2",
                     "amflow_real": "2", "amflow_imag": "2"},
                    {"order": 0, "passed": True, "cpp_present": True,
                     "matched_to_reference_floor": True,
                     "matched_to_tolerance_digits": False,
                     "verdict": "matched-to-reference-floor",
                     "reference_floor_id": "b61n-row6-eps0-retained-amflow-floor",
                     "reference_floor_real_digits": 12,
                     "reference_floor_imag_digits": 12,
                     "reference_floor_reason": "synthetic retained AMFlow floor",
                     "real_agreement_digits": 12, "imag_agreement_digits": 12,
                     "cpp_real": "3", "cpp_imag": "4",
                     "amflow_real": "3.1", "amflow_imag": "4.1"},
                ],
            },
        ],
    }
    write_json(cpp_payload, cpp_path)
    write_json(compare_payload, compare_path)
    result = diagnose(load_json(cpp_path), load_json(compare_path))
    expect(result["comparison_summary"]["row56_specific_target_minimum_digit_agreement"] == 11,
           "self-check should preserve the row-specific minimum digit agreement")
    expect(result["runtime_path"]["coefficient_state_publication_blocked"],
           "self-check should detect the blocked publication gate")
    expect(result["runtime_path"]["row6_missing_laurent_orders_in_cpp_result"] == [],
           "self-check should preserve emitted row 6 Laurent orders")
    expect(result["comparison_summary"][
               "all_row56_specific_targets_matched_reference_floor"],
           "self-check should classify row 5/6 targets as reference-floor matches")
    expect(result["comparison_summary"][
               "row56_specific_target_reference_floor_count"] == 4,
           "self-check should count all four row 5/6 reference-floor matches")
    expect("retained AMFlow reference-floor convergence" in
           result["classification"]["current_evidence_points_to"],
           "self-check should classify the retained-reference floor without "
           "claiming a 50-digit match")

    missing_metadata_payload = copy.deepcopy(compare_payload)
    del missing_metadata_payload["integrals"][0]["coefficients"][0][
        "reference_floor_reason"]
    write_json(missing_metadata_payload, compare_path)
    try:
      diagnose(load_json(cpp_path), load_json(compare_path))
    except RuntimeError as error:
      expect("reference_floor_reason" in str(error),
             "missing floor metadata should identify the absent field")
    else:
      raise RuntimeError("self-check should reject a reference-floor target "
                         "with missing floor metadata")

    mixed_verdict_payload = copy.deepcopy(compare_payload)
    mixed_verdict_payload["integrals"][0]["coefficients"][0][
        "matched_to_tolerance_digits"] = True
    write_json(mixed_verdict_payload, compare_path)
    try:
      diagnose(load_json(cpp_path), load_json(compare_path))
    except RuntimeError as error:
      expect("must not also be a tolerance match" in str(error),
             "mixed floor/tolerance verdict should report the inconsistency")
    else:
      raise RuntimeError("self-check should reject a reference-floor target "
                         "that also claims a tolerance match")


def parse_args(argv: list[str]) -> argparse.Namespace:
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument("--cpp-result", type=Path, help="C++ solve-series result JSON")
  parser.add_argument("--comparison", type=Path, help="cpp-vs-AMFlow comparison JSON")
  parser.add_argument("--out", type=Path, help="write diagnostic JSON to this path")
  parser.add_argument("--self-check", action="store_true",
                      help="run built-in synthetic self-check")
  return parser.parse_args(argv)


def main(argv: list[str]) -> int:
  args = parse_args(argv)
  if args.self_check:
    run_self_check()
    return 0
  expect(args.cpp_result is not None, "--cpp-result is required unless --self-check is set")
  expect(args.comparison is not None, "--comparison is required unless --self-check is set")
  result = diagnose(load_json(args.cpp_result), load_json(args.comparison))
  write_json(result, args.out)
  return 0


if __name__ == "__main__":
  try:
    raise SystemExit(main(sys.argv[1:]))
  except Exception as error:  # pragma: no cover - command-line guard.
    print(f"diagnose_b61n_row56_specific_targets.py: {error}", file=sys.stderr)
    raise SystemExit(1)
