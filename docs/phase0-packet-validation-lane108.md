# Lane 108 Phase-0 Packet Aggregate Re-Run

Generated `2026-05-05T16:39:59.465679-04:00` from
`/n/holylabs/schwartz_lab/Lab/obarrera/autoIBP` at `HEAD`
`51e5f9209d76d51eecc8d28b358c71cee79b305b`.

## Gate And Methodology

- Fresh CLI rebuild before measurement:
  `PATH=/n/sw/helmod-rocky8/apps/Core/cmake/4.2.3-fasrc01/bin:$PATH cmake --build build --target amflow-cli --parallel 8`.
- Comparator self-check: passed.
- Comparator tolerance: every comparator invocation used
  `--tolerance-digits 30`; no tolerance was loosened.
- C++ solve precision: every `amflow-cli solve-series` invocation used
  `--digits 80`.
- Scope: all phase-0 `*golden-manifest.json` surfaces through
  `automatic_loop.eps24`, plus the non-loop phase-0 goldens. The current
  `automatic_loop.eps25` and `automatic_loop.eps26` manifests are present in
  the repository but outside this lane108 phase0_v4 eps2-through-eps24 scope.
- Counting policy: successful comparisons use `compare_cpp_vs_amflow.py`
  counts. If a solve probe fails, the scoped AMFlow coefficient count at that
  requested epsilon order is counted with zero passed coefficients.

## Aggregate

- All-manifest aggregate: `4559/4559` coefficients pass at `>=30` digits
  across `31` manifest surfaces.
- Delta vs lane 83 `2831/3359`: `+1728` passing coefficients and `+1200`
  total scoped coefficients.
- Delta vs lane 32 `671/671`: `+3888` passing coefficients and `+3888` total
  scoped coefficients.
- Remaining failing coefficients: `0`.

Raw evidence is retained under
`/tmp/autoibp_orch/exec/lane108_phase0_v4/`:

- aggregate JSON:
  `/tmp/autoibp_orch/exec/lane108_phase0_v4/aggregate.json`
- per-surface C++ result and comparator JSON files:
  `/tmp/autoibp_orch/exec/lane108_phase0_v4/runs/`
- orchestration report:
  `/tmp/autoibp_orch/exec/lane108_phase0_v4.md`

## Per-Benchmark Summary

