#!/usr/bin/env python3
"""Audit the b64ag first-block endpoint structure around gaugex=0."""

from __future__ import annotations

import argparse
import json
import tempfile
from pathlib import Path
from typing import Any


FIRST_MASTER = "gauge[1,1,1,0,1,0,0,0,0]"
COMPANION_MASTER = "gauge[1,1,1,-1,1,0,0,0,0]"

EXPECTED_DIFFEQ_SNIPPETS = {
    "row0_col0": "(1+20*gaugex-18*eps*gaugex)/(gaugex*(1+2*gaugex))",
    "row0_col1": "(24*(-1+eps)*gaugex)/(1+2*gaugex)",
    "row1_col0": "(5-5*eps+22*gaugex-22*eps*gaugex)/(gaugex^2*(1+2*gaugex))",
    "row1_col1": (
        "(2*(-3+3*eps-14*gaugex+14*eps*gaugex))/(gaugex*(1+2*gaugex))"
    ),
}

EXPECTED_REDUCTION_SNIPPETS = {
    "first_target_row": (
        "j[gauge,1,1,1,0,1,0,0,0,0]->{gaugex^(-1),0,0,0,0,0}"
    ),
    "companion_target_row": "j[gauge,1,1,1,-1,1,0,0,0,0]->{0,1,0,0,0,0}",
}

STRUCTURE_TOKENS = ("Log[", "PolyLog", "HPL", "eta", "gaugex")


def expect(condition: bool, message: str) -> None:
  if not condition:
    raise RuntimeError(message)


def compact(value: str) -> str:
  return "".join(value.split())


def load_json(path: Path) -> dict[str, Any]:
  with path.open("r", encoding="utf-8") as stream:
    payload = json.load(stream)
  expect(isinstance(payload, dict), f"{path} must contain a JSON object")
  return payload


def state_file_raw(state: dict[str, Any], file_name: str) -> str:
  boundary_state = state.get("boundary_state")
  expect(isinstance(boundary_state, dict), "state.boundary_state must be an object")
  files = boundary_state.get("files")
  expect(isinstance(files, dict), "state.boundary_state.files must be an object")
  entry = files.get(file_name)
  expect(isinstance(entry, dict), f"state boundary file {file_name!r} is missing")
  raw = entry.get("raw")
  expect(isinstance(raw, str) and raw, f"state boundary file {file_name!r} has no raw text")
  return raw


def scan_solution_structure(solution_text: str) -> dict[str, Any]:
  present_tokens = [token for token in STRUCTURE_TOKENS if token in solution_text]
  return {
      "contains_log_or_polylog_tokens": any(
          token in present_tokens for token in ("Log[", "PolyLog", "HPL")
      ),
      "contains_endpoint_variable_tokens": any(
          token in present_tokens for token in ("eta", "gaugex")
      ),
      "present_structure_tokens": present_tokens,
      "structure_observation": (
          "The retained AMFlow final reference is already after PickZeroRuleS and "
          "contains Laurent-in-eps numeric coefficients, not an explicit local eta "
          "series with logs or polylogarithms."
      ),
  }


def analyze_state(state: dict[str, Any]) -> dict[str, Any]:
  diffeq_raw = state_file_raw(state, "diffeq")
  reduction_raw = state_file_raw(state, "reduction")
  solution_raw = state_file_raw(state, "solution")
  diffeq_compact = compact(diffeq_raw)
  reduction_compact = compact(reduction_raw)

  diffeq_checks = {
      name: compact(snippet) in diffeq_compact
      for name, snippet in EXPECTED_DIFFEQ_SNIPPETS.items()
  }
  reduction_checks = {
      name: compact(snippet) in reduction_compact
      for name, snippet in EXPECTED_REDUCTION_SNIPPETS.items()
  }
  missing = [
      f"diffeq:{name}" for name, present in diffeq_checks.items() if not present
  ] + [
      f"reduction:{name}" for name, present in reduction_checks.items() if not present
  ]
  expect(not missing, "missing reviewed b64ag first-block snippets: " + ", ".join(missing))

  solution_structure = scan_solution_structure(solution_raw)
  return {
      "schema_version": 1,
      "audit": "b64ag-first-block-structure",
      "masters": [FIRST_MASTER, COMPANION_MASTER],
      "source_contract_checks": {
          "diffeq_first_block_entries_present": all(diffeq_checks.values()),
          "reduction_first_block_rows_present": all(reduction_checks.values()),
          "diffeq_checks": diffeq_checks,
          "reduction_checks": reduction_checks,
      },
      "original_basis_first_block": {
          "matrix_entries": {
              "row0_col0": EXPECTED_DIFFEQ_SNIPPETS["row0_col0"],
              "row0_col1": EXPECTED_DIFFEQ_SNIPPETS["row0_col1"],
              "row1_col0": EXPECTED_DIFFEQ_SNIPPETS["row1_col0"],
              "row1_col1": EXPECTED_DIFFEQ_SNIPPETS["row1_col1"],
          },
          "pole_orders_at_gaugex_zero": {
              "row0_col0": 1,
              "row0_col1": 0,
              "row1_col0": 2,
              "row1_col1": 1,
          },
          "original_basis_first_block_has_double_pole": True,
          "matrix_singular_along_first_block_rows_at_gaugex_zero": True,
          "structural_difference": (
              "The two capped masters are the only retained targets whose reduction rows "
              "read the first two DE masters directly; their original first-block matrix has "
              "a row1/col0 gaugex^-2 entry before the reviewed shear."
          ),
      },
      "reviewed_sheared_local_model": {
          "variables": {
              "y": "j[gauge,1,1,1,0,1,0,0,0,0]/gaugex",
              "w": "j[gauge,1,1,1,-1,1,0,0,0,0] - (5/6)*y",
          },
          "regular_singular_after_shear": True,
          "residue_matrix": [["0", "0"], ["0", "-6+6*eps"]],
          "indicial_roots": ["0", "-6+6*eps"],
          "eps0_root_difference": "-6",
          "integer_shifted_root_at_eps0": True,
          "half_integer_root_at_eps0": False,
          "fixed_epsilon_second_kind_logs_required": False,
          "epsilon_laurent_log_surface_unresolved": True,
          "root_audit": (
              "For the small nonzero epsilon samples used by AMFlow the sheared block is "
              "diagonalizable and log-free, but the root tends to an integer-shifted "
              "-6 at eps=0. Expanding gaugex^(-6+6 eps) in eps generates powers of "
              "log(gaugex), so coefficient-level parity needs an explicit PickZeroRuleS "
              "audit of the sheared Frobenius branch rather than another local-term uplift."
          ),
      },
      "target_reduction_rows": {
          FIRST_MASTER: {
              "row": "{gaugex^(-1),0,0,0,0,0}",
              "direct_first_block_master": True,
          },
          COMPANION_MASTER: {
              "row": "{0,1,0,0,0,0}",
              "direct_first_block_master": True,
          },
      },
      "amflow_reference_structure": solution_structure,
      "m6_promotion_blocked": True,
      "recommended_next_check": (
          "Compare AMFlow SolveAsyExp/PickZeroRuleS fixed-eps coefficients for the "
          "sheared singular branch against the C++ skipped first-block Frobenius branch, "
          "especially the order-six term that can become the finite-part surface after "
          "the gaugex^-1 target row."
      ),
    }


