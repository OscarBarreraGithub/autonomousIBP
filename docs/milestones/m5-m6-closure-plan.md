APPROVE final design artifact

Milestone closure verdict: REJECT M5 closure and REJECT M6 closure.

# M5/M6 Milestone Closure Plan

## Top-Level Disagreements To Preserve

- Role A says `APPROVE`, but its approval is an approval-for-synthesis of the draft plan, not an approval of M5/M6 closure. Role A explicitly says the milestones are not ready to be claimed closed and classifies M5 as partial and M6 as blocked.
- Role B says `REJECT` and frames the decision as a direct rejection of milestone closure. It does not accept the more favorable retained-state evidence as enough for the durable M5/M6 gates.
- This consensus resolves that apparent conflict by separating artifact approval from milestone closure: the final design artifact is approved as a truthful closure plan, while the M5/M6 closure claim is rejected.
- Role C also says `REJECT`, and this cannot be papered over. Role C independently audited the same sources and rejected closure because the durable Phase F and Phase G gates are not met. This consensus preserves Role C's rejection as controlling audit evidence.
- Role A's lane20 probe read reported a `linear_propagator` solve parse failure involving `gaugex->1`, and the lane20 summaries later showed a coefficient-bearing but failing `29/57` comparison. Lane32 preserves the later passing retained-sample `linear_propagator` `57/57` comparison at unchanged 30-digit tolerance, while preserving the M5 distinction between retained-sample parity and full live gauge-link runtime closure.

## Sources

- Role inputs: `/tmp/autoibp_orch/exec/lane20_role_a_draft.md`, `/tmp/autoibp_orch/exec/lane20_role_b_independent.md`, `/tmp/autoibp_orch/exec/lane20_role_c_audit.md`.
- Canonical docs: `docs/full-amflow-completion-roadmap.md`, `docs/verification-strategy.md`, `docs/implementation-ledger.md`, `references/case-studies/selected-benchmarks.md`, `specs/parity-matrix.yaml`, `docs/parity-contract.md`.
- Probe summaries: `/tmp/autoibp_orch/exec/lane20_probe/*.compare.json` and matching `*.cpp.json`.
- Lane21 forward-roll evidence: `/tmp/autoibp_orch/exec/lane21_probe/differential_equation_solver.cpp.json` and `/tmp/autoibp_orch/exec/lane21_probe/differential_equation_solver.compare.json`.
- Lane29 forward-roll evidence: `tools/reference-harness/specs/phase0/feynman_prescription.amflow-state.json`, `/tmp/autoibp_orch/exec/lane29_feynman_prescription.cpp-result.json`, `/tmp/autoibp_orch/exec/lane29_feynman_prescription.compare.json`, and `/tmp/autoibp_orch/exec/lane29_parity_rerun/aggregate.json`.
- Lane32 aggregate-v3 evidence: `docs/phase0-packet-validation-lane32.md`, `tools/reference-harness/specs/phase0/lane32-packet-validation-aggregate.json`, and `/tmp/autoibp_orch/exec/lane32_aggregate_v3.md`.
- Lane39 forward-roll evidence: `/tmp/autoibp_orch/exec/lane39_probe/full/runs/*.compare.json` and `/tmp/autoibp_orch/exec/lane39_probe/m5-feature-surface-lane39.summary.json`.
- Lane37 eps7/eps8 forward-roll evidence: `/tmp/autoibp_orch/exec/lane37_runs/automatic_loop.eps7.compare.json` and `/tmp/autoibp_orch/exec/lane37_runs/automatic_loop.eps8.compare.json`.
- Lane47 singular runtime-lane design: `docs/theory/singular-runtime-lane.md`.

## Exact Acceptance Criteria

### M5: Feature-Surface Parity

M5 is the Phase F feature-parity gate over the frozen AMFlow example classes: `automatic_loop`, `automatic_vs_manual`, `complex_kinematics`, `spacetime_dimension`, `linear_propagator`, `feynman_prescription`, `automatic_phasespace`, and `user_defined_amfmode` / `user_defined_ending` (`docs/full-amflow-completion-roadmap.md:642-654`; `docs/parity-contract.md:60-71`). It depends on `Batch 59` through `Batch 66` plus `Milestone M0b` (`docs/full-amflow-completion-roadmap.md:646-654`).

