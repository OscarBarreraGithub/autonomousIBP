# Lane 1 Next11 b61n Coupled-Row Relative-Error Diagnostic

Status: Tier C diagnostic. This note does not flip M6, does not promote
`complex_kinematics`, and does not set `full_eta_zero_contour_applied=true`.

## Step Chosen

The requested baseline `43aacccaa3db3de265944728fc9e21e6e2409672` had advanced
to `origin/main` at `e0af52fe8d9d9d05074d87e354bfa98c50d675a5`. This lane used
the current `origin/main` comparator diagnostics and reran only the comparator
against the existing stripped b61n full eps0 packet from lane1-next8:

```text
python3 tools/reference-harness/scripts/compare_cpp_vs_amflow.py \
  --cpp-result tools/reference-harness/specs/m6/lane1-next8/complex_kinematics.two-constant-long-timeout.eps0.cpp-result.json \
  --amflow-golden tools/reference-harness/specs/phase0/complex_kinematics.golden-manifest.json \
  --tolerance-digits 50 \
  > tools/reference-harness/specs/m6/lane1-next11/complex_kinematics.two-constant-long-timeout.eps0.compare50.json
```

The comparator exited nonzero as expected because the packet still fails
against AMFlow. The rerun preserves the new per-coefficient relative-error
fields.

## Comparator Result

```text
passed=false
compared_coefficient_count=14
passed_coefficient_count=10
minimum_digit_agreement=2
```

Master row map from the stripped state:

```text
row 0: box[0,0,0,1]
row 1: box[1,0,1,0]
row 2: box[1,0,0,1]
row 3: box[0,1,0,1]
row 4: box[0,0,1,1]
row 5: box[1,0,1,1]
row 6: box[1,1,1,1]
```

The exact four failing coefficients are all in coupled rows 5 and 6:

| row | coefficient | C++ value | AMFlow value | digits | relative error |
| --- | --- | --- | --- | --- | --- |
| 5 | `box[1,0,1,1] eps^0` | `0 + 0 I` | `0.0333466826716029732631471786625881976810537819931807553542 - 0.0193068358175546708506659645771451919832479750420661842973 I` | `2/2` | `1` |
| 6 | `box[1,1,1,1] eps^-2` | absent, comparator zero | `-0.0000491446434735913467070691573012159103985270990756012428 - 0.0000011986498408193011391968087146638026926470024164780791 I` | `5/6` | `1` |
| 6 | `box[1,1,1,1] eps^-1` | absent, comparator zero | `0.0005779252240636638814919675522313552168373760882124884379 + 0.0001291464135236889781737288500452009818076048479323563313 I` | `4/4` | `1` |
| 6 | `box[1,1,1,1] eps^0` | `0 + 0 I` | `-0.0023932404599206230006337233056978502621507523814154688139 + 0.0004187109773346087695456928300466781575012039894687889349 I` | `3/4` | `1` |

## Pattern

This is not a sign or normalization offset: every failing entry is a missing or
zero C++ publication against a nonzero AMFlow coefficient, so the complex,
real-part, and imaginary-part relative errors are all exactly `1`.

The row pattern is fully localized to the two coupled rows:

- row 5 is missing only its eps0 coefficient;
- row 6 is missing its eps^-2, eps^-1, and eps0 coefficients;
- rows 0 through 4 still pass at 59 or more digits where AMFlow publishes them.

The result sidecar confirms that the coupled-row runtime did execute the
two-constant Frobenius endpoint matcher, but the downstream publication guard
withheld the coupled rows:

```text
coupled_frobenius_boundary_condition_solve=2x2-match
coupled_frobenius_free_constant_rows=[5, 6]
coupled_frobenius_source_anchor_rows=[0, 1, 2, 3, 4]
selected coupled-row refinement error 160.3761514976891537827532907754813424671
relative error 160.3761514976891537827532907754813424671
coefficient_publication=false
final_solution_samples_used_as_input=false
full_eta_zero_contour_applied=false
```

The concrete next attack should therefore not be a comparator-tolerance change
or a sign flip. The missing entries are consistent with an incomplete coupled
row endpoint ansatz after the current source-anchored two-free-constant match.
The next narrow implementation attempt should test a four-constant/log-eta
coupled endpoint model for rows 5 and 6, or prove from the indicial data that
AMFlow's row-6 pole coefficients require a singular/log endpoint branch that
the present regular two-constant matcher cannot publish.

## Artifacts

```text
tools/reference-harness/specs/m6/lane1-next11/complex_kinematics.two-constant-long-timeout.eps0.compare50.json
tools/reference-harness/specs/m6/lane1-next8/complex_kinematics.two-constant-long-timeout.eps0.cpp-result.json
tools/reference-harness/specs/phase0/complex_kinematics.golden-manifest.json
```

## Four-Role Review

- Role A, implementation: APPROVE Tier C. The failed values are exactly
  localized to rows 5 and 6, and the code path shows publication is blocked by
  the coupled-row refinement budget, not by comparator wiring.
- Role B, tests: APPROVE Tier C. The current comparator output includes
  `relative_error_abs`, `real_relative_error_abs`, and
  `imag_relative_error_abs` for every coefficient and keeps the 50-digit
  tolerance unchanged.
- Role C, numerics: APPROVE Tier C. The all-ones relative-error pattern means
  AMFlow is nonzero while C++ is absent or zero; it does not support a scalar
  sign or normalization correction.
- Role D, anti-fake: APPROVE Tier C only. The evidence uses the stripped packet,
  no final AMFlow solution samples are used as input, and M6 remains unflipped.

## Honest Status

`MIN_DIGIT_AGREEMENT_AFTER=2`

`M6_FLIPPED=false`
