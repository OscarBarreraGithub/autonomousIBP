#!/usr/bin/env python3
"""Compare C++ solve-series JSON against retained AMFlow rule-list goldens."""

from __future__ import annotations

import argparse
import json
import re
import sys
import tempfile
from decimal import Decimal, InvalidOperation, getcontext
from pathlib import Path
from typing import Any


IntegralKey = tuple[str, tuple[int, ...]]
CoefficientMap = dict[int, tuple[Decimal, Decimal]]

INTEGRAL_RE = re.compile(r"^j\[\s*([A-Za-z_][A-Za-z0-9_]*)\s*,(.*)\]$")


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


def integral_label(family: str, indices: tuple[int, ...]) -> str:
  return f"{family}[{','.join(str(index) for index in indices)}]"


def parse_integral_label(label: str) -> IntegralKey:
  match = re.fullmatch(r"\s*([A-Za-z_][A-Za-z0-9_]*)\s*\[(.*)\]\s*", label)
  if match is None:
    raise ValueError(f"malformed integral label: {label!r}")
  family = match.group(1)
  raw_indices = [item.strip() for item in match.group(2).split(",") if item.strip()]
  expect(raw_indices, f"integral label {label!r} must carry at least one index")
  return family, tuple(int(item) for item in raw_indices)


def decimal_from_any(raw: Any, label: str) -> Decimal:
  if isinstance(raw, int):
    return Decimal(raw)
  if isinstance(raw, float):
    raw = repr(raw)
  if not isinstance(raw, str):
    raise TypeError(f"{label} must be a number string, got {type(raw).__name__}")
  value = raw.strip()
  if not value:
    raise ValueError(f"{label} must not be empty")
  value = value.replace("`", "")
  if value.endswith("."):
    value += "0"
  if value.startswith("."):
    value = "0" + value
  if value.startswith("-."):
    value = "-0." + value[2:]
  try:
    return Decimal(value)
  except InvalidOperation as error:
    raise ValueError(f"{label} is not a decimal number: {raw!r}") from error


def normalize_mathematica_numeric_text(text: str) -> str:
  text = text.replace("\\\n", "")
  text = text.replace("\n", " ")
  return re.sub(r"`[0-9.]+", "", text)


def split_top_level(value: str, separator: str) -> list[str]:
  parts: list[str] = []
  depth = 0
  start = 0
  for index, character in enumerate(value):
    if character in "([{":
      depth += 1
    elif character in ")]}":
      depth -= 1
    elif character == separator and depth == 0:
      parts.append(value[start:index].strip())
      start = index + 1
  parts.append(value[start:].strip())
  return [part for part in parts if part]


def split_top_level_terms(expression: str) -> list[str]:
  terms: list[str] = []
  depth = 0
  start = 0
  expression = expression.strip()
  for index, character in enumerate(expression):
    if character in "([{":
      depth += 1
      continue
    if character in ")]}":
      depth -= 1
      continue
    if character not in "+-" or depth != 0 or index == 0:
      continue
    terms.append(expression[start:index].strip())
    start = index
  terms.append(expression[start:].strip())
  return [term for term in terms if term]


def strip_balanced_parens(value: str) -> str:
  value = value.strip()
  while value.startswith("(") and value.endswith(")"):
    depth = 0
    balanced = True
    for index, character in enumerate(value):
      if character == "(":
        depth += 1
      elif character == ")":
        depth -= 1
        if depth == 0 and index != len(value) - 1:
          balanced = False
          break
    if not balanced:
      break
    value = value[1:-1].strip()
  return value


def parse_decimal_atom(atom: str) -> Decimal:
  atom = atom.strip()
  if atom.startswith("+"):
    atom = atom[1:]
  if atom.endswith("*"):
    atom = atom[:-1]
  return decimal_from_any(atom, f"AMFlow numeric atom {atom!r}")


