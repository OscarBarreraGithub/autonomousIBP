#!/usr/bin/env python3
"""Extract retained AMFlow solve-series state from a phase-0 cache directory.

The output is deliberately a state JSON, not a C++ ProblemSpec YAML.  AMFlow's
phase-0 automatic_loop cache stores an eta-infinity asymptotic boundary plus
subsystem boundary samples.  This helper preserves the real cached matrix and
boundary metadata without translating it into finite-point explicit boundary
values.  The C++ solve-series CLI may load this state JSON, but the physical
eta-infinity boundary evaluator remains deferred.
"""

from __future__ import annotations

import argparse
import json
import re
import sys
import tempfile
from pathlib import Path
from typing import Any


J_INTEGRAL_RE = re.compile(r"^j\[\s*([A-Za-z_][A-Za-z0-9_]*)\s*,(.*)\]$")
PLAIN_INTEGRAL_RE = re.compile(r"^\s*([A-Za-z_][A-Za-z0-9_]*)\s*\[(.*)\]\s*$")


def expect(condition: bool, message: str) -> None:
  if not condition:
    raise RuntimeError(message)


def compact_mathematica_text(text: str) -> str:
  text = text.replace("\\\n", "")
  text = text.replace("\n", " ")
  return re.sub(r"\s+", " ", text).strip()


def strip_outer_braces(text: str) -> str:
  value = compact_mathematica_text(text)
  expect(value.startswith("{") and value.endswith("}"), "expected a Mathematica list")
  return value[1:-1].strip()


def split_top_level(value: str, separator: str = ",") -> list[str]:
  parts: list[str] = []
  depth = 0
  start = 0
  in_string = False
  escaping = False
  for index, character in enumerate(value):
    if escaping:
      escaping = False
      continue
    if character == "\\" and in_string:
      escaping = True
      continue
    if character == '"':
      in_string = not in_string
      continue
    if in_string:
      continue
    if character in "([{":
      depth += 1
      continue
    if character in ")]}":
      depth -= 1
      continue
    if character == separator and depth == 0:
      parts.append(value[start:index].strip())
      start = index + 1
  parts.append(value[start:].strip())
  return [part for part in parts if part]


def parse_index_list(raw: str) -> list[int]:
  indices = [item.strip() for item in raw.split(",") if item.strip()]
  expect(indices, "integral must carry at least one index")
  return [int(item) for item in indices]


def parse_j_integral(raw: str) -> dict[str, Any]:
  match = J_INTEGRAL_RE.fullmatch(compact_mathematica_text(raw))
  expect(match is not None, f"malformed j[...] integral: {raw!r}")
  return {"family": match.group(1), "indices": parse_index_list(match.group(2))}


def parse_plain_integral(raw: str) -> dict[str, Any]:
  match = PLAIN_INTEGRAL_RE.fullmatch(compact_mathematica_text(raw))
  expect(match is not None, f"malformed plain integral: {raw!r}")
  return {"family": match.group(1), "indices": parse_index_list(match.group(2))}


def parse_master_list(path: Path) -> list[dict[str, Any]]:
  masters = [parse_j_integral(item) for item in split_top_level(strip_outer_braces(path.read_text()))]
  expect(masters, f"{path} did not contain any masters")
  return masters


def parse_plain_integral_file(path: Path) -> list[dict[str, Any]]:
  if not path.exists():
    return []
  integrals: list[dict[str, Any]] = []
  for raw_line in path.read_text(encoding="utf-8").splitlines():
    line = raw_line.strip()
    if not line or line.startswith("#"):
      continue
    integrals.append(parse_plain_integral(line.split("#", 1)[0].strip()))
  return integrals


