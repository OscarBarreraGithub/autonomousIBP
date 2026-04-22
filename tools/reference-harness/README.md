# Reference Harness Bootstrap

This directory is the phase-0 bootstrap for standing up the upstream AMFlow validation environment. It can now materialize a real harness layout, record pinned inputs, optionally clone or refresh the public AMFlow checkout after verifying its remote, optionally download and extract the CPC archive into a clean directory, and freeze placeholder golden metadata without requiring Mathematica.

Once the bootstrap exists, `capture_phase0_reference.py` promotes the required phase-0 benchmark
set from placeholders to retained real outputs: it stages an isolated AMFlow work tree, patches the
Kira/Fermat install hook to the pinned toolchain, runs each selected example twice, retains the raw
Mathematica outputs as goldens, canonicalizes rule-list ordering for truthful comparisons, and
emits machine-readable backup-match plus rerun-reproducibility summaries.

The shipped phase-0 catalog now names all frozen upstream examples, including
`feynman_prescription`; only `automatic_vs_manual` and `automatic_loop` are required for the
accepted `reference-captured` gate today.

## Quick Start

```bash
python3 tools/reference-harness/scripts/bootstrap_reference_harness.py \
  --root /tmp/amflow-reference-bootstrap \
  --manifest-name phase0-reference \
  --freeze-placeholders
```

Add `--dry-run` to inspect the plan without writing files.

## Optional Upstream Fetch

```bash
python3 tools/reference-harness/scripts/bootstrap_reference_harness.py \
  --root /tmp/amflow-reference-bootstrap \
  --manifest-name phase0-reference \
  --amflow-url https://gitlab.com/amflow/amflow.git \
  --amflow-ref main \
  --cpc-url https://example.invalid/amflow-cpc.zip \
  --kira-executable /opt/kira/bin/kira \
  --fermat-executable /opt/fermat/fer64 \
  --wolfram-kernel /opt/Wolfram/WolframKernel \
  --freeze-placeholders
```

If you already have local inputs, record them instead of fetching:

```bash
python3 tools/reference-harness/scripts/bootstrap_reference_harness.py \
  --root /tmp/amflow-reference-bootstrap \
  --manifest-name phase0-reference \
  --amflow-path /opt/amflow \
  --cpc-archive-path /data/amflow-cpc-archive \
  --freeze-placeholders
```

The focused fetch helper is also available directly:

```bash
python3 tools/reference-harness/scripts/fetch_upstream_amflow.py \
  --root /tmp/amflow-reference-bootstrap \
  --amflow-url https://gitlab.com/amflow/amflow.git \
  --cpc-url https://example.invalid/amflow-cpc.zip
```

All thirteen harness helpers also expose a local `--self-check` mode for the regression cases fixed in
Batch 2 and the new M5/M6 catalog/scaffold coherence lock, including the theory-backed
`next_runtime_lane` blocker hints for the still-deferred `b61n` / `b62n` / `b63k` / `b64k`
surfaces and the `optional_capture_packet` grouping for the retained `de-d0-pair` and retained
`user-hook-pair`, plus the blocked M7 release-readiness audit:

```bash
python3 tools/reference-harness/scripts/bootstrap_reference_harness.py \
  --root /tmp/amflow-reference-bootstrap \
  --self-check

python3 tools/reference-harness/scripts/fetch_upstream_amflow.py \
  --root /tmp/amflow-reference-bootstrap \
  --self-check

python3 tools/reference-harness/scripts/freeze_phase0_goldens.py \
  --root /tmp/amflow-reference-bootstrap \
  --self-check

python3 tools/reference-harness/scripts/capture_phase0_reference.py \
  --root /tmp/amflow-reference-bootstrap \
  --self-check

python3 tools/reference-harness/scripts/validate_qualification_scaffold.py \
  --root /tmp/amflow-reference-bootstrap \
  --self-check

python3 tools/reference-harness/scripts/qualification_readiness.py \
  --root /tmp/amflow-reference-bootstrap \
  --self-check

python3 tools/reference-harness/scripts/qualification_case_study_readiness.py \
  --self-check

python3 tools/reference-harness/scripts/compare_phase0_results_to_reference.py \
  --reference-root /tmp/amflow-reference-bootstrap \
  --self-check

python3 tools/reference-harness/scripts/score_phase0_correct_digits.py \
  --self-check

python3 tools/reference-harness/scripts/compare_phase0_packet_set_to_reference.py \
  --self-check

python3 tools/reference-harness/scripts/score_phase0_packet_set_correct_digits.py \
  --self-check

python3 tools/reference-harness/scripts/audit_phase0_failure_codes.py \
  --self-check

python3 tools/reference-harness/scripts/release_signoff_readiness.py \
  --self-check
```

