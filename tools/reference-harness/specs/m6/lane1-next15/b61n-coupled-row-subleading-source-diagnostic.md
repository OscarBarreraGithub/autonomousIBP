# Lane 1 Next15 b61n Coupled-Row Subleading Source Diagnostic

Status: Tier C diagnostic. This note does not flip M6, does not promote
`complex_kinematics`, and does not set `full_eta_zero_contour_applied=true`.

## Step Chosen

Chosen atomic step: approach (a), mine the fresh per-coefficient diagnostics for
the exact 11-digit floor and identify the shared term.

The detached lane15 worktree was rebuilt from `origin/main` baseline
`7600551284ddbeee772ea7a92418f83eeeaa9f31`. The b61n stripped eps0 packet was
rerun without retained final solution samples:

```text
jq '.boundary_state.files |= (if has("solution") then del(.solution) else . end) |
    .solution_sample_cache.enabled = true' \
  tools/reference-harness/specs/phase0/complex_kinematics.amflow-state.json \
  > /tmp/autoibp_orch/exec/lane1_next15_probe/complex_kinematics.stripped-state.json

./build/amflow-cli solve-series \
  /tmp/autoibp_orch/exec/lane1_next15_probe/complex_kinematics.stripped-state.json \
  --eps-order 0 --digits 80 \
  --out /tmp/autoibp_orch/exec/lane1_next15_probe/complex_kinematics.stripped.eps0.digits80.cpp-result.json

python3 tools/reference-harness/scripts/compare_cpp_vs_amflow.py \
  --cpp-result /tmp/autoibp_orch/exec/lane1_next15_probe/complex_kinematics.stripped.eps0.digits80.cpp-result.json \
  --amflow-golden tools/reference-harness/specs/phase0/complex_kinematics.golden-manifest.json \
  --tolerance-digits 50 \
  > /tmp/autoibp_orch/exec/lane1_next15_probe/complex_kinematics.stripped.eps0.compare50.json
```

The comparator failed honestly:

```text
passed=false
compared_coefficient_count=14
passed_coefficient_count=10
minimum_digit_agreement=11
```

## Limiting Coefficients

The fresh failures are:

| coefficient | real/imag digits | relative error |
| --- | ---: | ---: |
| `box[1,0,1,1] eps^0` | `11/11` | `2.2076522212226090e-9` |
| `box[1,1,1,1] eps^-2` | `46/46` | `5.1083464122410221e-42` |
| `box[1,1,1,1] eps^-1` | `12/13` | `2.3198463908588170e-9` |
| `box[1,1,1,1] eps^0` | `12/12` | `4.0819758448562572e-9` |

The exact 11-digit ceiling is therefore the finite coefficient of the first
coupled row, `box[1,0,1,1] eps^0`. The row-6 leading pole is not part of the
same ceiling: it still agrees to 46 digits. The row-6 subleading and finite
terms carry the same `~1e-9` relative-error scale as row 5.

## Shared Term

The coupled-row audit reports the lower-triangular source order:

```text
transport_order=[box[1,0,1,1] -> box[1,1,1,1]]
coupled_row_dependencies={
  box[1,0,1,1]<-[box[0,0,0,1], box[1,0,1,0], box[1,0,0,1], box[0,0,1,1]];
  box[1,1,1,1]<-[box[0,0,0,1], box[1,0,1,0], box[1,0,0,1], box[0,0,1,1], box[1,0,1,1]]
}
source_anchor_epsilon_order=eps^2
source_anchored_evolution_rows=[5, 6]
```

That makes `box[1,0,1,1] eps^0` the shared inhomogeneous source for the row-6
subleading coefficients. The comparator pattern matches this topology:

- row 5 finite term: `~2.21e-9` relative error and `11/11` digits;
- row 6 pole term: independent leading behavior, `~5.11e-42` relative error;
- row 6 eps^-1 term: inherits the row-5 scale, `~2.32e-9`;
- row 6 finite term: inherits and amplifies the same scale, `~4.08e-9`.

The ceiling is not the Frobenius recurrence order itself. The endpoint matcher
still reports `coupled_frobenius_order=32`, 192 samples, and a basis residual of
`3.6266e-94` on the earlier nonpublishing diagnostic surface; the fresh
publishing surface reports source anchoring with
`max_relative_error_abs=7.5960809837572414e-22`. The loss appears only after
the coupled endpoint samples are converted back into epsilon Laurent
coefficients.

## Epsilon-Sample Conditioning

The retained epsilon samples are tightly clustered:

```text
1.4824772602982930e-14
1.4971552529745138e-14
1.5118332456507345e-14
1.5265112383269552e-14
1.5411892310031759e-14
1.5558672236793966e-14
1.5705452163556174e-14
```

On the row-5 fit, the published requested coefficient is plausible but the guard
coefficients explode:

```text
box[1,0,1,1] eps^0 = 0.033346682598731204... - 0.019306835861440871... I
box[1,0,1,1] eps^3 = 6.0868217328832e13 + 3.0835177208996e13 I
box[1,0,1,1] eps^4 = -2.3927392344030e27 - 1.2121356850184e27 I
box[1,0,1,1] eps^5 = 5.2251754739305e40 + 2.6470170929489e40 I
box[1,0,1,1] eps^6 = -4.8899321563530e53 - 2.4771864726469e53 I
```

This is the concrete structural wall: the source-anchored endpoint values are
good enough to publish the leading row-6 pole, but the clustered epsilon samples
make the row-5 finite coefficient the unstable subtraction term. Row 6 then
uses row 5 as a lower-triangular source, so its subleading coefficients inherit
the row-5 floor.

## Next Fix Target

The next implementation attempt should avoid extracting row 5 from clustered
sample values. The most direct path is coefficient-level coupled-row transport:
evolve the row-5 `eps^0` coefficient and the row-6 `eps^-1/eps^0` coefficients
as Laurent coefficients during the endpoint transport, rather than fitting them
afterward from seven `~1.5e-14` samples. A pure tolerance change, scalar
Frobenius order bump, or lower-order Vandermonde fit has already been ruled out.

## Four-Role Review

- Role A, implementation: APPROVE Tier C. The step is scoped to a fresh
  diagnostic rerun and a coefficient-source analysis; no runtime behavior or
  tolerance was changed.
- Role B, tests: APPROVE Tier C. The unchanged comparator was rerun at
  `--tolerance-digits 50` from a freshly rebuilt detached worktree.
- Role C, numerics: APPROVE Tier C. The row-6 leading pole accuracy versus
  row-5/row-6 subleading `~1e-9` errors isolates the shared row-5 finite
  source, not a global contour or sign error.
- Role D, anti-fake: APPROVE Tier C only. The stripped state removes retained
  final solution samples, `full_eta_zero_contour_applied` remains false, and
  M6 remains unclaimed.

## Honest Status

`MIN_DIGIT_AGREEMENT_AFTER=11`

`M6_FLIPPED=false`