The Phase F gate requires arbitrary `D0`, fixed-`eps`, complex kinematics, linear propagators, phase-space integration, `feynman_prescription`, singular-kinematics guardrails, and user-defined hooks to be exercised through the live solver path, with explicit coverage status for the frozen example classes (`docs/full-amflow-completion-roadmap.md:784-789`). The parity matrix's phase-2 gate similarly requires eta/invariant derivative closure, solver residuals, and coverage of phase-space, linear, complex, fixed-eps, D0, cache, and custom modes (`specs/parity-matrix.yaml:20-25`).

For M5 to close, every frozen example class must have a reviewed, coefficient-bearing C++ result surface or candidate packet that compares against the retained AMFlow golden without tolerance weakening; every required Phase F runtime feature must be live-path evidence rather than metadata-only scaffolding; partial-progress reference pointers cannot count as passing goldens; and a fail-closed M5 verdict must reject missing examples, failed comparisons, metadata-only features, or open runtime blockers.

### M6: Qualification Corpus

M6 is the Phase G qualification corpus over the parity-matrix benchmarks and upstream regression families, using already captured upstream goldens, and it depends on M5 and M0b (`docs/full-amflow-completion-roadmap.md:656-660`). The Phase G gate requires the qualification corpus to pass and diagnostics/performance to be reviewed on the mandatory benchmark set (`docs/full-amflow-completion-roadmap.md:791-795`).

The executable M6 path is already scaffolded but fail-closed: `qualification_readiness.py`, phase-0 packet comparison, correct-digit scoring, failure-code audit, `qualify_phase0_packet_set.py`, case-study readiness, case-study numeric summary, `qualify_case_study_families.py`, and `qualify_milestone_m6.py` (`docs/verification-strategy.md:327-387`). The final M6 composer requires passing phase-0 packet-set and case-study-family verdicts, plus closure of pending phase-0 runtime-lane blockers (`docs/verification-strategy.md:375-381`).

The mandatory qualification families are package double box, planar `ttbar j`, `ttbar H`, five-point one-mass scattering, `ttbar W`, diphoton heavy-quark form factors, `h -> bb`, `N=4` SYM three-loop form factor, single-top planar/nonplanar, and at least one singular-endpoint case (`specs/parity-matrix.yaml:68-78`; `references/case-studies/selected-benchmarks.md:39-60`). Digit thresholds are `>=50` digits by default, `>=100` for `2024-tth-light-quark-loop-mi`, and `>=200` for `2023-diphoton-heavy-quark-form-factors`; precision monotonicity and explicit unstable-run diagnostics are mandatory (`docs/verification-strategy.md:146-156`; `references/case-studies/selected-benchmarks.md:45-63`).

M6 cannot close until M5 passes, the phase-0 packet-set verdict passes with synchronized comparison/correct-digit/failure-code/regression profiles, case-study numeric sidecars pass for every family at the frozen digit thresholds, the singular endpoint runtime blocker is retired, and `qualify_milestone_m6.py` emits a passing M6-scoped summary without claiming M7 or release readiness.

## Current M5 Criteria And Evidence

### M5 Retained-Regression PASSING Criteria, Not Closure

These rows are PASSING only for the cited retained-regression comparator surfaces. They do not satisfy the separate Phase F requirement that every required runtime feature be exercised through the live solver path, and they must not be read as M5 closure.