def parse_complex_factor(raw_factor: str) -> tuple[Decimal, Decimal]:
  factor = strip_balanced_parens(raw_factor.replace(" ", ""))
  if factor in {"", "+"}:
    return Decimal(1), Decimal(0)
  if factor == "-":
    return Decimal(-1), Decimal(0)
  if factor.startswith("-(") and factor.endswith(")"):
    real, imag = parse_complex_factor(factor[1:])
    return -real, -imag
  if factor.startswith("+(") and factor.endswith(")"):
    return parse_complex_factor(factor[1:])

  real = Decimal(0)
  imag = Decimal(0)
  for term in split_top_level_terms(factor):
    term = strip_balanced_parens(term)
    if term in {"I", "+I"}:
      imag += Decimal(1)
      continue
    if term == "-I":
      imag -= Decimal(1)
      continue
    if term.endswith("*I"):
      imag += parse_decimal_atom(term[:-2])
    else:
      real += parse_decimal_atom(term)
  return real, imag


def term_epsilon_order_and_factor(term: str) -> tuple[int, str]:
  term = strip_balanced_parens(term.strip())
  compact = term.replace(" ", "")
  for pattern, order in (
      ("/eps^", None),
      ("*eps^", None),
  ):
    position = compact.rfind(pattern)
    if position == -1:
      continue
    exponent = int(compact[position + len(pattern):])
    factor = compact[:position]
    if pattern.startswith("/"):
      return -exponent, factor
    return exponent, factor[:-1] if factor.endswith("*") else factor

  if compact.endswith("/eps"):
    return -1, compact[:-4]
  if compact.endswith("*eps"):
    return 1, compact[:-4]
  if compact == "eps":
    return 1, "1"
  if compact == "-eps":
    return 1, "-1"
  return 0, compact


def add_complex(lhs: tuple[Decimal, Decimal],
                rhs: tuple[Decimal, Decimal]) -> tuple[Decimal, Decimal]:
  return lhs[0] + rhs[0], lhs[1] + rhs[1]


def parse_amflow_rhs(rhs: str) -> CoefficientMap:
  coefficients: CoefficientMap = {}
  for term in split_top_level_terms(rhs):
    order, factor = term_epsilon_order_and_factor(term)
    value = parse_complex_factor(factor)
    coefficients[order] = add_complex(coefficients.get(order, (Decimal(0), Decimal(0))), value)
  return coefficients


def parse_amflow_rule_list_text(text: str) -> dict[IntegralKey, CoefficientMap]:
  normalized = normalize_mathematica_numeric_text(text).strip()
  if normalized.startswith("{") and normalized.endswith("}"):
    normalized = normalized[1:-1].strip()
  rules: dict[IntegralKey, CoefficientMap] = {}
  for raw_rule in split_top_level(normalized, ","):
    if "->" not in raw_rule:
      continue
    lhs, rhs = raw_rule.split("->", 1)
    lhs = lhs.strip()
    match = INTEGRAL_RE.fullmatch(lhs)
    if match is None:
      raise ValueError(f"malformed AMFlow integral lhs: {lhs!r}")
    family = match.group(1)
    indices = tuple(int(item.strip()) for item in match.group(2).split(",") if item.strip())
    key = (family, indices)
    expect(key not in rules, f"duplicate AMFlow integral: {integral_label(*key)}")
    rules[key] = parse_amflow_rhs(rhs)
  expect(rules, "AMFlow golden did not contain any j[...] -> rules")
  return rules


def load_amflow_golden(path: Path) -> dict[IntegralKey, CoefficientMap]:
  if path.suffix == ".json":
    payload = load_json(path)
    if "golden_manifest" in payload:
      return load_amflow_golden(Path(str(payload["golden_manifest"])))
    outputs = payload.get("outputs")
    if isinstance(outputs, list):
      combined: dict[IntegralKey, CoefficientMap] = {}
      for raw_output in outputs:
        expect(isinstance(raw_output, dict), f"{path} outputs entries must be objects")
        output_path = Path(str(raw_output.get("canonical_text") or raw_output.get("path") or ""))
        expect(output_path.exists(), f"AMFlow output path does not exist: {output_path}")
        for key, coefficients in parse_amflow_rule_list_text(output_path.read_text(encoding="utf-8")).items():
          expect(key not in combined, f"duplicate AMFlow integral across outputs: {integral_label(*key)}")
          combined[key] = coefficients
      expect(combined, f"{path} did not reference any AMFlow outputs")
      return combined
  return parse_amflow_rule_list_text(path.read_text(encoding="utf-8"))


