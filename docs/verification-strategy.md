# Verification Strategy Bootstrap

The migration is phase-gated. Every phase must pass:

- self-consistency
- upstream AMFlow parity
- numerical robustness

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
