# Full AMFlow Completion Roadmap

This document is the durable execution plan from the reviewed bootstrap/runtime-hook state toward
full AMFlow parity. It is not a replacement for `docs/implementation-ledger.md`; it is the
long-range architecture, track scheduler, and batch queue that future turns should follow without
re-planning the whole port each time.

Use this roadmap together with:

- `docs/implementation-ledger.md`
- `docs/public-contract.md`
- `docs/parity-contract.md`
- `docs/verification-strategy.md`
- `specs/parity-matrix.yaml`
- `docs/orchestration-workflow.md`

## R0 Control Reset

The control model below supersedes the stale global rule “take the next unimplemented batch row.”
Batch numbers remain the local dependency labels, but authoritative scheduling is now by track and
exit gate. The current live state must always be read from `docs/implementation-ledger.md`.

### Build Vs Buy

| Area | In-House C++ | External Package / Tool |
| --- | --- | --- |
| public API, DE construction, solver, boundary logic, diagnostics, and runtime policy | yes | no |
| symbolic algebra backend | thin facade only | `GiNaC/CLN` |
| exact / high-precision numeric backend | thin facade only | `Boost.Multiprecision` + `MPFR` |
| YAML / manifest I/O | no | `yaml-cpp` |
| IBP reduction | no | `Kira 3.1` + `Fermat` |
| upstream parity oracle / reference capture | no | Wolfram/AMFlow harness only |

### Authoritative Tracks

| Track | Purpose | Current mapping |
| --- | --- | --- |
| `Track P: Production Backend` | choose, enable, and benchmark the production symbolic/exact/precision stack | backend decision, Linux build profile, perf/smoke evidence |
| `Track S: Solver Risk Retirement` | retire the highest-risk solver-core work early | reviewed `Batch 34` through `Batch 43`; `Milestone M1` complete |
| `Track B: Boundary And Parity Prep` | prepare boundary-provider and parity fixtures/contracts without widening live auto-boundary behavior early | reviewed `Batch 44` through `Batch 47`; `Milestone M2` accepted narrowly; `Batch 48` accepted on `main` as the bootstrap-only `Prescription` alias seam; `Batch 49` accepted on `main` as the narrow `Propagator` structural-selector seam; `Batch 49b` accepted on `main` as the narrow `Mass` selector seam; `Batch 50a` accepted on `main` as the internal eta-topology preflight snapshot seam; landed `Batch 50b` now has refreshed clean-candidate evidence recorded as the internal topology-prerequisite bridge packet; landed `Batch 50` implements the first supported Branch/Loop selector slice on the single-top-sector squared-linear-momentum subset and carries retained clean-candidate evidence via job `6265957`; landed `Batch 51` adds ordered multi-invariant invariant-generated list wrappers over the reviewed one-invariant path; landed `Batch 52` widens `BuildInvariantDerivativeSeed(...)` to the first invariant-independent nonzero-mass slice; landed `Batch 53` adds the first reviewed multiple-top-sector Kira target orchestration slice |
| `Track K: Kira/Fermat Cluster` | provision the real Linux reducer/runtime lane | `M0a` accepted, `B0/G1` accepted, and `K0-pre-spec` / `K0-pre` / `K0b.1` accepted on the narrow repo-local smoke subset; `K0` is closed on that subset |
| `Track R: Continuous Reference Capture` | make `M0` an always-on evidence lane instead of a late one-off | `M0a` accepted bootstrap/pinning, then rolling `M0b` golden capture |

### Exit Gates

| Gate | Exit Artifacts |
| --- | --- |
| `R0` | this track scheduler recorded in the roadmap; ledger note superseding the stale next-row rule; explicit owner/interface table for `P/S/B/K/R` |
| `P0` | backend decision record; enabled Linux build profile with the chosen stack; benchmark/smoke report on that stack |
| `K0` | pinned Linux/toolchain manifest with executable paths and versions; provisioning/runbook; one retained coherent reducer-smoke run with deterministic artifacts and an honest bootstrap manifest |
| `M0a` | cluster harness root, pinned manifests for AMFlow/CPC/Kira/Fermat/Wolfram inputs, frozen placeholder/golden layout, dependency-sanity report, and Wolfram smoke evidence |
| `S0` | reviewed `Batch 35`; reviewed `Batch 36-39`; residual and overlap checks live; library solver path no longer scaffolded on the supported nonsingular subset |
| `M0b` | real captured golden outputs and comparison summaries for the first benchmark set plus rerun reproducibility evidence |

### Cross-Track Blocking Rules

- `Track P`, `Track K`, and `Track R` run in parallel with `Track S` as long as they consume only
  stable public/runtime boundaries.
- `Track P` may benchmark or enable libraries in parallel with solver work, but it must not churn
  public solver semantics while `S0` is open.
- `Track K` is Linux-cluster-first; it is not a local macOS side path.
- `Track R` is continuous. `M0a` permits setup and harness/bootstrap work only; it does not justify
  any parity claim.
- `Track B` may prep contracts, fixtures, and acceptance plans in parallel now, but it must not
  ship live automatic-boundary semantics until `S0` is closed.
- Incomplete `M0b` blocks `T3/T4` parity-pass claims, any “matches upstream reference” claim
  outside the captured subset, and any `M3/M5/M6` or release sign-off.

## Current Durable Status

