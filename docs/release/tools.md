# Release Tooling Catalog

Date: 2026-06-12

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
| [`audit_m7_signoff_ctest_coverage.py`](../../tools/reference-harness/scripts/audit_m7_signoff_ctest_coverage.py) | Audit CTest gates added after the 66-test M7 closure baseline against the release signoff gate manifest, requiring each one to be signoff-required or explicitly excluded with a reason. | `m7-signoff-ctest-coverage-audit` and `m7-signoff-ctest-coverage-audit-self-check`. |

Use the wrapper for the normal accepted-input replay:

```sh
python3 tools/reference-harness/scripts/assert_m7_release_signoff_ready.py
```

Use `release_signoff_readiness.py` directly only when inspecting or archiving a
fresh JSON summary outside the retained specs tree.

Use `audit_m7_signoff_ctest_coverage.py --verify` before changing CTest coverage
so new gates cannot bypass signoff classification.

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
| [`verify_m7_release_readiness_sidecar_references.py`](../../tools/reference-harness/scripts/verify_m7_release_readiness_sidecar_references.py) | Verify the accepted readiness sidecar references existing, accepted M7 JSON sidecars and no stale unaccepted substitutes. Its optional `--m7-root` uses the same repository-relative root guard as the inventory audit. | `m7-release-readiness-sidecar-references`. |
| [`verify_m5_m6_m7_cross_links.py`](../../tools/reference-harness/scripts/verify_m5_m6_m7_cross_links.py) | Verify accepted M5 packets, accepted M6 summaries, accepted M7 readiness/signoff sidecars, and the release bundle manifest remain mutually linked. Its self-check rejects missing M5 packets, unaccepted M6 summaries, unaccepted M7 signoffs, and bundle omissions. | `m5-m6-m7-cross-link-closure`. |

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
python3 tools/reference-harness/scripts/verify_m5_m6_m7_cross_links.py --self-check
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
| [`verify_m6_readiness_sidecar_closure.py`](../../tools/reference-harness/scripts/verify_m6_readiness_sidecar_closure.py) | Validate the accepted M6 readiness packet against its phase-0, case-study, b64ag runtime, comparison, and release-readiness inputs, including b64ag per-coefficient witness pointers and fail-closed nullable-field checks. | `b64ag-phase0-packet-field-drift-gate` and the readiness-closure mode of `m6-sidecar-drift-audit`. |

Useful inspection commands:

```sh
python3 tools/reference-harness/scripts/validate_m6_sidecar_shapes.py --verify
python3 tools/reference-harness/scripts/audit_m6_sidecar_drift.py --verify --summary-only
python3 tools/reference-harness/scripts/audit_m6_sidecar_drift.py --verify --verify-unaccepted-not-promoted --summary-only
python3 tools/reference-harness/scripts/verify_m6_readiness_sidecar_closure.py --self-check
```

## Post-M7 Runtime Parity And Publication Guards

These checks summarize and guard the retained post-M7 b61n, b63n, and b64ag
runtime evidence surfaces. They are read-only release support tools: they do not
rerun AMFlow, publish withheld coefficients, close blocked runtime lanes, or
change the accepted M7 readiness packet.

