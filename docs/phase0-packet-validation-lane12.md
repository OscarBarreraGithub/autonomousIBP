# Lane 12 Phase-0 Packet Validation

Generated for FAILURE-SAFE LANE 12 from the main worktree at `/n/holylabs/schwartz_lab/Lab/obarrera/autoIBP`. This is an honest measurement report: no comparator tolerance was weakened, no benchmark was special-cased as passing, and infrastructure gaps are counted as non-passing coefficients.

## Gate And Harness

- Fresh build gate: `PATH=/n/sw/helmod-rocky8/apps/Core/cmake/4.2.3-fasrc01/bin:$PATH cmake --build build --target amflow-cli` exited `0`. The default shell still lacks `cmake` on `PATH`, so the module CMake path from `build/Makefile` was prepended before rerunning the literal gate command.
- Comparator tolerance: every `compare_cpp_vs_amflow.py` run used `--tolerance-digits 30`.
- Comparator self-check: `compare_cpp_vs_amflow.py --self-check` exited `0`.
- Packet-set comparator self-check: `compare_phase0_packet_set_to_reference.py --self-check` exited `0` with matching packet set, missing packet rejection, duplicate packet rejection, malformed pair rejection, and profile reporting all true.
- Full packet-root comparator was not applicable to the C++ solve-series outputs because they do not publish the packet schema `candidate_root/results/phase0/<benchmark>/result-manifest.json` plus primary run manifests. This report therefore uses the requested fallback: per-benchmark `compare_cpp_vs_amflow.py` aggregation.
- Machine-readable aggregate evidence: `tools/reference-harness/specs/phase0/lane12-packet-validation-aggregate.json` records the same per-benchmark denominators, zero-pass failed probes, and per-coefficient rows; a runtime mirror is at `/tmp/autoibp_orch/exec/lane12_packet_validation/runs/lane12.aggregate.json`.

## Inventory

| Benchmark | Manifest | Reference status | Included in promoted aggregate |
| --- | --- | --- | --- |
| `automatic_loop` | `tools/reference-harness/specs/phase0/automatic_loop.eps2-golden-manifest.json` | promoted eps2 audit manifest | yes |
| `automatic_phasespace` | `tools/reference-harness/specs/phase0/automatic_phasespace.golden-manifest.json` | promoted golden pointer | yes |
| `automatic_vs_manual` | `tools/reference-harness/specs/phase0/automatic_vs_manual.golden-manifest.json` | promoted golden pointer | yes |
| `complex_kinematics` | `tools/reference-harness/specs/phase0/complex_kinematics.golden-manifest.json` | promoted golden pointer | yes |
| `differential_equation_solver` | `tools/reference-harness/specs/phase0/differential_equation_solver.golden-manifest.json` | promoted golden pointer | yes |
| `linear_propagator` | `tools/reference-harness/specs/phase0/linear_propagator.golden-manifest.json` | promoted golden pointer | yes |
| `feynman_prescription` | `tools/reference-harness/specs/phase0/feynman_prescription.golden-manifest.json` | partial-progress pointer only, not promoted reference-captured golden | no |

## Aggregate

- Promoted/reference-captured benchmark aggregate: `54/217` explicit AMFlow coefficients pass at `>=30` digits.
- Including the partial `feynman_prescription` pointer for visibility only: `54/293` explicit AMFlow coefficients pass at `>=30` digits. This is not the qualification denominator because `feynman_prescription` was not promoted to a retained golden before this lane started.
- Benchmarks with missing or partial C++ implementation coverage: `automatic_phasespace`, `automatic_vs_manual`, `complex_kinematics`, `differential_equation_solver`, `linear_propagator`; `feynman_prescription` is also blocked on incomplete reference promotion before C++ parity can be claimed.

## Per-Benchmark Summary