- the actual SSH remote `main` head for this packet is `7dee2a0a574f2df991edf917290cc4600c9ae215`; local tracking `origin/main` matches at `7dee2a0a574f2df991edf917290cc4600c9ae215`. That landed head carries the narrow `Milestone M3` closure packet at `7dee2a0`, on top of `Batch 58h` at `53a6630` and `Batch 58i` at `9b619f1`, the earlier landed Kira rational-function prefactor surface at `ab4a311`, the landed xints `insert_prefactors` wiring at `b367daf`, landed `Batch 58g` at `53ec6a4`, landed `Batch 58f` at `7d3806a`, landed `Batch 58e` at `2f2538b`, landed `Batch 58` at `a5d627f906dfb2c5829bda88dce2407bfa67f043`, landed `Batch 56` through `Batch 57` at `56e4f96d03b0b54f541122c0d59b2ed0cefc2b98` and `48686b6590df1f1c52f760913129f1bf0ad3ad0b`, plus landed `Batch 50` through `Batch 55` at `b40b0dccb1d286b287e2fcb45e5e554901223d63`, `08220d2569d1a60c9181f53d5e809f334dcfcd4e`, `95c2ebf6f7f7adb713c04625d9fccd3c1266eeb8`, `0f623d65e7e933d464deef3da4ea02efaf57a535`, `23b64404680fe0c5425d2261f6e776bd1f197794`, and `4dcb17f6a4fd9d2ebf28e72922e74c06fb461d82`; landed-history references below key to that upstream head, and the retained family-model parity locks are now part of landed `main`
- last fully accepted release baseline remains `bbd7b744b69a413bf34e4b706cd737e2b266256a`; reviewed implementation on actual `main` now extends beyond that baseline through landed `Batch 58h` / `Batch 58i` plus the landed narrow `Milestone M3` closure packet at `7dee2a0`, on top of the earlier Kira rational-function prefactor surface and xints `insert_prefactors` wiring, while `Milestone M1` remains complete only on the accepted reviewed surface
- `Milestone M0a` is accepted as cluster/reference-harness bootstrap readiness only
- `Milestone M0b` is accepted on the required phase-0 benchmark set only: retained root
  `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260419-required-set`,
  initial packet job `6721330` completed the `automatic_vs_manual` primary before walltime,
  resumed packet job `6732338` completed the packet via `--resume-existing`, and both required
  benchmark comparison summaries now pass
- `Operational Gate B0/G1` is accepted: GNU 8 `std::filesystem` linkage is restored and the clean
  `sapphire` canonical configure/build/test packet passed at job `5305579`
- `K0-pre-spec` is accepted as a repo-local K0 smoke fixture freeze derived from preserved input;
  latest candidate-local smoke replay job `5356840` passed
- `K0-pre` is accepted as the narrow Kira kinematics YAML contract repair for that frozen smoke
  subset; latest clean-candidate build/test job `5356948` passed
- `K0b.1` is accepted on that frozen repo-local smoke subset: clean-candidate job `5425248`
  passed, packet job `5425379` passed on `sapphire`, and the retained root
  `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/k0/reducer-smoke` is coherent and complete
- `Gate K0` is now closed only for that frozen repo-local smoke subset: one coherent retained
  reducer-smoke packet with an honest bootstrap manifest is accepted on `main`
- `Batch 47` / `Milestone M2` are now accepted narrowly after that `K0` / `K0b` acceptance: on
  the supported simple Euclidean massless sample subset only, builtin `Tradition` plus one exact
  user-defined singleton `<family>::eta->infinity` path agree between manual and automatic
  attachment, and the reviewed pre-solve failures remain preserved with solver non-invocation
- `Batch 48` is accepted on `main` as the bootstrap-only builtin eta-mode `Prescription` planner
  seam on the current supported subset; final accepted clean-candidate `sapphire` job `5439311`
  cleared the landing packet and commit `f4bf8af2419a20f04ae40eceebbd5d12f3b2a92c` is the clean
  release baseline
- `Batch 49` is accepted on `main`: commit `b0275a8d8ce3f33577629f44d7b168b4d4ef8bb2` landed the
  narrow builtin eta-mode `Propagator` structural-selector packet
- `Batch 49b` is accepted on `main` on top of that clean baseline: local module-loaded
  configure/build/ctest passed in `/tmp/autoIBP-b49b-mass`, final clean-candidate `sapphire` job
  `5457143` passed for candidate
  `/n/holylabs/schwartz_lab/Lab/obarrera/autonomousIBP-artifacts/candidates/b49b-final-clean-candidate-20260413T112519Z-Kabcrq`,
  and second-pass rereview cleared with no blocking or medium findings remaining
- `Batch 50a` is accepted on `main` on top of that baseline: local module-loaded
  configure/build/ctest passed, final clean-candidate `sapphire` job `5465841` passed for
  candidate
  `/n/holylabs/schwartz_lab/Lab/obarrera/autonomousIBP-artifacts/candidates/b50a-final-clean-candidate-20260413T123155Z-1gWbow`,
  second-pass rereview cleared with no findings, and landing commit
  `bbd7b744b69a413bf34e4b706cd737e2b266256a` is the accepted release baseline
- this does not widen the public/runtime surface beyond reviewed `Batch 46`, does not accept
  broader automatic-boundary execution/provider parity, does not accept full upstream
  topology/component-order `Mass` semantics, broader symbolic mass canonicalization, or
  `Propagator::prescription` / `feynman_prescription` parity, and does not relax the separate
  `M0b` blocker on broader parity claims
- `Batch 50a` is accepted on `main` as the internal eta-topology preflight snapshot seam for
  `Branch` / `Loop`, with no truthful selector semantics yet
- landed `Batch 50b` on current `main` is a narrower internal topology-prerequisite
  bridge/prereq snapshot over the same family/kinematics surface, with explicit
  available-versus-missing field reporting while `Branch` and `Loop` remain blocked
- current-thread local verification reran `cmake -S . -B build`, `cmake --build build --parallel 1`,
  and `ctest --test-dir build --output-on-failure` successfully against that landed `Batch 50b`
  surface, and secondary review found no material findings on the declared narrow scope
- clean-candidate attempt `5480669` remains only an OOM-killed operational false start, but the
  later clean-candidate job `5482487` passed for candidate
  `/n/holylabs/schwartz_lab/Lab/obarrera/autonomousIBP-artifacts/candidates/b50b-final-clean-candidate-20260413T133615Z-775743d3`
  under retained artifact root
  `/n/holylabs/schwartz_lab/Lab/obarrera/autonomousIBP-artifacts/jobs/b50b-final-clean-candidate-20260413T133615Z-775743d3`,
  and that evidence is now recorded in the durable docs
- landed `Batch 50` on current `main` now implements the first supported `Branch` / `Loop`
  selector slice on the single-top-sector squared-linear-momentum subset; the retained
  clean-candidate packet remains the one recorded at job `6265957`, and broader topology/component
  parity remains deferred
- landed `Batch 51` on current `main` now adds ordered multi-invariant
  `BuildInvariantGeneratedDESystemList(...)` and `SolveInvariantGeneratedSeriesList(...)` wrappers
  over the reviewed one-invariant automatic path
- landed `Batch 52` on current `main` now widens `BuildInvariantDerivativeSeed(...)` to accept
  invariant-independent identifier or rational-constant propagator masses on the bootstrap subset
- landed `Batch 53` on current `main` now adds the first multiple-top-sector Kira target
  orchestration slice over the accepted preparation/execution seams
