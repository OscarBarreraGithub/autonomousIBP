# Verification Strategy Bootstrap

The migration is phase-gated. Every phase must pass:

- self-consistency
- upstream AMFlow parity
- numerical robustness

## Current Durable Status

- the actual SSH remote `main` head for this packet is `7dee2a0a574f2df991edf917290cc4600c9ae215`; local tracking `origin/main` matches at `7dee2a0a574f2df991edf917290cc4600c9ae215`
- last fully accepted release baseline remains `bbd7b744b69a413bf34e4b706cd737e2b266256a`, while reviewed code on actual `main` now extends beyond that baseline through landed `Batch 58h` / `Batch 58i` plus the landed narrow `Milestone M3` closure packet at `7dee2a0`, on top of the earlier Kira rational-function prefactor surface and xints `insert_prefactors` wiring
- `Milestone M0a` is accepted as cluster/reference-harness bootstrap readiness only
- `Milestone M0b` is accepted on the required phase-0 benchmark set: retained root
  `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260419-required-set`,
  initial packet job `6721330` reached the completed `automatic_vs_manual` primary before
  walltime, resumed job `6732338` completed the packet via `--resume-existing`, and the manifest
  now records `phase0.capture_state = "reference-captured"` with both required benchmarks
  captured
- a sibling optional-capture packet at
  `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260422-de-d0-pair`
  now retains `differential_equation_solver` and `spacetime_dimension` with passed
  bundled-backup and rerun reproducibility summaries; because it intentionally reran only those
  two examples, its manifest truthfully remains `phase0.capture_state = "bootstrap-only"` and it
  does not replace the accepted M0b root
- a sibling optional-capture packet at
  `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260422-user-hook-pair`
  now retains `user_defined_amfmode` and `user_defined_ending` with passed bundled-backup and
  rerun reproducibility summaries; because it intentionally reran only those two examples, its
  manifest truthfully remains `phase0.capture_state = "bootstrap-only"` and it does not replace
  the accepted M0b root
- `Operational Gate B0/G1` is accepted: clean-candidate `sapphire` job `5305579` passed
  `cmake -S . -B build`, `cmake --build build --parallel 1`, and
  `ctest --test-dir build --output-on-failure`
- `K0-pre-spec` is accepted as a repo-local K0 smoke fixture freeze derived from preserved input;
  latest candidate-local smoke replay job `5356840` passed
- `K0-pre` is accepted as the narrow Kira kinematics YAML contract repair for that frozen smoke
  subset; latest clean-candidate build/test job `5356948` passed the canonical build/test gate
- `K0b.1` is accepted: clean-candidate job `5425248` passed for candidate
  `/n/holylabs/schwartz_lab/Lab/obarrera/autonomousIBP-artifacts/candidates/k0b1-final-20260413T061602Z-2330933`,
  packet job `5425379` passed on `sapphire`, and the canonical retained root
  `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/k0/reducer-smoke` is coherent and complete
- `K0` and `K0b` are now accepted only for the frozen repo-local K0 smoke subset via that one
  coherent retained reducer-smoke packet with an honest bootstrap manifest on `main`
- `Batch 47` / `Milestone M2` remain accepted narrowly on the clean
  `main@f4bf8af2419a20f04ae40eceebbd5d12f3b2a92c` baseline: original clean-candidate `sapphire`
  job `5431987` passed for candidate
  `/n/holylabs/schwartz_lab/Lab/obarrera/autonomousIBP-artifacts/candidates/b47-clean-candidate-20260413T072226Z-LRVIIR`;
  and independent rereview found no blocking findings after the strengthened equivalence,
  non-mutation, and failure-preservation fixes
- `Batch 48` is accepted on `main`: final accepted clean-candidate `sapphire` job `5439311`
  cleared the landing packet and commit `f4bf8af2419a20f04ae40eceebbd5d12f3b2a92c` is the clean
  baseline before the current staging work
- `Batch 49` is accepted on `main`: commit `b0275a8d8ce3f33577629f44d7b168b4d4ef8bb2` landed the
  narrow builtin `Propagator` structural-selector packet
- `Batch 49b` is accepted on `main`: local module-loaded `cmake -S . -B build`,
  `cmake --build build --parallel 1`, and `ctest --test-dir build --output-on-failure` passed in
  `/tmp/autoIBP-b49b-mass`; final clean-candidate `sapphire` job `5457143` passed for candidate
  `/n/holylabs/schwartz_lab/Lab/obarrera/autonomousIBP-artifacts/candidates/b49b-final-clean-candidate-20260413T112519Z-Kabcrq`;
  and second-pass rereview found no blocking or medium findings remaining
