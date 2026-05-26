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
FamilyAliasMap = dict[str, str]
ZERO_COMPLEX = (Decimal(0), Decimal(0))

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


def normalize_family_name(family: str, aliases: FamilyAliasMap) -> str:
  if family in aliases:
    return aliases[family]
  if family.endswith("_amflow") and len(family) > len("_amflow"):
    return family[:-len("_amflow")]
  return family


def normalize_key_map(raw: dict[IntegralKey, CoefficientMap],
                      aliases: FamilyAliasMap,
                      source_label: str) -> dict[IntegralKey, CoefficientMap]:
  normalized: dict[IntegralKey, CoefficientMap] = {}
  for key, coefficients in raw.items():
    normalized_key = (normalize_family_name(key[0], aliases), key[1])
    if normalized_key in normalized:
      raise RuntimeError(
          f"{source_label} integrals collide after family normalization: "
          f"{integral_label(*key)} -> {integral_label(*normalized_key)}"
      )
    normalized[normalized_key] = coefficients
  return normalized


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
  text = text.replace("*^", "E")
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
    if expression[index - 1] in "Ee":
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
  rhs = rhs.strip()
  if rhs.startswith("{") and rhs.endswith("}"):
    list_items = split_top_level(rhs[1:-1], ",")
    if len(list_items) == 1:
      rhs = list_items[0]
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


def selected_manifest_outputs(payload: dict[str, Any], path: Path) -> set[str] | None:
  raw = payload.get("compare_cpp_vs_amflow_outputs")
  if raw is None:
    return None
  expect(isinstance(raw, list), f"{path} compare_cpp_vs_amflow_outputs must be a list")
  selected: set[str] = set()
  for index, raw_name in enumerate(raw):
    expect(
        isinstance(raw_name, str) and raw_name.strip(),
        f"{path} compare_cpp_vs_amflow_outputs[{index}] must be a non-empty string",
    )
    name = raw_name.strip()
    expect(name not in selected, f"{path} duplicate compare_cpp_vs_amflow output: {name}")
    selected.add(name)
  expect(selected, f"{path} compare_cpp_vs_amflow_outputs must not be empty")
  return selected


def selected_outputs_for_manifest(path: Path,
                                  *,
                                  inherited: set[str] | None = None) -> set[str] | None:
  if path.suffix != ".json":
    return inherited
  payload = load_json(path)
  local_selected_outputs = selected_manifest_outputs(payload, path)
  effective_selected_outputs = (
      local_selected_outputs if local_selected_outputs is not None else inherited
  )
  if "golden_manifest" in payload:
    return selected_outputs_for_manifest(
        Path(str(payload["golden_manifest"])),
        inherited=effective_selected_outputs,
    )
  return effective_selected_outputs


def resolve_manifest_relative_path(raw_path: str, manifest_path: Path) -> Path:
  candidate = Path(raw_path)
  if candidate.is_absolute():
    return candidate
  manifest_relative = manifest_path.parent / candidate
  if manifest_relative.exists():
    return manifest_relative
  return candidate


def apply_manifest_coefficient_overlays(
    combined: dict[IntegralKey, CoefficientMap],
    payload: dict[str, Any],
    path: Path,
) -> None:
  raw_overlays = payload.get("compare_cpp_vs_amflow_coefficient_overlays")
  if raw_overlays is None:
    return
  expect(
      isinstance(raw_overlays, list),
      f"{path} compare_cpp_vs_amflow_coefficient_overlays must be a list",
  )
  observed_overlay_keys: set[IntegralKey] = set()
  for overlay_index, raw_overlay in enumerate(raw_overlays):
    expect(
        isinstance(raw_overlay, dict),
        f"{path} compare_cpp_vs_amflow_coefficient_overlays[{overlay_index}] must be an object",
    )
    overlay_path = resolve_manifest_relative_path(
        str(raw_overlay.get("canonical_text") or raw_overlay.get("path") or ""),
        path,
    )
    expect(
        overlay_path.exists(),
        f"AMFlow coefficient overlay path does not exist: {overlay_path}",
    )
    overlay_rules = parse_amflow_rule_list_text(
        overlay_path.read_text(encoding="utf-8")
    )
    for key, coefficients in overlay_rules.items():
      label = integral_label(*key)
      expect(key in combined, f"{path} overlay references unknown AMFlow integral: {label}")
      expect(
          key not in observed_overlay_keys,
          f"{path} duplicate coefficient overlay for AMFlow integral: {label}",
      )
      combined[key] = coefficients
      observed_overlay_keys.add(key)