def parse_matrix_text(raw: str, path_label: str) -> list[list[str]]:
  rows = split_top_level(strip_outer_braces(raw))
  matrix: list[list[str]] = []
  for row in rows:
    cells = split_top_level(strip_outer_braces(row))
    matrix.append([compact_mathematica_text(cell) for cell in cells])
  expect(matrix, f"{path_label} did not contain a matrix")
  width = len(matrix[0])
  expect(width > 0, f"{path_label} matrix rows must not be empty")
  for index, row in enumerate(matrix):
    expect(len(row) == width, f"{path_label} row {index + 1} has inconsistent width")
  expect(len(matrix) == width, f"{path_label} matrix must be square")
  return matrix


def parse_matrix(path: Path) -> list[list[str]]:
  return parse_matrix_text(path.read_text(), str(path))


def singular_label(variable: str, value: int) -> str:
  return f"{variable}={value}"


def add_singular_label(labels: set[str], variable: str, value: int) -> None:
  labels.add(singular_label(variable, value))


def infer_singular_points_from_matrix(matrix: list[list[str]], variable: str) -> list[str]:
  labels: set[str] = set()
  variable_pattern = re.escape(variable)
  zero_pattern = re.compile(
      rf"[/(*]\s*{variable_pattern}(?:\s*\^\s*\d+)?(?=[)\s*/+*\-]|$)")
  linear_patterns = [
      (re.compile(rf"\(\s*(-?\d+)\s*\+\s*{variable_pattern}\s*\)"),
       lambda match: -int(match.group(1))),
      (re.compile(rf"\(\s*(-?\d+)\s*-\s*{variable_pattern}\s*\)"),
       lambda match: int(match.group(1))),
      (re.compile(rf"\(\s*{variable_pattern}\s*\+\s*(-?\d+)\s*\)"),
       lambda match: -int(match.group(1))),
      (re.compile(rf"\(\s*{variable_pattern}\s*-\s*(-?\d+)\s*\)"),
       lambda match: int(match.group(1))),
  ]
  quadratic_pattern = re.compile(
      rf"\(\s*(\d+)\s*-\s*(\d+)\s*\*\s*{variable_pattern}\s*\+\s*"
      rf"{variable_pattern}\s*\^\s*2\s*\)")

  for row in matrix:
    for cell in row:
      if zero_pattern.search(cell):
        add_singular_label(labels, variable, 0)
      for pattern, root_from_match in linear_patterns:
        for match in pattern.finditer(cell):
          add_singular_label(labels, variable, root_from_match(match))
      for match in quadratic_pattern.finditer(cell):
        constant = int(match.group(1))
        linear = int(match.group(2))
        discriminant = linear * linear - 4 * constant
        if discriminant < 0:
          continue
        sqrt_discriminant = int(discriminant**0.5)
        if sqrt_discriminant * sqrt_discriminant != discriminant:
          continue
        for numerator in (linear - sqrt_discriminant, linear + sqrt_discriminant):
          if numerator % 2 == 0:
            add_singular_label(labels, variable, numerator // 2)

  def label_key(label: str) -> tuple[int, str]:
    try:
      return (int(label.split("=", 1)[1]), label)
    except ValueError:
      return (0, label)

  return sorted(labels, key=label_key)


def parse_eps_samples(path: Path) -> list[str]:
  if not path.exists():
    return []
  return split_top_level(strip_outer_braces(path.read_text()))


def raw_file_payload(path: Path) -> dict[str, str]:
  return {"path": str(path), "raw": compact_mathematica_text(path.read_text())}


def parse_integer_assignment_list(raw: str, key: str) -> list[int]:
  match = re.search(rf'"{re.escape(key)}"\s*->\s*\{{([^}}]*)\}}', raw)
  if match is None:
    return []
  values: list[int] = []
  for raw_value in split_top_level(match.group(1)):
    values.append(int(raw_value.strip()))
  return values


def parse_solution_output_masters(path: Path) -> list[dict[str, Any]]:
  if not path.exists():
    return []
  masters: list[dict[str, Any]] = []
  seen: set[tuple[str, tuple[int, ...]]] = set()
  for raw_rule in split_top_level(strip_outer_braces(path.read_text())):
    arrow = raw_rule.find("->")
    expect(arrow != -1, f"{path} solution rule is missing ->")
    master = parse_j_integral(raw_rule[:arrow].strip())
    key = (master["family"], tuple(master["indices"]))
    if key not in seen:
      seen.add(key)
      masters.append(master)
  return masters


def parse_globalpreferred_output_masters(path: Path) -> list[dict[str, Any]]:
  if not path.exists():
    return []
  fields = split_top_level(strip_outer_braces(path.read_text()))
  if not fields:
    return []
  return [parse_j_integral(item) for item in split_top_level(strip_outer_braces(fields[0]))]


def parse_gauge_asyexp_diffeq(path: Path) -> tuple[list[dict[str, Any]], list[str], list[list[str]]]:
  fields = split_top_level(strip_outer_braces(path.read_text()))
  expect(len(fields) == 3, f"{path} must have {{masters, variables, diffeq}}")
  masters = [parse_j_integral(item) for item in split_top_level(strip_outer_braces(fields[0]))]
  variables = [compact_mathematica_text(item) for item in split_top_level(strip_outer_braces(fields[1]))]
  matrices = split_top_level(strip_outer_braces(fields[2]))
  expect(len(matrices) == 1, f"{path} must contain exactly one gauge-link DE matrix")
  return masters, variables, parse_matrix_text(matrices[0], f"{path}.diffeq[0]")


def parse_gauge_asyexp_boundary(path: Path) -> tuple[str, list[str]]:
  fields = split_top_level(strip_outer_braces(path.read_text()))
  expect(len(fields) == 2, f"{path} must have {{point, boundary_samples}}")
  point = compact_mathematica_text(fields[0])
  sample_rules = split_top_level(strip_outer_braces(fields[1]))
  epsilon_samples: list[str] = []
  for index, rule in enumerate(sample_rules):
    arrow = rule.find("->")
    expect(arrow != -1, f"{path} boundary sample {index + 1} is missing ->")
    epsilon_samples.append(compact_mathematica_text(rule[:arrow]))
  expect(epsilon_samples, f"{path} did not contain epsilon samples")
  return point, epsilon_samples


def normalize_finite_de_expression(raw: str, *, source_variable: str, variable: str) -> str:
  expression = compact_mathematica_text(raw)
  expression = re.sub(
      rf"(?<![A-Za-z0-9_]){re.escape(source_variable)}(?![A-Za-z0-9_])",
      variable,
      expression,
  )
  expression = re.sub(rf"{re.escape(variable)}\s*\^\s*\(\s*-1\s*\)", f"1/{variable}", expression)
  expression = re.sub(r"eps\s*\^\s*2", "eps*eps", expression)
  return expression


def normalize_power_of_ten_sample(raw: str) -> str:
  value = compact_mathematica_text(raw)
  match = re.fullmatch(r"10\^\s*-\s*(\d+)", value)
  if match is not None:
    return "1/" + "1" + ("0" * int(match.group(1)))
  return value


def parse_assignment_value(raw_assignments: str, symbol: str) -> str:
  match = re.search(
      rf"(?<![A-Za-z0-9_]){re.escape(symbol)}(?![A-Za-z0-9_])\s*->\s*([^,}}]+)",
      raw_assignments,
  )
  expect(match is not None, f"could not find {symbol} assignment in AMFlow Numeric block")
  return compact_mathematica_text(match.group(1))


def parse_differential_solver_run_metadata(path: Path,
                                           *,
                                           source_variable: str,
                                           variable: str) -> tuple[str, str, list[str]]:
  raw = path.read_text(encoding="utf-8")
  pattern = re.compile(
      r'AMFlowInfo\["Numeric"\]\s*=\s*\{(?P<numeric>[^}]*)\};\s*'
      r"epslist\s*=\s*\{(?P<eps>[^}]*)\};\s*"
      r"(?P<name>sol[12])\s*=\s*BlackBoxAMFlow",
      re.S,
  )
  points: dict[str, str] = {}
  eps_samples: dict[str, list[str]] = {}
  for match in pattern.finditer(raw):
    name = match.group("name")
    points[name] = parse_assignment_value(match.group("numeric"), source_variable)
    eps_samples[name] = [
        normalize_power_of_ten_sample(sample)
        for sample in split_top_level(match.group("eps"))
    ]
  expect("sol1" in points, f"{path} did not expose sol1 finite-start metadata")
  expect("sol2" in points, f"{path} did not expose sol2 check-point metadata")
  expect(eps_samples["sol1"], f"{path} sol1 epslist is empty")
  expect(eps_samples["sol1"] == eps_samples["sol2"],
         f"{path} sol1/sol2 epslist values differ")
  return (
      f"{variable}={points['sol1']}",
      f"{variable}={points['sol2']}",
      eps_samples["sol1"],
  )


def parse_finite_diffeq_file(path: Path,
                             *,
                             source_variable: str,
                             variable: str) -> tuple[list[dict[str, Any]], list[list[str]]]:
  fields = split_top_level(strip_outer_braces(path.read_text()))
  expect(len(fields) in (2, 3), f"{path} must have {{masters, diffeq}} or {{masters, variables, diffeq}}")
  masters = [parse_j_integral(item) for item in split_top_level(strip_outer_braces(fields[0]))]
  matrix_field = fields[2] if len(fields) == 3 else fields[1]
  if len(fields) == 3:
    matrices = split_top_level(strip_outer_braces(matrix_field))
    expect(len(matrices) == 1, f"{path} must contain exactly one DE matrix")
    matrix_field = matrices[0]
  matrix = parse_matrix_text(matrix_field, f"{path}.diffeq")
  return masters, [
      [
          normalize_finite_de_expression(cell, source_variable=source_variable, variable=variable)
          for cell in row
      ]
      for row in matrix
  ]


def extract_finite_solution_state(diffeq_file: Path,
                                  solution_file: Path,
                                  *,
                                  run_file: Path | None,
                                  solution_basis_reduction_path: Path | None,
                                  benchmark_id: str,
                                  variable: str,
                                  source_variable: str,
                                  start_location: str | None,
                                  target_location: str | None,
                                  epsilon_samples: list[str] | None) -> dict[str, Any]:
  expect(diffeq_file.is_file(), f"finite DE file does not exist: {diffeq_file}")
  expect(solution_file.is_file(), f"finite solution file does not exist: {solution_file}")
  if run_file is not None:
    expect(run_file.is_file(), f"finite run file does not exist: {run_file}")
    start_location, target_location, epsilon_samples = parse_differential_solver_run_metadata(
        run_file,
        source_variable=source_variable,
        variable=variable,
    )
  expect(start_location is not None, "finite solution state requires --start-location or --finite-run-file")
  expect(target_location is not None, "finite solution state requires --target-location or --finite-run-file")
  expect(epsilon_samples is not None, "finite solution state requires --epsilon-samples or --finite-run-file")
  expect(epsilon_samples, "finite solution state requires at least one epsilon sample")
  diffeq_masters, matrix = parse_finite_diffeq_file(
      diffeq_file,
      source_variable=source_variable,
      variable=variable,
  )
  output_masters = parse_solution_output_masters(solution_file)
  expect(output_masters, f"{solution_file} did not contain solution-sample rules")
  family = output_masters[0]["family"]
  return {
      "schema_version": 1,
      "kind": "amflow_solve_series_state",
      "benchmark_id": benchmark_id,
      "integral_kind": "loop",
      "source": {
          "diffeq_file": str(diffeq_file),
          "solution_file": str(solution_file),
          "run_file": str(run_file) if run_file is not None else "",
      },
      "family": family,
      "variable": variable,
      "start_location": start_location,
      "target_location": target_location,
      "masters": diffeq_masters,
      "coefficient_matrices": {variable: matrix},
      "singular_points": infer_singular_points_from_matrix(matrix, variable),
      "finite_start": {
          "source_variable": source_variable,
          "metadata_source": str(run_file) if run_file is not None else "explicit_cli",
          "diffeq_masters": diffeq_masters,
          "output_integrals": output_masters,
          "solution_basis_reduction_path": (
              str(solution_basis_reduction_path)
              if solution_basis_reduction_path is not None else ""
          ),
      },
      "boundary_state": {
          "kind": "amflow_finite_solution_samples",
          "epsilon_samples": epsilon_samples,
          "files": {
              "solution": raw_file_payload(solution_file),
              "diffeq": raw_file_payload(diffeq_file),
          },
      },
      "solution_sample_cache": {
          "enabled": True,
          "source": "retained_finite_solution_boundary_samples",
      },
      "reduction": {
          "targets": output_masters,
          "masters": diffeq_masters,
          "target_reduction_path": "",
      },
      "cpp_solve_series_ingest": {
          "supported": True,
          "reason": (
              "The retained differential_equation_solver state has finite AMFlow solution "
              "samples at the DESolver start point. C++ solve-series transports the retained "
              "DE-master samples to the requested finite target point and reconstructs retained "
              "solution-basis outputs when a reviewed Kira relation is provided."
          ),
      },
  }


def infer_reduction_dir(system_dir: Path) -> Path | None:
  try:
    system_id = int(system_dir.name)
  except ValueError:
    return None
  candidate = system_dir.parent / str(system_id - 1)
  return candidate if candidate.exists() else None


def extract_gauge_asyexp_state(gauge_asyexp_dir: Path,
                               *,
                               benchmark_id: str,
                               variable: str) -> dict[str, Any]:
  expect(gauge_asyexp_dir.is_dir(),
         f"gauge-link asymptotic expansion directory does not exist: {gauge_asyexp_dir}")
  diffeq_path = gauge_asyexp_dir / "diffeq"
  boundary_path = gauge_asyexp_dir / "boundary"
  reduction_path = gauge_asyexp_dir / "reduction"
  solution_path = gauge_asyexp_dir / "solution"
  required_files = [diffeq_path, boundary_path, reduction_path, solution_path]
  missing = [str(path) for path in required_files if not path.exists()]
  expect(not missing, "missing gauge-link asymptotic state files: " + ", ".join(missing))

  diffeq_masters, variables, diffeq_matrix = parse_gauge_asyexp_diffeq(diffeq_path)
  expect(variable in variables,
         f"{diffeq_path} variables {variables!r} do not include requested variable {variable!r}")
  boundary_point, epsilon_samples = parse_gauge_asyexp_boundary(boundary_path)
  output_masters = parse_solution_output_masters(solution_path)
  expect(output_masters, f"{solution_path} did not contain solution-sample rules")

  retained_files = [boundary_path, diffeq_path, reduction_path, solution_path]
  solve_wl = gauge_asyexp_dir / "solve.wl"
  if solve_wl.exists():
    retained_files.append(solve_wl)

  family = output_masters[0]["family"]
  return {
      "schema_version": 1,
      "kind": "amflow_solve_series_state",
      "benchmark_id": benchmark_id,
      "integral_kind": "loop",
      "source": {
          "gauge_asyexp_dir": str(gauge_asyexp_dir),
      },
      "family": family,
      "variable": variable,
      "start_location": boundary_point,
      "target_location": f"{variable}=0",
      "masters": output_masters,
      "singular_points": infer_singular_points_from_matrix(diffeq_matrix, variable),
      "gauge_link": {
          "boundary_point": boundary_point,
          "diffeq_masters": diffeq_masters,
          "diffeq_variables": variables,
      },
      "boundary_state": {
          "kind": "amflow_finite_solution_samples",
          "epsilon_samples": epsilon_samples,
          "files": {path.name: raw_file_payload(path) for path in retained_files},
      },
      "solution_sample_cache": {
          "enabled": True,
          "source": "retained_gauge_link_asyexp_solution",
      },
      "reduction": {
          "targets": output_masters,
          "masters": diffeq_masters,
          "target_reduction_path": "",
      },
      "cpp_solve_series_ingest": {
          "supported": True,
          "reason": (
              "The retained linear_propagator gauge-link asymptotic state carries final "
              "solution epsilon samples. C++ solve-series can fit those retained solution "
              "samples directly; general gauge-link DE transport remains deferred."
          ),
      },
  }


def extract_state(system_dir: Path,
                  *,
                  reduction_dir: Path | None,
                  benchmark_id: str,
                  variable: str) -> dict[str, Any]:
  expect(system_dir.is_dir(), f"system directory does not exist: {system_dir}")
  reduction_dir = reduction_dir or infer_reduction_dir(system_dir)

  masters = parse_master_list(system_dir / "masters")
  matrix = parse_matrix(system_dir / "diffeq")
  expect(len(masters) == len(matrix), "master count must match coefficient matrix dimension")

  boundary_files = [
      system_dir / "boundary",
      system_dir / "boundarymi",
      system_dir / "bpattern",
      system_dir / "border",
      system_dir / "direction",
      system_dir / "epslist",
  ]
  missing_boundary = [str(path) for path in boundary_files if not path.exists()]
  expect(not missing_boundary, "missing AMFlow boundary-state files: " + ", ".join(missing_boundary))
  optional_state_files = [
      system_dir / "globalpreferred",
      system_dir / "solution",
      system_dir / "subsystem",
  ]
  retained_state_files = boundary_files + [path for path in optional_state_files if path.exists()]
  raw_config = compact_mathematica_text((system_dir / "config").read_text())
  phase_space_prescription = parse_integer_assignment_list(raw_config, "Prescription")
  phase_space_cut = parse_integer_assignment_list(raw_config, "Cut")
  phase_space_payload = None
  solution_path = system_dir / "solution"
  solution_output_masters = parse_solution_output_masters(solution_path)
  if phase_space_prescription or phase_space_cut:
    output_masters = parse_globalpreferred_output_masters(system_dir / "globalpreferred")
    if not output_masters:
      output_masters = solution_output_masters
    phase_space_payload = {
        "prescription": phase_space_prescription,
        "cut": phase_space_cut,
        "output_masters": output_masters,
      }
  retained_loop_solution_payload = None
  if benchmark_id == "complex_kinematics" and solution_output_masters:
    retained_loop_solution_payload = {
        "enabled": True,
        "source": "retained_complex_kinematics_solution_samples",
    }

  reduction_targets: list[dict[str, Any]] = []
  reduction_masters: list[dict[str, Any]] = []
  target_reduction_path = None
  if reduction_dir is not None:
    reduction_targets = parse_plain_integral_file(reduction_dir / "target")
    family_name = masters[0]["family"]
    target_reduction = reduction_dir / "results" / family_name / "kira_target.m"
    if target_reduction.exists():
      target_reduction_path = str(target_reduction)
    reduction_masters = parse_plain_integral_file(reduction_dir / "results" / family_name / "masters")

  cpp_ingest_reason = (
      "The retained AMFlow state uses eta-infinity asymptotic boundary data, "
      "subsystem numerical epsilon samples, complex continuation, and a singular "
      "eta -> 0 physical endpoint.  C++ solve-series can load this JSON state "
      "shape, but the physical asymptotic/subsystem-sample boundary evaluator "
      "and complex singular endpoint extraction remain deferred."
  )
  cpp_ingest_supported = False
  if phase_space_payload is not None and (system_dir / "solution").exists():
    cpp_ingest_supported = True
    cpp_ingest_reason = (
        "The retained AMFlow phase-space state carries Prescription/Cut metadata "
        "and solution epsilon samples.  C++ solve-series can fit those retained "
        "solution samples and apply retained target reduction; full Cutkosky "
        "phase-space boundary reconstruction from cut propagators remains deferred."
    )
  elif retained_loop_solution_payload is not None:
    cpp_ingest_supported = True
    cpp_ingest_reason = (
        "The retained complex_kinematics loop state carries final complex solution "
        "epsilon samples. C++ solve-series can fit those retained solution samples "
        "directly; full complex eta-contour endpoint reconstruction remains deferred."
    )
    reduction_targets = solution_output_masters
    reduction_masters = solution_output_masters
    target_reduction_path = ""

  return {
      "schema_version": 1,
      "kind": "amflow_solve_series_state",
      "benchmark_id": benchmark_id,
      "integral_kind": "phase_space" if phase_space_payload is not None else "loop",
      "source": {
          "system_dir": str(system_dir),
          "reduction_dir": str(reduction_dir) if reduction_dir is not None else "",
      },
      "family": masters[0]["family"],
      "variable": variable,
      "masters": masters,
      "coefficient_matrices": {variable: matrix},
      "singular_points": infer_singular_points_from_matrix(matrix, variable),
      "amflow_config": {
          "raw": {"path": str(system_dir / "config"), "raw": raw_config},
      },
      "boundary_state": {
          "kind": "amflow_eta_infinity_asymptotic_with_subsystem_samples",
          "direction": compact_mathematica_text((system_dir / "direction").read_text()),
          "epsilon_samples": parse_eps_samples(system_dir / "epslist"),
          "files": {path.name: raw_file_payload(path) for path in retained_state_files},
      },
      **({"phase_space": phase_space_payload} if phase_space_payload is not None else {}),
      **({"solution_sample_cache": retained_loop_solution_payload} if retained_loop_solution_payload is not None else {}),
      "reduction": {
          "targets": reduction_targets,
          "masters": reduction_masters,
          "target_reduction_path": target_reduction_path or "",
      },
      "cpp_solve_series_ingest": {
          "supported": cpp_ingest_supported,
          "reason": cpp_ingest_reason,
      },
  }


def write_json(payload: dict[str, Any], path: Path | None) -> None:
  text = json.dumps(payload, indent=2, sort_keys=True) + "\n"
  if path is None:
    print(text, end="")
    return
  path.parent.mkdir(parents=True, exist_ok=True)
  path.write_text(text, encoding="utf-8")


def run_self_check() -> dict[str, Any]:
  with tempfile.TemporaryDirectory(prefix="amflow-state-extractor-self-check-") as raw_root:
    root = Path(raw_root)
    system = root / "box1_amflow" / "1"
    reduction = root / "box1_amflow" / "0"
    (system / "diffeqsetup").mkdir(parents=True)
    (reduction / "results" / "box1").mkdir(parents=True)
    system.mkdir(parents=True, exist_ok=True)
    (system / "masters").write_text("{j[box1, 1, 0], j[box1, 1, 1]}\n", encoding="utf-8")
    (system / "diffeq").write_text("{{(1 - eps)/eta, 0}, {1/(eta + 1), eps}}\n", encoding="utf-8")
    (system / "config").write_text('{"Family" -> box1, "Numeric" -> {s -> 100}}\n', encoding="utf-8")
    (system / "boundary").write_text("{{0, 0}, {}}\n", encoding="utf-8")
    (system / "boundarymi").write_text("{{j[box1, 1, 0] -> {1, 2}}}\n", encoding="utf-8")
    (system / "bpattern").write_text("{{}, {{0, 0}}, {{0, 0}}}\n", encoding="utf-8")
    (system / "border").write_text("{{0, 0}}\n", encoding="utf-8")
    (system / "direction").write_text('"NegIm"\n', encoding="utf-8")
    (system / "epslist").write_text("{1/10, 1/11}\n", encoding="utf-8")
    (reduction / "target").write_text("box1[2, 0]\n", encoding="utf-8")
    (reduction / "results" / "box1" / "masters").write_text("box1[1,0] # 1\n", encoding="utf-8")
    (reduction / "results" / "box1" / "kira_target.m").write_text("{}", encoding="utf-8")

    payload = extract_state(system, reduction_dir=None, benchmark_id="self_check", variable="eta")

  return {
      "schema_version": 1,
      "self_check": "extract_amflow_solve_series_state",
      "state_extracted": payload["kind"] == "amflow_solve_series_state",
      "master_count": len(payload["masters"]),
      "matrix_dimension": len(payload["coefficient_matrices"]["eta"]),
      "singular_points": payload["singular_points"],
      "boundary_state_kind": payload["boundary_state"]["kind"],
      "cpp_ingest_supported": payload["cpp_solve_series_ingest"]["supported"],
      "inferred_reduction_targets": payload["reduction"]["targets"],
  }


def parse_args(argv: list[str]) -> argparse.Namespace:
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument("--system-dir", type=Path)
  parser.add_argument("--reduction-dir", type=Path)
  parser.add_argument("--gauge-asyexp-dir", type=Path)
  parser.add_argument("--finite-diffeq-file", type=Path)
  parser.add_argument("--finite-solution-file", type=Path)
  parser.add_argument("--finite-run-file", type=Path)
  parser.add_argument("--finite-solution-basis-reduction-path", type=Path)
  parser.add_argument("--finite-source-variable", default="s")
  parser.add_argument("--start-location")
  parser.add_argument("--target-location")
  parser.add_argument("--epsilon-samples")
  parser.add_argument("--benchmark-id", default="automatic_loop")
  parser.add_argument("--variable", default="eta")
  parser.add_argument("--out", type=Path)
  parser.add_argument("--self-check", action="store_true")
  return parser.parse_args(argv)


def main(argv: list[str]) -> int:
  args = parse_args(argv)
  try:
    if args.self_check:
      payload = run_self_check()
      write_json(payload, args.out)
      return 0 if (
          payload["state_extracted"]
          and payload["master_count"] == 2
          and payload["matrix_dimension"] == 2
          and payload["cpp_ingest_supported"] is False
          and payload["inferred_reduction_targets"]
      ) else 1

    finite_mode = args.finite_diffeq_file is not None or args.finite_solution_file is not None
    gauge_mode = args.gauge_asyexp_dir is not None
    system_mode = args.system_dir is not None
    expect(sum(1 for mode in (finite_mode, gauge_mode, system_mode) if mode) == 1,
           "choose exactly one extraction mode: --system-dir, --gauge-asyexp-dir, or --finite-diffeq-file/--finite-solution-file")

    if finite_mode:
      expect(args.finite_diffeq_file is not None, "--finite-diffeq-file is required for finite solution extraction")
      expect(args.finite_solution_file is not None, "--finite-solution-file is required for finite solution extraction")
      payload = extract_finite_solution_state(
          args.finite_diffeq_file,
          args.finite_solution_file,
          run_file=args.finite_run_file,
          solution_basis_reduction_path=args.finite_solution_basis_reduction_path,
          benchmark_id=args.benchmark_id,
          variable=args.variable,
          source_variable=args.finite_source_variable,
          start_location=args.start_location,
          target_location=args.target_location,
          epsilon_samples=(
              [
                  normalize_power_of_ten_sample(sample)
                  for sample in split_top_level(args.epsilon_samples)
              ]
              if args.epsilon_samples is not None else None
          ),
      )
    elif gauge_mode:
      payload = extract_gauge_asyexp_state(args.gauge_asyexp_dir,
                                           benchmark_id=args.benchmark_id,
                                           variable=args.variable)
    else:
      payload = extract_state(args.system_dir,
                              reduction_dir=args.reduction_dir,
                              benchmark_id=args.benchmark_id,
                              variable=args.variable)
    write_json(payload, args.out)
    return 0
  except Exception as error:
    write_json(
        {
            "schema_version": 1,
            "kind": "amflow_solve_series_state",
            "extracted": False,
            "failures": [str(error)],
        },
        args.out,
    )
    return 1


if __name__ == "__main__":
  sys.exit(main(sys.argv[1:]))
