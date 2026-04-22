# Reference Harness Bootstrap

The upstream AMFlow harness is separate from the C++ runtime and exists only to capture goldens and
parity signals. Batch 2 turned the harness from a dry-run sketch into a concrete phase-0 bootstrap
that can materialize its layout, record pinned inputs, optionally fetch upstream assets, and
freeze placeholder golden metadata before Mathematica is available. The follow-on retained-capture
path now stages an isolated AMFlow work tree, patches the pinned Kira/Fermat install hook, runs
selected examples twice, promotes the primary run to retained goldens, and writes canonicalized
backup-match plus rerun-reproducibility summaries. `Milestone M0a` remains the bootstrap-only
readiness seam; `Milestone M0b` closes only once the required phase-0 benchmark set has real
retained outputs and rerun evidence.

## Current Durable Status

- starting `main` / `origin/main` head for this closeout packet: `4dcb17f6a4fd9d2ebf28e72922e74c06fb461d82`
- accepted harness/bootstrap milestone: `M0a`
- accepted retained-capture milestone: `M0b`
- accepted Slurm lane for the retained `M0a` packets: `sapphire`
- retained acceptance jobs:
  - phase-0 bootstrap rerun: `5276851`
  - shared Linux toolchain manifest: `5296876`
  - dependency sanity: `5297205`
  - Wolfram smoke: `5297206`
  - retained required-benchmark capture, initial packet: `6721330` (`automatic_vs_manual` primary completed before walltime)
  - retained required-benchmark capture, resumed packet: `6732338`
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
  - retained required-benchmark capture root:
    `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260419-required-set/`
  - retained required-benchmark capture summary:
    `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260419-required-set/state/phase0-reference.capture.json`
  - retained required-benchmark comparison summaries:
    `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260419-required-set/comparisons/phase0/automatic_vs_manual.summary.json`
    `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260419-required-set/comparisons/phase0/automatic_loop.summary.json`
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
- accepted non-harness follow-on on top of this baseline: `K0b.1` is accepted on that frozen
  repo-local smoke subset; clean-candidate job `5425248` passed, packet job `5425379` passed on
  `sapphire`, and the canonical retained root
  `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/k0/reducer-smoke` is coherent and complete
- `K0` and `K0b` are therefore accepted only for that frozen repo-local smoke subset, and none of
  this widens `M0a` beyond bootstrap readiness or implies any broader parity claim
- `M0a` remains the bootstrap precursor: placeholder goldens and pending comparison summaries are
  sufficient there for path stability only
- `M0b` is now accepted on the required phase-0 benchmark set only: the retained root above has
  promoted goldens plus passed bundled-backup and rerun-reproducibility summaries for
  `automatic_vs_manual` and `automatic_loop`. The remaining frozen examples
  `complex_kinematics`, `differential_equation_solver`, `feynman_prescription`,
  `automatic_phasespace`, `linear_propagator`, `spacetime_dimension`, `user_defined_amfmode`,
  and `user_defined_ending`, plus the later regression families, stay on the rolling
  future-capture lane. The copied phase-0 catalog now also marks the theory-blocked feature
  captures explicitly: `complex_kinematics -> b61g`, `feynman_prescription -> b63e`,
  `automatic_phasespace -> b63e`, and `linear_propagator -> b64d`

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
- final raw Mathematica outputs and canonicalized sidecars
- promoted golden-output manifests and result manifests
- comparison summaries against bundled `kira_*` backups and rerun reproducibility
- placeholder golden metadata and comparison index before real goldens are captured

## Repo-Local Layout