`amflow-tests` now exercises all thirteen helper self-checks through the configured
`Python3_EXECUTABLE`, so the repo-local gate covers bootstrap, fetch, placeholder-freeze,
retained-capture, scaffold-validation, qualification-readiness, case-study-family readiness,
blocked release-readiness, the single-packet comparator, packet-level correct-digit scorer,
packet-level failure-code audit, plus the packet-set retained-reference comparison and packet-set
correct-digit scorer regression paths without needing a real benchmark packet.

If `inputs/upstream/amflow` already exists, the fetch helper verifies that `origin` matches `--amflow-url` and fetches the requested ref before it records the pinned commit. If the CPC archive is re-extracted, the helper recreates `inputs/extracted/cpc` first so stale files cannot survive reruns.
Tar extraction is policy-driven inside the helper itself: it rejects symlink, hardlink, device, absolute-path, and escaping entries before any tar payload is written, rather than relying on interpreter defaults.

To freeze or refresh the placeholder golden layout separately:

```bash
python3 tools/reference-harness/scripts/freeze_phase0_goldens.py \
  --root /tmp/amflow-reference-bootstrap \
  --manifest-path /tmp/amflow-reference-bootstrap/manifests/phase0-reference.json
```

Benchmark IDs in the phase-0 catalog must be path-safe ASCII tokens such as `automatic_vs_manual`. By default the freezer refreshes only missing or placeholder-status files and leaves promoted real artifacts alone. Use `--force` only if you intentionally want to overwrite existing non-placeholder files.

To promote the required phase-0 benchmark set into retained real goldens:

```bash
python3 tools/reference-harness/scripts/capture_phase0_reference.py \
  --root /tmp/amflow-reference-bootstrap \
  --required-only
```

Add `--resume-existing` to reuse any already-retained `primary` or `rerun` run manifests after a
walltime kill instead of replaying completed labels.

To resume one narrow optional-capture packet in the same frozen order as the catalog:

```bash
python3 tools/reference-harness/scripts/capture_phase0_reference.py \
  --root /tmp/amflow-reference-bootstrap \
  --optional-capture-packet de-d0-pair \
  --resume-existing
```

Repeated `--benchmark-id` flags still collapse duplicates and run in the catalog's frozen order
rather than CLI order. `--optional-capture-packet` selects every matching catalog entry in that
same frozen order, and at most one explicit selection mode may be used at a time. Packet
selection is therefore the preferred path when the catalog already groups a ready optional pair
such as the retained `de-d0-pair` or retained `user-hook-pair`.
If the required phase-0 pair is absent, the packet summary truthfully stays `bootstrap-only` even
when the selected optional examples become `reference-captured`.

To audit the current qualification scaffold against the retained packet split without running any
qualification numerics:

```bash
python3 tools/reference-harness/scripts/validate_qualification_scaffold.py \
  --root /n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260419-required-set \
  --root /n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260422-de-d0-pair \
  --root /n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260422-user-hook-pair
```

The validator keeps the accepted `required-set` packet distinct from the narrower `de-d0-pair`
and `user-hook-pair` packets whose manifests truthfully remain `bootstrap-only`, while still
crediting their benchmark-level retained goldens in the readiness report.

To aggregate the accepted required retained root plus any narrower optional packets into the first
machine-readable M6 readiness summary:

```bash
python3 tools/reference-harness/scripts/qualification_readiness.py \
  --root /n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260419-required-set \
  --optional-packet-root /n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260422-de-d0-pair \
  --optional-packet-root /n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260422-user-hook-pair
```

Add `--summary-path` if you want the JSON summary written to disk as well as printed to stdout.
The helper is evidence-only: it validates the retained phase-0 packet set against
`templates/qualification-benchmarks.json`, keeps blocked `next_runtime_lane` hints visible, and
normalizes older optional packets that predate the explicit `optional_capture_packet` summary field
by inferring that packet id from the scaffold when the retained packet contents make the mapping
unambiguous.

To summarize the frozen M6 case-study-family anchors without running any case-study numerics:

```bash
python3 tools/reference-harness/scripts/qualification_case_study_readiness.py
```

Add `--summary-path` if you want the JSON summary written to disk as well as printed to stdout.
The helper validates the case-study-family portion of `templates/qualification-benchmarks.json`
against `references/case-studies/selected-benchmarks.md`, `specs/parity-matrix.yaml`,
`docs/verification-strategy.md`, and the implementation ledger, and keeps the reviewed singular
`next_runtime_lane` blocker plus its landed predecessor anchor visible. This is still
evidence/planning only: it does not compare retained case-study numerics and does not claim that
`Milestone M6` is passing.

To compare one candidate phase-0 packet root against one retained reference packet root on the
first actual M6 comparator path:

```bash
python3 tools/reference-harness/scripts/compare_phase0_results_to_reference.py \
  --reference-root /n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260419-required-set \
  --candidate-root /path/to/candidate-packet-root
```

The comparator reuses the existing `results/phase0/<benchmark>/result-manifest.json` and primary
`run-manifest.json` schema, requires the exact retained output-name set and canonical hashes on the
selected benchmarks, and surfaces the frozen digit-threshold, failure-code, and regression
profiles from `templates/qualification-benchmarks.json`. It is still comparator plumbing only: it
does not launch the C++ runtime, does not compute correct-digit scores, and does not by itself
claim that `Milestone M6` is passing.

To score one candidate phase-0 packet root against one retained reference packet root on the first
actual packet-level correct-digit path:

```bash
python3 tools/reference-harness/scripts/score_phase0_correct_digits.py \
  --reference-root /n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260422-de-d0-pair \
  --candidate-root /path/to/candidate-packet-root \
  --benchmark-id differential_equation_solver
```

The scorer reuses the same `result-manifest.json` and primary `run-manifest.json` schema, requires
the retained output-name set and nonnumeric canonical-text skeleton to stay fixed, scores only
approximate Mathematica numeric literals tokenwise against the frozen digit-threshold profiles,
and leaves exact symbolic outputs structural-only on this reviewed path. It still does not audit
candidate failure-code behavior, aggregate scores across the full packet split, or claim that
`Milestone M6` is passing.

To compare the full retained phase-0 packet split against candidate packet roots in one aggregated
report:

```bash
python3 tools/reference-harness/scripts/compare_phase0_packet_set_to_reference.py \
  --packet-root-pair /n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260419-required-set::/path/to/candidate-required-set \
  --packet-root-pair /n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260422-de-d0-pair::/path/to/candidate-de-d0-pair \
  --packet-root-pair /n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260422-user-hook-pair::/path/to/candidate-user-hook-pair
```

This packet-set comparator composes the existing single-packet comparator across the retained
`required-set`, `de-d0-pair`, and `user-hook-pair` split, requires one unique reference packet
label per pair, requires each candidate packet root to publish exactly the retained benchmark split
for that packet through `result-manifest.json` entries, and ignores uncaptured placeholder
directories that do not publish a manifest. It still fails closed unless the compared benchmark ids
match the scaffold's full current `reference-captured` phase-0 set exactly. It is still
harness-only comparator plumbing: it does not launch the C++ runtime, does not compute
correct-digit scores, does not inspect candidate failure-code behavior, and does not by itself
claim that `Milestone M6` is passing.