- that landed `Batch 49b` packet is limited to the narrow bootstrap builtin `Mass` seam on the
  reviewed subset plus the minimal eta-generated-path mass-coherence widening required to keep
  selected equal-mass reducer-facing literals aligned with planner grouping. It does not accept
  full upstream topology/component-order `Mass` semantics, broader same-priority tie-break parity,
  broader symbolic mass canonicalization, `Propagator::prescription` metadata interpretation,
  broader automatic boundary execution/provider parity, broader ending semantics, broader Kira
  smoke, or upstream parity
- `Batch 50a` is accepted on `main`: clean-candidate `sapphire` job `5465841` passed for
  candidate
  `/n/holylabs/schwartz_lab/Lab/obarrera/autonomousIBP-artifacts/candidates/b50a-final-clean-candidate-20260413T123155Z-1gWbow`,
  final status `COMPLETED` on `holy8a32607` in `00:00:32`, second-pass rereview found no
  remaining findings, and the landing commit is `bbd7b744b69a413bf34e4b706cd737e2b266256a`
- that landed `Batch 50a` packet is still narrow: it adds only the internal eta-topology preflight
  seam and truthful early failure for `Branch` / `Loop`; it does not accept real selector
  semantics, `AnalyzeTopSector` parity, graph-polynomial availability, or broader topology
  analysis
- landed `Batch 50b` on current `main` is the internal topology-prerequisite bridge and
  prereq-snapshot seam for `Branch` / `Loop`, still blocked and still deferring truthful selector
  semantics to `Batch 50`
- current-thread local module-loaded `cmake -S . -B build`, `cmake --build build --parallel 1`,
  and `ctest --test-dir build --output-on-failure` passed again for that landed `Batch 50b`
  surface, and secondary review found no material findings on the declared narrow scope
- clean-candidate attempt `5480669` is not acceptance evidence: that submission OOM-killed during
  configure because it omitted an explicit memory request
- a later clean-candidate job `5482487` did pass for candidate
  `/n/holylabs/schwartz_lab/Lab/obarrera/autonomousIBP-artifacts/candidates/b50b-final-clean-candidate-20260413T133615Z-775743d3`
  under retained artifact root
  `/n/holylabs/schwartz_lab/Lab/obarrera/autonomousIBP-artifacts/jobs/b50b-final-clean-candidate-20260413T133615Z-775743d3`,
  and the durable docs now record that evidence
- landed `Batch 50` through `Batch 55` are now dependency-satisfied `main` history for future
  verification planning; `M0b` is now accepted as retained phase-0 reference capture for the
  required benchmark set, so later parity milestones may consume those goldens but still require
  their own reviewed gates
- landed `Batch 55` on current `main` now adds typed `master_set_instability` and
  `continuation_budget_exhausted` failures on top of the landed retry surface; local
  `cmake -S . -B build`, `cmake --build build --parallel 1`,
  `ctest --test-dir build --output-on-failure`, and `./build/amflow-tests` all passed before the
  landing commit `4dcb17f6a4fd9d2ebf28e72922e74c06fb461d82`
- landed `Batch 56` adds a narrow solved-path cache manifest plus `UseCache` replay of
  successful solved-path diagnostics on the two
  `SolveAmfOptionsEtaModeSeries(...)` overloads only; the repaired `Batch 57` keeps that cache
  slice and adds wrapper-only `amf_options.skip_reduction == true` reuse over already-prepared
  matching eta-generated state on those same two overloads; and landed `Batch 58` keeps both of
  those seams while wiring the listed `AmfOptions` runtime fields into a live wrapper-owned solve
  policy on those same two overloads. Solved-path input/request fingerprinting and
  `skip_reduction` replay validation now treat those fields as live inputs there, while public
  eta-reduction helpers plus direct eta/standalone solver entry points remain unchanged
