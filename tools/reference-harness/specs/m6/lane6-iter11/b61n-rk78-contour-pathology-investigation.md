# Lane 6 Iteration 11 B61n RK78 Contour Pathology Investigation

Status: Tier C diagnostic. This sidecar does not flip M6, does not promote
`complex_kinematics`, and does not set `full_eta_zero_contour_applied=true`.

## Step Chosen

This lane chose option (d): characterize the eta region behind the lane10 RK78
25x endpoint-refinement failure.

The investigation used the retained lane10/lane6 stripped replay artifacts and a
matrix-only trace of the actual retained `complex_kinematics` eta matrix at the
first epsilon sample. No tolerance was relaxed, no final AMFlow solution samples
were read as inputs, and no endpoint coefficient was published.

## Evidence Inputs

- RK78 result:
  `/tmp/autoibp_orch/exec/lane6_iter10/complex_kinematics.rk78.eps0.cpp-result.json`
- RK78 comparator:
  `/tmp/autoibp_orch/exec/lane6_iter10/complex_kinematics.rk78.eps0.compare50.json`
- stripped state:
  `/tmp/autoibp_orch/exec/lane6_next6/complex_kinematics.stripped-state.json`
- first epsilon sample: `101/6812920690579600`
- RK78 endpoint diagnostic:
  `refinement_error_abs=55681.02456282516381184745055518851688918`,
  `refinement_effective_tolerance_abs=2236.810778904889259074285070443532811295`,
  `endpoint_vector_norm_abs=223681077890488925907428.5070443532811295`,
  `max_embedded_error_abs=2.160694965273843977307790859506711850119e-06`

The failure ratio is therefore `55681.02456 / 2236.81078 = 24.89`. Relative to
the endpoint vector norm, the observed coarse-vs-fine mismatch is about
`2.49e-19`, while the active relative refinement budget is `1e-20`.

## Eta Region Trace

The current coupled-row path certifies a closer eta-infinity start after two
rejected precision-guard candidates. The propagated waypoint chain is:

```text
-253154.9334720136712554724*I
-> -31644.3666840017089068681*I
-> -3955.5458355002136133585*I
-> -988.886458875053403341688*I
-> -247.221614718763350835422*I
-> -1*I
-> -0.0625*I
-> 0
```

The matrix trace evaluated the retained eta matrix `A(eta)` at those points.
Rows 5 and 6 are the deferred coupled rows
`box[1,0,1,1]` and `box[1,1,1,1]`.

| eta | max `|A_ij|` | row 5 sum | row 6 sum | nearest pole distance |
| --- | ---: | ---: | ---: | ---: |
| `-253154.93347201367*I` | `3.95013e-06` | `1.56043e-11` | `3.95013e-06` | far-start |
| `-31644.36668400171*I` | `3.16002e-05` | `9.98849e-10` | `3.16002e-05` | radial |
| `-3955.545835500214*I` | `2.52746e-04` | `6.36033e-08` | `2.52732e-04` | radial |
| `-988.8864588750534*I` | `1.01022e-03` | `9.27486e-07` | `1.00936e-03` | radial |
| `-247.22161471876335*I` | `4.02858e-03` | `7.74812e-06` | `3.97484e-03` | `248.226` to `1.5+I` |
| `-1*I` | `4.00000e-01` | `3.09196e-03` | `2.44903e-02` | `2.5` to `1.5+I` |
| `-0.0625*I` | `5.44016e-01` | `4.63506e-03` | `2.46280e-02` | `1.83818` to `1.5+I` |
| `0` | `5.54700e-01` | `4.75244e-03` | `2.46396e-02` | `1.80278` to `1.5+I` |

This identifies the only material matrix-scale change as the final endpoint
approach, from `eta=-1*I` to `eta=0`, especially the last contour interval
`[-0.0625*I, 0]`. The trace does not show a large denominator-factor blowup:
the largest retained-matrix entry remains below `0.56`, and the coupled-row
absolute row sums remain below `0.025` at the endpoint.

## Pole Proximity Finding

The closest pole to the reviewed lower-half-plane contour is not `-2+I`; it is
`1.5+I`, at distance `sqrt(1.5^2 + 1^2) = 1.8027756377319946` from `eta=0`.
The `-2+I` pole is second nearest at distance `sqrt(5) =
2.23606797749979` from `eta=0`.

Both nearest approaches occur at the eta-zero endpoint, not in the long radial
interior. The current retained matrix trace therefore does not support an
interior contour pole-pinch explanation for the 25x RK78 refinement failure.

## Actual Mismatch Source

The observed failure is not a working-precision ceiling and not an RK embedded
local-error explosion:

- RK78 is already using `cpp_dec_float_100`.
- `max_embedded_error_abs` is `2.16e-06`, much smaller than the final
  coarse-vs-fine endpoint difference.
- Matrix denominators along the lower-half-plane contour do not create a large
  RHS multiplier near the known poles.

The mismatch source visible in current diagnostics is the global endpoint
coarse-vs-fine refinement gate after the full contour, dominated by the huge
endpoint vector scale (`2.2368e23`). The current diagnostics are not yet
per-segment, so they cannot prove which segment first accumulates the
`2.49e-19` relative endpoint discrepancy. The best localized bad-region
identification from this trace is:

```text
BAD_REGION_IDENTIFIED=[-1*I, 0], closest-pole subregion=[-0.0625*I, 0],
but no retained-matrix/RHS-denominator blowup is observed there.
```

One implementation gap was also found: the generic complex contour propagator
has `options.contour_poles` and reports `pole_pinch_step_count`, but the b61n
coupled-row caller does not pass the extracted eta poles from the contour
scaffold into those options. That makes the current `pole_pinch_step_count`
blind to the documented pole list. This is a diagnostic blind spot, not evidence
that a pole-pinch occurred.

## Next Atomic Step

The next lane should add per-segment refinement diagnostics, or wire the
extracted contour poles into the coupled-row propagation options before
attempting a detour. A detour around `-2+I` alone is not justified by this trace:
the closest documented pole is `1.5+I`, and neither pole produces a large
lower-half-plane matrix multiplier at the sampled eta values.

## Four-Role Review

- Role A, implementation: APPROVE Tier C. The change is documentation-only and
  records a reproducible retained-matrix trace without changing runtime
  behavior.
- Role B, test: APPROVE Tier C. Mandatory gates remain required, but this sidecar
  does not alter compiled code or expected test data.
- Role C, numerics: APPROVE Tier C. The final endpoint segment is the only
  identified bad region, and the trace rejects a simple denominator blowup along
  the reviewed lower-half-plane contour.
- Role D, anti-fake: APPROVE Tier C. M6 remains unflipped, the coupled rows
  remain unpublished, `full_eta_zero_contour_applied=false`, and the document
  explicitly records the remaining per-segment diagnostic gap.
