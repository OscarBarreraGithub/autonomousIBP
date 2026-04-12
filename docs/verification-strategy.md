# Verification Strategy Bootstrap

The migration is phase-gated. Every phase must pass:

- self-consistency
- upstream AMFlow parity
- numerical robustness

## Current Durable Status

- authoritative `main` base is `fdbceea3cb94ee2e811573ad446e5777917c1bb0`
  (`Fix GNU 8 std::filesystem linkage`)
- `Milestone M0a` is accepted as cluster/reference-harness bootstrap readiness only
- `Operational Gate B0/G1` is accepted: clean-candidate `sapphire` job `5305579` passed
  `cmake -S . -B build`, `cmake --build build --parallel 1`, and
  `ctest --test-dir build --output-on-failure`
- `K0a` is accepted as a narrow parser-contract hardening milestone: clean-candidate `sapphire`
  job `5315267` passed the canonical build/test gate with the mixed-root regression coverage in
  `amflow-tests`
- `K0` is still pending; no accepted Kira reducer-smoke packet or honest bootstrap-manifest packet
  exists on `main`
- the next atomic engineering milestone is `K0b: Honest Bootstrap Manifest And Clean K0
  Acceptance Packet`

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
- current accepted reference-harness bootstrap evidence for `M0a` is the combination of the
  shared Linux toolchain manifest, the phase-0 bootstrap root, the dependency-sanity packet, and
  the Wolfram smoke packet described in `docs/reference-harness.md`
- `M0a` remains bootstrap-only: placeholder goldens and pending comparisons are acceptable there,
  but they do not support any upstream parity claim
- `K0` acceptance will additionally require one coherent retained reducer packet with an honest
  bootstrap manifest; the current `main` branch does not have that yet

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
