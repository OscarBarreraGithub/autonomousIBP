# Re-run M7 Release Readiness

This runbook replays the accepted M7 release-readiness packet from the
repository root. The canonical retained output is
[`release-readiness.m5-accepted.full-output.json`](../../tools/reference-harness/specs/m7/lane3/release-readiness.m5-accepted.full-output.json).

## CI-equivalent check

Use this command when you only need to confirm that the accepted packet still
passes the release gate:

```sh
python3 tools/reference-harness/scripts/assert_m7_release_signoff_ready.py
```

Expected result:

```text
M7 release readiness accepted-input gate passed: release_signoff_ready=true
```

The wrapper reads the accepted readiness sidecar, replays every recorded input
path through `release_signoff_readiness.py`, and fails if the fresh summary does
not report `release_signoff_ready=true` with `release_signoff_blockers=[]`.
CTest runs the same wrapper as `m7-release-signoff-readiness-accepted-inputs`.

## CTest Gate Coverage Audit

Use this command when CTest coverage changes after the 66-test M7 closure
baseline:

```sh
python3 tools/reference-harness/scripts/audit_m7_signoff_ctest_coverage.py --verify
```

The audit compares the current CTest names against
[`m7-signoff-ctest-gate-coverage.json`](../../tools/reference-harness/specs/release/m7-signoff-ctest-gate-coverage.json)
and fails if any post-closure CTest gate is neither listed as required for M7
signoff nor explicitly excluded with a reason. CTest runs the same wrapper as
`m7-signoff-ctest-coverage-audit`; its synthetic fixture coverage runs as
`m7-signoff-ctest-coverage-audit-self-check`.

## Health Summary

Use this command when you need a one-page operator summary of the committed
readiness, blocker, sidecar-inventory, and performance-review state:

```sh
python3 tools/reference-harness/scripts/release_health_summary.py --verify
```

The summary reads the accepted readiness sidecar, reconciles the M7 sidecar
inventory with schema validation, summarizes the accepted performance review
sidecar, and reports the documented AMFlow example reproduction gap counts from
[`amflow-example-coverage.md`](amflow-example-coverage.md) and
[`known-gaps.md`](known-gaps.md). It does not rerun AMFlow numerics, close
runtime lanes, or create new release evidence. CTest runs the same command as
`m7-release-health-summary`.

## Status Badge

Use this command when you need a Shields-compatible endpoint JSON payload for
the committed release state:

```sh
python3 tools/reference-harness/scripts/release_status_badge.py --verify
```

The badge helper reads the same release health summary JSON and renders only the
badge label, message, and color. It does not rerun AMFlow numerics, alter release
evidence, or promote sidecar acceptance. CTest runs `m7-release-status-badge`,
`m7-release-status-badge-self-check`, and `m7-release-status-badge-fixture` to
cover the live health path, synthetic status mapping, and committed fixture
contract.

## Performance Baseline

Use this command when you need to confirm that the accepted-input replay has not
regressed into a materially slower release gate:

```sh
python3 tools/reference-harness/scripts/assert_m7_release_signoff_performance.py
```

The gate replays the accepted readiness inputs three times, reports the measured
wall-clock samples as JSON, and fails if the median replay exceeds 5 seconds or
any single replay exceeds 10 seconds. This is a gross regression guard for
`release_signoff_readiness.py` orchestration overhead only; it is not an AMFlow
runtime numeric benchmark. CTest runs the same wrapper as
`m7-release-signoff-readiness-performance-baseline`.

## Bundle Accepted Evidence

Use this command to create a distributable tarball of the accepted release
evidence sidecars:

```sh
python3 tools/reference-harness/scripts/package_m7_release_evidence.py \
  --output /tmp/m7-release-evidence.tar.gz
```

The archive contains the accepted readiness output, the direct readiness input
sidecars and checklist, the M5 acceptance sidecar, the JSON sidecars named by
that M5 acceptance packet, and a checksum manifest. The manifest also publishes
`evidence_corpus_sha256`, a deterministic SHA-256 over sorted repo-relative
evidence paths and exact file bytes. CTest runs
`m7-release-evidence-bundle-self-check` to build and validate the same archive
shape in a temporary directory, and
`m7-release-evidence-manifest-digest-fixture` to fail if the committed evidence
corpus drifts without an intentional digest refresh.