| Tool | Primary use | CI coverage |
| --- | --- | --- |
| [`audit_b61n_publication_qualifier.py`](../../tools/reference-harness/scripts/audit_b61n_publication_qualifier.py) | Audit the b61n publication qualifier sidecar for deterministic replay, required diagnostic fields, precision evidence binding, inventory consistency, and fail-closed field drift. | `b61n-publication-qualifier-determinism`, `b61n-publication-precision-evidence-binding`, `b61n-publication-precision-inventory-consistency`, `b61n-publication-diagnostic-fields`, `b61n-publication-diagnostic-field-removal-self-check`, `b61n-publication-diagnostic-field-addition-self-check`, and `b61n-publication-qualifier-failure-modes-self-check`. |
| [`diagnose_b61n_row56_specific_targets.py`](../../tools/reference-harness/scripts/diagnose_b61n_row56_specific_targets.py) | Read-only diagnostic for the four b61n row 5/6 comparator targets, classifying sample-space, publication-gate, transport-precision, and AMFlow reference-precision limits. | `b61n-row56-specific-target-diagnostic-self-check`. |
| [`verify_b61n_publication_audit_field.py`](../../tools/reference-harness/scripts/verify_b61n_publication_audit_field.py) | Query and assert one addressed field from the generated b61n publication audit JSON, with path-parser rejection checks. | `b61n-publication-audit-field-query-self-check`. |
| [`verify_b61n_reference_floor_parity.py`](../../tools/reference-harness/scripts/verify_b61n_reference_floor_parity.py) | Verify the retained b61n row 5/6 C++ comparison against the documented AMFlow reference floor and target-count expectations. | `b61n-reference-floor-parity-gate` and `b61n-reference-floor-parity-gate-self-check`. |
| [`verify_b61n_precision_uplift_monotonicity.py`](../../tools/reference-harness/scripts/verify_b61n_precision_uplift_monotonicity.py) | Verify the b61n row 5/6 precision-uplift sidecar is monotonic and remains bound to its committed C++ result and diagnostic sources. | `b61n-precision-uplift-monotonicity` and `b61n-precision-uplift-monotonicity-self-check`. |
| [`verify_b61n_exact_rational_extension_stability.py`](../../tools/reference-harness/scripts/verify_b61n_exact_rational_extension_stability.py) | Verify the b61n exact-rational 160-digit extension fixture against the committed source precision evidence. | `b61n-exact-rational-extension-stability` and `b61n-exact-rational-extension-stability-self-check`. |
| [`verify_b61n_publication_audit_trail.py`](../../tools/reference-harness/scripts/verify_b61n_publication_audit_trail.py) | Query the C++ b61n publication audit emitter and validate pinned audit labels, keys, fingerprints, and fail-closed mutations. | `b61n-publication-audit-trail-query-self-check`. |
| [`regenerate_b61n_publication_audit_fingerprints.py`](../../tools/reference-harness/scripts/regenerate_b61n_publication_audit_fingerprints.py) | Emit fresh b61n publication audit fingerprints from `singular-runtime-lane-tests` and fail unless they match the pinned values. | `b61n-publication-audit-fingerprint-regenerator-self-check`. |
| [`b61n_parity_status_summary.py`](../../tools/reference-harness/scripts/b61n_parity_status_summary.py) | Summarize post-M7 b61n parity status from committed reference-floor, precision, publication, and optional runtime-audit evidence. | `b61n-parity-status-summary` and `b61n-parity-status-summary-self-check`. |
| [`assert_b61n_parity_status_json_fixture.py`](../../tools/reference-harness/scripts/assert_b61n_parity_status_json_fixture.py) | Fixture gate for the pinned b61n parity status JSON contract and synthetic drift checks. | `b61n-parity-status-summary-json-fixture` and `b61n-parity-status-summary-json-fixture-self-check`. |
| [`verify_boundary_sidecar_provenance.py`](../../tools/reference-harness/scripts/verify_boundary_sidecar_provenance.py) | Verify b61n, b63n, and b64ag boundary sidecar paths, SHA pins, source provenance digests, selected endpoint witnesses, and semantic evidence gates against the committed boundary surface. | `boundary-sidecar-provenance-audit` and `boundary-sidecar-provenance-audit-self-check`. |
| [`audit_b64ag_golden_recapture_readiness.py`](../../tools/reference-harness/scripts/audit_b64ag_golden_recapture_readiness.py) | Fail-closed audit for b64ag golden-recapture readiness, covering runtime packet scope, AMFlow state contract, target coverage, and comparison-level 50-digit evidence completeness. The accepted evidence-sidecar schema and per-coefficient witnesses are guarded by `verify_m6_readiness_sidecar_closure.py`. | `b64ag-golden-recapture-readiness-self-check`. |
| [`diagnose_b64ag_first_block_gap.py`](../../tools/reference-harness/scripts/diagnose_b64ag_first_block_gap.py) | Classify the b64ag first-block 50-digit comparison gap by target path and retained AMFlow precision limitation. | `b64ag-first-block-gap-diagnostic-self-check`. |
| [`verify_b63n_d246_evidence.py`](../../tools/reference-harness/scripts/verify_b63n_d246_evidence.py) | Validate the b63n D2/D4/D6 weighted-residue sidecar schema, source hashes, blocker semantics, and optional published-evidence requirements. | `b63n-d246-evidence-verifier` and `b63n-d246-evidence-verifier-self-check`. |
| [`verify_b63n_selected4_permutation_audit.py`](../../tools/reference-harness/scripts/verify_b63n_selected4_permutation_audit.py) | Cross-check the b63n permutation-invariant audit against selected4 parity evidence without publishing D2/D4/D6 coefficients. | `b63n-selected4-permutation-audit-verifier` and `b63n-selected4-permutation-audit-verifier-self-check`. |
| [`verify_b63n_scoped_gate_audit_trail.py`](../../tools/reference-harness/scripts/verify_b63n_scoped_gate_audit_trail.py) | Query the b63n scoped gate audit emitter and validate pinned blocked/published weighted-residue labels plus fail-closed mutations. | `b63n-scoped-gate-audit-trail-query-self-check`. |
| [`regenerate_b63n_weighted_residue_fingerprints.py`](../../tools/reference-harness/scripts/regenerate_b63n_weighted_residue_fingerprints.py) | Emit fresh b63n weighted-residue audit fingerprints from `cutkosky-weighted-residue-tests` and fail unless they match the pinned values. | `b63n-weighted-residue-fingerprint-regenerator-self-check`. |
| [`b63n_parity_status_summary.py`](../../tools/reference-harness/scripts/b63n_parity_status_summary.py) | Summarize post-M7 b63n parity status from committed first, selected4, D246, and optional runtime-audit evidence. | `b63n-parity-status-summary` and `b63n-parity-status-summary-self-check`. |
| [`assert_b63n_parity_status_json_fixture.py`](../../tools/reference-harness/scripts/assert_b63n_parity_status_json_fixture.py) | Fixture gate for the pinned b63n parity status JSON contract and synthetic drift checks. | `b63n-parity-status-summary-json-fixture` and `b63n-parity-status-summary-json-fixture-self-check`. |

