# Lane 3 Next14 b64ag Downstream-Master Relative-Error Gap

Tier C. This iteration does not flip M6, does not set
`full_eta_zero_contour_applied=true`, does not relax comparator tolerance, and
does not consume retained final solution samples as C++ runtime input.

## Step Chosen

Use the comparator relative-error diagnostic from commit `43aaccc` on the
current full-stripped `linear_propagator -> b64ag` packet after commit
`8c4ecf7`, then classify the remaining `25` failing coefficients.

No runtime fix is landed in this step. The diagnostic shows the remaining
failures are not a small eps-order normalization miss in the new `NormalizeMat`
target switch. They are the five downstream/full-packet target rows that depend
on the fifth and sixth gauge-link DE masters after target reduction.

## Fresh Diagnostic

The lane-local run wrote:

```text
/tmp/autoibp_orch/exec/lane3_next14/full-stripped-result.json
/tmp/autoibp_orch/exec/lane3_next14/full-stripped-compare30.json
```

The comparator summary is unchanged from lane3_next13:

```text
matched_integral_count = 9
compared_coefficient_count = 39
passed_coefficient_count = 14
minimum_digit_agreement = 0
```

The passing `14` coefficients are exactly the selected/master-prefix target
surface:

```text
gauge[0,1,1,1,1,-1,0,0,0]   3/3 passed
gauge[0,1,1,1,1,0,0,0,0]    3/3 passed
gauge[1,1,1,-1,1,0,0,0,0]   4/4 passed
gauge[1,1,1,0,1,0,0,0,0]    4/4 passed
```

The other `25` failures are all epsilon orders for these five targets:

```text
gauge[1,1,1,1,1,-1,0,0,0]   eps^-2..eps^2 failed
gauge[1,1,1,1,1,0,-1,0,0]   eps^-2..eps^2 failed
gauge[1,1,1,1,1,0,0,-1,0]   eps^-2..eps^2 failed
gauge[1,1,1,1,1,0,0,0,-1]   eps^-2..eps^2 failed
gauge[1,1,1,1,1,0,0,0,0]    eps^-2..eps^2 failed
```

Representative relative-error rows from the full `NormalizeMat` path:

```text
gauge[1,1,1,1,1,-1,0,0,0] eps^-2
  C++    = 4.838164480748698961385e21 + 2.301741871211802673e18 I
  AMFlow = -0.02777777777777777777777777777777777778
  relative_error_abs = 1.74173941017791133042868e23

gauge[1,1,1,1,1,0,0,-1,0] eps^-2
  C++    = 6.055931974418831912526e21 + 2.881105869466541409e18 I
  AMFlow = 0.00555555555555555555555555555555555556
  relative_error_abs = 1.09006787875697202093798e24

gauge[1,1,1,1,1,0,0,0,0] eps^-2
  C++    = -4.677215144026957991317e20 - 2.225094785085136160e17 I
  AMFlow = -0.05555555555555555555555555555555555555
  relative_error_abs = 8.418988211939759023298e21
```

These are not near misses. The diagnostic reports `cpp_present=true` and
`amflow_present=true` for every failed coefficient, so this is not a missing-row
or parser-selection problem.

## Raw-Versus-NormalizeMat Control

I also ran a lane-local control with the `NormalizeMat` target selection
temporarily disabled, then reverted the experiment before committing. That
control wrote:

```text
/tmp/autoibp_orch/exec/lane3_next14/raw_control/full-stripped-compare30.json
```

It also reported:

```text
passed_coefficient_count = 14
minimum_digit_agreement = 0
```

The raw path changes the size of the bad values but not the failed set. For
example, `gauge[1,1,1,1,1,-1,0,0,0] eps^-2` moved from relative error
`1.7417e23` on the NormalizeMat path to `3.5984e13` on the raw path, still a
zero-digit failure. This rules out a one-line fix that merely chooses raw versus
`NormalizeMat` publication rows for the downstream targets.

## Pattern

The retained reduction table shows why the failing set is coherent:

- `gauge[1,1,1,1,1,0,-1,0,0]` and
  `gauge[1,1,1,1,1,-1,0,0,0]` both reduce to the sixth DE master with the same
  `gaugex^-2` shift.
- `gauge[1,1,1,1,1,0,0,0,0]` reduces to the fifth DE master with `gaugex^-2`.
- The `D8^-1` and `D9^-1` rows mix the first four masters with high shifted
  fifth/sixth-master contributions.

The existing selected b64ag path has reviewed coefficient tables for the direct
fifth and sixth selected masters through eps^2, but the full-packet path does
not yet compute the fifth/sixth endpoint terms with enough source-derived
correctness to make the retained reduction rows agree externally. The current
`NormalizeMat` endpoint transform is therefore downstream of the real blocker:
the fifth/sixth master endpoint publication, including the high shifted powers
needed by `gaugex^-2` and `gaugex^-3` target rows, is still not correct.

## Required Follow-Up

1. Replace or complete the source-anchored downstream two-master recurrence for
   the fifth/sixth gauge-link DE masters, using the DESolver publication-basis
   direction rather than retained final target values.
2. Add a focused regression that compares the full-packet fifth/sixth direct
   target rows against the already reviewed selected-master eps^-2..eps^2
   tables without routing through final solution samples.
3. Only after the direct fifth/sixth rows pass should the mixed `D8^-1` and
   `D9^-1` target rows be rechecked; their failures currently inherit the same
   bad downstream endpoint data.

## Four-Role Review

- Implementer: APPROVE Tier C. The relative-error table identifies a real
  downstream-master endpoint blocker; no honest one-step source-derived runtime
  fix was available without importing final target values.
- Test: APPROVE Tier C. The fresh comparator and raw-control comparator both
  stayed at `14/39` with `minimum_digit_agreement=0`; no tolerance or expected
  pass count was changed.
- Physics/source: APPROVE Tier C. The failed rows all depend on the fifth and
  sixth gauge-link DE masters after AMFlow retained target reduction, so the
  next valid fix must address those endpoint master terms before target
  reduction and PickZeroRuleS.
- Anti-fake: APPROVE Tier C. The selected-master hardcoded tables were used only
  to localize the blocker, not to synthesize full-packet target outputs. M6
  remains fail-closed.

## Honest Status

`PASSED_COUNT=14`, `MIN_DIGIT_AGREEMENT_AFTER=0`, and `M6_FLIPPED=false`.
