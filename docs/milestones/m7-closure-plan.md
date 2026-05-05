APPROVE final design artifact

Milestone closure verdict: REJECT M7 closure.

# M7 Milestone Closure Plan

## Top-Level Disagreements To Preserve

- Role A marked docs completion as `PARTIAL` when no docs-completion sidecar was supplied. Role B, Role C, and the lane60 generated sidecar run show the current docs-completion marker audit can be `PASSING` when `review_release_docs_completion.py` is run and consumed by `release_signoff_readiness.py`.
- Role B treats the final M7 readiness helper itself as an infrastructure blocker because `release_signoff_ready` is currently always `false`. Role C frames the main gap as evidence production with most scaffolding already present. This plan preserves both points: the sidecar scaffolding exists, but a final non-blocked release-readiness path or final release-packet consumer still has to land.
- Role A, Role B, Role C, and Role D all reject M7 closure. Role D explicitly approves this final planning artifact only as a blocker-preserving M7 plan, not as release readiness.
- M5 has moved since lane20: `tools/reference-harness/specs/m5/m5-qualification-lane58.json` reports `m5_feature_parity_passed=true` with `390/390` coefficients and no missing examples or runtime features. The M7/M6 readiness path still publishes phase-0 runtime blockers from `qualification-benchmarks.json`, so M5 evidence must be reconciled into the qualification/release-readiness truth surface before it is used as a release prerequisite.

## Sources

- Role inputs: lane60 Role A primary planner, Role B independent reviewer, Role C verification-strategy auditor, and Role D synthesis over A/B/C.
- Canonical docs: `docs/full-amflow-completion-roadmap.md`, `docs/verification-strategy.md`, `docs/implementation-ledger.md`, `docs/release-signoff-checklist.md`, `references/case-studies/selected-benchmarks.md`, `tools/reference-harness/templates/qualification-benchmarks.json`, and `tools/reference-harness/templates/release-signoff-checklist.json`.
- Current generated lane60 summaries: `/tmp/autoibp_orch/exec/lane60_current_state/qualification-readiness.json`, `phase0-qualification.json`, `case-study-qualification.json`, `m6-qualification.json`, `release-qualification-corpus.json`, `release-performance.json`, `release-diagnostic.json`, `release-docs-completion.json`, `release-parity-signoff.json`, and `release-readiness.json`.
- Current M5 artifact: `tools/reference-harness/specs/m5/m5-qualification-lane58.json`.

## Exact Acceptance Criteria

M7 is the Phase G release gate. It depends on `Milestone M6`, and Phase G requires the qualification corpus to pass, diagnostics and performance to be reviewed on the mandatory benchmark set, and a release packet to record docs/parity sign-off against `docs/release-signoff-checklist.md` (`docs/full-amflow-completion-roadmap.md:656-661`; `docs/full-amflow-completion-roadmap.md:791-796`).

The release checklist requires a passing M6 summary, a fresh rebuild gate on the exact candidate tree, pinned retained reference roots, aligned public/durable docs, diagnostics review, performance review, docs completion review, parity sign-off review, mandatory release notes, and final reviewer dispositions (`docs/release-signoff-checklist.md:24-66`).

The machine-readable checklist freezes three release prerequisites: `milestone-m6`, `phase-f-feature-parity`, and `retained-reference-evidence`; and five review sections: `qualification-corpus`, `performance-review`, `diagnostic-review`, `docs-completion`, and `parity-signoff` (`tools/reference-harness/templates/release-signoff-checklist.json:19-138`).

The executable M7 path is sidecar-based. `release_signoff_readiness.py` consumes qualification readiness plus optional phase-0, case-study, qualification-corpus, performance, diagnostic, docs-completion, and parity-signoff summaries. It preserves blockers for each prerequisite and review section, and currently returns `"release_signoff_ready": false` unconditionally (`tools/reference-harness/scripts/release_signoff_readiness.py:1142-1766`).

