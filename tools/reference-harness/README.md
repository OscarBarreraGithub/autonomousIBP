# Reference Harness Bootstrap

This directory is the phase-0 bootstrap for standing up the upstream AMFlow validation environment. It can now materialize a real harness layout, record pinned inputs, optionally clone or refresh the public AMFlow checkout after verifying its remote, optionally download and extract the CPC archive into a clean directory, and freeze placeholder golden metadata without requiring Mathematica.

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

Both helper scripts also expose a local `--self-check` mode for the regression cases fixed in Batch 2:

```bash
python3 tools/reference-harness/scripts/fetch_upstream_amflow.py \
  --root /tmp/amflow-reference-bootstrap \
  --self-check

python3 tools/reference-harness/scripts/freeze_phase0_goldens.py \
  --root /tmp/amflow-reference-bootstrap \
  --self-check
```

If `inputs/upstream/amflow` already exists, the fetch helper verifies that `origin` matches `--amflow-url` and fetches the requested ref before it records the pinned commit. If the CPC archive is re-extracted, the helper recreates `inputs/extracted/cpc` first so stale files cannot survive reruns.
Tar extraction is policy-driven inside the helper itself: it rejects symlink, hardlink, device, absolute-path, and escaping entries before any tar payload is written, rather than relying on interpreter defaults.

To freeze or refresh the placeholder golden layout separately:

```bash
python3 tools/reference-harness/scripts/freeze_phase0_goldens.py \
  --root /tmp/amflow-reference-bootstrap \
  --manifest-path /tmp/amflow-reference-bootstrap/manifests/phase0-reference.json
```

Benchmark IDs in the phase-0 catalog must be path-safe ASCII tokens such as `automatic_vs_manual`. By default the freezer refreshes only missing or placeholder-status files and leaves promoted real artifacts alone. Use `--force` only if you intentionally want to overwrite existing non-placeholder files.

## Output Layout

- `manifests/`: pinned environment manifests
- `inputs/`: optional cloned/downloaded upstream AMFlow and CPC inputs
- `logs/`: Wolfram and Kira logs
- `generated-config/`: emitted reducer/job config
- `results/`: captured replacement rules and coefficient tables
- `comparisons/`: parity reports vs frozen goldens
- `goldens/`: phase-0 golden metadata, placeholder coefficient tables, and index files
- `templates/`: copied manifest, environment, Wolfram, and phase-0 benchmark templates
- `state/`: bootstrap/fetch summaries for automation and audit trails

## Required External Inputs

- a licensed Wolfram kernel
- a pinned public AMFlow checkout
- the AMFlow CPC archive
- Kira 3.1
- Fermat

## Notes

- The phase-0 bootstrap does not require Mathematica to run. It freezes the expected benchmark, output, and comparison paths so later parity tooling can bind to stable locations before real reference data is captured.
- Real goldens are still produced only after the pinned upstream AMFlow environment is run under a licensed Wolfram kernel.
- The templates here are designed to match the repo-level docs in `docs/reference-harness.md` and the manifest shape in `specs/reference-harness-manifest.yaml`.
- The fetch helper `--self-check` now also verifies the tar extraction policy against rejected symlink, hardlink, device, absolute-path, and escaping entries.