- landed `Batch 54` on current `main` now adds precision-budget preflight plus an internal retry
  controller on generated-solver handoffs; direct `SolveDifferentialEquation(...)` remains
  passthrough by design
- landed `Batch 55` now hardens typed diagnostics on top of that landed surface:
  DE-construction master-basis drift in the generated wrappers returns
  `failure_code = "master_set_instability"`, exhausted monotone retry progress returns
  `failure_code = "continuation_budget_exhausted"`, and local
  `cmake -S . -B build`, `cmake --build build --parallel 1`, `ctest --test-dir build --output-on-failure`,
  and `./build/amflow-tests` all passed before landing commit `4dcb17f6a4fd9d2ebf28e72922e74c06fb461d82`
- truthful `Milestone M3` closure review was reconsidered after `M0b`; the repo now has a first
  explicit in-repo prefactor/sign-convention surface and tests, and actual `main` through
  `7dee2a0` now carries landed `Batch 58d` ROLE B coverage over the locked prefactor evidence
  packet plus the earlier landed Kira rational-function prefactor surface and narrow xints
  `insert_prefactors` wiring. Landed `Batch 58g` supplies the retained `tt`
  mandatory-family reduction-span packet on the same generic Kira span path, landed `Batch 58h`
  guards the same-path retained `tt` widening lane, landed `Batch 58i` supplies retained
  `automatic_loop` `box1` / `box2` stage-1 and stage-2 reduction-span/order evidence, and landed
  `7dee2a0` adds retained family-model parity locks on those same first mandatory-package
  seams in `tests/amflow_tests.cpp`:
  `tests/amflow_tests.cpp`
  cross-checks `references/snapshots/amflow/prefactor_convention_lock.md` against
  `specs/amflow-prefactor-reference.yaml`, while the retained phase-0 README still backs the
  `+i0` loop prefactor and cut prefactor, the explicit `-i0` loop-prefactor note remains
  repo-snapshot backed only, and retained `AMFlow.m` is cited only for prescription polarity.
  Landed `main@7dee2a0` now compares normalized `integralfamilies.yaml` and `kinematics.yaml`
  against the retained `automatic_vs_manual` `tt` and retained `automatic_loop` `box1` / `box2`
  stage-1 and stage-2 captures. That closes `Milestone M3` narrowly on the first mandatory
  package families only; narrow opt-in Kira `insert_prefactors` wiring still exists separately
- truthful `Milestone M4` closure was reconsidered after landed `Batch 58f`, and landed
  `Batch 58f` now records the remaining direct precision-monotonicity gap as closed on the
  current reviewed exact subset only
- the current `M4`-closing landed tests/docs lane is therefore `Batch 58f`:
  `tests/amflow_tests.cpp` already includes
  `BootstrapSeriesSolverExactSubsetRequestedDigitsMonotonicityTest()` and
  `SolveDifferentialEquationExactSubsetRequestedDigitsMonotonicityTest()`, both driven by
  `ExpectRequestedDigitsMonotonicityOnReviewedExactSubset(...)` over the under-cap
  requested-digits ladder `{11, 73, 145, 290}` on the reviewed exact scalar, exact
  upper-triangular, mixed scalar, mixed upper-triangular diagonal, and mixed upper-triangular
  zero-forcing-resonance cases. This is under-cap diagnostic invariance on the direct exact
  `BootstrapSeriesSolver` subset plus one representative `SolveDifferentialEquation(...)`
  passthrough over the same reviewed cases. Because that implemented exact path ignores
  precision fields except for hard-ceiling rejection, this evidence is limited to under-cap
  diagnostic invariance plus the existing hard-ceiling threshold failure already covered by
  `BootstrapSeriesSolverRejectsDigitsAboveConfiguredCeilingTest()` and
  `SolveDifferentialEquationInsufficientPrecisionPassthroughTest()`. Together with landed
  `Batch 54` through `Batch 58e` explicit failure-code, cache,
  `UseCache`, and `SkipReduction` coverage, that closes `Milestone M4` narrowly on the
  implemented exact subset only. This packet is still narrow: it does not widen runtime behavior, broader cache/restart semantics, broader
  monotone digit refinement, standalone `SolveDifferentialEquation(...)` / solver-policy parity
  beyond that reviewed exact subset, or the separate landed `Milestone M3` closure on landed
  `main@7dee2a0`, which is itself limited to the first mandatory package families rather than broader parity
  widening; narrow opt-in Kira `insert_prefactors` wiring is already present separately

## Current State At R0

### Implemented And Reviewed

- file-backed `ProblemSpec` loading, validation, deterministic YAML emission, and bootstrap CLI
  entrypoints
- reference-harness bootstrap, pinned manifesting, placeholder benchmark catalog layout, safe
  extraction/pinning helpers, and the accepted `M0a` cluster bootstrap packet for pinned
  Linux/Kira/Fermat/Wolfram inputs
- explicit Kira preparation, execution, parsing, and typed DE assembly from reduced rows
- eta insertion plus eta-generated reduction preparation, execution, parsing, `DESystem`
  consumption, and solver handoff wrappers
- invariant derivative generation, one-invariant automatic seed construction on a bootstrap subset,
  invariant-generated reduction preparation/execution, `DESystem` consumption, and solver handoff
  wrappers, plus landed ordered multi-invariant `DESystem` / solver list wrappers that iterate
  those one-invariant seams without widening solver behavior, and landed
  invariant-independent identifier/rational-constant mass support in
  `BuildInvariantDerivativeSeed(...)`
- builtin eta-mode planning wrappers through caller-supplied names and `AmfOptions::amf_modes`
- mixed builtin/user-defined eta-mode solver wrappers through single-name, ordered-list, and
  `AmfOptions` entrypoints
- builtin-or-user-defined eta-mode resolution through `ResolveEtaMode(...)`
- builtin-or-user-defined ending-scheme resolution plus single-name, ordered-list, and
  `AmfOptions` ending planners returning typed `EndingDecision`
- typed manual-boundary request/attachment surfaces
- exact rational coefficient evaluation over reviewed `DESystem` coefficient strings
- reviewed finite singular-point detection/classification, regular-point patching, Frobenius
  patching, exact overlap/residual diagnostics, exact one-hop continuation, and the standalone
  default-solver wrapper through `Batch 40`
- reviewed boundary-provider and builtin/planned `eta -> infinity` boundary-request seams through
  `Batch 46`
