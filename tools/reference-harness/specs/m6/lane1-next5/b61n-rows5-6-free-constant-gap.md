# Lane 1 Next5 b61n Rows 5/6 Free-Constant Gap

Status: Tier C structural diagnostic. This note does not flip M6, does not
promote `complex_kinematics`, and does not set
`full_eta_zero_contour_applied=true`.

## Step Chosen

Step (a): determine what makes b61n rows 5 and 6 different from rows 0-4.

Rows 5 and 6 are not blocked by a missing logarithmic Frobenius branch at
eta=0. In the retained b61n eta matrix, all row 5/6 denominators are nonzero at
eta=0, and the sampled eta-residue at eta=0 is zero. The active local model is
therefore an ordinary eta=0 Taylor model with repeated rho=0 roots, not a
regular-singular resonance that can be fixed by adding log-eta terms.

The difference is the free homogeneous endpoint constant. Rows 0-4 either have
reviewed scalar/primitive endpoint series or are source rows already anchored to
those reviewed endpoint values. Row 5 is an inhomogeneous lower-triangular
ordinary row sourced by rows 0, 1, 2, and 4 plus its own diagonal term. Row 6 is
another ordinary row sourced by rows 0, 1, 2, 4, and row 5 plus its own diagonal
term. Source rows determine only the particular part of those equations; they do
not determine the independent homogeneous constants for rows 5 and 6.

## Matrix Structure

The b61n master order is:

```text
0 box[0,0,0,1]
1 box[1,0,1,0]
2 box[1,0,0,1]
3 box[0,1,0,1]
4 box[0,0,1,1]
5 box[1,0,1,1]
6 box[1,1,1,1]
```

The nonzero row 5 pattern is:

```text
row 5 <- rows 0, 1, 2, 4, 5
```

The nonzero row 6 pattern is:

```text
row 6 <- rows 0, 1, 2, 4, 5, 6
```

For both rows, the row diagonal denominator is nonzero at eta=0:

```text
row 5 diagonal denominator at eta=0: 2000 - 977*I
row 6 diagonal denominator at eta=0: 41 - I
```

That confirms ordinary-point behavior for the active eps0 path. A local
Frobenius recurrence with rho=0 has a free c0 component for these rows, so the
endpoint matcher still needs a certified row 5/6 finite-start constant or an
independent row 5/6 endpoint boundary. Anchoring rows 0-4 cannot determine those
constants.

## Regression Added

`B61nCoupledFrobeniusEndpointMatcherKeepsCoupledRowsFreeUnderSourceAnchorsTest`
now exercises the source-anchored coupled Frobenius matcher on an ordinary
lower-triangular row. The source endpoint is identical and anchored in both
controls, while only the coupled row finite-start constant is varied. The test
asserts that the coupled endpoint changes by exactly the same homogeneous
constant. This is the row 5/6 blocker in minimal form.

## Next Surgical Fix

The next implementation should not add log-eta terms for rows 5/6. It should
build a row-specific variation-of-constants transport:

1. keep rows 0-4 on the reviewed endpoint-anchored source trajectory;
2. solve row 5 as a scalar inhomogeneous ordinary equation with its own certified
   finite-start constant and the anchored rows 0-4 as sources;
3. solve row 6 similarly with rows 0-4 plus the certified row 5 trajectory as
   sources;
4. publish only after the AMFlow comparator clears the required digit floor on a
   stripped input that has no final solution sample.

## Four-Role Review

- Role A, implementation: APPROVE Tier C. The new regression isolates the exact
  free-constant behavior in the landed source-anchored matcher.
- Role B, tests: APPROVE Tier C. The test prevents future lanes from treating
  source-row anchoring as row 5/6 endpoint anchoring.
- Role C, physics/numerics: APPROVE Tier C. The real b61n row 5/6 eta=0
  structure is ordinary-point inhomogeneous lower triangular, not a logarithmic
  Frobenius resonance.
- Role D, anti-fake: APPROVE Tier C only. This step uses no AMFlow final
  solution sample as input, does not loosen the comparator, and keeps
  `full_eta_zero_contour_applied=false`.

## Honest Status

`MIN_DIGIT_AGREEMENT_AFTER=2`

`M6_FLIPPED=false`