To aggregate correct-digit scoring across the full retained phase-0 packet split in one report:

```bash
python3 tools/reference-harness/scripts/score_phase0_packet_set_correct_digits.py \
  --packet-root-pair /n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260419-required-set::/path/to/candidate-required-set \
  --packet-root-pair /n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260422-de-d0-pair::/path/to/candidate-de-d0-pair \
  --packet-root-pair /n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260422-user-hook-pair::/path/to/candidate-user-hook-pair
```

This packet-set correct-digit aggregator composes the reviewed packet-level scorer across the
retained `required-set`, `de-d0-pair`, and `user-hook-pair` split, requires one unique reference
packet label per pair, requires each candidate packet root to publish exactly the retained
benchmark split for that packet, and fails closed unless the scored benchmark ids match the
scaffold's current `reference-captured` phase-0 set exactly. It is still harness-only score
aggregation plumbing: it does not launch the C++ runtime, does not inspect candidate failure-code
behavior, does not compare case-study numerics, and does not by itself claim that `Milestone M6`
is passing.

To audit whether one candidate phase-0 packet root publishes the required failure-code coverage:

```bash
python3 tools/reference-harness/scripts/audit_phase0_failure_codes.py \
  --candidate-root /path/to/candidate-packet-root
```

This helper consumes one full candidate packet root that publishes the existing
`result-manifest.json` and primary `run-manifest.json` schema plus one optional
`results/phase0/<benchmark>/failure-code-audit.json` sidecar per audited benchmark. Each sidecar
lists the benchmark's observed typed failure codes; the helper then surfaces the frozen required
failure-code profile from `templates/qualification-benchmarks.json` alongside the candidate's
missing and unexpected codes. It is still harness-only qualification plumbing: it does not launch
the C++ runtime, does not compare canonical outputs or correct digits, and does not by itself
claim that `Milestone M6` is passing.

To turn one retained M6 readiness summary into the first blocked M7 release-readiness report:

```bash
python3 tools/reference-harness/scripts/release_signoff_readiness.py \
  --qualification-summary /tmp/qualification-readiness.json
```

Add `--summary-path` if you want the JSON report written to disk as well as printed to stdout.
This helper is still release-prep plumbing only: it audits the release-signoff checklist sources
and docs-completion targets, keeps the current blocked `b61n` / `b62n` / `b63k` / `b64k`
frontier visible from the retained M6 evidence packet, and writes one blocked release-readiness
summary without claiming that `Milestone M6` or `Milestone M7` is closed.

The capture script writes:

- `results/phase0/<benchmark>/primary/` and `rerun/`: the retained raw Mathematica outputs for the
  two pinned executions, plus canonicalized sidecars and per-run manifests
- `goldens/phase0/<benchmark>/captured/`: the promoted primary-run golden outputs
- `comparisons/phase0/<benchmark>.summary.json`: bundled-`kira_*` backup agreement plus rerun
  reproducibility checks after canonicalization
- `state/phase0-reference.capture.json`: packet-wide capture summary, which also advances
  `phase0.capture_state` in the main manifest to `reference-captured` once every required
  benchmark passes

## Output Layout

- `manifests/`: pinned environment manifests
- `inputs/`: optional cloned/downloaded upstream AMFlow and CPC inputs
- `logs/`: Wolfram and Kira logs
- `generated-config/`: emitted reducer/job config
- `results/`: captured benchmark outputs, run manifests, and canonical sidecars
- `comparisons/`: parity and reproducibility reports vs retained goldens
- `goldens/`: phase-0 golden metadata, promoted output manifests, and index files
- `templates/`: copied manifest, environment, Wolfram, phase-0 benchmark, qualification, and release-signoff templates
- `state/`: bootstrap/fetch summaries for automation and audit trails

## Required External Inputs

- a licensed Wolfram kernel
- a pinned public AMFlow checkout
- the AMFlow CPC archive
- Kira 3.1
- Fermat

## Notes

