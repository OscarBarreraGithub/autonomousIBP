# Lane 6 Iteration 10 B61n RK78 Endpoint Refinement Result

Status: Tier C evidence. This sidecar does not flip M6, does not promote
`complex_kinematics`, and does not set `full_eta_zero_contour_applied=true`.

## Step Chosen

Current `origin/main` already wires the b61n coupled-row endpoint refinement
through `fehlberg-rk78-adaptive` in the live contour propagation path. This
lane kept that wiring and added structured fail-closed refinement diagnostics
so the caller no longer has to recover the observed refinement error, effective
tolerance, endpoint scale, and embedded error by parsing the propagator summary.

The diagnostic-only code step is intentionally non-publishing: it does not
relax tolerances, does not widen the endpoint publication gate, and does not
change the retained AMFlow comparator contract.

## Comparator Evidence

Replay command:

```text
/tmp/autoibp_lane6_iter10_build/amflow-cli solve-series \
  /tmp/autoibp_orch/exec/lane6_next6/complex_kinematics.stripped-state.json \
  --eps-order 0 --digits 80 \
  --out /tmp/autoibp_orch/exec/lane6_iter10/complex_kinematics.rk78.eps0.cpp-result.json
```

Comparator command:

```text
python3 tools/reference-harness/scripts/compare_cpp_vs_amflow.py \
  --cpp-result /tmp/autoibp_orch/exec/lane6_iter10/complex_kinematics.rk78.eps0.cpp-result.json \
  --amflow-golden tools/reference-harness/specs/phase0/complex_kinematics.golden-manifest.json \
  --tolerance-digits 50 \
  > /tmp/autoibp_orch/exec/lane6_iter10/complex_kinematics.rk78.eps0.compare50.json
```

Observed status:

- solve status: `0`
- solve duration: `103.083632` seconds
- comparator status: `1`
- comparator passed: `false`
- compared coefficients: `14`
- passed coefficients: `10`
- minimum digit agreement: `2`

The failing coefficients remain the coupled rows:

```text
box[1,0,1,1] eps^0:  real/imag agreement 2/2
box[1,1,1,1] eps^-2: real/imag agreement 5/6
box[1,1,1,1] eps^-1: real/imag agreement 4/4
box[1,1,1,1] eps^0:  real/imag agreement 3/4
```

## RK78 Diagnostic

The coupled-row RK78 attempt is wired and reaches the high-order propagator, but
fails closed at epsilon sample 0:

```text
failure_code=refinement-tolerance-failed
integrator=fehlberg-rk78-adaptive
refinement_error_abs=55681.02456282516381184745055518851688918
refinement_effective_tolerance_abs=2236.810778904889259074285070443532811295
max_embedded_error_abs=2.160694965273843977307790859506711850119e-06
endpoint_vector_norm_abs=223681077890488925907428.5070443532811295
finite_start_selection=closer-certified-eta-infinity-start
closer_start_candidate_count=3
closer_start_certified=true
closer_start_root_causes={slot-incompatibility:0, divisibility:0, recurrence-depth:0, precision:2}
closer_start_largest_root_cause=precision
closer_start_next_fix_target=improve finite-start precision guard evidence
```

This is not evidence for a clean RK78 order-limited improvement, and it is not a
double-precision ceiling: the contour propagator uses `cpp_dec_float_100`. The
post-rebase closer-start handoff now certifies, but the coupled-row endpoint
still fails the RK78 outer refinement gate by roughly 25x before any coupled-row
coefficient publication is allowed.

## Four-Role Review

- Role A, implementation: APPROVE Tier C. The code change only preserves
  measured RK78 failure diagnostics in structured fields.
- Role B, test: APPROVE Tier C. The singular-runtime test now asserts
  fail-closed refinement diagnostics are populated.
- Role C, numerics: APPROVE Tier C. RK78 does not certify the coupled-row
  endpoint; minimum digit agreement remains 2.
- Role D, anti-fake: APPROVE Tier C. No tolerance was loosened, no AMFlow final
  solution samples are used as inputs, and `full_eta_zero_contour_applied`
  remains false.
