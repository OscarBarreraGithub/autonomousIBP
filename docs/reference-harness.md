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
  - retained ready-example optional-capture root:
    `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260422-de-d0-pair/`
  - retained ready-example optional-capture summary:
    `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260422-de-d0-pair/state/phase0-reference.capture.json`
  - retained ready-example optional-capture comparison summaries:
    `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260422-de-d0-pair/comparisons/phase0/differential_equation_solver.summary.json`
    `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260422-de-d0-pair/comparisons/phase0/spacetime_dimension.summary.json`
  - retained user-hook optional-capture root:
    `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260422-user-hook-pair/`
  - retained user-hook optional-capture summary:
    `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260422-user-hook-pair/state/phase0-reference.capture.json`
  - retained user-hook optional-capture comparison summaries:
    `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260422-user-hook-pair/comparisons/phase0/user_defined_amfmode.summary.json`
    `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260422-user-hook-pair/comparisons/phase0/user_defined_ending.summary.json`
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
  `automatic_vs_manual` and `automatic_loop`. The sibling optional packet
  `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260422-de-d0-pair/`
  also retains `differential_equation_solver` and `spacetime_dimension` with passed
  bundled-backup and rerun-reproducibility summaries, but because that packet intentionally reran
  only those two examples its manifest still truthfully records
  `phase0.capture_state = "bootstrap-only"` and it does not replace the accepted M0b root. The
  sibling optional packet
  `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260422-user-hook-pair/`
  likewise retains `user_defined_amfmode` and `user_defined_ending` with passed bundled-backup and
  rerun-reproducibility summaries, and for the same reason its manifest also stays
  `phase0.capture_state = "bootstrap-only"`. The remaining still-blocked frozen examples
  `complex_kinematics`, `feynman_prescription`, `automatic_phasespace`, and `linear_propagator`,
  plus later regression families, stay on the rolling future-capture lane. The copied phase-0
  catalog now marks the theory-blocked feature captures explicitly:
  `complex_kinematics -> b61n`, `feynman_prescription -> b63k`,
  `automatic_phasespace -> b63k`, and `linear_propagator -> b64k`; it also carries
  `optional_capture_packet = de-d0-pair` and `optional_capture_packet = user-hook-pair` for the
  two retained ready-example pairs. The qualification scaffold keeps the singular guardrail anchor
  `one-singular-endpoint-case -> b62n`

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
- `templates/`: copied benchmark, manifest, env, Wolfram, placeholder, qualification, and release-signoff templates
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
9. Reproduce the required phase-0 benchmark set (`automatic_vs_manual` and `automatic_loop`) first, then any remaining examples that are not still marked with a `next_runtime_lane` blocker in the copied phase-0 catalog, such as `differential_equation_solver`, `spacetime_dimension`, `user_defined_amfmode`, and `user_defined_ending`. The copied catalog now also groups those ready optional examples by `optional_capture_packet`, so `de-d0-pair` and `user-hook-pair` stay explicit, and `capture_phase0_reference.py` can consume those packet ids directly.
10. Promote the primary retained outputs to goldens, canonicalize the Mathematica output files for truthful comparison, and rerun the pinned environment to prove reproducibility.

## Current Batch-2 Scripts

- `tools/reference-harness/scripts/bootstrap_reference_harness.py`: phase-0 coordinator for layout creation, template copying, manifest writing, optional upstream fetch, and optional placeholder golden freeze.
- `tools/reference-harness/scripts/bootstrap_reference_harness.py --self-check`: local regression
  for bootstrap layout creation, placeholder-freeze wiring, and machine-checked coherence between
  the copied phase-0 catalog, placeholder index benchmark IDs, qualification scaffold,
  `specs/parity-matrix.yaml`, `references/case-studies/selected-benchmarks.md`, the
  digit-threshold floors in `docs/verification-strategy.md`, the retained optional-capture state
  for `differential_equation_solver` / `spacetime_dimension` and
  `user_defined_amfmode` / `user_defined_ending`, the absence of any remaining ready uncaptured
  optional packet, and the theory-backed `next_runtime_lane` blocker hints for the still-deferred
  `b61n` / `b62n` / `b63k` / `b64k` surfaces.
