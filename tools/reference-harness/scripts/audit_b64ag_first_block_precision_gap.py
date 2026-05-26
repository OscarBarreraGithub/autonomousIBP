#!/usr/bin/env python3
"""Audit the b64ag first-block precision gap in C++ vs AMFlow comparisons."""

from __future__ import annotations

import argparse
import json
import tempfile
from decimal import Decimal, InvalidOperation
from pathlib import Path
from typing import Any


FIRST_BLOCK_LABELS = {
    "gauge[1,1,1,0,1,0,0,0,0]",
    "gauge[1,1,1,-1,1,0,0,0,0]",
}
EXPECTED_FIRST_BLOCK_FAILURE_ORDERS = {
    label: [-1, 0, 1, 2]
    for label in sorted(FIRST_BLOCK_LABELS)
}


def expect(condition: bool, message: str) -> None:
  if not condition:
    raise RuntimeError(message)


def load_json(path: Path) -> dict[str, Any]:
  with path.open("r", encoding="utf-8") as stream:
    payload = json.load(stream)
  expect(isinstance(payload, dict), f"{path} must contain a JSON object")
  return payload


def write_json(payload: dict[str, Any]) -> None:
  print(json.dumps(payload, indent=2, sort_keys=True))


def decimal_is_zero(raw: Any) -> bool:
  if raw is None:
    return True
  value = str(raw).strip()
  if not value:
    return True
  try:
    return Decimal(value) == 0
  except InvalidOperation:
    return False


def mantissa_digit_count(raw: Any) -> int:
  value = str(raw).strip()
  if not value or decimal_is_zero(value):
    return 0
  if value[0] in "+-":
    value = value[1:]
  mantissa = value.split("E", 1)[0].split("e", 1)[0]
  return sum(1 for character in mantissa if character.isdigit())


def coefficient_component_digit_counts(coefficient: dict[str, Any],
                                       component: str) -> tuple[int, int] | None:
  amflow_value = coefficient.get(f"amflow_{component}")
  if decimal_is_zero(amflow_value):
    return None
  return (
      mantissa_digit_count(amflow_value),
      mantissa_digit_count(coefficient.get(f"cpp_{component}", "")),
  )


def require_list(value: Any, label: str) -> list[Any]:
  expect(isinstance(value, list), f"{label} must be a list")
  return value


def require_dict(value: Any, label: str) -> dict[str, Any]:
  expect(isinstance(value, dict), f"{label} must be an object")
  return value


def require_int(value: Any, label: str) -> int:
  expect(value is not None, f"{label} is required")
  if isinstance(value, bool):
    raise RuntimeError(f"{label} must be an integer, not a boolean")
  try:
    return int(value)
  except (TypeError, ValueError) as error:
    raise RuntimeError(f"{label} must be an integer") from error


def agreement_digits(coefficient: dict[str, Any], component: str) -> int | None:
  raw = coefficient.get(f"{component}_agreement_digits")
  if raw is None:
    return None
  return require_int(raw, f"{component}_agreement_digits")


