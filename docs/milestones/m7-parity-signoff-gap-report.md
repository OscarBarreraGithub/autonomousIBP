# M7 Parity Signoff Gap Report

Status: audit only. This report does not flip final parity signoff, does not
claim Milestone M7 closure, and does not modify b61n or b63n source files.

## Lane3 Follow-up: Readiness Validator M6 Input

Status: validator gap closed. `release_signoff_readiness.py` now accepts an
explicit `--m6-qualification-summary` input and validates the
`milestone-m6-qualification` sidecar before using it to clear stale derived
`case-study-numerics` carry-forward blockers.

The helper self-check now includes an accepted-M6 scenario where complete M7
review sidecars plus a complete parity sidecar must produce a reviewed
`parity-signoff` section with no `milestone-m6` blocker. A retained-input probe
using lane133 M6, lane133 qualification-corpus, lane70 performance, lane76
diagnostic, lane92 docs, and a fresh parity-signoff sidecar produces
`parity-signoff: reviewed` while keeping `release_signoff_ready=false`.

Remaining separate work: refresh or supersede the stale retained lane92
`release-parity-signoff.json`, retain a current readiness output, and refresh
the M7 closure plan without turning the readiness output into a release claim.

## Current Finding

The parity helper itself is not blocked by b61n or b63n numerics on current
`origin/main` (`8aa73a33e9a8fa92272eb015e4b72e2bc3f6225f`). A fresh
`review_release_parity_signoff.py` run using the current retained prerequisites
produces:

```text
current_state=parity-signoff-reviewed
parity_signoff_complete=true
qualification_closure_reviewed=true
qualification_corpus_reviewed=true
b61n_single_row_publication_hook_reviewed=true
missing_or_blocked_parity_paths=[]
blocking_reasons=[]
```

Inputs used for that probe:

- `tools/reference-harness/specs/m7/lane133/m6-qualification.json`
- `tools/reference-harness/specs/m7/lane133/release-qualification-corpus.json`
- `tools/reference-harness/specs/m7/lane70/release-performance-review.json`
- `tools/reference-harness/specs/m7/lane76/release-diagnostic-review.json`
- `tools/reference-harness/specs/m7/lane92/release-docs-completion.json`

## Exact Blockers Today

The first blocker is a stale retained parity sidecar. The committed
`tools/reference-harness/specs/m7/lane92/release-parity-signoff.json` still
reports `current_state=blocked-on-qualification-closure`,
`parity_signoff_complete=false`, `qualification_closure_reviewed=false`, and
`qualification_corpus_reviewed=false`. When the current readiness helper is run
with that sidecar, the parity review section remains blocked by:

```text
parity-signoff-incomplete
parity-qualification-closure
parity-qualification-corpus-summary
parity-path:qualification-closure-note
milestone-m6
```

The second blocker is in the release-readiness validator. Even when the fresh
parity probe above is provided to `release_signoff_readiness.py`, the parity
review section is still blocked by `milestone-m6`. The helper derives the
release prerequisite from qualification, phase0, and case-study fields; it does
not consume `tools/reference-harness/specs/m7/lane133/m6-qualification.json`.
Current `lane133/phase0-qualification.json` is
`phase0-packet-set-qualified`, but it still preserves
`milestone_m6_requires_case_study_numerics=true`. The readiness helper carries
that into a `case-study-numerics` blocker even though
`lane115/case-study-qualification.json` is `case-study-families-qualified` and
`lane133/m6-qualification.json` is `milestone-m6-qualified`.

The relevant helper behavior is:

- `release_signoff_readiness.py` computes `milestone-m6` from phase0 and
  case-study inputs around the release-prerequisite construction.
- The parity section then appends `milestone-m6` when that derived prerequisite
  is still `blocked-on-qualification-closure`.
- There is no `--m6-qualification-summary` input on
  `release_signoff_readiness.py`, so the accepted M6 sidecar cannot clear that
  derived blocker today.

## Missing Artifacts

- A retained current `release-parity-signoff.json` generated from the current
  M6, qualification-corpus, performance, diagnostic, and docs sidecars.
- A retained current release-readiness full output showing parity signoff
  reviewed. This cannot be produced honestly until the readiness helper can
  observe or reconcile accepted M6 closure.

## Missing Tests

- A `release_signoff_readiness.py --self-check` case for the current
  all-reviewed prereq set: lane133 M6, lane133 qualification corpus, lane70
  performance, lane76 diagnostic, lane92 docs, and a complete parity sidecar.
  Expected result: no `milestone-m6` blocker on the parity-signoff section.
- A guard that prevents a stale retained parity sidecar from being silently
  reused after newer M6 and qualification-corpus closure sidecars land.

## Plan Drift

`docs/milestones/m7-closure-plan.md` still describes the older lane92 state in
which qualification corpus and phase0 were blocked by b61n, b63n, and b64ag
runtime lanes. Current retained evidence has moved past that state:

```text
lane133/phase0-qualification.json: phase0-packet-set-qualified
lane115/case-study-qualification.json: case-study-families-qualified
lane133/release-qualification-corpus.json: qualification-corpus-reviewed
lane133/m6-qualification.json: milestone-m6-qualified
```

That plan should be refreshed after the readiness validator gap is fixed, with
the distinction preserved: the parity sidecar can be generated as reviewed now,
but the release-readiness summary still blocks it on a stale derived M6
prerequisite.

## Honest Next Step

The next implementation lane should update `release_signoff_readiness.py` to
consume an accepted M6 qualification summary or otherwise reconcile the
qualified phase0 and case-study subverdicts without retaining
`case-study-numerics` as a blocker. Then add the self-check above and promote a
fresh retained parity sidecar.