- landed `Batch 58g` on actual `main` adds target-aware Kira reduction-span widening on
  exact-sector positive-support matches plus the retained `tt` span-evidence packet, landed
  `Batch 58h` adds the same-path retained `tt` widening guard at `53a6630`, and landed
  `Batch 58i` adds retained `automatic_loop` `box1` / `box2` stage-1 and stage-2
  reduction-span/order evidence at `9b619f1`. Landed `main@7dee2a0` then extends the retained
  `tt`, `box1`, and `box2` seams to compare normalized `integralfamilies.yaml` and
  `kinematics.yaml` against capture. Together with landed `Batch 58d` prefactor lock coverage,
  this closes `Milestone M3` narrowly on the first mandatory package families only, while narrow
  opt-in Kira `insert_prefactors` wiring remains a separate reviewed path
- landed `Batch 58f` adds exact-subset tests plus mirrored docs and records the remaining direct
  precision-monotonicity evidence on the current reviewed exact subset:
  `tests/amflow_tests.cpp` includes
  `BootstrapSeriesSolverExactSubsetRequestedDigitsMonotonicityTest()` and
  `SolveDifferentialEquationExactSubsetRequestedDigitsMonotonicityTest()`, both driven by
  `ExpectRequestedDigitsMonotonicityOnReviewedExactSubset(...)` over the under-cap
  requested-digits ladder `{11, 73, 145, 290}` on the reviewed exact scalar, exact
  upper-triangular, mixed scalar, mixed upper-triangular diagonal, and mixed upper-triangular
  zero-forcing-resonance cases. This is under-cap diagnostic invariance on the direct exact
  `BootstrapSeriesSolver` subset plus one representative `SolveDifferentialEquation(...)`
  passthrough over the same reviewed cases. Combined with the existing hard-ceiling rejection
  coverage from
  `BootstrapSeriesSolverRejectsDigitsAboveConfiguredCeilingTest()` and
  `SolveDifferentialEquationInsufficientPrecisionPassthroughTest()`, this closes `Milestone M4`
  narrowly on that implemented exact subset only. This packet does not widen runtime behavior,
  broader cache/restart semantics, broader monotone digit refinement, or standalone solver-policy
  parity beyond the reviewed exact subset. The separate landed `Milestone M3` closure on landed
  `main@7dee2a0` remains limited to the first mandatory package families only, while narrow opt-in Kira
  `insert_prefactors` wiring is already present separately

## Test Taxonomy

- `T0`: normalization and I/O
- `T1`: structural self-consistency
- `T2`: solver self-consistency
- `T3`: upstream AMFlow parity
- `T4`: paper and case-study benchmark parity
- `T5`: robustness and failure handling

## Acceptance Defaults

- `>= 50` correct digits on core package families
- `>= 100` digits on `2024-tth-light-quark-loop-mi`
- `>= 200` digits on `2023-diphoton-heavy-quark-form-factors`
- precision monotonicity is mandatory
- unstable runs must fail explicitly with diagnostics

These floors are mirrored in
`tools/reference-harness/templates/qualification-benchmarks.json` so future M6 qualification lanes
can consume one machine-readable threshold scaffold instead of reverse-engineering this document
and `specs/parity-matrix.yaml`.

## Phase-0 Harness States

- `bootstrap-only`: the harness layout, manifest, pinned inputs, and placeholder benchmark metadata exist, but the required phase-0 benchmark set has not yet been retained as real Wolfram outputs
- `reference-captured`: the pinned upstream AMFlow environment has populated the required phase-0 benchmark paths with retained raw outputs, promoted goldens, result manifests, and comparison summaries

The bootstrap-only state is allowed for repository setup and interface work. It is not sufficient to claim `Phase 0` parity is passing.

## Current Canonical Cluster Gates

- light repo inspection can happen on the login node, but real builds, tests, reducer jobs,
  reference-harness jobs, and benchmark captures must run through fresh Slurm jobs
- canonical repo-wide acceptance now uses the retained configure/build/test triplet:
  - `cmake -S . -B build`
  - `cmake --build build --parallel 1`
  - `ctest --test-dir build --output-on-failure`
- current accepted clean-candidate build gate evidence is job `5305579` on `sapphire`
- latest accepted candidate-local smoke replay for the repo-local `K0-pre-spec` fixture freeze is
  job `5356840`
- latest accepted clean-candidate build/test confirmation for the `K0-pre` kinematics-YAML repair
  is job `5356948`
- latest accepted clean-candidate gate for the `K0b.1` packet is job `5425248`
- latest accepted coherent K0 reducer-smoke packet job is `5425379` on `sapphire`, retained at
  `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/k0/reducer-smoke`