Useful inspection commands:

```sh
python3 tools/reference-harness/scripts/b61n_parity_status_summary.py --format json
python3 tools/reference-harness/scripts/b63n_parity_status_summary.py --format json
python3 tools/reference-harness/scripts/diagnose_b61n_row56_specific_targets.py --self-check
python3 tools/reference-harness/scripts/diagnose_b64ag_first_block_gap.py --self-check
python3 tools/reference-harness/scripts/verify_b63n_d246_evidence.py
```

## Evidence Bundle And Health

| Tool | Primary use | CI coverage |
| --- | --- | --- |
| [`package_m7_release_evidence.py`](../../tools/reference-harness/scripts/package_m7_release_evidence.py) | Create a deterministic tarball containing the accepted readiness sidecar, direct inputs, M5 acceptance sidecar, referenced M5 sidecars, the seven pinned AMFlow live-rerun notes, and checksum manifest. | `m7-release-evidence-bundle-self-check`. |
| [`assert_m7_release_evidence_manifest_digest.py`](../../tools/reference-harness/scripts/assert_m7_release_evidence_manifest_digest.py) | Fixture guard for the committed evidence corpus digest. | `m7-release-evidence-manifest-digest-fixture`. |
| [`verify_m5_golden_fingerprint_pins.py`](../../tools/reference-harness/scripts/verify_m5_golden_fingerprint_pins.py) | Verify the committed `tools/reference-harness/specs/m5/lane*/goldens/` byte fingerprints against the current M5 feature-evidence comparison surface. | `m5-golden-fingerprint-pins` and `m5-golden-fingerprint-pins-self-check`. |
| [`verify_m5_packet_provenance.py`](../../tools/reference-harness/scripts/verify_m5_packet_provenance.py) | Verify source commits, generator script digests, input hashes, output hashes, timestamps, and signers for the accepted M5 qualification packet surface. | `m5-packet-provenance-audit` and `m5-packet-provenance-audit-self-check`. |
| [`release_health_summary.py`](../../tools/reference-harness/scripts/release_health_summary.py) | Print a compact readiness, inventory, performance-review, and documented AMFlow example coverage-gap summary from committed files. | `m7-release-health-summary`, `m7-release-health-source-sidecar-self-check`, and `m7-release-health-summary-json`. |
| [`assert_m7_release_health_json_schema.py`](../../tools/reference-harness/scripts/assert_m7_release_health_json_schema.py) | Schema gate for the machine-readable health JSON contract, including required keys, exact JSON types, enum values, ranges, and count/list consistency. | `m7-release-health-summary-json-schema` and `m7-release-health-summary-json-schema-self-check`. |
| [`assert_m7_release_health_json_fixture.py`](../../tools/reference-harness/scripts/assert_m7_release_health_json_fixture.py) | Fixture gate for the machine-readable health JSON contract, including a synthetic drift self-check. | `m7-release-health-summary-json-fixture` and `m7-release-health-summary-json-fixture-self-check`. |
| [`assert_m7_release_health_text_fixture.py`](../../tools/reference-harness/scripts/assert_m7_release_health_text_fixture.py) | Fixture gate for the operator-facing text health contract. | `m7-release-health-summary-text-fixture`. |
| [`release_status_badge.py`](../../tools/reference-harness/scripts/release_status_badge.py) | Render a Shields-compatible JSON status badge from the release health summary. | `m7-release-status-badge` and `m7-release-status-badge-self-check`. |
| [`assert_m7_release_health_outputs_consistent.py`](../../tools/reference-harness/scripts/assert_m7_release_health_outputs_consistent.py) | Verify text, JSON, and badge health outputs remain mutually consistent, including the AMFlow example coverage counters and not-full runtime example list. | `m7-release-health-output-consistency`. |