- `automatic_loop` retained-state C++/AMFlow parity is passing through the eps8 surface. Lane39 preserves the `automatic_loop.eps2` `54/54`, `automatic_loop.eps3` `66/66`, `automatic_loop.eps4` `78/78`, `automatic_loop.eps5` `90/90`, and `automatic_loop.eps6` `102/102` rows, adds `automatic_loop.eps7` `114/114`, and records `automatic_loop.eps8` as `126/126` at unchanged `tolerance_digits=30` with `minimum_digit_agreement=41`.
- `automatic_phasespace` retained-sample ingest comparison is passing as a narrow regression. Lane32 reports `11/11`, `minimum_digit_agreement=39`, `tolerance_digits=30`; the retained-state C++ summary still does not make this a full live Cutkosky phase-space boundary reconstruction claim.
- `automatic_vs_manual` retained-state C++/AMFlow comparison is passing as a narrow regression. Lane32 reports `89/89`, `minimum_digit_agreement=36`, `tolerance_digits=30`; this replaces the older lane20 failed plain-ProblemSpec probe but still needs M5 acceptance as a live/direct feature-surface path or a packet-shaped retained-state exception.
- `complex_kinematics` retained-sample comparison is passing as a narrow regression through the positive-order eps2 surface. Lane32 reports the eps-through-zero surface at `14/14` and the `complex_kinematics.eps2` surface at `28/28`, both with `minimum_digit_agreement=41` and `tolerance_digits=30`; full complex eta-contour endpoint reconstruction remains deferred.
- `differential_equation_solver` retained finite-start comparisons are now passing for both tracked surfaces. Lane32 reports the default `sol1` surface at `3/3`, `minimum_digit_agreement=41`, and the `sol2` transport surface at `3/3`, `minimum_digit_agreement=30`, both at unchanged `tolerance_digits=30`. This remains retained-state parity evidence; full live finite-start boundary solving still does not close M5 by itself.
- `feynman_prescription` retained `sol1` solution-sample comparison is passing as a narrow regression. Lane32 reports `passed=true`, `matched_integral_count=16`, `compared_coefficient_count=76`, `passed_coefficient_count=76`, `minimum_digit_agreement=39`, `tolerance_digits=30`, and no failures for `tools/reference-harness/specs/phase0/feynman_prescription.amflow-state.json` against the retained scoped `sol1` golden.
- `linear_propagator` retained solution-sample comparison is passing as a narrow regression. Lane32 reports `57/57`, `minimum_digit_agreement=31`, `tolerance_digits=30`; this supersedes the older lane20 `29/57` failing probe while preserving the distinction between retained-sample parity and a full live gauge-link runtime gate.

### M5 PARTIAL Criteria

- `automatic_phasespace` is partial for true M5 because the passing evidence is retained sample ingestion, not full phase-space integration through the live solver path required by Phase F.
- `automatic_vs_manual`, `complex_kinematics`, and `linear_propagator` are partial for true M5 for the same reason: they now have passing retained-state or retained-sample numeric surfaces, but the Phase F closure gate still needs accepted live-path evidence or an explicit fail-closed retained-state exception policy.
- `feynman_prescription` is partial for true M5 because Lane32 proves retained `sol1` solution-sample parity only. The retained golden pointer is now reference-captured with requested-integral backup scope, but this C++ state does not broaden that into full auxiliary-basis equivalence, full live Cutkosky/prescription boundary reconstruction, or opposite-prescription output handling.
- `spacetime_dimension` / arbitrary `D0` is partial/TODO. Optional captured evidence exists, but the Phase F live-path arbitrary-D0 gate remains unclosed.
- User-defined `AMFMode` and `EndingScheme` are partial/TODO. Optional retained captures and planning surfaces exist, but there is no accepted coefficient-bearing C++ parity packet for those execution paths.
- Singular-kinematics guardrails are partial. Multiple guardrail slices exist, but Phase F requires the first physical-region subset through live runtime evidence, and later docs still preserve singular blockers.

### M5 FAILING Criteria

- Lane39 verifies the current retained phase-0 manifest-surface comparison set through `automatic_loop.eps8` without tolerance weakening: `automatic_loop.eps2` through `automatic_loop.eps8`, `automatic_phasespace`, `automatic_vs_manual`, both `complex_kinematics` surfaces, both `differential_equation_solver` surfaces, `feynman_prescription`, and `linear_propagator` all compare successfully at 30 digits.
- M5 remains fail-closed because retained manifest parity is not the same as Phase F closure. The lane39 verifier evidence accepts the retained `automatic_loop.eps8` row (`126/126`, `minimum_digit_agreement=41`) and still blocks overall M5 on the missing frozen-example rows and the required Phase F runtime-feature rows.

### M5 TODO Criteria

- Preserve the Lane39 retained regressions for `automatic_loop.eps2` through `automatic_loop.eps8`, `automatic_phasespace`, `automatic_vs_manual`, `complex_kinematics`, `differential_equation_solver.sol1`, `differential_equation_solver.sol2`, `feynman_prescription`, and `linear_propagator`.
- Build the complete M5 evidence sidecar instead of the lane39 narrow `automatic_loop`-only sidecar, with every required frozen example and runtime feature represented.
- Add live solver-path evidence or accepted retained-state exception criteria for `automatic_phasespace`, `automatic_vs_manual`, `complex_kinematics`, `linear_propagator`, `spacetime_dimension`, user hooks, fixed-`eps`, and singular physical-region coverage.
- Populate and run the M5 fail-closed verdict helper so closure cannot be claimed from loose summaries, partial pointers, metadata-only feature slices, tolerance drift, or open runtime blockers.

