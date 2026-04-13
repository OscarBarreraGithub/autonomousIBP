# Verification Strategy Bootstrap

The migration is phase-gated. Every phase must pass:

- self-consistency
- upstream AMFlow parity
- numerical robustness

## Current Durable Status

- authoritative `main` base is `f4bf8af2419a20f04ae40eceebbd5d12f3b2a92c`
- `Milestone M0a` is accepted as cluster/reference-harness bootstrap readiness only
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
- the current accepted staging packet is `Batch 49` (`Propagator` only): local module-loaded
  `cmake -S . -B build`, `cmake --build build --parallel 1`, and
  `ctest --test-dir build --output-on-failure` passed in `/tmp/autoIBP-b49-propagator`; clean-
  candidate `sapphire` job `5445260` passed for candidate
  `/n/holylabs/schwartz_lab/Lab/obarrera/autonomousIBP-artifacts/candidates/b49-clean-candidate-20260413T092450Z-c6f0e6`;
  and second-pass rereview found no blockers remaining
- that staging acceptance is limited to the bootstrap structural selector for builtin
  `Propagator` on the current reviewed subset and does not accept `Mass` selector semantics,
  `Propagator::prescription` metadata interpretation, broader automatic boundary
  execution/provider parity, broader ending semantics, broader Kira smoke, or upstream parity
- `Batch 49b` is now the next roadmap-owned implementation lane, while `M0b` remains separately
  open and still blocks broader parity claims

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

- `bootstrap-only`: the harness layout, manifest, pinned inputs, and placeholder benchmark metadata exist, but no real Wolfram reference outputs have been captured yet
- `reference-captured`: the pinned upstream AMFlow environment has populated the placeholder benchmark paths with real replacement rules, coefficient tables, and comparison summaries

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
- latest accepted clean-candidate gate for the current `Batch 49` / `Propagator`-only staging
  packet is job `5445260` for candidate
  `/n/holylabs/schwartz_lab/Lab/obarrera/autonomousIBP-artifacts/candidates/b49-clean-candidate-20260413T092450Z-c6f0e6`
- current accepted reference-harness bootstrap evidence for `M0a` is the combination of the
  shared Linux toolchain manifest, the phase-0 bootstrap root, the dependency-sanity packet, and
  the Wolfram smoke packet described in `docs/reference-harness.md`
- `M0a` remains bootstrap-only: placeholder goldens and pending comparisons are acceptable there,
  but they do not support any upstream parity claim
- `K0` is now satisfied on the accepted narrow subset by that retained packet and honest bootstrap
  manifest; `Batch 47` / `Milestone M2` and landed `Batch 48` are now also satisfied narrowly on
  the supported sample subset on `main`, and the current `Batch 49` / `Propagator`-only staging
  packet has also cleared the clean-candidate gate on top of clean
  `main@f4bf8af2419a20f04ae40eceebbd5d12f3b2a92c`, while broader Kira smoke, broader reducer
  parity, `M0b`, and later parity milestones remain separate future gates

## Immediate Enforcement In This Bootstrap

- public-contract and default-option tests
- Kira config emission tests
- artifact manifest tests
- placeholder CLI smoke tests
- reference-harness manifest shape and pinned-input recording
- placeholder golden layout and benchmark index generation without Mathematica
- verified AMFlow remote/origin matching before pinning an existing checkout
- clean CPC extraction on reruns so stale archive contents cannot survive
- path-safe benchmark catalog IDs and placeholder-only refresh semantics
- local `--self-check` coverage in the fetch and placeholder-freeze helpers for the Batch-2 regression cases

## Batch-2 Reviewable Artifacts

- `specs/reference-harness-manifest.yaml` defines the manifest shape that the bootstrap writes as JSON.
- `tools/reference-harness/templates/phase0-benchmarks.json` freezes the initial phase-0 benchmark catalog.
- `tools/reference-harness/templates/phase0-golden.template.json` and `comparison-summary.template.json` define the placeholder golden and comparison contracts.
- `docs/implementation-ledger.md` tracks which implementation batches have been reviewed and what verification was run.

The full benchmark matrix is frozen in `specs/parity-matrix.yaml` and grows into a qualification suite as the solver is implemented.

The durable staged plan for building that qualification suite from the current reviewed bootstrap state is frozen in `docs/full-amflow-completion-roadmap.md`.
