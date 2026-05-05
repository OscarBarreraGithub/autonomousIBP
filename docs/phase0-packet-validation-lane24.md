# Lane 24 Phase-0 Packet Aggregate Re-Run

Generated `2026-05-05T04:28:57.622217-04:00` from `/n/holylabs/schwartz_lab/Lab/obarrera/autoIBP`.

## Gate And Methodology

- Fresh rebuild gate: `PATH=/n/sw/helmod-rocky8/apps/Core/cmake/4.2.3-fasrc01/bin:$PATH cmake --build build --target amflow-cli` exited `0`.
- Comparator tolerance: every comparator invocation used `--tolerance-digits 30`; no tolerance was loosened from lane 12.
- C++ solve precision: every `amflow-cli solve-series` invocation used `--digits 40`.
- Counting policy: successful comparisons use `compare_cpp_vs_amflow.py` counts; failed/missing coefficient-bearing C++ probes count every explicit AMFlow coefficient in the scoped manifest as zero passed.
- Scope: the all-manifest aggregate includes every current `tools/reference-harness/specs/phase0/*golden-manifest.json` surface exactly once. A de-duplicated latest-per-benchmark view is also reported for comparison against the old benchmark-level lane 12 framing.

## Aggregate

- All-manifest aggregate: `400/479` coefficients pass at `>=30` digits.
- Delta vs lane 12 `54/217`: `+346` passing coefficients and `+262` total scoped coefficients.
- Latest-per-benchmark aggregate: `266/342` coefficients pass at `>=30` digits, using `automatic_loop.eps4, automatic_phasespace, automatic_vs_manual, complex_kinematics.eps2, differential_equation_solver.sol2, feynman_prescription, linear_propagator`.
- Latest-per-benchmark delta vs lane 12: `+212` passing coefficients and `+125` total scoped coefficients.

## Manifest Inventory

- `tools/reference-harness/specs/phase0/automatic_loop.eps2-golden-manifest.json`
- `tools/reference-harness/specs/phase0/automatic_loop.eps3-golden-manifest.json`
- `tools/reference-harness/specs/phase0/automatic_loop.eps4-golden-manifest.json`
- `tools/reference-harness/specs/phase0/automatic_phasespace.golden-manifest.json`
- `tools/reference-harness/specs/phase0/automatic_vs_manual.golden-manifest.json`
- `tools/reference-harness/specs/phase0/complex_kinematics.eps2-golden-manifest.json`
- `tools/reference-harness/specs/phase0/complex_kinematics.golden-manifest.json`
- `tools/reference-harness/specs/phase0/differential_equation_solver.golden-manifest.json`
- `tools/reference-harness/specs/phase0/differential_equation_solver.sol2-golden-manifest.json`
- `tools/reference-harness/specs/phase0/feynman_prescription.golden-manifest.json`
- `tools/reference-harness/specs/phase0/linear_propagator.golden-manifest.json`

## Per-Benchmark Summary

| Manifest surface | Benchmark | C++ input/probe | eps order | solve exit | compare exit | compare passed | coefficients passed | total coefficients | min digits | AMFlow order counts | Failure/gap |
| --- | --- | --- | ---: | ---: | ---: | --- | ---: | ---: | --- | --- | --- |
| `automatic_loop.eps2` | `automatic_loop` | `tools/reference-harness/specs/phase0/automatic_loop.amflow-state.json` | 2 | 0 | 0 | true | 54 | 54 | 41 | eps^-2: 6, eps^-1: 12, eps^0: 12, eps^1: 12, eps^2: 12 |  |
| `automatic_loop.eps3` | `automatic_loop` | `tools/reference-harness/specs/phase0/automatic_loop.amflow-state.json` | 3 | 0 | 0 | true | 66 | 66 | 41 | eps^-2: 6, eps^-1: 12, eps^0: 12, eps^1: 12, eps^2: 12, eps^3: 12 |  |
| `automatic_loop.eps4` | `automatic_loop` | `tools/reference-harness/specs/phase0/automatic_loop.amflow-state.json` | 4 | 0 | 0 | true | 78 | 78 | 34 | eps^-2: 6, eps^-1: 12, eps^0: 12, eps^1: 12, eps^2: 12, eps^3: 12, eps^4: 12 |  |
| `automatic_phasespace` | `automatic_phasespace` | `tools/reference-harness/specs/phase0/automatic_phasespace.amflow-state.json` | 0 | 0 | 0 | true | 11 | 11 | 39 | eps^-3: 2, eps^-2: 2, eps^-1: 2, eps^0: 5 |  |
| `automatic_vs_manual` | `automatic_vs_manual` | `tools/reference-harness/specs/phase0/automatic_vs_manual.amflow-state.json` | 0 | 0 | 0 | true | 89 | 89 | 36 | eps^-4: 7, eps^-3: 9, eps^-2: 20, eps^-1: 25, eps^0: 25 |  |
| `complex_kinematics.eps2` | `complex_kinematics` | `tools/reference-harness/specs/phase0/complex_kinematics.amflow-state.json` | 2 | 0 | 0 | true | 28 | 28 | 41 | eps^-2: 1, eps^-1: 6, eps^0: 7, eps^1: 7, eps^2: 7 |  |
| `complex_kinematics` | `complex_kinematics` | `tools/reference-harness/specs/phase0/complex_kinematics.amflow-state.json` | 0 | 0 | 0 | true | 14 | 14 | 41 | eps^-2: 1, eps^-1: 6, eps^0: 7 |  |
| `differential_equation_solver` | `differential_equation_solver` | `tools/reference-harness/specs/phase0/differential_equation_solver.amflow-state.json` | 0 | 0 | 1 | false | 0 | 3 | 0 | eps^0: 3 | sunrise[1,1,1,-1,0] eps^0 has real/imag agreement 0/31, below tolerance 30; sunrise[1,1,1,0,0] eps^0 has real/imag agreement 0/33, below tolerance 30; sunrise[2,1,1,0,0] eps^0 has real/imag agreement 1/32, below tolerance 30 |
| `differential_equation_solver.sol2` | `differential_equation_solver` | `tools/reference-harness/specs/phase0/differential_equation_solver.amflow-state.json` | 0 | 0 | 0 | true | 3 | 3 | 30 | eps^0: 3 |  |
| `feynman_prescription` | `feynman_prescription` | `tools/reference-harness/specs/phase0/feynman_prescription.golden-manifest.json` | 2 | 3 | 1 | false | 0 | 76 | n/a | eps^-2: 12, eps^-1: 16, eps^0: 16, eps^1: 16, eps^2: 16 | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `linear_propagator` | `linear_propagator` | `tools/reference-harness/specs/phase0/linear_propagator.amflow-state.json` | 4 | 0 | 0 | true | 57 | 57 | 31 | eps^-2: 5, eps^-1: 7, eps^0: 9, eps^1: 7, eps^2: 7, eps^3: 7, eps^4: 7 |  |

## Command Evidence

