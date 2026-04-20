# Verification Strategy Bootstrap

The migration is phase-gated. Every phase must pass:

- self-consistency
- upstream AMFlow parity
- numerical robustness

## Current Durable Status

- the starting `main` / `origin/main` head for this packet was `a5d627f906dfb2c5829bda88dce2407bfa67f043`
- last fully accepted release baseline remains `bbd7b744b69a413bf34e4b706cd737e2b266256a`, while reviewed code on `main` now extends beyond that baseline through landed `Batch 58`
- `Milestone M0a` is accepted as cluster/reference-harness bootstrap readiness only
- `Milestone M0b` is accepted on the required phase-0 benchmark set: retained root
  `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260419-required-set`,
  initial packet job `6721330` reached the completed `automatic_vs_manual` primary before
  walltime, resumed job `6732338` completed the packet via `--resume-existing`, and the manifest
  now records `phase0.capture_state = "reference-captured"` with both required benchmarks
  captured
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
- current worktree `Batch 58c` freezes prefactor reference evidence only around the existing
  helper surface: `specs/amflow-prefactor-reference.yaml` records retained phase-0 README backing
  for the `+i0` loop prefactor and cut prefactor, and records the explicit `-i0`
  loop-prefactor note as repo-snapshot backed only; retained `AMFlow.m` is cited there only for
  prescription polarity. Local
  `module load cmake/4.2.3-fasrc01 && cmake --build build --parallel 1` passed for this staging
  slice. First-family reduction-span parity evidence is still missing, so `Milestone M3`
  remains open; `Milestone M4` also remains open

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
- local `--self-check` coverage in the fetch, placeholder-freeze, and retained-capture helpers for the Batch-2/M0b regression cases

## Batch-2 Reviewable Artifacts

- `specs/reference-harness-manifest.yaml` defines the manifest shape that the bootstrap writes as JSON.
- `tools/reference-harness/templates/phase0-benchmarks.json` freezes the initial phase-0 benchmark catalog.
- `tools/reference-harness/templates/phase0-golden.template.json` and `comparison-summary.template.json` define the placeholder and promoted comparison contracts.
- `docs/implementation-ledger.md` tracks which implementation batches have been reviewed and what verification was run.

The full benchmark matrix is frozen in `specs/parity-matrix.yaml` and grows into a qualification suite as the solver is implemented.

The durable staged plan for building that qualification suite from the current reviewed bootstrap state is frozen in `docs/full-amflow-completion-roadmap.md`.
