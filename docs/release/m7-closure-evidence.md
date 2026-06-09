# M7 Closure Evidence

This document consolidates the retained M7 release-closure audit trail at
`28b04683d3ef09e94390c224a75c7c9b238f84a0`.

The final closure proof is
[`release-readiness.m5-accepted.full-output.json`](../../tools/reference-harness/specs/m7/lane3/release-readiness.m5-accepted.full-output.json):
it reports `release_signoff_ready=true`, `release_signoff_blockers=[]`, every
release prerequisite satisfied, and every release review section reviewed. The
CTest gate added at `28b0468` regenerates readiness from that accepted packet's
input paths and fails if `release_signoff_ready=true` or the empty blocker list
regresses.

## Closure Trail

| Commit | Landing | Retained evidence | Closure role |
| --- | --- | --- | --- |
| `e69a8c8` | Refresh M6 phase-0 qualification summaries | [`m6-qualification.json`](../../tools/reference-harness/specs/m7/lane133/m6-qualification.json), [`phase0-qualification.json`](../../tools/reference-harness/specs/m7/lane133/phase0-qualification.json), [`qualification-readiness.json`](../../tools/reference-harness/specs/m7/lane133/qualification-readiness.json) | Establishes `milestone_m6_ready=true`, `phase0_packet_set_qualified=true`, `case_study_families_qualified=true`, and no M6 blockers. |
| `bd20cbd` | Add M7 qualification-corpus review sidecar | [`release-qualification-corpus.json`](../../tools/reference-harness/specs/m7/lane133/release-qualification-corpus.json) | Reviews the qualification corpus against the M6 evidence, with `qualification_corpus_review_complete=true`, no pending phase-0 ids, no blocked case-study ids, and no missing qualification paths. |
| `4b5ba7a` | Audit M7 parity signoff blockers | [`parity-signoff-scope.json`](../../tools/reference-harness/specs/m7/lane3/parity-signoff-scope.json), [`m7-parity-signoff-gap-report.md`](../milestones/m7-parity-signoff-gap-report.md) | Records the then-current blockers: stale parity sidecar selection, readiness not yet consuming the M6-qualified sidecar, and closure-plan doc drift. This is provenance for the gap, not the final signoff. |
| `a1f0e1d` | Consume M6 qualification in release readiness | [`release_signoff_readiness.py`](../../tools/reference-harness/scripts/release_signoff_readiness.py) | Adds the explicit M6 qualification input to release readiness so M7 can distinguish accepted M6 closure from raw phase-0 and case-study blockers. |
| `2bbae02` | Refresh M7 release signoff readiness sidecars | [`release-parity-signoff.post-a1f0e1d.json`](../../tools/reference-harness/specs/m7/lane3/release-parity-signoff.post-a1f0e1d.json), [`release-readiness.post-a1f0e1d.full-output.json`](../../tools/reference-harness/specs/m7/lane3/release-readiness.post-a1f0e1d.full-output.json) | Publishes a current parity sidecar with `parity_signoff_complete=true` and no parity blockers. The readiness output remains blocked only on the M5 prerequisite: `prerequisite:phase-f-feature-parity:awaiting-reviewed-and-accepted-m5-packet`. |
| `60fd79c` | Accept M5 packet in release readiness | [`m5-packet-acceptance.m5-accepted.json`](../../tools/reference-harness/specs/m7/lane3/m5-packet-acceptance.m5-accepted.json), [`release-readiness.m5-accepted.full-output.json`](../../tools/reference-harness/specs/m7/lane3/release-readiness.m5-accepted.full-output.json) | Accepts the reviewed M5 packet as the phase-F feature-parity prerequisite and produces the final readiness output with `release_signoff_ready=true` and no blockers. |
| `28b0468` | Add M7 release readiness CTest gate | [`assert_m7_release_signoff_ready.py`](../../tools/reference-harness/scripts/assert_m7_release_signoff_ready.py), [`CMakeLists.txt`](../../CMakeLists.txt) | Adds `m7-release-signoff-readiness-accepted-inputs`, which re-runs readiness from the accepted sidecar inputs and requires `release_signoff_ready=true` plus an empty blocker list. |

## Accepted Readiness Inputs

The accepted readiness output consumes these repo-local sidecars and checklist
sources. These links are the canonical audit trail for the final
`release_signoff_ready=true` packet.

