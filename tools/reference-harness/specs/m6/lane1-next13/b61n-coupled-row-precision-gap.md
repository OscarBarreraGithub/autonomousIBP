# Lane 1 Next13 b61n Coupled-Row Precision Gap

Status: Tier C diagnostic. This note does not flip M6, does not promote
`complex_kinematics`, and does not set `full_eta_zero_contour_applied=true`.

## Step Chosen

Chosen atomic step: identify the coefficients still agreeing at only about
11 digits and isolate the common cause.

The fresh stripped-state eps0 solve and comparator were run from `origin/main`
baseline `1b1a8b0a752f72136f2d0bc3738c0717aa0cb215`:

```text
./build/amflow-cli solve-series \
  /tmp/autoibp_orch/exec/lane1_next13_probe/complex_kinematics.stripped-state.json \
  --eps-order 0 --digits 80 \
  --out /tmp/autoibp_orch/exec/lane1_next13_probe/complex_kinematics.stripped.eps0.digits80.cpp-result.json

python3 tools/reference-harness/scripts/compare_cpp_vs_amflow.py \
  --cpp-result /tmp/autoibp_orch/exec/lane1_next13_probe/complex_kinematics.stripped.eps0.digits80.cpp-result.json \
  --amflow-golden tools/reference-harness/specs/phase0/complex_kinematics.golden-manifest.json \
  --tolerance-digits 50 \
  > /tmp/autoibp_orch/exec/lane1_next13_probe/complex_kinematics.stripped.eps0.compare50.json
```

The comparator failed honestly:

```text
passed=false
compared_coefficient_count=14
passed_coefficient_count=10
minimum_digit_agreement=11
```

## Limiting Coefficients

The remaining sub-50 coefficients are all in the two coupled rows:

| coefficient | real/imag digits | relative error |
| --- | ---: | ---: |
| `box[1,0,1,1] eps^0` | `11/11` | `2.2076522212226090e-9` |
| `box[1,1,1,1] eps^-2` | `46/46` | `5.1083464122410221e-42` |
| `box[1,1,1,1] eps^-1` | `12/13` | `2.3198463908588170e-9` |
| `box[1,1,1,1] eps^0` | `12/12` | `4.0819758448562572e-9` |

Rows 0 through 4 still compare at 59 or more digits on the eps0 surface. The
common failure surface is therefore the coupled-row endpoint transport plus
post-endpoint Laurent fit, not the primitive endpoint series or comparator
wiring.

## Cause

The coupled-row runtime publishes endpoint samples, but the retained epsilon
samples are all near `1.5e-14`. The `box[1,1,1,1]` leading pole is already
accurate to 46 digits, while its subleading and finite coefficients are only
around 12 digits. This pattern is consistent with sample-level contour error
being amplified by the tiny-epsilon Laurent interpolation after the large pole
terms are subtracted.

The result summary reports:

```text
max_relative_error_abs=7.59608098375724141636934e-22
transported_endpoint_norm_abs=223681077890488925907096
source_anchor_trajectory_applied=true
coupled_frobenius_endpoint_matcher_applied=true
coefficient_publication=true
full_eta_zero_contour_applied=false
```

A dry-run code experiment tightened the b61n coupled-row RK78 source-anchor
and endpoint propagation budget from `1e-20` relative / `1e-24` absolute to
`1e-72` relative / `1e-90` absolute with a larger adaptive-step cap. That
experiment is not committed because it made the source-anchor reverse
trajectory fail closed at the adaptive step limit and dropped the eps0 minimum
back to 2 digits. The failure summary was:

```text
reviewed source-anchor reverse trajectory failed:
failure_code=propagation-failed
propagator_summary=complex contour waypoint propagation failed closed:
adaptive-step-limit-exceeded; accepted_steps=8192
```

So the next substantive fix should not be a blind tolerance tighten. It should
either provide a higher-precision source-anchor trajectory strategy that can
actually satisfy the tighter budget, or avoid the tiny-epsilon Vandermonde
amplification by fitting the coupled-row Laurent coefficients from a better
conditioned endpoint representation.

## Four-Role Review

- Role A, implementation: APPROVE Tier C. The fresh comparator localizes the
  remaining floor to rows 5 and 6 only, and no code path or tolerance is
  widened in this commit.
- Role B, tests: APPROVE Tier C. The diagnostic uses the current comparator
  relative-error fields at unchanged `--tolerance-digits 50`.
- Role C, numerics: APPROVE Tier C. The leading pole/subleading split and
  `~1.5e-14` epsilon sample scale point to Laurent-fit amplification of
  coupled-row sample error, not a sign, missing coefficient, or primitive
  series truncation issue.
- Role D, anti-fake: APPROVE Tier C only. The full-contour flag remains false,
  the failed tighter-budget experiment is documented rather than hidden, and
  M6 remains unclaimed.

## Honest Status

`MIN_DIGIT_AGREEMENT_AFTER=11`

`M6_FLIPPED=false`
