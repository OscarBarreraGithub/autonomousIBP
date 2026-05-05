# Retroactive Multi-Agent Audit Of Legacy Landings 50-55

Date: 2026-05-05

Scope: this retroactive audit covers the numbered `Batch 50` through `Batch 55` landings on
`main` before `7224f53`. The target commits were identified from
`docs/implementation-ledger.md` and matching commit subjects. The predecessor bridge packets
`Batch 50a` and `Batch 50b` are not part of this numbered `50-55` audit.

Target commits:

| Batch | Commit | Subject |
| --- | --- | --- |
| `Batch 50` | `b40b0dccb1d286b287e2fcb45e5e554901223d63` | `Implement Batch 50 Branch/Loop selector slice` |
| `Batch 51` | `08220d2569d1a60c9181f53d5e809f334dcfcd4e` | `Implement Batch 51 invariant list wrappers` |
| `Batch 52` | `95c2ebf6f7f7adb713c04625d9fccd3c1266eeb8` | `Implement Batch 52 invariant-independent mass widening` |
| `Batch 53` | `0f623d65e7e933d464deef3da4ea02efaf57a535` | `Implement Batch 53 multi-top-sector Kira targets` |
| `Batch 54` | `23b64404680fe0c5425d2261f6e776bd1f197794` | `Implement Batch 54 precision retry controller` |
| `Batch 55` | `4dcb17fef6db1a3e2afbe73ae2ad1c30bd43e293` | `Implement Batch 55 diagnostics hardening` |

## Methodology Signoff

Four independent sub-codex roles audited the exact target commits:

| Role | Focus | Methodology verdict |
| --- | --- | --- |
| Role A | Re-implementation comparison against what the conservative multi-agent flow should have written | `APPROVE` |
| Role B | Test fall-back review: whether added tests would fail on the parent implementation | `APPROVE` |
| Role C | Physics/math parity review against local AMFlow Mathematica snapshots and repo reference docs | `APPROVE` |
| Role D | Anti-fake-parity audit calibrated against `91f49c9` and `9ad4773` | `APPROVE` |

Role D found no `91f49c9` / `9ad4773` anti-pattern in the audited commits: no hardcoded
endpoint zeros, no output suppression, no comparator tolerance loosening, and no lowering of
tests to accept wrong data. Therefore this audit recommends no automatic revert.

## Audit Table

| Batch | Role A re-implementation | Role B test fall-back | Role C physics/reference review | Role D anti-fake review | Retro verdict | Follow-up status |
| --- | --- | --- | --- | --- | --- | --- |
| `Batch 50` | No implementation mismatch found for the narrow single-top-sector squared-linear-momentum Branch/Loop selector slice. | Parent-failing happy-path tests exist for `BranchEtaModeHappyPathTest` and `LoopEtaModeHappyPathTest`; rejection tests are mostly diagnostic. | Real concern: AMFlow drops Branch/Loop groups that intersect `cutvar`, while the original landing intersected mixed groups with uncut candidates before selection. A mixed cut/noncut group could select a propagator AMFlow would exclude. | No fake-parity pattern found. Unsupported cases fail explicitly instead of being silently skipped. | `APPROVED-IN-RETRO` | Follow-up completed on `main` by `Resolve Batch 50 cut-group parity per retro audit`: Branch/Loop now drops groups intersecting cut propagators before representative selection, with mixed cut/noncut regressions for both modes. |
| `Batch 51` | Real mismatch: the original landing implemented one child `DESystem` per invariant instead of one AMFlow-style ordered multi-invariant batch. | Parent-failing API and behavior coverage exists for list construction, caller order, artifact isolation, and stop-on-hard-failure behavior. | Real concern: local AMFlow reference builds a joint ordered variable list, unions targets once, reduces once, and emits one joint system. | No fake-parity pattern found. | `NEEDS-FOLLOWUP` | Follow-up completed on `main` by `1681094` (`Fix Batch 51: align joint invariant list wrappers with AMFlow`). |
| `Batch 52` | No role-A mismatch found for the intended narrow mass-literal widening. | Parent-failing positive coverage exists for invariant-independent identifier and rational masses; negative tests lock invariant-dependent and unsupported mass grammar rejection. | Real concern: allowing invariant-independent masses while factor-matching only `propagator.expression` misses full-denominator terms such as `D0 = k^2 + msq`, where `k^2` should decompose as `D0 - msq`. | No fake-parity pattern found. The literal `"0"` handling is algebraic mass validation, not fabricated output. | `NEEDS-FOLLOWUP` | TODO: queue an invariant-derivative mass-denominator patch that factor-matches full denominators including invariant-independent masses and covers constant remainder terms through seed/composition and wrapper tests. |
| `Batch 53` | No implementation mismatch found for preserving declared top-level sectors and emitting per-sector reduce entries. | Parent-failing coverage exists for per-sector Kira emission, invalid sector masks, and invariant-generated wrapper preservation. | Real concern in the original landing: `r` was active-line-count plus configured dot, not target-aware rank/dot from the actual target integrals, so dotted targets could be under-covered. | No fake-parity pattern found. | `NEEDS-FOLLOWUP` | Follow-up completed on `main` by `53ec6a4` (`Implement Batch 58g mandatory-family reduction-span evidence`) and `e52c8ee` (`Fix Batch 58g: correct reduction-span r for dotted descendants`). |
| `Batch 54` | Real mismatch: the original retry controller allowed another solve when `x_order` advanced even if working precision stalled. | Overall parent-failing coverage exists for precision-budget rejection and production retry wrappers, with some helper-only and unchanged assertions noted. | Real concern: retry progress was keyed to working precision or `x_order`, permitting retries without actual precision progress. | No fake-parity pattern found; hard-cap failures remained explicit failures. | `NEEDS-FOLLOWUP` | Follow-up completed on `main` by `7e8d1d8` (`Fix Batch 54: stop retrying when working precision stalls`). |
| `Batch 55` | Real mismatches: substring-based `master_set_instability` classification over-broadened runtime errors, and hard precision-ceiling rejection was reclassified as `continuation_budget_exhausted`. | Parent-failing coverage exists for wrapper `master_set_instability` classification and the changed ceiling-stop diagnostic; list/builtin/resolved wrapper coverage was weaker. | Real concern: drift classification was string-matched rather than typed at the reducer/master-basis boundary, and hard precision-ceiling failures should preserve `insufficient_precision`. | No fake-parity pattern found. Diagnostics were explicit failure codes, not success conversion or suppression. | `NEEDS-FOLLOWUP` | Follow-up completed on `main` by `fc8b9e8` (`Fix Batch 55: narrow drift diagnostics and preserve ceiling failures`). |

## Result

No audited commit is `REVERT-RECOMMENDED`: the multi-agent audit found no fake-parity pattern
requiring revert. All six original legacy landings originally entered `NEEDS-FOLLOWUP` because at
least one role found a real historical parity or diagnostic issue. Five are now resolved on
current `main` by later fix commits or the Batch 50 retro follow-up. One remains open:

- `Batch 52`: invariant derivative factor matching should use the full denominator surface when
  invariant-independent nonzero masses are accepted.
