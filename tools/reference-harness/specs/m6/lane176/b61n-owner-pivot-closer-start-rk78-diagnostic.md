# Lane 176 B61n Owner-Pivot Closer-Start RK78 Diagnostic

Status: Tier C diagnostic. This sidecar does not flip M6, does not promote
`complex_kinematics`, and does not set `full_eta_zero_contour_applied=true`.

## Step Chosen

This lane tested whether lane2-next8's owner-pivot eta-infinity recurrence
fix unlocks the previously failing b61n closer-start candidates, then ran the
current coupled-row endpoint refinement wiring on the stripped b61n input.

The production path on current `main` uses the RK78 endpoint refinement landed
by lane6 (`endpoint_refinement_integrator=fehlberg-rk78-adaptive`). It stops at
the first certified closer finite start for propagation. A temporary diagnostic
program, built outside the repository from the same internal recurrence code,
counted all 11 candidate starts without changing committed runtime behavior.

## Closer-Start Recurrence Result

Input:

```text
/tmp/autoibp_orch/exec/lane2_iter9/complex_kinematics.stripped-state.json
```

Diagnostic output:

```text
/tmp/autoibp_orch/exec/lane2_iter9/b61n_closer_count.out
/tmp/autoibp_orch/exec/lane2_iter9/b61n_closer_count_tail.out
```

Observed recurrence count:

- considered closer starts: 11
- newly certifying closer starts: 9
- still failing closer starts: 2
- remaining failure class: finite-start precision guard, not recurrence slot incompatibility

The first two candidates still fail the 70-digit guard:

```text
eta=-15822.1833420008545*I: min_certified_digits=52.9786363827
eta=-63288.7333680034178*I: min_certified_digits=68.040863274
```

Candidates 3 through 11 certify. The closest certified start is:

```text
eta=-253154.933472013671*I
min_certified_digits=83.0950112995
total_initial_error_bound_abs=2.91167452387001302462778e-64
```

The farthest checked certified start is:

```text
eta=-16590761720.021888*I
min_certified_digits=96.6196078399
total_initial_error_bound_abs=2.68701719472882309130563e-73
```

## RK78 Endpoint Refinement Result

Command:

```text
timeout 120s /tmp/autoibp_orch/exec/lane2_iter9_build/amflow-cli solve-series \
  /tmp/autoibp_orch/exec/lane2_iter9/complex_kinematics.stripped-state.json \
  --eps-order 0 --digits 80 \
  --out /tmp/autoibp_orch/exec/lane2_iter9/complex_kinematics.rk78-ownerpivot.eps0.cpp-result.json
```

The solve exited 0 and used the first certified closer start:

```text
finite_start_selection=closer-certified-eta-infinity-start
closer_start_candidate_count=3
closer_start_certified=true
endpoint_refinement_integrator=fehlberg-rk78-adaptive
final_solution_samples_used_as_input=false
```

The coupled-row refinement still failed closed before coefficient publication:

```text
failure_code=refinement-tolerance-failed
refinement_error_abs=55681.02456282516381184745055518851688918
refinement_effective_tolerance_abs=2236.810778904889259074285070443532811295
endpoint_vector_norm_abs=223681077890488925907428.5070443532811295
endpoint_relative_error_abs=2.489304195417271792215541913017093928143126e-19
```

The RK78 residual is about 24.9x above the effective tolerance, so no coupled
endpoint coefficient is certified and the runtime correctly leaves
`full_eta_zero_contour_applied=false`.

## AMFlow Comparator

Command:

```text
python3 tools/reference-harness/scripts/compare_cpp_vs_amflow.py \
  --cpp-result /tmp/autoibp_orch/exec/lane2_iter9/complex_kinematics.rk78-ownerpivot.eps0.cpp-result.json \
  --amflow-golden tools/reference-harness/specs/phase0/complex_kinematics.golden-manifest.json \
  --tolerance-digits 50 \
  > /tmp/autoibp_orch/exec/lane2_iter9/complex_kinematics.rk78-ownerpivot.eps0.compare50.json
```

Observed comparator status:

- comparator passed: false
- compared coefficients: 14
- passed coefficients: 10
- minimum digit agreement: 2

Failures remain the two coupled rows only:

```text
box[1,0,1,1] eps^0:  real/imag agreement 2/2
box[1,1,1,1] eps^-2: real/imag agreement 5/6
box[1,1,1,1] eps^-1: real/imag agreement 4/4
box[1,1,1,1] eps^0:  real/imag agreement 3/4
```

The digit floor did not improve because the endpoint refinement failed closed
and did not publish coupled-row coefficients. The remaining gap is now an
endpoint integrator/refinement-residual blocker, not the previous finite
infinity recurrence slot-pattern blocker.

## Four-Role Review

- Role A, implementation: APPROVE Tier C. The diagnostic uses the landed
  owner-pivot recurrence and current RK78 runtime path; no production flag is
  changed.
- Role B, tests: APPROVE Tier C. Fresh comparator data remains below the
  50-digit floor, and the sidecar records the exact failing coefficients.
- Role C, numerics: APPROVE Tier C. The closest certified start has 83
  certified digits, but RK78 endpoint residual exceeds the effective tolerance
  by about 24.9x before coefficient publication.
- Role D, anti-fake: APPROVE Tier C only. The stripped input omits AMFlow final
  solution samples, the comparator uses the phase-0 AMFlow golden, no tolerance
  is loosened, no self-comparison is used, and
  `full_eta_zero_contour_applied` remains false.

## Honest Status

`CLOSER_STARTS_NOW_CERTIFYING=9/11`

`MIN_DIGIT_AGREEMENT_AFTER=2`

`M6_FLIPPED=false`