def load_amflow_golden(
    path: Path,
    *,
    selected_outputs: set[str] | None = None,
) -> dict[IntegralKey, CoefficientMap]:
  if path.suffix == ".json":
    payload = load_json(path)
    local_selected_outputs = selected_manifest_outputs(payload, path)
    effective_selected_outputs = (
        local_selected_outputs if local_selected_outputs is not None else selected_outputs
    )
    if "golden_manifest" in payload:
      combined = load_amflow_golden(
          Path(str(payload["golden_manifest"])),
          selected_outputs=effective_selected_outputs,
      )
      apply_manifest_coefficient_overlays(combined, payload, path)
      return combined
    outputs = payload.get("outputs")
    if isinstance(outputs, list):
      combined: dict[IntegralKey, CoefficientMap] = {}
      observed_selected_outputs: set[str] = set()
      for raw_output in outputs:
        expect(isinstance(raw_output, dict), f"{path} outputs entries must be objects")
        output_name = str(raw_output.get("name", "")).strip()
        expect(output_name, f"{path} outputs entries must include non-empty name")
        if effective_selected_outputs is not None and output_name not in effective_selected_outputs:
          continue
        observed_selected_outputs.add(output_name)
        output_path = Path(str(raw_output.get("canonical_text") or raw_output.get("path") or ""))
        expect(output_path.exists(), f"AMFlow output path does not exist: {output_path}")
        for key, coefficients in parse_amflow_rule_list_text(output_path.read_text(encoding="utf-8")).items():
          expect(key not in combined, f"duplicate AMFlow integral across outputs: {integral_label(*key)}")
          combined[key] = coefficients
      if effective_selected_outputs is not None:
        missing_selected_outputs = sorted(effective_selected_outputs - observed_selected_outputs)
        expect(
            not missing_selected_outputs,
            f"{path} missing compare_cpp_vs_amflow outputs: {', '.join(missing_selected_outputs)}",
        )
      expect(combined, f"{path} did not reference any AMFlow outputs")
      apply_manifest_coefficient_overlays(combined, payload, path)
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


def load_cpp_result(
    path: Path,
    *,
    selected_outputs: set[str] | None = None,
) -> tuple[dict[str, Any], dict[IntegralKey, CoefficientMap]]:
  payload = load_json(path)
  expect(payload.get("schema_version") == 1, f"{path} schema_version must be 1")
  expect(payload.get("status") == "success", f"{path} status must be success")
  raw_results = payload.get("results")
  expect(isinstance(raw_results, list), f"{path} results must be a list")
  has_named_outputs = any(
      isinstance(raw_result, dict)
      and isinstance(raw_result.get("amflow_output_name"), str)
      and raw_result.get("amflow_output_name", "").strip()
      for raw_result in raw_results
  )
  observed_output_names = {
      raw_result.get("amflow_output_name", "").strip()
      for raw_result in raw_results
      if isinstance(raw_result, dict)
      and isinstance(raw_result.get("amflow_output_name"), str)
      and raw_result.get("amflow_output_name", "").strip()
  }
  if selected_outputs is not None and has_named_outputs:
    missing_outputs = sorted(selected_outputs - observed_output_names)
    expect(
        not missing_outputs,
        f"{path} missing selected C++ AMFlow output(s): {', '.join(missing_outputs)}",
    )
  parsed: dict[IntegralKey, CoefficientMap] = {}
  for result_index, raw_result in enumerate(raw_results):
    expect(isinstance(raw_result, dict), f"{path} results entries must be objects")
    if selected_outputs is not None and has_named_outputs:
      output_name = str(raw_result.get("amflow_output_name", "")).strip()
      if output_name not in selected_outputs:
        continue
    integral = str(raw_result.get("integral", "")).strip()
    expect(integral, f"{path} results[{result_index}].integral must not be empty")
    key = parse_integral_label(integral)
    expect(key not in parsed, f"duplicate C++ integral: {integral}")
    raw_orders = raw_result.get("epsilon_orders")
    expect(isinstance(raw_orders, list), f"{path} {integral} epsilon_orders must be a list")
    expect(raw_orders, f"{path} {integral} epsilon_orders must not be empty")
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