- The phase-0 bootstrap does not require Mathematica to run. It freezes the expected benchmark, output, and comparison paths so later parity tooling can bind to stable locations before real reference data is captured.
- Real goldens are produced only after the pinned upstream AMFlow environment is run under a licensed Wolfram kernel.
- The capture script compares Mathematica outputs after canonicalizing rule-list ordering, because
  upstream examples can rewrite the same rule set in a different textual order across runs even
  when the mathematical result is unchanged.
- The templates here are designed to match the repo-level docs in `docs/reference-harness.md` and the manifest shape in `specs/reference-harness-manifest.yaml`.
- `templates/qualification-benchmarks.json` is the first machine-readable M6 scaffold: it mirrors
  the parity-matrix benchmark families, the current digit-threshold profiles, the required failure
  codes, and the known regression families without claiming any new captured evidence. It now also
  carries optional `next_runtime_lane` hints for feature or qualification anchors that are still
  blocked on reviewed runtime slices, plus `optional_capture_packet` hints for ready example pairs
  that belong in the retained `de-d0-pair` or retained `user-hook-pair` packets. Those hints
  stay aligned with the current theory frontier while still anchoring against the recorded
  predecessor slices `b61j` / `b62m` / `b63j` / `b64j`.
- `templates/release-signoff-checklist.json` is the first machine-readable M7 scaffold: it freezes
  the later release-review sections for qualification closure, performance review, diagnostic
  review, docs completion, and the final parity sign-off statement. It is planning metadata only
  and does not claim `Milestone M6` closure, `Milestone M7` closure, or release readiness.
- `validate_qualification_scaffold.py` is the first narrow M6 evidence-audit helper: it validates
  retained packet manifests, comparison summaries, and promoted goldens against the scaffold and
  reports which phase-0 example classes are already covered by the current `required-set`,
  `de-d0-pair`, and `user-hook-pair` packet split without claiming any new parity or `Milestone M6`
  closure.
- `qualification_readiness.py` is the first M6 groundwork summary helper: it aggregates the
  accepted required retained root plus any narrower optional packet roots into one
  machine-readable readiness summary, validates that the observed retained artifacts still match
  the scaffold, and keeps blocked `next_runtime_lane` hints visible without running any
  qualification numerics.
- `qualification_case_study_readiness.py` is the first M6 case-study-family summary helper: it
  validates the selected literature anchors, parity labels, digit floors, failure/regression
  profiles, and the reviewed singular blocker hint against the frozen sources and emits one
  machine-readable family-readiness summary without comparing case-study numerics.
- `release_signoff_readiness.py` is the first executable M7 helper: it consumes one
  machine-readable `qualification_readiness.py` summary plus the release-signoff checklist,
  audits that the checklist source/doc paths exist inside the repo, preserves the blocked
  `next_runtime_lane` frontier, and writes one blocked release-readiness summary without
  overclaiming qualified release evidence.
- `compare_phase0_results_to_reference.py` is the first actual M6 packet comparator: it compares
  one candidate packet root against one retained reference packet root through exact canonical
  output-name/hash agreement on the selected phase-0 benchmarks while surfacing the frozen
  threshold/failure/regression metadata from the qualification scaffold.
- `score_phase0_correct_digits.py` is the first actual M6 packet-level correct-digit scorer: it
  consumes one retained reference packet root plus one candidate packet root, keeps the retained
  output-name set and nonnumeric canonical-text skeleton fixed, scores only approximate
  Mathematica numeric literals tokenwise against the frozen digit-threshold profiles, and leaves
  exact symbolic outputs structural-only on this reviewed path.
- `compare_phase0_packet_set_to_reference.py` is the first multi-packet M6 comparator: it
  composes the existing packet comparator across the retained `required-set`, `de-d0-pair`, and
  `user-hook-pair` split, requires one unique reference packet label per comparison pair, requires
  each candidate packet root to publish exactly the retained benchmark split for that packet
  through `result-manifest.json` entries while ignoring uncaptured placeholder directories without
  manifests, and fails closed unless the compared benchmark ids match the scaffold's current
  `reference-captured` phase-0 set exactly.
