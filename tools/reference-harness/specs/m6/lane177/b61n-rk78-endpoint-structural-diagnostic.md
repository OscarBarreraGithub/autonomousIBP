# Lane 177 B61n RK78 Endpoint Structural Diagnostic

Status: Tier C diagnostic. This sidecar does not flip M6, does not promote
`complex_kinematics`, and does not set `full_eta_zero_contour_applied=true`.

## Step Chosen

This lane kept the lane2-next9 owner-pivot recurrence result and instrumented the
existing b61n coupled-row RK78 endpoint refinement with diagnostic-only scale
fields:

- the waypoint where outer step-doubling disagreement is largest;
- the eta location where the embedded RK estimate is largest;
- ODE RHS norm, matrix entry norm, row L1 norm, LU pivot spread, and nearest
  documented eta-pole distance at those locations.

On the rebased lane, the b61n runtime forwards the extracted eta-matrix pole
list as `contour_poles`, and the diagnostics use that same documented pole list
for nearest-pole reporting. The generic `diagnostic_poles` option remains tested
separately so callers can request nearest-pole diagnostics without activating
pole-step limiting.

Replay command:

```text
timeout 180s /tmp/autoibp_lane2_iter10_build/amflow-cli solve-series \
  /tmp/autoibp_orch/exec/lane2_iter9/complex_kinematics.stripped-state.json \
  --eps-order 0 --digits 80 \
  --out /tmp/autoibp_orch/exec/lane2_iter10/complex_kinematics.rk78-ownerpivot-diagnostics.eps0.cpp-result.json
```

Comparator command:

```text
python3 tools/reference-harness/scripts/compare_cpp_vs_amflow.py \
  --cpp-result /tmp/autoibp_orch/exec/lane2_iter10/complex_kinematics.rk78-ownerpivot-diagnostics.eps0.cpp-result.json \
  --amflow-golden tools/reference-harness/specs/phase0/complex_kinematics.golden-manifest.json \
  --tolerance-digits 50 \
  > /tmp/autoibp_orch/exec/lane2_iter10/complex_kinematics.rk78-ownerpivot-diagnostics.eps0.compare50.json
```

## Observed Failure Region

The first certified closer start remains the lane2-next9 owner-pivot start:

```text
finite_start_selection=closer-certified-eta-infinity-start
closer_start_candidate_count=3
closer_start_certified=true
```

With the forwarded contour poles, RK78 reaches eta=0 but the coupled-row
publication path still rejects the endpoint because the selected endpoint
refinement error exceeds the scoped coefficient budget:

```text
selected coupled-row refinement error=160.3822968877596121637165256266063014319
relative error=160.3822968877596121637165256266063014319
refinement_effective_tolerance_abs=2236.81077890488925907096292264081838977
max_embedded_error_abs=1.243665934658690429605395366692614573706
pole_pinch_step_count=12
```

The largest outer step-doubling disagreement is at the endpoint:

```text
refinement_error_peak_segment_index=6
refinement_error_peak_eta=0 + 0*I
refinement_error_peak_waypoint_error_abs=160.3822968877596121637165256266063014319
refinement_error_peak_nearest_pole=1.5 + 1*I
refinement_error_peak_nearest_pole_distance_abs=1.802775637731994646559610633735247973126
```

The largest embedded RK estimate is earlier on the lower-half-plane contour,
still not near a documented pole:

```text
max_embedded_error_segment_index=4
max_embedded_error_eta=0 - 47.16655275976812828164164394263560652196*I
max_embedded_error_nearest_pole=1.5 + 1*I
max_embedded_error_nearest_pole_distance_abs=48.18990355623807113170099920029615603861
```

The failure is therefore not at the documented nonendpoint poles
`-486.442201567+I`, `-41+I`, `-2.0577984327+I`, `-2+I`, `1.5+I`, or `6+I`.
It appears at the eta=0 endpoint extraction/refinement comparison.

## RHS And Matrix Scale

At the refinement-error peak:

```text
refinement_error_peak_rhs_norm_abs=5454014034467065898803.012886092643664433
refinement_error_peak_matrix_max_entry_abs=0.5547001962252126754097979953926809211071
refinement_error_peak_matrix_max_row_l1_abs=0.8027696654036259102168388589962210828884
refinement_error_peak_matrix_min_lu_pivot_abs=0
refinement_error_peak_matrix_pivot_ratio_abs=inf
```

At the embedded-error peak:

```text
max_embedded_error_rhs_norm_abs=2292830061362838135504.171180463336727622
max_embedded_error_matrix_max_entry_abs=0.02075123472353417217368064538208342275793
max_embedded_error_matrix_max_row_l1_abs=0.02118168630805734517194289593522258083214
max_embedded_error_matrix_min_lu_pivot_abs=0
max_embedded_error_matrix_pivot_ratio_abs=inf
```

The matrix entries themselves do not spike near a pole; they stay O(1). The RHS
is enormous because the propagated endpoint vector is enormous, and the sparse
7x7 eta matrix is rank-deficient under LU at both diagnostic locations.

## Root-Cause Hypothesis

This points away from a local pole-crossing or an RK78 local-estimator-only bug.
The embedded error estimate and endpoint step-doubling estimate do not identify
a documented pole crossing, while the selected endpoint coefficient budget still
rejects the coupled rows. The ODE RHS is already O(1e21) near the problematic
contour sections, and the 7x7 matrix reports a zero LU pivot at both
eta=-47.16655275976812828164164394263560652196*I and eta=0.

The honest structural hypothesis is that the coupled-row b61n system is
ill-conditioned or rank-deficient in the sparse seven-master basis near the
endpoint. The next implementation attack should be a block/triangular
normalization or Drinfeld-style basis transformation before endpoint integration,
not higher RK order, higher decimal precision, or longer eta-infinity
propagation distance.

## Comparator Evidence

The AMFlow comparator still fails exactly as lane2-next9 did:

```text
passed=false
minimum_digit_agreement=2
compared_coefficient_count=14
passed_coefficient_count=10
```

Failing coefficients remain the coupled rows:

```text
box[1,0,1,1] eps^0:  real/imag agreement 2/2
box[1,1,1,1] eps^-2: real/imag agreement 5/6
box[1,1,1,1] eps^-1: real/imag agreement 4/4
box[1,1,1,1] eps^0:  real/imag agreement 3/4
```

## Four-Role Review

- Role A, implementation: APPROVE Tier C. The change is diagnostic-only and
  keeps coefficient publication blocked.
- Role B, tests: APPROVE Tier C. Synthetic tests assert refinement peak
  diagnostics, pole forwarding, and the fact that `diagnostic_poles` do not
  activate pole-step limiting.
- Role C, numerics: APPROVE Tier C. The measured peak is eta=0, not a
  documented pole, with huge RHS norm and rank-deficient matrix pivots.
- Role D, anti-fake: APPROVE Tier C only. Comparator remains below the M6 digit
  floor, no AMFlow final solution samples are used as inputs, and
  `full_eta_zero_contour_applied` remains false.

## Honest Status

`FAILURE_REGION_ETA=eta=0 endpoint; embedded peak at eta=-47.16655275976812828164164394263560652196*I`

`FAILURE_ROOT_CAUSE=structural sparse/rank-deficient coupled-row ODE basis near endpoint`

`MIN_DIGIT_AGREEMENT_AFTER=2`

`M6_FLIPPED=false`
