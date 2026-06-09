# Lane 2 B61n Row 5/6 Boundary Matching Audit

Status: Tier C structural audit. This sidecar does not flip M7 parity
signoff, does not promote `complex_kinematics`, and does not set
`full_eta_zero_contour_applied=true`.

## Step Chosen

Chosen atomic step: option (a), audit the row 5/6 boundary-matching structure
instead of repeating another blind precision or RK retry.

The relevant history is consistent across the b61n notes:

- `lane1-next5` established that rows 5 and 6 are ordinary at eta=0 but have
  independent homogeneous endpoint constants.
- `lane1-next13` through `lane1-next16` isolated the later 11-digit ceiling to
  the post-endpoint extraction of `box[1,0,1,1] eps^0`, with row 6 subleading
  terms inheriting that row-5 finite source.
- `lane176` and `lane177` moved the live propagation blocker past the old
  finite-start recurrence issue and onto the endpoint refinement/basis
  structure: the peak is at eta=0, not at a documented contour pole.

## Current Boundary Contract

The live b61n coupled-row path in `src/cli/main.cpp` still runs one endpoint
transport per retained epsilon sample. In `ApplyB61nCoupledRowContourTransport`,
rows 5 and 6 are the transported rows, rows 0 through 4 are source anchors, and
each epsilon sample builds a scalar `ComplexContourPropagationOptions` packet.
The source anchors are evaluated through `eps^2`, the two coupled rows are
propagated as sample values, and the resulting samples are copied back into
`master_samples`.

After that, the common solve-series path fits every master with
`FitBoundarySamplesAsLaurentCoefficients`, which estimates the leading order
and calls `SolveVandermondeFit` on the retained epsilon sample values. The
latest failing evidence says this is exactly where row 5 loses the needed
digits: the endpoint samples are clustered near `1.5e-14`, and row 6 uses row
5 as a lower-triangular source.

The runtime already has a coefficient-state primitive:
`BuildComplexContourCoefficientStateMatrix` maps Laurent matrix coefficients
and Laurent state coefficients into an augmented ODE. The synthetic
`B61nCoefficientStateTransportPropagatesLaurentOrdersBeforeEndpointFitTest`
proves the intended idea on a two-master lower-triangular fixture. The real
b61n row 5/6 path does not yet use that primitive; it still transports sample
values first and performs the Laurent extraction afterward.

## Boundary Matcher Diagnosis

The current coupled Frobenius matcher is not enough to close b61n because it
certifies the wrong object for the remaining wall.

`MatchCoupledFrobeniusEndpointFromSmallEta` constructs one Frobenius basis per
root index, evaluates each basis at the small-eta match point, and, when source
rows are anchored and exactly two rows remain free, delegates to
`MatchTwoFreeCoupledFrobeniusConstants`. That helper solves a 2x2 system using
the free rows at the match point and then overwrites the anchored rows with the
reviewed source endpoint values. The small residuals reported by earlier lanes
therefore prove that the per-sample match vector can be reconstructed. They do
not prove that the row-5 `eps^0` coefficient and row-6 `eps^-1/eps^0`
coefficients have been transported as Laurent coefficients.

This distinction matches the observed parity surface:

- row 5 `eps^0` is the digit ceiling, at about 11 agreeing digits;
- row 6 `eps^-2` can be much better, so this is not a global contour sign or
  normalization error;
- row 6 `eps^-1` and `eps^0` carry the same `~1e-9` relative scale as row 5,
  as expected for a lower-triangular dependency on row 5;
- later RK78 diagnostics can fail closed at the endpoint before publication,
  but when publication occurs the remaining sub-50 mismatch is still the
  sample-to-coefficient extraction of these coupled rows.

The structural bug is therefore a contract mismatch: the real row 5/6 endpoint
matcher solves free constants in sample space, while the parity gate is a
coefficient-level AMFlow comparison. A tighter Frobenius order, a different
small-eta radius, or another RK step budget can make the sample-space residual
smaller without resolving the coefficient-level boundary constants that the
comparator checks.

## Non-Claims

No new AMFlow comparator is claimed in this lane. The latest comparable
publishing surface remains the lane1-next15 50-digit comparator:

```text
passed=false
compared_coefficient_count=14
passed_coefficient_count=10
minimum_digit_agreement=11
```

The later RK78 fail-closed surface remains:

```text
passed=false
compared_coefficient_count=14
passed_coefficient_count=10
minimum_digit_agreement=2
full_eta_zero_contour_applied=false
```

This audit does not loosen tolerance, does not use final AMFlow solution
samples as input, and does not touch b63n files.

## Next Narrow Implementation Target

The next honest implementation attempt should wire coefficient-state transport
into the real b61n row 5/6 path before endpoint matching. The first useful
slice should carry only the coefficients needed by the comparator wall:

- source rows 0 through 4 through the orders that feed the coupled equations;
- row 5 `eps^0`;
- row 6 `eps^-2`, `eps^-1`, and `eps^0`.

The acceptance test should fail if those coefficients are reconstructed from
the seven clustered epsilon sample values after propagation. It should instead
verify that the augmented coefficient-state ODE feeds the coupled endpoint
matcher or an ordinary endpoint boundary solve directly, then compare the
published coefficients against the unchanged AMFlow golden at the 50-digit
gate.

## Honest Status

`M7_PARITY_SIGNOFF_FLIPPED=false`

`B61N_ROOT_CAUSE=sample-space row 5/6 free-constant matching is not coefficient-level endpoint transport`

`NEXT_FIX=wire real b61n coupled rows through coefficient-state transport before Laurent fitting`