Operator commands:

```sh
python3 tools/reference-harness/scripts/package_m7_release_evidence.py \
  --output /tmp/m7-release-evidence.tar.gz
python3 tools/reference-harness/scripts/verify_m5_golden_fingerprint_pins.py --verify
python3 tools/reference-harness/scripts/verify_m5_golden_fingerprint_pins.py --self-check
python3 tools/reference-harness/scripts/verify_m5_packet_provenance.py --verify
python3 tools/reference-harness/scripts/verify_m5_packet_provenance.py --self-check
python3 tools/reference-harness/scripts/release_health_summary.py --verify
python3 tools/reference-harness/scripts/release_health_summary.py --verify --format json
python3 tools/reference-harness/scripts/assert_m7_release_health_json_schema.py
python3 tools/reference-harness/scripts/assert_m7_release_health_json_schema.py --self-check
python3 tools/reference-harness/scripts/assert_m7_release_health_json_fixture.py
python3 tools/reference-harness/scripts/assert_m7_release_health_json_fixture.py --self-check
python3 tools/reference-harness/scripts/assert_m7_release_health_text_fixture.py
python3 tools/reference-harness/scripts/release_status_badge.py --verify
python3 tools/reference-harness/scripts/assert_m7_release_health_outputs_consistent.py
```

The evidence bundle is a packaging step only. It validates and carries the
seven pinned live-rerun notes so the signoff archive cannot silently drop that
release surface, but it must not be treated as new runtime, parity, or
qualification evidence.

### Deterministic Artifact Check

Lane6 v8 checked build-artifact reproducibility at
`263f2f67eab1241cb58202a150d571e126892c10` on 2026-06-12 from a detached
worktree with two independent Release/Ninja build directories, optional
external dependencies disabled, and GCC 8.5.0:

```sh
cmake -S . -B /tmp/autoibp-lane6-v8-build-a -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DAMFLOW_WITH_GINAC=OFF -DAMFLOW_WITH_MPFR=OFF -DAMFLOW_WITH_YAML_CPP=OFF
cmake --build /tmp/autoibp-lane6-v8-build-a --parallel 4
ctest --test-dir /tmp/autoibp-lane6-v8-build-a --output-on-failure --parallel 4

cmake -S . -B /tmp/autoibp-lane6-v8-build-b -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DAMFLOW_WITH_GINAC=OFF -DAMFLOW_WITH_MPFR=OFF -DAMFLOW_WITH_YAML_CPP=OFF
cmake --build /tmp/autoibp-lane6-v8-build-b --parallel 4
ctest --test-dir /tmp/autoibp-lane6-v8-build-b --output-on-failure --parallel 4
```

