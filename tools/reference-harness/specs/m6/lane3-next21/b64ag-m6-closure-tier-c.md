# Lane 3 Next 21 B64ag M6 Closure Tier-C

Status: Tier-C. No `full_eta_zero_contour_applied=true` flip was made.

## Decision

Lane 3 next21 did not promote `linear_propagator` to `reference-captured`
and did not declare a b64ag optional capture packet. The b64ag numeric
comparator clears the requested 50-digit fact, but the formal M6 qualifier
still fails because phase-0 runtime-lane blockers remain pending.

## Fresh Evidence

Detached worktree:

`/tmp/autoibp_worktrees/lane3_next21_20260526_0152`

Rebased baseline:

`origin/main = f28c05f9c7cfa28d44bbd9354f59c0e43444c3b5`

Fresh packet-set reruns under `/tmp/autoibp_orch/exec`:

- `lane3_next21_rebased_qualification_readiness.json`
- `lane3_next21_rebased_phase0_packet_comparison.json`
- `lane3_next21_rebased_phase0_correct_digits.json`
- `lane3_next21_rebased_phase0_failure_codes.json`
- `lane3_next21_rebased_phase0_qualification.json`
- `lane3_next21_rebased_m6_qualification.json`

The retained packet-set checks pass for the already captured packet roots:

- `packet_set_reference_comparison_passed=true`
- `packet_set_correct_digits_passed=true`
- `minimum_observed_correct_digits_across_packet_set=58`
- `packet_set_failure_code_audits_complete=true`
- `phase0_packet_set_qualified=true`

The formal M6 qualifier still reports:

- `current_state=blocked-on-phase0-runtime-lanes`
- `milestone_m6_ready=false`
- `phase0_pending_runtime_lanes_closed=false`
- `blocked_runtime_lanes=["b61n","b63n","b64ag"]`

Pending phase-0 ids are still:

- `automatic_phasespace`
- `complex_kinematics`
- `feynman_prescription`
- `linear_propagator`

## B64ag Digit Fact

The b64ag full stripped comparator was copied from the fresh `amflow-tests`
run to:

- `/tmp/autoibp_orch/exec/lane3_next21_rebased_b64ag_full_stripped_result.json`
- `/tmp/autoibp_orch/exec/lane3_next21_rebased_b64ag_full_stripped_compare50.json`

It confirms:

- `comparison=cpp-vs-amflow`
- `benchmark_id=linear_propagator`
- `passed=true`
- `tolerance_digits=50`
- `matched_integral_count=9`
- `compared_coefficient_count=39`
- `passed_coefficient_count=39`
- minimum detailed digit agreement: `51`

The copied runtime result also still reports:

- `continuation.full_eta_zero_contour_applied=false`
- `continuation.blocked_reason` is non-empty
- summary includes `retained_solution_samples_used=false`
- summary includes `final_solution_samples_used_as_input=false`

## Readiness Audit

The dedicated b64ag readiness helper was run as:

`tools/reference-harness/scripts/audit_b64ag_golden_recapture_readiness.py`

with the copied runtime result, copied 50-digit comparator, and
`tools/reference-harness/specs/phase0/linear_propagator.amflow-state.json`.
It exited nonzero and wrote:

`/tmp/autoibp_orch/exec/lane3_next21_rebased_b64ag_golden_recapture_readiness.json`

Key result:

- `status=blocked`
- `golden_recapture_ready=false`
- `m6_closure_claimed=false`
- `full_eta_zero_contour_applied_claimed_by_helper=false`
- primary postmortem failure code: `b64ag_runtime_scope_not_full_contour`

The helper also reports missing runtime-provenance/full-contour diagnostic
contract fields and packet-shape mismatches against its current readiness
contract. Those failures make a formal M6 flip unsafe in this iteration.

## Four-Role Review

Role A, harness state: REJECT flip. The current scaffold has no b64ag
optional packet root and leaves `linear_propagator` pending.

Role B, numeric comparator: APPROVE the narrow digit fact only. The b64ag
comparison passes `39/39` rows at the 50-digit threshold with minimum agreement
`51`, but this is not sufficient to close M6.

Role C, provenance: REJECT flip. The copied runtime result states
`retained_solution_samples_used=false` and
`final_solution_samples_used_as_input=false`, but it does not publish the full
runtime provenance and full-contour diagnostics required by the readiness
helper.

Role D, anti-fake qualifier: REJECT flip. The formal M6 qualifier is not
`milestone-m6-qualified`; it is `blocked-on-phase0-runtime-lanes`.

Unanimity for M6 flip: no.

## Exact Remaining Work

To close this honestly, the next iteration needs all of:

1. A b64ag optional phase-0 packet root that captures `linear_propagator` and
   is accepted by `qualification_readiness.py`.
2. A scaffold update setting only `linear_propagator` to
   `current_evidence_state=reference-captured`, declaring that optional packet,
   and dropping `next_runtime_lane`.
3. Runtime evidence that passes
   `audit_b64ag_golden_recapture_readiness.py`, including
   `full_eta_zero_contour_applied=true`, empty blocked reason, runtime
   provenance, and full-contour diagnostics.
4. A fresh `qualify_milestone_m6.py` result with
   `phase0_pending_ids=[]`, `blocked_phase0_examples=[]`, and
   `milestone_m6_ready=true`.
