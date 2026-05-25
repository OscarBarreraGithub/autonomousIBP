# b64ag M6 Closure Attempt - Tier C

Status: Tier C gap evidence. This lane does not close M6, does not promote
`linear_propagator` to `reference-captured`, does not add an
`optional_capture_packet`, does not remove `next_runtime_lane`, and does not set
`full_eta_zero_contour_applied=true`.

## Verdict

The b64ag downstream packet is real but still does not satisfy the lane148 M6
qualifier requirements.

- `tools/reference-harness/specs/m6/lane148/m6-runtime-lane-qualifier-requirements.md`
  requires the default phase-0 digit floor of 50 correct digits.
- A fresh b64ag full-stripped run reached 9 retained targets and 39 compared
  coefficients without consuming retained final AMFlow solution samples.
- The 30-digit comparison passed at 39/39 coefficients, but the minimum
  observed non-sentinel digit agreement was 35.
- The 50-digit comparison failed with 31/39 coefficients passing and 8 failures.
- Runtime evidence still reports `full_eta_zero_contour_applied=false`.
- The b64ag golden-recapture readiness audit returned `blocked`.
- The M6 qualifier returned `milestone_m6_ready=false` with
  `current_state=blocked-on-phase0-runtime-lanes` and blocked lanes
  `b61n`, `b63n`, and `b64ag`.

Therefore the honest action is to keep `linear_propagator` pending on `b64ag`.

## Reproduction

Worktree:

```text
/tmp/autoibp_orch/worktrees/lane3_iter17_b64ag_20260525T131125
```

Scratch evidence:

```text
/tmp/autoibp_orch/exec/lane3_iter17_b64ag/full-stripped-result.json
/tmp/autoibp_orch/exec/lane3_iter17_b64ag/full-stripped-compare30.json
/tmp/autoibp_orch/exec/lane3_iter17_b64ag/full-stripped-compare50.json
/tmp/autoibp_orch/exec/lane3_iter17_b64ag/b64ag-readiness-audit.json
/tmp/autoibp_orch/exec/lane3_iter17_b64ag/phase0-readiness-current.json
/tmp/autoibp_orch/exec/lane3_iter17_b64ag/phase0-packet-comparison-current.json
/tmp/autoibp_orch/exec/lane3_iter17_b64ag/phase0-correct-digits-current.json
/tmp/autoibp_orch/exec/lane3_iter17_b64ag/phase0-failure-codes-current.json
/tmp/autoibp_orch/exec/lane3_iter17_b64ag/m6-qualification-current.json
```

Key command outcomes:

```text
cmake -S . -B build: exit 0
cmake --build build --parallel 1: exit 0
amflow-cli solve-series stripped linear_propagator state, --eps-order 2 --digits 50: exit 0
compare_cpp_vs_amflow.py --tolerance-digits 30: exit 0, passed=true, 39/39, minimum_digit_agreement=35
compare_cpp_vs_amflow.py --tolerance-digits 50: exit 1, passed=false, 31/39, minimum_digit_agreement=35
audit_b64ag_golden_recapture_readiness.py: exit 1, status=blocked
qualification_readiness.py on current retained packet roots: exit 0, linear_propagator remains pending on b64ag
compare_phase0_packet_set_to_reference.py on current retained packet roots: exit 0
score_phase0_packet_set_correct_digits.py on current retained packet roots: exit 0, minimum observed packet-set digits=58 for already captured rows
audit_phase0_packet_set_failure_codes.py on current retained packet roots: exit 0
qualify_milestone_m6.py with lane133 phase0 and lane115 case-study summaries: exit 0, milestone_m6_ready=false
```

The stripped input used for the b64ag solve removed
`boundary_state.files.solution` and enabled only the non-solution
`solution_sample_cache` path. The runtime summary includes:

```text
retained_solution_samples_used=false
final_solution_samples_used_as_input=false
full_eta_zero_contour_applied=false
```

## Primary Blockers

The readiness audit reported these failure-code families:

```text
b64ag_runtime_scope_not_full_contour
b64ag_full_contour_diagnostics_missing
b64ag_runtime_packet_contract_mismatch
b64ag_comparison_packet_mismatch
b64ag_digit_evidence_incomplete
b64ag_precision_floor_not_met
b64ag_provenance_contract_mismatch
b64ag_comparison_contract_mismatch
```

The first concrete 50-digit comparison failure was:

```text
gauge[1,1,1,-1,1,0,0,0,0] eps^-1 has real/imag agreement 39/49, below tolerance 50
```

## Four-Role Review

- Role A: APPROVE Tier-C. The phase-0 scaffold must not be edited because the
  b64ag packet does not meet the 50-digit floor.
- Role B: APPROVE Tier-C. The current packet readiness, packet comparison,
  digit scoring, and failure-code audit pass only for already captured rows;
  they do not include `linear_propagator`.
- Role C: APPROVE Tier-C. The M6 composer still reports open runtime lanes and
  cannot return PASS from a b64ag-only promotion.
- Role D: APPROVE Tier-C and REJECT flip. The fresh stripped b64ag solve did
  not consume final AMFlow solution samples, but it still reports
  `full_eta_zero_contour_applied=false`, lacks the full-contour diagnostics
  required by the b64ag readiness audit, and fails the 50-digit comparator.

Unanimous decision: do not flip M6 for b64ag in this iteration.