Both full test passes reported `91/91` passing tests. The selected stable
artifacts matched byte-for-byte:

| Artifact | Normalization | SHA-256 |
| --- | --- | --- |
| `m7-release-evidence.tar.gz` from `package_m7_release_evidence.py` | None; the packager already fixes gzip filename/mtime plus tar member order, uid/gid, uname/gname, mode, and mtime. | `af0079e9c9da48ddcc7c543f9fc364508109fa811d4c9e508dc0d292f9489687` |
| `amflow-tests` | None; raw Release executable from the two build directories compared equal. | `4d1debd4224517ef015a07b4a3eccbd2c6280c3303fbbca973da64920101e42f` |
| `amflow-tests` | `strip --strip-all` copy, as a conservative stable-binary check. | `2d02ba56eefb8cf53a84f1f5b3727f925f6a9142695538fe057e19932f6808ab` |

The only observed variable field was the `bundle` path in the packager stdout
JSON, which reflects the caller-selected output location. After normalizing that
path, the package summaries matched with
`evidence_corpus_sha256=c1478382a02fbe1c4b6f4a7baea9932e454ade7cb5f7ae5d6aedc794d50f3ab5`
and `file_count=22`.

This is a narrow determinism guarantee for the listed artifacts under the same
source checkout, compiler, build type, CMake options, and source path. It is not
a claim that CMake's generated build trees, arbitrary debug builds, alternate
compiler/linker versions, or optional external dependency builds are hermetic.

The health summary's AMFlow example coverage block is an inventory guard. It
keeps the detailed coverage doc and release known-gaps table aligned in both
content and not-full-row order, verifies the frozen ten-example AMFlow inventory,
and rejects status/gap mismatches, but it does not close the listed runtime
lanes or change the accepted M7 readiness scope.

## Release Markdown

| Tool | Primary use | CI coverage |
| --- | --- | --- |
| [`verify_amflow_upstream_version_pin.py`](../../tools/reference-harness/scripts/verify_amflow_upstream_version_pin.py) | Load the pinned upstream AMFlow package through `wolframscript` and fail if live package `$PackageInfo` diverges from [`amflow-upstream-version-pin.registry.json`](../../tools/reference-harness/specs/release/amflow-upstream-version-pin.registry.json). | `amflow-upstream-version-pin` and `amflow-upstream-version-pin-self-check`. |
| [`verify_public_symbol_surface.py`](../../tools/reference-harness/scripts/verify_public_symbol_surface.py) | Extract defined external symbols from the built `amflow` library with `nm`, filter to top-level `amflow` C++ symbols, and fail if the result differs from the pinned ABI manifest. | `amflow-public-symbol-surface` and `amflow-public-symbol-surface-self-check`. |
| [`validate_amflow_upstream_example_inventory.py`](../../tools/reference-harness/scripts/validate_amflow_upstream_example_inventory.py) | Validate that the committed stock upstream AMFlow example inventory and hashes in [`amflow-upstream-example-inventory.registry.json`](../../tools/reference-harness/specs/release/amflow-upstream-example-inventory.registry.json) still match the audited cluster Git checkout and same-path cross-check refs. | `amflow-upstream-example-inventory` and `amflow-upstream-example-inventory-self-check`. |
| [`validate_release_markdown.py`](../../tools/reference-harness/scripts/validate_release_markdown.py) | Validate release markdown links and fenced code blocks under `docs/release/` plus [`docs/release-signoff-checklist.md`](../release-signoff-checklist.md). | `m7-release-markdown-docs-validation` and `m7-release-markdown-docs-self-check`. |
| [`validate_release_tools_catalog.py`](../../tools/reference-harness/scripts/validate_release_tools_catalog.py) | Validate this tooling catalog against CTest-wired mode-capable release scripts and reject stale tool links or missing CTest coverage names. | `m7-release-tools-catalog-validation` and `m7-release-tools-catalog-self-check`. |
| [`validate_amflow_live_rerun_docs.py`](../../tools/reference-harness/scripts/validate_amflow_live_rerun_docs.py) | Validate that the AMFlow coverage table, the seven live retained-golden rerun notes, and the blocked `automatic_phasespace` row remain synchronized. | `m7-amflow-live-rerun-doc-inventory` and `m7-amflow-live-rerun-doc-inventory-self-check`. |
| [`validate_amflow_live_rerun_doc_structure.py`](../../tools/reference-harness/scripts/validate_amflow_live_rerun_doc_structure.py) | Validate structured evidence blocks, digest references, exit lines, and external retained-evidence markers in the seven AMFlow live-rerun notes. | `m7-amflow-live-rerun-doc-structure` and `m7-amflow-live-rerun-doc-structure-self-check`. |
| [`validate_amflow_live_rerun_freshness.py`](../../tools/reference-harness/scripts/validate_amflow_live_rerun_freshness.py) | Validate that every AMFlow live-rerun note either carries `last-re-verified` metadata or is explicitly listed as `pending-first-re-verify` in [`amflow-live-rerun-freshness-registry.json`](amflow-live-rerun-freshness-registry.json). | `m7-amflow-live-rerun-freshness` and `m7-amflow-live-rerun-freshness-self-check`. |
| [`validate_automatic_phasespace_blocker_docs.py`](../../tools/reference-harness/scripts/validate_automatic_phasespace_blocker_docs.py) | Validate that the `automatic_phasespace` known-gap row, blocker record, absent live-rerun note, and D246 sidecar stay fail-closed and synchronized. | `m7-automatic-phasespace-blocker-docs` and `m7-automatic-phasespace-blocker-docs-self-check`. |