def analyze_comparison(payload: dict[str, Any]) -> dict[str, Any]:
  tolerance_digits = require_int(payload.get("tolerance_digits", 0), "tolerance_digits")
  failures_by_integral: dict[str, list[int]] = {}
  failed_coefficients: list[dict[str, Any]] = []
  first_block_nonzero_amflow_digit_counts: list[int] = []
  first_block_nonzero_cpp_digit_counts: list[int] = []
  first_block_nonzero_agreement_digits: list[int] = []
  first_block_nonzero_missing_agreement_digit_count = 0
  first_block_zero_reference_cpp_residual_count = 0

  for integral_index, raw_integral in enumerate(require_list(payload.get("integrals", []),
                                                            "integrals")):
    integral = require_dict(raw_integral, f"integrals[{integral_index}]")
    label = integral.get("integral")
    if not isinstance(label, str):
      raise RuntimeError(f"integrals[{integral_index}].integral must be a string")
    coefficients = require_list(integral.get("coefficients", []),
                                f"integrals[{integral_index}].coefficients")
    for coefficient_index, raw_coefficient in enumerate(coefficients):
      coefficient = require_dict(
          raw_coefficient,
          f"integrals[{integral_index}].coefficients[{coefficient_index}]",
      )
      if coefficient.get("passed", True):
        continue
      order = require_int(
          coefficient.get("order"),
          f"integrals[{integral_index}].coefficients[{coefficient_index}].order",
      )
      failures_by_integral.setdefault(label, []).append(order)
      failed_coefficients.append({"integral": label, "order": order})
      if label in FIRST_BLOCK_LABELS:
        for component in ("real", "imag"):
          counts = coefficient_component_digit_counts(coefficient, component)
          if counts is None:
            if not decimal_is_zero(coefficient.get(f"cpp_{component}")):
              first_block_zero_reference_cpp_residual_count += 1
            continue
          amflow_count, cpp_count = counts
          first_block_nonzero_amflow_digit_counts.append(amflow_count)
          first_block_nonzero_cpp_digit_counts.append(cpp_count)
          component_agreement = agreement_digits(coefficient, component)
          if component_agreement is None:
            first_block_nonzero_missing_agreement_digit_count += 1
          else:
            first_block_nonzero_agreement_digits.append(component_agreement)

  failed_labels = set(failures_by_integral)
  first_block_failure_count = sum(
      len(orders)
      for label, orders in failures_by_integral.items()
      if label in FIRST_BLOCK_LABELS
  )
  unexpected_failure_labels = sorted(failed_labels - FIRST_BLOCK_LABELS)
  all_failures_are_first_block = bool(failed_labels) and not unexpected_failure_labels
  sorted_failures_by_integral = {
      label: sorted(orders) for label, orders in sorted(failures_by_integral.items())
  }
  first_block_failure_orders_match_expected = (
      sorted_failures_by_integral == EXPECTED_FIRST_BLOCK_FAILURE_ORDERS
  )
  nonzero_amflow_digit_min = (
      min(first_block_nonzero_amflow_digit_counts)
      if first_block_nonzero_amflow_digit_counts else 0
  )
  nonzero_amflow_digit_max = (
      max(first_block_nonzero_amflow_digit_counts)
      if first_block_nonzero_amflow_digit_counts else 0
  )
  nonzero_cpp_digit_min = (
      min(first_block_nonzero_cpp_digit_counts)
      if first_block_nonzero_cpp_digit_counts else 0
  )
  nonzero_agreement_digit_min = (
      min(first_block_nonzero_agreement_digits)
      if first_block_nonzero_agreement_digits else 0
  )
  nonzero_agreement_digit_max = (
      max(first_block_nonzero_agreement_digits)
      if first_block_nonzero_agreement_digits else 0
  )
  agreement_digits_match_retained_envelope = (
      bool(first_block_nonzero_agreement_digits) and
      first_block_nonzero_missing_agreement_digit_count == 0 and
      nonzero_agreement_digit_min >= max(0, nonzero_amflow_digit_min - 1) and
      nonzero_agreement_digit_max <= nonzero_amflow_digit_max + 1
  )
  retained_golden_envelope_limited = (
      all_failures_are_first_block and
      first_block_failure_orders_match_expected and
      tolerance_digits > 0 and
      nonzero_amflow_digit_max < tolerance_digits and
      nonzero_cpp_digit_min >= tolerance_digits and
      agreement_digits_match_retained_envelope
  )
  compare50_passed = tolerance_digits >= 50 and bool(payload.get("passed", False))

  return {
      "schema_version": 1,
      "audit": "b64ag-first-block-precision-gap",
      "comparison_passed": bool(payload.get("passed", False)),
      "tolerance_digits": tolerance_digits,
      "minimum_digit_agreement": payload.get("minimum_digit_agreement"),
      "compared_coefficient_count": payload.get("compared_coefficient_count"),
      "passed_coefficient_count": payload.get("passed_coefficient_count"),
      "failed_coefficient_count": len(failed_coefficients),
      "first_block_failure_count": first_block_failure_count,
      "failed_orders_by_integral": sorted_failures_by_integral,
      "all_failures_are_first_block": all_failures_are_first_block,
      "first_block_failure_orders_match_expected": first_block_failure_orders_match_expected,
      "unexpected_failure_labels": unexpected_failure_labels,
      "first_block_nonzero_amflow_digit_count_min": nonzero_amflow_digit_min,
      "first_block_nonzero_amflow_digit_count_max": nonzero_amflow_digit_max,
      "first_block_nonzero_cpp_digit_count_min": nonzero_cpp_digit_min,
      "first_block_nonzero_agreement_digit_min": nonzero_agreement_digit_min,
      "first_block_nonzero_agreement_digit_max": nonzero_agreement_digit_max,
      "first_block_nonzero_missing_agreement_digit_count":
          first_block_nonzero_missing_agreement_digit_count,
      "first_block_zero_reference_cpp_residual_count":
          first_block_zero_reference_cpp_residual_count,
      "agreement_digits_match_retained_envelope": agreement_digits_match_retained_envelope,
      "precision_gap_matches_retained_golden_envelope": retained_golden_envelope_limited,
      "seed_path_audit": (
          "The failing labels are the two first-block coefficients fitted from the finite "
          "gaugex=1/40 boundary seed; the remaining compared b64ag rows are not part of this "
          "first-block retained-golden precision envelope."
      ),
      "series_order_audit": (
          "The live b64ag first-block Frobenius evaluators already use 180 local terms, so this "
          "audit does not support a 20-term truncation explanation."
      ),
      "m6_promotion_blocked": not compare50_passed,
    }


