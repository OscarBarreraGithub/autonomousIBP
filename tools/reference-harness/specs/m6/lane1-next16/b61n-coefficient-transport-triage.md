# Lane 1 Next16 b61n Coefficient-Transport Triage

Status: Tier C diagnostic. This sidecar does not flip M6, does not promote
`complex_kinematics`, and does not set `full_eta_zero_contour_applied=true`.

## Step Chosen

Chosen atomic step: approach (d), document the next implementation boundary
after checking whether the lane1-next15 subleading source gap has a surgical
fix in the current endpoint path.

The lane1-next15 evidence isolated the remaining `11`-digit floor to the
post-endpoint extraction of the row-5 finite coefficient
`box[1,0,1,1] eps^0`. The current runtime path still transports coupled-row
endpoint values sample by sample in `ApplyB61nCoupledRowContourTransport`, then
collapses those values with `FitBoundarySamplesAsLaurentCoefficients`. That is
the exact boundary lane1-next15 identified as unstable: the seven retained
epsilon samples are clustered near `1.5e-14`, and the row-6 subleading
coefficients inherit the row-5 finite coefficient as a lower-triangular source.

## Findings

No runtime patch is landed in this lane. The direct source-gap fix is not a
single tolerance, sample count, or comparator option. It requires representing
and propagating the relevant Laurent coefficients as the ODE state before the
endpoint match, at least:

- `box[1,0,1,1] eps^0`;
- `box[1,1,1,1] eps^-1`;
- `box[1,1,1,1] eps^0`;
- the source-anchor rows through the orders used by those inhomogeneous terms.

The current public comparator does not receive a per-coefficient convergence
sequence. `compare_cpp_vs_amflow.py` compares already-fitted coefficient maps.
Applying Aitken, Wynn epsilon, or Pade there would only transform the result
after the unstable sample-to-coefficient fit has already selected a coefficient.
That would be post-processing the candidate answer, not independent AMFlow
parity evidence.

A scaled Vandermonde or lower-order sample fit is also not sufficient as a
promotion fix. Lane1-next14 already showed that a requested-order fit preserved
the coupled-row `11`-digit floor while damaging scalar coefficients. Variable
rescaling may improve arithmetic conditioning of the linear solve, but it does
not add the missing endpoint information: lane1-next15 measured source-anchored
endpoint errors around `1e-22` before the clustered epsilon extrapolation
amplified the row-5 finite term to the observed `~1e-9` relative-error scale.

Multi-eta matching plus Richardson extrapolation remains a larger architectural
change, not a one-line forward roll, because the present b61n solve-series path
serializes only one endpoint coefficient packet per retained epsilon sample.
Doing that honestly would require retaining multiple independent endpoint
packets and extending the result schema so the comparator can distinguish
transport convergence from coefficient fitting.

## Current Comparator State

This lane does not produce new comparator evidence. The latest comparable
lane1-next15 stripped replay remains:

```text
passed=false
compared_coefficient_count=14
passed_coefficient_count=10
minimum_digit_agreement=11
```

The limiting coefficients remain:

```text
box[1,0,1,1] eps^0: 11/11 digits
box[1,1,1,1] eps^-2: 46/46 digits
box[1,1,1,1] eps^-1: 12/13 digits
box[1,1,1,1] eps^0: 12/12 digits
```

## Next Fix Target

The next implementation attempt should add a coefficient-state b61n transport
surface rather than another endpoint sample fitter. A narrow first cut would
build an augmented ODE over the required Laurent orders for the two coupled
rows and source-anchor rows, propagate those coefficients over the same
lower-half-plane contour, and let the Frobenius matcher operate on coefficient
vectors instead of seven clustered epsilon sample values.

If that is too large for one lane, the next diagnostic should prototype the
augmented coefficient matrix on a synthetic lower-triangular b61n system first,
then wire the real `complex_kinematics` matrix only after the synthetic
coefficient-state recurrence is gateable.

## Four-Role Review

- Role A, implementation: APPROVE Tier C. No code change is landed because the
  reviewed path still needs coefficient-state propagation, not a shallow
  tolerance or fitter tweak.
- Role B, tests: APPROVE Tier C. The retained comparator floor from
  lane1-next15 remains the honest numeric status; no new passing output is
  invented.
- Role C, numerics: APPROVE Tier C. Comparator-stage sequence acceleration is
  not meaningful without a convergence sequence, and sample-space rescaling
  cannot recover coefficient information lost to clustered epsilon
  extrapolation.
- Role D, anti-fake: APPROVE Tier C only. No AMFlow final solution samples are
  consumed, no comparator tolerance is loosened, no self-comparison is used,
  and `full_eta_zero_contour_applied` remains false.

## Honest Status

`MIN_DIGIT_AGREEMENT_AFTER=11`

`M6_FLIPPED=false`
