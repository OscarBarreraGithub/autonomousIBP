# Lane 32 Phase-0 Packet Aggregate Re-Run

Generated `2026-05-05T05:47:27.480297-04:00` from `/n/holylabs/schwartz_lab/Lab/obarrera/autoIBP`.

## Gate And Methodology

- Fresh rebuild gate: `PATH=/n/sw/helmod-rocky8/apps/Core/cmake/4.2.3-fasrc01/bin:$PATH cmake --build build --target amflow-cli` exited `0`.
- Comparator tolerance: every comparator invocation used `--tolerance-digits 30`; no tolerance was loosened.
- C++ solve precision: every `amflow-cli solve-series` invocation used `--digits 40`.
- Scope: every current `tools/reference-harness/specs/phase0/*golden-manifest.json` surface is included exactly once, including `automatic_loop.eps2` through `automatic_loop.eps6`.
- Counting policy: successful comparisons use `compare_cpp_vs_amflow.py` counts; failed or missing coefficient-bearing C++ probes count explicit AMFlow coefficients in the scoped manifest at the requested epsilon order as zero passed.

## Aggregate

- All-manifest aggregate: `669/671` coefficients pass at `>=30` digits across `13` manifest surfaces.
- Delta vs lane 24 `400/479`: `+269` passing coefficients and `+192` total scoped coefficients.
- Delta vs lane 29 `476/479`: `+193` passing coefficients and `+192` total scoped coefficients.
- Latest-per-benchmark aggregate: `364/366` coefficients pass at `>=30` digits, using `automatic_loop.eps6`, `automatic_phasespace`, `automatic_vs_manual`, `complex_kinematics.eps2`, `differential_equation_solver.sol2`, `feynman_prescription`, and `linear_propagator`.
- Remaining failures: `automatic_loop.eps6` has two eps^6 real-part coefficient misses; all other manifest surfaces passed at 30 digits.

## Manifest Inventory

- `tools/reference-harness/specs/phase0/automatic_loop.eps2-golden-manifest.json`
- `tools/reference-harness/specs/phase0/automatic_loop.eps3-golden-manifest.json`
- `tools/reference-harness/specs/phase0/automatic_loop.eps4-golden-manifest.json`
- `tools/reference-harness/specs/phase0/automatic_loop.eps5-golden-manifest.json`
- `tools/reference-harness/specs/phase0/automatic_loop.eps6-golden-manifest.json`
- `tools/reference-harness/specs/phase0/automatic_phasespace.golden-manifest.json`
- `tools/reference-harness/specs/phase0/automatic_vs_manual.golden-manifest.json`
- `tools/reference-harness/specs/phase0/complex_kinematics.golden-manifest.json`
- `tools/reference-harness/specs/phase0/complex_kinematics.eps2-golden-manifest.json`
- `tools/reference-harness/specs/phase0/differential_equation_solver.golden-manifest.json`
- `tools/reference-harness/specs/phase0/differential_equation_solver.sol2-golden-manifest.json`
- `tools/reference-harness/specs/phase0/feynman_prescription.golden-manifest.json`
- `tools/reference-harness/specs/phase0/linear_propagator.golden-manifest.json`

## Per-Benchmark Summary

| Manifest surface | Benchmark | C++ input/probe | eps order | solve exit | compare exit | compare passed | coefficients passed | total coefficients | min digits | AMFlow order counts | Failure/gap |
| --- | --- | --- | ---: | ---: | ---: | --- | ---: | ---: | --- | --- | --- |
| `automatic_loop.eps2` | `automatic_loop` | `tools/reference-harness/specs/phase0/automatic_loop.amflow-state.json` | 2 | 0 | 0 | true | 54 | 54 | 41 | eps^-2: 6, eps^-1: 12, eps^0: 12, eps^1: 12, eps^2: 12 |  |
| `automatic_loop.eps3` | `automatic_loop` | `tools/reference-harness/specs/phase0/automatic_loop.amflow-state.json` | 3 | 0 | 0 | true | 66 | 66 | 41 | eps^-2: 6, eps^-1: 12, eps^0: 12, eps^1: 12, eps^2: 12, eps^3: 12 |  |
| `automatic_loop.eps4` | `automatic_loop` | `tools/reference-harness/specs/phase0/automatic_loop.amflow-state.json` | 4 | 0 | 0 | true | 78 | 78 | 41 | eps^-2: 6, eps^-1: 12, eps^0: 12, eps^1: 12, eps^2: 12, eps^3: 12, eps^4: 12 |  |
| `automatic_loop.eps5` | `automatic_loop` | `tools/reference-harness/specs/phase0/automatic_loop.amflow-state.json` | 5 | 0 | 0 | true | 90 | 90 | 41 | eps^-2: 6, eps^-1: 12, eps^0: 12, eps^1: 12, eps^2: 12, eps^3: 12, eps^4: 12, eps^5: 12 |  |
| `automatic_loop.eps6` | `automatic_loop` | `tools/reference-harness/specs/phase0/automatic_loop.amflow-state.json` | 6 | 0 | 1 | false | 100 | 102 | 14 | eps^-2: 6, eps^-1: 12, eps^0: 12, eps^1: 12, eps^2: 12, eps^3: 12, eps^4: 12, eps^5: 12, eps^6: 12 | box1[-2,1,1,2] eps^6 has real/imag agreement 14/999, below tolerance 30; box2[1,1,0,1] eps^6 has real/imag agreement 18/999, below tolerance 30 |
| `automatic_phasespace` | `automatic_phasespace` | `tools/reference-harness/specs/phase0/automatic_phasespace.amflow-state.json` | 0 | 0 | 0 | true | 11 | 11 | 39 | eps^-3: 2, eps^-2: 2, eps^-1: 2, eps^0: 5 |  |
| `automatic_vs_manual` | `automatic_vs_manual` | `tools/reference-harness/specs/phase0/automatic_vs_manual.amflow-state.json` | 0 | 0 | 0 | true | 89 | 89 | 36 | eps^-4: 7, eps^-3: 9, eps^-2: 20, eps^-1: 25, eps^0: 25 |  |
| `complex_kinematics` | `complex_kinematics` | `tools/reference-harness/specs/phase0/complex_kinematics.amflow-state.json` | 0 | 0 | 0 | true | 14 | 14 | 41 | eps^-2: 1, eps^-1: 6, eps^0: 7 |  |
| `complex_kinematics.eps2` | `complex_kinematics` | `tools/reference-harness/specs/phase0/complex_kinematics.amflow-state.json` | 2 | 0 | 0 | true | 28 | 28 | 41 | eps^-2: 1, eps^-1: 6, eps^0: 7, eps^1: 7, eps^2: 7 |  |
| `differential_equation_solver` | `differential_equation_solver` | `tools/reference-harness/specs/phase0/differential_equation_solver.amflow-state.json` | 0 | 0 | 0 | true | 3 | 3 | 41 | eps^0: 3 |  |
| `differential_equation_solver.sol2` | `differential_equation_solver` | `tools/reference-harness/specs/phase0/differential_equation_solver.amflow-state.json` | 0 | 0 | 0 | true | 3 | 3 | 30 | eps^0: 3 |  |
| `feynman_prescription` | `feynman_prescription` | `tools/reference-harness/specs/phase0/feynman_prescription.amflow-state.json` | 2 | 0 | 0 | true | 76 | 76 | 39 | eps^-2: 12, eps^-1: 16, eps^0: 16, eps^1: 16, eps^2: 16 |  |
| `linear_propagator` | `linear_propagator` | `tools/reference-harness/specs/phase0/linear_propagator.amflow-state.json` | 4 | 0 | 0 | true | 57 | 57 | 31 | eps^-2: 5, eps^-1: 7, eps^0: 9, eps^1: 7, eps^2: 7, eps^3: 7, eps^4: 7 |  |