| Manifest surface | solve-series command | compare command |
| --- | --- | --- |
| `automatic_loop.eps2` | `/n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/build/amflow-cli solve-series /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/specs/phase0/automatic_loop.amflow-state.json --eps-order 2 --digits 40 --out /tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/automatic_loop.eps2.cpp-result.json` | `/n/home09/obarrera/.conda/envs/ising_bootstrap/bin/python3 tools/reference-harness/scripts/compare_cpp_vs_amflow.py --cpp-result /tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/automatic_loop.eps2.cpp-result.json --amflow-golden /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/specs/phase0/automatic_loop.eps2-golden-manifest.json --tolerance-digits 30` |
| `automatic_loop.eps3` | `/n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/build/amflow-cli solve-series /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/specs/phase0/automatic_loop.amflow-state.json --eps-order 3 --digits 40 --out /tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/automatic_loop.eps3.cpp-result.json` | `/n/home09/obarrera/.conda/envs/ising_bootstrap/bin/python3 tools/reference-harness/scripts/compare_cpp_vs_amflow.py --cpp-result /tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/automatic_loop.eps3.cpp-result.json --amflow-golden /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/specs/phase0/automatic_loop.eps3-golden-manifest.json --tolerance-digits 30` |
| `automatic_loop.eps4` | `/n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/build/amflow-cli solve-series /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/specs/phase0/automatic_loop.amflow-state.json --eps-order 4 --digits 40 --out /tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/automatic_loop.eps4.cpp-result.json` | `/n/home09/obarrera/.conda/envs/ising_bootstrap/bin/python3 tools/reference-harness/scripts/compare_cpp_vs_amflow.py --cpp-result /tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/automatic_loop.eps4.cpp-result.json --amflow-golden /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/specs/phase0/automatic_loop.eps4-golden-manifest.json --tolerance-digits 30` |
| `automatic_phasespace` | `/n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/build/amflow-cli solve-series /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/specs/phase0/automatic_phasespace.amflow-state.json --eps-order 0 --digits 40 --out /tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/automatic_phasespace.cpp-result.json` | `/n/home09/obarrera/.conda/envs/ising_bootstrap/bin/python3 tools/reference-harness/scripts/compare_cpp_vs_amflow.py --cpp-result /tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/automatic_phasespace.cpp-result.json --amflow-golden /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/specs/phase0/automatic_phasespace.golden-manifest.json --tolerance-digits 30` |
| `automatic_vs_manual` | `/n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/build/amflow-cli solve-series /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/specs/phase0/automatic_vs_manual.amflow-state.json --eps-order 0 --digits 40 --out /tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/automatic_vs_manual.cpp-result.json` | `/n/home09/obarrera/.conda/envs/ising_bootstrap/bin/python3 tools/reference-harness/scripts/compare_cpp_vs_amflow.py --cpp-result /tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/automatic_vs_manual.cpp-result.json --amflow-golden /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/specs/phase0/automatic_vs_manual.golden-manifest.json --tolerance-digits 30` |
| `complex_kinematics.eps2` | `/n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/build/amflow-cli solve-series /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/specs/phase0/complex_kinematics.amflow-state.json --eps-order 2 --digits 40 --out /tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/complex_kinematics.eps2.cpp-result.json` | `/n/home09/obarrera/.conda/envs/ising_bootstrap/bin/python3 tools/reference-harness/scripts/compare_cpp_vs_amflow.py --cpp-result /tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/complex_kinematics.eps2.cpp-result.json --amflow-golden /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/specs/phase0/complex_kinematics.eps2-golden-manifest.json --tolerance-digits 30` |
| `complex_kinematics` | `/n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/build/amflow-cli solve-series /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/specs/phase0/complex_kinematics.amflow-state.json --eps-order 0 --digits 40 --out /tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/complex_kinematics.cpp-result.json` | `/n/home09/obarrera/.conda/envs/ising_bootstrap/bin/python3 tools/reference-harness/scripts/compare_cpp_vs_amflow.py --cpp-result /tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/complex_kinematics.cpp-result.json --amflow-golden /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/specs/phase0/complex_kinematics.golden-manifest.json --tolerance-digits 30` |
| `differential_equation_solver` | `/n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/build/amflow-cli solve-series /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/specs/phase0/differential_equation_solver.amflow-state.json --eps-order 0 --digits 40 --out /tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/differential_equation_solver.cpp-result.json` | `/n/home09/obarrera/.conda/envs/ising_bootstrap/bin/python3 tools/reference-harness/scripts/compare_cpp_vs_amflow.py --cpp-result /tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/differential_equation_solver.cpp-result.json --amflow-golden /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/specs/phase0/differential_equation_solver.golden-manifest.json --tolerance-digits 30` |
| `differential_equation_solver.sol2` | `/n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/build/amflow-cli solve-series /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/specs/phase0/differential_equation_solver.amflow-state.json --eps-order 0 --digits 40 --out /tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/differential_equation_solver.sol2.cpp-result.json` | `/n/home09/obarrera/.conda/envs/ising_bootstrap/bin/python3 tools/reference-harness/scripts/compare_cpp_vs_amflow.py --cpp-result /tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/differential_equation_solver.sol2.cpp-result.json --amflow-golden /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/specs/phase0/differential_equation_solver.sol2-golden-manifest.json --tolerance-digits 30` |
| `feynman_prescription` | `/n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/build/amflow-cli solve-series /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/specs/phase0/feynman_prescription.golden-manifest.json --eps-order 2 --digits 40 --out /tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json` | `/n/home09/obarrera/.conda/envs/ising_bootstrap/bin/python3 tools/reference-harness/scripts/compare_cpp_vs_amflow.py --cpp-result /tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json --amflow-golden /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/specs/phase0/feynman_prescription.golden-manifest.json --tolerance-digits 30` |
| `linear_propagator` | `/n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/build/amflow-cli solve-series /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/specs/phase0/linear_propagator.amflow-state.json --eps-order 4 --digits 40 --out /tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/linear_propagator.cpp-result.json` | `/n/home09/obarrera/.conda/envs/ising_bootstrap/bin/python3 tools/reference-harness/scripts/compare_cpp_vs_amflow.py --cpp-result /tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/linear_propagator.cpp-result.json --amflow-golden /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/specs/phase0/linear_propagator.golden-manifest.json --tolerance-digits 30` |

## Per-Coefficient Digit Agreement

Rows with `n/a` digit agreement had no coefficient-bearing C++ comparison result and are counted as non-passing. `999` is the comparator sentinel for exact equality.

