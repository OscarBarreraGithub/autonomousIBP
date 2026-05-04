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


def parse_matrix(path: Path) -> list[list[str]]:
  rows = split_top_level(strip_outer_braces(path.read_text()))
  matrix: list[list[str]] = []
  for row in rows:
    cells = split_top_level(strip_outer_braces(row))
    matrix.append([compact_mathematica_text(cell) for cell in cells])
  expect(matrix, f"{path} did not contain a matrix")
  width = len(matrix[0])
  expect(width > 0, f"{path} matrix rows must not be empty")
  for index, row in enumerate(matrix):
    expect(len(row) == width, f"{path} row {index + 1} has inconsistent width")
  expect(len(matrix) == width, f"{path} matrix must be square")
  return matrix


def parse_eps_samples(path: Path) -> list[str]:
  if not path.exists():
    return []
  return split_top_level(strip_outer_braces(path.read_text()))


def raw_file_payload(path: Path) -> dict[str, str]:
  return {"path": str(path), "raw": compact_mathematica_text(path.read_text())}


def infer_reduction_dir(system_dir: Path) -> Path | None:
  try:
    system_id = int(system_dir.name)
  except ValueError:
    return None
  candidate = system_dir.parent / str(system_id - 1)
  return candidate if candidate.exists() else None


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

  return {
      "schema_version": 1,
      "kind": "amflow_solve_series_state",
      "benchmark_id": benchmark_id,
      "source": {
          "system_dir": str(system_dir),
          "reduction_dir": str(reduction_dir) if reduction_dir is not None else "",
      },
      "family": masters[0]["family"],
      "variable": variable,
      "masters": masters,
      "coefficient_matrices": {variable: matrix},
      "amflow_config": {
          "raw": raw_file_payload(system_dir / "config"),
      },
      "boundary_state": {
          "kind": "amflow_eta_infinity_asymptotic_with_subsystem_samples",
          "direction": compact_mathematica_text((system_dir / "direction").read_text()),
          "epsilon_samples": parse_eps_samples(system_dir / "epslist"),
          "files": {path.name: raw_file_payload(path) for path in boundary_files},
      },
      "reduction": {
          "targets": reduction_targets,
          "masters": reduction_masters,
          "target_reduction_path": target_reduction_path or "",
      },
      "cpp_solve_series_ingest": {
          "supported": False,
          "reason": (
              "The retained AMFlow state uses eta-infinity asymptotic boundary data, "
              "subsystem numerical epsilon samples, complex continuation, and a singular "
              "eta -> 0 physical endpoint.  C++ solve-series can load this JSON state "
              "shape, but the physical asymptotic/subsystem-sample boundary evaluator "
              "and complex singular endpoint extraction remain deferred."
          ),
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
      "boundary_state_kind": payload["boundary_state"]["kind"],
      "cpp_ingest_supported": payload["cpp_solve_series_ingest"]["supported"],
      "inferred_reduction_targets": payload["reduction"]["targets"],
  }


def parse_args(argv: list[str]) -> argparse.Namespace:
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument("--system-dir", type=Path)
  parser.add_argument("--reduction-dir", type=Path)
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

    expect(args.system_dir is not None, "--system-dir is required outside --self-check")
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