- `tools/reference-harness/scripts/fetch_upstream_amflow.py`: focused helper for cloning or refreshing the upstream AMFlow checkout after verifying the requested remote, and for downloading/extracting the CPC archive into a clean extraction directory with explicit tar-entry policy enforcement.
- `tools/reference-harness/scripts/freeze_phase0_goldens.py`: freezes or refreshes the benchmark-specific placeholder golden and comparison layout without requiring Mathematica, while rejecting unsafe benchmark IDs.
- `tools/reference-harness/scripts/capture_phase0_reference.py`: stages isolated AMFlow example runs, patches the pinned reducer install hook, retains the primary and rerun outputs, canonicalizes Mathematica file ordering for truthful comparisons, and promotes the required phase-0 benchmark set into `reference-captured` state when every required benchmark matches both bundled `kira_*` backups and the rerun. Repeated `--benchmark-id` flags are deduplicated and executed in the frozen phase-0 catalog order, `--optional-capture-packet` selects every matching ready benchmark in that same frozen order, at most one explicit selection mode may be used at a time, and `--resume-existing` reuses already-retained per-run manifests after a walltime kill instead of replaying completed labels. Narrower optional packets may retain individual examples while the manifest truthfully remains `bootstrap-only` if the required phase-0 pair is absent.
- `tools/reference-harness/scripts/validate_qualification_scaffold.py`: the first narrow M6 readiness helper. It consumes one or more retained phase-0 packet roots, validates their manifests / capture summaries / promoted comparison surfaces against the current qualification scaffold, and reports which frozen phase-0 example classes already have retained goldens across the accepted `required-set`, `de-d0-pair`, and `user-hook-pair` packet split. This is an evidence-audit helper only: it does not run any qualification numerics and does not claim case-study parity or `Milestone M6` closure.
- `tools/reference-harness/scripts/qualification_readiness.py`: first M6 groundwork helper. It aggregates the accepted required retained root plus any narrower optional packet roots against `templates/qualification-benchmarks.json`, validates that every observed captured example still publishes promoted golden/result manifests plus passing comparison summaries, and writes one machine-readable summary of retained `reference-captured` versus still-pending phase-0 examples together with the blocked `next_runtime_lane` hints that remain on the scaffold. Older optional packets may predate the explicit `optional_capture_packet` summary field; when every captured example in one packet shares the same scaffold packet hint, this helper infers that packet id from the scaffold instead of requiring the external retained packet to be rewritten.
- `tools/reference-harness/scripts/qualification_case_study_readiness.py`: first M6 case-study-family helper. It consumes the frozen qualification scaffold plus `references/case-studies/selected-benchmarks.md`, `specs/parity-matrix.yaml`, `docs/verification-strategy.md`, and the implementation ledger, validates that the selected literature anchors, digit floors, failure/regression profiles, and the reviewed singular `next_runtime_lane` blocker stay synchronized, and writes one machine-readable readiness summary of literature-anchor, matrix-only, strong-precision, and runtime-blocked case-study families. This is still evidence/planning only: it does not compare retained case-study numerics and does not claim `Milestone M6` is passing.
- `tools/reference-harness/scripts/compare_phase0_results_to_reference.py`: first actual M6 phase-0 packet comparator. It consumes one retained reference packet root plus one candidate packet root that publishes the existing `result-manifest.json` and primary `run-manifest.json` schema, fails closed unless every selected benchmark keeps the exact retained output-name set and canonical hashes, and surfaces the frozen digit-threshold, failure-code, and regression metadata from the qualification scaffold alongside the per-benchmark match report. This is still comparator plumbing only: it does not launch the C++ runtime, does not compute correct-digit scores, does not inspect candidate failure-code behavior, and does not claim `Milestone M6` is passing.
- `tools/reference-harness/scripts/score_phase0_correct_digits.py`: first actual M6 packet-level correct-digit scorer. It consumes one retained reference packet root plus one candidate packet root on the same `result-manifest.json` and primary `run-manifest.json` schema, requires the retained output-name set and nonnumeric canonical-text skeleton to stay fixed, scores only approximate Mathematica numeric literals tokenwise against the frozen digit-threshold profiles, and leaves exact symbolic outputs structural-only on this reviewed path. This is still qualification plumbing only: it does not launch the C++ runtime, does not inspect candidate failure-code behavior, does not aggregate scores across the full packet split, and does not claim `Milestone M6` is passing.
- `tools/reference-harness/scripts/compare_phase0_packet_set_to_reference.py`: first multi-packet M6 phase-0 comparator. It consumes one or more `--packet-root-pair <reference_root>::<candidate_root>` mappings, composes the retained packet comparator across the accepted `required-set`, `de-d0-pair`, and `user-hook-pair` split, requires one unique reference packet label per pair, requires each candidate packet root to publish exactly the retained benchmark split for that packet through `result-manifest.json` entries while ignoring uncaptured placeholder directories without manifests, and fails closed unless the compared benchmark ids match the scaffold's full current `reference-captured` phase-0 set exactly. This is still comparator plumbing only: it does not launch the C++ runtime, does not compute correct-digit scores, does not inspect candidate failure-code behavior, and does not claim `Milestone M6` is passing.
- `tools/reference-harness/scripts/score_phase0_packet_set_correct_digits.py`: first multi-packet M6 correct-digit aggregator. It consumes one or more `--packet-root-pair <reference_root>::<candidate_root>` mappings, composes the reviewed packet-level correct-digit scorer across the accepted `required-set`, `de-d0-pair`, and `user-hook-pair` split, requires one unique reference packet label per pair, requires each candidate packet root to publish exactly the retained benchmark split for that packet, and fails closed unless the scored benchmark ids match the scaffold's full current `reference-captured` phase-0 set exactly. This is still score-aggregation plumbing only: it does not launch the C++ runtime, does not inspect candidate failure-code behavior, does not compare case-study numerics, and does not claim `Milestone M6` is passing.
- `tools/reference-harness/scripts/audit_phase0_failure_codes.py`: first packet-level M6 candidate failure-code audit. It consumes one candidate packet root that publishes the existing `result-manifest.json` and primary `run-manifest.json` schema plus one optional `results/phase0/<benchmark>/failure-code-audit.json` sidecar per benchmark, surfaces the frozen required failure-code profile from the qualification scaffold, and reports which required codes are still missing or unexpectedly extra on the published candidate audit path. This is still qualification plumbing only: it does not launch the C++ runtime, does not compare canonical outputs or correct digits, and does not claim `Milestone M6` is passing.
- `tools/reference-harness/scripts/release_signoff_readiness.py`: first executable M7 helper. It consumes one machine-readable `qualification_readiness.py` summary plus `templates/release-signoff-checklist.json`, audits that the checklist source/docs targets exist inside the repo, preserves the blocked `next_runtime_lane` frontier from the M6 evidence packet, and writes one blocked release-readiness summary that keeps `Milestone M6`, feature-parity closure, retained-reference qualification, and final parity sign-off withheld explicitly. This is still release-prep plumbing only: it does not mark `Milestone M6` or `Milestone M7` closed, does not review performance or diagnostics, and does not claim release readiness.

