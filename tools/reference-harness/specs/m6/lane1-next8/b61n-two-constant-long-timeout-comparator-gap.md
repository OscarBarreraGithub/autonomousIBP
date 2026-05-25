# Lane 1 Next8 b61n Two-Constant Long-Timeout Comparator Gap

Status: Tier C comparator evidence. This note does not flip M6, does not
promote `complex_kinematics`, and does not set
`full_eta_zero_contour_applied=true`.

## Step Chosen

Step: rerun the stripped b61n eps0 packet after the lane1-next6 two-constant
boundary matcher with a longer timeout.

The prior lane timed out at 180 seconds before producing a packet. This lane
kept the same stripped AMFlow state and ran the current `main` binary with a
1200 second timeout. The solve completed, so the previous 180 second cutoff was
too short, but the AMFlow comparator still stayed at the old digit floor.

## Commands

```text
timeout 1200s ./build/amflow-cli solve-series \
  /tmp/autoibp_orch/exec/lane1_next2/complex_kinematics.stripped-state.json \
  --eps-order 0 --digits 80 \
  --out /tmp/autoibp_orch/exec/lane1_next8/complex_kinematics.two-constant-long-timeout.eps0.cpp-result.json

python3 tools/reference-harness/scripts/compare_cpp_vs_amflow.py \
  --cpp-result /tmp/autoibp_orch/exec/lane1_next8/complex_kinematics.two-constant-long-timeout.eps0.cpp-result.json \
  --amflow-golden tools/reference-harness/specs/phase0/complex_kinematics.golden-manifest.json \
  --tolerance-digits 50 \
  > /tmp/autoibp_orch/exec/lane1_next8/complex_kinematics.two-constant-long-timeout.eps0.compare50.json
```

The solve exited 0 after `real 504.54` seconds.

## Comparator Result

```text
passed=false
compared_coefficient_count=14
passed_coefficient_count=10
minimum_digit_agreement=2
```

Failing coefficients remain the same coupled-row coefficients:

```text
box[1,0,1,1] eps^0   real/imag agreement 2/2
box[1,1,1,1] eps^-2  real/imag agreement 5/6
box[1,1,1,1] eps^-1  real/imag agreement 4/4
box[1,1,1,1] eps^0   real/imag agreement 3/4
```

The runtime did exercise the new two-constant boundary matcher:

```text
coupled_frobenius_boundary_condition_solve=2x2-match
coupled_frobenius_free_constant_rows=[5, 6]
coupled_frobenius_free_constant_source=upstream-rk-propagated-match-vector
coupled_frobenius_boundary_condition_residual_abs=3.62662389011560151744831696472992219609e-94
```

But the publication guard still failed closed before the coupled-row
coefficients entered the public packet:

```text
selected coupled-row refinement error 160.3761514976891537827532907754813424671
relative error 160.3761514976891537827532907754813424671
coefficient_publication=false
final_solution_samples_used_as_input=false
full_eta_zero_contour_applied=false
```

## Artifacts

```text
df40f2f1958669cf3222e769564cbf10f697c675fc23f10d395b4bb46d301dfb  complex_kinematics.two-constant-long-timeout.eps0.cpp-result.json
52a3b624839b72f5957267e5a672037d25748e9de588ffe5a227ce5e2ac631ff  complex_kinematics.two-constant-long-timeout.eps0.compare50.json
b484fcd44c610015903b0fd6ebbdd818fb07b2a57b3b9d229ee0c2cb628ebd44  stripped input state
cd25bdc520e4e76daa2288522f24d2b10e9cf891a5dd651a955aaaab7bbcb0f2  phase0 complex_kinematics golden manifest
```

## Four-Role Review

- Role A, implementation: APPROVE Tier C. The lane1-next6 matcher is reached
  and solves the two free rows with tiny boundary residual, but the downstream
  refinement guard still blocks publication.
- Role B, tests: APPROVE Tier C. The requested long-timeout comparator was run
  against the phase0 AMFlow golden at 50 digits and records the unchanged
  failing coefficients.
- Role C, numerics: APPROVE Tier C. The 504 second runtime is a long
  high-precision eta-infinity and contour solve, not a 180 second evidence
  cutoff issue. The endpoint residual remains far above the scoped publication
  budget.
- Role D, anti-fake: APPROVE Tier C only. The stripped input omits retained
  final solution samples, no comparator tolerance was loosened, no self
  comparison was used, and `full_eta_zero_contour_applied` remains false.

## Honest Status

`MIN_DIGIT_AGREEMENT_AFTER=2`

`M6_FLIPPED=false`
