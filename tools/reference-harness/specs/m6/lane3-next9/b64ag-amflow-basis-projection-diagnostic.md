# Lane 3 Next9 b64ag AMFlow Basis Projection Diagnostic

Tier C.  This step does not flip M6, does not set
`full_eta_zero_contour_applied=true`, does not relax comparator tolerance, and
does not consume retained final solution samples as C++ runtime input.

## Step Chosen

Compare our b64ag target representation against AMFlow Mathematica's published
basis for the same retained `linear_propagator` packet.  The comparison was run
for the first retained epsilon sample, `eps = 101/208000`, by replaying AMFlow's
own `SolveAsyExp` path from the retained `reduction`, `diffeq`, and `boundary`
files:

```text
/tmp/autoibp_orch/exec/lane3_next9_amflow_basis/
```

The direct AMFlow dump used the recorded Mathematica 13.3 kernel and the
available AMFlow 1.1 DESolver snapshot:

```text
/n/sw/helmod/apps/centos7/Core/mathematica/Mathematica_13.3.0/Executables/WolframKernel
/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/lane101-gg-to-gammagamma-light-quark-mi/amflow/diffeq_solver/DESolver.m
```

## Basis Convention Found

AMFlow does not expose the raw downstream endpoint master series directly to the
target finite-part picker.  `SolveAsyExp` first runs DESolver's `NormalizeMat`
block normalization.  For the first b64ag epsilon sample, AMFlow reports:

```text
block {1, 2}: behavior {{0, 0}, {303/104000, 0}}
block {3, 4}: behavior {{101/104000, 0}, {303/104000, 0}}
block {5, 6}: behavior {{0, 1}, {101/52000, 0}, {303/104000, 0}, {101/104000, 0}}
```

That normalization moves integer residue powers into AMFlow's local block basis
before target reduction and `PickZeroRuleS`.  In AMFlow's target asymptotics,
the downstream failing targets therefore have one integer key with zero singular
slots, plus non-integer normalized keys:

```text
gauge[1,1,1,1,1,0,-1,0,0]  keys {-6, -623697/104000, -623899/104000, -207899/52000}
gauge[1,1,1,1,1,0,0,-1,0]  keys {-7, -727697/104000, -727899/104000, -259899/52000}
gauge[1,1,1,1,1,0,0,0,-1]  keys {-6, -623697/104000, -623899/104000, -207899/52000}
gauge[1,1,1,1,1,0,0,0,0]   keys {-6, -623697/104000, -623899/104000, -155899/52000}
gauge[1,1,1,1,1,-1,0,0,0]  keys {-6, -623697/104000, -623899/104000, -207899/52000}
```

The Mathematica replay printed `dropped terms when pickzero` with zero singular
slots for all retained targets in this sample.  The first-sample AMFlow
post-`PickZeroRuleS` target values are finite, for example:

```text
gauge[1,1,1,1,1,0,-1,0,0] = -118166.6684276813831667304512435703456
                              - 540.7881785019691084554211364182841 I
gauge[1,1,1,1,1,0,0,-1,0] =  23631.4211916020430978878657516374899
                              + 108.1488831974684879938967288649346 I
gauge[1,1,1,1,1,0,0,0,0]  = -236341.0087664000626631544479270378843
                              - 1081.6114674022484457379066627729443 I
```

The C++ path currently applies retained target reduction to endpoint series in
its raw/original downstream master basis after fitting the row-4/row-5 boundary
constants.  A lane-local probe that returned the already-diagonalized internal
row-4/row-5 basis instead of converting back to original labels did not improve
external comparison:

```text
/tmp/autoibp_orch/exec/lane3_next9_probe/full-stripped-compare30.json
matched_integral_count=9
compared_coefficient_count=39
passed_coefficient_count=14
minimum_digit_agreement=0
```

So the missing convention is not a simple downstream sign flip or the existing
two-by-two diagonalization basis.  The missing transform is AMFlow DESolver's
target-level `NormalizeMat` block basis normalization before `PickZeroRuleS`.

## Next Required Slice

Implement the DESolver `NormalizeMat` block basis transform for the b64ag
six-master endpoint packet before target reduction.  The transform must be
derived from the AMFlow block behavior and recurrence basis, not from hardcoded
golden target values.  Endpoint terms needed for finite-boundary reconstruction
must remain available, while target publication must consume the AMFlow
normalized block basis that has zero singular integer slots for the downstream
targets.

## Four-Role Review

- Implementer: APPROVE Tier C.  The direct Mathematica replay identifies the
  missing convention as `NormalizeMat` block basis normalization, and the
  simple downstream diagonalized-basis probe was ruled out by a fresh comparator.
- Test: APPROVE Tier C.  No expected comparator counts or tolerances were
  weakened; the fresh probe remains `14/39` with `minimum_digit_agreement=0`.
- Physics/source: APPROVE Tier C.  AMFlow's published target asymptotics are
  produced after `SolveAsyExp` normalization and before `PickZeroRuleS`, matching
  the retained `solve.wl` pipeline.
- Anti-fake: APPROVE Tier C.  This diagnostic adds no C++ coefficient shortcut,
  no retained final solution input, no implicit downstream zero, and no M6
  promotion.

## Honest Status

`linear_propagator -> b64ag` remains blocked with
`MIN_DIGIT_AGREEMENT_AFTER=0` and `M6_FLIPPED=false`.