M5 overall status: PARTIAL/FAILING, not closeable.

## Current M6 Criteria And Evidence

### M6 Scaffold/Freeze PASSING Criteria, Not Closure

These rows are PASSING only for scaffold presence and frozen benchmark metadata. They do not satisfy the M6 qualification-corpus pass, case-study numeric evidence, performance/diagnostic review, or final composer requirements.

- M6 harness plumbing is substantially present. The verification strategy documents the qualification scaffold, packet correct-digit scorer, packet-set scorer, failure-code audits, phase-0 packet verdict, case-study readiness/numeric summary, case-study verdict, and M6 composer (`docs/verification-strategy.md:327-387`).
- The qualification benchmark families and digit thresholds are frozen in durable docs (`references/case-studies/selected-benchmarks.md:39-63`; `specs/parity-matrix.yaml:68-90`).

### M6 PARTIAL Criteria

- Phase-0 retained goldens and optional captures exist for several examples, but they are not a passing candidate packet-set qualification. The verification strategy explicitly says the scaffold is planning metadata and does not by itself claim new solver parity or qualification closure (`docs/verification-strategy.md:427-442`).
- Case-study readiness metadata exists, but it must be paired with explicit numeric evidence sidecars. The case-study numeric producer reports missing sidecars and threshold misses; it does not launch runtime or claim M6 closure (`docs/verification-strategy.md:382-387`).
- Singular runtime is now partial through SRL-2 local-model analysis: SRL-1 lets the eta-continuation plan explicitly represent a reviewed target-endpoint singular, and SRL-2 can build a non-executing endpoint local-model sidecar for the reviewed integer, non-resonant simple-pole subset while returning typed unsupported results for missing endpoint markers, direct-real-only reuse, regular endpoints, fractional exponents, resonances, and logarithmic cases. This is code progress toward the singular lane, not a branch/prescription ledger, not live complex `eta -> 0` endpoint extraction, not a `full_eta_zero_contour_applied: true` claim, and not numeric qualification evidence.

### M6 FAILING Criteria

- M6 is structurally blocked because M5 is not closed, and M6 depends on M5 (`docs/full-amflow-completion-roadmap.md:660`).
- The case-study-family verdict is blocked in the current retained repo state by the singular runtime-lane blocker and missing case-study numeric evidence (`docs/full-amflow-completion-roadmap.md:663-676`; `docs/verification-strategy.md:366-374`).
- `qualify_milestone_m6.py` must require both phase-0 and case-study verdicts to pass and pending phase-0 runtime blockers to be closed; those prerequisites are not met (`docs/verification-strategy.md:375-381`).

### M6 TODO Criteria

- Close M5 first.
- Publish C++ candidate outputs in the packet schema expected by the M6 tools.
- Run passing phase-0 packet-set comparison, correct-digit scoring, and failure-code audit summaries with synchronized profile labels.
- Produce numeric evidence sidecars for every selected case-study family, including the 100-digit `ttbar-h` and 200-digit diphoton anchors.
- Retire the singular endpoint blocker only after the complex `eta -> 0` endpoint runtime milestones in `docs/theory/singular-runtime-lane.md` produce live runtime and numeric evidence, then compose a passing `qualify_milestone_m6.py` summary.

M6 overall status: TODO/FAILING, not closeable.

## Shortest Atomic Landing Chain

