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

All four harness scripts also expose a local `--self-check` mode for the regression cases fixed in
Batch 2 and the new M5/M6 catalog/scaffold coherence lock, including the theory-backed
`next_runtime_lane` blocker hints for the still-deferred `b61k` / `b62k` / `b63h` / `b64h`
surfaces and the `optional_capture_packet` grouping for the retained `de-d0-pair` and retained
`user-hook-pair`:

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
```

`amflow-tests` now exercises all four helper self-checks through the configured
`Python3_EXECUTABLE`, so the repo-local gate covers bootstrap, fetch, placeholder-freeze, and
retained-capture regression paths without needing a real benchmark packet.

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
- `templates/`: copied manifest, environment, Wolfram, phase-0 benchmark, and qualification templates
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
  predecessor slices `b61h` / `b62j` / `b63f` / `b64g`.
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
