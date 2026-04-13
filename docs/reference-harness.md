# Reference Harness Bootstrap

The upstream AMFlow harness is separate from the C++ runtime and exists only to capture goldens and
parity signals. Batch 2 turned the harness from a dry-run sketch into a concrete phase-0 bootstrap
that can materialize its layout, record pinned inputs, optionally fetch upstream assets, and
freeze placeholder golden metadata before Mathematica is available. `Milestone M0a` is now
accepted on top of that bootstrap as cluster/reference-harness readiness only; it does not imply
captured goldens or completed upstream comparisons.

## Current Durable Status

- accepted harness/bootstrap milestone: `M0a`
- authoritative repo baseline for that status: `fdbceea3cb94ee2e811573ad446e5777917c1bb0`
- accepted Slurm lane for the retained `M0a` packets: `sapphire`
- retained acceptance jobs:
  - phase-0 bootstrap rerun: `5276851`
  - shared Linux toolchain manifest: `5296876`
  - dependency sanity: `5297205`
  - Wolfram smoke: `5297206`
- accepted retained artifact roots:
  - shared Linux toolchain manifest:
    `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/toolchain/linux-toolchain-manifest.json`
  - phase-0 bootstrap root:
    `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-rerun-20260412-url/`
  - phase-0 provenance sidecar:
    `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-rerun-20260412-url/manifests/phase0-reference-upstream-provenance.json`
  - dependency-sanity packet:
    `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/m0a-dependency-sanity/`
  - Wolfram smoke packet:
    `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/m0a-wolfram-smoke/`
- accepted pinned upstream AMFlow source for the bootstrap lane:
  `https://gitlab.com/multiloop-pku/amflow.git` at ref `1.1`, with annotated tag object
  `a29fbdfe330ab172fe5ccdafcac2d6ec9211800e` and materialized checkout commit
  `775162498ab18493c45254b861669b4151b841ee`
- accepted pinned external tool paths in the current bootstrap lane:
  - Kira default binary:
    `/n/holylabs/schwartz_lab/Lab/obarrera/toolchains/autonomousIBP/kira/installs/kira-3.1/bin/kira-3.1`
  - Kira fallback binary:
    `/n/holylabs/schwartz_lab/Lab/obarrera/toolchains/autonomousIBP/kira/installs/kira-3.1_128/bin/kira-3.1_128`
  - Fermat:
    `/n/holylabs/schwartz_lab/Lab/obarrera/toolchains/autonomousIBP/fermat/5.25/ferl64/fer64`
  - Wolfram kernel:
    `/n/sw/helmod/apps/centos7/Core/mathematica/Mathematica_13.3.0/Executables/MathKernel`
- accepted pinned CPC extraction root:
  `/n/holylabs/schwartz_lab/Lab/obarrera/reference-inputs/autonomousIBP/cpc/amflow-gitlab-1.1-extracted`
- accepted non-harness follow-on on top of this baseline: `K0-pre-spec` is accepted as a repo-local
  K0 smoke fixture freeze derived from preserved input; latest candidate-local smoke replay job
  `5356840` passed
- accepted non-harness follow-on on top of this baseline: `K0-pre` is accepted as the narrow Kira
  kinematics YAML contract repair for that frozen smoke subset; latest clean-candidate build/test
  job `5356948` passed
- `K0` and `K0b` are still not accepted; `K0b` resumes next, and none of this widens `M0a`
  beyond bootstrap readiness
- `M0a` remains bootstrap-only: the phase-0 root still carries placeholder goldens and pending
  comparison summaries, so `M0b` remains open and no upstream parity claim is available yet

## Canonical Baseline

- Ubuntu 22.04 x86_64
- headless Wolfram kernel
- pinned public AMFlow checkout
- CPC archive as frozen example/backup source
- Kira 3.1
- Fermat

## Immutable Inputs

- AMFlow source kind, URL or local path, requested ref, and resolved commit SHA
- CPC archive source kind, URL or local path, archive path, extraction path, and sha256 when available
- Kira version and executable path
- Fermat version and executable path
- Mathematica version and Wolfram kernel path
- pinned thread count and platform label

## Per-Run Outputs

- environment manifest
- Mathematica stdout/stderr
- Kira logs
- generated `config/*.yaml`
- `jobs.yaml`
- final replacement rules / coefficient tables
- comparison summary against pinned goldens
- placeholder golden metadata and comparison index before real goldens are captured

## Repo-Local Layout

- `manifests/`: one JSON manifest per pinned harness bootstrap
- `inputs/`: optional cloned upstream AMFlow checkout, downloaded CPC archive, extracted CPC tree
- `logs/`: benchmark-specific Wolfram and Kira logs
- `generated-config/`: generated Kira and harness configuration files
- `results/`: benchmark-specific replacement rules and coefficient tables
- `comparisons/`: placeholder and later real comparison summaries
- `goldens/phase0/`: per-benchmark placeholder golden metadata plus an index file
- `templates/`: copied benchmark, manifest, env, Wolfram, and placeholder templates
- `state/`: bootstrap and fetch summaries for automation and audit trails

## Bring-Up Sequence

1. Materialize the harness root with `bootstrap_reference_harness.py`.
2. Copy the templates into the harness root and write a pinned manifest describing the layout and external inputs.
3. If `--amflow-url` is supplied, clone upstream AMFlow into `inputs/upstream/amflow` or reuse the existing checkout only after verifying its `origin` matches the requested URL. Fetch and refresh the requested ref before pinning the resolved commit.
4. If `--cpc-url` is supplied, download the CPC archive into `inputs/downloads/`, recreate `inputs/extracted/cpc`, extract the archive into that clean directory, and record the sha256.
   Tar extraction must enforce its own archive policy before writing any payload: reject symlink, hardlink, device, absolute-path, and escaping entries instead of relying on Python extraction defaults.
5. If local input paths are supplied instead, record them in the manifest without requiring a network fetch.
6. Freeze or refresh the placeholder phase-0 golden layout with `freeze_phase0_goldens.py`, creating stable metadata, coefficient-table, comparison, log, config, and result paths for each benchmark. Benchmark IDs are path-safe only. By default the script refreshes missing or placeholder-status files and preserves promoted real artifacts unless `--force` is supplied.
7. Verify the Wolfram kernel is usable in non-interactive mode once the licensed environment is ready.
8. Run dependency sanity checks in the pinned environment.
9. Reproduce `automatic_vs_manual` and `automatic_loop` first, then any available complex, phase-space, linear-propagator, arbitrary-`D0`, and custom-mode examples.
10. Replace placeholder golden files with real reference outputs, then rerun the pinned environment to prove reproducibility.

## Current Batch-2 Scripts

- `tools/reference-harness/scripts/bootstrap_reference_harness.py`: phase-0 coordinator for layout creation, template copying, manifest writing, optional upstream fetch, and optional placeholder golden freeze.
- `tools/reference-harness/scripts/fetch_upstream_amflow.py`: focused helper for cloning or refreshing the upstream AMFlow checkout after verifying the requested remote, and for downloading/extracting the CPC archive into a clean extraction directory with explicit tar-entry policy enforcement.
- `tools/reference-harness/scripts/freeze_phase0_goldens.py`: freezes or refreshes the benchmark-specific placeholder golden and comparison layout without requiring Mathematica, while rejecting unsafe benchmark IDs.

The scripts under `tools/reference-harness/` now implement a real repo-local bootstrap and can be wired to a licensed environment later for actual golden capture.
The two focused helpers also expose `--self-check` modes so the repo can rerun the Batch-2 regression scenarios without external network access, including tar-policy rejection cases for unsafe archive members.