- accepted `B0/G1` build-gate repair on `main`, restoring GNU 8 `std::filesystem` linkage for the
  canonical cluster build/test path

### Still Missing Or Still Bootstrap-Only

- builtin eta-mode planning beyond reviewed `All` is still bootstrap-limited: landed
  `Prescription` is only an alias over `All`, landed `Propagator` is only the reviewed structural
  selector on `main`, and `Batch 49b` adds only a narrow local `Mass` seam with a token-based
  independence heuristic plus selected-propagator outer-whitespace trimming on rewritten mass
  literals. Landed `Batch 50a` adds only internal eta-topology preflight blocker telemetry for
  `Branch` / `Loop`, and the landed `Batch 50b` commit on current `main` adds only an internal
  topology-prerequisite bridge/prereq snapshot with explicit availability and missing-field
  reporting. Landed `Batch 50` adds a first supported selector slice over the single-top-sector
  squared-linear-momentum subset, but broader gaps remain explicit: no multi-top-sector support,
  no broader propagator grammar, no linear-propagator path, no full upstream `AnalyzeTopSector` /
  component-factorization / vacuum-fallback parity, and no broader prescription interpretation
- accepted manual-vs-automatic boundary equivalence is still narrow: only the supported simple
  Euclidean massless sample subset is covered, and only for builtin `Tradition` plus one exact
  user-defined singleton `<family>::eta->infinity`
- solver/provider coupling, automatic boundary execution, and `BoundaryCondition` generation from
  builtin/planned `eta -> infinity` requests remain deferred
- `BuildInvariantDerivativeSeed(...)` is still limited to a narrow bootstrap symbolic subset: the
  landed `Batch 52` packet widens only the mass field to invariant-independent identifiers or
  rational constants, while broader symbolic mass grammar and other symbolic widening remain
  missing
- multi-invariant orchestration on `main` remains narrow: landed `Batch 51` adds only the first
  ordered invariant-list wrappers over the reviewed one-invariant path, while broader list-surface
  widening and the loop-core reduction-span parity gate remain missing
- a narrow internal retry controller exists on generated-solver handoffs, and the current
  `Batch 57` worktree slice now adds solved-path cache replay plus wrapper-owned
  `amf_options.skip_reduction == true` reuse on the two `AmfOptions` eta solve wrappers only;
  broader cache/replay or interruption-resume coverage, public/helper `SkipReduction` widening,
  and later runtime-policy rows remain open
- feature parity is still missing for complex kinematics, arbitrary `D0`, fixed-`eps`, linear
  propagators, phase-space integration, standalone DE solving at parity quality, and full
  user-defined eta/ending execution paths
- the reference harness is now `reference-captured` on the required phase-0 benchmark set:
  `automatic_vs_manual` and `automatic_loop` both have promoted goldens, result manifests, and
  passed bundled-backup plus rerun reproducibility summaries under the retained root
  `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260419-required-set`

## Missing Workstreams Grouped Into Major Tracks

### Track A: Runtime Hook Completion

Goal:
- finish the runtime-extension surfaces so eta-mode and ending-scheme selection are no longer
  builtin-only plumbing gaps

Status:
- completed in reviewed `Batch 25` through `Batch 31`

Primary output:
- stable user-extension boundaries before the numerical engine and boundary system depend on them

### Track B: Flow-Construction Parity

Goal:
- widen the current DE-construction and reducer-preparation surface until loop-core parity is
  credible

Missing:
- builtin eta-mode planning semantics beyond `All`
- broader symbolic invariant derivative construction beyond the current bootstrap subset
- multi-invariant orchestration
- multiple top-sector target orchestration
- loop-core reduction-span and prefactor parity gates

Primary output:
- phase-1 parity evidence that the port is constructing the right DE systems and reduction spans
  on the mandatory loop-core families

### Track C: Boundary And Ending System

Goal:
- turn ending decisions and `eta -> infinity` theory into typed boundary requests and explicit
  boundary data

Missing:
- provider lookup/execution and solver coupling for automatic boundary generation on the supported
  subset
- `BoundaryCondition` generation and attachment from builtin/planned `eta -> infinity` requests
- broader user-defined ending integration beyond the reviewed singleton
  `<family>::eta->infinity` path
- manual-vs-automatic boundary equivalence evidence for the supported subset (`Batch 47`)

Primary output:
- explicit, testable boundary tasks instead of placeholder terminal-node lists

### Track D: Local Solver Kernel

Goal:
- replace the solver scaffold with a high-precision local-series engine

Missing:
- singular-start boundary semantics
- singular-to-regular and singular-to-singular continuation paths
- fractional-exponent mixed continuation
- public Frobenius diagnostics
- multi-hop stepping and transported-target output
- broader matrix/kinematic coverage beyond the current reviewed exact one-hop subset

Primary output:
- a real `SeriesSolver` implementation with T2 self-consistency rather than a scaffold

### Track E: Precision, Diagnostics, And Runtime Policy

Goal:
- make solver behavior reliable enough for parity claims and user-facing options

Missing:
- standalone `SolveDifferentialEquation(...)` integration is still passthrough while a narrow
  internal retry controller exists on generated-solver handoffs at
  `src/solver/series_solver.cpp:1904-1928`
- cancellation/truncation detection
- remaining required failure codes from `specs/parity-matrix.yaml`
- broader cache/restart artifact coverage beyond the current solved-path `UseCache` slice
- broader `SkipReduction` semantics beyond the wrapper-only reuse slice
- broader standalone DE-solver/runtime-policy parity beyond the current wrapper-owned
  `AmfOptions` eta-wrapper wiring for `WorkingPre`, `ChopPre`, `XOrder`, `ExtraXOrder`,
  `LearnXOrder`, `TestXOrder`, `RationalizePre`, and `RunLength`

Primary output:
- explicit stability management and honest runtime policy rather than stored-but-ignored options

Current status:
- landed `Batch 54` contributes precision-budget preflight plus a narrow internal retry controller
  on generated-solver handoffs: `EvaluatePrecisionBudget(...)` treats
  `max_working_precision` as a hard cap, the exact standalone solver returns
  `failure_code = "insufficient_precision"` when `requested_digits` exceed that configured
  ceiling, and `SolveInvariantGeneratedSeries(...)` / `SolveEtaGeneratedSeries(...)` route solver
  calls through `SolveWithPrecisionRetry(...)`