def self_check() -> dict[str, Any]:
  fixture = {
      "boundary_state": {
          "files": {
              "diffeq": {
                  "raw": (
                      "{{j[gauge,1,1,1,0,1,0,0,0,0],"
                      "j[gauge,1,1,1,-1,1,0,0,0,0]}, {gaugex},"
                      "{{{(1+20*gaugex-18*eps*gaugex)/(gaugex*(1+2*gaugex)),"
                      "(24*(-1+eps)*gaugex)/(1+2*gaugex)},"
                      "{(5-5*eps+22*gaugex-22*eps*gaugex)/(gaugex^2*(1+2*gaugex)),"
                      "(2*(-3+3*eps-14*gaugex+14*eps*gaugex))/(gaugex*(1+2*gaugex))}}}}"
                  )
              },
              "reduction": {
                  "raw": (
                      "{{j[gauge,1,1,1,0,1,0,0,0,0],"
                      "j[gauge,1,1,1,-1,1,0,0,0,0]},"
                      "{j[gauge,1,1,1,0,1,0,0,0,0]->{gaugex^(-1),0,0,0,0,0},"
                      "j[gauge,1,1,1,-1,1,0,0,0,0]->{0,1,0,0,0,0}}}"
                  )
              },
              "solution": {
                  "raw": (
                      "{j[gauge, 1, 1, 1, 0, 1, 0, 0, 0, 0] -> "
                      "-0.00277777777777777777777777777777777778`20./eps, "
                      "j[gauge, 1, 1, 1, -1, 1, 0, 0, 0, 0] -> "
                      "-0.00231481481481481481481481481481481481`20./eps}"
                  )
              },
          }
      }
  }
  with tempfile.TemporaryDirectory(prefix="b64ag-first-block-structure-") as directory:
    fixture_path = Path(directory) / "state.json"
    fixture_path.write_text(json.dumps(fixture), encoding="utf-8")
    result = analyze_state(load_json(fixture_path))

  expect(result["source_contract_checks"]["diffeq_first_block_entries_present"],
         "self-check should verify the first-block diffeq entries")
  expect(result["source_contract_checks"]["reduction_first_block_rows_present"],
         "self-check should verify the direct first-block reduction rows")
  expect(result["original_basis_first_block"]["original_basis_first_block_has_double_pole"],
         "self-check should report the original-basis double pole")
  expect(result["reviewed_sheared_local_model"]["regular_singular_after_shear"],
         "self-check should report the reviewed regular-singular shear")
  expect(result["reviewed_sheared_local_model"]["indicial_roots"] == ["0", "-6+6*eps"],
         "self-check should retain the reviewed indicial roots")
  expect(result["reviewed_sheared_local_model"]["integer_shifted_root_at_eps0"],
         "self-check should flag the eps=0 integer-shifted root")
  expect(result["reviewed_sheared_local_model"]["epsilon_laurent_log_surface_unresolved"],
         "self-check should flag the unresolved eps-Laurent log surface")
  expect(not result["amflow_reference_structure"]["contains_log_or_polylog_tokens"],
         "self-check final reference should be post-PickZeroRuleS numeric output")
  expect(result["m6_promotion_blocked"],
         "self-check should not promote M6")
  result["self_check"] = True
  return result


def parse_args() -> argparse.Namespace:
  parser = argparse.ArgumentParser(
      description="Audit the b64ag first-block gaugex=0 matrix and indicial structure."
  )
  parser.add_argument("--state-json", type=Path, help="linear_propagator AMFlow state JSON")
  parser.add_argument("--root", type=Path, help="accepted for reference-harness self-check parity")
  parser.add_argument("--self-check", action="store_true")
  return parser.parse_args()


def main() -> int:
  args = parse_args()
  if args.self_check:
    print(json.dumps(self_check(), indent=2, sort_keys=True))
    return 0
  expect(args.state_json is not None, "--state-json is required unless --self-check is used")
  print(json.dumps(analyze_state(load_json(args.state_json)), indent=2, sort_keys=True))
  return 0


if __name__ == "__main__":
  raise SystemExit(main())