- latest accepted clean-candidate gate for the narrow `Batch 47` / `Milestone M2` packet is job
  `5431987` for candidate
  `/n/holylabs/schwartz_lab/Lab/obarrera/autonomousIBP-artifacts/candidates/b47-clean-candidate-20260413T072226Z-LRVIIR`
- latest accepted clean-candidate gate for the landed `Batch 48` packet is job `5439311`; the
  landing commit is `f4bf8af2419a20f04ae40eceebbd5d12f3b2a92c`
- latest accepted clean-candidate gate for the landed `Batch 49` packet is job `5445260`; the
  landing commit is `b0275a8d8ce3f33577629f44d7b168b4d4ef8bb2`
- latest accepted clean-candidate gate for the landed `Batch 49b` packet is job `5457143` for candidate
  `/n/holylabs/schwartz_lab/Lab/obarrera/autonomousIBP-artifacts/candidates/b49b-final-clean-candidate-20260413T112519Z-Kabcrq`
- latest accepted clean-candidate gate for the landed `Batch 50a` packet is job `5465841` for
  candidate
  `/n/holylabs/schwartz_lab/Lab/obarrera/autonomousIBP-artifacts/candidates/b50a-final-clean-candidate-20260413T123155Z-1gWbow`;
  final status `COMPLETED` on `holy8a32607` in `00:00:32`, and the landing commit is
  `bbd7b744b69a413bf34e4b706cd737e2b266256a`
- current `main` additionally carries landed `Batch 50b` at commit
  `95f33f398bbdebf2084bf360a498fea3de89fc30`; local verification was rerun successfully in the
  current thread, and the latest recorded clean-candidate gate for that landed packet is job
  `5482487` for candidate
  `/n/holylabs/schwartz_lab/Lab/obarrera/autonomousIBP-artifacts/candidates/b50b-final-clean-candidate-20260413T133615Z-775743d3`
  under retained artifact root
  `/n/holylabs/schwartz_lab/Lab/obarrera/autonomousIBP-artifacts/jobs/b50b-final-clean-candidate-20260413T133615Z-775743d3`
- current accepted reference-harness bootstrap evidence for `M0a` is the combination of the
  shared Linux toolchain manifest, the phase-0 bootstrap root, the dependency-sanity packet, and
  the Wolfram smoke packet described in `docs/reference-harness.md`
- current accepted retained-capture evidence for `M0b` is the same pinned root at
  `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260419-required-set`,
  where `automatic_vs_manual` and `automatic_loop` both have promoted goldens, result manifests,
  and passed bundled-backup plus rerun reproducibility summaries
- the current ready-example optional capture evidence is the sibling root at
  `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260422-de-d0-pair`,
  where `differential_equation_solver` and `spacetime_dimension` each now have promoted goldens,
  result manifests, and passed bundled-backup plus rerun reproducibility summaries; because that
  packet omits the required pair, its manifest remains `bootstrap-only`
- the current user-hook optional capture evidence is the sibling root at
  `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260422-user-hook-pair`,
  where `user_defined_amfmode` and `user_defined_ending` each now have promoted goldens, result
  manifests, and passed bundled-backup plus rerun reproducibility summaries; because that packet
  also omits the required pair, its manifest remains `bootstrap-only`
- `M0a` remains the bootstrap precursor only; `M0b` now supplies the accepted
  `reference-captured` state for the required phase-0 benchmark set
- `K0` is now satisfied on the accepted narrow subset by that retained packet and honest bootstrap
  manifest; `Batch 47` / `Milestone M2`, landed `Batch 48`, landed `Batch 49`, landed `Batch 49b`,
  and landed `Batch 50a` are now also satisfied narrowly on the supported sample subset on the last
  fully accepted baseline, while landed `Batch 50b`, `Batch 50`, broader Kira smoke, broader
  reducer parity, and later parity milestones remain separate future gates

## Immediate Enforcement In This Bootstrap

- public-contract and default-option tests
- Kira config emission tests
- artifact manifest tests
- solved-path cache manifest and solved-path diagnostic replay/invalidation tests for the
  `AmfOptions` eta solve wrappers
