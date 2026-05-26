#!/usr/bin/env python3
"""Classify the b64ag first-block 50-digit comparison gap."""

from __future__ import annotations

import argparse
import json
import sys
import tempfile
from pathlib import Path
from typing import Any


FAILING_DIRECT_TARGETS = [
    "gauge[1,1,1,-1,1,0,0,0,0]",
    "gauge[1,1,1,0,1,0,0,0,0]",
]

CONTRASTING_PASSING_TARGETS = [
    "gauge[0,1,1,1,1,-1,0,0,0]",
    "gauge[0,1,1,1,1,0,0,0,0]",
    "gauge[1,1,1,1,1,-1,0,0,0]",
    "gauge[1,1,1,1,1,0,0,0,0]",
]

TARGET_CLASSIFICATION = {
    "gauge[1,1,1,-1,1,0,0,0,0]": {
        "path": "live_first_block_fit",
        "normalizemat_target_switch": False,
        "source": (
            "first-block endpoint transport plus Frobenius branch, raw retained "
            "target reduction, then post-endpoint Laurent fit"
        ),
        "pass_or_fail_reason": (
            "the row is still limited by the retained AMFlow golden precision for "
            "this direct target"
        ),
    },
    "gauge[1,1,1,0,1,0,0,0,0]": {
        "path": "live_first_block_fit",
        "normalizemat_target_switch": False,
        "source": (
            "first-block endpoint transport plus Frobenius branch, raw retained "
            "target reduction, then post-endpoint Laurent fit"
        ),
        "pass_or_fail_reason": (
            "the row is still limited by the retained AMFlow golden precision for "
            "this direct target"
        ),
    },
    "gauge[0,1,1,1,1,-1,0,0,0]": {
        "path": "zero_prefix_row",
        "normalizemat_target_switch": False,
        "source": "retained reduction publishes an all-zero prefix target surface",
        "pass_or_fail_reason": (
            "all explicit or implicit coefficients compare as zero, so no "
            "post-endpoint fit precision is exercised"
        ),
    },
    "gauge[0,1,1,1,1,0,0,0,0]": {
        "path": "zero_prefix_row",
        "normalizemat_target_switch": False,
        "source": "retained reduction publishes an all-zero prefix target surface",
        "pass_or_fail_reason": (
            "all explicit or implicit coefficients compare as zero, so no "
            "post-endpoint fit precision is exercised"
        ),
    },
    "gauge[1,1,1,1,1,-1,0,0,0]": {
        "path": "reviewed_downstream_target_table",
        "normalizemat_target_switch": True,
        "source": "reviewed downstream target Laurent row publication",
        "pass_or_fail_reason": (
            "the published row is the reviewed table row and matches the retained "
            "AMFlow text exactly in this comparison artifact"
        ),
    },
    "gauge[1,1,1,1,1,0,0,0,0]": {
        "path": "reviewed_downstream_target_table",
        "normalizemat_target_switch": True,
        "source": "reviewed downstream target Laurent row publication",
        "pass_or_fail_reason": (
            "the published row is the reviewed table row and matches the retained "
            "AMFlow text exactly in this comparison artifact"
        ),
    },
}


def expect(condition: bool, message: str) -> None:
  if not condition:
    raise RuntimeError(message)


def load_json(path: Path) -> dict[str, Any]:
  with path.open("r", encoding="utf-8") as stream:
    payload = json.load(stream)
  expect(isinstance(payload, dict), f"{path} must contain a JSON object")
  return payload


def write_json(payload: dict[str, Any], output_path: Path | None) -> None:
  text = json.dumps(payload, indent=2, sort_keys=True) + "\n"
  if output_path is None:
    sys.stdout.write(text)
    return
  output_path.write_text(text, encoding="utf-8")


def as_int(value: Any, label: str) -> int:
  if isinstance(value, int):
    return value
  if isinstance(value, str):
    return int(value)
  raise TypeError(f"{label} must be an integer or integer string")


def coefficient_component_floor(coefficient: dict[str, Any]) -> int:
  return min(
      as_int(coefficient.get("real_agreement_digits", 0), "real_agreement_digits"),
      as_int(coefficient.get("imag_agreement_digits", 0), "imag_agreement_digits"),
  )


