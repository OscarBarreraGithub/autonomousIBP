# Lane 1 Next 8 b64ag Second-Block Frobenius Finite-Part Gap

## Verdict

Tier C.  This lane does not flip M6 and does not set
`full_eta_zero_contour_applied=true`.

The b64ag full-packet path now reaches the external AMFlow comparator, but the
next concrete digit-agreement failure is structural rather than a transport
precision floor.  The retained second/downstream non-integer Frobenius endpoint
branches are propagated through retained target reduction into finite-part
coefficients.  That keeps all nine target rows externally comparable, but it
produces large nonzero Laurent coefficients for targets whose canonical AMFlow
packet has exact-zero second-block entries and finite downstream coefficients.

## Reproduction

From a clean build at `origin/main` commit `e7c9430`, strip only the retained
final solution file from
`tools/reference-harness/specs/phase0/linear_propagator.amflow-state.json`, then
run:

```bash
build/amflow-cli solve-series \
  /tmp/autoibp_lane1_next8_diag/linear-full-stripped.json \
  --eps-order 2 --digits 50 \
  --out /tmp/autoibp_lane1_next8_diag/full-stripped-result.json

python3 tools/reference-harness/scripts/compare_cpp_vs_amflow.py \
  --cpp-result /tmp/autoibp_lane1_next8_diag/full-stripped-result.json \
  --amflow-golden tools/reference-harness/specs/phase0/linear_propagator.golden-manifest.json \
  --tolerance-digits 30
```

The comparator reports:

- `matched_integral_count = 9`
- `compared_coefficient_count = 39`
- `passed_coefficient_count = 8`
- `minimum_digit_agreement = 0`

The eight passing coefficients are the regular first-block rows fixed by
lane1_next7.  The failing rows start with the canonical second-block targets:

```text
gauge[0,1,1,1,1,0,0,0,0] -> AMFlow exact 0
gauge[0,1,1,1,1,-1,0,0,0] -> AMFlow exact 0
```

The current C++ full-packet path instead fits large nonzero values for those
rows, for example at eps order 0:

```text
gauge[0,1,1,1,1,0,0,0,0]:
  cpp_real = -3430938816.91027994284628525956482887529490213436161359919718
  cpp_imag = -2546480.64094329082823159803052789503634906379581575850839

gauge[0,1,1,1,1,-1,0,0,0]:
  cpp_real = -19395536370270810196416.16418931031384395536046780140713647191814584545406
  cpp_imag = -14405164411505895903.22400330370059692937708625410266643910226203672325
```

Raising the b64ag endpoint transport arithmetic from `cpp_dec_float_100` to a
300-digit decimal backend did not change the comparator result or these fitted
coefficients.  That rules out the observed 0-digit failure as a simple
working-precision truncation in the current recurrence.

## Failure Mode

The retained reduction table contains identity-style second-block target rows
with embedded `gaugex^-2` normalization:

```text
j[gauge, 0, 1, 1, 1, 1, 0, 0, 0, 0] -> {0, 0, gaugex^(-2), 0, 0, 0}
j[gauge, 0, 1, 1, 1, 1, -1, 0, 0, 0] -> {0, 0, 0, gaugex^(-2), 0, 0}
```

The current endpoint transport legitimately carries the reviewed small-epsilon
second-block Frobenius regions, including exponents
`-7 + 8 eps` and downstream `-2 + 4 eps`, so that finite boundary values can be
reconstructed.  However, the reduced finite-part evaluator currently treats
those carried branches as retained AMFlow finite-part target contributions.  In
the canonical AMFlow golden, the two second-block DE-basis targets are exact
zero rules and the downstream target rows remain finite.  The branch projection
used by AMFlow for this retained packet is therefore not yet modeled by the C++
finite-part reducer.

This is analogous to the lane1_next7 first-block issue, where the unresolved
first-block Frobenius branch had to remain available for boundary
reconstruction but be excluded from retained AMFlow target finite parts.  The
second/downstream blocks need the same level of reviewed branch-projection
derivation, not a hardcoded zero or golden-value injection.

## Required Follow-Up

1. Derive the AMFlow branch-projection rule for the retained b64ag
   second/downstream non-integer Frobenius branches after GenerateSquare
   normalization.
2. Add unit coverage that distinguishes endpoint boundary reconstruction from
   retained target finite-part publication for the exact-zero second-block
   target rows.
3. Apply the rule inside the reduced finite-part evaluator before Laurent
   fitting, without reading retained final solution samples and without
   hardcoding AMFlow golden coefficients.
4. Re-run the full stripped packet comparator and update the expected
   `passed_coefficient_count` only if the external AMFlow comparison improves.

## Four-Role Review

- Implementer: APPROVE Tier C.  The failure was reproduced after the
  lane1_next7 first-block fix, and a 300-digit transport experiment showed no
  comparator improvement.
- Test: APPROVE Tier C.  Existing tests cover Frobenius branch transport and
  first-block branch exclusion, but they do not yet bind retained zero
  second-block target publication.
- Physics: APPROVE Tier C.  The missing object is a reviewed branch projection
  for retained b64ag second/downstream Frobenius finite parts, not another
  precision knob.
- Anti-fake: APPROVE Tier C.  This note does not change tolerances, inject
  AMFlow golden values, hardcode zero coefficients, consume retained final
  solution samples, or promote M6.
