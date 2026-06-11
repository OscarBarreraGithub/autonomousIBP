# Release Tooling Catalog

Date: 2026-06-10

This catalog lists the repository-local tools used to replay, inspect, package,
and validate the retained M7 release evidence. It is documentation only: it does
not create new benchmark evidence, change release sidecars, widen runtime
behavior, or promote any unaccepted sidecar.

The accepted closure trail is documented in
[`m7-closure-evidence.md`](m7-closure-evidence.md). The operator runbook for the
primary readiness replay is
[`re-run-release-readiness.md`](re-run-release-readiness.md).
The AMFlow example reproduction boundary is summarized in
[`known-gaps.md`](known-gaps.md) and detailed in
[`amflow-example-coverage.md`](amflow-example-coverage.md).

## Readiness Replay

| Tool | Primary use | CI coverage |
| --- | --- | --- |
| [`release_signoff_readiness.py`](../../tools/reference-harness/scripts/release_signoff_readiness.py) | Regenerate the M7 release-readiness summary from explicit qualification, review, parity, and checklist inputs. | Replayed by `m7-release-signoff-readiness-accepted-inputs`. |
| [`assert_m7_release_signoff_ready.py`](../../tools/reference-harness/scripts/assert_m7_release_signoff_ready.py) | CI wrapper for the accepted readiness input set. It fails unless `release_signoff_ready=true` and `release_signoff_blockers=[]`. | `m7-release-signoff-readiness-accepted-inputs`. |
| [`assert_m7_release_signoff_failures.py`](../../tools/reference-harness/scripts/assert_m7_release_signoff_failures.py) | Negative fixture coverage for blocked readiness states and malformed readiness inputs. | `m7-release-signoff-readiness-failure-modes`. |
| [`assert_m7_release_signoff_performance.py`](../../tools/reference-harness/scripts/assert_m7_release_signoff_performance.py) | Gross regression guard for readiness orchestration overhead only. It is not an AMFlow numeric benchmark. | `m7-release-signoff-readiness-performance-baseline`. |

Use the wrapper for the normal accepted-input replay:

```sh
python3 tools/reference-harness/scripts/assert_m7_release_signoff_ready.py
```

Use `release_signoff_readiness.py` directly only when inspecting or archiving a
fresh JSON summary outside the retained specs tree.

## Sidecar Producers

These helpers produce consumer-compatible release-review sidecars for
`release_signoff_readiness.py`. Their default fail-closed behavior keeps
withheld claims explicit until their required evidence is reviewed.

| Tool | Sidecar schema | Boundary |
| --- | --- | --- |
| [`review_release_qualification_corpus.py`](../../tools/reference-harness/scripts/review_release_qualification_corpus.py) | `release-qualification-corpus` | Consumes M6, phase-0, and case-study qualification summaries; does not run numerics. |
| [`review_release_performance.py`](../../tools/reference-harness/scripts/review_release_performance.py) | `release-performance-review` | Audits retained timing/rebuild evidence; does not create benchmark timings. |
| [`review_release_diagnostic.py`](../../tools/reference-harness/scripts/review_release_diagnostic.py) | `release-diagnostic-review` | Audits retained failure-code and unstable-run evidence; does not run runtime diagnostics. |
| [`review_release_docs_completion.py`](../../tools/reference-harness/scripts/review_release_docs_completion.py) | `release-docs-completion` | Audits release doc targets and non-claim markers; does not alter release evidence. |
| [`review_release_parity_signoff.py`](../../tools/reference-harness/scripts/review_release_parity_signoff.py) | `release-parity-signoff` | Consumes completed release-review sidecars and parity scope evidence; does not widen parity claims. |

The checklist schema consumed by these producers is
[`release-signoff-checklist.json`](../../tools/reference-harness/templates/release-signoff-checklist.json).

## Sidecar Inventory And Provenance