## Failing Coefficients

| Manifest surface | Integral | eps order | real digits | imag digits | C++ real | AMFlow real | Failure |
| --- | --- | ---: | ---: | ---: | ---: | ---: | --- |
| `automatic_loop.eps6` | `box1[-2,1,1,2]` | 6 | 14 | 999 | `-43332.3032275465731761101123164311880797226270` | `-43332.3032275465732208221193608054627827893117808747838067656666` | box1[-2,1,1,2] eps^6 has real/imag agreement 14/999, below tolerance 30 |
| `automatic_loop.eps6` | `box2[1,1,0,1]` | 6 | 18 | 999 | `1.5564022450546435538719634422048224679849` | `1.5564022450546435516363630899861087328316626703803650415273` | box2[1,1,0,1] eps^6 has real/imag agreement 18/999, below tolerance 30 |

## Command Evidence

| Manifest surface | solve-series command | compare command |
| --- | --- | --- |
| `automatic_loop.eps2` | `/n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/build/amflow-cli solve-series /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/specs/phase0/automatic_loop.amflow-state.json --eps-order 2 --digits 40 --out /tmp/autoibp_orch/exec/lane32_aggregate_v3/runs/automatic_loop.eps2.cpp-result.json` | `/n/home09/obarrera/.conda/envs/ising_bootstrap/bin/python3 /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/scripts/compare_cpp_vs_amflow.py --cpp-result /tmp/autoibp_orch/exec/lane32_aggregate_v3/runs/automatic_loop.eps2.cpp-result.json --amflow-golden /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/specs/phase0/automatic_loop.eps2-golden-manifest.json --tolerance-digits 30` |
| `automatic_loop.eps3` | `/n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/build/amflow-cli solve-series /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/specs/phase0/automatic_loop.amflow-state.json --eps-order 3 --digits 40 --out /tmp/autoibp_orch/exec/lane32_aggregate_v3/runs/automatic_loop.eps3.cpp-result.json` | `/n/home09/obarrera/.conda/envs/ising_bootstrap/bin/python3 /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/scripts/compare_cpp_vs_amflow.py --cpp-result /tmp/autoibp_orch/exec/lane32_aggregate_v3/runs/automatic_loop.eps3.cpp-result.json --amflow-golden /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/specs/phase0/automatic_loop.eps3-golden-manifest.json --tolerance-digits 30` |
| `automatic_loop.eps4` | `/n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/build/amflow-cli solve-series /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/specs/phase0/automatic_loop.amflow-state.json --eps-order 4 --digits 40 --out /tmp/autoibp_orch/exec/lane32_aggregate_v3/runs/automatic_loop.eps4.cpp-result.json` | `/n/home09/obarrera/.conda/envs/ising_bootstrap/bin/python3 /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/scripts/compare_cpp_vs_amflow.py --cpp-result /tmp/autoibp_orch/exec/lane32_aggregate_v3/runs/automatic_loop.eps4.cpp-result.json --amflow-golden /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/specs/phase0/automatic_loop.eps4-golden-manifest.json --tolerance-digits 30` |
| `automatic_loop.eps5` | `/n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/build/amflow-cli solve-series /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/specs/phase0/automatic_loop.amflow-state.json --eps-order 5 --digits 40 --out /tmp/autoibp_orch/exec/lane32_aggregate_v3/runs/automatic_loop.eps5.cpp-result.json` | `/n/home09/obarrera/.conda/envs/ising_bootstrap/bin/python3 /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/scripts/compare_cpp_vs_amflow.py --cpp-result /tmp/autoibp_orch/exec/lane32_aggregate_v3/runs/automatic_loop.eps5.cpp-result.json --amflow-golden /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/specs/phase0/automatic_loop.eps5-golden-manifest.json --tolerance-digits 30` |
| `automatic_loop.eps6` | `/n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/build/amflow-cli solve-series /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/specs/phase0/automatic_loop.amflow-state.json --eps-order 6 --digits 40 --out /tmp/autoibp_orch/exec/lane32_aggregate_v3/runs/automatic_loop.eps6.cpp-result.json` | `/n/home09/obarrera/.conda/envs/ising_bootstrap/bin/python3 /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/scripts/compare_cpp_vs_amflow.py --cpp-result /tmp/autoibp_orch/exec/lane32_aggregate_v3/runs/automatic_loop.eps6.cpp-result.json --amflow-golden /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/specs/phase0/automatic_loop.eps6-golden-manifest.json --tolerance-digits 30` |
| `automatic_phasespace` | `/n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/build/amflow-cli solve-series /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/specs/phase0/automatic_phasespace.amflow-state.json --eps-order 0 --digits 40 --out /tmp/autoibp_orch/exec/lane32_aggregate_v3/runs/automatic_phasespace.cpp-result.json` | `/n/home09/obarrera/.conda/envs/ising_bootstrap/bin/python3 /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/scripts/compare_cpp_vs_amflow.py --cpp-result /tmp/autoibp_orch/exec/lane32_aggregate_v3/runs/automatic_phasespace.cpp-result.json --amflow-golden /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/specs/phase0/automatic_phasespace.golden-manifest.json --tolerance-digits 30` |
| `automatic_vs_manual` | `/n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/build/amflow-cli solve-series /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/specs/phase0/automatic_vs_manual.amflow-state.json --eps-order 0 --digits 40 --out /tmp/autoibp_orch/exec/lane32_aggregate_v3/runs/automatic_vs_manual.cpp-result.json` | `/n/home09/obarrera/.conda/envs/ising_bootstrap/bin/python3 /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/scripts/compare_cpp_vs_amflow.py --cpp-result /tmp/autoibp_orch/exec/lane32_aggregate_v3/runs/automatic_vs_manual.cpp-result.json --amflow-golden /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/specs/phase0/automatic_vs_manual.golden-manifest.json --tolerance-digits 30` |
| `complex_kinematics` | `/n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/build/amflow-cli solve-series /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/specs/phase0/complex_kinematics.amflow-state.json --eps-order 0 --digits 40 --out /tmp/autoibp_orch/exec/lane32_aggregate_v3/runs/complex_kinematics.cpp-result.json` | `/n/home09/obarrera/.conda/envs/ising_bootstrap/bin/python3 /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/scripts/compare_cpp_vs_amflow.py --cpp-result /tmp/autoibp_orch/exec/lane32_aggregate_v3/runs/complex_kinematics.cpp-result.json --amflow-golden /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/specs/phase0/complex_kinematics.golden-manifest.json --tolerance-digits 30` |
| `complex_kinematics.eps2` | `/n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/build/amflow-cli solve-series /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/specs/phase0/complex_kinematics.amflow-state.json --eps-order 2 --digits 40 --out /tmp/autoibp_orch/exec/lane32_aggregate_v3/runs/complex_kinematics.eps2.cpp-result.json` | `/n/home09/obarrera/.conda/envs/ising_bootstrap/bin/python3 /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/scripts/compare_cpp_vs_amflow.py --cpp-result /tmp/autoibp_orch/exec/lane32_aggregate_v3/runs/complex_kinematics.eps2.cpp-result.json --amflow-golden /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/specs/phase0/complex_kinematics.eps2-golden-manifest.json --tolerance-digits 30` |
| `differential_equation_solver` | `/n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/build/amflow-cli solve-series /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/specs/phase0/differential_equation_solver.amflow-state.json --eps-order 0 --digits 40 --out /tmp/autoibp_orch/exec/lane32_aggregate_v3/runs/differential_equation_solver.cpp-result.json` | `/n/home09/obarrera/.conda/envs/ising_bootstrap/bin/python3 /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/scripts/compare_cpp_vs_amflow.py --cpp-result /tmp/autoibp_orch/exec/lane32_aggregate_v3/runs/differential_equation_solver.cpp-result.json --amflow-golden /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/specs/phase0/differential_equation_solver.golden-manifest.json --tolerance-digits 30` |
| `differential_equation_solver.sol2` | `/n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/build/amflow-cli solve-series /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/specs/phase0/differential_equation_solver.amflow-state.json --eps-order 0 --digits 40 --out /tmp/autoibp_orch/exec/lane32_aggregate_v3/runs/differential_equation_solver.sol2.cpp-result.json` | `/n/home09/obarrera/.conda/envs/ising_bootstrap/bin/python3 /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/scripts/compare_cpp_vs_amflow.py --cpp-result /tmp/autoibp_orch/exec/lane32_aggregate_v3/runs/differential_equation_solver.sol2.cpp-result.json --amflow-golden /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/specs/phase0/differential_equation_solver.sol2-golden-manifest.json --tolerance-digits 30` |
| `feynman_prescription` | `/n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/build/amflow-cli solve-series /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/specs/phase0/feynman_prescription.amflow-state.json --eps-order 2 --digits 40 --out /tmp/autoibp_orch/exec/lane32_aggregate_v3/runs/feynman_prescription.cpp-result.json` | `/n/home09/obarrera/.conda/envs/ising_bootstrap/bin/python3 /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/scripts/compare_cpp_vs_amflow.py --cpp-result /tmp/autoibp_orch/exec/lane32_aggregate_v3/runs/feynman_prescription.cpp-result.json --amflow-golden /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/specs/phase0/feynman_prescription.golden-manifest.json --tolerance-digits 30` |
| `linear_propagator` | `/n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/build/amflow-cli solve-series /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/specs/phase0/linear_propagator.amflow-state.json --eps-order 4 --digits 40 --out /tmp/autoibp_orch/exec/lane32_aggregate_v3/runs/linear_propagator.cpp-result.json` | `/n/home09/obarrera/.conda/envs/ising_bootstrap/bin/python3 /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/scripts/compare_cpp_vs_amflow.py --cpp-result /tmp/autoibp_orch/exec/lane32_aggregate_v3/runs/linear_propagator.cpp-result.json --amflow-golden /n/holylabs/schwartz_lab/Lab/obarrera/autoIBP/tools/reference-harness/specs/phase0/linear_propagator.golden-manifest.json --tolerance-digits 30` |

