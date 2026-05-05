APPROVE final design artifact

Milestone closure verdict: REJECT M7 closure at lane92 current `HEAD`.

# M7 Milestone Closure Plan

## Lane92 End-To-End Readiness Snapshot

Lane92 reran `tools/reference-harness/scripts/release_signoff_readiness.py`
against fresh lane92 prerequisite summaries on the current candidate tree. The
committed full JSON output is
`tools/reference-harness/specs/m7/lane92/release-readiness.full-output.json`;
the lane stdout capture is also retained at
`/tmp/autoibp_orch/exec/lane92_release-readiness.full-output.json`.

Current release readiness remains blocked:

- `release_signoff_ready = false`
- `qualification_corpus_review_complete = false`
- `performance_review_complete = true`
- `diagnostic_review_complete = true`
- `docs_completion_review_complete = true`
- `parity_signoff_complete = false`

The release review-section inventory is now `3/5 reviewed` and `2/5 blocked`:

- `qualification-corpus`: `blocked`
- `performance-review`: `reviewed`
- `diagnostic-review`: `reviewed`
- `docs-completion`: `reviewed`
- `parity-signoff`: `blocked`

Lane89's cycle fix is effective in the lane92 recheck: `performance-review`
now has status `reviewed`, `performance_review_current_state =
"performance-review-reviewed"`, and an empty blocker list. It is no longer
blocked by `milestone-m6` when its own retained performance evidence is
reviewed. This does not claim M6, M7, final parity sign-off, or release
readiness.

Lane92 must still not document M7 as closed or one step from closure. Two
review sections remain blocked, and all release prerequisites remain
unsatisfied:

- `milestone-m6`: `blocked-on-qualification-closure`
- `phase-f-feature-parity`: `blocked-on-runtime-lanes` (`b61n`, `b63n`,
  `b64ag`)
- `retained-reference-evidence`: `captured-but-phase0-not-qualified`

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
  lane92 docs sidecar reports `docs_completion_review_complete = true` and no
  docs blockers.
- Case-study numeric sidecar coverage is complete but not fully passing:
  lane92 consumed all 10 selected `*.numeric-evidence.json` sidecars, with
  9/10 families meeting their digit threshold.

## Blocked Evidence

Qualification corpus is blocked by current generated evidence, not by a
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
  it has 11/222 coefficients passing and 56 observed correct digits against
  the required 200-digit profile, so the case-study set remains 9/10.
- The case-study verdict remains dependent on an accepted phase-0 verdict.
- The closed benchmark-family coverage statement is not reviewed.

Parity sign-off is blocked by qualification closure. With the retained lane83
parity sidecar, performance, diagnostic, and docs reviews are already consumed;
the live lane92 blockers are:

- `parity-signoff-incomplete`
- `parity-qualification-closure`
- `parity-path:qualification-closure-note`
- `qualification-corpus`
- `milestone-m6`

## Clean Closures In Lane92

Lane92 confirms one behavior change from the lane89 cycle fix: performance
review is PASSING in the release-readiness section rather than blocked by
`milestone-m6`. Diagnostic review and docs completion are also reviewed.

Lane92 does not close a new release prerequisite or final sign-off:

- Qualification corpus cannot close while phase-0 packet-set blockers, the
  diphoton 56/200 digit failure, the phase-0 dependency, and the unreviewed
  closed coverage statement remain in the generated output.
- Milestone M6 cannot close until the phase-0 packet-set verdict and
  case-study-family verdict are both accepted.
- Parity sign-off cannot close until qualification closure is reviewed and the
  live readiness helper no longer adds `qualification-corpus` and
  `milestone-m6` as blockers.

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
5. Refresh final parity sign-off after accepted qualification closure. The
   sidecar must preserve exact withheld claims until every prerequisite is
   reviewed.
6. Rerun `release_signoff_readiness.py` and require the output to be the source
   of truth for any M7 closure or release-readiness statement.

## Final Consensus

Role A, Role B, Role C, and Role D approve this lane92 plan only as a
blocker-preserving M7 release-readiness recheck. It confirms the
performance-review cycle fix and rejects Milestone M6 closure, Milestone M7
closure, final parity sign-off, and release readiness.