def coefficient_real(raw_order: dict[str, Any], label: str) -> Decimal:
  for key in ("real_digits", "real", "real_value"):
    if key in raw_order:
      return decimal_from_any(raw_order[key], f"{label}.{key}")
  raise ValueError(f"{label} must include real_digits, real, or real_value")


def coefficient_imag(raw_order: dict[str, Any], label: str) -> Decimal:
  for key in ("imag_digits", "imag", "imaginary", "imag_value"):
    if key in raw_order:
      return decimal_from_any(raw_order[key], f"{label}.{key}")
  return Decimal(0)


def load_cpp_result(path: Path) -> tuple[dict[str, Any], dict[IntegralKey, CoefficientMap]]:
  payload = load_json(path)
  expect(payload.get("schema_version") == 1, f"{path} schema_version must be 1")
  expect(payload.get("status") == "success", f"{path} status must be success")
  raw_results = payload.get("results")
  expect(isinstance(raw_results, list), f"{path} results must be a list")
  parsed: dict[IntegralKey, CoefficientMap] = {}
  for result_index, raw_result in enumerate(raw_results):
    expect(isinstance(raw_result, dict), f"{path} results entries must be objects")
    integral = str(raw_result.get("integral", "")).strip()
    expect(integral, f"{path} results[{result_index}].integral must not be empty")
    key = parse_integral_label(integral)
    expect(key not in parsed, f"duplicate C++ integral: {integral}")
    raw_orders = raw_result.get("epsilon_orders")
    expect(isinstance(raw_orders, list), f"{path} {integral} epsilon_orders must be a list")
    coefficients: CoefficientMap = {}
    for order_index, raw_order in enumerate(raw_orders):
      expect(isinstance(raw_order, dict), f"{path} {integral} epsilon_orders entries must be objects")
      order = raw_order.get("order")
      expect(isinstance(order, int), f"{path} {integral} epsilon_orders[{order_index}].order must be int")
      expect(order not in coefficients, f"duplicate C++ epsilon order {order} for {integral}")
      coefficients[order] = (
          coefficient_real(raw_order, f"{path} {integral} epsilon_orders[{order_index}]"),
          coefficient_imag(raw_order, f"{path} {integral} epsilon_orders[{order_index}]"),
      )
    parsed[key] = coefficients
  return payload, parsed


def digit_agreement(lhs: Decimal, rhs: Decimal) -> int:
  difference = abs(lhs - rhs)
  if difference == 0:
    return 999
  return max(0, -difference.adjusted())