## Per-Coefficient Digit Agreement

`999` is the comparator sentinel for exact equality. Rows with `pass >=30 = no` are counted as non-passing in the aggregate.

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
| `automatic_loop.eps4` | `box1[-2,1,1,2]` | 4 | 41 | 999 | yes |  |
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
| `automatic_loop.eps4` | `box2[1,1,0,1]` | 4 | 42 | 999 | yes |  |
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
| `automatic_loop.eps5` | `box1[-2,1,1,2]` | -2 | 999 | 999 | yes |  |
| `automatic_loop.eps5` | `box1[-2,1,1,2]` | -1 | 41 | 999 | yes |  |
| `automatic_loop.eps5` | `box1[-2,1,1,2]` | 0 | 41 | 999 | yes |  |
| `automatic_loop.eps5` | `box1[-2,1,1,2]` | 1 | 41 | 999 | yes |  |
| `automatic_loop.eps5` | `box1[-2,1,1,2]` | 2 | 41 | 999 | yes |  |
| `automatic_loop.eps5` | `box1[-2,1,1,2]` | 3 | 41 | 999 | yes |  |
| `automatic_loop.eps5` | `box1[-2,1,1,2]` | 4 | 41 | 999 | yes |  |
| `automatic_loop.eps5` | `box1[-2,1,1,2]` | 5 | 41 | 999 | yes |  |
| `automatic_loop.eps5` | `box1[0,1,0,1]` | -1 | 999 | 999 | yes |  |
| `automatic_loop.eps5` | `box1[0,1,0,1]` | 0 | 41 | 999 | yes |  |
| `automatic_loop.eps5` | `box1[0,1,0,1]` | 1 | 41 | 999 | yes |  |
| `automatic_loop.eps5` | `box1[0,1,0,1]` | 2 | 41 | 999 | yes |  |
| `automatic_loop.eps5` | `box1[0,1,0,1]` | 3 | 41 | 999 | yes |  |
| `automatic_loop.eps5` | `box1[0,1,0,1]` | 4 | 41 | 999 | yes |  |
| `automatic_loop.eps5` | `box1[0,1,0,1]` | 5 | 41 | 999 | yes |  |
| `automatic_loop.eps5` | `box1[1,0,1,0]` | -1 | 999 | 999 | yes |  |
| `automatic_loop.eps5` | `box1[1,0,1,0]` | 0 | 41 | 41 | yes |  |
| `automatic_loop.eps5` | `box1[1,0,1,0]` | 1 | 41 | 41 | yes |  |
| `automatic_loop.eps5` | `box1[1,0,1,0]` | 2 | 41 | 41 | yes |  |
| `automatic_loop.eps5` | `box1[1,0,1,0]` | 3 | 42 | 41 | yes |  |
| `automatic_loop.eps5` | `box1[1,0,1,0]` | 4 | 41 | 41 | yes |  |
| `automatic_loop.eps5` | `box1[1,0,1,0]` | 5 | 41 | 41 | yes |  |
| `automatic_loop.eps5` | `box1[1,1,1,1]` | -2 | 999 | 999 | yes |  |
| `automatic_loop.eps5` | `box1[1,1,1,1]` | -1 | 41 | 41 | yes |  |
| `automatic_loop.eps5` | `box1[1,1,1,1]` | 0 | 41 | 41 | yes |  |
| `automatic_loop.eps5` | `box1[1,1,1,1]` | 1 | 41 | 41 | yes |  |
| `automatic_loop.eps5` | `box1[1,1,1,1]` | 2 | 41 | 41 | yes |  |
| `automatic_loop.eps5` | `box1[1,1,1,1]` | 3 | 41 | 41 | yes |  |
| `automatic_loop.eps5` | `box1[1,1,1,1]` | 4 | 41 | 41 | yes |  |
| `automatic_loop.eps5` | `box1[1,1,1,1]` | 5 | 41 | 41 | yes |  |
| `automatic_loop.eps5` | `box1[1,2,2,1]` | -2 | 58 | 999 | yes |  |
| `automatic_loop.eps5` | `box1[1,2,2,1]` | -1 | 41 | 41 | yes |  |
| `automatic_loop.eps5` | `box1[1,2,2,1]` | 0 | 42 | 41 | yes |  |
| `automatic_loop.eps5` | `box1[1,2,2,1]` | 1 | 41 | 41 | yes |  |
| `automatic_loop.eps5` | `box1[1,2,2,1]` | 2 | 41 | 41 | yes |  |
| `automatic_loop.eps5` | `box1[1,2,2,1]` | 3 | 42 | 41 | yes |  |
| `automatic_loop.eps5` | `box1[1,2,2,1]` | 4 | 41 | 41 | yes |  |
| `automatic_loop.eps5` | `box1[1,2,2,1]` | 5 | 42 | 41 | yes |  |
| `automatic_loop.eps5` | `box1[2,0,1,0]` | -1 | 999 | 999 | yes |  |
| `automatic_loop.eps5` | `box1[2,0,1,0]` | 0 | 41 | 41 | yes |  |
| `automatic_loop.eps5` | `box1[2,0,1,0]` | 1 | 41 | 41 | yes |  |
| `automatic_loop.eps5` | `box1[2,0,1,0]` | 2 | 41 | 42 | yes |  |
| `automatic_loop.eps5` | `box1[2,0,1,0]` | 3 | 41 | 41 | yes |  |
| `automatic_loop.eps5` | `box1[2,0,1,0]` | 4 | 41 | 41 | yes |  |
| `automatic_loop.eps5` | `box1[2,0,1,0]` | 5 | 42 | 41 | yes |  |
| `automatic_loop.eps5` | `box2[0,1,0,1]` | -1 | 999 | 999 | yes |  |
| `automatic_loop.eps5` | `box2[0,1,0,1]` | 0 | 41 | 999 | yes |  |
| `automatic_loop.eps5` | `box2[0,1,0,1]` | 1 | 41 | 999 | yes |  |
| `automatic_loop.eps5` | `box2[0,1,0,1]` | 2 | 41 | 999 | yes |  |
| `automatic_loop.eps5` | `box2[0,1,0,1]` | 3 | 41 | 999 | yes |  |
| `automatic_loop.eps5` | `box2[0,1,0,1]` | 4 | 41 | 999 | yes |  |
| `automatic_loop.eps5` | `box2[0,1,0,1]` | 5 | 41 | 999 | yes |  |
| `automatic_loop.eps5` | `box2[1,-1,1,0]` | -1 | 999 | 999 | yes |  |
| `automatic_loop.eps5` | `box2[1,-1,1,0]` | 0 | 41 | 41 | yes |  |
| `automatic_loop.eps5` | `box2[1,-1,1,0]` | 1 | 41 | 41 | yes |  |
| `automatic_loop.eps5` | `box2[1,-1,1,0]` | 2 | 41 | 41 | yes |  |
| `automatic_loop.eps5` | `box2[1,-1,1,0]` | 3 | 41 | 41 | yes |  |
| `automatic_loop.eps5` | `box2[1,-1,1,0]` | 4 | 41 | 41 | yes |  |
| `automatic_loop.eps5` | `box2[1,-1,1,0]` | 5 | 41 | 42 | yes |  |
| `automatic_loop.eps5` | `box2[1,0,1,0]` | -1 | 999 | 999 | yes |  |
| `automatic_loop.eps5` | `box2[1,0,1,0]` | 0 | 41 | 41 | yes |  |
| `automatic_loop.eps5` | `box2[1,0,1,0]` | 1 | 41 | 41 | yes |  |
| `automatic_loop.eps5` | `box2[1,0,1,0]` | 2 | 41 | 41 | yes |  |
| `automatic_loop.eps5` | `box2[1,0,1,0]` | 3 | 42 | 41 | yes |  |
| `automatic_loop.eps5` | `box2[1,0,1,0]` | 4 | 41 | 41 | yes |  |
| `automatic_loop.eps5` | `box2[1,0,1,0]` | 5 | 41 | 41 | yes |  |
| `automatic_loop.eps5` | `box2[1,1,0,1]` | -2 | 999 | 999 | yes |  |
| `automatic_loop.eps5` | `box2[1,1,0,1]` | -1 | 41 | 999 | yes |  |
| `automatic_loop.eps5` | `box2[1,1,0,1]` | 0 | 41 | 999 | yes |  |
| `automatic_loop.eps5` | `box2[1,1,0,1]` | 1 | 41 | 999 | yes |  |
| `automatic_loop.eps5` | `box2[1,1,0,1]` | 2 | 41 | 999 | yes |  |
| `automatic_loop.eps5` | `box2[1,1,0,1]` | 3 | 42 | 999 | yes |  |
| `automatic_loop.eps5` | `box2[1,1,0,1]` | 4 | 42 | 999 | yes |  |
| `automatic_loop.eps5` | `box2[1,1,0,1]` | 5 | 41 | 999 | yes |  |
| `automatic_loop.eps5` | `box2[1,1,1,1]` | -2 | 999 | 999 | yes |  |
| `automatic_loop.eps5` | `box2[1,1,1,1]` | -1 | 41 | 41 | yes |  |
| `automatic_loop.eps5` | `box2[1,1,1,1]` | 0 | 41 | 41 | yes |  |
| `automatic_loop.eps5` | `box2[1,1,1,1]` | 1 | 41 | 41 | yes |  |
| `automatic_loop.eps5` | `box2[1,1,1,1]` | 2 | 41 | 41 | yes |  |
| `automatic_loop.eps5` | `box2[1,1,1,1]` | 3 | 41 | 41 | yes |  |
| `automatic_loop.eps5` | `box2[1,1,1,1]` | 4 | 41 | 41 | yes |  |
| `automatic_loop.eps5` | `box2[1,1,1,1]` | 5 | 41 | 41 | yes |  |
| `automatic_loop.eps5` | `box2[2,1,1,1]` | -2 | 999 | 999 | yes |  |
| `automatic_loop.eps5` | `box2[2,1,1,1]` | -1 | 41 | 41 | yes |  |
| `automatic_loop.eps5` | `box2[2,1,1,1]` | 0 | 41 | 41 | yes |  |
| `automatic_loop.eps5` | `box2[2,1,1,1]` | 1 | 41 | 41 | yes |  |
| `automatic_loop.eps5` | `box2[2,1,1,1]` | 2 | 41 | 41 | yes |  |
| `automatic_loop.eps5` | `box2[2,1,1,1]` | 3 | 42 | 41 | yes |  |
| `automatic_loop.eps5` | `box2[2,1,1,1]` | 4 | 42 | 41 | yes |  |
| `automatic_loop.eps5` | `box2[2,1,1,1]` | 5 | 41 | 41 | yes |  |
| `automatic_loop.eps6` | `box1[-2,1,1,2]` | -2 | 999 | 999 | yes |  |
| `automatic_loop.eps6` | `box1[-2,1,1,2]` | -1 | 41 | 999 | yes |  |
| `automatic_loop.eps6` | `box1[-2,1,1,2]` | 0 | 41 | 999 | yes |  |
| `automatic_loop.eps6` | `box1[-2,1,1,2]` | 1 | 41 | 999 | yes |  |
| `automatic_loop.eps6` | `box1[-2,1,1,2]` | 2 | 41 | 999 | yes |  |
| `automatic_loop.eps6` | `box1[-2,1,1,2]` | 3 | 41 | 999 | yes |  |
| `automatic_loop.eps6` | `box1[-2,1,1,2]` | 4 | 41 | 999 | yes |  |
| `automatic_loop.eps6` | `box1[-2,1,1,2]` | 5 | 41 | 999 | yes |  |
| `automatic_loop.eps6` | `box1[-2,1,1,2]` | 6 | 14 | 999 | no | box1[-2,1,1,2] eps^6 has real/imag agreement 14/999, below tolerance 30 |
| `automatic_loop.eps6` | `box1[0,1,0,1]` | -1 | 999 | 999 | yes |  |
| `automatic_loop.eps6` | `box1[0,1,0,1]` | 0 | 41 | 999 | yes |  |
| `automatic_loop.eps6` | `box1[0,1,0,1]` | 1 | 41 | 999 | yes |  |
| `automatic_loop.eps6` | `box1[0,1,0,1]` | 2 | 41 | 999 | yes |  |
| `automatic_loop.eps6` | `box1[0,1,0,1]` | 3 | 41 | 999 | yes |  |
| `automatic_loop.eps6` | `box1[0,1,0,1]` | 4 | 41 | 999 | yes |  |
| `automatic_loop.eps6` | `box1[0,1,0,1]` | 5 | 41 | 999 | yes |  |
| `automatic_loop.eps6` | `box1[0,1,0,1]` | 6 | 41 | 999 | yes |  |
| `automatic_loop.eps6` | `box1[1,0,1,0]` | -1 | 999 | 999 | yes |  |
| `automatic_loop.eps6` | `box1[1,0,1,0]` | 0 | 41 | 41 | yes |  |
| `automatic_loop.eps6` | `box1[1,0,1,0]` | 1 | 41 | 41 | yes |  |
| `automatic_loop.eps6` | `box1[1,0,1,0]` | 2 | 41 | 41 | yes |  |
| `automatic_loop.eps6` | `box1[1,0,1,0]` | 3 | 42 | 41 | yes |  |
| `automatic_loop.eps6` | `box1[1,0,1,0]` | 4 | 41 | 41 | yes |  |
| `automatic_loop.eps6` | `box1[1,0,1,0]` | 5 | 41 | 41 | yes |  |
| `automatic_loop.eps6` | `box1[1,0,1,0]` | 6 | 41 | 41 | yes |  |
| `automatic_loop.eps6` | `box1[1,1,1,1]` | -2 | 999 | 999 | yes |  |
| `automatic_loop.eps6` | `box1[1,1,1,1]` | -1 | 41 | 41 | yes |  |
| `automatic_loop.eps6` | `box1[1,1,1,1]` | 0 | 41 | 41 | yes |  |
| `automatic_loop.eps6` | `box1[1,1,1,1]` | 1 | 41 | 41 | yes |  |
| `automatic_loop.eps6` | `box1[1,1,1,1]` | 2 | 41 | 41 | yes |  |
| `automatic_loop.eps6` | `box1[1,1,1,1]` | 3 | 41 | 41 | yes |  |
| `automatic_loop.eps6` | `box1[1,1,1,1]` | 4 | 41 | 41 | yes |  |
| `automatic_loop.eps6` | `box1[1,1,1,1]` | 5 | 41 | 41 | yes |  |
| `automatic_loop.eps6` | `box1[1,1,1,1]` | 6 | 41 | 41 | yes |  |
| `automatic_loop.eps6` | `box1[1,2,2,1]` | -2 | 58 | 999 | yes |  |
| `automatic_loop.eps6` | `box1[1,2,2,1]` | -1 | 41 | 41 | yes |  |
| `automatic_loop.eps6` | `box1[1,2,2,1]` | 0 | 42 | 41 | yes |  |
| `automatic_loop.eps6` | `box1[1,2,2,1]` | 1 | 41 | 41 | yes |  |
| `automatic_loop.eps6` | `box1[1,2,2,1]` | 2 | 41 | 41 | yes |  |
| `automatic_loop.eps6` | `box1[1,2,2,1]` | 3 | 42 | 41 | yes |  |
| `automatic_loop.eps6` | `box1[1,2,2,1]` | 4 | 41 | 41 | yes |  |
| `automatic_loop.eps6` | `box1[1,2,2,1]` | 5 | 42 | 41 | yes |  |
| `automatic_loop.eps6` | `box1[1,2,2,1]` | 6 | 42 | 41 | yes |  |
| `automatic_loop.eps6` | `box1[2,0,1,0]` | -1 | 999 | 999 | yes |  |
| `automatic_loop.eps6` | `box1[2,0,1,0]` | 0 | 41 | 41 | yes |  |
| `automatic_loop.eps6` | `box1[2,0,1,0]` | 1 | 41 | 41 | yes |  |
| `automatic_loop.eps6` | `box1[2,0,1,0]` | 2 | 41 | 42 | yes |  |
| `automatic_loop.eps6` | `box1[2,0,1,0]` | 3 | 41 | 41 | yes |  |
| `automatic_loop.eps6` | `box1[2,0,1,0]` | 4 | 41 | 41 | yes |  |
| `automatic_loop.eps6` | `box1[2,0,1,0]` | 5 | 42 | 41 | yes |  |
| `automatic_loop.eps6` | `box1[2,0,1,0]` | 6 | 41 | 41 | yes |  |
| `automatic_loop.eps6` | `box2[0,1,0,1]` | -1 | 999 | 999 | yes |  |
| `automatic_loop.eps6` | `box2[0,1,0,1]` | 0 | 41 | 999 | yes |  |
| `automatic_loop.eps6` | `box2[0,1,0,1]` | 1 | 41 | 999 | yes |  |
| `automatic_loop.eps6` | `box2[0,1,0,1]` | 2 | 41 | 999 | yes |  |
| `automatic_loop.eps6` | `box2[0,1,0,1]` | 3 | 41 | 999 | yes |  |
| `automatic_loop.eps6` | `box2[0,1,0,1]` | 4 | 41 | 999 | yes |  |
| `automatic_loop.eps6` | `box2[0,1,0,1]` | 5 | 41 | 999 | yes |  |
| `automatic_loop.eps6` | `box2[0,1,0,1]` | 6 | 41 | 999 | yes |  |
| `automatic_loop.eps6` | `box2[1,-1,1,0]` | -1 | 999 | 999 | yes |  |
| `automatic_loop.eps6` | `box2[1,-1,1,0]` | 0 | 41 | 41 | yes |  |
| `automatic_loop.eps6` | `box2[1,-1,1,0]` | 1 | 41 | 41 | yes |  |
| `automatic_loop.eps6` | `box2[1,-1,1,0]` | 2 | 41 | 41 | yes |  |
| `automatic_loop.eps6` | `box2[1,-1,1,0]` | 3 | 41 | 41 | yes |  |
| `automatic_loop.eps6` | `box2[1,-1,1,0]` | 4 | 41 | 41 | yes |  |
| `automatic_loop.eps6` | `box2[1,-1,1,0]` | 5 | 41 | 42 | yes |  |
| `automatic_loop.eps6` | `box2[1,-1,1,0]` | 6 | 41 | 41 | yes |  |
| `automatic_loop.eps6` | `box2[1,0,1,0]` | -1 | 999 | 999 | yes |  |
| `automatic_loop.eps6` | `box2[1,0,1,0]` | 0 | 41 | 41 | yes |  |
| `automatic_loop.eps6` | `box2[1,0,1,0]` | 1 | 41 | 41 | yes |  |
| `automatic_loop.eps6` | `box2[1,0,1,0]` | 2 | 41 | 41 | yes |  |
| `automatic_loop.eps6` | `box2[1,0,1,0]` | 3 | 42 | 41 | yes |  |
| `automatic_loop.eps6` | `box2[1,0,1,0]` | 4 | 41 | 41 | yes |  |
| `automatic_loop.eps6` | `box2[1,0,1,0]` | 5 | 41 | 41 | yes |  |
| `automatic_loop.eps6` | `box2[1,0,1,0]` | 6 | 41 | 41 | yes |  |
| `automatic_loop.eps6` | `box2[1,1,0,1]` | -2 | 999 | 999 | yes |  |
| `automatic_loop.eps6` | `box2[1,1,0,1]` | -1 | 41 | 999 | yes |  |
| `automatic_loop.eps6` | `box2[1,1,0,1]` | 0 | 41 | 999 | yes |  |
| `automatic_loop.eps6` | `box2[1,1,0,1]` | 1 | 41 | 999 | yes |  |
| `automatic_loop.eps6` | `box2[1,1,0,1]` | 2 | 41 | 999 | yes |  |
| `automatic_loop.eps6` | `box2[1,1,0,1]` | 3 | 42 | 999 | yes |  |
| `automatic_loop.eps6` | `box2[1,1,0,1]` | 4 | 42 | 999 | yes |  |
| `automatic_loop.eps6` | `box2[1,1,0,1]` | 5 | 41 | 999 | yes |  |
| `automatic_loop.eps6` | `box2[1,1,0,1]` | 6 | 18 | 999 | no | box2[1,1,0,1] eps^6 has real/imag agreement 18/999, below tolerance 30 |
| `automatic_loop.eps6` | `box2[1,1,1,1]` | -2 | 999 | 999 | yes |  |
| `automatic_loop.eps6` | `box2[1,1,1,1]` | -1 | 41 | 41 | yes |  |
| `automatic_loop.eps6` | `box2[1,1,1,1]` | 0 | 41 | 41 | yes |  |
| `automatic_loop.eps6` | `box2[1,1,1,1]` | 1 | 41 | 41 | yes |  |
| `automatic_loop.eps6` | `box2[1,1,1,1]` | 2 | 41 | 41 | yes |  |
| `automatic_loop.eps6` | `box2[1,1,1,1]` | 3 | 41 | 41 | yes |  |
| `automatic_loop.eps6` | `box2[1,1,1,1]` | 4 | 41 | 41 | yes |  |
| `automatic_loop.eps6` | `box2[1,1,1,1]` | 5 | 41 | 41 | yes |  |
| `automatic_loop.eps6` | `box2[1,1,1,1]` | 6 | 41 | 41 | yes |  |
| `automatic_loop.eps6` | `box2[2,1,1,1]` | -2 | 999 | 999 | yes |  |
| `automatic_loop.eps6` | `box2[2,1,1,1]` | -1 | 41 | 41 | yes |  |
| `automatic_loop.eps6` | `box2[2,1,1,1]` | 0 | 41 | 41 | yes |  |
| `automatic_loop.eps6` | `box2[2,1,1,1]` | 1 | 41 | 41 | yes |  |
| `automatic_loop.eps6` | `box2[2,1,1,1]` | 2 | 41 | 41 | yes |  |
| `automatic_loop.eps6` | `box2[2,1,1,1]` | 3 | 42 | 41 | yes |  |
| `automatic_loop.eps6` | `box2[2,1,1,1]` | 4 | 42 | 41 | yes |  |
| `automatic_loop.eps6` | `box2[2,1,1,1]` | 5 | 41 | 41 | yes |  |
| `automatic_loop.eps6` | `box2[2,1,1,1]` | 6 | 41 | 42 | yes |  |
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
| `differential_equation_solver` | `sunrise[1,1,1,-1,0]` | 0 | 41 | 41 | yes |  |
| `differential_equation_solver` | `sunrise[1,1,1,0,0]` | 0 | 41 | 41 | yes |  |
| `differential_equation_solver` | `sunrise[2,1,1,0,0]` | 0 | 42 | 41 | yes |  |
| `differential_equation_solver.sol2` | `sunrise[1,1,1,-1,0]` | 0 | 31 | 30 | yes |  |
| `differential_equation_solver.sol2` | `sunrise[1,1,1,0,0]` | 0 | 31 | 31 | yes |  |
| `differential_equation_solver.sol2` | `sunrise[2,1,1,0,0]` | 0 | 30 | 31 | yes |  |
| `feynman_prescription` | `loopxloop[0,0,1,1,0,0,1,1,1,1,0,0]` | -2 | 40 | 999 | yes |  |
| `feynman_prescription` | `loopxloop[0,0,1,1,0,0,1,1,1,1,0,0]` | -1 | 39 | 999 | yes |  |
| `feynman_prescription` | `loopxloop[0,0,1,1,0,0,1,1,1,1,0,0]` | 0 | 39 | 999 | yes |  |
| `feynman_prescription` | `loopxloop[0,0,1,1,0,0,1,1,1,1,0,0]` | 1 | 39 | 999 | yes |  |
| `feynman_prescription` | `loopxloop[0,0,1,1,0,0,1,1,1,1,0,0]` | 2 | 40 | 999 | yes |  |
| `feynman_prescription` | `loopxloop[0,0,1,1,1,0,0,1,1,1,0,0]` | -2 | 40 | 999 | yes |  |
| `feynman_prescription` | `loopxloop[0,0,1,1,1,0,0,1,1,1,0,0]` | -1 | 39 | 999 | yes |  |
| `feynman_prescription` | `loopxloop[0,0,1,1,1,0,0,1,1,1,0,0]` | 0 | 39 | 999 | yes |  |
| `feynman_prescription` | `loopxloop[0,0,1,1,1,0,0,1,1,1,0,0]` | 1 | 39 | 999 | yes |  |
| `feynman_prescription` | `loopxloop[0,0,1,1,1,0,0,1,1,1,0,0]` | 2 | 39 | 999 | yes |  |
| `feynman_prescription` | `loopxloop[0,0,1,1,1,0,1,0,1,1,0,0]` | -2 | 40 | 999 | yes |  |
| `feynman_prescription` | `loopxloop[0,0,1,1,1,0,1,0,1,1,0,0]` | -1 | 39 | 999 | yes |  |
| `feynman_prescription` | `loopxloop[0,0,1,1,1,0,1,0,1,1,0,0]` | 0 | 39 | 999 | yes |  |
| `feynman_prescription` | `loopxloop[0,0,1,1,1,0,1,0,1,1,0,0]` | 1 | 39 | 999 | yes |  |
| `feynman_prescription` | `loopxloop[0,0,1,1,1,0,1,0,1,1,0,0]` | 2 | 40 | 999 | yes |  |
| `feynman_prescription` | `loopxloop[0,0,1,1,1,0,1,1,1,1,0,0]` | -1 | 39 | 999 | yes |  |
| `feynman_prescription` | `loopxloop[0,0,1,1,1,0,1,1,1,1,0,0]` | 0 | 39 | 999 | yes |  |
| `feynman_prescription` | `loopxloop[0,0,1,1,1,0,1,1,1,1,0,0]` | 1 | 39 | 999 | yes |  |
| `feynman_prescription` | `loopxloop[0,0,1,1,1,0,1,1,1,1,0,0]` | 2 | 39 | 999 | yes |  |
| `feynman_prescription` | `loopxloop[0,1,-1,1,0,0,1,1,1,1,0,0]` | -2 | 39 | 999 | yes |  |
| `feynman_prescription` | `loopxloop[0,1,-1,1,0,0,1,1,1,1,0,0]` | -1 | 39 | 39 | yes |  |
| `feynman_prescription` | `loopxloop[0,1,-1,1,0,0,1,1,1,1,0,0]` | 0 | 39 | 39 | yes |  |
| `feynman_prescription` | `loopxloop[0,1,-1,1,0,0,1,1,1,1,0,0]` | 1 | 39 | 39 | yes |  |
| `feynman_prescription` | `loopxloop[0,1,-1,1,0,0,1,1,1,1,0,0]` | 2 | 39 | 40 | yes |  |
| `feynman_prescription` | `loopxloop[0,1,-1,1,1,0,0,1,1,1,0,0]` | -2 | 39 | 999 | yes |  |
| `feynman_prescription` | `loopxloop[0,1,-1,1,1,0,0,1,1,1,0,0]` | -1 | 40 | 39 | yes |  |
| `feynman_prescription` | `loopxloop[0,1,-1,1,1,0,0,1,1,1,0,0]` | 0 | 39 | 39 | yes |  |
| `feynman_prescription` | `loopxloop[0,1,-1,1,1,0,0,1,1,1,0,0]` | 1 | 39 | 39 | yes |  |
| `feynman_prescription` | `loopxloop[0,1,-1,1,1,0,0,1,1,1,0,0]` | 2 | 39 | 40 | yes |  |
| `feynman_prescription` | `loopxloop[0,1,-1,1,1,0,1,0,1,1,0,0]` | -2 | 39 | 999 | yes |  |
| `feynman_prescription` | `loopxloop[0,1,-1,1,1,0,1,0,1,1,0,0]` | -1 | 39 | 39 | yes |  |
| `feynman_prescription` | `loopxloop[0,1,-1,1,1,0,1,0,1,1,0,0]` | 0 | 39 | 39 | yes |  |
| `feynman_prescription` | `loopxloop[0,1,-1,1,1,0,1,0,1,1,0,0]` | 1 | 39 | 39 | yes |  |
| `feynman_prescription` | `loopxloop[0,1,-1,1,1,0,1,0,1,1,0,0]` | 2 | 39 | 39 | yes |  |
| `feynman_prescription` | `loopxloop[0,1,-1,1,1,0,1,1,1,1,0,0]` | -1 | 39 | 999 | yes |  |
| `feynman_prescription` | `loopxloop[0,1,-1,1,1,0,1,1,1,1,0,0]` | 0 | 40 | 40 | yes |  |
| `feynman_prescription` | `loopxloop[0,1,-1,1,1,0,1,1,1,1,0,0]` | 1 | 40 | 39 | yes |  |
| `feynman_prescription` | `loopxloop[0,1,-1,1,1,0,1,1,1,1,0,0]` | 2 | 39 | 39 | yes |  |
| `feynman_prescription` | `loopxloop[0,1,0,1,0,0,1,1,1,1,0,0]` | -2 | 40 | 999 | yes |  |
| `feynman_prescription` | `loopxloop[0,1,0,1,0,0,1,1,1,1,0,0]` | -1 | 39 | 39 | yes |  |
| `feynman_prescription` | `loopxloop[0,1,0,1,0,0,1,1,1,1,0,0]` | 0 | 40 | 39 | yes |  |
| `feynman_prescription` | `loopxloop[0,1,0,1,0,0,1,1,1,1,0,0]` | 1 | 40 | 40 | yes |  |
| `feynman_prescription` | `loopxloop[0,1,0,1,0,0,1,1,1,1,0,0]` | 2 | 39 | 39 | yes |  |
| `feynman_prescription` | `loopxloop[0,1,0,1,1,0,0,1,1,1,0,0]` | -2 | 40 | 999 | yes |  |
| `feynman_prescription` | `loopxloop[0,1,0,1,1,0,0,1,1,1,0,0]` | -1 | 39 | 39 | yes |  |
| `feynman_prescription` | `loopxloop[0,1,0,1,1,0,0,1,1,1,0,0]` | 0 | 39 | 39 | yes |  |
| `feynman_prescription` | `loopxloop[0,1,0,1,1,0,0,1,1,1,0,0]` | 1 | 40 | 39 | yes |  |
| `feynman_prescription` | `loopxloop[0,1,0,1,1,0,0,1,1,1,0,0]` | 2 | 39 | 39 | yes |  |
| `feynman_prescription` | `loopxloop[0,1,0,1,1,0,1,0,1,1,0,0]` | -2 | 40 | 999 | yes |  |
| `feynman_prescription` | `loopxloop[0,1,0,1,1,0,1,0,1,1,0,0]` | -1 | 39 | 39 | yes |  |
| `feynman_prescription` | `loopxloop[0,1,0,1,1,0,1,0,1,1,0,0]` | 0 | 39 | 39 | yes |  |
| `feynman_prescription` | `loopxloop[0,1,0,1,1,0,1,0,1,1,0,0]` | 1 | 39 | 40 | yes |  |
| `feynman_prescription` | `loopxloop[0,1,0,1,1,0,1,0,1,1,0,0]` | 2 | 40 | 40 | yes |  |
| `feynman_prescription` | `loopxloop[0,1,0,1,1,0,1,1,1,1,0,0]` | -1 | 39 | 999 | yes |  |
| `feynman_prescription` | `loopxloop[0,1,0,1,1,0,1,1,1,1,0,0]` | 0 | 39 | 39 | yes |  |
| `feynman_prescription` | `loopxloop[0,1,0,1,1,0,1,1,1,1,0,0]` | 1 | 39 | 39 | yes |  |
| `feynman_prescription` | `loopxloop[0,1,0,1,1,0,1,1,1,1,0,0]` | 2 | 39 | 39 | yes |  |
| `feynman_prescription` | `loopxloop[0,1,1,1,0,0,1,1,1,1,0,0]` | -2 | 39 | 39 | yes |  |
| `feynman_prescription` | `loopxloop[0,1,1,1,0,0,1,1,1,1,0,0]` | -1 | 39 | 39 | yes |  |
| `feynman_prescription` | `loopxloop[0,1,1,1,0,0,1,1,1,1,0,0]` | 0 | 39 | 39 | yes |  |
| `feynman_prescription` | `loopxloop[0,1,1,1,0,0,1,1,1,1,0,0]` | 1 | 39 | 39 | yes |  |
| `feynman_prescription` | `loopxloop[0,1,1,1,0,0,1,1,1,1,0,0]` | 2 | 39 | 40 | yes |  |
| `feynman_prescription` | `loopxloop[0,1,1,1,1,0,0,1,1,1,0,0]` | -2 | 39 | 39 | yes |  |
| `feynman_prescription` | `loopxloop[0,1,1,1,1,0,0,1,1,1,0,0]` | -1 | 40 | 999 | yes |  |
| `feynman_prescription` | `loopxloop[0,1,1,1,1,0,0,1,1,1,0,0]` | 0 | 40 | 39 | yes |  |
| `feynman_prescription` | `loopxloop[0,1,1,1,1,0,0,1,1,1,0,0]` | 1 | 39 | 39 | yes |  |
| `feynman_prescription` | `loopxloop[0,1,1,1,1,0,0,1,1,1,0,0]` | 2 | 39 | 999 | yes |  |
| `feynman_prescription` | `loopxloop[0,1,1,1,1,0,1,0,1,1,0,0]` | -2 | 39 | 39 | yes |  |
| `feynman_prescription` | `loopxloop[0,1,1,1,1,0,1,0,1,1,0,0]` | -1 | 40 | 39 | yes |  |
| `feynman_prescription` | `loopxloop[0,1,1,1,1,0,1,0,1,1,0,0]` | 0 | 40 | 39 | yes |  |
| `feynman_prescription` | `loopxloop[0,1,1,1,1,0,1,0,1,1,0,0]` | 1 | 39 | 39 | yes |  |
| `feynman_prescription` | `loopxloop[0,1,1,1,1,0,1,0,1,1,0,0]` | 2 | 39 | 39 | yes |  |
| `feynman_prescription` | `loopxloop[0,1,1,1,1,0,1,1,1,1,0,0]` | -1 | 39 | 40 | yes |  |
| `feynman_prescription` | `loopxloop[0,1,1,1,1,0,1,1,1,1,0,0]` | 0 | 40 | 40 | yes |  |
| `feynman_prescription` | `loopxloop[0,1,1,1,1,0,1,1,1,1,0,0]` | 1 | 40 | 39 | yes |  |
| `feynman_prescription` | `loopxloop[0,1,1,1,1,0,1,1,1,1,0,0]` | 2 | 39 | 39 | yes |  |
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

