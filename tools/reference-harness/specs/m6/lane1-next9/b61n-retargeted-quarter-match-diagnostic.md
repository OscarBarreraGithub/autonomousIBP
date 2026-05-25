# Lane 1 Next9 b61n Retargeted Quarter-Match Diagnostic

Status: Tier C diagnostic plus guarded runtime step. This does not flip M6,
does not promote `complex_kinematics`, and does not set
`full_eta_zero_contour_applied=true`.

## Step Chosen

Step: option (a), retarget the coupled Frobenius two-free-constant match away
from the numerically bad `eta=-0.0625*I` point.

The previous two-constant matcher matched at the final pre-endpoint waypoint.
For the retained b61n contour this was `eta=-0.0625*I`, exactly where the
refinement peak and infinite LU pivot-ratio diagnostics were recorded. This
lane keeps the reviewed contour and endpoint guard, but when the coupled
matcher sees a pre-endpoint match radius below `0.25`, it retargets the live
match to `eta=-0.25*I` on the same lower-half-plane axis.

## Probe Result

Command:

```text
timeout 1200s ./build/amflow-cli solve-series \
  /tmp/autoibp_orch/exec/lane1_next2/complex_kinematics.stripped-state.json \
  --eps-order 0 --digits 80 \
  --out /tmp/autoibp_orch/exec/lane1_next9/complex_kinematics.retargeted-quarter-match.eps0.cpp-result.json
```

The solve exited 0 after `real 88.14` seconds. The prior long-timeout
baseline took about 504 seconds, so the retargeted match removes most of the
near-endpoint runtime cost. It does not repair the coupled-row values.

Comparator:

```text
python3 tools/reference-harness/scripts/compare_cpp_vs_amflow.py \
  --cpp-result /tmp/autoibp_orch/exec/lane1_next9/complex_kinematics.retargeted-quarter-match.eps0.cpp-result.json \
  --amflow-golden tools/reference-harness/specs/phase0/complex_kinematics.golden-manifest.json \
  --tolerance-digits 50 \
  > /tmp/autoibp_orch/exec/lane1_next9/complex_kinematics.retargeted-quarter-match.eps0.compare50.json
```

Observed comparator status:

```text
passed=false
compared_coefficient_count=14
passed_coefficient_count=10
minimum_digit_agreement=2
```

The same four coupled-row coefficients fail:

```text
box[1,0,1,1] eps^0   real/imag agreement 2/2
box[1,1,1,1] eps^-2  real/imag agreement 5/6
box[1,1,1,1] eps^-1  real/imag agreement 4/4
box[1,1,1,1] eps^0   real/imag agreement 3/4
```

The runtime exercised the retargeted matcher:

```text
coupled_frobenius_match_eta=0 - 0.25*I
coupled_frobenius_match_eta_policy=retargeted-to-minimum-radius-0.25
coupled_frobenius_boundary_condition_residual_abs=4.417668562076380818627455235803835275054e-93
```

The publication guard still failed closed:

```text
selected coupled-row refinement error 160.3554859460147357203562888199531620796
relative error 160.3554859460147357203562888199531620796
refinement_error_peak_eta=0 - 0.25*I
refinement_error_peak_matrix_pivot_ratio_abs=inf
coefficient_publication=false
full_eta_zero_contour_applied=false
```

## Artifacts

```text
4419314d97ce26cbd7daefb205a361a0750400b3adf528eff0ec594e4aacb5ac  complex_kinematics.retargeted-quarter-match.eps0.cpp-result.json
571c872d539236dafa9b96780a237b4cdbd81001feca4e45e76372588d82b2e8  complex_kinematics.retargeted-quarter-match.eps0.compare50.json
b484fcd44c610015903b0fd6ebbdd818fb07b2a57b3b9d229ee0c2cb628ebd44  stripped input state
cd25bdc520e4e76daa2288522f24d2b10e9cf891a5dd651a955aaaab7bbcb0f2  phase0 complex_kinematics golden manifest
```

## Four-Role Review

- Role A, implementation: APPROVE Tier C. The retarget is restricted to the
  reviewed coupled Frobenius endpoint path and records the match policy in
  diagnostics.
- Role B, tests: APPROVE Tier C. A synthetic two-free-constant regression
  covers the `-0.0625*I` to `-0.25*I` retarget and verifies endpoint constants
  are still recovered.
- Role C, numerics: APPROVE Tier C only. The retarget reduces solve time but
  leaves the coupled-row refinement residual at about 160 and the comparator
  digit floor at 2.
- Role D, anti-fake: APPROVE Tier C only. The stripped input omits retained
  final solution samples, the comparator uses the phase0 AMFlow golden, no
  tolerance was loosened, and `full_eta_zero_contour_applied` remains false.

## Honest Status

`MIN_DIGIT_AGREEMENT_AFTER=2`

`M6_FLIPPED=false`