- landed `Batch 55` adds the first typed diagnostics on top of that retry surface:
  generated-wrapper master-basis drift is classified as `master_set_instability`, exhausted
  monotone retry progress is classified as `continuation_budget_exhausted`, direct
  `SolveDifferentialEquation(...)` remains passthrough, and cancellation/truncation detection,
  broader cache/restart, `SkipReduction`, and the rest of Track E remain open
- landed `Batch 58` keeps that narrow solved-path cache manifest plus the wrapper-owned
  `amf_options.skip_reduction == true` reuse on the two
  `SolveAmfOptionsEtaModeSeries(...)` overloads and wires the listed `AmfOptions` runtime fields
  into a live wrapper-owned solve policy: after wrapper-owned eta-mode selection,
  `WorkingPre`, `ChopPre`, `XOrder`, and `RationalizePre` rebuild the live `PrecisionPolicy`,
  while `ExtraXOrder`, `LearnXOrder`, `TestXOrder`, and `RunLength` are carried through a narrow
  typed request-side runtime-policy carrier. Matching `UseCache` requests and
  `skip_reduction` replay validation now treat those fields as live inputs through the same
  deterministic solved-path input/request fingerprints. The current bootstrap solver still does
  not implement full asymptotic learning, extra-large-eta expansion, or multi-step continuation
  behavior from those fields; on the reviewed subset they are carried and fingerprinted rather
  than given broader standalone semantics. Direct `RunEtaGeneratedReduction(...)`,
  `BuildEtaGeneratedDESystem(...)`, `SolveEtaGeneratedSeries(...)`, invariant-generated wrappers,
  and direct `SolveDifferentialEquation(...)` remain unchanged
- landed `Batch 58g` on actual `main` widens Kira preparation `r` spans on exact-sector
  positive-support matches and records the retained `tt` span-evidence packet, landed
  `Batch 58h` guards the same-path retained `tt` widening lane, and landed `Batch 58i` adds
  retained `automatic_loop` `box1` / `box2` stage-1 and stage-2 reduction-span/order evidence.
  Landed `main@7dee2a0` then extends the retained `tt`, `box1`, and `box2` seams to compare
  normalized `integralfamilies.yaml` and `kinematics.yaml` against capture. Together with landed
  `Batch 58d` prefactor lock coverage, that closes `Milestone M3` narrowly on the first
  mandatory package families only; Kira `insert_prefactors` now has a separate narrow opt-in
  reviewed wiring
- landed `Batch 58f` adds exact-subset tests plus mirrored docs and records the remaining direct
  precision-monotonicity gap as closed on
  the current reviewed exact subset only: `tests/amflow_tests.cpp` already carries
  `BootstrapSeriesSolverExactSubsetRequestedDigitsMonotonicityTest()` and
  `SolveDifferentialEquationExactSubsetRequestedDigitsMonotonicityTest()` for under-cap
  diagnostic invariance on the direct exact `BootstrapSeriesSolver` subset plus one
  representative `SolveDifferentialEquation(...)` passthrough, while
  `BootstrapSeriesSolverRejectsDigitsAboveConfiguredCeilingTest()` and
  `SolveDifferentialEquationInsufficientPrecisionPassthroughTest()` keep the hard-ceiling
  threshold behavior explicit. This is enough to close `Milestone M4` narrowly on that
  implemented exact subset, but it does not widen runtime behavior, broader cache/restart
  semantics, broader monotone digit refinement, or standalone
  `SolveDifferentialEquation(...)` runtime-policy parity. The separate landed `Milestone M3`
  closure on landed `main@7dee2a0` remains limited to the first mandatory package families only

### Track F: Feature-Surface Parity

Goal:
- close the remaining public runtime gaps frozen by `docs/parity-contract.md`

Missing:
- complex kinematics
- singular-kinematics guardrails and first physical-region subset
- arbitrary `D0`
- fixed-`eps` mode
- phase-space integration
- `feynman_prescription` parity
- linear-propagator parity
- standalone DE-solver parity example
- full user-defined eta-mode and ending execution paths

Primary output:
- support for the frozen example classes and feature list, not just the bootstrap loop core

### Track G: Verification And Qualification

Goal:
- upgrade from structural bootstrap coverage to genuine parity evidence

Missing:
- reference-captured AMFlow goldens
- example-class parity runs
- qualification-benchmark coverage
- known upstream regression families carried forward into deterministic tests
- performance and diagnostics review for release readiness

Primary output:
- a release-quality parity claim with evidence instead of interface plausibility

## Recommended Phase Ordering With Dependencies

### Phase A: Runtime Hook Completion

Dependencies:
- reviewed `Batch 24`

Delivers:
- stable eta-mode and ending-scheme selection surfaces
- stable user-defined hook boundaries before the solver and boundary runtime widen

Reason for order:
- these are small, low-risk seams and should freeze first

### Milestones M0a And M0b: Continuous Reference Capture

Dependencies:
- existing phase-0 harness state
- no production-runtime widening beyond the reviewed bootstrap seams is required to capture pinned
  upstream goldens

Delivers:
- `M0a`: a reference-ready cluster lane with pinned manifests, placeholder/golden layout, and
  dependency sanity checks
- `M0b`: retained real captured evidence for the required phase-0 benchmark set first, then the
  same rolling harness/tools widened to the remaining frozen example classes, benchmark families,
  and upstream regression surfaces before later phases claim "matches reference"

Reason for order:
- parity gates that mention upstream reference behavior must not outrun the reference harness

### Phase B: Solver MVP

Dependencies:
- Phase A for stable eta-mode and ending hooks
- explicit manual boundary input surfaces, not full automatic boundary generation

Delivers:
- typed manual boundary requests and attachment rules
- numeric coefficient evaluation over `DESystem`
- regular-point and regular-singular local-series generation
- overlap checks, residual checks, and a live standalone DE solver
- replacement of the `BootstrapSeriesSolver` scaffold on a supported subset

Reason for order:
- the standalone solver core does not need full AMFlow-specific DE-construction widening before it
  becomes real

### Phase C: Boundary And Ending Infrastructure

Dependencies:
- Phase A for ending hooks
- Phase B for a real solver core that can consume explicit terminal data

Delivers:
- boundary providers and `eta -> infinity` boundary generators
- ending-planned boundary selection
- explicit `boundary_unsolved` failures when boundary planning or provider lookup cannot supply
  valid terminal data
