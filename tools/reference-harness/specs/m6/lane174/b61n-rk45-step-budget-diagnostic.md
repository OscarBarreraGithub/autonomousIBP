# Lane 174 B61n RK45 Step-Budget Diagnostic

Status: Tier C diagnostic. This sidecar does not flip M6, does not promote
`complex_kinematics`, and does not set `full_eta_zero_contour_applied=true`.

## Step Chosen

This lane chose option (e): diagnose whether the residual b61n digit gap is
integrator-bound, far-start-bound, or contour-density-bound by probing the
adaptive RK45 step budget on top of lane172's coupled-row wiring.

The production code was not changed. A default budget increase was tested
locally, then reverted because it did not emit a comparable result within a
gateable runtime.

## Baseline RK45 Evidence

The lane172/lane6-next7 retained stripped replay remains the latest comparable
evidence:

```text
cpp-result: /tmp/autoibp_orch/exec/lane6_next7/complex_kinematics.rk45.eps0.cpp-result.json
compare50:  /tmp/autoibp_orch/exec/lane6_next7/complex_kinematics.rk45.eps0.compare50.json
```

Observed comparator status:

- comparator passed: `false`
- compared coefficients: `14`
- passed coefficients: `10`
- minimum digit agreement: `2`

The failing coefficients are still the coupled rows only:

```text
box[1,0,1,1] eps^0:  real/imag agreement 2/2
box[1,1,1,1] eps^-2: real/imag agreement 5/6
box[1,1,1,1] eps^-1: real/imag agreement 4/4
box[1,1,1,1] eps^0:  real/imag agreement 3/4
```

The RK45 diagnostic in that result reports:

```text
finite_start_selection=original-certified-eta-infinity-start
closer_start_candidate_count=11
closer_start_certified=false
failure_code=refinement-tolerance-failed
propagator_summary=... adaptive-step-limit-exceeded
accepted_steps=2048
max_embedded_error_abs=9945854883.58389447539331171979029290674
```

## Step-Budget Probe

Two unlanded local probes were run from the clean lane worktree
`/tmp/autoibp_lane6_next8` after rebuilding `amflow-cli`.

Probe A changed only the coupled-row RK45 cap from `2048` to `4096`, leaving
the lane172 tolerances at `refinement_error_tolerance=1e-24` and
`refinement_relative_error_tolerance=1e-20`.

```text
timeout 90s /tmp/autoibp_lane6_next8_build/amflow-cli solve-series \
  /tmp/autoibp_orch/exec/lane6_next6/complex_kinematics.stripped-state.json \
  --eps-order 0 --digits 80 \
  --out /tmp/autoibp_orch/exec/lane6_next8/complex_kinematics.step4096.eps0.cpp-result.json
```

Result: exit code `124` after 90 seconds. No JSON result was emitted, so no
AMFlow comparator could be run for this cap.

Probe B tightened the coupled-row RK45 tolerances to
`refinement_error_tolerance=1e-30`,
`refinement_relative_error_tolerance=1e-26`, and raised the cap to `32768`.
It was stopped after more than 120 seconds with no JSON result and no stderr.

## Diagnosis

This does not establish the expected `-log(h^order)` convergence trend for an
integrator-bound error. The first larger step-budget probe already fails the
lane time/runtime practicality requirement before producing a comparable
endpoint packet.

The evidence points to the current far-start handoff as the dominant blocker:
all 11 closer finite-start eta-infinity candidates fail to certify, the runtime
falls back to the original certified eta-infinity start, and the RK45 path then
exhausts its adaptive budget before certifying the coupled-row endpoint. Merely
raising the step cap is not an acceptable default M6 fix because it made the
stripped b61n solve non-gateable without yielding new AMFlow digits.

## Honest Status

`MIN_DIGIT_AGREEMENT_AFTER` remains `2`.

`full_eta_zero_contour_applied` remains `false`.

The next substantive lane should target the finite-start certification problem
or a different high-order/stiff-aware contour integrator with a gateable runtime
budget. A flag flip is not justified by this diagnostic.

