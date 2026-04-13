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
| `Track B: Boundary And Parity Prep` | prepare boundary-provider and parity fixtures/contracts without widening live auto-boundary behavior early | reviewed `Batch 44` through `Batch 47`; `Milestone M2` accepted narrowly; `Batch 48` is the next roadmap-owned lane |
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

- authoritative `main` base is `f2f3f03f36ef1095b76bf1f52c413a907d041856` and is the current accepted release baseline
- reviewed implementation remains accepted through `Batch 46`; `Milestone M1` is complete on that
  reviewed surface
- `Milestone M0a` is accepted as cluster/reference-harness bootstrap readiness only
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
- this does not widen the public/runtime surface beyond reviewed `Batch 46`, does not accept
  broader automatic-boundary execution/provider parity, and does not relax the separate `M0b`
  blocker on broader parity claims
- `Batch 48` is now the next roadmap-owned implementation lane

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
  wrappers
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

- builtin eta modes other than `All` are still explicit bootstrap stubs
- accepted manual-vs-automatic boundary equivalence is still narrow: only the supported simple
  Euclidean massless sample subset is covered, and only for builtin `Tradition` plus one exact
  user-defined singleton `<family>::eta->infinity`
- solver/provider coupling, automatic boundary execution, and `BoundaryCondition` generation from
  builtin/planned `eta -> infinity` requests remain deferred
- `BuildInvariantDerivativeSeed(...)` is still limited to a narrow bootstrap symbolic subset
- there is no multi-invariant orchestration, multiple top-sector orchestration, or loop-core
  reduction-span parity gate
- there is no dynamic precision manager, no cache/restart semantics, and no live `SkipReduction`
  runtime path
- feature parity is still missing for complex kinematics, arbitrary `D0`, fixed-`eps`, linear
  propagators, phase-space integration, standalone DE solving at parity quality, and full
  user-defined eta/ending execution paths
- the reference harness is still `bootstrap-only`; `M0a` is accepted, but `M0b` is still open and
  no real upstream AMFlow goldens have been captured yet

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
- dynamic precision escalation
- cancellation/truncation detection
- required failure codes from `specs/parity-matrix.yaml`
- cache/restart artifact model
- `SkipReduction` semantics
- live wiring for `WorkingPre`, `ChopPre`, `XOrder`, `ExtraXOrder`, `LearnXOrder`,
  `TestXOrder`, `RationalizePre`, and `RunLength`

Primary output:
- explicit stability management and honest runtime policy rather than stored-but-ignored options

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
- `M0b`: rolling real captured evidence for the frozen example classes, mandatory benchmark
  families, and frozen upstream regression surfaces before later phases claim "matches reference"

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
| `Milestone M0b` | rolling real-golden capture and reproducibility evidence for the frozen example classes, mandatory benchmark families, and upstream regression families | `Milestone M0a` |

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
| `Batch 48` | first full builtin eta-mode planning seam for `Prescription`, keeping the other stubbed builtins deferred | `Batch 27`, `Milestone M1`, `Milestone M2` |
| `Batch 49` | builtin eta-mode planning seam for `Mass` and `Propagator`, with explicit regression coverage for the known upstream `Mass`-mode bug surface | `Batch 48` |
| `Batch 50` | builtin eta-mode planning seam for `Branch` and `Loop` | `Batch 49` |
| `Batch 51` | multi-invariant orchestration seam that iterates the reviewed one-invariant generator/execution path without widening solver behavior | `Batch 17`, `Milestone M1`, `Milestone M2` |
| `Batch 52` | symbolic widening of `BuildInvariantDerivativeSeed(...)` beyond the current massless-standard bootstrap subset while staying representable on the family propagator table | `Batch 51` |
| `Batch 53` | multiple top-sector Kira target orchestration and reduction-span validation over the accepted preparation/execution seams | `Batch 8`, `Batch 12`, `Batch 51`, `Batch 52` |
| `Milestone M3` | loop-core parity gate: family model matches reference, prefactors are locked, and Kira reduction span matches on the first mandatory package families | `Batch 48` through `Batch 53`, `Milestone M0b` |

### Phase E: Numerical Robustness And Runtime Policy

| Batch | Scope | Depends On |
| --- | --- | --- |
| `Batch 54` | dynamic precision-escalation controller with explicit `insufficient_precision` diagnostics | `Milestone M2`, `Milestone M3` |
| `Batch 55` | diagnostics hardening for `master_set_instability` and `continuation_budget_exhausted` | `Batch 54` |
| `Batch 56` | patch/cache artifact model plus `UseCache` replay and invalidation semantics for solved paths | `Batch 55` |
| `Batch 57` | `SkipReduction` runtime path over cached or caller-provided `DESystem` inputs | `Batch 56` |
| `Batch 58` | live wiring of `WorkingPre`, `ChopPre`, `XOrder`, `ExtraXOrder`, `LearnXOrder`, `TestXOrder`, `RationalizePre`, and `RunLength` into solver policy | `Batch 57` |
| `Milestone M4` | robustness gate: precision monotonicity, explicit failure codes, cache/restart, `UseCache`, and `SkipReduction` all pass T2/T5 coverage | `Batch 54` through `Batch 58` |

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

- precision monotonicity is demonstrated on the supported benchmark subset
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

## Explicit Non-Goals And Defers For Early Solver Phases

These items are intentionally deferred until the phase gates above say otherwise:

- no in-house IBP reducer or finite-field reconstruction engine in v1
- no performance tuning before the solver passes T2 self-consistency and T3 parity checks
- no cache/restart or `SkipReduction` semantics before there is a real solver and patch-artifact
  model
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