- `score_phase0_packet_set_correct_digits.py` is the first multi-packet M6 correct-digit
  aggregator: it composes the reviewed packet-level scorer across that same retained packet split,
  requires one unique reference packet label per pair, requires each candidate packet root to
  publish exactly the retained benchmark split for that packet, and fails closed unless the scored
  benchmark ids match the scaffold's current `reference-captured` phase-0 set exactly.
- `audit_phase0_failure_codes.py` is the first packet-level M6 candidate failure-code audit: it
  consumes one candidate packet root on the existing manifest/run schema plus optional
  `failure-code-audit.json` sidecars, surfaces the frozen required failure-code profile for each
  selected benchmark, and reports which required codes are still missing or unexpectedly extra on
  the published candidate audit path.
- `bootstrap_reference_harness.py --self-check` now validates that the copied phase-0 catalog,
  placeholder index benchmark IDs, qualification scaffold IDs, digit-threshold floors, the
  reviewed `next_runtime_lane` blocker hints, and the ready-example `optional_capture_packet`
  hints stay synchronized with
  `specs/parity-matrix.yaml`, `references/case-studies/selected-benchmarks.md`, and
  `docs/verification-strategy.md`.
- The fetch helper `--self-check` now also verifies the tar extraction policy against rejected symlink, hardlink, device, absolute-path, and escaping entries.
- `capture_phase0_reference.py --self-check` exercises the retained-golden promotion flow end to
  end against a synthetic benchmark without requiring Kira or Fermat, including reuse of retained
  per-run manifests through `--resume-existing` and the explicit benchmark-selection contract for
  repeated `--benchmark-id` flags, `--required-only`, and direct `--optional-capture-packet`
  selection.
- `qualification_readiness.py --self-check` exercises the first M6 groundwork summary against one
  synthetic required retained root plus two synthetic optional packets, including scaffold-based
  inference of an older optional packet id that is absent from the retained packet summary.
- `qualification_case_study_readiness.py --self-check` exercises the first M6 case-study-family
  summary against synthetic selected-benchmark anchors, stronger-threshold inheritance, the
  reviewed singular blocker lane, and the recorded predecessor batch.
- `release_signoff_readiness.py --self-check` exercises the first blocked M7 release-readiness
  audit against one synthetic M6 summary, covering withheld release claims, visible runtime-lane
  blockers, checklist/doc-path auditing, and the docs-completion review path that is ready to
  audit before signoff itself is allowed to proceed.
- `compare_phase0_results_to_reference.py --self-check` exercises the first actual packet
  comparator against one synthetic retained reference root plus matching and mismatched candidate
  packets, covering hash mismatch, output-name drift, and missing-result-manifest rejection.
- `score_phase0_correct_digits.py --self-check` exercises the first packet-level correct-digit
  scorer against one synthetic retained reference root plus threshold-meeting, below-threshold,
  and skeleton-mismatched candidate packets, covering structural-only outputs and missing-root
  rejection outside the synthetic path.
- `compare_phase0_packet_set_to_reference.py --self-check` exercises the first multi-packet
  comparator against one synthetic required retained root plus two synthetic optional packet
  pairs, covering missing-packet rejection, placeholder-directory ignore behavior,
  extra-candidate-benchmark rejection, duplicate packet-label rejection, and malformed
  `--packet-root-pair` rejection.
- `score_phase0_packet_set_correct_digits.py --self-check` exercises the first multi-packet
  correct-digit aggregator against one synthetic required retained root plus two synthetic
  optional packet pairs, covering threshold failure, structural-only preservation,
  placeholder-directory ignore behavior, extra-candidate-benchmark rejection, duplicate
  packet-label rejection, skeleton drift rejection, and malformed `--packet-root-pair`
  rejection.
- `audit_phase0_failure_codes.py --self-check` exercises the first packet-level failure-code
  audit against one synthetic candidate packet, covering missing audit sidecars, malformed
  audit rejection, incomplete required-code coverage, duplicate observed-code rejection, and
  missing-result-manifest rejection on the published candidate path.
