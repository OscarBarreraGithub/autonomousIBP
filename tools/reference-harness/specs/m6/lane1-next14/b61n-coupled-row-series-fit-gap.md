# Lane 1 Next14 b61n Coupled-Row Series-Fit Gap

Status: Tier C diagnostic. This note does not flip M6, does not promote
`complex_kinematics`, and does not set `full_eta_zero_contour_applied=true`.

## Step Chosen

Chosen atomic step: test the two concrete precision/series fixes suggested by
lane1-next13 for the remaining b61n coupled-row 11-digit floor.

The stripped eps0 packet was rerun from `origin/main` baseline
`930935ca0867d41e1dadc5af0a79b0e94c966ff9` with:

```text
./build/amflow-cli solve-series \
  /tmp/autoibp_orch/exec/lane1_next13_probe/complex_kinematics.stripped-state.json \
  --eps-order 0 --digits 80 \
  --out /tmp/autoibp_orch/exec/lane1_next14_probe/complex_kinematics.stripped.eps0.digits80.cpp-result.json

python3 tools/reference-harness/scripts/compare_cpp_vs_amflow.py \
  --cpp-result /tmp/autoibp_orch/exec/lane1_next14_probe/complex_kinematics.stripped.eps0.digits80.cpp-result.json \
  --amflow-golden tools/reference-harness/specs/phase0/complex_kinematics.golden-manifest.json \
  --tolerance-digits 50 \
  > /tmp/autoibp_orch/exec/lane1_next14_probe/complex_kinematics.stripped.eps0.compare50.json
```

## Result

Neither tested implementation was committable:

- Tightening the b61n source-anchor and coupled-row RK78 budgets from
  `1e-20` relative / `1e-24` absolute to `1e-36` relative / `1e-48`
  absolute made the stripped solve impractically long before producing
  comparator evidence; the probe was killed.
- A bounded tightening to `1e-24` relative / `1e-32` absolute completed, but
  regressed the comparator to `minimum_digit_agreement=0`. It improved only
  the row-6 leading pole to 18/19 digits while damaging the row-6 finite term.
- A low-order requested-eps fit that avoided fitting all seven tiny epsilon
  samples as a degree-six Vandermonde polynomial also failed. It preserved the
  coupled-row floor at `minimum_digit_agreement=11`, but regressed the scalar
  first row to 14 and 28 digits because that row still relies on the full
  seven-sample fit before its diagnostics are serialized.

The low-order fit failure surface was:

```text
passed=false
compared_coefficient_count=14
passed_coefficient_count=8
minimum_digit_agreement=11
box[0,0,0,1] eps^-1: 28/28 digits
box[0,0,0,1] eps^0: 14/14 digits
box[1,0,1,1] eps^0: 11/11 digits
box[1,1,1,1] eps^-2: 44/45 digits
box[1,1,1,1] eps^-1: 12/13 digits
box[1,1,1,1] eps^0: 12/12 digits
```

Therefore the 11-digit ceiling is not fixed by a local tolerance multiplier or
by simply truncating the post-endpoint Vandermonde fit to the requested eps
order. The remaining fix must change the coupled-row endpoint representation
itself, for example by producing coefficient-level coupled-row transport or a
better conditioned set of live epsilon samples. The retained AMFlow epsilon
samples near `1.5e-14` remain too poorly conditioned for the current
sample-value endpoint path to extract row-5/6 subleading coefficients at the
50-digit floor.

## Four-Role Review

- Role A, implementation: REJECT code change. Both candidate patches were
  reverted because they either ran too long or regressed non-coupled
  coefficients.
- Role B, tests: APPROVE Tier C. The comparator was rerun at unchanged
  `--tolerance-digits 50` and captured the low-order fit regression explicitly.
- Role C, numerics: APPROVE Tier C. The evidence rules out two shallow fixes:
  tighter RK tolerances alone and requested-order Vandermonde truncation alone.
- Role D, anti-fake: APPROVE Tier C only. No comparator tolerance was loosened,
  no retained final solution samples were consumed, and M6 remains unclaimed.

## Honest Status

`MIN_DIGIT_AGREEMENT_AFTER=11`

`M6_FLIPPED=false`