- manual-vs-automatic boundary equivalence on simple supported families

Reason for order:
- automatic boundary work is meaningful only once the solver can already consume explicit
  boundary inputs

### Phase D: Flow-Construction Parity

Dependencies:
- Phase A for runtime-hook stability
- Phase B and Phase C so loop-core widening lands on a live solver and boundary stack

Delivers:
- fuller builtin eta-mode planning semantics
- broader invariant generation, multi-invariant orchestration, and multi-top-sector coverage
- loop-core DE/reduction construction parity on the first mandatory package families

Reason for order:
- the AMFlow-specific flow-construction widening should land on top of a credible solver/runtime
  core rather than blocking that core from existing

### Phase E: Numerical Robustness And Runtime Policy

Dependencies:
- Phase B for a real solver
- Phase C and Phase D for live boundary and flow-construction paths

Delivers:
- precision escalation, typed instability failures, cache/restart semantics, `UseCache`,
  `SkipReduction`, and live option semantics

Reason for order:
- most runtime policy controls only become meaningful once the solver and boundary core are real

### Phase F: Full Feature-Surface Parity

Dependencies:
- Phase B for a working solver
- Phase C for boundary generation
- Phase D for AMFlow-specific flow-construction parity
- Phase E for reliable runtime policy

Delivers:
- arbitrary `D0`, fixed-`eps`, complex kinematics, singular-kinematics guardrails, phase-space,
  linear-propagator support, and full user-extension execution on the live solver path

Reason for order:
- these feature-surface requirements are broad and should land only after the numerical and
  runtime foundations are credible

### Phase G: Qualification And Release

Dependencies:
- `M0b`
- Phase A through Phase F

Delivers:
- qualification-corpus pass over captured goldens and benchmark families
- release sign-off with diagnostics, performance, and parity evidence reviewed

First groundwork:
- `docs/release-signoff-checklist.md` freezes the first `Milestone M7`
  evidence buckets, reviewer roles, and mandatory sign-off questions without
  claiming that `Milestone M6` or release readiness has been achieved

Reason for order:
- qualification and release claims belong after implementation, capture, and verification all
  exist in stable form

## Proposed Sequence Of Atomic Batches Or Milestones From Batch 25 Onward

The queue below is now a dependency index and local work queue, not the global scheduler. Future
turns should schedule by `Track P/S/B/K/R` and the exit gates above. Within an active track, the
next unreviewed dependency row remains the default local move unless the ledger records a justified
dependency change.

### Phase A: Runtime Hook Completion

| Batch | Scope | Depends On |
| --- | --- | --- |
| `Batch 25` | single-name library-only solver wrapper that resolves one eta-mode name against builtin plus user-defined registrations and then reuses `SolveEtaModePlannedSeries(...)` | `Batch 24` |
| `Batch 26` | ordered mixed builtin-plus-user-defined eta-mode-list solver wrapper over `ResolveEtaMode(...)` and the reviewed single-name wrapper | `Batch 25` |
| `Batch 27` | `AmfOptions`-fed mixed eta-mode solver wrapper that consumes `amf_options.amf_modes` through the reviewed mixed list-selection path | `Batch 26` |
| `Batch 28` | one-name builtin-or-user-defined ending-scheme resolver seam over the existing `EndingScheme` interface | `Batch 24` |
| `Batch 29` | single-name ending-scheme planning wrapper that resolves one ending name and returns a typed `EndingDecision` without solver coupling | `Batch 28` |
| `Batch 30` | ordered ending-scheme list-selection wrapper that probes ending planners in caller order and preserves deterministic fallback diagnostics | `Batch 29` |
| `Batch 31` | `AmfOptions`-fed ending-scheme planner wrapper that consumes `amf_options.ending_schemes` only | `Batch 30` |

### Milestones M0a And M0b: Continuous Reference Capture

| Batch | Scope | Depends On |
| --- | --- | --- |
| `Milestone M0a` | cluster-first reference-harness bootstrap: pinned Linux manifests, placeholder/golden layout, dependency sanity checks, Wolfram smoke evidence, and stable artifact paths for AMFlow/CPC/Kira/Fermat/Wolfram inputs | existing Batch-2 harness state |
| `Milestone M0b` | retained real-golden capture and rerun reproducibility evidence for the required phase-0 benchmark set, with the rolling harness lane/tools in place for the remaining frozen example classes and later benchmark/regression families | `Milestone M0a` |

### Phase B: Solver MVP

| Batch | Scope | Depends On |
| --- | --- | --- |
| `Batch 32` | typed boundary-request model decoupled from `DESystem.boundaries`, plus validation rules and docs | `Batch 31` |
| `Batch 33` | manual boundary-input surface and boundary-attachment seam that merges explicit boundary data into a `DESystem` or `SolveRequest` with typed `boundary_unsolved` failures | `Batch 32` |
| `Batch 34` | numeric coefficient-evaluation seam from symbolic matrix strings to arbitrary-precision scalar values at one kinematic point | `Batch 33` |
| `Batch 35` | singular-point detection and classification seam over a `DESystem` variable | `Batch 34` |
| `Batch 36` | scalar regular-point local-series patch generator with exact residual checks on toy systems | `Batch 35` |
| `Batch 37` | triangular or block-upper-triangular matrix regular-point patch generator over reviewed `DESystem` inputs | `Batch 36` |
| `Batch 38` | overlap matcher and local residual evaluator for two regular patches | `Batch 37` |
| `Batch 39` | replace `BootstrapSeriesSolver` with a real regular-point continuation solver on nonsingular paths | `Batch 38` |
| `Batch 40` | standalone DE-solver wrapper over the real solver, reusing `SolveRequest` and explicit boundary inputs | `Batch 39` |
| `Batch 41` | scalar regular-singular/Frobenius patch generator | `Batch 40` |
| `Batch 42` | block-matrix regular-singular patch generator and first supported resonance handling | `Batch 41` |
| `Batch 43` | mixed regular/regular-singular continuation driver with patch handoff and residual-based acceptance | `Batch 42` |
| `Milestone M1` | solver MVP gate: `SeriesSolver` is no longer scaffolded, T2 self-consistency passes, and the standalone DE-solver example is live | `Batch 32` through `Batch 43` |

### Phase C: Boundary And Ending Infrastructure