def self_check() -> dict[str, Any]:
  fixture = {
      "passed": False,
      "tolerance_digits": 50,
      "minimum_digit_agreement": 37,
      "compared_coefficient_count": 9,
      "passed_coefficient_count": 1,
      "integrals": [
          {
              "integral": "gauge[1,1,1,0,1,0,0,0,0]",
              "coefficients": [
                  {
                      "order": order,
                      "passed": False,
                      "amflow_real": "-0.0332454250147094483838346214715355353",
                      "amflow_imag": "-0.0261799387799149436538553615273291907",
                      "cpp_real": "-0.03324542501470944838383462147153553529687089442269",
                      "cpp_imag": "-0.02617993877991494365385536152732919070164307576890",
                      "real_agreement_digits": 39,
                      "imag_agreement_digits": 39 if order != 2 else 37,
                  }
                  for order in [-1, 0, 1, 2]
              ],
          },
          {
              "integral": "gauge[1,1,1,-1,1,0,0,0,0]",
              "coefficients": [
                  {
                      "order": order,
                      "passed": False,
                      "amflow_real": "-0.02770452084559120698652885122627961275",
                      "amflow_imag": "-0.02181661564992911971154613460610765892",
                      "cpp_real": "-0.02770452084559120698652885122627961274739241201890",
                      "cpp_imag": "-0.02181661564992911971154613460610765891803589647408",
                      "real_agreement_digits": 39,
                      "imag_agreement_digits": 40 if order == 1 else (37 if order == 2 else 39),
                  }
                  for order in [-1, 0, 1, 2]
              ],
          },
          {
              "integral": "gauge[1,1,1,1,1,0,0,0,0]",
              "coefficients": [
                  {
                      "order": 0,
                      "passed": True,
                      "amflow_real": "-3.33741932089154818571720514204504616074",
                      "amflow_imag": "-3.28617743327989907546421514111030244718",
                      "cpp_real": "-3.33741932089154818571720514204504616074000000000000",
                      "cpp_imag": "-3.28617743327989907546421514111030244718000000000000",
                      "real_agreement_digits": 999,
                      "imag_agreement_digits": 999,
                  }
              ],
          },
      ],
  }
  with tempfile.TemporaryDirectory(prefix="b64ag-first-block-audit-") as directory:
    fixture_path = Path(directory) / "compare50.json"
    fixture_path.write_text(json.dumps(fixture), encoding="utf-8")
    result = analyze_comparison(load_json(fixture_path))

  expect(result["all_failures_are_first_block"],
         "self-check should classify only first-block failures")
  expect(result["first_block_failure_count"] == 8,
         "self-check should count the eight first-block failures")
  expect(result["minimum_digit_agreement"] == 37,
         "self-check should retain the minimum digit floor")
  expect(result["precision_gap_matches_retained_golden_envelope"],
         "self-check should identify the retained-golden precision envelope")
  expect(result["agreement_digits_match_retained_envelope"],
         "self-check should bind agreement digits to the retained-golden precision envelope")
  expect(result["m6_promotion_blocked"],
         "self-check should keep M6 promotion blocked")
  compare30_fixture = dict(fixture)
  compare30_fixture["passed"] = True
  compare30_fixture["tolerance_digits"] = 30
  compare30_result = analyze_comparison(compare30_fixture)
  expect(compare30_result["m6_promotion_blocked"],
         "passing lower-tolerance compare must not unblock M6 promotion")

  duplicate_failure_fixture = dict(fixture)
  duplicate_failure_fixture["integrals"] = [
      {
          "integral": "gauge[1,1,1,0,1,0,0,0,0]",
          "coefficients": fixture["integrals"][0]["coefficients"] * 2,
      }
  ]
  duplicate_failure_result = analyze_comparison(duplicate_failure_fixture)
  expect(not duplicate_failure_result["precision_gap_matches_retained_golden_envelope"],
         "duplicate first-block failures must not satisfy the exact retained-envelope shape")

  low_agreement_fixture = json.loads(json.dumps(fixture))
  low_agreement_fixture["integrals"][0]["coefficients"][0]["real_agreement_digits"] = 12
  low_agreement_result = analyze_comparison(low_agreement_fixture)
  expect(not low_agreement_result["precision_gap_matches_retained_golden_envelope"],
         "short AMFlow strings alone must not satisfy the retained-envelope audit")

  missing_agreement_fixture = json.loads(json.dumps(fixture))
  del missing_agreement_fixture["integrals"][0]["coefficients"][0]["real_agreement_digits"]
  missing_agreement_result = analyze_comparison(missing_agreement_fixture)
  expect(not missing_agreement_result["precision_gap_matches_retained_golden_envelope"],
         "missing agreement digits must not satisfy the retained-envelope audit")
  result["self_check"] = True
  return result


def parse_args() -> argparse.Namespace:
  parser = argparse.ArgumentParser(
      description="Audit whether a b64ag compare50 failure is isolated to the first-block "
                  "retained-golden precision envelope."
  )
  parser.add_argument("--compare-json", type=Path, help="compare_cpp_vs_amflow.py JSON output")
  parser.add_argument("--root", type=Path, help="accepted for reference-harness self-check parity")
  parser.add_argument("--self-check", action="store_true")
  return parser.parse_args()


def main() -> int:
  args = parse_args()
  if args.self_check:
    write_json(self_check())
    return 0
  expect(args.compare_json is not None, "--compare-json is required unless --self-check is used")
  write_json(analyze_comparison(load_json(args.compare_json)))
  return 0


if __name__ == "__main__":
  raise SystemExit(main())
