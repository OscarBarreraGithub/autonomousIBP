# Lane 172 B61n RK45 Endpoint Refinement Gap

Status: Tier C implementation gap. This sidecar does not flip M6, does not
promote `complex_kinematics`, and does not set
`full_eta_zero_contour_applied=true`.

## Change Tested

The b61n coupled-row endpoint path now calls the adaptive Dormand-Prince RK45
contour propagator directly for the two deferred coupled rows
`box[1,0,1,1]` and `box[1,1,1,1]`. The old caller-side 64-vs-128 endpoint
coarse/fine comparison is no longer used as the selected-row refinement test.

The RK45 call is made from the original certified eta-infinity finite start,
with:

- `refinement_error_tolerance=1e-24`
- `refinement_relative_error_tolerance=1e-20`
- `max_adaptive_steps_per_segment=2048`
- `final_solution_samples_used_as_input=false`

The Lane2 closer-start recurrence path is preserved. On top of the current
`origin/main` it tries 11 closer finite-start candidates, but none certifies;
the coupled-row RK45 propagation therefore falls back to the original certified
eta-infinity finite start.

## Runtime Evidence

Commands rerun after replaying the interrupted patch onto `origin/main` from
`/tmp/autoibp_lane6_next7`:

```text
./build/amflow-cli solve-series /tmp/autoibp_orch/exec/lane6_next6/complex_kinematics.stripped-state.json --eps-order 0 --digits 80 --out /tmp/autoibp_orch/exec/lane6_next7/complex_kinematics.rk45.eps0.cpp-result.json
python3 tools/reference-harness/scripts/compare_cpp_vs_amflow.py --cpp-result /tmp/autoibp_orch/exec/lane6_next7/complex_kinematics.rk45.eps0.cpp-result.json --amflow-golden tools/reference-harness/specs/phase0/complex_kinematics.golden-manifest.json --tolerance-digits 50 > /tmp/autoibp_orch/exec/lane6_next7/complex_kinematics.rk45.eps0.compare50.json
```

Observed status:

- solve status: `0`
- comparator status: `1`
- comparator passed: `false`
- compared coefficients: `14`
- passed coefficients: `10`
- minimum digit agreement: `2`

The stripped state removed `boundary_state.files.solution`, and the runtime
summary reports `final_solution_samples_used_as_input=false`.

## RK45 Diagnostic

The coupled-row RK45 attempt fails closed at epsilon sample 0:

```text
failure_code=refinement-tolerance-failed
integrator=dormand-prince-rk45-adaptive
propagator_summary=b61n complex contour propagation failed closed because adaptive RK45 refinement could not satisfy the effective tolerance: adaptive-step-limit-exceeded
accepted_steps=2048
rejected_steps=36
pole_pinch_step_count=0
max_embedded_error_abs=9945854883.58389447539331171979029290674
finite_start_selection=original-certified-eta-infinity-start
closer_start_candidate_count=11
closer_start_certified=false
closer_start_first_failure=eta=-15822.1833420008545*I failed: eta-infinity initializer could not solve a finite infinity recurrence system
closer_start_last_failure=eta=-16590761720.021888*I failed: eta-infinity initializer could not solve a finite infinity recurrence system
```

No endpoint relative error is claimed after RK45, because no coupled-row endpoint
value is certified by the adaptive propagator before the step budget is
exhausted.

## Comparator Failures

The eps^0 comparison fails only on the two coupled rows:

```text
box[1,0,1,1] eps^0 real/imag agreement: 2/2
box[1,1,1,1] eps^-2 real/imag agreement: 5/6
box[1,1,1,1] eps^-1 real/imag agreement: 4/4
box[1,1,1,1] eps^0 real/imag agreement: 3/4
```

This is not sufficient AMFlow parity for M6, so
`full_eta_zero_contour_applied` stays false.

## Four-Role Review

- Role A, implementer: APPROVE Tier C. The caller is now wired to the RK45
  propagator with real abs/rel tolerances and no legacy caller-side RK4
  coarse/fine pair.
- Role B, test: APPROVE Tier C after rerun. The comparator fails at the
  50-digit floor, and the test expectation follows the bounded RK45 fail-closed
  diagnostic.
- Role C, physics/numerics: APPROVE Tier C. The coupled-row endpoint functional
  is still not certified from eta-infinity to eta=0; no endpoint coefficient
  publication is warranted.
- Role D, anti-fake: APPROVE Tier C. The stripped input uses no AMFlow final
  solution samples, no tolerance was loosened, no self-comparison is used, and
  the full-contour flag remains false.