| Tool | Primary use | CI coverage |
| --- | --- | --- |
| [`validate_m7_release_sidecar_schemas.py`](../../tools/reference-harness/scripts/validate_m7_release_sidecar_schemas.py) | Validate committed M7 JSON sidecar schemas, confirm each `source_commit` is a known commit, and recompute `source_provenance_sha256`. | `m7-release-sidecar-schema-validation`. |
| [`audit_m7_sidecar_inventory.py`](../../tools/reference-harness/scripts/audit_m7_sidecar_inventory.py) | List committed M7 sidecars by accepted or unaccepted status. Its optional `--m7-root` is guarded as a repository-relative directory under the checkout. | `m7-release-sidecar-inventory-audit` and `m7-release-sidecar-inventory-root-guard`. |
| [`list_m7_unaccepted_sidecars.py`](../../tools/reference-harness/scripts/list_m7_unaccepted_sidecars.py) | Print only the unaccepted subset from the same inventory classifier for focused review queue triage. Its self-check verifies queue render/count invariants and inherited root guard failures. | `m7-release-unaccepted-sidecar-review-queue` and `m7-release-unaccepted-sidecar-review-queue-self-check`. |
| [`verify_m7_release_readiness_sidecar_references.py`](../../tools/reference-harness/scripts/verify_m7_release_readiness_sidecar_references.py) | Verify the accepted readiness sidecar references existing, accepted M7 JSON sidecars and no stale unaccepted substitutes. | `m7-release-readiness-sidecar-references`. |

Useful inspection commands:

```sh
python3 tools/reference-harness/scripts/validate_m7_release_sidecar_schemas.py
python3 tools/reference-harness/scripts/audit_m7_sidecar_inventory.py --verify --verify-schema-reconciliation --summary-only
python3 tools/reference-harness/scripts/audit_m7_sidecar_inventory.py --self-check
python3 tools/reference-harness/scripts/audit_m7_sidecar_inventory.py --format json
python3 tools/reference-harness/scripts/list_m7_unaccepted_sidecars.py --format text
python3 tools/reference-harness/scripts/list_m7_unaccepted_sidecars.py --format json
python3 tools/reference-harness/scripts/list_m7_unaccepted_sidecars.py --self-check
python3 tools/reference-harness/scripts/verify_m7_release_readiness_sidecar_references.py
```

`audit_m7_sidecar_inventory.py --format json` is the most direct machine-readable
queue for unaccepted sidecars. Treat entries with `"status": "unaccepted"` as
review candidates, not as release evidence. Use the inventory `--self-check` to
exercise the root guard against absolute paths, parent traversal, and
non-directory roots; use the unaccepted-queue `--self-check` to exercise queue
shape and count invariants.
Use `list_m7_unaccepted_sidecars.py` when you want that queue without the
accepted sidecar rows.

## M6 Prerequisite Sidecar Guards

These checks validate the committed M6 sidecar corpus that the accepted release
readiness packet consumes through qualification summaries. They read committed
artifacts only; they do not regenerate AMFlow evidence, edit sidecars, or alter
release acceptance.

| Tool | Primary use | CI coverage |
| --- | --- | --- |
| [`validate_m6_sidecar_shapes.py`](../../tools/reference-harness/scripts/validate_m6_sidecar_shapes.py) | Validate committed M6 JSON and SQLite sidecar schemas, repo-local path references, and accepted/blocked shape consistency. | `m6-sidecar-shape-validation` and `m6-sidecar-shape-validation-self-check`. |
| [`audit_m6_sidecar_drift.py`](../../tools/reference-harness/scripts/audit_m6_sidecar_drift.py) | Summarize accepted and unaccepted M6 sidecars, report stale metadata groups, and pin known unaccepted sidecars against silent promotion. | `m6-sidecar-drift-audit`, `m6-sidecar-drift-audit-self-check`, and `m6-unaccepted-sidecar-promotion-guard`. |

Useful inspection commands:

