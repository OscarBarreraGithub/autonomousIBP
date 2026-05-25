# Lane 3 Next11 b64ag NormalizeMat Source-Form Gap

Tier C. This iteration does not flip M6, does not set
`full_eta_zero_contour_applied=true`, does not relax comparator tolerance, and
does not consume retained final solution samples as C++ runtime input.

## Step Chosen

Continue the b64ag `NormalizeMat` slice from lane3_next10 by trying to turn the
DESolver transform evidence into a C++ runtime map from the transported
six-master endpoint packet to AMFlow's normalized publication basis.

No runtime implementation was landed. The new probes confirm that the needed
map is a full epsilon-dependent Laurent/power-series transform. A correct C++
change needs either a source-derived closed form for the b64ag surface or a port
of the relevant DESolver normalization steps. Hardcoding numeric transform
samples would be a retained-sample shortcut, not an implementation of
`NormalizeMat`.

## New Evidence

The lane-local probe under
`/tmp/autoibp_orch/exec/lane3_next11_normalizemat_probe/` replayed the retained
b64ag differential equation through the same DESolver source used by AMFlow:

```text
{T, invT, B} = NormalizeMat[de];
DESolver`Private`ToPS[T]
```

For the first retained epsilon sample, the normalized block structure is:

```text
{{1, 2}, {3, 4}, {5, 6}}
```

The emitted `ToPS[T]` matrix has 60 nonzero coefficient rows, including the
previously identified diagonal two-by-two blocks and lower downstream couplings:

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

A second probe emitted `ToPS[T]` for 16 numeric epsilon samples
`eps = 1/1000 .. 16/1000` and produced:

```text
/tmp/autoibp_orch/exec/lane3_next11_normalizemat_probe/Tps_samples.tsv
960 rows = 16 epsilon samples * 60 transform coefficient rows
```

Those samples are useful for auditing support and future interpolation checks,
but they are not a reviewed symbolic transform. The direct symbolic attempts to
emit `ToPS[T]` with `eps` left symbolic, including a diagonal-block-only
variant, did not return within the lane-local bounded attempts.

## Why This Is Still Blocked

AMFlow's publication path is not just "multiply current endpoint masters by the
first-sample `T`". DESolver computes:

```text
{T, invT, B} = NormalizeMat[de];
bcT = (invT /. eta -> x0).bc;
SolveAsyExp[B, bcT, ...];
PSMapRuleS[ToPS[T], ...]
```

The C++ path currently transports endpoint terms in the internal six-master
representation and then applies the retained target reduction. To match AMFlow,
the runtime must model the normalized-basis boundary handoff and the final
`ToPS[T]` publication map with the correct direction and epsilon dependence.

The fresh lane11 stripped comparator was re-run after the probe:

```text
/tmp/autoibp_orch/exec/lane3_next11/full-stripped-compare30.json
matched_integral_count = 9
compared_coefficient_count = 39
passed_coefficient_count = 14
minimum_digit_agreement = 0
```

This preserves the lane3_next10 blocker honestly: the transform shape is now
better evidenced, but the runtime still has no source-derived `NormalizeMat`
implementation.

## Required Follow-Up

1. Port the b64ag-relevant subset of DESolver `NormalizeMat`: diagonal
   `ToFuchsian`/eigen normalization and `ToFuchsianGlobal` off-diagonal
   couplings for `{{1,2},{3,4},{5,6}}`.
2. Represent the resulting `T(eps, gaugex)` as runtime Laurent/power-series
   data with explicit power shifts and epsilon-rational coefficients.
3. Apply the transform in the same direction as DESolver's
   `bcT = invT[x0].bc` plus `PSMapRuleS[ToPS[T], ...]`, then feed the normalized
   publication terms into retained target reduction.
4. Add a regression that checks the first-sample support pattern above without
   importing AMFlow final target values.

## Four-Role Review

- Implementer: APPROVE Tier C. The lane produced stronger source evidence for
  the required map, but no honest one-step runtime implementation was available
  without hardcoding numeric samples.
- Test: APPROVE Tier C. The fresh comparator was re-run from a new lane11
  stripped state and remains `14/39` with `minimum_digit_agreement=0`.
- Physics/source: APPROVE Tier C. AMFlow normalizes the differential equation,
  transforms boundary data with `invT`, solves in the normalized basis, and
  publishes through `ToPS[T]`; the runtime needs that full direction, not only a
  target-label projection.
- Anti-fake: APPROVE Tier C. No golden target values, retained final solution
  samples, or first-sample-only transform tables were imported into runtime
  code. M6 remains fail-closed.

## Honest Status

`MIN_DIGIT_AGREEMENT_AFTER=0` and `M6_FLIPPED=false`.