def cpp_requested_epsilon_order(payload: dict[str, Any]) -> int | None:
  raw_solver = payload.get("solver")
  if not isinstance(raw_solver, dict):
    return None
  raw_order = raw_solver.get("epsilon_order")
  return raw_order if isinstance(raw_order, int) and raw_order >= 0 else None


def named_cpp_outputs(path: Path) -> set[str]:
  payload = load_json(path)
  raw_results = payload.get("results")
  if not isinstance(raw_results, list):
    return set()
  return {
      raw_result.get("amflow_output_name", "").strip()
      for raw_result in raw_results
      if isinstance(raw_result, dict)
      and isinstance(raw_result.get("amflow_output_name"), str)
      and raw_result.get("amflow_output_name", "").strip()
  }


def underlying_golden_manifest_path(path: Path) -> Path:
  current = path
  while current.suffix == ".json":
    payload = load_json(current)
    if "golden_manifest" not in payload:
      return current
    current = Path(str(payload["golden_manifest"]))
  return current


def digit_agreement(lhs: Decimal, rhs: Decimal) -> int:
  difference = abs(lhs - rhs)
  if difference == 0:
    return 999
  return max(0, -difference.adjusted())


def relative_error_text(actual: Decimal, expected: Decimal) -> str:
  difference = abs(actual - expected)
  if difference == 0:
    return "0"
  if expected == 0:
    return "Infinity"
  return str(difference / abs(expected))


def complex_abs(value: tuple[Decimal, Decimal]) -> Decimal:
  return (value[0] * value[0] + value[1] * value[1]).sqrt()


def complex_relative_error_text(
    actual: tuple[Decimal, Decimal],
    expected: tuple[Decimal, Decimal],
) -> str:
  difference = complex_abs((actual[0] - expected[0], actual[1] - expected[1]))
  if difference == 0:
    return "0"
  scale = complex_abs(expected)
  if scale == 0:
    return "Infinity"
  return str(difference / scale)


def compare_coefficient_maps(
    *,
    cpp: dict[IntegralKey, CoefficientMap],
    amflow: dict[IntegralKey, CoefficientMap],
    requested_epsilon_order: int | None,
    tolerance_digits: int,
) -> dict[str, Any]:
  all_keys = sorted(set(cpp) | set(amflow), key=lambda item: (item[0], item[1]))
  integral_summaries: list[dict[str, Any]] = []
  failures: list[str] = []
  minimum_digit_agreement: int | None = None
  compared_coefficients = 0
  passed_coefficients = 0

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
    if requested_epsilon_order is not None:
      explicit_orders = {
          order
          for order in set(cpp_coefficients) | set(amflow_coefficients)
          if order <= requested_epsilon_order
      }
      explicit_orders.add(0)
      explicit_orders.add(requested_epsilon_order)
    else:
      explicit_orders = set(cpp_coefficients) | set(amflow_coefficients)
    orders = (
        list(range(min(explicit_orders), max(explicit_orders) + 1))
        if explicit_orders
        else []
    )
    for order in orders:
      cpp_present = order in cpp_coefficients
      amflow_present = order in amflow_coefficients
      cpp_value = cpp_coefficients.get(order, ZERO_COMPLEX)
      amflow_value = amflow_coefficients.get(order, ZERO_COMPLEX)
      real_digits = digit_agreement(cpp_value[0], amflow_value[0])
      imag_digits = digit_agreement(cpp_value[1], amflow_value[1])
      relative_error = complex_relative_error_text(cpp_value, amflow_value)
      coefficient_passed = real_digits >= tolerance_digits and imag_digits >= tolerance_digits
      if not coefficient_passed:
        failures.append(
            f"{label} eps^{order} has real/imag agreement {real_digits}/{imag_digits}, "
            f"below tolerance {tolerance_digits}"
        )
      else:
        passed_coefficients += 1
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
              "relative_error_abs": relative_error,
              "real_relative_error_abs": relative_error_text(cpp_value[0], amflow_value[0]),
              "imag_relative_error_abs": relative_error_text(cpp_value[1], amflow_value[1]),
              "cpp_present": cpp_present,
              "amflow_present": amflow_present,
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
      "passed": passed,
      "matched_integral_count": len(set(cpp) & set(amflow)),
      "compared_coefficient_count": compared_coefficients,
      "passed_coefficient_count": passed_coefficients,
      "minimum_digit_agreement": minimum_digit_agreement if minimum_digit_agreement is not None else 0,
      "integrals": integral_summaries,
      "failures": failures,
  }