| Manifest surface | Benchmark | Manifest | eps order | solve exit | compare exit | coefficients passed | total coefficients | min digits |
| --- | --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `automatic_loop.eps2` | `automatic_loop` | `tools/reference-harness/specs/phase0/automatic_loop.eps2-golden-manifest.json` | 2 | 0 | 0 | 54 | 54 | 58 |
| `automatic_loop.eps3` | `automatic_loop` | `tools/reference-harness/specs/phase0/automatic_loop.eps3-golden-manifest.json` | 3 | 0 | 0 | 66 | 66 | 58 |
| `automatic_loop.eps4` | `automatic_loop` | `tools/reference-harness/specs/phase0/automatic_loop.eps4-golden-manifest.json` | 4 | 0 | 0 | 78 | 78 | 58 |
| `automatic_loop.eps5` | `automatic_loop` | `tools/reference-harness/specs/phase0/automatic_loop.eps5-golden-manifest.json` | 5 | 0 | 0 | 90 | 90 | 58 |
| `automatic_loop.eps6` | `automatic_loop` | `tools/reference-harness/specs/phase0/automatic_loop.eps6-golden-manifest.json` | 6 | 0 | 0 | 102 | 102 | 58 |
| `automatic_loop.eps7` | `automatic_loop` | `tools/reference-harness/specs/phase0/automatic_loop.eps7-golden-manifest.json` | 7 | 0 | 0 | 114 | 114 | 58 |
| `automatic_loop.eps8` | `automatic_loop` | `tools/reference-harness/specs/phase0/automatic_loop.eps8-golden-manifest.json` | 8 | 0 | 0 | 126 | 126 | 58 |
| `automatic_loop.eps9` | `automatic_loop` | `tools/reference-harness/specs/phase0/automatic_loop.eps9-golden-manifest.json` | 9 | 0 | 0 | 138 | 138 | 58 |
| `automatic_loop.eps10` | `automatic_loop` | `tools/reference-harness/specs/phase0/automatic_loop.eps10-golden-manifest.json` | 10 | 0 | 0 | 150 | 150 | 58 |
| `automatic_loop.eps11` | `automatic_loop` | `tools/reference-harness/specs/phase0/automatic_loop.eps11-golden-manifest.json` | 11 | 0 | 0 | 162 | 162 | 58 |
| `automatic_loop.eps12` | `automatic_loop` | `tools/reference-harness/specs/phase0/automatic_loop.eps12-golden-manifest.json` | 12 | 0 | 0 | 174 | 174 | 57 |
| `automatic_loop.eps13` | `automatic_loop` | `tools/reference-harness/specs/phase0/automatic_loop.eps13-golden-manifest.json` | 13 | 0 | 0 | 186 | 186 | 58 |
| `automatic_loop.eps14` | `automatic_loop` | `tools/reference-harness/specs/phase0/automatic_loop.eps14-golden-manifest.json` | 14 | 0 | 0 | 198 | 198 | 57 |
| `automatic_loop.eps15` | `automatic_loop` | `tools/reference-harness/specs/phase0/automatic_loop.eps15-golden-manifest.json` | 15 | 0 | 0 | 210 | 210 | 58 |
| `automatic_loop.eps16` | `automatic_loop` | `tools/reference-harness/specs/phase0/automatic_loop.eps16-golden-manifest.json` | 16 | 0 | 0 | 222 | 222 | 56 |
| `automatic_loop.eps17` | `automatic_loop` | `tools/reference-harness/specs/phase0/automatic_loop.eps17-golden-manifest.json` | 17 | 0 | 0 | 234 | 234 | 58 |
| `automatic_loop.eps18` | `automatic_loop` | `tools/reference-harness/specs/phase0/automatic_loop.eps18-golden-manifest.json` | 18 | 0 | 0 | 246 | 246 | 55 |
| `automatic_loop.eps19` | `automatic_loop` | `tools/reference-harness/specs/phase0/automatic_loop.eps19-golden-manifest.json` | 19 | 0 | 0 | 258 | 258 | 57 |
| `automatic_loop.eps20` | `automatic_loop` | `tools/reference-harness/specs/phase0/automatic_loop.eps20-golden-manifest.json` | 20 | 0 | 0 | 270 | 270 | 55 |
| `automatic_loop.eps21` | `automatic_loop` | `tools/reference-harness/specs/phase0/automatic_loop.eps21-golden-manifest.json` | 21 | 0 | 0 | 282 | 282 | 56 |
| `automatic_loop.eps22` | `automatic_loop` | `tools/reference-harness/specs/phase0/automatic_loop.eps22-golden-manifest.json` | 22 | 0 | 0 | 294 | 294 | 54 |
| `automatic_loop.eps23` | `automatic_loop` | `tools/reference-harness/specs/phase0/automatic_loop.eps23-golden-manifest.json` | 23 | 0 | 0 | 306 | 306 | 55 |
| `automatic_loop.eps24` | `automatic_loop` | `tools/reference-harness/specs/phase0/automatic_loop.eps24-golden-manifest.json` | 24 | 0 | 0 | 318 | 318 | 64 |
| `automatic_phasespace` | `automatic_phasespace` | `tools/reference-harness/specs/phase0/automatic_phasespace.golden-manifest.json` | 0 | 0 | 0 | 11 | 11 | 39 |
| `automatic_vs_manual` | `automatic_vs_manual` | `tools/reference-harness/specs/phase0/automatic_vs_manual.golden-manifest.json` | 0 | 0 | 0 | 89 | 89 | 36 |
| `complex_kinematics` | `complex_kinematics` | `tools/reference-harness/specs/phase0/complex_kinematics.golden-manifest.json` | 0 | 0 | 0 | 14 | 14 | 59 |
| `complex_kinematics.eps2` | `complex_kinematics` | `tools/reference-harness/specs/phase0/complex_kinematics.eps2-golden-manifest.json` | 2 | 0 | 0 | 28 | 28 | 43 |
| `differential_equation_solver` | `differential_equation_solver` | `tools/reference-harness/specs/phase0/differential_equation_solver.golden-manifest.json` | 0 | 0 | 0 | 3 | 3 | 81 |
| `differential_equation_solver.sol2` | `differential_equation_solver` | `tools/reference-harness/specs/phase0/differential_equation_solver.sol2-golden-manifest.json` | 0 | 0 | 0 | 3 | 3 | 30 |
| `feynman_prescription` | `feynman_prescription` | `tools/reference-harness/specs/phase0/feynman_prescription.golden-manifest.json` | 2 | 0 | 0 | 76 | 76 | 39 |
| `linear_propagator` | `linear_propagator` | `tools/reference-harness/specs/phase0/linear_propagator.golden-manifest.json` | 4 | 0 | 0 | 57 | 57 | 31 |

## Failing Coefficients

None.