def integral_rows(compare: dict[str, Any]) -> dict[str, dict[str, Any]]:
  raw_rows = compare.get("integrals")
  expect(isinstance(raw_rows, list), "comparison JSON must contain an integral list")
  rows: dict[str, dict[str, Any]] = {}
  for row in raw_rows:
    expect(isinstance(row, dict), "comparison integral rows must be objects")
    label = row.get("integral")
    expect(isinstance(label, str) and label, "comparison row missing integral label")
    expect(label not in rows, f"duplicate comparison row for {label}")
    rows[label] = row
  return rows


def summarize_row(row: dict[str, Any], label: str) -> dict[str, Any]:
  raw_coefficients = row.get("coefficients")
  expect(isinstance(raw_coefficients, list), f"{label} missing coefficient list")
  coefficients: list[dict[str, Any]] = []
  for raw_coefficient in raw_coefficients:
    expect(isinstance(raw_coefficient, dict), f"{label} coefficient must be an object")
    coefficients.append(raw_coefficient)
  expect(coefficients, f"{label} has no coefficient rows")

  failure_orders = [
      as_int(coefficient.get("order"), f"{label} order")
      for coefficient in coefficients
      if not bool(coefficient.get("passed"))
  ]
  per_order = []
  for coefficient in coefficients:
    order = as_int(coefficient.get("order"), f"{label} order")
    per_order.append({
        "order": order,
        "passed": bool(coefficient.get("passed")),
        "real_agreement_digits": as_int(
            coefficient.get("real_agreement_digits", 0),
            f"{label} eps^{order} real_agreement_digits",
        ),
        "imag_agreement_digits": as_int(
            coefficient.get("imag_agreement_digits", 0),
            f"{label} eps^{order} imag_agreement_digits",
        ),
        "component_digit_floor": coefficient_component_floor(coefficient),
        "relative_error_abs": str(coefficient.get("relative_error_abs", "")),
    })

  minimum_component_digits = min(item["component_digit_floor"] for item in per_order)
  return {
      "target": label,
      "coefficient_count": len(coefficients),
      "passed_coefficient_count": len(coefficients) - len(failure_orders),
      "failed_orders": failure_orders,
      "minimum_component_digits": minimum_component_digits,
      "per_order": per_order,
      **TARGET_CLASSIFICATION[label],
  }


def diagnose(compare: dict[str, Any]) -> dict[str, Any]:
  rows = integral_rows(compare)
  required_targets = FAILING_DIRECT_TARGETS + CONTRASTING_PASSING_TARGETS
  for label in required_targets:
    expect(label in rows, f"missing required b64ag comparison target {label}")

  failing = [summarize_row(rows[label], label) for label in FAILING_DIRECT_TARGETS]
  passing_contrasts = [
      summarize_row(rows[label], label) for label in CONTRASTING_PASSING_TARGETS
  ]
  observed_failures = [
      (target["target"], order)
      for target in failing + passing_contrasts
      for order in target["failed_orders"]
  ]
  failing_direct_failures = [
      item for item in observed_failures if item[0] in FAILING_DIRECT_TARGETS
  ]
  contrast_failures = [
      item for item in observed_failures if item[0] in CONTRASTING_PASSING_TARGETS
  ]

  return {
      "schema_version": 1,
      "step_chosen": "c",
      "status": "tier_c_gap_diagnostic",
      "comparison": {
          "benchmark_id": compare.get("benchmark_id"),
          "tolerance_digits": compare.get("tolerance_digits"),
          "passed": bool(compare.get("passed")),
          "matched_integral_count": compare.get("matched_integral_count"),
          "compared_coefficient_count": compare.get("compared_coefficient_count"),
          "passed_coefficient_count": compare.get("passed_coefficient_count"),
          "minimum_digit_agreement": compare.get("minimum_digit_agreement"),
      },
      "failing_direct_targets": failing,
      "contrasting_passing_targets": passing_contrasts,
      "failing_direct_failure_count": len(failing_direct_failures),
      "contrast_failure_count": len(contrast_failures),
      "all_contrasts_passed": len(contrast_failures) == 0,
      "conclusion": (
          "The two remaining 50-digit failures are the only contrasted rows still "
          "coming from the live first-block endpoint/Frobenius transport and "
          "post-endpoint Laurent fit path. The four contrasted passing rows either "
          "compare as zero/implicit zero or use reviewed downstream target Laurent "
          "row publication, so they do not exercise the same retained-golden "
          "precision ceiling."
      ),
      "m6_flipped": False,
  }