| Benchmark | C++ input/probe | solve exit | compare exit | compare passed | coefficients passed | total explicit AMFlow coefficients | min digits | AMFlow order counts | Failure/gap |
| --- | --- | ---: | ---: | --- | ---: | ---: | --- | --- | --- |
| `automatic_loop` | `tools/reference-harness/specs/phase0/automatic_loop.amflow-state.json` | 0 | 0 | true | 54 | 54 | 41 | eps^-2: 6, eps^-1: 12, eps^0: 12, eps^1: 12, eps^2: 12 | Only benchmark with a repo-local retained AMFlow solve-series state bundle; compared with box1/box2 family aliases. |
| `automatic_phasespace` | `tools/reference-harness/specs/phase0/automatic_phasespace.golden-manifest.json` | 3 | 1 | false | 0 | 11 | n/a | eps^-3: 2, eps^-2: 2, eps^-1: 2, eps^0: 5 | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane12_packet_validation/runs/automatic_phasespace.cpp-result.json' |
| `automatic_vs_manual` | `specs/problem-spec.k0-smoke.yaml` | 2 | 1 | false | 0 | 86 | n/a | eps^-4: 7, eps^-3: 9, eps^-2: 20, eps^-1: 25, eps^0: 25 | /tmp/autoibp_orch/exec/lane12_packet_validation/runs/automatic_vs_manual.cpp-result.json status must be success |
| `complex_kinematics` | `tools/reference-harness/specs/phase0/complex_kinematics.golden-manifest.json` | 3 | 1 | false | 0 | 14 | n/a | eps^-2: 1, eps^-1: 6, eps^0: 7 | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane12_packet_validation/runs/complex_kinematics.cpp-result.json' |
| `differential_equation_solver` | `tools/reference-harness/specs/phase0/differential_equation_solver.golden-manifest.json` | 3 | 1 | false | 0 | 3 | n/a | eps^0: 3 | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane12_packet_validation/runs/differential_equation_solver.cpp-result.json' |
| `linear_propagator` | `tools/reference-harness/specs/phase0/linear_propagator.golden-manifest.json` | 3 | 1 | false | 0 | 49 | n/a | eps^-2: 5, eps^-1: 7, eps^0: 9, eps^1: 7, eps^2: 7, eps^3: 7, eps^4: 7 | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane12_packet_validation/runs/linear_propagator.cpp-result.json' |
| `feynman_prescription` | `tools/reference-harness/specs/phase0/feynman_prescription.golden-manifest.json` | 3 | 1 | false | 0 | 76 | n/a | eps^-2: 12, eps^-1: 16, eps^0: 16, eps^1: 16, eps^2: 16 | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane12_packet_validation/runs/feynman_prescription.cpp-result.json' |

## Per-Coefficient Digit Agreement

Rows with `n/a` have no C++ coefficient-bearing result; those coefficients are counted as non-passing in the aggregate. `999` is the comparator sentinel for exact equality.