def prefixed_failures(prefix: str, failures: list[str]) -> list[str]:
  return [prefix + failure for failure in failures]


def compare_cpp_vs_amflow(
    *,
    cpp_result_path: Path,
    amflow_golden_path: Path,
    amflow_state_path: Path | None = None,
    tolerance_digits: int,
    family_aliases: FamilyAliasMap | None = None,
) -> dict[str, Any]:
  getcontext().prec = max(120, tolerance_digits + 40)
  selected_cpp_outputs = selected_outputs_for_manifest(amflow_golden_path)
  cpp_payload, cpp = load_cpp_result(
      cpp_result_path,
      selected_outputs=selected_cpp_outputs,
  )
  amflow = load_amflow_golden(amflow_golden_path)
  aliases = family_aliases or {}
  requested_epsilon_order = cpp_requested_epsilon_order(cpp_payload)
  comparison = compare_coefficient_maps(
      cpp=normalize_key_map(cpp, aliases, "C++"),
      amflow=normalize_key_map(amflow, aliases, "AMFlow"),
      requested_epsilon_order=requested_epsilon_order,
      tolerance_digits=tolerance_digits,
  )

  named_output_failures: list[str] = []
  named_outputs = named_cpp_outputs(cpp_result_path)
  if selected_cpp_outputs is not None and named_outputs:
    base_golden = underlying_golden_manifest_path(amflow_golden_path)
    for output_name in sorted(named_outputs):
      _, named_cpp = load_cpp_result(cpp_result_path, selected_outputs={output_name})
      named_amflow = load_amflow_golden(base_golden, selected_outputs={output_name})
      named_comparison = compare_coefficient_maps(
          cpp=normalize_key_map(named_cpp, aliases, f"C++ {output_name}"),
          amflow=normalize_key_map(named_amflow, aliases, f"AMFlow {output_name}"),
          requested_epsilon_order=requested_epsilon_order,
          tolerance_digits=tolerance_digits,
      )
      if not named_comparison["passed"]:
        named_output_failures.extend(
            prefixed_failures(f"named output {output_name}: ",
                              named_comparison["failures"])
        )

  failures = comparison["failures"] + named_output_failures
  passed = not failures and comparison["compared_coefficient_count"] > 0
  payload: dict[str, Any] = {
      "schema_version": 1,
      "comparison": "cpp-vs-amflow",
      "cpp_result": str(cpp_result_path),
      "amflow_golden": str(amflow_golden_path),
      "benchmark_id": cpp_payload.get("benchmark_id", ""),
      "tolerance_digits": tolerance_digits,
      "family_aliases": aliases,
      "default_family_suffix_normalization": "_amflow -> stripped",
      "passed": passed,
      "matched_integral_count": comparison["matched_integral_count"],
      "compared_coefficient_count": comparison["compared_coefficient_count"],
      "passed_coefficient_count": comparison["passed_coefficient_count"],
      "minimum_digit_agreement": comparison["minimum_digit_agreement"],
      "integrals": comparison["integrals"],
      "failures": failures,
  }
  if amflow_state_path is not None:
    payload["amflow_state"] = str(amflow_state_path)
  return payload


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
    matching_state_path = matching_cpp.parent / "amflow-state.json"
    matching_state_path.write_text("{}\n", encoding="utf-8")
    matching_with_state = compare_cpp_vs_amflow(
        cpp_result_path=matching_cpp,
        amflow_golden_path=matching_amflow,
        amflow_state_path=matching_state_path,
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

    bounded_root = root / "bounded-positive-order"
    bounded_root.mkdir(parents=True, exist_ok=True)
    bounded_amflow = bounded_root / "amflow-golden.txt"
    bounded_amflow.write_text(
        "{j[toy, 1] -> 1.0000000000000000000000000000000000000000`40. + "
        "2.0000000000000000000000000000000000000000`40.*eps + "
        "999.0000000000000000000000000000000000000000`40.*eps^2}\n",
        encoding="utf-8",
    )
    bounded_cpp = bounded_root / "cpp-result.json"
    bounded_cpp.write_text(
        json.dumps(
            {
                "schema_version": 1,
                "benchmark_id": "bounded",
                "solver": {"precision_digits": 40, "epsilon_order": 1},
                "results": [
                    {
                        "integral": "toy[1]",
                        "epsilon_orders": [
                            {"order": 0, "real_digits": "1.0000000000000000000000000000000000000000", "imag_digits": "0"},
                            {"order": 1, "real_digits": "2.0000000000000000000000000000000000000000", "imag_digits": "0"},
                        ],
                    }
                ],
                "status": "success",
            },
            indent=2,
            sort_keys=True,
        )
        + "\n",
        encoding="utf-8",
    )
    bounded = compare_cpp_vs_amflow(
        cpp_result_path=bounded_cpp,
        amflow_golden_path=bounded_amflow,
        tolerance_digits=30,
    )

    suffix_root = root / "suffix-family-normalization"
    suffix_cpp, suffix_amflow = write_synthetic_inputs(suffix_root)
    suffix_payload = load_json(suffix_cpp)
    suffix_payload["family"] = "toy_amflow"
    suffix_payload["targets"] = ["toy_amflow[1]"]
    suffix_payload["results"][0]["integral"] = "toy_amflow[1]"
    suffix_cpp.write_text(json.dumps(suffix_payload, indent=2, sort_keys=True) + "\n",
                          encoding="utf-8")
    suffix_normalized = compare_cpp_vs_amflow(
        cpp_result_path=suffix_cpp,
        amflow_golden_path=suffix_amflow,
        tolerance_digits=30,
    )

    alias_root = root / "explicit-family-alias"
    alias_cpp, alias_amflow = write_synthetic_inputs(alias_root)
    alias_payload = load_json(alias_cpp)
    alias_payload["family"] = "cpp_box"
    alias_payload["targets"] = ["cpp_box[1]"]
    alias_payload["results"][0]["integral"] = "cpp_box[1]"
    alias_cpp.write_text(json.dumps(alias_payload, indent=2, sort_keys=True) + "\n",
                         encoding="utf-8")
    alias_normalized = compare_cpp_vs_amflow(
        cpp_result_path=alias_cpp,
        amflow_golden_path=alias_amflow,
        tolerance_digits=30,
        family_aliases={"cpp_box": "toy"},
    )

    scientific_root = root / "mathematica-scientific-notation"
    scientific_root.mkdir(parents=True, exist_ok=True)
    scientific_amflow = scientific_root / "amflow-golden.txt"
    scientific_amflow.write_text(
        "{j[phase, 1] -> -9.09128955251943629976022322143`20.*^-9 - "
        "2.54510214908455564902320491544`20.*^-10/eps^3}\n",
        encoding="utf-8",
    )
    scientific_cpp = scientific_root / "cpp-result.json"
    scientific_cpp.write_text(
        json.dumps(
            {
                "schema_version": 1,
                "benchmark_id": "scientific-notation",
                "solver": {"precision_digits": 40, "epsilon_order": 0},
                "results": [
                    {
                        "integral": "phase[1]",
                        "epsilon_orders": [
                            {
                                "order": -3,
                                "real_digits": "-2.54510214908455564902320491544E-10",
                                "imag_digits": "0",
                            },
                            {
                                "order": 0,
                                "real_digits": "-9.09128955251943629976022322143E-9",
                                "imag_digits": "0",
                            },
                        ],
                    }
                ],
                "status": "success",
            },
            indent=2,
            sort_keys=True,
        )
        + "\n",
        encoding="utf-8",
    )
    scientific_notation = compare_cpp_vs_amflow(
        cpp_result_path=scientific_cpp,
        amflow_golden_path=scientific_amflow,
        tolerance_digits=30,
    )

    selected_output_root = root / "selected-manifest-output"
    selected_cpp, selected_amflow = write_synthetic_inputs(selected_output_root)
    selected_rule = selected_amflow.read_text(encoding="utf-8").strip()[1:-1]
    selected_lhs, selected_rhs = selected_rule.split("->", 1)
    selected_amflow.write_text(
        "{" + selected_lhs.strip() + " -> {" + selected_rhs.strip() + "}}\n",
        encoding="utf-8",
    )
    structural_output = selected_output_root / "structural.txt"
    structural_output.write_text("{{j[toy, 1]}, {{(1 - eps)/s}}}\n", encoding="utf-8")
    duplicate_kinematic_output = selected_output_root / "different-point.txt"
    duplicate_kinematic_output.write_text(
        "{j[toy, 1] -> 9.0000000000000000000000000000000000000000`40.}\n",
        encoding="utf-8",
    )
    full_manifest = selected_output_root / "full-manifest.json"
    full_manifest.write_text(
        json.dumps(
            {
                "schema_version": 1,
                "benchmark_id": "selected-output",
                "outputs": [
                    {"name": "redtable", "canonical_text": str(structural_output)},
                    {"name": "sol1", "canonical_text": str(selected_amflow)},
                    {"name": "sol2", "canonical_text": str(duplicate_kinematic_output)},
                ],
            },
            indent=2,
            sort_keys=True,
        )
        + "\n",
        encoding="utf-8",
    )
    selected_wrapper = selected_output_root / "wrapper-manifest.json"
    selected_wrapper.write_text(
        json.dumps(
            {
                "schema_version": 1,
                "benchmark_id": "selected-output",
                "golden_manifest": str(full_manifest),
                "compare_cpp_vs_amflow_outputs": ["sol1"],
            },
            indent=2,
            sort_keys=True,
        )
        + "\n",
        encoding="utf-8",
    )
    selected_manifest_output = compare_cpp_vs_amflow(
        cpp_result_path=selected_cpp,
        amflow_golden_path=selected_wrapper,
        tolerance_digits=30,
    )
    selected_named_cpp = selected_output_root / "selected-named-cpp-result.json"
    selected_named_payload = load_json(selected_cpp)
    selected_named_payload["results"] = [
        {
            **selected_named_payload["results"][0],
            "amflow_output_name": "sol1",
        },
        {
            **selected_named_payload["results"][0],
            "amflow_output_name": "sol2",
            "epsilon_orders": [
                {
                    **selected_named_payload["results"][0]["epsilon_orders"][0],
                    "real_digits": "9.0000000000000000000000000000000000000000",
                }
            ],
        },
    ]
    selected_named_cpp.write_text(
        json.dumps(selected_named_payload, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    selected_bad_cpp_output = compare_cpp_vs_amflow(
        cpp_result_path=selected_named_cpp,
        amflow_golden_path=selected_wrapper,
        tolerance_digits=30,
    )

    mismatch_coefficients = [
        coefficient
        for integral in mismatch["integrals"]
        for coefficient in integral.get("coefficients", [])
        if isinstance(coefficient, dict)
    ]

  return {
      "schema_version": 1,
      "self_check": "compare_cpp_vs_amflow",
      "matching_synthetic_passed": matching["passed"],
      "amflow_state_binding_reported": (
          matching_with_state["passed"]
          and matching_with_state.get("amflow_state") == str(matching_state_path)
      ),
      "mismatch_synthetic_rejected": not mismatch["passed"] and bool(mismatch["failures"]),
      "missing_integral_rejected": not missing["passed"] and bool(missing["failures"]),
      "failed_cpp_status_rejected": failed_status_rejected,
      "positive_order_above_request_ignored": bounded["passed"],
      "amflow_suffix_family_normalized": suffix_normalized["passed"],
      "explicit_family_alias_normalized": alias_normalized["passed"],
      "mathematica_scientific_notation_parsed": scientific_notation["passed"],
      "selected_manifest_output_loaded": selected_manifest_output["passed"],
      "selected_cpp_output_rejects_bad_unselected": (
          not selected_bad_cpp_output["passed"] and bool(selected_bad_cpp_output["failures"])
      ),
      "coefficient_relative_error_reported": any(
          "relative_error_abs" in coefficient
          and "real_relative_error_abs" in coefficient
          and "imag_relative_error_abs" in coefficient
          for coefficient in mismatch_coefficients
      ),
      "passed_coefficient_count_reported": (
          matching["passed_coefficient_count"] == matching["compared_coefficient_count"]
          and mismatch["passed_coefficient_count"] < mismatch["compared_coefficient_count"]
      ),
      "summary_written": False,
  }


def parse_args(argv: list[str]) -> argparse.Namespace:
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument("--cpp-result", type=Path)
  parser.add_argument("--amflow-golden", type=Path)
  parser.add_argument(
      "--amflow-state",
      type=Path,
      help="Optional retained AMFlow state JSON path to bind into the comparison summary.",
  )
  parser.add_argument("--tolerance-digits", type=int, default=30)
  parser.add_argument(
      "--family-alias",
      action="append",
      default=[],
      metavar="FROM=TO",
      help=(
          "Normalize one result family name before matching; may be repeated. "
          "The comparator also strips a trailing _amflow suffix by default."
      ),
  )
  parser.add_argument("--self-check", action="store_true")
  return parser.parse_args(argv)


def parse_family_aliases(raw_aliases: list[str]) -> FamilyAliasMap:
  aliases: FamilyAliasMap = {}
  for raw_alias in raw_aliases:
    if raw_alias.count("=") != 1:
      raise ValueError(f"--family-alias must have FROM=TO syntax: {raw_alias!r}")
    source, target = (part.strip() for part in raw_alias.split("=", 1))
    if not source or not target:
      raise ValueError(f"--family-alias must not contain empty names: {raw_alias!r}")
    if source in aliases and aliases[source] != target:
      raise ValueError(f"conflicting --family-alias mapping for {source!r}")
    aliases[source] = target
  return aliases


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
              "amflow_state_binding_reported",
              "mismatch_synthetic_rejected",
              "missing_integral_rejected",
              "failed_cpp_status_rejected",
              "positive_order_above_request_ignored",
              "amflow_suffix_family_normalized",
              "explicit_family_alias_normalized",
              "selected_manifest_output_loaded",
              "selected_cpp_output_rejects_bad_unselected",
              "coefficient_relative_error_reported",
          )
      ) else 1

    expect(args.cpp_result is not None, "--cpp-result is required outside --self-check")
    expect(args.amflow_golden is not None, "--amflow-golden is required outside --self-check")
    expect(args.tolerance_digits > 0, "--tolerance-digits must be positive")
    payload = compare_cpp_vs_amflow(
        cpp_result_path=args.cpp_result,
        amflow_golden_path=args.amflow_golden,
        amflow_state_path=args.amflow_state,
        tolerance_digits=args.tolerance_digits,
        family_aliases=parse_family_aliases(args.family_alias),
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
