# Lane 1 Next 20 b64ag 50-Digit First-Block Golden Gap

Status: Tier C diagnostic. This step does not flip M6, does not promote
`linear_propagator`, does not relax comparator tolerance, does not inject
retained final solution samples, and does not set
`full_eta_zero_contour_applied=true`.

## Step Chosen

Identify the specific eight b64ag coefficients that remain below the 50-digit
floor with the comparator relative-error diagnostic, then localize where the
remaining loss enters the b64ag evidence pipeline.

This is complementary to lane3's precision uplift. It does not duplicate the
runtime precision experiment. It reruns the external comparator on the latest
rebased lane3 full-stripped result and records the exact failing coefficient
surface.

## Fresh Comparator

Inputs:

```text
C++ result:
/tmp/autoibp_orch/exec/lane3_iter18_b64ag_rebased/full-stripped-result.json

AMFlow golden:
tools/reference-harness/specs/phase0/linear_propagator.golden-manifest.json
```

Lane1 rerun artifacts:

```text
/tmp/autoibp_orch/exec/lane1_iter20_b64ag/full-stripped-compare30.json
/tmp/autoibp_orch/exec/lane1_iter20_b64ag/full-stripped-compare50.json
```

Results:

```text
compare_cpp_vs_amflow.py --tolerance-digits 30:
  exit 0, passed=true, 39/39, minimum_digit_agreement=37

compare_cpp_vs_amflow.py --tolerance-digits 50:
  exit 1, passed=false, 31/39, minimum_digit_agreement=37
```

## Exact Eight Failures

| Target | eps order | real/imag digits | complex rel. error | limiting component |
| --- | ---: | ---: | ---: | --- |
| `gauge[1,1,1,-1,1,0,0,0,0]` | -1 | 39/50 | `2.080E-36` | real |
| `gauge[1,1,1,-1,1,0,0,0,0]` | 0 | 39/39 | `9.258E-38` | real, imag |
| `gauge[1,1,1,-1,1,0,0,0,0]` | 1 | 39/40 | `1.808E-38` | real, imag |
| `gauge[1,1,1,-1,1,0,0,0,0]` | 2 | 39/37 | `1.443E-37` | real, imag |
| `gauge[1,1,1,0,1,0,0,0,0]` | -1 | 39/50 | `8.000E-37` | real |
| `gauge[1,1,1,0,1,0,0,0,0]` | 0 | 39/39 | `8.352E-38` | real, imag |
| `gauge[1,1,1,0,1,0,0,0,0]` | 1 | 39/39 | `1.826E-38` | real, imag |
| `gauge[1,1,1,0,1,0,0,0,0]` | 2 | 40/37 | `1.430E-37` | real, imag |

All five downstream/reduced rows now pass the 50-digit comparator in this
artifact. The remaining failures are exactly the two direct first-block target
rows:

```text
gauge[1,1,1,0,1,0,0,0,0]    first DE master with retained gaugex^-1 row
gauge[1,1,1,-1,1,0,0,0,0]   first-block companion row
```

The retained reduction rows for those two targets do not depend on the fifth or
sixth downstream masters, and they are not part of the `NormalizeMat` target
switch used for the downstream packet. This rules out the earlier lane3_next14
fifth/sixth downstream publication blocker as the source of the current
eight-coefficient gap.

## Where The Loss Enters

The relative errors are all around `1e-36` to `1e-38`, not zero-digit structural
mismatches. The compared values are present on both sides, and the C++ values
continue beyond the retained AMFlow text. For example:

```text
gauge[1,1,1,0,1,0,0,0,0] eps^-1
  C++    = -0.00277777777777777777777777777777777777777777777777 - 7E-50 I
  AMFlow = -0.00277777777777777777777777777777777778

gauge[1,1,1,-1,1,0,0,0,0] eps^2
  C++    = -1.04954090252454964296459857236208778680780325618936
           -2.51409321748724903241314090710726896912700986556992 I
  AMFlow = -1.04954090252454964296459857236208778681
           -2.51409321748724903241314090710726896952 I
```

The retained canonical AMFlow golden source confirms the ceiling. Its first two
rows carry Mathematica precision marks near 20 digits:

```text
/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/work/goldens/phase0/linear_propagator/captured/canonical/sol.canonical.txt

j[gauge, 1, 1, 1, 0, 1, 0, 0, 0, 0] -> ... `20.045741839440364 ...
j[gauge, 1, 1, 1, -1, 1, 0, 0, 0, 0] -> ... `20.045741839440364 ...
```

Therefore the current 50-digit loss is introduced at the retained AMFlow
golden-capture precision boundary for the direct first-block rows, then exposed
by the comparator after C++ computes and serializes a more precise first-block
endpoint/Laurent result. It is not introduced by:

- missing C++ or AMFlow rows;
- `NormalizeMat` endpoint-basis selection;
- reviewed fifth/sixth downstream endpoint publication;
- retained target reduction over the downstream packet;
- the external comparator parser.

The C++ b64ag pipeline step that first sees the mismatch is the final
post-endpoint Laurent comparison for the two direct first-block targets. The
evidence bottleneck before that comparison is the low-precision retained AMFlow
golden text, not the now-passing downstream transport rows.

## Required Follow-Up

1. Recapture or provide an independently reviewed AMFlow golden for the two
   direct first-block b64ag targets at a precision sufficient for a 50-digit
   comparator floor.
2. Re-run the same 39-coefficient comparator against that high-precision golden
   before considering any M6 promotion.
3. Do not make C++ round or truncate these two rows to the retained low-precision
   golden; that would hide the evidence ceiling rather than close the runtime
   lane.

## Four-Role Review

- Implementer: APPROVE Tier C. The remaining failures are isolated to the two
  direct first-block target rows; downstream target transport is no longer the
  limiting surface in this artifact.
- Test: APPROVE Tier C. The comparator rerun is explicit: `39/39` at 30 digits,
  `31/39` at 50 digits, with the exact eight failures listed individually.
- Physics/source: APPROVE Tier C. The retained AMFlow canonical text itself
  carries near-20-digit precision marks for the two failing rows, so a 50-digit
  M6 claim needs a recaptured or otherwise reviewed high-precision reference.
- Anti-fake: APPROVE Tier C. This note does not change code, tolerances,
  expected pass counts, phase-0 metadata, or M6 state. It rejects a fake closure
  by identifying the golden-evidence ceiling as still below the required floor.

## Honest Status

`PASSED_COUNT_30=39/39`, `PASSED_COUNT_50=31/39`,
`MIN_DIGIT_AGREEMENT_AFTER=37`, and `M6_FLIPPED=false`.