| Manifest surface | Integral | eps order | real digits | imag digits | pass >=30 | Gap |
| --- | --- | ---: | ---: | ---: | --- | --- |
| `automatic_loop.eps2` | `box1[-2,1,1,2]` | -2 | 999 | 999 | yes |  |
| `automatic_loop.eps2` | `box1[-2,1,1,2]` | -1 | 41 | 999 | yes |  |
| `automatic_loop.eps2` | `box1[-2,1,1,2]` | 0 | 41 | 999 | yes |  |
| `automatic_loop.eps2` | `box1[-2,1,1,2]` | 1 | 41 | 999 | yes |  |
| `automatic_loop.eps2` | `box1[-2,1,1,2]` | 2 | 41 | 999 | yes |  |
| `automatic_loop.eps2` | `box1[0,1,0,1]` | -1 | 999 | 999 | yes |  |
| `automatic_loop.eps2` | `box1[0,1,0,1]` | 0 | 41 | 999 | yes |  |
| `automatic_loop.eps2` | `box1[0,1,0,1]` | 1 | 41 | 999 | yes |  |
| `automatic_loop.eps2` | `box1[0,1,0,1]` | 2 | 41 | 999 | yes |  |
| `automatic_loop.eps2` | `box1[1,0,1,0]` | -1 | 999 | 999 | yes |  |
| `automatic_loop.eps2` | `box1[1,0,1,0]` | 0 | 41 | 41 | yes |  |
| `automatic_loop.eps2` | `box1[1,0,1,0]` | 1 | 41 | 41 | yes |  |
| `automatic_loop.eps2` | `box1[1,0,1,0]` | 2 | 41 | 41 | yes |  |
| `automatic_loop.eps2` | `box1[1,1,1,1]` | -2 | 999 | 999 | yes |  |
| `automatic_loop.eps2` | `box1[1,1,1,1]` | -1 | 41 | 41 | yes |  |
| `automatic_loop.eps2` | `box1[1,1,1,1]` | 0 | 41 | 41 | yes |  |
| `automatic_loop.eps2` | `box1[1,1,1,1]` | 1 | 41 | 41 | yes |  |
| `automatic_loop.eps2` | `box1[1,1,1,1]` | 2 | 41 | 41 | yes |  |
| `automatic_loop.eps2` | `box1[1,2,2,1]` | -2 | 58 | 999 | yes |  |
| `automatic_loop.eps2` | `box1[1,2,2,1]` | -1 | 41 | 41 | yes |  |
| `automatic_loop.eps2` | `box1[1,2,2,1]` | 0 | 42 | 41 | yes |  |
| `automatic_loop.eps2` | `box1[1,2,2,1]` | 1 | 41 | 41 | yes |  |
| `automatic_loop.eps2` | `box1[1,2,2,1]` | 2 | 41 | 41 | yes |  |
| `automatic_loop.eps2` | `box1[2,0,1,0]` | -1 | 999 | 999 | yes |  |
| `automatic_loop.eps2` | `box1[2,0,1,0]` | 0 | 41 | 41 | yes |  |
| `automatic_loop.eps2` | `box1[2,0,1,0]` | 1 | 41 | 41 | yes |  |
| `automatic_loop.eps2` | `box1[2,0,1,0]` | 2 | 41 | 42 | yes |  |
| `automatic_loop.eps2` | `box2[0,1,0,1]` | -1 | 999 | 999 | yes |  |
| `automatic_loop.eps2` | `box2[0,1,0,1]` | 0 | 41 | 999 | yes |  |
| `automatic_loop.eps2` | `box2[0,1,0,1]` | 1 | 41 | 999 | yes |  |
| `automatic_loop.eps2` | `box2[0,1,0,1]` | 2 | 41 | 999 | yes |  |
| `automatic_loop.eps2` | `box2[1,-1,1,0]` | -1 | 999 | 999 | yes |  |
| `automatic_loop.eps2` | `box2[1,-1,1,0]` | 0 | 41 | 41 | yes |  |
| `automatic_loop.eps2` | `box2[1,-1,1,0]` | 1 | 41 | 41 | yes |  |
| `automatic_loop.eps2` | `box2[1,-1,1,0]` | 2 | 41 | 41 | yes |  |
| `automatic_loop.eps2` | `box2[1,0,1,0]` | -1 | 999 | 999 | yes |  |
| `automatic_loop.eps2` | `box2[1,0,1,0]` | 0 | 41 | 41 | yes |  |
| `automatic_loop.eps2` | `box2[1,0,1,0]` | 1 | 41 | 41 | yes |  |
| `automatic_loop.eps2` | `box2[1,0,1,0]` | 2 | 41 | 41 | yes |  |
| `automatic_loop.eps2` | `box2[1,1,0,1]` | -2 | 999 | 999 | yes |  |
| `automatic_loop.eps2` | `box2[1,1,0,1]` | -1 | 41 | 999 | yes |  |
| `automatic_loop.eps2` | `box2[1,1,0,1]` | 0 | 41 | 999 | yes |  |
| `automatic_loop.eps2` | `box2[1,1,0,1]` | 1 | 41 | 999 | yes |  |
| `automatic_loop.eps2` | `box2[1,1,0,1]` | 2 | 41 | 999 | yes |  |
| `automatic_loop.eps2` | `box2[1,1,1,1]` | -2 | 999 | 999 | yes |  |
| `automatic_loop.eps2` | `box2[1,1,1,1]` | -1 | 41 | 41 | yes |  |
| `automatic_loop.eps2` | `box2[1,1,1,1]` | 0 | 41 | 41 | yes |  |
| `automatic_loop.eps2` | `box2[1,1,1,1]` | 1 | 41 | 41 | yes |  |
| `automatic_loop.eps2` | `box2[1,1,1,1]` | 2 | 41 | 41 | yes |  |
| `automatic_loop.eps2` | `box2[2,1,1,1]` | -2 | 999 | 999 | yes |  |
| `automatic_loop.eps2` | `box2[2,1,1,1]` | -1 | 41 | 41 | yes |  |
| `automatic_loop.eps2` | `box2[2,1,1,1]` | 0 | 41 | 41 | yes |  |
| `automatic_loop.eps2` | `box2[2,1,1,1]` | 1 | 41 | 41 | yes |  |
| `automatic_loop.eps2` | `box2[2,1,1,1]` | 2 | 41 | 41 | yes |  |
| `automatic_loop.eps3` | `box1[-2,1,1,2]` | -2 | 999 | 999 | yes |  |
| `automatic_loop.eps3` | `box1[-2,1,1,2]` | -1 | 41 | 999 | yes |  |
| `automatic_loop.eps3` | `box1[-2,1,1,2]` | 0 | 41 | 999 | yes |  |
| `automatic_loop.eps3` | `box1[-2,1,1,2]` | 1 | 41 | 999 | yes |  |
| `automatic_loop.eps3` | `box1[-2,1,1,2]` | 2 | 41 | 999 | yes |  |
| `automatic_loop.eps3` | `box1[-2,1,1,2]` | 3 | 41 | 999 | yes |  |
| `automatic_loop.eps3` | `box1[0,1,0,1]` | -1 | 999 | 999 | yes |  |
| `automatic_loop.eps3` | `box1[0,1,0,1]` | 0 | 41 | 999 | yes |  |
| `automatic_loop.eps3` | `box1[0,1,0,1]` | 1 | 41 | 999 | yes |  |
| `automatic_loop.eps3` | `box1[0,1,0,1]` | 2 | 41 | 999 | yes |  |
| `automatic_loop.eps3` | `box1[0,1,0,1]` | 3 | 41 | 999 | yes |  |
| `automatic_loop.eps3` | `box1[1,0,1,0]` | -1 | 999 | 999 | yes |  |
| `automatic_loop.eps3` | `box1[1,0,1,0]` | 0 | 41 | 41 | yes |  |
| `automatic_loop.eps3` | `box1[1,0,1,0]` | 1 | 41 | 41 | yes |  |
| `automatic_loop.eps3` | `box1[1,0,1,0]` | 2 | 41 | 41 | yes |  |
| `automatic_loop.eps3` | `box1[1,0,1,0]` | 3 | 42 | 41 | yes |  |
| `automatic_loop.eps3` | `box1[1,1,1,1]` | -2 | 999 | 999 | yes |  |
| `automatic_loop.eps3` | `box1[1,1,1,1]` | -1 | 41 | 41 | yes |  |
| `automatic_loop.eps3` | `box1[1,1,1,1]` | 0 | 41 | 41 | yes |  |
| `automatic_loop.eps3` | `box1[1,1,1,1]` | 1 | 41 | 41 | yes |  |
| `automatic_loop.eps3` | `box1[1,1,1,1]` | 2 | 41 | 41 | yes |  |
| `automatic_loop.eps3` | `box1[1,1,1,1]` | 3 | 41 | 41 | yes |  |
| `automatic_loop.eps3` | `box1[1,2,2,1]` | -2 | 58 | 999 | yes |  |
| `automatic_loop.eps3` | `box1[1,2,2,1]` | -1 | 41 | 41 | yes |  |
| `automatic_loop.eps3` | `box1[1,2,2,1]` | 0 | 42 | 41 | yes |  |
| `automatic_loop.eps3` | `box1[1,2,2,1]` | 1 | 41 | 41 | yes |  |
| `automatic_loop.eps3` | `box1[1,2,2,1]` | 2 | 41 | 41 | yes |  |
| `automatic_loop.eps3` | `box1[1,2,2,1]` | 3 | 42 | 41 | yes |  |
| `automatic_loop.eps3` | `box1[2,0,1,0]` | -1 | 999 | 999 | yes |  |
| `automatic_loop.eps3` | `box1[2,0,1,0]` | 0 | 41 | 41 | yes |  |
| `automatic_loop.eps3` | `box1[2,0,1,0]` | 1 | 41 | 41 | yes |  |
| `automatic_loop.eps3` | `box1[2,0,1,0]` | 2 | 41 | 42 | yes |  |
| `automatic_loop.eps3` | `box1[2,0,1,0]` | 3 | 41 | 41 | yes |  |
| `automatic_loop.eps3` | `box2[0,1,0,1]` | -1 | 999 | 999 | yes |  |
| `automatic_loop.eps3` | `box2[0,1,0,1]` | 0 | 41 | 999 | yes |  |
| `automatic_loop.eps3` | `box2[0,1,0,1]` | 1 | 41 | 999 | yes |  |
| `automatic_loop.eps3` | `box2[0,1,0,1]` | 2 | 41 | 999 | yes |  |
| `automatic_loop.eps3` | `box2[0,1,0,1]` | 3 | 41 | 999 | yes |  |
| `automatic_loop.eps3` | `box2[1,-1,1,0]` | -1 | 999 | 999 | yes |  |
| `automatic_loop.eps3` | `box2[1,-1,1,0]` | 0 | 41 | 41 | yes |  |
| `automatic_loop.eps3` | `box2[1,-1,1,0]` | 1 | 41 | 41 | yes |  |
| `automatic_loop.eps3` | `box2[1,-1,1,0]` | 2 | 41 | 41 | yes |  |
| `automatic_loop.eps3` | `box2[1,-1,1,0]` | 3 | 41 | 41 | yes |  |
| `automatic_loop.eps3` | `box2[1,0,1,0]` | -1 | 999 | 999 | yes |  |
| `automatic_loop.eps3` | `box2[1,0,1,0]` | 0 | 41 | 41 | yes |  |
| `automatic_loop.eps3` | `box2[1,0,1,0]` | 1 | 41 | 41 | yes |  |
| `automatic_loop.eps3` | `box2[1,0,1,0]` | 2 | 41 | 41 | yes |  |
| `automatic_loop.eps3` | `box2[1,0,1,0]` | 3 | 42 | 41 | yes |  |
| `automatic_loop.eps3` | `box2[1,1,0,1]` | -2 | 999 | 999 | yes |  |
| `automatic_loop.eps3` | `box2[1,1,0,1]` | -1 | 41 | 999 | yes |  |
| `automatic_loop.eps3` | `box2[1,1,0,1]` | 0 | 41 | 999 | yes |  |
| `automatic_loop.eps3` | `box2[1,1,0,1]` | 1 | 41 | 999 | yes |  |
| `automatic_loop.eps3` | `box2[1,1,0,1]` | 2 | 41 | 999 | yes |  |
| `automatic_loop.eps3` | `box2[1,1,0,1]` | 3 | 42 | 999 | yes |  |
| `automatic_loop.eps3` | `box2[1,1,1,1]` | -2 | 999 | 999 | yes |  |
| `automatic_loop.eps3` | `box2[1,1,1,1]` | -1 | 41 | 41 | yes |  |
| `automatic_loop.eps3` | `box2[1,1,1,1]` | 0 | 41 | 41 | yes |  |
| `automatic_loop.eps3` | `box2[1,1,1,1]` | 1 | 41 | 41 | yes |  |
| `automatic_loop.eps3` | `box2[1,1,1,1]` | 2 | 41 | 41 | yes |  |
| `automatic_loop.eps3` | `box2[1,1,1,1]` | 3 | 41 | 41 | yes |  |
| `automatic_loop.eps3` | `box2[2,1,1,1]` | -2 | 999 | 999 | yes |  |
| `automatic_loop.eps3` | `box2[2,1,1,1]` | -1 | 41 | 41 | yes |  |
| `automatic_loop.eps3` | `box2[2,1,1,1]` | 0 | 41 | 41 | yes |  |
| `automatic_loop.eps3` | `box2[2,1,1,1]` | 1 | 41 | 41 | yes |  |
| `automatic_loop.eps3` | `box2[2,1,1,1]` | 2 | 41 | 41 | yes |  |
| `automatic_loop.eps3` | `box2[2,1,1,1]` | 3 | 42 | 41 | yes |  |
| `automatic_loop.eps4` | `box1[-2,1,1,2]` | -2 | 999 | 999 | yes |  |
| `automatic_loop.eps4` | `box1[-2,1,1,2]` | -1 | 41 | 999 | yes |  |
| `automatic_loop.eps4` | `box1[-2,1,1,2]` | 0 | 41 | 999 | yes |  |
| `automatic_loop.eps4` | `box1[-2,1,1,2]` | 1 | 41 | 999 | yes |  |
| `automatic_loop.eps4` | `box1[-2,1,1,2]` | 2 | 41 | 999 | yes |  |
| `automatic_loop.eps4` | `box1[-2,1,1,2]` | 3 | 41 | 999 | yes |  |
| `automatic_loop.eps4` | `box1[-2,1,1,2]` | 4 | 34 | 999 | yes |  |
| `automatic_loop.eps4` | `box1[0,1,0,1]` | -1 | 999 | 999 | yes |  |
| `automatic_loop.eps4` | `box1[0,1,0,1]` | 0 | 41 | 999 | yes |  |
| `automatic_loop.eps4` | `box1[0,1,0,1]` | 1 | 41 | 999 | yes |  |
| `automatic_loop.eps4` | `box1[0,1,0,1]` | 2 | 41 | 999 | yes |  |
| `automatic_loop.eps4` | `box1[0,1,0,1]` | 3 | 41 | 999 | yes |  |
| `automatic_loop.eps4` | `box1[0,1,0,1]` | 4 | 41 | 999 | yes |  |
| `automatic_loop.eps4` | `box1[1,0,1,0]` | -1 | 999 | 999 | yes |  |
| `automatic_loop.eps4` | `box1[1,0,1,0]` | 0 | 41 | 41 | yes |  |
| `automatic_loop.eps4` | `box1[1,0,1,0]` | 1 | 41 | 41 | yes |  |
| `automatic_loop.eps4` | `box1[1,0,1,0]` | 2 | 41 | 41 | yes |  |
| `automatic_loop.eps4` | `box1[1,0,1,0]` | 3 | 42 | 41 | yes |  |
| `automatic_loop.eps4` | `box1[1,0,1,0]` | 4 | 41 | 41 | yes |  |
| `automatic_loop.eps4` | `box1[1,1,1,1]` | -2 | 999 | 999 | yes |  |
| `automatic_loop.eps4` | `box1[1,1,1,1]` | -1 | 41 | 41 | yes |  |
| `automatic_loop.eps4` | `box1[1,1,1,1]` | 0 | 41 | 41 | yes |  |
| `automatic_loop.eps4` | `box1[1,1,1,1]` | 1 | 41 | 41 | yes |  |
| `automatic_loop.eps4` | `box1[1,1,1,1]` | 2 | 41 | 41 | yes |  |
| `automatic_loop.eps4` | `box1[1,1,1,1]` | 3 | 41 | 41 | yes |  |
| `automatic_loop.eps4` | `box1[1,1,1,1]` | 4 | 41 | 41 | yes |  |
| `automatic_loop.eps4` | `box1[1,2,2,1]` | -2 | 58 | 999 | yes |  |
| `automatic_loop.eps4` | `box1[1,2,2,1]` | -1 | 41 | 41 | yes |  |
| `automatic_loop.eps4` | `box1[1,2,2,1]` | 0 | 42 | 41 | yes |  |
| `automatic_loop.eps4` | `box1[1,2,2,1]` | 1 | 41 | 41 | yes |  |
| `automatic_loop.eps4` | `box1[1,2,2,1]` | 2 | 41 | 41 | yes |  |
| `automatic_loop.eps4` | `box1[1,2,2,1]` | 3 | 42 | 41 | yes |  |
| `automatic_loop.eps4` | `box1[1,2,2,1]` | 4 | 41 | 41 | yes |  |
| `automatic_loop.eps4` | `box1[2,0,1,0]` | -1 | 999 | 999 | yes |  |
| `automatic_loop.eps4` | `box1[2,0,1,0]` | 0 | 41 | 41 | yes |  |
| `automatic_loop.eps4` | `box1[2,0,1,0]` | 1 | 41 | 41 | yes |  |
| `automatic_loop.eps4` | `box1[2,0,1,0]` | 2 | 41 | 42 | yes |  |
| `automatic_loop.eps4` | `box1[2,0,1,0]` | 3 | 41 | 41 | yes |  |
| `automatic_loop.eps4` | `box1[2,0,1,0]` | 4 | 41 | 41 | yes |  |
| `automatic_loop.eps4` | `box2[0,1,0,1]` | -1 | 999 | 999 | yes |  |
| `automatic_loop.eps4` | `box2[0,1,0,1]` | 0 | 41 | 999 | yes |  |
| `automatic_loop.eps4` | `box2[0,1,0,1]` | 1 | 41 | 999 | yes |  |
| `automatic_loop.eps4` | `box2[0,1,0,1]` | 2 | 41 | 999 | yes |  |
| `automatic_loop.eps4` | `box2[0,1,0,1]` | 3 | 41 | 999 | yes |  |
| `automatic_loop.eps4` | `box2[0,1,0,1]` | 4 | 41 | 999 | yes |  |
| `automatic_loop.eps4` | `box2[1,-1,1,0]` | -1 | 999 | 999 | yes |  |
| `automatic_loop.eps4` | `box2[1,-1,1,0]` | 0 | 41 | 41 | yes |  |
| `automatic_loop.eps4` | `box2[1,-1,1,0]` | 1 | 41 | 41 | yes |  |
| `automatic_loop.eps4` | `box2[1,-1,1,0]` | 2 | 41 | 41 | yes |  |
| `automatic_loop.eps4` | `box2[1,-1,1,0]` | 3 | 41 | 41 | yes |  |
| `automatic_loop.eps4` | `box2[1,-1,1,0]` | 4 | 41 | 41 | yes |  |
| `automatic_loop.eps4` | `box2[1,0,1,0]` | -1 | 999 | 999 | yes |  |
| `automatic_loop.eps4` | `box2[1,0,1,0]` | 0 | 41 | 41 | yes |  |
| `automatic_loop.eps4` | `box2[1,0,1,0]` | 1 | 41 | 41 | yes |  |
| `automatic_loop.eps4` | `box2[1,0,1,0]` | 2 | 41 | 41 | yes |  |
| `automatic_loop.eps4` | `box2[1,0,1,0]` | 3 | 42 | 41 | yes |  |
| `automatic_loop.eps4` | `box2[1,0,1,0]` | 4 | 41 | 41 | yes |  |
| `automatic_loop.eps4` | `box2[1,1,0,1]` | -2 | 999 | 999 | yes |  |
| `automatic_loop.eps4` | `box2[1,1,0,1]` | -1 | 41 | 999 | yes |  |
| `automatic_loop.eps4` | `box2[1,1,0,1]` | 0 | 41 | 999 | yes |  |
| `automatic_loop.eps4` | `box2[1,1,0,1]` | 1 | 41 | 999 | yes |  |
| `automatic_loop.eps4` | `box2[1,1,0,1]` | 2 | 41 | 999 | yes |  |
| `automatic_loop.eps4` | `box2[1,1,0,1]` | 3 | 42 | 999 | yes |  |
| `automatic_loop.eps4` | `box2[1,1,0,1]` | 4 | 38 | 999 | yes |  |
| `automatic_loop.eps4` | `box2[1,1,1,1]` | -2 | 999 | 999 | yes |  |
| `automatic_loop.eps4` | `box2[1,1,1,1]` | -1 | 41 | 41 | yes |  |
| `automatic_loop.eps4` | `box2[1,1,1,1]` | 0 | 41 | 41 | yes |  |
| `automatic_loop.eps4` | `box2[1,1,1,1]` | 1 | 41 | 41 | yes |  |
| `automatic_loop.eps4` | `box2[1,1,1,1]` | 2 | 41 | 41 | yes |  |
| `automatic_loop.eps4` | `box2[1,1,1,1]` | 3 | 41 | 41 | yes |  |
| `automatic_loop.eps4` | `box2[1,1,1,1]` | 4 | 41 | 41 | yes |  |
| `automatic_loop.eps4` | `box2[2,1,1,1]` | -2 | 999 | 999 | yes |  |
| `automatic_loop.eps4` | `box2[2,1,1,1]` | -1 | 41 | 41 | yes |  |
| `automatic_loop.eps4` | `box2[2,1,1,1]` | 0 | 41 | 41 | yes |  |
| `automatic_loop.eps4` | `box2[2,1,1,1]` | 1 | 41 | 41 | yes |  |
| `automatic_loop.eps4` | `box2[2,1,1,1]` | 2 | 41 | 41 | yes |  |
| `automatic_loop.eps4` | `box2[2,1,1,1]` | 3 | 42 | 41 | yes |  |
| `automatic_loop.eps4` | `box2[2,1,1,1]` | 4 | 42 | 41 | yes |  |
| `automatic_phasespace` | `phase[1,-1,1,0,1,0,0]` | 0 | 39 | 999 | yes |  |
| `automatic_phasespace` | `phase[1,0,1,0,1,0,0]` | 0 | 39 | 999 | yes |  |
| `automatic_phasespace` | `phase[1,1,1,0,1,0,1]` | 0 | 39 | 999 | yes |  |
| `automatic_phasespace` | `phase[1,1,1,1,1,1,1]` | -3 | 41 | 999 | yes |  |
| `automatic_phasespace` | `phase[1,1,1,1,1,1,1]` | -2 | 39 | 999 | yes |  |
| `automatic_phasespace` | `phase[1,1,1,1,1,1,1]` | -1 | 39 | 999 | yes |  |
| `automatic_phasespace` | `phase[1,1,1,1,1,1,1]` | 0 | 40 | 999 | yes |  |
| `automatic_phasespace` | `phase[1,2,1,1,1,1,1]` | -3 | 41 | 999 | yes |  |
| `automatic_phasespace` | `phase[1,2,1,1,1,1,1]` | -2 | 41 | 999 | yes |  |
| `automatic_phasespace` | `phase[1,2,1,1,1,1,1]` | -1 | 41 | 999 | yes |  |
| `automatic_phasespace` | `phase[1,2,1,1,1,1,1]` | 0 | 40 | 999 | yes |  |
| `automatic_vs_manual` | `tt[-1,1,0,0,1,0,1,0,0]` | -2 | 39 | 999 | yes |  |
| `automatic_vs_manual` | `tt[-1,1,0,0,1,0,1,0,0]` | -1 | 39 | 999 | yes |  |
| `automatic_vs_manual` | `tt[-1,1,0,0,1,0,1,0,0]` | 0 | 39 | 999 | yes |  |
| `automatic_vs_manual` | `tt[-1,1,0,1,1,1,1,0,0]` | -1 | 40 | 39 | yes |  |
| `automatic_vs_manual` | `tt[-1,1,0,1,1,1,1,0,0]` | 0 | 39 | 39 | yes |  |
| `automatic_vs_manual` | `tt[-1,1,1,1,1,0,1,0,0]` | -2 | 999 | 999 | yes |  |
| `automatic_vs_manual` | `tt[-1,1,1,1,1,0,1,0,0]` | -1 | 39 | 40 | yes |  |
| `automatic_vs_manual` | `tt[-1,1,1,1,1,0,1,0,0]` | 0 | 39 | 39 | yes |  |
| `automatic_vs_manual` | `tt[0,0,1,1,0,0,1,0,0]` | -1 | 999 | 999 | yes |  |
| `automatic_vs_manual` | `tt[0,0,1,1,0,0,1,0,0]` | 0 | 39 | 39 | yes |  |
| `automatic_vs_manual` | `tt[0,0,1,1,1,0,1,0,0]` | -2 | 40 | 999 | yes |  |
| `automatic_vs_manual` | `tt[0,0,1,1,1,0,1,0,0]` | -1 | 39 | 999 | yes |  |
| `automatic_vs_manual` | `tt[0,0,1,1,1,0,1,0,0]` | 0 | 39 | 39 | yes |  |
| `automatic_vs_manual` | `tt[0,1,0,0,1,0,1,0,0]` | -2 | 40 | 999 | yes |  |
| `automatic_vs_manual` | `tt[0,1,0,0,1,0,1,0,0]` | -1 | 39 | 999 | yes |  |
| `automatic_vs_manual` | `tt[0,1,0,0,1,0,1,0,0]` | 0 | 39 | 999 | yes |  |
| `automatic_vs_manual` | `tt[0,1,0,1,0,1,1,0,0]` | -2 | 999 | 999 | yes |  |
| `automatic_vs_manual` | `tt[0,1,0,1,0,1,1,0,0]` | -1 | 40 | 39 | yes |  |
| `automatic_vs_manual` | `tt[0,1,0,1,0,1,1,0,0]` | 0 | 39 | 39 | yes |  |
| `automatic_vs_manual` | `tt[0,1,0,1,1,1,1,0,0]` | -1 | 39 | 39 | yes |  |
| `automatic_vs_manual` | `tt[0,1,0,1,1,1,1,0,0]` | 0 | 39 | 39 | yes |  |
| `automatic_vs_manual` | `tt[0,1,1,1,1,0,1,0,0]` | -1 | 39 | 39 | yes |  |
| `automatic_vs_manual` | `tt[0,1,1,1,1,0,1,0,0]` | 0 | 39 | 39 | yes |  |
| `automatic_vs_manual` | `tt[1,-1,1,0,1,0,1,0,0]` | -2 | 999 | 999 | yes |  |
| `automatic_vs_manual` | `tt[1,-1,1,0,1,0,1,0,0]` | -1 | 39 | 39 | yes |  |
| `automatic_vs_manual` | `tt[1,-1,1,0,1,0,1,0,0]` | 0 | 39 | 39 | yes |  |
| `automatic_vs_manual` | `tt[1,0,0,0,1,0,1,0,0]` | -2 | 40 | 999 | yes |  |
| `automatic_vs_manual` | `tt[1,0,0,0,1,0,1,0,0]` | -1 | 39 | 999 | yes |  |
| `automatic_vs_manual` | `tt[1,0,0,0,1,0,1,0,0]` | 0 | 39 | 999 | yes |  |
| `automatic_vs_manual` | `tt[1,0,1,-1,1,0,1,0,0]` | -2 | 999 | 999 | yes |  |
| `automatic_vs_manual` | `tt[1,0,1,-1,1,0,1,0,0]` | -1 | 39 | 39 | yes |  |
| `automatic_vs_manual` | `tt[1,0,1,-1,1,0,1,0,0]` | 0 | 39 | 39 | yes |  |
| `automatic_vs_manual` | `tt[1,0,1,0,1,0,0,0,0]` | -2 | 999 | 999 | yes |  |
| `automatic_vs_manual` | `tt[1,0,1,0,1,0,0,0,0]` | -1 | 39 | 39 | yes |  |
| `automatic_vs_manual` | `tt[1,0,1,0,1,0,0,0,0]` | 0 | 39 | 39 | yes |  |
| `automatic_vs_manual` | `tt[1,0,1,0,1,0,1,0,0]` | -2 | 999 | 999 | yes |  |
| `automatic_vs_manual` | `tt[1,0,1,0,1,0,1,0,0]` | -1 | 40 | 39 | yes |  |
| `automatic_vs_manual` | `tt[1,0,1,0,1,0,1,0,0]` | 0 | 40 | 40 | yes |  |
| `automatic_vs_manual` | `tt[1,0,1,1,0,1,0,0,0]` | -2 | 40 | 999 | yes |  |
| `automatic_vs_manual` | `tt[1,0,1,1,0,1,0,0,0]` | -1 | 39 | 39 | yes |  |
| `automatic_vs_manual` | `tt[1,0,1,1,0,1,0,0,0]` | 0 | 39 | 39 | yes |  |
| `automatic_vs_manual` | `tt[1,0,1,1,1,1,0,0,0]` | -1 | 39 | 39 | yes |  |
| `automatic_vs_manual` | `tt[1,0,1,1,1,1,0,0,0]` | 0 | 40 | 39 | yes |  |
| `automatic_vs_manual` | `tt[1,1,1,-1,1,0,1,0,0]` | -3 | 39 | 999 | yes |  |
| `automatic_vs_manual` | `tt[1,1,1,-1,1,0,1,0,0]` | -2 | 39 | 39 | yes |  |
| `automatic_vs_manual` | `tt[1,1,1,-1,1,0,1,0,0]` | -1 | 39 | 39 | yes |  |
| `automatic_vs_manual` | `tt[1,1,1,-1,1,0,1,0,0]` | 0 | 39 | 39 | yes |  |
| `automatic_vs_manual` | `tt[1,1,1,0,1,0,1,0,0]` | -3 | 39 | 999 | yes |  |
| `automatic_vs_manual` | `tt[1,1,1,0,1,0,1,0,0]` | -2 | 39 | 39 | yes |  |
| `automatic_vs_manual` | `tt[1,1,1,0,1,0,1,0,0]` | -1 | 39 | 39 | yes |  |
| `automatic_vs_manual` | `tt[1,1,1,0,1,0,1,0,0]` | 0 | 39 | 39 | yes |  |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,-3,0]` | -4 | 39 | 999 | yes |  |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,-3,0]` | -3 | 39 | 39 | yes |  |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,-3,0]` | -2 | 39 | 40 | yes |  |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,-3,0]` | -1 | 39 | 40 | yes |  |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,-3,0]` | 0 | 39 | 39 | yes |  |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,-2,-1]` | -5 | 999 | 999 | yes |  |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,-2,-1]` | -4 | 39 | 999 | yes |  |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,-2,-1]` | -3 | 39 | 999 | yes |  |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,-2,-1]` | -2 | 40 | 39 | yes |  |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,-2,-1]` | -1 | 39 | 39 | yes |  |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,-2,-1]` | 0 | 37 | 39 | yes |  |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,-1,-2]` | -5 | 999 | 999 | yes |  |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,-1,-2]` | -4 | 39 | 999 | yes |  |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,-1,-2]` | -3 | 40 | 999 | yes |  |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,-1,-2]` | -2 | 39 | 39 | yes |  |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,-1,-2]` | -1 | 39 | 39 | yes |  |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,-1,-2]` | 0 | 37 | 39 | yes |  |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,-1,0]` | -4 | 39 | 999 | yes |  |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,-1,0]` | -3 | 40 | 39 | yes |  |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,-1,0]` | -2 | 39 | 39 | yes |  |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,-1,0]` | -1 | 39 | 39 | yes |  |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,-1,0]` | 0 | 39 | 39 | yes |  |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,0,-3]` | -5 | 999 | 999 | yes |  |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,0,-3]` | -4 | 39 | 999 | yes |  |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,0,-3]` | -3 | 39 | 999 | yes |  |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,0,-3]` | -2 | 39 | 39 | yes |  |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,0,-3]` | -1 | 39 | 39 | yes |  |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,0,-3]` | 0 | 36 | 39 | yes |  |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,0,-1]` | -4 | 39 | 999 | yes |  |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,0,-1]` | -3 | 39 | 999 | yes |  |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,0,-1]` | -2 | 39 | 39 | yes |  |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,0,-1]` | -1 | 39 | 39 | yes |  |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,0,-1]` | 0 | 40 | 39 | yes |  |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,0,0]` | -4 | 40 | 999 | yes |  |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,0,0]` | -3 | 40 | 39 | yes |  |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,0,0]` | -2 | 39 | 39 | yes |  |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,0,0]` | -1 | 39 | 39 | yes |  |
| `automatic_vs_manual` | `tt[1,1,1,1,1,1,1,0,0]` | 0 | 39 | 39 | yes |  |
| `complex_kinematics.eps2` | `box[0,0,0,1]` | -1 | 999 | 999 | yes |  |
| `complex_kinematics.eps2` | `box[0,0,0,1]` | 0 | 41 | 41 | yes |  |
| `complex_kinematics.eps2` | `box[0,0,0,1]` | 1 | 41 | 41 | yes |  |
| `complex_kinematics.eps2` | `box[0,0,0,1]` | 2 | 41 | 41 | yes |  |
| `complex_kinematics.eps2` | `box[0,0,1,1]` | -1 | 999 | 999 | yes |  |
| `complex_kinematics.eps2` | `box[0,0,1,1]` | 0 | 41 | 41 | yes |  |
| `complex_kinematics.eps2` | `box[0,0,1,1]` | 1 | 41 | 41 | yes |  |
| `complex_kinematics.eps2` | `box[0,0,1,1]` | 2 | 41 | 42 | yes |  |
| `complex_kinematics.eps2` | `box[0,1,0,1]` | -1 | 999 | 999 | yes |  |
| `complex_kinematics.eps2` | `box[0,1,0,1]` | 0 | 41 | 41 | yes |  |
| `complex_kinematics.eps2` | `box[0,1,0,1]` | 1 | 41 | 41 | yes |  |
| `complex_kinematics.eps2` | `box[0,1,0,1]` | 2 | 41 | 42 | yes |  |
| `complex_kinematics.eps2` | `box[1,0,0,1]` | -1 | 999 | 999 | yes |  |
| `complex_kinematics.eps2` | `box[1,0,0,1]` | 0 | 41 | 41 | yes |  |
| `complex_kinematics.eps2` | `box[1,0,0,1]` | 1 | 41 | 41 | yes |  |
| `complex_kinematics.eps2` | `box[1,0,0,1]` | 2 | 41 | 41 | yes |  |
| `complex_kinematics.eps2` | `box[1,0,1,0]` | -1 | 999 | 999 | yes |  |
| `complex_kinematics.eps2` | `box[1,0,1,0]` | 0 | 41 | 41 | yes |  |
| `complex_kinematics.eps2` | `box[1,0,1,0]` | 1 | 41 | 41 | yes |  |
| `complex_kinematics.eps2` | `box[1,0,1,0]` | 2 | 41 | 41 | yes |  |
| `complex_kinematics.eps2` | `box[1,0,1,1]` | 0 | 41 | 41 | yes |  |
| `complex_kinematics.eps2` | `box[1,0,1,1]` | 1 | 41 | 41 | yes |  |
| `complex_kinematics.eps2` | `box[1,0,1,1]` | 2 | 41 | 41 | yes |  |
| `complex_kinematics.eps2` | `box[1,1,1,1]` | -2 | 41 | 41 | yes |  |
| `complex_kinematics.eps2` | `box[1,1,1,1]` | -1 | 41 | 42 | yes |  |
| `complex_kinematics.eps2` | `box[1,1,1,1]` | 0 | 41 | 42 | yes |  |
| `complex_kinematics.eps2` | `box[1,1,1,1]` | 1 | 41 | 42 | yes |  |
| `complex_kinematics.eps2` | `box[1,1,1,1]` | 2 | 41 | 41 | yes |  |
| `complex_kinematics` | `box[0,0,0,1]` | -1 | 999 | 999 | yes |  |
| `complex_kinematics` | `box[0,0,0,1]` | 0 | 41 | 41 | yes |  |
| `complex_kinematics` | `box[0,0,1,1]` | -1 | 999 | 999 | yes |  |
| `complex_kinematics` | `box[0,0,1,1]` | 0 | 41 | 41 | yes |  |
| `complex_kinematics` | `box[0,1,0,1]` | -1 | 999 | 999 | yes |  |
| `complex_kinematics` | `box[0,1,0,1]` | 0 | 41 | 41 | yes |  |
| `complex_kinematics` | `box[1,0,0,1]` | -1 | 999 | 999 | yes |  |
| `complex_kinematics` | `box[1,0,0,1]` | 0 | 41 | 41 | yes |  |
| `complex_kinematics` | `box[1,0,1,0]` | -1 | 999 | 999 | yes |  |
| `complex_kinematics` | `box[1,0,1,0]` | 0 | 41 | 41 | yes |  |
| `complex_kinematics` | `box[1,0,1,1]` | 0 | 41 | 41 | yes |  |
| `complex_kinematics` | `box[1,1,1,1]` | -2 | 41 | 41 | yes |  |
| `complex_kinematics` | `box[1,1,1,1]` | -1 | 41 | 42 | yes |  |
| `complex_kinematics` | `box[1,1,1,1]` | 0 | 41 | 42 | yes |  |
| `differential_equation_solver` | `sunrise[1,1,1,-1,0]` | 0 | 0 | 31 | no | below tolerance or missing coefficient |
| `differential_equation_solver` | `sunrise[1,1,1,0,0]` | 0 | 0 | 33 | no | below tolerance or missing coefficient |
| `differential_equation_solver` | `sunrise[2,1,1,0,0]` | 0 | 1 | 32 | no | below tolerance or missing coefficient |
| `differential_equation_solver.sol2` | `sunrise[1,1,1,-1,0]` | 0 | 31 | 30 | yes |  |
| `differential_equation_solver.sol2` | `sunrise[1,1,1,0,0]` | 0 | 31 | 31 | yes |  |
| `differential_equation_solver.sol2` | `sunrise[2,1,1,0,0]` | 0 | 30 | 31 | yes |  |
| `feynman_prescription` | `loopxloop[0,0,1,1,0,0,1,1,1,1,0,0]` | -2 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,0,1,1,0,0,1,1,1,1,0,0]` | -1 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,0,1,1,0,0,1,1,1,1,0,0]` | 0 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,0,1,1,0,0,1,1,1,1,0,0]` | 1 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,0,1,1,0,0,1,1,1,1,0,0]` | 2 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,0,1,1,1,0,0,1,1,1,0,0]` | -2 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,0,1,1,1,0,0,1,1,1,0,0]` | -1 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,0,1,1,1,0,0,1,1,1,0,0]` | 0 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,0,1,1,1,0,0,1,1,1,0,0]` | 1 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,0,1,1,1,0,0,1,1,1,0,0]` | 2 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,0,1,1,1,0,1,0,1,1,0,0]` | -2 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,0,1,1,1,0,1,0,1,1,0,0]` | -1 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,0,1,1,1,0,1,0,1,1,0,0]` | 0 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,0,1,1,1,0,1,0,1,1,0,0]` | 1 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,0,1,1,1,0,1,0,1,1,0,0]` | 2 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,0,1,1,1,0,1,1,1,1,0,0]` | -1 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,0,1,1,1,0,1,1,1,1,0,0]` | 0 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,0,1,1,1,0,1,1,1,1,0,0]` | 1 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,0,1,1,1,0,1,1,1,1,0,0]` | 2 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,1,-1,1,0,0,1,1,1,1,0,0]` | -2 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,1,-1,1,0,0,1,1,1,1,0,0]` | -1 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,1,-1,1,0,0,1,1,1,1,0,0]` | 0 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,1,-1,1,0,0,1,1,1,1,0,0]` | 1 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,1,-1,1,0,0,1,1,1,1,0,0]` | 2 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,1,-1,1,1,0,0,1,1,1,0,0]` | -2 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,1,-1,1,1,0,0,1,1,1,0,0]` | -1 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,1,-1,1,1,0,0,1,1,1,0,0]` | 0 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,1,-1,1,1,0,0,1,1,1,0,0]` | 1 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,1,-1,1,1,0,0,1,1,1,0,0]` | 2 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,1,-1,1,1,0,1,0,1,1,0,0]` | -2 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,1,-1,1,1,0,1,0,1,1,0,0]` | -1 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,1,-1,1,1,0,1,0,1,1,0,0]` | 0 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,1,-1,1,1,0,1,0,1,1,0,0]` | 1 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,1,-1,1,1,0,1,0,1,1,0,0]` | 2 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,1,-1,1,1,0,1,1,1,1,0,0]` | -1 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,1,-1,1,1,0,1,1,1,1,0,0]` | 0 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,1,-1,1,1,0,1,1,1,1,0,0]` | 1 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,1,-1,1,1,0,1,1,1,1,0,0]` | 2 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,1,0,1,0,0,1,1,1,1,0,0]` | -2 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,1,0,1,0,0,1,1,1,1,0,0]` | -1 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,1,0,1,0,0,1,1,1,1,0,0]` | 0 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,1,0,1,0,0,1,1,1,1,0,0]` | 1 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,1,0,1,0,0,1,1,1,1,0,0]` | 2 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,1,0,1,1,0,0,1,1,1,0,0]` | -2 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,1,0,1,1,0,0,1,1,1,0,0]` | -1 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,1,0,1,1,0,0,1,1,1,0,0]` | 0 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,1,0,1,1,0,0,1,1,1,0,0]` | 1 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,1,0,1,1,0,0,1,1,1,0,0]` | 2 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,1,0,1,1,0,1,0,1,1,0,0]` | -2 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,1,0,1,1,0,1,0,1,1,0,0]` | -1 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,1,0,1,1,0,1,0,1,1,0,0]` | 0 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,1,0,1,1,0,1,0,1,1,0,0]` | 1 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,1,0,1,1,0,1,0,1,1,0,0]` | 2 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,1,0,1,1,0,1,1,1,1,0,0]` | -1 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,1,0,1,1,0,1,1,1,1,0,0]` | 0 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,1,0,1,1,0,1,1,1,1,0,0]` | 1 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,1,0,1,1,0,1,1,1,1,0,0]` | 2 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,1,1,1,0,0,1,1,1,1,0,0]` | -2 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,1,1,1,0,0,1,1,1,1,0,0]` | -1 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,1,1,1,0,0,1,1,1,1,0,0]` | 0 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,1,1,1,0,0,1,1,1,1,0,0]` | 1 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,1,1,1,0,0,1,1,1,1,0,0]` | 2 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,1,1,1,1,0,0,1,1,1,0,0]` | -2 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,1,1,1,1,0,0,1,1,1,0,0]` | -1 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,1,1,1,1,0,0,1,1,1,0,0]` | 0 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,1,1,1,1,0,0,1,1,1,0,0]` | 1 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,1,1,1,1,0,0,1,1,1,0,0]` | 2 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,1,1,1,1,0,1,0,1,1,0,0]` | -2 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,1,1,1,1,0,1,0,1,1,0,0]` | -1 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,1,1,1,1,0,1,0,1,1,0,0]` | 0 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,1,1,1,1,0,1,0,1,1,0,0]` | 1 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,1,1,1,1,0,1,0,1,1,0,0]` | 2 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,1,1,1,1,0,1,1,1,1,0,0]` | -1 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,1,1,1,1,0,1,1,1,1,0,0]` | 0 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,1,1,1,1,0,1,1,1,1,0,0]` | 1 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `feynman_prescription` | `loopxloop[0,1,1,1,1,0,1,1,1,1,0,0]` | 2 | n/a | n/a | no | [Errno 2] No such file or directory: '/tmp/autoibp_orch/exec/lane24_aggregate_rerun/runs/feynman_prescription.cpp-result.json' |
| `linear_propagator` | `gauge[0,1,1,1,1,-1,0,0,0]` | 0 | 999 | 999 | yes |  |
| `linear_propagator` | `gauge[0,1,1,1,1,-1,0,0,0]` | 1 | 999 | 999 | yes |  |
| `linear_propagator` | `gauge[0,1,1,1,1,-1,0,0,0]` | 2 | 999 | 999 | yes |  |
| `linear_propagator` | `gauge[0,1,1,1,1,-1,0,0,0]` | 3 | 999 | 999 | yes |  |
| `linear_propagator` | `gauge[0,1,1,1,1,-1,0,0,0]` | 4 | 999 | 999 | yes |  |
| `linear_propagator` | `gauge[0,1,1,1,1,0,0,0,0]` | 0 | 999 | 999 | yes |  |
| `linear_propagator` | `gauge[0,1,1,1,1,0,0,0,0]` | 1 | 999 | 999 | yes |  |
| `linear_propagator` | `gauge[0,1,1,1,1,0,0,0,0]` | 2 | 999 | 999 | yes |  |
| `linear_propagator` | `gauge[0,1,1,1,1,0,0,0,0]` | 3 | 999 | 999 | yes |  |
| `linear_propagator` | `gauge[0,1,1,1,1,0,0,0,0]` | 4 | 999 | 999 | yes |  |
| `linear_propagator` | `gauge[1,1,1,-1,1,0,0,0,0]` | -1 | 39 | 999 | yes |  |
| `linear_propagator` | `gauge[1,1,1,-1,1,0,0,0,0]` | 0 | 39 | 39 | yes |  |
| `linear_propagator` | `gauge[1,1,1,-1,1,0,0,0,0]` | 1 | 39 | 39 | yes |  |
| `linear_propagator` | `gauge[1,1,1,-1,1,0,0,0,0]` | 2 | 39 | 39 | yes |  |
| `linear_propagator` | `gauge[1,1,1,-1,1,0,0,0,0]` | 3 | 39 | 38 | yes |  |
| `linear_propagator` | `gauge[1,1,1,-1,1,0,0,0,0]` | 4 | 39 | 34 | yes |  |
| `linear_propagator` | `gauge[1,1,1,0,1,0,0,0,0]` | -1 | 39 | 999 | yes |  |
| `linear_propagator` | `gauge[1,1,1,0,1,0,0,0,0]` | 0 | 39 | 39 | yes |  |
| `linear_propagator` | `gauge[1,1,1,0,1,0,0,0,0]` | 1 | 39 | 39 | yes |  |
| `linear_propagator` | `gauge[1,1,1,0,1,0,0,0,0]` | 2 | 40 | 39 | yes |  |
| `linear_propagator` | `gauge[1,1,1,0,1,0,0,0,0]` | 3 | 39 | 38 | yes |  |
| `linear_propagator` | `gauge[1,1,1,0,1,0,0,0,0]` | 4 | 39 | 34 | yes |  |
| `linear_propagator` | `gauge[1,1,1,1,1,-1,0,0,0]` | -2 | 39 | 999 | yes |  |
| `linear_propagator` | `gauge[1,1,1,1,1,-1,0,0,0]` | -1 | 39 | 39 | yes |  |
| `linear_propagator` | `gauge[1,1,1,1,1,-1,0,0,0]` | 0 | 39 | 39 | yes |  |
| `linear_propagator` | `gauge[1,1,1,1,1,-1,0,0,0]` | 1 | 39 | 39 | yes |  |
| `linear_propagator` | `gauge[1,1,1,1,1,-1,0,0,0]` | 2 | 40 | 39 | yes |  |
| `linear_propagator` | `gauge[1,1,1,1,1,-1,0,0,0]` | 3 | 39 | 35 | yes |  |
| `linear_propagator` | `gauge[1,1,1,1,1,-1,0,0,0]` | 4 | 35 | 32 | yes |  |
| `linear_propagator` | `gauge[1,1,1,1,1,0,-1,0,0]` | -2 | 39 | 999 | yes |  |
| `linear_propagator` | `gauge[1,1,1,1,1,0,-1,0,0]` | -1 | 39 | 39 | yes |  |
| `linear_propagator` | `gauge[1,1,1,1,1,0,-1,0,0]` | 0 | 39 | 39 | yes |  |
| `linear_propagator` | `gauge[1,1,1,1,1,0,-1,0,0]` | 1 | 39 | 39 | yes |  |
| `linear_propagator` | `gauge[1,1,1,1,1,0,-1,0,0]` | 2 | 40 | 39 | yes |  |
| `linear_propagator` | `gauge[1,1,1,1,1,0,-1,0,0]` | 3 | 39 | 35 | yes |  |
| `linear_propagator` | `gauge[1,1,1,1,1,0,-1,0,0]` | 4 | 35 | 32 | yes |  |
| `linear_propagator` | `gauge[1,1,1,1,1,0,0,-1,0]` | -2 | 39 | 999 | yes |  |
| `linear_propagator` | `gauge[1,1,1,1,1,0,0,-1,0]` | -1 | 40 | 39 | yes |  |
| `linear_propagator` | `gauge[1,1,1,1,1,0,0,-1,0]` | 0 | 39 | 39 | yes |  |
| `linear_propagator` | `gauge[1,1,1,1,1,0,0,-1,0]` | 1 | 39 | 39 | yes |  |
| `linear_propagator` | `gauge[1,1,1,1,1,0,0,-1,0]` | 2 | 39 | 39 | yes |  |
| `linear_propagator` | `gauge[1,1,1,1,1,0,0,-1,0]` | 3 | 39 | 36 | yes |  |
| `linear_propagator` | `gauge[1,1,1,1,1,0,0,-1,0]` | 4 | 36 | 32 | yes |  |
| `linear_propagator` | `gauge[1,1,1,1,1,0,0,0,-1]` | -2 | 39 | 999 | yes |  |
| `linear_propagator` | `gauge[1,1,1,1,1,0,0,0,-1]` | -1 | 39 | 39 | yes |  |
| `linear_propagator` | `gauge[1,1,1,1,1,0,0,0,-1]` | 0 | 39 | 39 | yes |  |
| `linear_propagator` | `gauge[1,1,1,1,1,0,0,0,-1]` | 1 | 40 | 40 | yes |  |
| `linear_propagator` | `gauge[1,1,1,1,1,0,0,0,-1]` | 2 | 39 | 39 | yes |  |
| `linear_propagator` | `gauge[1,1,1,1,1,0,0,0,-1]` | 3 | 39 | 35 | yes |  |
| `linear_propagator` | `gauge[1,1,1,1,1,0,0,0,-1]` | 4 | 35 | 32 | yes |  |
| `linear_propagator` | `gauge[1,1,1,1,1,0,0,0,0]` | -2 | 39 | 999 | yes |  |
| `linear_propagator` | `gauge[1,1,1,1,1,0,0,0,0]` | -1 | 40 | 39 | yes |  |
| `linear_propagator` | `gauge[1,1,1,1,1,0,0,0,0]` | 0 | 39 | 39 | yes |  |
| `linear_propagator` | `gauge[1,1,1,1,1,0,0,0,0]` | 1 | 39 | 39 | yes |  |
| `linear_propagator` | `gauge[1,1,1,1,1,0,0,0,0]` | 2 | 39 | 39 | yes |  |
| `linear_propagator` | `gauge[1,1,1,1,1,0,0,0,0]` | 3 | 39 | 35 | yes |  |
| `linear_propagator` | `gauge[1,1,1,1,1,0,0,0,0]` | 4 | 35 | 31 | yes |  |

## Milestone Status

- M3: unchanged from the previously reviewed narrow automatic-loop retained-state closure; this aggregate does not broaden M3 into a full packet-set proof.
- M4: unchanged from the previously reviewed retained automatic-loop epsilon-coverage proof through eps4; the packet aggregate is validation evidence, not a broader contour/runtime closure.
- M5: PARTIAL/FAILING. Current retained surfaces improved substantially, but at least one current phase-0 manifest surface still lacks a passing coefficient-bearing C++ comparison and Phase F live-path criteria remain open.
- M6: TODO/FAILING because M6 depends on M5 plus packet-shaped candidate outputs and case-study qualification evidence; this lane does not claim M6 or release readiness.

## Review Roles

- Role A APPROVE: rechecked the current manifest inventory against the report and aggregate JSON; all 11 current `*golden-manifest.json` surfaces are included, including `complex_kinematics.eps2`, and the per-surface solve/compare command evidence is coherent.
- Role B APPROVE: recomputed the all-manifest aggregate as `400/479`, the latest-per-benchmark aggregate as `266/342`, and the deltas against Lane 12 `54/217` as `+346` and `+212` passing coefficients respectively.
- Role C APPROVE: rechecked the ledger, milestone plan, and report wording; the docs preserve M3/M4 boundaries, keep M5 as partial/failing, and do not claim M6/M7/release readiness.
- Role D APPROVE: verified every coefficient-bearing compare JSON and aggregate entry uses `tolerance_digits=30`, `feynman_prescription` remains fail-closed at `0/76`, and no current phase-0 golden-manifest surface is omitted from the all-manifest aggregate.