1. Preserve the Lane32 passing retained parity regressions, including `automatic_loop.eps5` `90/90`, both `differential_equation_solver` surfaces, `feynman_prescription` `76/76`, and `linear_propagator` `57/57`.
2. Preserve the Lane39 `automatic_loop.eps6` `102/102`, `automatic_loop.eps7` `114/114`, and `automatic_loop.eps8` `126/126` retained comparisons at unchanged 30-digit tolerance.
3. Keep the retained `feynman_prescription` golden pointer's requested-integral backup scope distinct from full auxiliary-basis equivalence, and do not treat retained solution-sample parity as full live prescription runtime closure.
4. Finish the retained-state/direct runtime acceptance path for `automatic_vs_manual`, `complex_kinematics`, `linear_propagator`, `spacetime_dimension`, `user_defined_amfmode`, and `user_defined_ending`; keep structural outputs separate from solve-series numeric comparison.
5. Turn `automatic_phasespace` retained-sample parity into accepted M5 phase-space support. Keep the `11/11` comparison as a regression, then land the smallest live Cutkosky/prescription runtime slice needed for Phase F.
6. Publish candidate packet roots for the retained surfaces that already pass at 30 digits, including `automatic_loop.eps5`, `automatic_vs_manual`, `complex_kinematics.eps2`, both `differential_equation_solver` surfaces, `feynman_prescription`, and `linear_propagator`.
7. Close fixed-`eps`, arbitrary-D0, and singular physical-region runtime gaps with coefficient-bearing evidence or explicitly reviewed typed-failure acceptance where the contract allows it.
8. Use the M5 fail-closed verdict helper now implemented at `tools/reference-harness/scripts/qualify_milestone_m5.py`; feed it an explicit feature sidecar and per-example comparisons, and keep it rejecting closure on missing examples, partial references, failed comparisons, metadata-only features, tolerance drift, or open runtime blockers.
9. Publish candidate packet roots for successful C++ outputs in the schema expected by the M6 tools.
10. Run and retain passing phase-0 M6 verdict evidence: packet-set comparison, correct-digit score, failure-code audit, and `qualify_phase0_packet_set.py`.
11. Produce case-study numeric evidence sidecars for all qualification families at the frozen thresholds and run `compare_case_study_numeric_results.py` plus `qualify_case_study_families.py`.
12. Implement the singular runtime-lane milestones in `docs/theory/singular-runtime-lane.md`, including complex branch-aware `eta -> 0` endpoint extraction and a passing `one-singular-endpoint-case` numeric sidecar.
13. Retire the singular endpoint case-study blocker and compose the final `qualify_milestone_m6.py` verdict from passing phase-0 and case-study subverdicts.

## Worker Prompt Skeletons

Worker 1, `feynman_prescription` retained parity maintenance: Preserve Lane29's retained `sol1` solution-sample comparison (`76/76`, `minimum_digit_agreement=39`) and keep it in the parity rerun set. Then add the smallest live prescription/phase-space runtime slice needed for Phase F while keeping requested-integral backup acceptance distinct from full auxiliary-basis equivalence. Do not weaken tolerance or treat retained solution samples as full live runtime closure.

Worker 2, `automatic_vs_manual` M5 acceptance: Preserve the Lane24 passing retained-state comparison for the upstream `tt` benchmark (`89/89`, `minimum_digit_agreement=36`) and decide whether M5 accepts that retained-state surface or still requires a direct live `solve_series` path. Keep automatic/manual family scoping explicit, compare against `automatic_vs_manual.golden-manifest.json` at unchanged 30-digit tolerance, and retain tests that fail on fallback/no-DE input rather than silently producing a pass.

Worker 3, complex/D0/DE coverage: Add tracked, reviewed C++ comparison surfaces for `complex_kinematics` and `spacetime_dimension`, and preserve the Lane21 `differential_equation_solver` retained sol2 regression. For DE solver, `redtable`/`diffeq` stay structural golden outputs while `tools/reference-harness/specs/phase0/differential_equation_solver.sol2-golden-manifest.json` is the numeric solve-series comparison surface; Lane21 repaired the missing AMFlow integral by reconstructing the retained solution-basis output from the reviewed Kira relation. Future workers should record exact denominators, pass counts, and minimum digit agreement, and add fail-closed tests for missing state, malformed state, and profile drift.

Worker 4, `linear_propagator` M5 acceptance: Preserve the Lane24 passing retained solution-sample comparison (`57/57`, `minimum_digit_agreement=31`) and convert it into accepted M5 evidence only if the fail-closed criteria allow retained-sample parity for this feature. If live gauge-link runtime evidence is still required, add it without weakening tolerance, and keep the old `gaugex->1` parser regression protected if it still applies.

Worker 5, phase-space and prescription runtime: Keep `automatic_phasespace` `11/11` and Lane29 `feynman_prescription` `76/76` retained-sample comparisons as regressions, then add the smallest live Cutkosky phase-space boundary/runtime slice and opposite-prescription handling needed for Phase F. Unsupported provider or boundary surfaces must fail with typed `boundary_unsolved`, not metadata-only success.

Worker 6, fixed-eps/D0/singular runtime: Close the non-example Phase F runtime gaps with coefficient-bearing evidence. Add one fixed-`eps` comparison, one arbitrary-D0 comparison through `spacetime_dimension`, and one singular physical-region subset that either passes against an accepted golden or has an explicitly reviewed typed-failure contract. Retire the singular endpoint blocker only with runtime evidence, not guardrail metadata alone.