| Batch | Scope | Depends On |
| --- | --- | --- |
| `Batch 44` | boundary-provider interface that maps a `BoundaryRequest` to explicit `BoundaryCondition` data with deterministic failures | `Milestone M1`, `Batch 33` |
| `Batch 45` | first builtin `eta -> infinity` boundary-request generator for a trivial bootstrap subset, still without full numerical solving | `Batch 44`, `Batch 31` |
| `Batch 46` | first ending-planned boundary wrapper that composes reviewed ending selection with boundary-request generation | `Batch 45` |
| `Batch 47` | manual-vs-automatic boundary equivalence harness for simple Euclidean loop families | `Batch 46`, `Milestone M0a`, `K0b` |
| `Milestone M2` | boundary gate: ending decisions produce typed boundary requests, boundary planning/provider failures surface as `boundary_unsolved`, and manual vs automatic boundary workflows agree on the supported subset | `Batch 44` through `Batch 47` |

### Phase D: Flow-Construction Parity

| Batch | Scope | Depends On |
| --- | --- | --- |
| `Batch 48` | bootstrap-only builtin eta-mode planning seam for `Prescription`, keeping the other stubbed builtins deferred | `Batch 27`, `Milestone M1`, `Milestone M2` |
| `Batch 49` | builtin eta-mode planning seam for `Propagator` as a structural selector over the current reviewed subset only | `Batch 48` |
| `Batch 49b` | narrow bootstrap builtin eta-mode planning seam for `Mass` on the current local reviewed subset, with only the minimal eta-generated-path mass-coherence widening needed to keep selected equal-mass reducer-facing literals aligned with planner grouping | `Batch 49` |
| `Batch 50a` | internal eta-topology preflight snapshot seam for `Branch` and `Loop` on the current family/kinematics surface, keeping the public API unchanged and making the blocker explicit | `Batch 49b` |
| `Batch 50b` | internal topology-prerequisite bridge/prereq snapshot for `Branch` and `Loop` over the current family/kinematics surface, with explicit availability and missing-field reporting while both modes remain blocked | `Batch 50a` |
| `Batch 50` | truthful builtin eta-mode selector semantics for `Branch` and `Loop` over the internal topology-prerequisite bridge, after real candidate analysis exists | `Batch 50b` |
| `Batch 51` | multi-invariant orchestration seam that iterates the reviewed one-invariant generator/execution path without widening solver behavior | `Batch 17`, `Milestone M1`, `Milestone M2` |
| `Batch 52` | symbolic widening of `BuildInvariantDerivativeSeed(...)` beyond the current massless-standard bootstrap subset while staying representable on the family propagator table | `Batch 51` |
| `Batch 53` | multiple top-sector Kira target orchestration and reduction-span validation over the accepted preparation/execution seams | `Batch 8`, `Batch 12`, `Batch 51`, `Batch 52` |
| `Milestone M3` | loop-core parity gate: family model matches reference, prefactors are locked, and Kira reduction span matches on the first mandatory package families | `Batch 48`, `Batch 49`, `Batch 49b`, `Batch 50a`, `Batch 50b`, and `Batch 50` through `Batch 53`, `Milestone M0b` |

### Phase E: Numerical Robustness And Runtime Policy

| Batch | Scope | Depends On |
| --- | --- | --- |
| `Batch 54` | landed on `main`: precision-budget preflight plus an internal retry controller on generated-solver handoffs, while direct `SolveDifferentialEquation(...)` remains passthrough | `Milestone M2` |
| `Batch 55` | landed on `main`: diagnostics hardening for `master_set_instability` and `continuation_budget_exhausted` | `Batch 54` |
| `Batch 56` | solved-path cache manifest plus `UseCache` replay and invalidation of successful solved-path diagnostics | `Batch 55` |
| `Batch 57` | wrapper-only `skip_reduction` reuse on the two `SolveAmfOptionsEtaModeSeries(...)` overloads over matching prepared eta-generated state | `Batch 56` |
| `Batch 58` | wrapper-owned live wiring of `WorkingPre`, `ChopPre`, `XOrder`, `ExtraXOrder`, `LearnXOrder`, `TestXOrder`, `RationalizePre`, and `RunLength` on the two `SolveAmfOptionsEtaModeSeries(...)` overloads, with direct non-`AmfOptions` entry points unchanged | `Batch 57` |
| `Milestone M4` | robustness gate: narrow exact-subset requested-digits evidence plus explicit failure codes, solved-path cache/`UseCache`, and `SkipReduction` all pass the reviewed T2/T5 coverage slice | `Batch 54` through `Batch 58f` |

### Phase F: Full Feature-Surface Parity

| Batch | Scope | Depends On |
| --- | --- | --- |
| `Batch 59` | arbitrary `D0` support through the solver request, DE construction, and evaluation path | `Milestone M3`, `Milestone M4` |
| `Batch 60` | fixed-`eps` numeric mode over the same solver path | `Batch 59` |
| `Batch 61` | complex-kinematics continuation and branch-bookkeeping surface | `Batch 60` |
| `Batch 62` | singular-kinematics guardrails and failure diagnostics for the first physical-region subset | `Batch 61` |
| `Batch 63` | phase-space integration and `feynman_prescription` driver over the accepted DE solver path | `Batch 62` |
| `Batch 64` | linear-propagator family-model widening through `ProblemSpec`, derivative generation, Kira preparation, and solver-path compatibility | `Batch 63`, `Milestone M3` |
| `Batch 65` | user-defined ending-scheme execution path from `AmfOptions` through boundary generation and solver use | `Batch 46`, `Milestone M4` |
| `Batch 66` | full mixed builtin/user-defined eta-mode execution through the `AmfOptions` solve path with reviewed selection semantics preserved | `Batch 27`, `Batch 50`, `Milestone M4` |
| `Milestone M5` | feature-parity gate covering `automatic_loop`, `automatic_vs_manual`, `complex_kinematics`, `spacetime_dimension`, `linear_propagator`, `feynman_prescription`, `automatic_phasespace`, and `user_defined_amfmode` / `user_defined_ending` | `Batch 59` through `Batch 66`, `Milestone M0b` |

### Phase G: Qualification And Release

| Batch Or Milestone | Scope | Depends On |
| --- | --- | --- |
| `Milestone M6` | qualification corpus over the parity-matrix benchmarks and upstream regression families, using already captured upstream goldens | `Milestone M5`, `Milestone M0b` |
| `Milestone M7` | release gate: performance review, diagnostic review, docs completion, and parity sign-off | `Milestone M6` |