## M5/M6 Status Impact

- This lane improves the retained phase-0 packet aggregate to `669/671`, and confirms `automatic_loop.eps5` passes `90/90` while `automatic_loop.eps6` remains partial at `100/102`.
- The default `differential_equation_solver` sol1 surface now passes `3/3` in the all-manifest sweep, while the sol2 transport surface remains passing at `3/3`.
- M5 remains PARTIAL/FAILING because two eps6 coefficients miss the 30-digit threshold and because the broader Phase F live-path gates for phase-space, prescription, D0, user hooks, and singular coverage are still not closed by retained-state packet parity alone.
- M6 remains TODO/FAILING because it depends on M5 and still lacks passing packet-shaped candidate roots, case-study numeric sidecars at the frozen digit thresholds, and singular-endpoint qualification closure.

## Multi-Agent Review

- Role A APPROVE: verified the 13-file phase-0 `*golden-manifest.json` inventory matches the aggregate JSON and report exactly, including `automatic_loop.eps2` through `automatic_loop.eps6`; per-benchmark rows sum to `669/671`, and latest-per-benchmark recomputes to `364/366`.
- Role B APPROVE: verified the fresh build gate is recorded as exit `0`, all 13 solve commands use `--digits 40`, all 13 comparator commands and compare JSONs use `--tolerance-digits 30`, all solve exits are `0`, and the single compare exit `1` is the expected `automatic_loop.eps6` `100/102` failure.
- Role C APPROVE: verified `automatic_loop.eps6` has exactly two eps^6 failures, `box1[-2,1,1,2]` at `14/999` and `box2[1,1,0,1]` at `18/999`, while every other manifest surface passes; also verified the M5/M6 closure plan still rejects M5 and M6 closure.
- Role D APPROVE: verified `docs/phase0-packet-validation-lane32.md` and `/tmp/autoibp_orch/exec/lane32_aggregate_v3.md` are byte-identical after the report sync, the latest-per-benchmark line matches the JSON, and the aggregate JSON is internally consistent at `669/671` over 13 manifest surfaces.