## Qualification Scaffold

- `tools/reference-harness/templates/qualification-benchmarks.json` is the first machine-readable
  M6 scaffold. It mirrors every parity-matrix frozen example class and case-study family, freezes
  the current digit-threshold profiles, and carries the required failure-code and regression
  profiles that future qualification packets must keep visible. Where the next retained capture is
  still blocked by unfinished runtime work, the scaffold and copied phase-0 catalog also carry one
  optional `next_runtime_lane` hint so future capture threads do not need to rediscover the
  current `b61n` / `b62n` / `b63k` / `b64k` blocker map from scratch. Those hints stay aligned
  with the current theory frontier while still anchoring against the recorded predecessor slices
  `b61j` / `b62m` / `b63j` / `b64j`. Ready optional examples may
  instead carry `optional_capture_packet` so future capture threads keep the retained `de-d0-pair`
  and retained `user-hook-pair` grouped without re-planning that packet shape.
- `tools/reference-harness/scripts/validate_qualification_scaffold.py` is the first live consumer
  of that scaffold on actual retained artifacts. It keeps the required `reference-captured`
  `required-set` packet distinct from the narrower optional packets whose manifests truthfully stay
  `bootstrap-only`, while still crediting their benchmark-level retained goldens on the reviewed
  `de-d0-pair` and `user-hook-pair` surfaces.
- The scaffold is planning metadata only. Adding or editing it does not claim any new
  `reference-captured` benchmark, any new runtime parity, or any reviewed solver widening.
- Future optional-capture lanes should pair the scaffold with
  `tools/reference-harness/templates/phase0-benchmarks.json` so that catalog completeness
  (`feynman_prescription` included) and later qualification thresholds stay synchronized.
- The first M6 groundwork helper is `tools/reference-harness/scripts/qualification_readiness.py`.
  It is still evidence-only: the helper summarizes the currently retained phase-0 packet set and
  verifies scaffold alignment, but it does not run any benchmark family qualification corpus and
  does not claim `Milestone M6` is passing.
- `tools/reference-harness/scripts/qualification_case_study_readiness.py` is the first live
  consumer of the scaffold's case-study-family metadata. It validates the selected literature
  anchors, parity labels, digit floors, failure/regression profiles, and the reviewed singular
  runtime-lane blocker map against the frozen sources and emits one machine-readable family
  readiness summary, but it still does not compare retained case-study numerics or claim
  `Milestone M6` is passing.
- The first actual M6 phase-0 comparator is
  `tools/reference-harness/scripts/compare_phase0_results_to_reference.py`. It compares one
  candidate packet root against one retained reference packet root on the existing manifest/run
  schema and requires exact canonical output-hash agreement on the selected phase-0 examples, but
  it still does not score correct digits, audit candidate failure-code behavior, or claim
  `Milestone M6` is passing.
