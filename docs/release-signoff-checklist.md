# Milestone M7 Release Sign-Off Checklist

This checklist is the first `Milestone M7` groundwork scaffold. It freezes the
evidence buckets and review questions that a future release packet must fill in
only after `Milestone M6` is actually passing on the reviewed qualification
corpus.

This document is planning metadata only. Adding it does not claim that the
qualification corpus passes, that benchmark performance is acceptable, that the
public docs are complete, or that the project is release-ready.

## Release Packet Header

| Field | Required release-packet value |
| --- | --- |
| `release_candidate_commit` | exact commit SHA being signed off |
| `release_baseline` | exact upstream base or rebased release branch head |
| `signoff_date_utc` | one ISO-like UTC timestamp for the decision packet |
| `qualification_summary_artifact` | exact `Milestone M6` summary path proving the reviewed corpus passed |
| `mandatory_benchmark_artifacts` | pinned artifact roots for the diagnostics/performance benchmark set |
| `reviewers` | named owners for diagnostics, performance, docs/parity, and final release disposition |
| `open_blockers_or_waivers` | explicit list of unresolved blockers or approved waivers; `none` only when empty |

## Prerequisites

- [ ] `Milestone M6` is recorded as passing on the reviewed qualification corpus
      and the exact summary artifact is attached above.
- [ ] The candidate commit has one fresh rebuild gate recorded on the exact tree
      being signed off.
- [ ] The retained reference roots used for parity or qualification comparisons
      are pinned and immutable for this packet.
- [ ] `docs/implementation-ledger.md`, `docs/public-contract.md`,
      `docs/verification-strategy.md`, and
      `docs/full-amflow-completion-roadmap.md` all describe the same candidate
      state.

## Review Buckets

| Area | Required questions before approval | Evidence that must be attached |
| --- | --- | --- |
| Diagnostics review | Do the mandatory benchmark families emit the expected success/failure classifications, and are all reviewed failure-code surfaces still visible and correctly typed? | exact benchmark list, exact commands, summary artifact paths, and any typed diagnostic deviations with explicit disposition |
| Performance review | Is runtime on the mandatory benchmark set acceptable on the pinned environment, and are any regressions versus the reviewed baseline explained or waived explicitly? | pinned platform/configuration, benchmark timings, comparison baseline, and written disposition for every material slowdown |
| Docs completion review | Do the public docs describe the current release state truthfully, with no stale milestone, blocker, or capability claims? | exact doc paths reviewed, any required follow-up edits, and final sign-off that the docs match the candidate commit |
| Parity sign-off review | Do the accepted retained captures, qualification corpus results, and known-regression notes justify the release claim without overreaching beyond the reviewed subset? | qualification summary, retained-root references, parity-matrix scope, known-regression profile, and any explicit non-goal or waiver that remains live |

## Mandatory Release Notes

- [ ] State the exact supported subset being released.
- [ ] State the exact unsupported or deferred surfaces that remain out of scope.
- [ ] State the exact benchmark families reviewed for diagnostics and
      performance.
- [ ] State the exact retained artifact roots or summaries that back the parity
      claim.
- [ ] State every blocker or waiver that was discussed during sign-off, even if
      the final disposition is still pending.

## Sign-Off Record

| Role | Decision | Notes |
| --- | --- | --- |
| Diagnostics reviewer | `pending` | |
| Performance reviewer | `pending` | |
| Docs/parity reviewer | `pending` | |
| Release owner | `pending` | |

Final release disposition: `pending`
