# Lane 1 Next 10 b64ag Post-Zero-Rule Comparator Gap

## Verdict

Tier C.  This lane does not flip M6 and does not set
`full_eta_zero_contour_applied=true`.

After lane1_next9 projected retained b64ag non-integer Frobenius rows through
the AMFlow no-integer-key zero rule, the full stripped b64ag packet still fails
the external AMFlow comparator.  The first-block rows and the first canonical
second-block exact-zero row now pass, but the second-block companion and every
downstream target still publish large endpoint finite parts that AMFlow does not
publish.

## Fresh Comparator

The fresh run was performed from commit
`07793f336c3fec994cb6faf7141740422a9e4fbb`, with only the retained final
solution file removed from
`tools/reference-harness/specs/phase0/linear_propagator.amflow-state.json`:

```bash
/tmp/autoibp_lane1_next10_build/amflow-cli solve-series \
  /tmp/autoibp_orch/exec/lane1_next10/linear-full-stripped.json \
  --eps-order 2 --digits 50 \
  --out /tmp/autoibp_orch/exec/lane1_next10/full-stripped-result.json

python3 tools/reference-harness/scripts/compare_cpp_vs_amflow.py \
  --cpp-result /tmp/autoibp_orch/exec/lane1_next10/full-stripped-result.json \
  --amflow-golden tools/reference-harness/specs/phase0/linear_propagator.golden-manifest.json \
  --tolerance-digits 30 \
  > /tmp/autoibp_orch/exec/lane1_next10/full-stripped-compare30.json
```

The comparator returned exit code 1:

- `matched_integral_count = 9`
- `compared_coefficient_count = 39`
- `passed_coefficient_count = 11`
- `minimum_digit_agreement = 0`

The eleven passing coefficients are the four first selected endpoint
coefficients, the four first-block companion coefficients, the explicit
eps^0 zero for `gauge[0,1,1,1,1,0,0,0,0]`, and two absent-on-both-sides
zero passes for that same row at eps^1 and eps^2.  Those last two are
not newly published C++ zero coefficients.

## Residual Failing Surface

The residual zero-digit failures begin with the second-block companion target:

```text
gauge[0,1,1,1,1,-1,0,0,0] eps^0:
  AMFlow = 0
  C++    = -19395532764386506928254.26082794939152124531948964474961883117692031036060
           -14405161012653599870.87258893398020590752654360716210047741417859641753*I
```

The downstream targets also remain zero-digit mismatches at their leading pole
orders:

```text
gauge[1,1,1,1,1,-1,0,0,0] eps^-2:
  AMFlow real = -0.0277777777777777777777777...
  C++ real    = 953374740122.451677263842074...

gauge[1,1,1,1,1,0,-1,0,0] eps^-2:
  AMFlow real = -0.0277777777777777777777777...
  C++ real    = 953374740122.451677263842074...

gauge[1,1,1,1,1,0,0,-1,0] eps^-2:
  AMFlow real = 0.00555555555555555555555555...
  C++ real    = -1992679603311.0250560902821...

gauge[1,1,1,1,1,0,0,0,-1] eps^-2:
  AMFlow real = 0.02777777777777777777777777...
  C++ real    = 120880731712.551231978782500...

gauge[1,1,1,1,1,0,0,0,0] eps^-2:
  AMFlow real = -0.0555555555555555555555555...
  C++ real    = 80587154475.0341546525216670...
```

This is no longer the lane1_next8 exact-zero projection blocker alone.  The
C++ scalar endpoint recurrence is still publishing large integer/homogeneous
pieces for the second-block companion and downstream rows after retained target
reduction.

## AMFlow Normalization Gap

The retained AMFlow driver embedded in the state does not publish master
finite parts row-by-row.  Its `solve.wl` pipeline is:

```text
SetExpansionOptions["XOrder" -> 492, "LearnXOrder" -> -1, "TestXOrder" -> 5];
tablecoe = Values[table] /. {variables[[1]] -> eta, eps -> boundary[[n,1]]} // Factor;
de = diffeq[[1]] /. {variables[[1]] -> eta, eps -> boundary[[n,1]]} // Factor;
bc = boundary[[n,2]];
LoadSystem[n, de, bc, point[[2]]];
SolveAsyExp[n];
masterexp = masters /. Thread[mastersde -> AsyExp[n]];
allexp = PlusAsyExp @ MapThread[TimesAsyExp, {#, masterexp}]& /@ tablecoe;
PickZeroRuleS /@ allexp;
```

The remaining mismatch points at an unmodeled AMFlow target-level asymptotic
projection or basis normalization after `SolveAsyExp`, not at the already-fixed
no-integer-key zero rule.  The second-block companion row is especially
diagnostic: the retained reduction table maps it as
`{0, 0, 0, gaugex^(-2), 0, 0}`, AMFlow publishes an exact zero, while C++ fits
a large finite contribution from the scalar endpoint connection.

A temporary probe that changed the scalar endpoint recurrence budget from
80 powers toward AMFlow's `XOrder -> 492` was not retained: it did not complete
the stripped packet solve within a practical lane-local budget, and therefore
provided no honest digit-improvement evidence.  The next code change should be
a derived AMFlow-compatible target-level asymptotic projection, not a blind
budget increase and not a hardcoded golden zero.

## Required Follow-Up

1. Derive how AMFlow's `SolveAsyExp` represents and projects the coupled
   second-block companion and downstream asymptotic branches before
   `PickZeroRuleS`.
2. Model that target-level projection on the assembled retained reduction rows,
   so endpoint terms needed for finite-boundary reconstruction remain available
   but non-AMFlow-published homogeneous pieces are not emitted as target finite
   parts.
3. Add focused coverage for
   `gauge[0,1,1,1,1,-1,0,0,0] -> 0` that is derived from the projection rule
   rather than from a target-label hardcode.
4. Re-run the full stripped comparator and update any expected pass count only
   after the external AMFlow comparison improves.

## Four-Role Review

- Implementer: APPROVE Tier C.  The full stripped path transports all six
  masters and reaches the comparator, but external comparison remains
  `11/39` passed with `minimum_digit_agreement = 0`; the reduced finite-part
  code has only a reviewed first-block Frobenius exclusion, while
  second-block/downstream pieces still flow into target finite parts.
- Test: APPROVE Tier C.  Keep the existing comparator-fail assertions:
  compare command returns nonzero, `matched_integral_count = 9`,
  `compared_coefficient_count = 39`, `passed_coefficient_count = 11`, and
  `minimum_digit_agreement = 0`.  Do not change tests to expect comparator
  success or `39/39` passing coefficients.
- Physics: APPROVE Tier C.  The retained AMFlow state assembles target
  expansions with `PlusAsyExp @ MapThread[...]` and only then applies
  `PickZeroRuleS`.  The residual second-block companion zero cannot be safely
  hardcoded because its retained reduction row is structurally nonzero:
  `{0, 0, 0, gaugex^(-2), 0, 0}`.
- Anti-fake: APPROVE Tier C.  This remains a failing external AMFlow
  comparison at `tolerance_digits=30` with `minimum_digit_agreement=0`; it
  does not flip M6, does not promote `full_eta_zero_contour_applied`, and is
  not an independent fresh AMFlow recapture.  The stripped input removes the
  retained final target solution file, but still uses retained `gaugex=1/40`
  finite boundary vectors and retained AMFlow state metadata.