- The first actual M6 packet-level correct-digit scorer is
  `tools/reference-harness/scripts/score_phase0_correct_digits.py`. It consumes one retained
  reference packet root plus one candidate packet root on that same manifest/run schema, keeps the
  retained output-name set and nonnumeric canonical-text skeleton fixed, scores only approximate
  Mathematica numeric literals tokenwise against the frozen digit-threshold profiles, and leaves
  exact symbolic outputs structural-only on this reviewed path. It still does not audit candidate
  failure-code behavior, aggregate scores across the full packet split, or claim `Milestone M6`
  is passing.
- The first multi-packet M6 phase-0 comparator is
  `tools/reference-harness/scripts/compare_phase0_packet_set_to_reference.py`. It composes that
  packet-level comparator across the retained `required-set`, `de-d0-pair`, and `user-hook-pair`
  split, requires one unique reference packet label per pair, requires each candidate packet root
  to publish exactly the retained benchmark split for that packet through `result-manifest.json`
  entries while ignoring uncaptured placeholder directories without manifests, and requires the
  compared benchmark ids to match the scaffold's full current `reference-captured` phase-0 set
  exactly, but it still does not aggregate packet-level digit scores, audit candidate failure-code
  behavior, or
  claim `Milestone M6` is passing.
- The first multi-packet M6 correct-digit aggregator is
  `tools/reference-harness/scripts/score_phase0_packet_set_correct_digits.py`. It composes the
  reviewed packet-level correct-digit scorer across that same retained `required-set`,
  `de-d0-pair`, and `user-hook-pair` split, requires one unique reference packet label per pair,
  requires each candidate packet root to publish exactly the retained benchmark split for that
  packet, and requires the scored benchmark ids to match the scaffold's full current
  `reference-captured` phase-0 set exactly, but it still does not audit candidate failure-code
  behavior, compare case-study numerics, or claim `Milestone M6` is passing.
- The first packet-level M6 candidate failure-code audit is
  `tools/reference-harness/scripts/audit_phase0_failure_codes.py`. It consumes one candidate
  packet root on the existing manifest/run schema plus optional
  `results/phase0/<benchmark>/failure-code-audit.json` sidecars, surfaces the frozen required
  failure-code profile for each selected benchmark, and reports which required codes are still
  missing or unexpectedly extra on the published audit path, but it still does not compare
  canonical outputs, does not score correct digits, and does not claim `Milestone M6` is passing.

## Release Sign-Off Scaffold

- `tools/reference-harness/templates/release-signoff-checklist.json` is the first
  machine-readable M7 scaffold. It freezes the release-review sections that later sign-off lanes
  must clear in order: qualification-corpus closure, performance review, diagnostic review, docs
  completion, and the final parity sign-off statement.
- The scaffold consumes the existing qualification surfaces rather than replacing them: it points
  future M7 helpers at `docs/release-signoff-checklist.md`,
  `templates/qualification-benchmarks.json`,
  `tools/reference-harness/scripts/qualification_readiness.py`,
  `tools/reference-harness/scripts/release_signoff_readiness.py`, the parity matrix, and the
  durable docs as the inputs that must already be coherent before release review can start.
- The first executable M7 helper is
  `tools/reference-harness/scripts/release_signoff_readiness.py`. It consumes one retained
  M6-readiness summary instead of re-running qualification, audits the checklist source/doc paths,
  and writes one blocked release-readiness summary that keeps the current `b61n` / `b62n` /
  `b63k` / `b64k` frontier visible while `Milestone M6` and the later release-review sections
  remain open.
- The scaffold is planning metadata only. Adding or editing it does not run qualification,
  performance, or diagnostic review; does not claim `Milestone M6` or `Milestone M7` is closed;
  and does not claim release readiness.

The scripts under `tools/reference-harness/` now implement both the real repo-local bootstrap and
the retained-golden promotion path. All thirteen helpers expose `--self-check` modes so the repo
can rerun the bootstrap, catalog/scaffold coherence, retained-capture regression scenarios,
scaffold validation, qualification-readiness, case-study-family readiness, blocked
release-readiness, the packet-level candidate failure-code audit, and the single-packet plus
packet-set retained-reference comparators and correct-digit scorers without needing a full
benchmark packet. `amflow-tests` now drives those bootstrap, fetch, placeholder-freeze,
retained-capture, scaffold-validation, qualification-readiness, case-study-family readiness,
blocked release-readiness, the retained single-packet comparator, packet-level correct-digit
scorer, packet-level failure-code audit, packet-set comparator, and packet-set correct-digit
scorer self-checks through the configured repo-local Python interpreter, and the retained-capture
helper also self-checks the restart-safe `--resume-existing` path plus the explicit benchmark-id,
optional-packet, and `--required-only` selection contract, including direct
`optional_capture_packet` selection.