## Direct replay

Use the direct command when you need to inspect or archive the regenerated JSON
summary. Keep the output path outside the retained specs tree unless you are
intentionally refreshing release evidence.

```sh
OUT=/tmp/release-readiness.json
STDOUT=/tmp/release-readiness.stdout.json
python3 tools/reference-harness/scripts/release_signoff_readiness.py \
  --qualification-summary tools/reference-harness/specs/m7/lane133/qualification-readiness.json \
  --checklist-path tools/reference-harness/templates/release-signoff-checklist.json \
  --qualification-corpus-summary tools/reference-harness/specs/m7/lane133/release-qualification-corpus.json \
  --m5-qualification-summary tools/reference-harness/specs/m5/m5-qualification-lane62.json \
  --m6-qualification-summary tools/reference-harness/specs/m7/lane133/m6-qualification.json \
  --phase0-qualification-summary tools/reference-harness/specs/m7/lane133/phase0-qualification.json \
  --case-study-qualification-summary tools/reference-harness/specs/m7/lane115/case-study-qualification.json \
  --performance-review-summary tools/reference-harness/specs/m7/lane70/release-performance-review.json \
  --diagnostic-review-summary tools/reference-harness/specs/m7/lane76/release-diagnostic-review.json \
  --docs-completion-summary tools/reference-harness/specs/m7/lane92/release-docs-completion.json \
  --parity-signoff-summary tools/reference-harness/specs/m7/lane3/release-parity-signoff.post-a1f0e1d.json \
  --summary-path "$OUT" > "$STDOUT"
python3 -m json.tool "$OUT" | sed -n '/"release_prerequisites"/,$p'
```

The script prints the full JSON summary to stdout and writes the same summary to
`--summary-path`; the command above captures stdout so the terminal only shows
the fields you inspect. A successful accepted replay should include:

```json
{
  "release_signoff_ready": true,
  "release_signoff_blockers": []
}
```

Each `release_prerequisites` entry should have `"satisfied": true`, and each
`review_sections` entry should have `"status": "reviewed"` and an empty
`blockers` list.

## Interpreting the verdict

`release_signoff_ready=true` means the replayed input set satisfies all release
prerequisites and all M7 review sections. For the accepted packet, that means:

- `milestone-m6` is `reviewed-and-accepted-m6-packet`.
- `phase-f-feature-parity` is `reviewed-and-accepted-m5-packet`.
- `retained-reference-evidence` is `captured-and-qualified`.
- `qualification-corpus`, `performance-review`, `diagnostic-review`,
  `docs-completion`, and `parity-signoff` are all reviewed.

`release_signoff_ready=false` is a blocked release-readiness summary, not a
release approval. Treat every value in `release_signoff_blockers` as actionable
evidence debt. The detailed source of each blocker is in `release_prerequisites`
or `review_sections`.

## When Blockers Appear

1. Do not edit the readiness output to force a pass. Fix or refresh the input
   sidecar that produced the blocker.
2. If a blocker starts with `prerequisite:`, inspect `release_prerequisites` for
   the unsatisfied prerequisite and its `current_state`.
3. If a blocker starts with `review:`, inspect `review_sections` for the blocked
   section and its local `blockers`.
4. For path-prefixed blockers such as `qualification-path:`, `parity-path:`,
   `docs-path:`, `performance-path:`, or `diagnostic-path:`, rerun the matching
   M7 producer or review helper and confirm the referenced sidecar path exists
   inside the repository.
5. For `m5:`, `m6:`, `phase0:`, or `case-study:` blockers, refresh the upstream
   qualification sidecar first; release readiness only consumes those verdicts.
6. Re-run the CI-equivalent check after the producing sidecar is updated.

If `release_signoff_readiness.py` exits nonzero, treat that as input schema,
path, or coherence drift rather than a normal blocked release verdict. Fix the
malformed input before interpreting `release_signoff_ready`.