## Current Criteria And Evidence

### PASSING Criteria

- M7 scaffold and consumers exist. The checklist, JSON template, readiness helper, and populated sidecar producer scripts are present and recorded in the roadmap and ledger as release-prep scaffolding, not closure evidence (`docs/full-amflow-completion-roadmap.md:678-741`; `docs/implementation-ledger.md:243-254`; `docs/implementation-ledger.md:262`; `docs/implementation-ledger.md:269-270`).
- M5 feature-parity evidence is passing as an M5-only artifact. `m5-qualification-lane58.json` records `current_state = "m5-feature-parity-passed"`, `aggregate_passed_coefficient_count = 390`, no missing example classes, no missing runtime features, and explicit non-claims for M6, M7, and release readiness (`tools/reference-harness/specs/m5/m5-qualification-lane58.json:1-8`; `tools/reference-harness/specs/m5/m5-qualification-lane58.json:171-197`; `tools/reference-harness/specs/m5/m5-qualification-lane58.json:198-268`).
- Docs-completion review is passing for the current marker audit when its sidecar is generated. The lane60 `release-readiness.json` records `docs_completion_current_state = "docs-completion-reviewed"`, no docs blockers, all six checklist targets present, and `docs-completion` review-section status `reviewed` (`/tmp/autoibp_orch/exec/lane60_current_state/release-readiness.json:225-287`; `/tmp/autoibp_orch/exec/lane60_current_state/release-readiness.json:648-664`).

### PARTIAL Criteria

- Phase-F/M5 as a release prerequisite is only partially reconciled. M5 has a passing M5-only artifact, but the qualification/release-readiness scaffold still reports `automatic_phasespace`, `complex_kinematics`, `feynman_prescription`, and `linear_propagator` as phase-0 pending runtime lanes `b63n`, `b61n`, `b63n`, and `b64ag` (`tools/reference-harness/templates/qualification-benchmarks.json:63-125`; `/tmp/autoibp_orch/exec/lane60_current_state/release-readiness.json:8-31`; `/tmp/autoibp_orch/exec/lane60_current_state/release-readiness.json:530-541`).
- Retained reference evidence is present but not qualified. Required and optional retained roots are coherent enough for `qualification_readiness.py`, but phase-0 qualification remains blocked on correct-digit thresholds, missing failure-code audits, and missing required typed failure codes (`/tmp/autoibp_orch/exec/lane60_current_state/release-readiness.json:382-421`; `/tmp/autoibp_orch/exec/lane60_current_state/release-readiness.json:543-565`).

### FAILING Criteria