def compare_cpp_vs_amflow(
    *,
    cpp_result_path: Path,
    amflow_golden_path: Path,
    tolerance_digits: int,
) -> dict[str, Any]:
  getcontext().prec = max(120, tolerance_digits + 40)
  cpp_payload, cpp = load_cpp_result(cpp_result_path)
  amflow = load_amflow_golden(amflow_golden_path)

  all_keys = sorted(set(cpp) | set(amflow), key=lambda item: (item[0], item[1]))
  integral_summaries: list[dict[str, Any]] = []
  failures: list[str] = []
  minimum_digit_agreement: int | None = None
  compared_coefficients = 0

  for key in all_keys:
    label = integral_label(*key)
    cpp_coefficients = cpp.get(key)
    amflow_coefficients = amflow.get(key)
    if cpp_coefficients is None:
      failures.append(f"missing C++ integral {label}")
      integral_summaries.append({"integral": label, "status": "missing-cpp-integral"})
      continue
    if amflow_coefficients is None:
      failures.append(f"missing AMFlow integral {label}")
      integral_summaries.append({"integral": label, "status": "missing-amflow-integral"})
      continue

    coefficient_summaries: list[dict[str, Any]] = []
    orders = sorted(set(cpp_coefficients) | set(amflow_coefficients))
    for order in orders:
      cpp_value = cpp_coefficients.get(order)
      amflow_value = amflow_coefficients.get(order)
      if cpp_value is None:
        failures.append(f"missing C++ coefficient {label} eps^{order}")
        coefficient_summaries.append({"order": order, "status": "missing-cpp-coefficient"})
        continue
      if amflow_value is None:
        failures.append(f"missing AMFlow coefficient {label} eps^{order}")
        coefficient_summaries.append({"order": order, "status": "missing-amflow-coefficient"})
        continue
      real_digits = digit_agreement(cpp_value[0], amflow_value[0])
      imag_digits = digit_agreement(cpp_value[1], amflow_value[1])
      coefficient_passed = real_digits >= tolerance_digits and imag_digits >= tolerance_digits
      if not coefficient_passed:
        failures.append(
            f"{label} eps^{order} has real/imag agreement {real_digits}/{imag_digits}, "
            f"below tolerance {tolerance_digits}"
        )
      minimum_digit_agreement = (
          min(real_digits, imag_digits)
          if minimum_digit_agreement is None
          else min(minimum_digit_agreement, real_digits, imag_digits)
      )
      compared_coefficients += 1
      coefficient_summaries.append(
          {
              "order": order,
              "real_agreement_digits": real_digits,
              "imag_agreement_digits": imag_digits,
              "passed": coefficient_passed,
              "cpp_real": str(cpp_value[0]),
              "cpp_imag": str(cpp_value[1]),
              "amflow_real": str(amflow_value[0]),
              "amflow_imag": str(amflow_value[1]),
          }
      )
    integral_summaries.append(
        {
            "integral": label,
            "status": "compared",
            "coefficients": coefficient_summaries,
        }
    )

  passed = not failures and compared_coefficients > 0
  return {
      "schema_version": 1,
      "comparison": "cpp-vs-amflow",
      "cpp_result": str(cpp_result_path),
      "amflow_golden": str(amflow_golden_path),
      "benchmark_id": cpp_payload.get("benchmark_id", ""),
      "tolerance_digits": tolerance_digits,
      "passed": passed,
      "matched_integral_count": len(set(cpp) & set(amflow)),
      "compared_coefficient_count": compared_coefficients,
      "minimum_digit_agreement": minimum_digit_agreement if minimum_digit_agreement is not None else 0,
      "integrals": integral_summaries,
      "failures": failures,
  }


def write_synthetic_inputs(root: Path, *, mismatch: bool = False, missing: bool = False) -> tuple[Path, Path]:
  root.mkdir(parents=True, exist_ok=True)
  amflow_path = root / "amflow-golden.txt"
  amflow_path.write_text(
      "{j[toy, 1] -> 3.0000000000000000000000000000000000000000`40. + "
      "1.2500000000000000000000000000000000000000`40./eps - "
      "(0.5000000000000000000000000000000000000000`40. - "
      "0.2500000000000000000000000000000000000000`40.*I)*eps}\n",
      encoding="utf-8",
  )
  cpp_path = root / "cpp-result.json"
  integral = "toy[2]" if missing else "toy[1]"
  order0 = "3.1000000000000000000000000000000000000000" if mismatch else "3.0000000000000000000000000000000000000000"
  cpp_path.write_text(
      json.dumps(
          {
              "schema_version": 1,
              "benchmark_id": "synthetic",
              "family": "toy",
              "targets": [integral],
              "solver": {"precision_digits": 40, "epsilon_order": 1},
              "results": [
                  {
                      "integral": integral,
                      "epsilon_orders": [
                          {"order": -1, "real_digits": "1.2500000000000000000000000000000000000000", "imag_digits": "0"},
                          {"order": 0, "real_digits": order0, "imag_digits": "0"},
                          {"order": 1, "real_digits": "-0.5000000000000000000000000000000000000000", "imag_digits": "0.2500000000000000000000000000000000000000"},
                      ],
                  }
              ],
              "status": "success",
              "duration_seconds": 0.0,
          },
          indent=2,
          sort_keys=True,
      )
      + "\n",
      encoding="utf-8",
  )
  return cpp_path, amflow_path


