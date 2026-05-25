# Lane 3 Next10 b64ag NormalizeMat Transform Gap

Tier C.  This step does not flip M6, does not set
`full_eta_zero_contour_applied=true`, does not relax comparator tolerance, and
does not consume retained final solution samples as C++ runtime input.

## Step Chosen

Attempt the next b64ag basis-normalization slice identified by lane3_next9:
derive the DESolver `NormalizeMat` transform that maps the C++ six-master
endpoint packet into AMFlow's published local basis before retained target
reduction and `PickZeroRuleS`.

The implementation was not landed because the reviewed evidence shows that the
missing transform is a nontrivial power-series matrix, not a target-label
constant, target-value shortcut, or simple downstream sign/diagonalization map.

## New Evidence

Using the retained AMFlow replay under
`/tmp/autoibp_orch/exec/lane3_next9_amflow_basis/`, the numeric first-sample
probe for `eps = 101/208000` ran DESolver's own:

```text
{T, invT, B} = NormalizeMat[de];
ToPS[T]
```

AMFlow normalized the retained block structure as:

```text
blocks = {{1, 2}, {3, 4}, {5, 6}}
```

The resulting `T` power-series map has nonzero downstream couplings from
multiple normalized branches and multiple powers.  Representative entries from
the same first epsilon sample are:

```text
Tps[1,1] = {{6/5}, 1}
Tps[1,2] = {{... six coefficients ...}, -4}
Tps[2,1] = {{1}, 0}
Tps[2,2] = {{... seven coefficients ...}, -6}

Tps[3,3] = {{-2}, -1}
Tps[3,4] = {{... four coefficients ...}, -4}
Tps[4,3] = {{1}, -3}
Tps[4,4] = {{... four coefficients ...}, -6}

Tps[5,2] = {{... four coefficients ...}, -2}
Tps[5,3] = {{623697/155899, 64843490201/16197750201}, 0}
Tps[5,4] = {{... four coefficients ...}, -2}
Tps[5,5] = {{2}, 2}
Tps[5,6] = {{... four coefficients ...}, -1}

Tps[6,2] = {{... six coefficients ...}, -4}
Tps[6,3] = {{623697/311798, 64843490201/32395500402}, 0}
Tps[6,4] = {{... six coefficients ...}, -4}
Tps[6,5] = {{1}, 2}
Tps[6,6] = {{... five coefficients ...}, -2}
```

This confirms that a correct C++ implementation must either port the relevant
DESolver `NormalizeMat` algorithm or derive a reviewed closed-form
epsilon-dependent `T(eps, gaugex)` for the b64ag six-master surface.  Applying
only the known first-sample numeric matrix would be a retained-sample-specific
shortcut and would not be a valid runtime transform.

## Symbolic Transform Probe

I also tried to emit the symbolic `ToPS[T]` map from the same DESolver source
with symbolic `eps`:

```text
de = diffeq[[1]] /. {variables[[1]] -> eta} // Factor;
{T, invT, B} = NormalizeMat[de];
ToPS[T]
```

That probe did not return within the bounded lane-local attempt.  Therefore
this slice has numeric proof of the transform shape but not a reviewed symbolic
map that can be safely transcribed into C++.

## Why No Code Shortcut Was Landed

The existing C++ path already applies retained target reduction and the
AMFlow-compatible no-integer-key `PickZeroRuleS` rule, but it does so on the
raw endpoint master representation.  The fresh next10 stripped comparator was
run after the probe and still reports:

```text
/tmp/autoibp_orch/exec/lane3_next10/full-stripped-compare30.json
matched_integral_count = 9
compared_coefficient_count = 39
passed_coefficient_count = 14
minimum_digit_agreement = 0
```

That matches lane3_next9's failed downstream-basis probe, which ruled out
returning the already-diagonalized downstream internal row basis as the missing
normalization convention.

The new `Tps` probe explains why: AMFlow's published target asymptotics are
formed after a power-series basis map that mixes upstream and downstream
normalized branches before `PickZeroRuleS`.  The transform changes both powers
and branch combinations.  It is not equivalent to changing the finite-part
selector from power zero, deleting one target row, or hardcoding the AMFlow
first-sample target values.

## Required Follow-Up

1. Port or derive the b64ag-reviewed subset of DESolver `NormalizeMat`:
   diagonal block Fuchsian/eigen normalization plus the lower off-diagonal
   `ToFuchsianGlobal` couplings for the `{{1,2},{3,4},{5,6}}` surface.
2. Represent `T(eps, gaugex)` as runtime Laurent/power-series terms and apply
   it to the transported endpoint packet before retained target reduction.
3. Add a focused regression that checks the first-sample `ToPS[T]` support
   pattern above without using AMFlow target finite values as C++ inputs.
4. Only after that, re-run the stripped `linear_propagator -> b64ag` comparator
   and update pass counts if the external AMFlow comparison improves.

## Four-Role Review

- Implementer: APPROVE Tier C.  The first-sample DESolver replay exposed the
  concrete missing basis map, but the map is too broad to implement honestly as
  a one-off numeric or target-label transform.
- Test: APPROVE Tier C.  No comparator expectation, tolerance, or M6 qualifier
  was changed.  The fresh next10 stripped comparator remains `14/39` with
  `minimum_digit_agreement=0`.
- Physics/source: APPROVE Tier C.  AMFlow computes `bcT = invT[x0].bc`, solves
  in the normalized basis, and then publishes `PSMapRuleS[ToPS[T], ...]`.
  The C++ path must model that basis publication step before target reduction.
- Anti-fake: APPROVE Tier C.  No golden target values, retained final solution
  samples, or first-sample-only matrices were imported into runtime code.
  `linear_propagator -> b64ag` remains blocked.

## Honest Status

`MIN_DIGIT_AGREEMENT_AFTER=0` and `M6_FLIPPED=false`.