The first `Milestone M7` groundwork packet is documentation-only:
`docs/release-signoff-checklist.md` records the exact evidence buckets,
mandatory release-note prompts, and reviewer dispositions that a future release
packet must fill in. The follow-on machine-readable scaffold
`tools/reference-harness/templates/release-signoff-checklist.json` freezes the
prerequisite M6 gate plus the same later release-review sections for later
harness-side consumers. Neither
artifact relaxes the dependency on truthful `Milestone M6` closure or claims
that release sign-off is complete.
The first executable M7 helper then stays blocked on purpose:
`tools/reference-harness/scripts/release_signoff_readiness.py` consumes one
machine-readable `qualification_readiness.py` summary plus that checklist,
audits the durable checklist/doc targets, and writes one blocked release-
readiness summary that keeps the current `b61n` / `b62n` / `b63k` / `b64k`
frontier visible without claiming that `Milestone M6` or `Milestone M7`
has closed.

## Acceptance Gates Per Phase

### Phase A Gate

- mixed builtin/user-defined eta-mode execution is available at single-name, ordered-list, and
  `AmfOptions` layers
- builtin/user-defined ending-scheme selection is available at single-name, ordered-list, and
  `AmfOptions` layers
- all new seams remain library-only and do not yet claim numerical solver parity

### Phase B Gate

- `BootstrapSeriesSolver` has been replaced by a real high-precision solver on the supported subset
- regular-point and regular-singular patch generation both exist
- residual checks and overlap checks are mandatory, not optional
- manual boundary input and attachment are explicit
- `boundary_unsolved` is already a live typed failure for missing or invalid explicit boundary data

### Phase C Gate

- ending decisions produce typed boundary requests
- boundary generation and boundary attachment are explicit and deterministic
- failures in boundary planning or provider lookup surface as typed `boundary_unsolved` before
  solver execution begins

### Phase D Gate

- builtin eta-mode planning semantics are no longer stubs
- eta and invariant derivative generation close on the reviewed master-space families
- multiple top-sector loop-core coverage exists
- the phase-1 parity-matrix gates are backed by concrete tests, captured references, and fixture
  evidence

### Phase E Gate

- precision monotonicity is demonstrated only on the reviewed exact subset
- all required failure codes from `specs/parity-matrix.yaml` are live and tested
- cache/restart, `UseCache`, and `SkipReduction` are explicit, deterministic, and do not hide
  stale data
- live solver policy fields from `AmfOptions` are wired and documented

### Phase F Gate

- arbitrary `D0`, fixed-`eps`, complex kinematics, linear propagators, phase-space integration,
  `feynman_prescription`, singular-kinematics guardrails, and user-defined hooks are all
  exercised through the live solver path
- the frozen example classes in `docs/parity-contract.md` have explicit coverage status

### Phase G Gate

- the qualification corpus passes against already captured goldens
- diagnostics and performance have been reviewed on the mandatory benchmark set
- the release packet records docs/parity sign-off against the frozen checklist
  in `docs/release-signoff-checklist.md`

## Explicit Non-Goals And Defers For Early Solver Phases

These items are intentionally deferred until the phase gates above say otherwise:

- no in-house IBP reducer or finite-field reconstruction engine in v1
- no performance tuning before the solver passes T2 self-consistency and T3 parity checks
- no broader cache/restart claims beyond the reviewed solved-path artifact slice, and no
  `SkipReduction` semantics, before there is a real solver and patch-artifact model
- no broad CLI widening before the library solver and boundary surfaces are frozen
- no assumption that canonical bases always exist
- no claim of full elliptic or non-canonical-sector support before the regular/regular-singular
  solver is stable
- no phase-space or complex physical-region automation before Euclidean loop-core parity and
  boundary plumbing are in place
- no mixed multi-family orchestration before single-family solve paths and standalone DE solving are
  stable

## Mandatory Benchmark And Regression Gates

### Solver-Core Gates

- standalone `differential_equation_solver` example
- known regression: asymptotic-series overflow
- known regression: `AnalyzeBlock` on non-block-triangular systems
- solver self-consistency:
  - residual checks
  - patch-overlap mismatch checks
  - precision monotonicity under increased requested digits

### Boundary And Ending Gates

- `automatic_vs_manual`
- known regression: boundary-generation errors for unnormalized loop momenta
- at least one reviewed `eta -> infinity` boundary case with recursive follow-on boundary tasks

### Mode And Hook Gates

- `user_defined_amfmode`
- `user_defined_ending`
- known regression: incorrect `Mass`-mode insertion

### Runtime-Feature Gates

- `automatic_loop`
- `complex_kinematics`
- `feynman_prescription`
- `spacetime_dimension`
- `linear_propagator`
- `automatic_phasespace`
- fixed-`eps` workflow coverage
- multiple top-sector coverage

### Reducer-And-Assembly Regression Gates

- known regression: unexpected master sets in the Kira interface
- previously accepted reducer parsing and generated-target assembly regressions must remain green
  while the solver and option layers widen

### Qualification Benchmark Gates

- package double box
- planar `ttbar j`
- `ttbar H` light-quark-loop master integrals
- five-point one-mass scattering
- `ttbar W`
- diphoton heavy-quark form factors
- `h -> bb`
- `N=4` SYM three-loop form factor
- single-top planar and nonplanar
- at least one singular-endpoint case from the singular-kinematics AMF literature

Digit thresholds remain those frozen in `docs/verification-strategy.md`:

- `>= 50` correct digits on core package families
- `>= 100` digits on `2024-tth-light-quark-loop-mi`
- `>= 200` digits on `2023-diphoton-heavy-quark-form-factors`

Precision monotonicity and explicit unstable-run diagnostics are mandatory.

## Execution Guidance For Future Turns

- treat `Track P/S/B/K/R` plus the exit gates above as the authoritative scheduler
- keep using `docs/orchestration-workflow.md` for the per-batch planner/theory/verification /
  implementation / review loop inside each active track
- update `docs/public-contract.md` and `docs/implementation-ledger.md` on every accepted code batch
  and on every accepted control-model change such as `R0`, `P0`, `K0`, `M0a`, `S0`, or `M0b`
- do not claim parity, reference-match, or release readiness outside the subset justified by the
  currently closed gates and captured evidence
- if a future batch must break local row order inside a track, record the dependency reason in the
  new ledger row