```sh
python3 tools/reference-harness/scripts/validate_m6_sidecar_shapes.py --verify
python3 tools/reference-harness/scripts/audit_m6_sidecar_drift.py --verify --summary-only
python3 tools/reference-harness/scripts/audit_m6_sidecar_drift.py --verify --verify-unaccepted-not-promoted --summary-only
```

## Evidence Bundle And Health

| Tool | Primary use | CI coverage |
| --- | --- | --- |
| [`package_m7_release_evidence.py`](../../tools/reference-harness/scripts/package_m7_release_evidence.py) | Create a deterministic tarball containing the accepted readiness sidecar, direct inputs, M5 acceptance sidecar, referenced M5 sidecars, and checksum manifest. | `m7-release-evidence-bundle-self-check`. |
| [`assert_m7_release_evidence_manifest_digest.py`](../../tools/reference-harness/scripts/assert_m7_release_evidence_manifest_digest.py) | Fixture guard for the committed evidence corpus digest. | `m7-release-evidence-manifest-digest-fixture`. |
| [`release_health_summary.py`](../../tools/reference-harness/scripts/release_health_summary.py) | Print a compact readiness, inventory, and performance-review summary from committed sidecars. | `m7-release-health-summary`, `m7-release-health-source-sidecar-self-check`, and `m7-release-health-summary-json`. |
| [`assert_m7_release_health_json_fixture.py`](../../tools/reference-harness/scripts/assert_m7_release_health_json_fixture.py) | Fixture gate for the machine-readable health JSON contract, including a synthetic drift self-check. | `m7-release-health-summary-json-fixture` and `m7-release-health-summary-json-fixture-self-check`. |
| [`assert_m7_release_health_text_fixture.py`](../../tools/reference-harness/scripts/assert_m7_release_health_text_fixture.py) | Fixture gate for the operator-facing text health contract. | `m7-release-health-summary-text-fixture`. |
| [`release_status_badge.py`](../../tools/reference-harness/scripts/release_status_badge.py) | Render a Shields-compatible JSON status badge from the release health summary. | `m7-release-status-badge` and badge fixture tests. |
| [`assert_m7_release_health_outputs_consistent.py`](../../tools/reference-harness/scripts/assert_m7_release_health_outputs_consistent.py) | Verify text, JSON, and badge health outputs remain mutually consistent. | `m7-release-health-output-consistency`. |

Operator commands:

```sh
python3 tools/reference-harness/scripts/package_m7_release_evidence.py \
  --output /tmp/m7-release-evidence.tar.gz
python3 tools/reference-harness/scripts/release_health_summary.py --verify
python3 tools/reference-harness/scripts/release_health_summary.py --verify --format json
python3 tools/reference-harness/scripts/assert_m7_release_health_json_fixture.py
python3 tools/reference-harness/scripts/assert_m7_release_health_json_fixture.py --self-check
python3 tools/reference-harness/scripts/assert_m7_release_health_text_fixture.py
python3 tools/reference-harness/scripts/release_status_badge.py --verify
```

The evidence bundle is a packaging step only. It must not be treated as new
runtime, parity, or qualification evidence.

## Release Markdown

| Tool | Primary use | CI coverage |
| --- | --- | --- |
| [`validate_release_markdown.py`](../../tools/reference-harness/scripts/validate_release_markdown.py) | Validate release markdown links and fenced code blocks under `docs/release/` plus [`docs/release-signoff-checklist.md`](../release-signoff-checklist.md). | `m7-release-markdown-docs-validation` and `m7-release-markdown-docs-self-check`. |

Run it directly after editing release docs:

```sh
python3 tools/reference-harness/scripts/validate_release_markdown.py
python3 tools/reference-harness/scripts/validate_release_markdown.py --self-check
```

The validator checks local links, markdown fragments, and parseability of `sh`,
`bash`, and `json` fenced blocks. Its self-check exercises valid release-doc
fixtures plus missing-anchor, repository-escape, bad-shell-fence, and
unsupported-language failures. It does not decide release readiness.