def sample_coefficient(order: int,
                       passed: bool,
                       real_digits: int,
                       imag_digits: int) -> dict[str, Any]:
  return {
      "order": order,
      "passed": passed,
      "real_agreement_digits": real_digits,
      "imag_agreement_digits": imag_digits,
      "relative_error_abs": "0" if passed else "1e-37",
  }


def make_self_check_comparison() -> dict[str, Any]:
  rows = []
  for label in FAILING_DIRECT_TARGETS:
    rows.append({
        "integral": label,
        "status": "compared",
        "coefficients": [
            sample_coefficient(-1, False, 39, 50),
            sample_coefficient(0, False, 39, 39),
            sample_coefficient(1, False, 39, 39),
            sample_coefficient(2, False, 40, 37),
        ],
    })
  for label in CONTRASTING_PASSING_TARGETS[:2]:
    rows.append({
        "integral": label,
        "status": "compared",
        "coefficients": [
            sample_coefficient(0, True, 999, 999),
            sample_coefficient(1, True, 999, 999),
            sample_coefficient(2, True, 999, 999),
        ],
    })
  for label in CONTRASTING_PASSING_TARGETS[2:]:
    rows.append({
        "integral": label,
        "status": "compared",
        "coefficients": [
            sample_coefficient(-2, True, 999, 999),
            sample_coefficient(-1, True, 999, 999),
            sample_coefficient(0, True, 999, 999),
            sample_coefficient(1, True, 999, 999),
            sample_coefficient(2, True, 999, 999),
        ],
    })
  return {
      "benchmark_id": "linear_propagator",
      "tolerance_digits": 50,
      "passed": False,
      "matched_integral_count": 9,
      "compared_coefficient_count": 39,
      "passed_coefficient_count": 31,
      "minimum_digit_agreement": 37,
      "integrals": rows,
  }


def self_check() -> None:
  with tempfile.TemporaryDirectory() as tmp:
    compare_path = Path(tmp) / "compare.json"
    output_path = Path(tmp) / "diagnostic.json"
    compare_path.write_text(
        json.dumps(make_self_check_comparison()),
        encoding="utf-8",
    )
    diagnostic = diagnose(load_json(compare_path))
    write_json(diagnostic, output_path)
    round_trip = load_json(output_path)
  expect(round_trip["step_chosen"] == "c", "self-check should record step c")
  expect(round_trip["failing_direct_failure_count"] == 8,
         "self-check should find the eight direct-row failures")
  expect(round_trip["contrast_failure_count"] == 0,
         "self-check contrasts should all pass")
  expect(round_trip["all_contrasts_passed"] is True,
         "self-check contrasts should be marked passing")
  expect(round_trip["m6_flipped"] is False,
         "self-check must stay fail-closed")


def parse_args() -> argparse.Namespace:
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument("--compare50", type=Path, help="b64ag 50-digit comparator JSON")
  parser.add_argument("--output", type=Path, help="optional output JSON path")
  parser.add_argument("--self-check", action="store_true")
  return parser.parse_args()


def main() -> int:
  args = parse_args()
  try:
    if args.self_check:
      self_check()
      return 0
    if args.compare50 is None:
      raise RuntimeError("--compare50 is required unless --self-check is used")
    diagnostic = diagnose(load_json(args.compare50))
    write_json(diagnostic, args.output)
    return 0
  except Exception as error:  # noqa: BLE001 - command-line diagnostic.
    print(f"error: {error}", file=sys.stderr)
    return 1


if __name__ == "__main__":
  raise SystemExit(main())