- placeholder CLI smoke tests
- reference-harness manifest shape and pinned-input recording
- placeholder golden layout and benchmark index generation without Mathematica
- retained-golden promotion with canonicalized Mathematica output comparison and rerun reproducibility summaries
- verified AMFlow remote/origin matching before pinning an existing checkout
- clean CPC extraction on reruns so stale archive contents cannot survive
- path-safe benchmark catalog IDs and placeholder-only refresh semantics
- local `--self-check` coverage in the bootstrap, fetch, placeholder-freeze, and retained-capture
  helpers for the Batch-2/M0b regression cases, including qualification scaffold/catalog
  coherence against the frozen parity sources and repo-local `amflow-tests` wiring for those
  helper checks
- qualification-readiness validation across the retained `required-set`, `de-d0-pair`, and
  `user-hook-pair` phase-0 packet roots so the first M6 groundwork helper can audit which frozen
  example classes already have captured goldens without claiming a full qualification pass
- frozen `docs/release-signoff-checklist.md` planning scaffold so future
  release-sign-off packets have one checklist for candidate metadata,
  prerequisite gates, diagnostics/performance review, docs/parity review, and
  final dispositions without claiming that qualification or release is complete

## Batch-2 Reviewable Artifacts

- `specs/reference-harness-manifest.yaml` defines the manifest shape that the bootstrap writes as JSON.
- `tools/reference-harness/templates/phase0-benchmarks.json` freezes the initial phase-0 benchmark catalog.
- `tools/reference-harness/templates/phase0-golden.template.json` and `comparison-summary.template.json` define the placeholder and promoted comparison contracts.
- `tools/reference-harness/templates/qualification-benchmarks.json` freezes the current
  qualification scaffold: parity-matrix benchmark families, digit-threshold profiles, required
  failure codes, and known regression families.
- `tools/reference-harness/templates/release-signoff-checklist.json` freezes the first
  machine-readable release sign-off scaffold: the prerequisite M6 gate plus the later
  qualification-closure, performance, diagnostic, docs-completion, and parity-signoff review
  sections.
- `tools/reference-harness/scripts/score_phase0_correct_digits.py` is the first packet-level M6
  correct-digit scorer: it consumes one retained reference packet root plus one candidate packet
  root on the existing manifest/run schema, keeps the retained output-name set and nonnumeric
  canonical-text skeleton fixed, scores only approximate Mathematica numeric literals tokenwise
  against the frozen digit-threshold profiles, and leaves exact symbolic outputs structural-only.
- `tools/reference-harness/scripts/score_phase0_packet_set_correct_digits.py` is the first
  packet-set M6 correct-digit aggregator: it composes the reviewed packet-level scorer across the
  retained `required-set`, `de-d0-pair`, and `user-hook-pair` split, requires one unique
  reference packet label per pair, requires each candidate packet root to publish exactly the
  retained benchmark split for that packet, and keeps the scored benchmark ids synchronized with
  the scaffold's current `reference-captured` phase-0 set without overclaiming qualification
  closure.
- `tools/reference-harness/scripts/release_signoff_readiness.py` is the first executable M7
  helper: it consumes one `qualification_readiness.py` summary plus the release-signoff checklist,
  audits the checklist source/docs targets, and writes one blocked release-readiness summary that
  keeps the current runtime-lane frontier visible without claiming `Milestone M6` or `Milestone M7`
  closure.
- `tools/reference-harness/scripts/validate_qualification_scaffold.py` audits retained phase-0
  packet roots against that scaffold and reports benchmark-level readiness while keeping packet-
  level `bootstrap-only` versus `reference-captured` truthfulness explicit.
- `docs/implementation-ledger.md` tracks which implementation batches have been reviewed and what verification was run.

The full benchmark matrix is frozen in `specs/parity-matrix.yaml` and grows into a qualification
suite as the solver is implemented. The qualification scaffold is planning metadata only: it does
not by itself claim any new captured benchmark evidence or solver parity. The packet-level
correct-digit scorer and the packet-set correct-digit aggregator both remain narrower than
qualification closure as well: they score reviewed approximate numeric literals against retained
references, but they do not audit candidate failure-code behavior, do not compare case-study
numerics, and do not claim that `Milestone M6` is passing.

The release-signoff scaffold is planning metadata only as well: it does not claim qualification
closure, release readiness, or any broader parity surface beyond the evidence already recorded in
the retained artifacts and durable docs. The blocked release-readiness helper remains in that same
planning-only category: it audits prerequisites and keeps withheld claims explicit, but it does not
run performance review, diagnostic review, or parity sign-off.

The durable staged plan for building that qualification suite from the current reviewed bootstrap state is frozen in `docs/full-amflow-completion-roadmap.md`.