Worker 7, user-defined hooks: Convert `user_defined_amfmode` and `user_defined_ending` from retained capture/planning coverage into numeric execution parity. Exercise the user hook through `AmfOptions`, verify boundary generation and solver use, compare against the retained upstream outputs, and add regressions proving builtin fallback, wrong-hook diagnostics, and selection order stay deterministic.

Worker 8, M5 verdict helper evidence population: Use `tools/reference-harness/scripts/qualify_milestone_m5.py` to produce the M5-only blocked/pass summary. Build the explicit `m5-feature-surface` sidecar with every required per-example comparison summary and feature-status row, require successful promoted goldens and successful C++ comparisons, reject partial references and metadata-only feature rows, surface exact blockers, and state explicitly that this is M5-only evidence, not M6/M7/release readiness.

Worker 9, candidate packet publisher: Publish successful C++ outputs in the packet schema consumed by the existing M6 tools: `candidate_root/results/phase0/<benchmark>/result-manifest.json` plus primary run manifests and any failure-code audit sidecars. Preserve benchmark labels and packet splits matching retained roots, avoid loose JSON-only comparator outputs as M6 inputs, and do not change golden manifests or tolerance policy.

Worker 10, phase-0 M6 qualification: After M5 packets exist, run packet-set comparison, packet-set correct-digit scoring, packet-set failure-code audit, and `qualify_phase0_packet_set.py`. Require synchronized benchmark ids, packet labels, digit-threshold profiles, required-failure-code profiles, and known-regression profile labels. Keep case-study numerics and full M6 closure withheld until the separate case-study verdict passes.

Worker 11, case-study numeric qualification: Produce explicit numeric evidence sidecars for every selected case-study family: package double box, `ttbar-j`, `ttbar-h`, five-point one-mass scattering, `ttbar-w`, diphoton heavy-quark form factors, `h-to-bb`, `n4-sym-three-loop-form-factor`, single-top planar/nonplanar, and one singular-endpoint case. Enforce the default 50-digit floor plus 100 digits for `ttbar-h` and 200 digits for diphoton heavy-quark form factors, then run `compare_case_study_numeric_results.py` and `qualify_case_study_families.py`; missing sidecars remain blockers.

Worker 12, final M6 composer: Run `qualify_milestone_m6.py` only after the phase-0 packet-set verdict and case-study-family verdict both pass. Verify there are no pending runtime lanes, missing sidecars, threshold failures, failure-code misses, regression-profile drift, or singular endpoint blockers. The output may claim M6 only if the composer passes; it must not claim M7 or release readiness.

Worker 13, singular runtime lane: Follow `docs/theory/singular-runtime-lane.md`. Do not retire `one-singular-endpoint-case -> b62p` with metadata-only edits. The accepted runtime path must replace the current complex `eta=0` `unsupported_solver_path` deferral with branch-aware endpoint extraction, publish an endpoint audit, and provide a real `one-singular-endpoint-case` numeric evidence sidecar at the default 50-digit floor.

## Feasibility Assessment

M5 closure is feasible, but not as a documentation-only or single-lane cleanup. Lane39 removes the retained `automatic_loop` eps6 numeric blocker and extends retained automatic-loop parity through eps8, but the M5 verifier still blocks closure unless every frozen example class and every required Phase F runtime feature has accepted evidence. The remaining work is still real implementation and qualification work: accepted live-path or retained-state exception criteria for the feature surfaces, D0/fixed-eps/user-hook coverage, feynman requested-integral backup scope versus full auxiliary-basis equivalence, live opposite-prescription handling, and phase-space/prescription live-path evidence.

M6 closure is not feasible until M5 closes. Even after M5, M6 requires candidate packet publishing, passing phase-0 packet qualification, explicit case-study numeric sidecars at the frozen digit thresholds, and retirement of the singular endpoint blocker. Lane47 determined that the singular endpoint blocker is not safely fixable by a small metadata or guardrail patch: the remaining work is complex branch-aware `eta -> 0` endpoint execution and numeric evidence as designed in `docs/theory/singular-runtime-lane.md`. The harness scaffolding exists, so this is not a blank infrastructure project, but it still needs targeted infrastructure investment in packet-shaped candidate output, failure-code sidecars, complex endpoint runtime work, and case-study numeric evidence production. Current evidence supports planning and partial retained-state progress, not immediate M5/M6 closure.

## Final Consensus

Approve this final design artifact as the truthful closure plan. Reject any claim that M5 or M6 is currently closed.
