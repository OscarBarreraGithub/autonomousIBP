# Lane 3 Next8 b64ag Source-Anchoring Structural Gap

Tier C.  This step does not flip M6, does not set
`full_eta_zero_contour_applied=true`, does not relax comparator tolerance, and
does not consume retained final solution samples.

## Step Chosen

Diagnose the post-`df2077e` b64ag zero-digit frontier instead of adding another
endpoint source anchor.  The current six-master endpoint transport has no
unanchored DE row left in the reviewed b64ag basis:

- rows 0 and 1 are matched from the finite `gaugex=1/40` first-block boundary
  through the reviewed regular plus Frobenius first-block endpoint model;
- row 2 is a scalar finite-boundary recurrence sourced by rows 0 and 1;
- row 3 is a scalar finite-boundary recurrence sourced by row 2 and the
  already-built first block;
- rows 4 and 5 are now solved together by
  `BuildGaugeLinkCoupledDownstreamEndpointSeries`, with rows 0-3 as source
  endpoint series and both downstream constants fitted from the row-4/row-5
  finite boundary vector.

The in-tree b64ag full-packet comparison regression locks the current external
frontier after the coupled downstream anchor:

```text
matched_integral_count=9
compared_coefficient_count=39
passed_coefficient_count=14
minimum_digit_agreement=0
```

The passing rows remain the regular first-block rows and exact-zero
second-block target rows.  The residual zero-digit failures are retained target
rows, not missing DE endpoint rows:

```text
gauge[1,1,1,1,1,-1,0,0,0]
gauge[1,1,1,1,1,0,-1,0,0]
gauge[1,1,1,1,1,0,0,-1,0]
gauge[1,1,1,1,1,0,0,0,-1]
gauge[1,1,1,1,1,0,0,0,0]
```

## Structural Diagnosis

The remaining b64ag failure is now downstream of source anchoring.  AMFlow's
`ExpandGaugeX` applies the retained target-reduction rows to the asymptotic
master expansions and then runs `PickZeroRuleS`.  The C++ path now transports
all six reviewed endpoint masters without retained solution samples, but the
full-packet comparator still disagrees at the target finite-part surface.

This is structurally different from the b61n two-digit frontier.  The b61n
case still behaves like a source-anchored evolution accuracy/path issue.  The
b64ag full-packet path instead reaches a six-master endpoint packet and then
fails in the target-level AMFlow projection or basis-normalization stage after
source-anchored endpoint rows are available.

The existing second-block target projection proves one part of that shape:
single-row non-integer or analytic homogeneous target rows may have to be
removed from the target finite-part publication while remaining available for
finite-boundary endpoint reconstruction.  The five downstream failures need the
same derivation from AMFlow's target-level asymptotic projection, not another
endpoint row anchor and not hardcoded AMFlow coefficients.

## Next Honest Slice

The next implementation should derive and test a b64ag target-level projection
diagnostic for the retained downstream reduction rows:

1. Compare per-epsilon reduced finite-part samples before Laurent fitting for
   the five failing target rows.
2. Identify which reduced endpoint regions AMFlow drops after `SolveAsyExp` and
   before `PickZeroRuleS`.
3. Encode only a derived projection rule, with coverage showing that endpoint
   terms remain available for reconstruction but non-AMFlow-published
   homogeneous pieces are not published as target finite parts.
4. Keep `linear_propagator -> b64ag` blocked until the external AMFlow
   comparator improves.

## Four-Role Review

- Implementer: APPROVE Tier C.  All reviewed b64ag endpoint DE rows are already
  source-anchored or finite-boundary anchored in the current code; adding a
  fake next row would not address the failing surface.
- Test: APPROVE Tier C.  The existing CLI regression explicitly checks
  `passed_coefficient_count=14` and `minimum_digit_agreement=0` for the stripped
  full packet, so this note does not weaken expected failures.
- Physics/source: APPROVE Tier C.  AMFlow applies target reduction to asymptotic
  master expansions before `PickZeroRuleS`; the residual downstream failures
  are target projection/basis-normalization work, not missing source rows.
- Anti-fake: APPROVE Tier C.  This step adds no golden values, no implicit
  downstream zeros, no tolerance change, no retained final solution input, and
  no M6 or full-contour promotion.

## Honest Status

`linear_propagator -> b64ag` remains blocked with
`MIN_DIGIT_AGREEMENT_AFTER=0` and `M6_FLIPPED=false`.
