# Lane 3 Next6 b64ag Downstream Source-Anchoring Gap

Tier C.  The b64ag full-packet path remains externally blocked at zero
minimum digit agreement.  This note documents the next structural gap and does
not flip `full_eta_zero_contour_applied`, M6 qualification metadata, comparator
tolerances, or retained-solution provenance.

## Current Comparator Frontier

Baseline for this lane was clean `origin/main` at:

```text
9ae2649b6c09995d8b256e520fd441859d6cfc51
```

After lane3_next5, the stripped `linear_propagator` full-packet path reaches all
nine retained b64ag targets.  The in-tree test locks the current external
comparator frontier to:

```text
matched_integral_count=9
compared_coefficient_count=39
passed_coefficient_count=14
minimum_digit_agreement=0
```

The fourteen passing coefficients are the two first-block masters plus the two
exact-zero second-block target rows.  The remaining wrong rows are the
downstream packet surface:

```text
gauge[1,1,1,1,1,0,-1,0,0]
gauge[1,1,1,1,1,0,0,-1,0]
gauge[1,1,1,1,1,0,0,0,-1]
gauge[1,1,1,1,1,0,0,0,0]
gauge[1,1,1,1,1,-1,0,0,0]
```

This is not the older lane1_next8 second-block zero-projection failure.  The
current failure is downstream of the now-passing exact-zero second block.

## AMFlow Source-Anchoring Convention

The AMFlow gauge-link path does not solve target rows from independent scalar
endpoint matches.  In the Mathematica reference, `SolveIntegralsGaugeLink`
generates the square family, multiplies each target reduction row by the
affected-propagator `gaugex` normalization, solves finite boundary masters at
the chosen point, then calls `ExpandGaugeX`.

`ExpandGaugeX` anchors the full DE system at the finite boundary before
extracting endpoint target finite parts:

```text
tablecoe = Values[table] /. {variables[[1]] -> eta, eps -> boundary[[n,1]]}
de = diffeq[[1]] /. {variables[[1]] -> eta, eps -> boundary[[n,1]]}
bc = boundary[[n,2]]
LoadSystem[n, de, bc, point[[2]]]
SolveAsyExp[n]
masterexp = masters /. Thread[mastersde -> AsyExp[n]]
allexp = PlusAsyExp@MapThread[TimesAsyExp, {#, masterexp}] & /@ tablecoe
PickZeroRuleS /@ allexp
```

Reference anchors:

```text
/n/holylabs/schwartz_lab/Lab/obarrera/reference-inputs/autonomousIBP/cpc/amflow-gitlab-1.1-extracted/AMFlow.m:1238-1262
/n/holylabs/schwartz_lab/Lab/obarrera/reference-inputs/autonomousIBP/cpc/amflow-gitlab-1.1-extracted/AMFlow.m:1281-1334
tools/reference-harness/specs/phase0/linear_propagator.amflow-state.json:54-57
```

The required ordering is therefore:

```text
finite gaugex=1/40 boundary vector
-> full-system local endpoint asymptotics
-> target-row reduction and D4/D5 normalization
-> PickZeroRuleS finite-part selection
-> epsilon Laurent fit
```

## C++ Gap

The current runtime correctly recognizes the reviewed six-master DE basis and
now carries endpoint terms for all six masters.  The implementation, however,
builds endpoint rows 2, 3, 4, and 5 by repeated scalar calls to
`BuildGaugeLinkScalarEndpointSeries`.

```text
src/runtime/lightlike_propagator.cpp:2079-2224
src/runtime/lightlike_propagator.cpp:3287-3314
```

That is a valid shape for the second block because row 3 only depends on the
already-built row 2.  It is not a valid shape for the downstream block.  The
retained b64ag DE matrix has a coupled downstream subsystem:

```text
row 4:
  source columns 0, 1, 2, 3
  self column 4: 2 / gaugex + regular terms
  coupled column 5: 6*(-1 + eps)/(1 + 2*gaugex)

row 5:
  source columns 0, 1, 2, 3
  coupled column 4: -2*(-1 + eps)/(gaugex*(1 + 2*gaugex)*(1 + 4*gaugex))
  self column 5: (-2 + 4*eps) / gaugex + regular terms
```

When the C++ path solves row 4 first, column 5 is still empty and is silently
ignored by the scalar source loop.  Row 5 is then solved using a row-4 series
that was not coupled back to row 5.  AMFlow's `SolveAsyExp` instead sees rows 4
and 5 together inside the same finite-boundary-anchored system before target
reduction.

This is the b64ag analogue of the b61n state after the coupled Frobenius matcher
was wired but before source-anchored coupled evolution was reviewed.  The
current b64ag first block and second block are useful source data, but the
downstream rows need a reviewed coupled-row endpoint evolution:

```text
1. Treat rows 0-3 as reviewed source endpoint series for each epsilon sample.
2. Solve rows 4 and 5 as a coupled downstream subsystem, not as two scalar rows.
3. Anchor the two downstream homogeneous constants to the finite gaugex=1/40
   boundary values for rows 4 and 5 together.
4. Apply the retained target reduction and PickZeroRuleS only after that
   coupled source-anchored endpoint system exists.
5. Keep publication closed until the five downstream packet rows clear the
   external AMFlow comparator.
```

## Four-Role Review

- Implementer: APPROVE Tier C.  The current runtime reaches the full-packet
  comparator and preserves source provenance, but the downstream 2x2 subsystem
  is not solved in the AMFlow source-anchored order.
- Test: APPROVE Tier C.  Existing `amflow-tests` lock the 14/39, min-0
  comparator frontier and `singular-runtime-lane-tests` cover the primitives,
  but no test can honestly assert downstream packet parity yet.
- Physics/source: APPROVE Tier C.  AMFlow's `ExpandGaugeX` anchors the full DE
  solution at the finite boundary before applying target rows; scalar row 4
  followed by scalar row 5 is not equivalent for the retained downstream block.
- Anti-fake: APPROVE Tier C.  This note adds no hardcoded AMFlow values, no
  implicit zero publication, no tolerance change, no retained final solution
  input, and no M6 or `full_eta_zero_contour_applied` promotion.

## Honest Status

The honest next implementation target is a b64ag downstream coupled
source-anchored endpoint solver for rows 4 and 5.  Until that exists and the
external AMFlow comparator improves, `linear_propagator -> b64ag` remains
blocked with `minimum_digit_agreement=0` and `M6_FLIPPED=false`.