| Benchmark | Integral | eps order | real digits | imag digits | pass >=30 | Gap |
| --- | --- | ---: | ---: | ---: | --- | --- |
| `automatic_loop` | `box1[-2,1,1,2]` | -2 | 999 | 999 | yes |  |
| `automatic_loop` | `box1[-2,1,1,2]` | -1 | 41 | 999 | yes |  |
| `automatic_loop` | `box1[-2,1,1,2]` | 0 | 41 | 999 | yes |  |
| `automatic_loop` | `box1[-2,1,1,2]` | 1 | 41 | 999 | yes |  |
| `automatic_loop` | `box1[-2,1,1,2]` | 2 | 41 | 999 | yes |  |
| `automatic_loop` | `box1[0,1,0,1]` | -1 | 999 | 999 | yes |  |
| `automatic_loop` | `box1[0,1,0,1]` | 0 | 41 | 999 | yes |  |
| `automatic_loop` | `box1[0,1,0,1]` | 1 | 41 | 999 | yes |  |
| `automatic_loop` | `box1[0,1,0,1]` | 2 | 41 | 999 | yes |  |
| `automatic_loop` | `box1[1,0,1,0]` | -1 | 999 | 999 | yes |  |
| `automatic_loop` | `box1[1,0,1,0]` | 0 | 41 | 41 | yes |  |
| `automatic_loop` | `box1[1,0,1,0]` | 1 | 41 | 41 | yes |  |
| `automatic_loop` | `box1[1,0,1,0]` | 2 | 41 | 41 | yes |  |
| `automatic_loop` | `box1[1,1,1,1]` | -2 | 999 | 999 | yes |  |
| `automatic_loop` | `box1[1,1,1,1]` | -1 | 41 | 41 | yes |  |
| `automatic_loop` | `box1[1,1,1,1]` | 0 | 41 | 41 | yes |  |
| `automatic_loop` | `box1[1,1,1,1]` | 1 | 41 | 41 | yes |  |
| `automatic_loop` | `box1[1,1,1,1]` | 2 | 41 | 41 | yes |  |
| `automatic_loop` | `box1[1,2,2,1]` | -2 | 58 | 999 | yes |  |
| `automatic_loop` | `box1[1,2,2,1]` | -1 | 41 | 41 | yes |  |
| `automatic_loop` | `box1[1,2,2,1]` | 0 | 42 | 41 | yes |  |
| `automatic_loop` | `box1[1,2,2,1]` | 1 | 41 | 41 | yes |  |
| `automatic_loop` | `box1[1,2,2,1]` | 2 | 41 | 41 | yes |  |
| `automatic_loop` | `box1[2,0,1,0]` | -1 | 999 | 999 | yes |  |
| `automatic_loop` | `box1[2,0,1,0]` | 0 | 41 | 41 | yes |  |
| `automatic_loop` | `box1[2,0,1,0]` | 1 | 41 | 41 | yes |  |
| `automatic_loop` | `box1[2,0,1,0]` | 2 | 41 | 42 | yes |  |
| `automatic_loop` | `box2[0,1,0,1]` | -1 | 999 | 999 | yes |  |
| `automatic_loop` | `box2[0,1,0,1]` | 0 | 41 | 999 | yes |  |
| `automatic_loop` | `box2[0,1,0,1]` | 1 | 41 | 999 | yes |  |
| `automatic_loop` | `box2[0,1,0,1]` | 2 | 41 | 999 | yes |  |
| `automatic_loop` | `box2[1,-1,1,0]` | -1 | 999 | 999 | yes |  |
| `automatic_loop` | `box2[1,-1,1,0]` | 0 | 41 | 41 | yes |  |
| `automatic_loop` | `box2[1,-1,1,0]` | 1 | 41 | 41 | yes |  |
| `automatic_loop` | `box2[1,-1,1,0]` | 2 | 41 | 41 | yes |  |
| `automatic_loop` | `box2[1,0,1,0]` | -1 | 999 | 999 | yes |  |
| `automatic_loop` | `box2[1,0,1,0]` | 0 | 41 | 41 | yes |  |
| `automatic_loop` | `box2[1,0,1,0]` | 1 | 41 | 41 | yes |  |
| `automatic_loop` | `box2[1,0,1,0]` | 2 | 41 | 41 | yes |  |
| `automatic_loop` | `box2[1,1,0,1]` | -2 | 999 | 999 | yes |  |
| `automatic_loop` | `box2[1,1,0,1]` | -1 | 41 | 999 | yes |  |
| `automatic_loop` | `box2[1,1,0,1]` | 0 | 41 | 999 | yes |  |
| `automatic_loop` | `box2[1,1,0,1]` | 1 | 41 | 999 | yes |  |
| `automatic_loop` | `box2[1,1,0,1]` | 2 | 41 | 999 | yes |  |
| `automatic_loop` | `box2[1,1,1,1]` | -2 | 999 | 999 | yes |  |
| `automatic_loop` | `box2[1,1,1,1]` | -1 | 41 | 41 | yes |  |
| `automatic_loop` | `box2[1,1,1,1]` | 0 | 41 | 41 | yes |  |
| `automatic_loop` | `box2[1,1,1,1]` | 1 | 41 | 41 | yes |  |
| `automatic_loop` | `box2[1,1,1,1]` | 2 | 41 | 41 | yes |  |
| `automatic_loop` | `box2[2,1,1,1]` | -2 | 999 | 999 | yes |  |
| `automatic_loop` | `box2[2,1,1,1]` | -1 | 41 | 41 | yes |  |
| `automatic_loop` | `box2[2,1,1,1]` | 0 | 41 | 41 | yes |  |
| `automatic_loop` | `box2[2,1,1,1]` | 1 | 41 | 41 | yes |  |
| `automatic_loop` | `box2[2,1,1,1]` | 2 | 41 | 41 | yes |  |
| `automatic_phasespace` | `phase[1,-1,1,0,1,0,0]` | 0 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `automatic_phasespace` | `phase[1,0,1,0,1,0,0]` | 0 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `automatic_phasespace` | `phase[1,1,1,0,1,0,1]` | 0 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `automatic_phasespace` | `phase[1,1,1,1,1,1,1]` | -3 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `automatic_phasespace` | `phase[1,1,1,1,1,1,1]` | -2 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `automatic_phasespace` | `phase[1,1,1,1,1,1,1]` | -1 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `automatic_phasespace` | `phase[1,1,1,1,1,1,1]` | 0 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `automatic_phasespace` | `phase[1,2,1,1,1,1,1]` | -3 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `automatic_phasespace` | `phase[1,2,1,1,1,1,1]` | -2 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `automatic_phasespace` | `phase[1,2,1,1,1,1,1]` | -1 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `automatic_phasespace` | `phase[1,2,1,1,1,1,1]` | 0 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `automatic_vs_manual` | `tt[-1,1,0,0,1,0,1,0,0]` | -2 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[-1,1,0,0,1,0,1,0,0]` | -1 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[-1,1,0,0,1,0,1,0,0]` | 0 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[-1,1,0,1,1,1,1,0,0]` | -1 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[-1,1,0,1,1,1,1,0,0]` | 0 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[-1,1,1,1,1,0,1,0,0]` | -2 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[-1,1,1,1,1,0,1,0,0]` | -1 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[-1,1,1,1,1,0,1,0,0]` | 0 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[0,0,1,1,0,0,1,0,0]` | -1 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[0,0,1,1,0,0,1,0,0]` | 0 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[0,0,1,1,1,0,1,0,0]` | -2 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[0,0,1,1,1,0,1,0,0]` | -1 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[0,0,1,1,1,0,1,0,0]` | 0 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[0,1,0,0,1,0,1,0,0]` | -2 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[0,1,0,0,1,0,1,0,0]` | -1 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[0,1,0,0,1,0,1,0,0]` | 0 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[0,1,0,1,0,1,1,0,0]` | -2 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[0,1,0,1,0,1,1,0,0]` | -1 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[0,1,0,1,0,1,1,0,0]` | 0 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[0,1,0,1,1,1,1,0,0]` | -1 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[0,1,0,1,1,1,1,0,0]` | 0 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[0,1,1,1,1,0,1,0,0]` | -1 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[0,1,1,1,1,0,1,0,0]` | 0 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,-1,1,0,1,0,1,0,0]` | -2 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,-1,1,0,1,0,1,0,0]` | -1 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,-1,1,0,1,0,1,0,0]` | 0 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,0,0,0,1,0,1,0,0]` | -2 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,0,0,0,1,0,1,0,0]` | -1 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,0,0,0,1,0,1,0,0]` | 0 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,0,1,-1,1,0,1,0,0]` | -2 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,0,1,-1,1,0,1,0,0]` | -1 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,0,1,-1,1,0,1,0,0]` | 0 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,0,1,0,1,0,0,0,0]` | -2 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,0,1,0,1,0,0,0,0]` | -1 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,0,1,0,1,0,0,0,0]` | 0 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,0,1,0,1,0,1,0,0]` | -2 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,0,1,0,1,0,1,0,0]` | -1 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,0,1,0,1,0,1,0,0]` | 0 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,0,1,1,0,1,0,0,0]` | -2 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,0,1,1,0,1,0,0,0]` | -1 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,0,1,1,0,1,0,0,0]` | 0 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,0,1,1,1,1,0,0,0]` | -1 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,0,1,1,1,1,0,0,0]` | 0 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,1,1,-1,1,0,1,0,0]` | -3 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,1,1,-1,1,0,1,0,0]` | -2 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,1,1,-1,1,0,1,0,0]` | -1 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,1,1,-1,1,0,1,0,0]` | 0 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,1,1,0,1,0,1,0,0]` | -3 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,1,1,0,1,0,1,0,0]` | -2 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,1,1,0,1,0,1,0,0]` | -1 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,1,1,0,1,0,1,0,0]` | 0 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,-3,0]` | -4 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,-3,0]` | -3 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,-3,0]` | -2 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,-3,0]` | -1 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,-3,0]` | 0 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,-2,-1]` | -4 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,-2,-1]` | -3 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,-2,-1]` | -2 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,-2,-1]` | -1 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,-2,-1]` | 0 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,-1,-2]` | -4 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,-1,-2]` | -3 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,-1,-2]` | -2 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,-1,-2]` | -1 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,-1,-2]` | 0 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,-1,0]` | -4 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,-1,0]` | -3 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,-1,0]` | -2 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,-1,0]` | -1 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,-1,0]` | 0 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,0,-3]` | -4 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,0,-3]` | -3 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,0,-3]` | -2 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,0,-3]` | -1 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,0,-3]` | 0 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,0,-1]` | -4 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,0,-1]` | -3 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,0,-1]` | -2 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,0,-1]` | -1 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,0,-1]` | 0 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,0,0]` | -4 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,0,0]` | -3 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,0,0]` | -2 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,0,0]` | -1 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,0,0]` | 0 | n/a | n/a | no | failed result JSON; plain ProblemSpec has no embedded solve_series block |
| `complex_kinematics` | `box[0,0,0,1]` | -1 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `complex_kinematics` | `box[0,0,0,1]` | 0 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `complex_kinematics` | `box[0,0,1,1]` | -1 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `complex_kinematics` | `box[0,0,1,1]` | 0 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `complex_kinematics` | `box[0,1,0,1]` | -1 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `complex_kinematics` | `box[0,1,0,1]` | 0 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `complex_kinematics` | `box[1,0,0,1]` | -1 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `complex_kinematics` | `box[1,0,0,1]` | 0 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `complex_kinematics` | `box[1,0,1,0]` | -1 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `complex_kinematics` | `box[1,0,1,0]` | 0 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `complex_kinematics` | `box[1,0,1,1]` | 0 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `complex_kinematics` | `box[1,1,1,1]` | -2 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `complex_kinematics` | `box[1,1,1,1]` | -1 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `complex_kinematics` | `box[1,1,1,1]` | 0 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `differential_equation_solver` | `sunrise[1,1,1,-1,0]` | 0 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `differential_equation_solver` | `sunrise[1,1,1,0,0]` | 0 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `differential_equation_solver` | `sunrise[2,1,1,0,0]` | 0 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `linear_propagator` | `gauge[0,1,1,1,1,-1,0,0,0]` | 0 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `linear_propagator` | `gauge[0,1,1,1,1,0,0,0,0]` | 0 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `linear_propagator` | `gauge[1,1,1,-1,1,0,0,0,0]` | -1 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `linear_propagator` | `gauge[1,1,1,-1,1,0,0,0,0]` | 0 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `linear_propagator` | `gauge[1,1,1,-1,1,0,0,0,0]` | 1 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `linear_propagator` | `gauge[1,1,1,-1,1,0,0,0,0]` | 2 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `linear_propagator` | `gauge[1,1,1,-1,1,0,0,0,0]` | 3 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `linear_propagator` | `gauge[1,1,1,-1,1,0,0,0,0]` | 4 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `linear_propagator` | `gauge[1,1,1,0,1,0,0,0,0]` | -1 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `linear_propagator` | `gauge[1,1,1,0,1,0,0,0,0]` | 0 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `linear_propagator` | `gauge[1,1,1,0,1,0,0,0,0]` | 1 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `linear_propagator` | `gauge[1,1,1,0,1,0,0,0,0]` | 2 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `linear_propagator` | `gauge[1,1,1,0,1,0,0,0,0]` | 3 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `linear_propagator` | `gauge[1,1,1,0,1,0,0,0,0]` | 4 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `linear_propagator` | `gauge[1,1,1,1,1,-1,0,0,0]` | -2 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `linear_propagator` | `gauge[1,1,1,1,1,-1,0,0,0]` | -1 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `linear_propagator` | `gauge[1,1,1,1,1,-1,0,0,0]` | 0 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `linear_propagator` | `gauge[1,1,1,1,1,-1,0,0,0]` | 1 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `linear_propagator` | `gauge[1,1,1,1,1,-1,0,0,0]` | 2 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `linear_propagator` | `gauge[1,1,1,1,1,-1,0,0,0]` | 3 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `linear_propagator` | `gauge[1,1,1,1,1,-1,0,0,0]` | 4 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `linear_propagator` | `gauge[1,1,1,1,1,0,-1,0,0]` | -2 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `linear_propagator` | `gauge[1,1,1,1,1,0,-1,0,0]` | -1 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `linear_propagator` | `gauge[1,1,1,1,1,0,-1,0,0]` | 0 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `linear_propagator` | `gauge[1,1,1,1,1,0,-1,0,0]` | 1 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `linear_propagator` | `gauge[1,1,1,1,1,0,-1,0,0]` | 2 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `linear_propagator` | `gauge[1,1,1,1,1,0,-1,0,0]` | 3 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `linear_propagator` | `gauge[1,1,1,1,1,0,-1,0,0]` | 4 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `linear_propagator` | `gauge[1,1,1,1,1,0,0,-1,0]` | -2 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `linear_propagator` | `gauge[1,1,1,1,1,0,0,-1,0]` | -1 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `linear_propagator` | `gauge[1,1,1,1,1,0,0,-1,0]` | 0 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `linear_propagator` | `gauge[1,1,1,1,1,0,0,-1,0]` | 1 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `linear_propagator` | `gauge[1,1,1,1,1,0,0,-1,0]` | 2 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `linear_propagator` | `gauge[1,1,1,1,1,0,0,-1,0]` | 3 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `linear_propagator` | `gauge[1,1,1,1,1,0,0,-1,0]` | 4 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `linear_propagator` | `gauge[1,1,1,1,1,0,0,0,-1]` | -2 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `linear_propagator` | `gauge[1,1,1,1,1,0,0,0,-1]` | -1 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `linear_propagator` | `gauge[1,1,1,1,1,0,0,0,-1]` | 0 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `linear_propagator` | `gauge[1,1,1,1,1,0,0,0,-1]` | 1 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `linear_propagator` | `gauge[1,1,1,1,1,0,0,0,-1]` | 2 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `linear_propagator` | `gauge[1,1,1,1,1,0,0,0,-1]` | 3 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `linear_propagator` | `gauge[1,1,1,1,1,0,0,0,-1]` | 4 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `linear_propagator` | `gauge[1,1,1,1,1,0,0,0,0]` | -2 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `linear_propagator` | `gauge[1,1,1,1,1,0,0,0,0]` | -1 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `linear_propagator` | `gauge[1,1,1,1,1,0,0,0,0]` | 0 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `linear_propagator` | `gauge[1,1,1,1,1,0,0,0,0]` | 1 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `linear_propagator` | `gauge[1,1,1,1,1,0,0,0,0]` | 2 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `linear_propagator` | `gauge[1,1,1,1,1,0,0,0,0]` | 3 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `linear_propagator` | `gauge[1,1,1,1,1,0,0,0,0]` | 4 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,0,1,1,0,0,1,1,1,1,0,0]` | -2 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,0,1,1,0,0,1,1,1,1,0,0]` | -1 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,0,1,1,0,0,1,1,1,1,0,0]` | 0 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,0,1,1,0,0,1,1,1,1,0,0]` | 1 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,0,1,1,0,0,1,1,1,1,0,0]` | 2 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,0,1,1,1,0,0,1,1,1,0,0]` | -2 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,0,1,1,1,0,0,1,1,1,0,0]` | -1 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,0,1,1,1,0,0,1,1,1,0,0]` | 0 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,0,1,1,1,0,0,1,1,1,0,0]` | 1 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,0,1,1,1,0,0,1,1,1,0,0]` | 2 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,0,1,1,1,0,1,0,1,1,0,0]` | -2 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,0,1,1,1,0,1,0,1,1,0,0]` | -1 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,0,1,1,1,0,1,0,1,1,0,0]` | 0 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,0,1,1,1,0,1,0,1,1,0,0]` | 1 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,0,1,1,1,0,1,0,1,1,0,0]` | 2 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,0,1,1,1,0,1,1,1,1,0,0]` | -1 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,0,1,1,1,0,1,1,1,1,0,0]` | 0 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,0,1,1,1,0,1,1,1,1,0,0]` | 1 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,0,1,1,1,0,1,1,1,1,0,0]` | 2 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,1,-1,1,0,0,1,1,1,1,0,0]` | -2 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,1,-1,1,0,0,1,1,1,1,0,0]` | -1 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,1,-1,1,0,0,1,1,1,1,0,0]` | 0 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,1,-1,1,0,0,1,1,1,1,0,0]` | 1 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,1,-1,1,0,0,1,1,1,1,0,0]` | 2 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,1,-1,1,1,0,0,1,1,1,0,0]` | -2 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,1,-1,1,1,0,0,1,1,1,0,0]` | -1 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,1,-1,1,1,0,0,1,1,1,0,0]` | 0 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,1,-1,1,1,0,0,1,1,1,0,0]` | 1 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,1,-1,1,1,0,0,1,1,1,0,0]` | 2 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,1,-1,1,1,0,1,0,1,1,0,0]` | -2 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,1,-1,1,1,0,1,0,1,1,0,0]` | -1 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,1,-1,1,1,0,1,0,1,1,0,0]` | 0 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,1,-1,1,1,0,1,0,1,1,0,0]` | 1 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,1,-1,1,1,0,1,0,1,1,0,0]` | 2 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,1,-1,1,1,0,1,1,1,1,0,0]` | -1 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,1,-1,1,1,0,1,1,1,1,0,0]` | 0 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,1,-1,1,1,0,1,1,1,1,0,0]` | 1 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,1,-1,1,1,0,1,1,1,1,0,0]` | 2 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,1,0,1,0,0,1,1,1,1,0,0]` | -2 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,1,0,1,0,0,1,1,1,1,0,0]` | -1 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,1,0,1,0,0,1,1,1,1,0,0]` | 0 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,1,0,1,0,0,1,1,1,1,0,0]` | 1 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,1,0,1,0,0,1,1,1,1,0,0]` | 2 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,1,0,1,1,0,0,1,1,1,0,0]` | -2 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,1,0,1,1,0,0,1,1,1,0,0]` | -1 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,1,0,1,1,0,0,1,1,1,0,0]` | 0 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,1,0,1,1,0,0,1,1,1,0,0]` | 1 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,1,0,1,1,0,0,1,1,1,0,0]` | 2 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,1,0,1,1,0,1,0,1,1,0,0]` | -2 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,1,0,1,1,0,1,0,1,1,0,0]` | -1 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,1,0,1,1,0,1,0,1,1,0,0]` | 0 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,1,0,1,1,0,1,0,1,1,0,0]` | 1 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,1,0,1,1,0,1,0,1,1,0,0]` | 2 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,1,0,1,1,0,1,1,1,1,0,0]` | -1 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,1,0,1,1,0,1,1,1,1,0,0]` | 0 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,1,0,1,1,0,1,1,1,1,0,0]` | 1 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,1,0,1,1,0,1,1,1,1,0,0]` | 2 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,1,1,1,0,0,1,1,1,1,0,0]` | -2 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,1,1,1,0,0,1,1,1,1,0,0]` | -1 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,1,1,1,0,0,1,1,1,1,0,0]` | 0 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,1,1,1,0,0,1,1,1,1,0,0]` | 1 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,1,1,1,0,0,1,1,1,1,0,0]` | 2 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,1,1,1,1,0,0,1,1,1,0,0]` | -2 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,1,1,1,1,0,0,1,1,1,0,0]` | -1 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,1,1,1,1,0,0,1,1,1,0,0]` | 0 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,1,1,1,1,0,0,1,1,1,0,0]` | 1 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,1,1,1,1,0,0,1,1,1,0,0]` | 2 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,1,1,1,1,0,1,0,1,1,0,0]` | -2 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,1,1,1,1,0,1,0,1,1,0,0]` | -1 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,1,1,1,1,0,1,0,1,1,0,0]` | 0 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,1,1,1,1,0,1,0,1,1,0,0]` | 1 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,1,1,1,1,0,1,0,1,1,0,0]` | 2 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,1,1,1,1,0,1,1,1,1,0,0]` | -1 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,1,1,1,1,0,1,1,1,1,0,0]` | 0 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,1,1,1,1,0,1,1,1,1,0,0]` | 1 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |
| `feynman_prescription` | `loopxloop[0,1,1,1,1,0,1,1,1,1,0,0]` | 2 | n/a | n/a | no | no cpp result; probe failed with $.kind is required |

