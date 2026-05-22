# M6 Closure Runbook

Status: runbook only. This document does not claim Milestone M6 closure, does
not edit qualification metadata, does not publish runtime evidence, and does not
mark Milestone M7 or release readiness.

## Purpose

Use this runbook only after the phase-0 runtime lanes have landed reviewed
optional packet evidence for the remaining M6 blockers:

- `complex_kinematics -> b61n`
- `automatic_phasespace -> b63n`
- `feynman_prescription -> b63n`
- `linear_propagator -> b64ag`

The runbook turns those future packets into one auditable M6 verdict chain. It
is deliberately fail-closed: any non-empty runtime-lane blocker, pending
phase-0 id, failed digit profile, missing failure-code audit, or case-study
qualification blocker stops the closure attempt.

## Non-Closure Boundary

The current retained state remains open. The M6 composer is allowed to report
`milestone-m6-qualified` only when all of these are true in the same reviewed
run:

- the phase-0 packet-set verdict passes;
- `phase0_pending_ids` is empty;
- `blocked_phase0_examples` is empty;
- the case-study-family verdict passes;
- `blocked_runtime_lanes` is empty;
- `blocking_reasons` is empty.

Do not close M6 from selected-coefficient counts, raw runtime flags, edited
frontier metadata, or a successful local build alone. M6 closure is a composed
qualification verdict over committed evidence.

## Preconditions

Before running the closure chain, verify the candidate commit has all of the
following:

1. Each retired runtime-lane row has a committed optional packet or equivalent
   reviewed packet-root evidence with a fresh C++ result, AMFlow comparison,
   correct-digit scoring, and failure-code sidecar.
2. The active digit threshold is not loosened. Phase-0 runtime-lane rows must
   satisfy the M6 packet-set threshold in the qualification scaffold.
3. Failure-code audits still report the required profile while rows are pending,
   and report no missing required codes after rows are accepted.
4. No runtime implementation consumes AMFlow final `solution` samples as live
   boundary input for the retiring lane.
5. No qualifier template, pending frontier, or blocker list is edited merely to
   hide an unresolved runtime lane.
6. The case-study-family qualification summary is current for the commit. Reuse
   an existing passing summary only when its inputs and labels still match.

If any precondition is not true, write a blocked report instead of producing an
M6 closure sidecar.

## Output Layout

Use a fresh output directory and preserve every intermediate JSON summary:

```bash
OUT=/tmp/autoibp_orch/exec/m6_closure_candidate
```

The final report should record the candidate commit SHA, the reference packet
root, the candidate packet root, every optional packet root, the exact commands,
and the SHA256 or committed path for each consumed evidence sidecar.

## Phase-0 Chain

Produce the phase-0 readiness, comparison, digit, failure-code, and packet-set
qualification summaries from the same candidate packet split:

```bash
python3 tools/reference-harness/scripts/qualification_readiness.py \
  --root <required-reference-packet-root> \
  --optional-packet-root <runtime-lane-optional-packet-root> \
  --summary-path "$OUT/qualification-readiness.json"

python3 tools/reference-harness/scripts/compare_phase0_packet_set_to_reference.py \
  --packet-root-pair <reference-packet-root>::<candidate-packet-root> \
  --summary-path "$OUT/phase0-packet-comparison.json"

python3 tools/reference-harness/scripts/score_phase0_packet_set_correct_digits.py \
  --packet-root-pair <reference-packet-root>::<candidate-packet-root> \
  --summary-path "$OUT/phase0-correct-digits.json"

python3 tools/reference-harness/scripts/audit_phase0_packet_set_failure_codes.py \
  --candidate-root <candidate-packet-root> \
  --summary-path "$OUT/phase0-failure-codes.json"

python3 tools/reference-harness/scripts/qualify_phase0_packet_set.py \
  --qualification-summary "$OUT/qualification-readiness.json" \
  --packet-set-comparison-summary "$OUT/phase0-packet-comparison.json" \
  --packet-set-correct-digit-summary "$OUT/phase0-correct-digits.json" \
  --packet-set-failure-code-summary "$OUT/phase0-failure-codes.json" \
  --summary-path "$OUT/phase0-qualification.json"
```

