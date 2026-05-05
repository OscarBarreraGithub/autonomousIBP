APPROVE final design artifact

Milestone closure verdict: REJECT M7 closure at lane86 current `HEAD`.

# M7 Milestone Closure Plan

## Lane86 End-To-End Readiness Snapshot

Lane86 reran `tools/reference-harness/scripts/release_signoff_readiness.py` on
the current candidate tree and captured the full JSON output at
`/tmp/autoibp_orch/exec/lane86_release-readiness.full-output.json`.

Current release readiness is not closed:

- `release_signoff_ready = false`
- `qualification_corpus_review_complete = false`
- `performance_review_complete = true`
- `diagnostic_review_complete = true`
- `docs_completion_review_complete = true`
- `parity_signoff_complete = false`

The sidecar-completion snapshot for the four release sign-offs is therefore
still `2/4 PASSING`: performance and diagnostic sidecars are complete;
qualification corpus and parity sign-off sidecars are blocked. Docs completion
is reviewed, but it is not counted as one of the four release sign-offs in the
lane86 handoff.

The release-readiness review-section gate is more conservative and remains
blocker-preserving: `qualification-corpus` is blocked, `performance-review` is
blocked by `milestone-m6` despite the passing performance sidecar,
`diagnostic-review` is reviewed, `docs-completion` is reviewed, and
`parity-signoff` is blocked.

Lane86 must not document M7 as "1 step from closure" yet. The current
readiness output does not show `3/4` sign-offs passing, and the remaining
blockers are not only the diphoton precision row.

## Passing Evidence

- Performance review is passing. The retained sidecar
  `tools/reference-harness/specs/m7/lane70/release-performance-review.json`
  reports `current_state = "performance-review-reviewed"`,
  `performance_review_complete = true`, and no performance blocker paths.
- Diagnostic review is passing. The retained sidecar
  `tools/reference-harness/specs/m7/lane76/release-diagnostic-review.json`
  reports `current_state = "diagnostic-review-reviewed"`,
  `diagnostic_review_complete = true`, and no diagnostic blocker paths.
- Docs completion is reviewed for the current checklist marker audit. The
  lane86 rerun of `review_release_docs_completion.py` reports
  `docs_completion_review_complete = true` and no docs blockers.
- Case-study numeric sidecar coverage improved since the previous M7 plan:
  the lane86 case-study numeric comparison consumed all 10 selected
  `*.numeric-evidence.json` sidecars, so no selected case-study family is
  missing a numeric sidecar.

## Blocked Evidence

Qualification corpus is blocked by the current generated evidence, not by a
missing sidecar alone. The exact missing or failed evidence is:

- Phase-0 packet-set closure is not accepted by the current retained summary.
  Pending phase-0 examples remain `automatic_phasespace`,
  `complex_kinematics`, `feynman_prescription`, and `linear_propagator`.
- Phase-0 packet-set correct-digit scoring has not fully passed.
- Phase-0 packet-set failure-code audits are not published.
- Required phase-0 typed failure codes are still missing across the packet set:
  `boundary_unsolved`, `continuation_budget_exhausted`,
  `insufficient_precision`, and `master_set_instability`.
- Case-study numeric evidence is present for all selected families, but
  `diphoton-heavy-quark-form-factors` fails comparison and threshold review:
  the current sidecar observes 56 correct digits against the required 200-digit
  profile.
- The case-study verdict remains dependent on an accepted phase-0 verdict.
- The closed benchmark-family coverage statement is not reviewed.

Parity sign-off is blocked by the current parity sidecar state. The exact
missing review evidence is:

- qualification closure note review,
- performance review summary review,
- diagnostic review summary review,
- docs completion note review.

The performance, diagnostic, and docs evidence exists, but the current parity
sidecar has not been refreshed to consume those reviews. Qualification closure
also cannot be truthfully reviewed until the qualification-corpus blockers above
are resolved.

## Clean Closures In Lane86

Lane86 did not close an additional release sign-off. Performance and diagnostic
were already passing before lane86, docs completion remains reviewed, and the
remaining release sign-offs are blocked by real prerequisite evidence:

- Qualification corpus cannot close while phase-0 packet-set blockers and the
  diphoton 56/200 digit failure remain in the generated output.
- Parity sign-off cannot close until qualification closure is reviewed; a
  refresh may clear the performance, diagnostic, and docs note blockers, but it
  cannot claim final parity while qualification corpus is still blocked.

## Shortest Actionable Chain

1. Produce or promote the accepted phase-0 packet-set evidence required by the
   current retained summary: no pending phase-0 examples, passing correct-digit
   scoring, published failure-code audits, and all required typed failure codes.
2. Replace the diphoton proxy evidence with a dedicated
   `diphoton-heavy-quark-form-factors` packet and C++ comparison that reaches
   the required 200-digit profile.
3. Review and record the closed benchmark-family coverage statement.
4. Rerun case-study qualification, M6 qualification, and the qualification
   corpus sidecar until `qualification_corpus_review_complete = true`.
5. Refresh parity sign-off against the accepted qualification, performance,
   diagnostic, and docs-completion reviews. The sidecar must preserve exact
   withheld claims until every prerequisite is reviewed.
6. Rerun `release_signoff_readiness.py` and require the output to be the source
   of truth for any M7 closure or "1 step from closure" statement.

## Final Consensus

Role A, Role B, Role C, and Role D approve this lane86 plan only as a
blocker-preserving M7 release-readiness review. It does not claim Milestone M6
closure, Milestone M7 closure, final parity sign-off, or release readiness.