def mark_cpp_result_failed(cpp_path: Path) -> None:
  payload = load_json(cpp_path)
  payload["status"] = "failed"
  payload["failure_code"] = "synthetic_failure"
  cpp_path.write_text(json.dumps(payload, indent=2, sort_keys=True) + "\n", encoding="utf-8")


def run_self_check() -> dict[str, Any]:
  with tempfile.TemporaryDirectory(prefix="amflow-cpp-vs-amflow-self-check-") as raw_root:
    root = Path(raw_root)
    matching_cpp, matching_amflow = write_synthetic_inputs(root / "matching")
    matching = compare_cpp_vs_amflow(
        cpp_result_path=matching_cpp,
        amflow_golden_path=matching_amflow,
        tolerance_digits=30,
    )

    mismatch_cpp, mismatch_amflow = write_synthetic_inputs(root / "mismatch", mismatch=True)
    mismatch = compare_cpp_vs_amflow(
        cpp_result_path=mismatch_cpp,
        amflow_golden_path=mismatch_amflow,
        tolerance_digits=30,
    )

    missing_cpp, missing_amflow = write_synthetic_inputs(root / "missing", missing=True)
    missing = compare_cpp_vs_amflow(
        cpp_result_path=missing_cpp,
        amflow_golden_path=missing_amflow,
        tolerance_digits=30,
    )

    failed_cpp, failed_amflow = write_synthetic_inputs(root / "failed-cpp")
    mark_cpp_result_failed(failed_cpp)
    try:
      failed_status = compare_cpp_vs_amflow(
          cpp_result_path=failed_cpp,
          amflow_golden_path=failed_amflow,
          tolerance_digits=30,
      )
      failed_status_rejected = not failed_status["passed"] and bool(failed_status["failures"])
    except Exception:
      failed_status_rejected = True

  return {
      "schema_version": 1,
      "self_check": "compare_cpp_vs_amflow",
      "matching_synthetic_passed": matching["passed"],
      "mismatch_synthetic_rejected": not mismatch["passed"] and bool(mismatch["failures"]),
      "missing_integral_rejected": not missing["passed"] and bool(missing["failures"]),
      "failed_cpp_status_rejected": failed_status_rejected,
      "summary_written": False,
  }


def parse_args(argv: list[str]) -> argparse.Namespace:
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument("--cpp-result", type=Path)
  parser.add_argument("--amflow-golden", type=Path)
  parser.add_argument("--tolerance-digits", type=int, default=30)
  parser.add_argument("--self-check", action="store_true")
  return parser.parse_args(argv)


def main(argv: list[str]) -> int:
  args = parse_args(argv)
  try:
    if args.self_check:
      payload = run_self_check()
      write_json(payload)
      return 0 if all(
          payload[key]
          for key in (
              "matching_synthetic_passed",
              "mismatch_synthetic_rejected",
              "missing_integral_rejected",
              "failed_cpp_status_rejected",
          )
      ) else 1

    expect(args.cpp_result is not None, "--cpp-result is required outside --self-check")
    expect(args.amflow_golden is not None, "--amflow-golden is required outside --self-check")
    expect(args.tolerance_digits > 0, "--tolerance-digits must be positive")
    payload = compare_cpp_vs_amflow(
        cpp_result_path=args.cpp_result,
        amflow_golden_path=args.amflow_golden,
        tolerance_digits=args.tolerance_digits,
    )
    write_json(payload)
    return 0 if payload["passed"] else 1
  except Exception as error:
    write_json(
        {
            "schema_version": 1,
            "comparison": "cpp-vs-amflow",
            "passed": False,
            "failures": [str(error)],
        }
    )
    return 1


if __name__ == "__main__":
  sys.exit(main(sys.argv[1:]))