- `manifests/`: one JSON manifest per pinned harness bootstrap
- `inputs/`: optional cloned upstream AMFlow checkout, downloaded CPC archive, extracted CPC tree
- `logs/`: benchmark-specific Wolfram and Kira logs
- `generated-config/`: generated Kira and harness configuration files
- `results/`: benchmark-specific raw outputs, run manifests, and canonicalized sidecars
- `comparisons/`: placeholder and later real comparison summaries
- `goldens/phase0/`: per-benchmark placeholder golden metadata, promoted golden-output manifests, and an index file
- `templates/`: copied benchmark, manifest, env, Wolfram, placeholder, and qualification templates
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
9. Reproduce the required phase-0 benchmark set (`automatic_vs_manual` and `automatic_loop`) first, then any remaining examples that are not still marked with a `next_runtime_lane` blocker in the copied phase-0 catalog, such as `differential_equation_solver`, `spacetime_dimension`, `user_defined_amfmode`, and `user_defined_ending`.
10. Promote the primary retained outputs to goldens, canonicalize the Mathematica output files for truthful comparison, and rerun the pinned environment to prove reproducibility.

## Current Batch-2 Scripts

- `tools/reference-harness/scripts/bootstrap_reference_harness.py`: phase-0 coordinator for layout creation, template copying, manifest writing, optional upstream fetch, and optional placeholder golden freeze.
- `tools/reference-harness/scripts/bootstrap_reference_harness.py --self-check`: local regression
  for bootstrap layout creation, placeholder-freeze wiring, and machine-checked coherence between
  the copied phase-0 catalog, placeholder index benchmark IDs, qualification scaffold,
  `specs/parity-matrix.yaml`, `references/case-studies/selected-benchmarks.md`, the
  digit-threshold floors in `docs/verification-strategy.md`, and the theory-backed
  `next_runtime_lane` blocker hints for the still-deferred `b61g` / `b62f` / `b63e` / `b64d`
  surfaces.
- `tools/reference-harness/scripts/fetch_upstream_amflow.py`: focused helper for cloning or refreshing the upstream AMFlow checkout after verifying the requested remote, and for downloading/extracting the CPC archive into a clean extraction directory with explicit tar-entry policy enforcement.
- `tools/reference-harness/scripts/freeze_phase0_goldens.py`: freezes or refreshes the benchmark-specific placeholder golden and comparison layout without requiring Mathematica, while rejecting unsafe benchmark IDs.
- `tools/reference-harness/scripts/capture_phase0_reference.py`: stages isolated AMFlow example runs, patches the pinned reducer install hook, retains the primary and rerun outputs, canonicalizes Mathematica file ordering for truthful comparisons, and promotes the required phase-0 benchmark set into `reference-captured` state when every required benchmark matches both bundled `kira_*` backups and the rerun. Repeated `--benchmark-id` flags are deduplicated and executed in the frozen phase-0 catalog order, `--required-only` stays mutually exclusive with explicit benchmark ids, and `--resume-existing` reuses already-retained per-run manifests after a walltime kill instead of replaying completed labels.

## Qualification Scaffold

- `tools/reference-harness/templates/qualification-benchmarks.json` is the first machine-readable
  M6 scaffold. It mirrors every parity-matrix frozen example class and case-study family, freezes
  the current digit-threshold profiles, and carries the required failure-code and regression
  profiles that future qualification packets must keep visible. Where the next retained capture is
  still blocked by unfinished runtime work, the scaffold and copied phase-0 catalog also carry one
  optional `next_runtime_lane` hint so future capture threads do not need to rediscover the
  current `b61g` / `b62f` / `b63e` / `b64d` blocker map from scratch.
- The scaffold is planning metadata only. Adding or editing it does not claim any new
  `reference-captured` benchmark, any new runtime parity, or any reviewed solver widening.
- Future optional-capture lanes should pair the scaffold with
  `tools/reference-harness/templates/phase0-benchmarks.json` so that catalog completeness
  (`feynman_prescription` included) and later qualification thresholds stay synchronized.

The scripts under `tools/reference-harness/` now implement both the real repo-local bootstrap and
the retained-golden promotion path. All four helpers expose `--self-check` modes so the repo can
rerun the bootstrap, catalog/scaffold coherence, and retained-capture regression scenarios without
needing a full benchmark packet. `amflow-tests` now drives those bootstrap, fetch,
placeholder-freeze, and retained-capture self-checks through the configured repo-local Python
interpreter, and the retained-capture helper also self-checks the restart-safe
`--resume-existing` path plus the explicit benchmark-selection contract.
