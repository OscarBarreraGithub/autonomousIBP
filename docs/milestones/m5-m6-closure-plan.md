# M5/M6 Milestone Closure Plan

## Closure Verdicts

M5 qualifier status: `CLOSED/all-phase` for Milestone M5.

M5 full-closure status: CLOSED by lane62.

The fail-closed all-phase M5 qualifier output is committed at
`tools/reference-harness/specs/m5/m5-qualification-lane62.json`. Running

```sh
python3 tools/reference-harness/scripts/qualify_milestone_m5.py \
  --evidence-summary tools/reference-harness/specs/m5/m5-feature-surface-lane50.json \
  --all-phase-closure-decision tools/reference-harness/specs/m5/m5-all-phase-closure-decision-lane62.json \
  --out tools/reference-harness/specs/m5/m5-qualification-lane62.json
```

reports:

- `scope: m5-all-phase`
- `current_state: CLOSED/all-phase`
- `m5_all_phase_closed: true`
- `m5_feature_parity_passed: true`
- `aggregate_compared_coefficient_count: 390`
- `aggregate_passed_coefficient_count: 390`
- `blocking_reasons: []`
- no missing or unknown frozen example classes
- no missing or unknown Phase F runtime features

Lane62 adds the higher-level fail-closed M5 all-phase composer. The legacy
feature-parity path remains available and still emits
`scope: m5-feature-parity-only` with `current_state: m5-feature-parity-passed`;
the all-phase output is emitted only when the closure-decision sidecar
`tools/reference-harness/specs/m5/m5-all-phase-closure-decision-lane62.json`
approves treating that passing feature-parity verdict as the M5 milestone
closure state, and when the committed lane58 feature-parity summary matches a
fresh recomputation from the lane50 sidecar. The lane62 output preserves
`does_not_claim_m6`, `does_not_claim_m7`, and
`does_not_claim_release_readiness`.

M6 overall status: TODO/FAILING, not closeable in this lane.

M5 full closure is no longer an M6 prerequisite blocker. M6 remains blocked on
passing phase-0 packet-set qualification, case-study-family numeric
qualification, retirement of pending runtime-lane blockers where required, and a
passing `qualify_milestone_m6.py` summary. None of those M6 closure claims are
made here.

## Sources

- M5 all-phase qualifier output: `tools/reference-harness/specs/m5/m5-qualification-lane62.json`.
- M5 closure-decision sidecar: `tools/reference-harness/specs/m5/m5-all-phase-closure-decision-lane62.json`.
- M5 feature-parity qualifier input: `tools/reference-harness/specs/m5/m5-qualification-lane58.json`.
- M5 evidence sidecar: `tools/reference-harness/specs/m5/m5-feature-surface-lane50.json`.
- Fail-closed M5 verdict helper: commit `8809981` (`Add fail-closed M5 verdict helper`).
- Frozen-example and runtime-feature evidence: commit `4842629` (`Add M5 frozen-example and runtime-feature evidence`).
- Runtime-feature proof runs and lane45 evidence: commit `3aee7a1` (`Add M5 runtime-feature evidence`).
- User-defined hook, spacetime-dimension, and final lane50 sidecar evidence: commit `9e28e7f` (`Implement M5 user-defined feature-parity rows`).
- Earlier user-defined and spacetime-dimension partial evidence retained in history: commit `9fc23bd` (`Add M5 user-defined feature-parity evidence`).
- Canonical criteria: `docs/full-amflow-completion-roadmap.md`, `docs/verification-strategy.md`, `docs/parity-contract.md`, and `specs/parity-matrix.yaml`.

## M5 Acceptance Criteria

M5 is the Phase F feature-parity gate over the frozen AMFlow example classes:
`automatic_loop`, `automatic_phasespace`, `automatic_vs_manual`,
`complex_kinematics`, `differential_equation_solver`, `feynman_prescription`,
`linear_propagator`, `spacetime_dimension`, `user_defined_amfmode`, and
`user_defined_ending`.

The fail-closed helper requires all of the following before the M5 feature-parity
sidecar can pass:

- `m0b_accepted: true`
- every required frozen example class represented exactly once
- every required Phase F runtime feature represented exactly once
- every example row marked `promoted_golden: true` and `coefficient_bearing: true`
- every example row using an accepted evidence kind: `live-solver-path` or `approved-retained-state-exception`
- every comparison summary present, coefficient-bearing, passing, and at `tolerance_digits: 30`
- every comparison passing all compared coefficients with no reported failures
- every runtime-feature row using an accepted evidence kind and carrying an evidence path or reviewed exception reference
- no sidecar blockers, missing rows, unknown rows, tolerance drift, or failed comparisons

The lane58 output satisfies those feature-parity criteria across the full M5
sidecar. The lane62 all-phase output consumes that result plus the explicit
closure-decision sidecar and marks Milestone M5 closed without widening M6, M7,
or release readiness.

## Current M5 Criteria And Evidence