Run it directly after editing release docs:

```sh
python3 tools/reference-harness/scripts/validate_release_markdown.py
python3 tools/reference-harness/scripts/validate_release_markdown.py --self-check
python3 tools/reference-harness/scripts/validate_release_tools_catalog.py --verify
python3 tools/reference-harness/scripts/validate_release_tools_catalog.py --self-check
python3 tools/reference-harness/scripts/validate_amflow_live_rerun_docs.py --verify
python3 tools/reference-harness/scripts/validate_amflow_live_rerun_docs.py --self-check
python3 tools/reference-harness/scripts/validate_amflow_live_rerun_doc_structure.py --verify
python3 tools/reference-harness/scripts/validate_amflow_live_rerun_freshness.py --verify
python3 tools/reference-harness/scripts/validate_amflow_live_rerun_freshness.py --self-check
python3 tools/reference-harness/scripts/validate_automatic_phasespace_blocker_docs.py --verify
```

The validator checks local links, markdown fragments, and parseability of `sh`,
`bash`, and `json` fenced blocks. Its self-check exercises valid release-doc
fixtures plus missing-anchor, repository-escape, bad-shell-fence, and
unsupported-language failures. It does not decide release readiness.

The live-rerun inventory validator is narrower: it pins the current seven live
retained-golden AMFlow rerun docs, requires each pinned row in
[`amflow-example-coverage.md`](amflow-example-coverage.md) to cite its matching
rerun note, and rejects an `automatic_phasespace` live-rerun promotion while the
D246 blocker remains open. It does not run Mathematica, create AMFlow outputs,
or widen the accepted runtime scope.

The live-rerun freshness validator is narrower still: it checks only the seven
committed live-rerun notes and
[`amflow-live-rerun-freshness-registry.json`](amflow-live-rerun-freshness-registry.json).
Every live-rerun note must either carry a valid `last-re-verified` line or be
listed with `pending-first-re-verify` until lane re-verification records the
first metadata line. It does not add metadata, run Mathematica, or change
retained-golden claims.

The tools catalog validator reads CTest declarations and this file only. It
guards catalog completeness and link freshness for release-script tests with
`--self-check` or `--verify` modes, but it does not run the underlying evidence
tools or change their release claims.