- M6 is failing. `qualify_milestone_m6.py` requires a qualified phase-0 packet set, no pending phase-0 runtime lanes, and qualified case-study families before `milestone_m6_ready=true` (`tools/reference-harness/scripts/qualify_milestone_m6.py:637-711`). The lane60 readiness summary blocks `milestone-m6` on phase-0 pending examples, phase-0 correct-digit and failure-code blockers, missing/failed case-study numerics, and the singular endpoint runtime blocker (`/tmp/autoibp_orch/exec/lane60_current_state/release-readiness.json:505-528`).
- Case-study qualification is failing. Current generated evidence reports missing numeric sidecars for `ttbar-h`, `ttbar-w`, `diphoton-heavy-quark-form-factors`, `h-to-bb`, `single-top-planar-nonplanar`, and `one-singular-endpoint-case`; `ttbar-j` has only 36 correct digits against the 50-digit floor; and `one-singular-endpoint-case` remains blocked on `b62p` (`/tmp/autoibp_orch/exec/lane60_current_state/release-readiness.json:63-102`; `/tmp/autoibp_orch/exec/lane60_current_state/release-readiness.json:445-461`). The strong precision anchors remain `ttbar-h` and diphoton (`/tmp/autoibp_orch/exec/lane60_current_state/release-readiness.json:99-102`; `references/case-studies/selected-benchmarks.md:45-58`).
- Qualification-corpus review is failing. The generated sidecar is present but blocked on unqualified phase-0 and case-study verdicts, pending phase-0 examples, missing failure-code evidence, missing case-study numerics, and an unreviewed closed benchmark-family coverage statement (`/tmp/autoibp_orch/exec/lane60_current_state/release-readiness.json:422-503`; `/tmp/autoibp_orch/exec/lane60_current_state/release-readiness.json:570-602`).
- Performance review is failing. The generated performance sidecar is blocked on missing mandatory benchmark timings, clean-rebuild performance review, and unstable-performance-run review (`/tmp/autoibp_orch/exec/lane60_current_state/release-readiness.json:329-359`; `/tmp/autoibp_orch/exec/lane60_current_state/release-readiness.json:603-625`).
- Diagnostic review is failing. The generated diagnostic sidecar is blocked on typed failure-path preservation review and retained unstable-run diagnostic evidence (`/tmp/autoibp_orch/exec/lane60_current_state/release-readiness.json:192-218`; `/tmp/autoibp_orch/exec/lane60_current_state/release-readiness.json:627-646`).
- Parity sign-off is failing. The generated parity sidecar is blocked on qualification closure, performance review, diagnostic review, and docs-completion note review (`/tmp/autoibp_orch/exec/lane60_current_state/release-readiness.json:288-328`; `/tmp/autoibp_orch/exec/lane60_current_state/release-readiness.json:665-690`).
- Final release readiness is failing. The generated release summary records `"release_signoff_ready": false`, and the helper currently hard-codes that field to false (`/tmp/autoibp_orch/exec/lane60_current_state/release-readiness.json:568`; `tools/reference-harness/scripts/release_signoff_readiness.py:1762-1766`).

## Shortest Atomic Landing Chain

1. Reconcile the M5 lane58 verdict into M6/M7 readiness. Either make the qualification/release-readiness path consume the accepted M5 artifact, or explicitly preserve the phase-0 runtime blockers as separate M6 evidence blockers with no M5 overclaim.
2. Publish packet-shaped candidate roots for the retained phase-0 packet split and produce required `failure-code-audit.json` sidecars.
3. Run `compare_phase0_packet_set_to_reference.py`, `score_phase0_packet_set_correct_digits.py`, `audit_phase0_packet_set_failure_codes.py`, and `qualify_phase0_packet_set.py` to a passing phase-0 verdict.
4. Produce missing case-study numeric sidecars and repair `ttbar-j` below-threshold evidence. Required missing or failing rows now include `ttbar-j`, `ttbar-h` at 100 digits, `ttbar-w`, diphoton at 200 digits, `h-to-bb`, single-top planar/nonplanar, and `one-singular-endpoint-case`.
5. Retire `one-singular-endpoint-case -> b62p` with live branch-aware singular endpoint runtime and numeric evidence, not metadata-only sidecars.
6. Run `compare_case_study_numeric_results.py`, `qualify_case_study_families.py`, and `qualify_milestone_m6.py` to a passing M6-only summary.
7. Produce a complete `release-qualification-corpus` sidecar from the accepted M6 evidence and a reviewed closed benchmark-family coverage statement.
8. Produce a complete `release-performance-review` sidecar with mandatory benchmark timings, clean rebuild performance evidence, benchmark-family scope, and unstable-run disposition.
9. Produce a complete `release-diagnostic-review` sidecar with required failure-code profiles, typed failure paths, retained unstable-run evidence, and known-regression outcomes reviewed.
10. Rerun `review_release_docs_completion.py` after final docs updates and keep the passing docs-completion sidecar.
11. Produce a complete `release-parity-signoff` sidecar after qualification, performance, diagnostics, and docs are all reviewed.
12. Land the final non-blocked release-readiness path or final release-packet consumer, then run `release_signoff_readiness.py` with all sidecars and zero blockers.

## Worker Prompt Skeletons