| Example class | Evidence | Passing coefficients |
| --- | --- | --- |
| `automatic_loop` | lane39 retained exception, eps8 comparison | `126/126`, minimum digit agreement `41` |
| `automatic_phasespace` | lane39 retained exception comparison | `11/11`, minimum digit agreement `39` |
| `automatic_vs_manual` | lane39 retained exception comparison | `89/89`, minimum digit agreement `36` |
| `complex_kinematics` | lane39 retained exception comparison | `14/14`, minimum digit agreement `41` |
| `differential_equation_solver` | lane39 retained exception comparison | `3/3`, minimum digit agreement `41` |
| `feynman_prescription` | lane39 retained exception comparison | `76/76`, minimum digit agreement `39` |
| `linear_propagator` | lane39 retained exception comparison | `57/57`, minimum digit agreement `31` |
| `spacetime_dimension` | lane50 live solver path | `2/2`, minimum digit agreement `51` |
| `user_defined_amfmode` | lane50 reviewed retained exception | `6/6`, minimum digit agreement `42` |
| `user_defined_ending` | lane50 reviewed retained exception | `6/6`, minimum digit agreement `36` |

Aggregate M5 comparison evidence is `390/390` at unchanged
`required_tolerance_digits: 30`, with `blocking_reasons: []`.

The required runtime-feature rows are also accepted:

| Runtime feature | Evidence kind | Evidence |
| --- | --- | --- |
| `arbitrary_D0` | `approved-retained-state-exception` | `runtime-feature-evidence-lane45.json#features/arbitrary_D0` |
| `fixed_eps` | `live-solver-path` | `runtime-feature-evidence-lane45.json#features/fixed_eps` |
| `complex_kinematics` | `live-solver-path` | `runtime-feature-evidence-lane45.json#features/complex_kinematics` |
| `linear_propagators` | `live-solver-path` | `runtime-feature-evidence-lane45.json#features/linear_propagators` |
| `phase_space_integration` | `live-solver-path` | `runtime-feature-evidence-lane45.json#features/phase_space_integration` |
| `feynman_prescription` | `live-solver-path` | `runtime-feature-evidence-lane45.json#features/feynman_prescription` |
| `singular_kinematics_guardrails` | `approved-retained-state-exception` | `runtime-feature-evidence-lane45.json#features/singular_kinematics_guardrails` |
| `user_defined_hooks` | `approved-retained-state-exception` | `runtime-feature-evidence-lane45.json#features/user_defined_hooks` |

The prior numeric feature-parity blockers are retired by the lane50 sidecar plus
the lane58 qualifier output. The prior milestone-closure-label blocker is retired
by the lane62 all-phase composer and closure-decision sidecar. This is not a
claim of M6 qualification-corpus passage, M7 passage, release readiness, broader
case-study numeric parity, or any loosened tolerance policy.

## Remaining M5 Blockers

None for M5 all-phase closure at lane62. The retired blockers were:

- the lack of a literal `CLOSED` state or all-phase M5 closure field in the
  lane58 feature-parity-only output
- the lane45 and lane50 sidecar non-claim wording, which remained correct for
  their local evidence but was not itself a milestone closure verdict
- the absence of a higher-level fail-closed M5 closure composer consuming the
  passing feature-parity result

## Current M6 Criteria And Evidence

M6 is the Phase G qualification-corpus milestone. It depends on M5 and M0b, but
it also requires a passing phase-0 packet-set verdict, passing case-study-family
verdicts at the frozen digit thresholds, closed pending runtime lanes, reviewed
diagnostics/performance where required, and a final passing
`qualify_milestone_m6.py` summary.

The M6 scaffolding exists, including packet comparison, correct-digit scoring,
failure-code audit, case-study numeric summary, case-study-family qualification,
and the M6 verdict composer. That scaffolding remains fail-closed: it does not
by itself claim M6 closure.

Current M6 blockers to preserve:

- no committed passing `qualify_milestone_m6.py` summary
- phase-0 candidate packet-set qualification is not closed by this lane
- mandatory case-study numeric sidecars are not all passing at the frozen thresholds
- singular endpoint qualification remains outside this M5 closure claim until M6 evidence retires it
- M7 and release-readiness review sidecars remain separate non-claims

Singular runtime lane status: SRL-1 and SRL-2 are landed, and lane64 adds SRL-3
branch/prescription ledger support. The new ledger records how the selected
half-plane and a single nonzero Feynman prescription fix the eta-zero endpoint
approach and log branch while binding that audit to the SRL-1 contour
fingerprint and SRL-2 local-model identity. It deliberately leaves
`live_endpoint_extraction_ready: false`; SRL-4 live eta-zero endpoint extraction
and SRL-5 case-study numeric qualification are still required before the
`one-singular-endpoint-case -> b62p` runtime blocker can be retired.

M6 overall status remains TODO/FAILING.

## Shortest Remaining Chain

1. Publish or refresh the phase-0 candidate
   packet roots expected by the M6 packet-set tools.
2. Run packet-set comparison, packet-set correct-digit scoring, failure-code
   audit, and `qualify_phase0_packet_set.py` with synchronized labels.
3. Produce numeric evidence sidecars for every selected case-study family at the
   frozen thresholds.
4. Run `compare_case_study_numeric_results.py` and
   `qualify_case_study_families.py`.
5. Retire any pending runtime-lane blockers only with reviewed runtime/numeric
   evidence accepted by the M6 tools.
6. Run `qualify_milestone_m6.py` and claim M6 only if that composer passes
   without blockers.

## Final Consensus

M5 is CLOSED/all-phase per the fail-closed lane62 qualifier output over the
cumulative phase-0 evidence, the committed lane58 feature-parity result, and the
lane62 closure-decision sidecar. M6 remains open and fail-closed until the M6
composer reports a passing M6-scoped verdict.
