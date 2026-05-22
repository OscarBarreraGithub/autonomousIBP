# Lane 6 Iteration 13 B61n Pole-Aware RK78 ODE Ordering Audit

Status: Tier C diagnostic. This sidecar does not flip M6, does not promote
`complex_kinematics`, and does not set `full_eta_zero_contour_applied=true`.

## Step Chosen

This lane reran the stripped b61n `complex_kinematics` comparator through the
pole-aware RK78 path landed by lane6 iteration 12, then audited the AMFlow
`diffeq` matrix ordering because the comparator stayed low.

The first plain `phase0/complex_kinematics.amflow-state.json` solve was rejected
as evidence because its JSON reports `transport_applied=false` and
`runtime_application="loop-solution-sample-laurent-fit"`. The qualifying run
below uses the stripped state from the previous RK78 diagnostic, which removes
final AMFlow solution samples and exercises the live coupled-row contour path.

## Commands

```text
timeout 120s ./build/amflow-cli solve-series \
  /tmp/autoibp_orch/exec/lane2_iter9/complex_kinematics.stripped-state.json \
  --eps-order 0 --digits 80 \
  --out tools/reference-harness/specs/m6/lane6-iter13/complex_kinematics.rk78-pole-aware.stripped.eps0.cpp-result.json

python3 tools/reference-harness/scripts/compare_cpp_vs_amflow.py \
  --cpp-result tools/reference-harness/specs/m6/lane6-iter13/complex_kinematics.rk78-pole-aware.stripped.eps0.cpp-result.json \
  --amflow-golden tools/reference-harness/specs/phase0/complex_kinematics.golden-manifest.json \
  --tolerance-digits 50 \
  > tools/reference-harness/specs/m6/lane6-iter13/complex_kinematics.rk78-pole-aware.stripped.eps0.compare50.json
```

## Comparator Result

- comparator passed: false
- compared coefficients: 14
- passed coefficients: 10
- minimum digit agreement: 2
- M6 flipped: false

The failures are unchanged in scope: `box[1,0,1,1] eps^0` and
`box[1,1,1,1] eps^-2..0`.

The RK78 path did exercise the lane6-next12 pole forwarding:

```text
endpoint_refinement_integrator=fehlberg-rk78-adaptive
finite_start_selection=closer-certified-eta-infinity-start
closer_start_candidate_count=3
closer_start_certified=true
max_refinement_error_abs=160.382296887759612163717
max_embedded_error_abs=1.2436659346586904296054
pole_pinch_step_count_max=12
contour_pole_count=6
contour_poles_forwarded_to_propagator=true
ode_propagation_applied=true
coefficient_publication=false
final_solution_samples_used_as_input=false
full_eta_zero_contour_applied=false
```

Compared with lane176, forwarding the extracted poles reduced the reported
refinement error from about `5.5681e4` to about `1.6038e2`, but not enough to
publish coupled-row coefficients or move the comparator floor above 2 digits.
The production runtime still stops at the first certified closer start after two
precision-guard rejections; lane176's auxiliary all-candidate diagnostic remains
the evidence that 9 of 11 closer starts certify.

## ODE Matrix Ordering Audit

The retained AMFlow reference files disagree on ordering labels:

```text
masters:
{j[box,0,0,0,1], j[box,1,0,1,0], j[box,1,0,0,1],
 j[box,0,1,0,1], j[box,0,0,1,1], j[box,1,0,1,1],
 j[box,1,1,1,1]}

preferred/globalpreferred:
{j[box,0,0,0,1], j[box,1,0,0,1], j[box,0,1,0,1],
 j[box,1,0,1,0], j[box,0,0,1,1], j[box,1,0,1,1],
 j[box,1,1,1,1]}
```

The C++ state `masters` order matches the AMFlow `masters` file, not
`preferred` or `globalpreferred`. Every cell in the JSON
`coefficient_matrices["eta"]` appears verbatim, after whitespace normalization,
inside the AMFlow Mathematica `diffeq` file:

```text
state_cells_missing_from_amflow_diffeq=0
nonzero_missing=0
```

This audit therefore did not find a direct sign flip or a JSON-vs-`diffeq`
matrix transcription error. It did identify the `masters` versus
`preferred/globalpreferred` ordering split as the next concrete suspect to test:
the live contour path currently treats the `diffeq` matrix, eta-infinity
regions, and propagated vectors in `masters` order. Any retained boundary or
publication data consumed in `preferred/globalpreferred` order would corrupt the
coupled rows without changing the scalar and primitive selected endpoints.

## Artifact Hashes

```text
830604b116ceba87f341ed4d74bed6ddf82a13e5a6cf178efdcbb6763c2c7102  AMFlow cache box_amflow/1/diffeq
d93f386977e0211e2fb98897c2278355177a95a1578bdac7662ccfaada58ffd5  stripped input state
81cbe05bd6a378755df4db71210f5743ac6c9b12355d4daf24b577dd2b5154bd  pole-aware RK78 result
68d074a37bca46f636da8a14bef8b7343fa0cc73320503e335a75854b33619d5  comparator JSON
```

## Next Atomic Step

Add a focused order-parity probe for b61n that evaluates the eta-infinity
boundary regions and the coupled-row contour initial vector under both
`masters` and `preferred/globalpreferred` label maps, then compares the
unpublished coupled-row endpoint samples against AMFlow. That will separate a
true ODE/sign error from a boundary-vector ordering error.

## Four-Role Review

- Role A, implementation: APPROVE Tier C. The run used the stripped state and
  the pole-aware RK78 path; the retained-sample shortcut was explicitly rejected.
- Role B, tests: APPROVE Tier C. The comparator JSON is fresh, fails honestly,
  and records `minimum_digit_agreement=2`.
- Role C, numerics: APPROVE Tier C. Pole forwarding changed RK78 diagnostics but
  not material AMFlow agreement; the coupled rows remain the only failing rows.
- Role D, anti-fake: APPROVE Tier C only. No tolerance was weakened, no final
  AMFlow solution samples were used as input, and M6 remains unflipped.