Worker 1, M5/M7 readiness reconciliation: Read `tools/reference-harness/specs/m5/m5-qualification-lane58.json`, `qualification_readiness.py`, `qualification-benchmarks.json`, and `release_signoff_readiness.py`. Make the M7 prerequisite state consume accepted M5 evidence or explicitly separate M5 closure from M6 phase-0 qualification blockers. Preserve `does_not_claim_m6`, `does_not_claim_m7`, and `does_not_claim_release_readiness`; do not modify runtime semantics.

Worker 2, phase-0 M6 packet publisher and audit: Publish candidate packet roots matching the required-set, `de-d0-pair`, and `user-hook-pair` packet split. Add required failure-code audit sidecars, run packet-set comparison, correct-digit scoring, failure-code audit, and `qualify_phase0_packet_set.py`. Fail closed on benchmark-id drift, packet-label drift, missing failure-code sidecars, threshold misses, or unexpected failure codes.

Worker 3, case-study numeric completion: Produce real numeric sidecars for all selected families at the frozen thresholds, repair `ttbar-j` to at least 50 digits, hit 100 digits for `ttbar-h`, hit 200 digits for diphoton, and keep all command paths, golden manifests, C++ results, comparison summaries, and withheld claims explicit.

Worker 4, singular endpoint closure: Follow the singular runtime lane after SRL-2. Retire `one-singular-endpoint-case -> b62p` only with branch-aware endpoint extraction and a coefficient-bearing numeric sidecar at the default 50-digit floor. Metadata-only local-model sidecars are not enough.

Worker 5, M6 composer: After phase-0 and case-study verdicts pass, run `qualify_milestone_m6.py` and publish one M6-only summary with `milestone_m6_ready=true`, no pending runtime lanes, no missing sidecars, and no M7/release-readiness claim.

Worker 6, qualification-corpus release sidecar: Consume the accepted M6 summary plus phase-0 and case-study verdicts, review closed benchmark-family coverage, preserve residual blockers or carve-outs, and emit an unblocked `release-qualification-corpus` sidecar.

Worker 7, performance review sidecar: Collect mandatory benchmark timings on the pinned environment, attach clean rebuild performance evidence, compare against the reviewed baseline, disposition unstable or unreviewed runs, and emit an unblocked `release-performance-review` sidecar.

Worker 8, diagnostic review sidecar: Review required failure-code profiles, typed failure paths, retained unstable-run evidence, and known-regression outcomes. Emit an unblocked `release-diagnostic-review` sidecar without suppressing missing or degraded diagnostic paths.

Worker 9, docs and parity sign-off: Rerun docs-completion against final docs, then emit `release-parity-signoff` only after qualification closure, performance review, diagnostic review, docs completion, required inputs/outputs, prerequisite section preservation, and exact withheld claims all pass.

Worker 10, final release readiness: Add the final non-blocked `release_signoff_ready` path or a separate final release-packet consumer. It must require all release prerequisites and review sections to be satisfied, preserve the release checklist non-claims until the final approved packet, and fail closed on any sidecar/schema/blocker drift.

## Feasibility Assessment

M7 closure is not feasible now. The release scaffolding is mostly present, and docs completion can already pass its marker audit, but release readiness is blocked by real M6 qualification gaps, missing performance and diagnostic evidence, a blocked parity-signoff sidecar, and the lack of a final non-blocked readiness path.

This is feasible with further infrastructure and evidence investment. It is not a single docs cleanup. The shortest path still requires packet-shaped candidate outputs, phase-0 failure-code audit sidecars, high-precision case-study numerics, singular endpoint runtime evidence, performance timing review, diagnostic review, parity sign-off, and a final release-readiness mechanism that can actually return ready only when every prerequisite is satisfied.

## Final Consensus

Role D APPROVE final planning artifact. Approve this final design artifact as the truthful M7 closure plan. Reject any claim that M7 is currently closed or that the project is release-ready.