Repeat `--optional-packet-root` once for each runtime-lane optional packet root
that belongs to the candidate closure set.

The phase-0 qualification summary must report:

- `phase0_packet_set_qualified: true`
- `packet_set_reference_comparison_passed: true`
- `packet_set_correct_digits_passed: true`
- `packet_set_failure_code_audits_complete: true`
- `phase0_pending_ids: []`
- `blocked_phase0_examples: []`
- `blocking_reasons: []`

Any other result is an honest M6 blocker.

## Case-Study Chain

Refresh the case-study readiness, numeric comparison, and family qualification
summaries when the candidate commit changes any case-study inputs or labels:

```bash
python3 tools/reference-harness/scripts/qualification_case_study_readiness.py \
  --summary-path "$OUT/case-study-readiness.json"

python3 tools/reference-harness/scripts/compare_case_study_numeric_results.py \
  --case-study-readiness-summary "$OUT/case-study-readiness.json" \
  --numeric-evidence <case-study-numeric-evidence-json> \
  --summary-path "$OUT/case-study-numeric-comparison.json"

python3 tools/reference-harness/scripts/qualify_case_study_families.py \
  --case-study-readiness-summary "$OUT/case-study-readiness.json" \
  --case-study-numeric-summary "$OUT/case-study-numeric-comparison.json" \
  --summary-path "$OUT/case-study-qualification.json"
```

Repeat `--numeric-evidence` for every case-study numeric evidence sidecar that
the refreshed case-study summary should consume.

The case-study qualification summary must report:

- `case_study_families_qualified: true`
- `milestone_m6_ready: true`
- `blocked_case_study_families: []`
- `blocking_reasons: []`

If no case-study inputs changed, the closure report may cite the latest reviewed
committed passing case-study-family verdict instead, but it must name the file
and explain why it still matches the candidate commit.

## M6 Composer

Compose the reviewed phase-0 and case-study summaries:

```bash
python3 tools/reference-harness/scripts/qualify_milestone_m6.py \
  --phase0-qualification-summary "$OUT/phase0-qualification.json" \
  --case-study-qualification-summary "$OUT/case-study-qualification.json" \
  --summary-path "$OUT/m6-qualification.json"
```

Accept M6 closure only if the composed summary reports:

- `current_state: milestone-m6-qualified`
- `phase0_ready_for_m6: true`
- `phase0_pending_runtime_lanes_closed: true`
- `case_study_ready_for_m6: true`
- `milestone_m6_ready: true`
- `blocked_runtime_lanes: []`
- `blocking_reasons: []`

The composer still does not mark M7 or release readiness. Those remain separate
handoff steps.

## Stop Conditions

Stop and report `blocked-on-runtime-lanes` or the more specific failing state if
any of the following appears:

- `b61n`, `b63n`, `b64ag`, or any successor lane remains in
  `blocked_runtime_lanes`;
- any of the four phase-0 rows listed in `Purpose` remains in
  `blocked_phase0_examples`;
- a packet result uses selected-only evidence for a full runtime-lane row;
- a candidate result compares a value against itself;
- a digit threshold or tolerance changes without an explicit review artifact;
- a failure-code audit is missing, incomplete, or unexpectedly silent;
- a runtime path reads AMFlow final `solution` samples as live boundary input;
- `full_eta_zero_contour_applied=true` appears without matching packet-level
  evidence for every row owned by that runtime lane.

## Commit Checklist

The closure commit, when it eventually exists, should include:

- the final `m6-qualification.json` or a committed sidecar pointing to the
  exact preserved output;
- links to all phase-0 and case-study intermediate summaries;
- an implementation-ledger entry that says M6 is closed only because the M6
  composer passed;
- no M7, release-readiness, or broad AMFlow parity claim beyond the composed
  verdict;
- the standard build, CTest, direct `amflow-tests`, and direct
  `singular-runtime-lane-tests` gate results.

After that commit lands, refresh M7 qualification-corpus and parity-signoff
sidecars as a separate lane.