## M3/M4 Assessment

- M3 remains narrowly closed only on the already-reviewed mandatory package-family model, prefactor, and reduction-span evidence. This lane does not widen M3 because only `automatic_loop` has a successful C++ solve-series comparison, and the other promoted phase-0 examples lack C++ solve-state/direct-spec ingestion surfaces.
- M4 remains narrowly closed only on the already-reviewed exact-subset robustness, precision, cache, and `SkipReduction` evidence. This lane does not widen M4 and does not close M5, M6, M7, or release readiness.
- TODO: add benchmark-specific C++ solve-series inputs/runtime support for `automatic_phasespace`, `automatic_vs_manual`, `complex_kinematics`, `differential_equation_solver`, and `linear_propagator`; resume and promote `feynman_prescription`; add a candidate packet publisher if future C++ outputs should use `compare_phase0_packet_set_to_reference.py` directly.

## Role Review

- Role A APPROVE: manifest inventory and packet schema review completed; current C++ solve-series outputs do not satisfy the packet-pair candidate schema.
- Role B APPROVE: command discovery completed; `automatic_loop` eps2 path and known `automatic_vs_manual` runtime-support blocker identified.
- Role C APPROVE: ledger milestone assessment reviewed; no M3/M4 expansion or M6/M7 claim is justified.
- Role D APPROVE: aggregate JSON now backs the `54/217` promoted and `54/293` visibility totals, failed probes are zero-pass rows with explicit AMFlow denominators, `automatic_loop` rows match comparator JSON exactly, no tolerance was loosened, no pass evidence was hidden, and no M3/M4/M6/M7/release claim was widened.