| Readiness input | Sidecar or source | Evidence state |
| --- | --- | --- |
| `qualification_summary_path` | [`qualification-readiness.json`](../../tools/reference-harness/specs/m7/lane133/qualification-readiness.json) | Required M6 qualification roots are present and internally coherent. |
| `m5_qualification_summary_path` | [`m5-qualification-lane62.json`](../../tools/reference-harness/specs/m5/m5-qualification-lane62.json) | Reviewed M5 packet used for the phase-F feature-parity prerequisite. |
| M5 acceptance sidecar | [`m5-packet-acceptance.m5-accepted.json`](../../tools/reference-harness/specs/m7/lane3/m5-packet-acceptance.m5-accepted.json) | `accepted_for_release_prerequisite=true`, `m5_packet_reviewed=true`, and no blockers. |
| `m6_qualification_summary_path` | [`m6-qualification.json`](../../tools/reference-harness/specs/m7/lane133/m6-qualification.json) | `milestone_m6_ready=true` and no M6 blocking reasons. |
| `phase0_qualification_summary_path` | [`phase0-qualification.json`](../../tools/reference-harness/specs/m7/lane133/phase0-qualification.json) | Phase-0 packet set qualified for the M6/M7 closure chain. |
| `case_study_qualification_summary_path` | [`case-study-qualification.json`](../../tools/reference-harness/specs/m7/lane115/case-study-qualification.json) | Case-study families qualified for the M6/M7 closure chain. |
| `qualification_corpus_summary_path` | [`release-qualification-corpus.json`](../../tools/reference-harness/specs/m7/lane133/release-qualification-corpus.json) | `qualification_corpus_review_complete=true`, no pending phase-0 ids, and no blocked case-study ids. |
| `performance_review_summary_path` | [`release-performance-review.json`](../../tools/reference-harness/specs/m7/lane70/release-performance-review.json) | `performance_review_complete=true` over the reviewed phase-0 and case-study benchmark families. |
| `diagnostic_review_summary_path` | [`release-diagnostic-review.json`](../../tools/reference-harness/specs/m7/lane76/release-diagnostic-review.json) | `diagnostic_review_complete=true` over the reviewed failure-code profiles. |
| `docs_completion_summary_path` | [`release-docs-completion.json`](../../tools/reference-harness/specs/m7/lane92/release-docs-completion.json) | `docs_completion_review_complete=true` and no stale doc paths. |
| `parity_signoff_summary_path` | [`release-parity-signoff.post-a1f0e1d.json`](../../tools/reference-harness/specs/m7/lane3/release-parity-signoff.post-a1f0e1d.json) | `parity_signoff_complete=true`, qualification closure reviewed, and no parity blockers. |
| `checklist_path` | [`release-signoff-checklist.json`](../../tools/reference-harness/templates/release-signoff-checklist.json) | Release prerequisite and review-section schema consumed by readiness. |
| Final readiness output | [`release-readiness.m5-accepted.full-output.json`](../../tools/reference-harness/specs/m7/lane3/release-readiness.m5-accepted.full-output.json) | `release_signoff_ready=true`, `release_signoff_blockers=[]`, all prerequisites satisfied, and all review sections reviewed. |

## Historical Non-Final Snapshots

These retained artifacts are useful provenance but are not the final closure
proof:

- [`release-readiness.full-output.json`](../../tools/reference-harness/specs/m7/lane92/release-readiness.full-output.json)
  is the older lane92 readiness snapshot and reports `release_signoff_ready=false`.
- [`release-parity-signoff.json`](../../tools/reference-harness/specs/m7/lane92/release-parity-signoff.json)
  is the stale blocked parity sidecar audited by `4b5ba7a`; it is superseded by
  [`release-parity-signoff.post-a1f0e1d.json`](../../tools/reference-harness/specs/m7/lane3/release-parity-signoff.post-a1f0e1d.json).
- [`release-readiness.post-a1f0e1d.full-output.json`](../../tools/reference-harness/specs/m7/lane3/release-readiness.post-a1f0e1d.full-output.json)
  shows the post-M6, post-parity state before M5 acceptance. It intentionally
  remains false because the phase-F feature-parity prerequisite had not yet been
  accepted.
