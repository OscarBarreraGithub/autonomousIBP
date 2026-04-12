# Public Contract Bootstrap

This document defines the first stable C++ interface boundary for the port.

The Batch 32 and Batch 33 notes below remain the current reviewed boundary
split. The Batch 34 and Batch 35 notes record the current reviewed
coefficient-evaluation and singular-point seams. The Batch 36 note records the
current implemented scalar regular-point local-series seam, and the Batch 37
note records the current implemented upper-triangular matrix regular-point
patch seam. The Batch 38 note records the current implemented exact
regular-patch residual and overlap diagnostics. The Batch 39 note records the
current implemented exact one-hop regular-point continuation solver surface on
the reviewed upper-triangular subset, without overclaiming later singular-path
or multi-hop solver semantics. The Batch 40 note records the current
implemented standalone library-only default-solver wrapper over that reviewed
Batch 39 continuation surface. The Batch 41 note records the current reviewed
scalar regular-singular / Frobenius patch seam. The Batch 42 note records the
current reviewed upper-triangular matrix regular-singular / Frobenius patch
seam on the diagonal-residue, no-log subset, and the Batch 43 note records the
current implemented exact mixed regular-start to regular-singular-target
continuation slice on the integer-exponent Frobenius subset. The Batch 44 note
records the current reviewed caller-supplied boundary-provider seam that
maps explicit `BoundaryRequest` entries to explicit `BoundaryCondition` data
without changing the solver path, and the Batch 45 note records the current
reviewed pure builtin `eta -> infinity` boundary-request generator over a
validated `ProblemSpec`. The Batch 46 note records the current reviewed
single-name ending-planned wrapper over that reviewed Batch 45 generator.

## Core Types

- `ProblemSpec`: family definition, propagators, cuts, conservation rules, invariants, prescriptions, targets, numeric substitutions, dimensional settings
- `AmfOptions`: AMFlow runtime controls
- `ReductionOptions`: backend and reducer controls
- `DESystem`: ordered masters, differentiation variables, exact coefficient matrices, singular-point annotations
- `BoundaryRequest`, `BoundaryCondition`, `BoundaryUnsolvedError`, `AttachManualBoundaryConditions(...)`, `BoundaryProvider`, and `AttachBoundaryConditionsFromProvider(...)`: typed boundary request, explicit boundary data, failure, manual attachment surface, and caller-supplied provider seam decoupled from `DESystem`-owned boundary storage
- `GenerateBuiltinEtaInfinityBoundaryRequest(...)`: pure builtin boundary-request generation over a validated bootstrap `ProblemSpec` subset, returning one explicit `BoundaryRequest` without boundary values or solver execution
- `GeneratePlannedEtaInfinityBoundaryRequest(...)`: single-name ending-planned wrapper that accepts only the exact singleton `<family>::eta->infinity` terminal-node decision and then returns the reviewed builtin `eta -> infinity` `BoundaryRequest`
- `ExactRational`, `EvaluateCoefficientExpression(...)`, and `EvaluateCoefficientMatrix(...)`: exact rational evaluation of one coefficient expression or one selected `DESystem` coefficient matrix at one explicit substitution point
- `SeriesPatch` plus `GenerateScalarRegularPointSeriesPatch(...)`: the first scalar-only regular-point local-series patch seam over one selected reviewed `DESystem` variable
- `ScalarFrobeniusSeriesPatch` plus `GenerateScalarFrobeniusSeriesPatch(...)`: the first scalar-only regular-singular / Frobenius local-series patch seam over one selected reviewed `DESystem` variable
- `UpperTriangularMatrixSeriesPatch` plus `GenerateUpperTriangularRegularPointSeriesPatch(...)`: the first upper-triangular matrix regular-point local propagator seam over one selected reviewed `DESystem` variable
- `UpperTriangularMatrixFrobeniusSeriesPatch` plus `GenerateUpperTriangularMatrixFrobeniusSeriesPatch(...)`: the first upper-triangular matrix regular-singular / Frobenius local propagator seam over one selected reviewed `DESystem` variable on the diagonal-residue, no-log subset
- `ScalarSeriesPatchOverlapDiagnostics`, `EvaluateScalarSeriesPatchResidual(...)`, and `MatchScalarSeriesPatches(...)`: exact scalar patch residual and overlap diagnostics over already-generated regular patches
- `UpperTriangularMatrixSeriesPatchOverlapDiagnostics`, `EvaluateUpperTriangularMatrixSeriesPatchResidual(...)`, and `MatchUpperTriangularMatrixSeriesPatches(...)`: exact upper-triangular matrix patch residual and overlap diagnostics over already-generated regular patches
- `SolveRequest`, `SolverDiagnostics`, `SeriesSolver`, `BootstrapSeriesSolver`, `MakeBootstrapSeriesSolver()`, and `SolveDifferentialEquation(...)`: the library-only exact one-hop continuation solver surface plus default bootstrap-solver construction and standalone wrapper over one declared reviewed `DESystem` variable with explicit manual start-boundary attachment, covering the reviewed regular/regular path and the implemented Batch 43 mixed regular-start to regular-singular-target path on the integer-exponent Frobenius subset
- `PrecisionPolicy`: precision and stability controls
- `ArtifactManifest`: reproducibility and cache metadata
- `ParsedMasterList` and `ParsedReductionResult`: deterministic typed views of Kira `masters` and `kira_target.m` artifacts for the bootstrap reducer boundary
- `ReducedDerivativeVariableInput` plus `AssembleReducedDESystem(...)`: the first typed ingestion path from already-reduced derivative targets into a `DESystem`
- `GeneratedDerivativeVariable` plus `GenerateEtaDerivativeVariable(...)`: typed eta-only unreduced derivative rows built from the accepted auxiliary-family transform
- `InvariantDerivativeSeed` plus `GenerateInvariantDerivativeVariable(...)`: typed invariant unreduced derivative rows generated from explicit precomputed denominator-derivative expressions
- `BuildInvariantDerivativeSeed(...)`: typed one-invariant-at-a-time construction of `InvariantDerivativeSeed` from a validated bootstrap `ProblemSpec` subset
- `GeneratedDerivativeVariableReductionInput` plus `AssembleGeneratedDerivativeDESystem(...)`: typed ingestion of generated rows plus parsed reductions into a `DESystem`
- `EtaGeneratedReductionPreparation` plus `PrepareEtaGeneratedReduction(...)`: typed eta-generated target preparation from the accepted auxiliary-family and eta-derivative seams into the Kira reducer boundary
- `InvariantGeneratedReductionPreparation` plus `PrepareInvariantGeneratedReduction(...)`: typed invariant-generated target preparation from the accepted invariant-derivative seam into the Kira reducer boundary, with overloads for either a precomputed `InvariantDerivativeSeed` or one invariant name on a validated bootstrap `ProblemSpec`
- `EtaGeneratedReductionExecution` plus `RunEtaGeneratedReduction(...)`: typed eta-only orchestration over preparation, execution, parsing, and generated-row assembly
- `BuildEtaGeneratedDESystem(...)`: the first library-only eta-generated `DESystem` consumer over the reviewed eta-generated execution wrapper
- `InvariantGeneratedReductionExecution` plus `RunInvariantGeneratedReduction(...)`: typed invariant-only orchestration over preparation, execution, parsing, and generated-row assembly, with overloads for either a precomputed `InvariantDerivativeSeed` or one invariant name on a validated bootstrap `ProblemSpec`
- `BuildInvariantGeneratedDESystem(...)`: the first library-only one-invariant `DESystem` consumer over the reviewed automatic invariant-generated execution wrapper
- `SolveInvariantGeneratedSeries(...)`: the first library-only one-invariant solver handoff from the reviewed automatic invariant-generated `DESystem` consumer into an injected `SeriesSolver`
- `SolveEtaGeneratedSeries(...)`: the first library-only eta-generated solver handoff from the reviewed eta-generated `DESystem` consumer into an injected `SeriesSolver`
- `SolveEtaModePlannedSeries(...)`: the first library-only eta-mode-planned solver handoff that composes `EtaMode::Plan(...)` with the reviewed eta-generated solver wrapper
- `SolveBuiltinEtaModeSeries(...)`: the first builtin eta-mode-name library-only solve wrapper that resolves `MakeBuiltinEtaMode(...)` and reuses the reviewed eta-mode-planned solver handoff
- `SolveBuiltinEtaModeListSeries(...)`: the first caller-supplied ordered builtin eta-mode-list library-only solve wrapper that selects the first planning-successful builtin and reuses the reviewed single-name builtin solver handoff
- `SolveAmfOptionsEtaModeSeries(...)`: the first `AmfOptions`-fed eta-mode solver-wrapper surface, with reviewed builtin-only and mixed builtin/user-defined overloads
- `SolveResolvedEtaModeSeries(...)`: the first single-name library-only eta-mode solver wrapper that resolves one name against builtin plus user-defined registrations and then reuses the reviewed eta-mode-planned solver handoff
- `SolveResolvedEtaModeListSeries(...)`: the first ordered mixed builtin-plus-user-defined eta-mode-list library-only solve wrapper that probes planning in caller order and carries the winning planned eta decision forward without re-planning
- `ResolveEtaMode(...)`: the first builtin-or-user-defined one-name eta-mode resolver hook over the existing `EtaMode` interface
- `ResolveEndingScheme(...)`: the first builtin-or-user-defined one-name ending-scheme resolver hook over the existing `EndingScheme` interface
- `PlanEndingScheme(...)`: the first single-name ending-scheme planning wrapper that resolves one ending name and returns a typed `EndingDecision`
- `PlanEndingSchemeList(...)`: the first ordered builtin-plus-user-defined ending-scheme selection wrapper that probes planning in caller order and carries the winning ending decision forward without re-planning
- `PlanAmfOptionsEndingScheme(...)`: the first `AmfOptions`-fed ending-scheme planner wrapper that reads only `AmfOptions::ending_schemes` and reuses the reviewed ordered ending-selection wrapper
- `EtaInsertionDecision` plus `ApplyEtaInsertion(...)`: the first typed auxiliary-family transformation seam from an immutable `ProblemSpec` to an eta-shifted auxiliary family

## Runtime Boundaries

- production AMFlow is C++ only
- Kira and Fermat are external processes
- Mathematica is allowed only in the reference harness
- user-defined `AMFMode` and `EndingScheme` become C++ hook interfaces
- external reducer execution is represented explicitly as a prepared command plus an execution result with command text, execution working directory, exit code, stdout/stderr log paths, and environment overrides

## CLI Commands In This Bootstrap

- `sample-problem`
- `emit-kira`
- `run-kira <kira> <fermat> [dir]`
- `load-spec <file>`
- `emit-kira-from-file <file> [dir]`
- `parse-kira-results <artifact-root> <family>`
- `run-kira-from-file <file> <kira> <fermat> [dir]`
- `show-defaults`
- `write-manifest <dir>`

The bootstrap CLI supports a deterministic YAML subset for `ProblemSpec` loading. The supported shape matches the checked-in example spec: nested `family`, `kinematics`, and `targets` sections; bracketed scalar lists; block lists for propagators, preferred masters, scalar-product rules, and targets; scalar maps for numeric substitutions; and top-level `dimension`, `complex_mode`, and `notes`.

The file-backed loader applies two safety rules on top of that subset:

- additive unknown fields are ignored and omitted from the canonicalized output, so older binaries can accept forward-compatible spec extensions
- duplicate keys and duplicate mapping entries are rejected at parse time

The CLI also applies loaded-spec validation before any Kira artifact emission. In this bootstrap that includes rejecting empty target index lists and target index arity mismatches against the declared propagator family.

The Kira runner keeps reducer execution explicit and deterministic:

- callers must pass both the Kira executable path and the Fermat executable path explicitly
- the runner records the actual execution working directory separately from the rendered command text so runs are replayable
- the runner writes deterministic stdout and stderr logs under the artifact layout before reporting success or failure, including invalid-configuration rejections
- repeated runs allocate unique attempt-scoped log files such as `kira.attempt-0001.stdout.log` instead of overwriting prior attempts
- emitted Kira preparation files are written with checked I/O and validated on disk before execution is attempted
- validation failures and missing executables are reported as invalid configuration errors without attempting execution
- fork, `chdir`, and `execve` startup failures are reported as `FailedToStart`, distinct from a completed reducer process that exits nonzero
- nonzero reducer exits are surfaced directly with the recorded exit code and preserved logs

The bootstrap also exposes a deterministic parsed-result surface for Kira artifacts:

- `results/<family>/masters` is parsed as one master integral per non-empty line, using the first whitespace-delimited token exactly as the AMFlow snapshot does
- `results/<family>/kira_target.m` is parsed as a narrow Mathematica-rule subset: an outer list of `target -> linear combination of masters`
- malformed masters, malformed rule expressions, duplicate targets, and rule terms that reference masters outside the parsed basis fail locally with deterministic parse errors
- nonlinear master occurrences such as inverse powers, explicit powers, nested function calls, or denominator-position master factors are rejected locally instead of being treated as linear coefficients
- parsed reduction terms are canonicalized per master by combining duplicate coefficients and dropping zero-net terms before identity rules are appended
- if `kira_target.m` is missing or reduces to no explicit rules, the parser falls back to identity rules over the parsed master basis, matching the current AMFlow Kira interface bootstrap behavior

The first DE-assembly ingestion path is intentionally narrow:

- it assembles coefficient matrices only from an explicit ordered master basis plus one or more already-reduced derivative-variable inputs
- each variable input must supply explicit row bindings of `{source master, reduced target}` and a parsed reduction result over the same ordered master basis
- row bindings must cover the full canonical master basis in exact order; row-permuted bindings fail locally instead of silently reordering the system
- derivative-target lookup uses only the explicit reduced rules from the parsed result; the appended identity rules are basis-closure helpers and do not satisfy missing derivative reductions
- missing reduced targets, master-basis mismatches, duplicate derivative targets, duplicate variable names, and malformed coefficient-matrix shapes fail locally with deterministic diagnostics
- this batch does not generate derivatives or perform physics-driven DE construction; it only ingests reduced derivative rows into a typed `DESystem`

The first eta-derivative generation seam is also intentionally narrow:

- it is eta-only and builds unreduced derivative rows from the reviewed auxiliary-family transform
- for each canonical master `J(a1,...,an)` and each rewritten propagator index `i`, it emits `-ai * J(a1,...,ai+1,...,an)` exactly when `ai != 0`
- selected exponent `0` emits no term; negative exponents use the same `-ai` rule and still increment the selected propagator index by one
- the generated variable is `eta` with `DifferentiationVariableKind::Eta`
- `reduction_targets` is the deduplicated first-appearance list of emitted nonzero targets in row order and then term order
- `ParsedMasterList.family`, per-master family labels, and master-index arity must all match the transformed family; mismatches fail locally with deterministic diagnostics

The first invariant-derivative generation seam is also intentionally narrow:

- it consumes explicit or auto-built precomputed denominator-derivative expressions and still does not implement the full upstream symbolic derivation route
- each propagator derivative is a linear combination of same-family factor integrals represented by full index vectors and string coefficients
- for each canonical master `J(a1,...,an)`, propagator `i`, and factor term `c * J(b1,...,bn)`, it emits `(-ai)*(c) * J(a1+b1,...,ai+1+bi,...,an+bn)` exactly when `ai != 0`
- duplicate generated targets within a row are combined in first-encounter order using literal ` + ` joins, repeated same-factor contributions are summed before rendering, and row terms whose literal collected multiplier net is zero are dropped
- `reduction_targets` preserves the deduplicated first-appearance order across rows after zero-net row terms are removed
- the generated variable must have `DifferentiationVariableKind::Invariant`, must use a non-empty non-`eta` variable name, and `ParsedMasterList.family`, per-master family labels, master-index arity, propagator-derivative table arity, and factor-index arity must all match the supplied family/propagator count
- invariant reduction preparation and invariant execution/orchestration remain out of scope in this batch

The first automatic invariant-seed construction seam is also bootstrap-only:

- `BuildInvariantDerivativeSeed(...)` derives one `InvariantDerivativeSeed` from `ProblemSpec` plus a requested invariant name, then leaves generated-row construction to the existing `GenerateInvariantDerivativeVariable(...)` seam
- it is one-invariant-at-a-time and supports only a validated bootstrap subset: `Standard` propagators with `mass == "0"`; arithmetic with `+`, `-`, `*`, parentheses, integer or rational constants, invariant symbols, and squared linear momentum combinations such as `(k-p1-p2)^2`; and scalar-product rules whose left side is an external-momentum pair such as `p1*p2` and whose right side is a linear scalar expression in the declared invariants
- auto-built derivatives are converted only when every differentiated term can be represented as a coefficient times a same-family propagator-factor product on the current propagator table, with bootstrap matching over exact normalized propagator subexpressions
- the seam rejects empty, unknown, or `eta` invariant names; unsupported propagator kinds; nonzero or invariant-dependent propagator masses; unsupported propagator or scalar-product-rule grammar; unknown momentum symbols; incomplete scalar-product data; normalized duplicate propagator expressions; and derivative terms that cannot be represented on the family propagator table
- the input `ProblemSpec` is not mutated, numeric substitutions are ignored during symbolic seed construction, and all-zero derivative slots remain present as empty propagator entries
- this batch does not claim the full upstream `LIBPGetDerivatives` / `LIBPComputeDerivatives` symbolic solver; it only covers the bootstrap subset above

The generated-row assembly path is similarly bootstrap-only:

- it consumes generated rows plus a parsed reduction result over the same ordered master basis
- generated target lookup uses only the explicit parsed-rule prefix; identity-fallback parsed reductions are rejected when generated targets need explicit rules
- matrix entries are composed literally as `(<generated coeff>)*(<reduction coeff>)`, with multiple contributions joined in encounter order using ` + `
- no symbolic simplification or coefficient combination is performed in this batch

The first generated-target reducer preparation seam is also bootstrap-only:

- `KiraBackend::PrepareForTargets(...)` prepares Kira files for an explicit override target list while leaving `Prepare(...)` unchanged on `ProblemSpec.targets`
- explicit target lists must be non-empty, duplicate-free, family-consistent with `spec.family.name`, and arity-consistent with `family.propagators.size()`
- `PrepareEtaGeneratedReduction(...)` composes the reviewed eta insertion seam, eta-derivative generation seam, and explicit-target Kira preparation seam into one typed preparation bundle
- eta-generated target preparation preserves the exact `reduction_targets` order from `GenerateEtaDerivativeVariable(...)`
- eta-generated preparation rejects empty generated target lists locally before reducer execution
- this batch stops at reducer preparation and fake-execution compatibility; it does not yet add automatic post-run parsing or end-to-end DE assembly orchestration

The first invariant-generated target reducer preparation seam is also bootstrap-only:

- `PrepareInvariantGeneratedReduction(...)` keeps the existing seed-based overload intact and also exposes a one-invariant-at-a-time overload that takes `(ProblemSpec, ParsedMasterList, invariant_name, ReductionOptions, ArtifactLayout)` and composes `BuildInvariantDerivativeSeed(...)` with the existing seed-based preparation path
- it consumes the original `ProblemSpec` unchanged; there is no eta insertion, no spec mutation, and the emitted family/kinematics YAML must still reflect the original family and invariant list
- preparation rejects `seed.family != spec.family.name` and `seed.propagator_derivatives.size() != spec.family.propagators.size()` locally before derivative generation
- the invariant-name overload preserves the existing `BuildInvariantDerivativeSeed(...)` diagnostics for empty, unknown, or `eta` invariant names and for unsupported bootstrap-subset masses or symbolic forms
- invariant-generated target preparation preserves the exact `reduction_targets` order from `GenerateInvariantDerivativeVariable(...)`
- invariant-generated preparation rejects empty generated target lists locally before reducer execution
- this seam remains one invariant at a time and preparation only; it does not add multi-invariant orchestration, CLI, `SkipReduction`, or broader symbolic parity beyond the reviewed bootstrap subset

The first eta-generated reduction execution seam is also bootstrap-only:

- `RunEtaGeneratedReduction(...)` composes the accepted eta preparation seam, existing Kira execution boundary, parsed-result ingestion, and generated-row DE assembly into one typed eta-only flow
- reducer execution always runs after successful preparation; if execution fails, the wrapper returns cleanly with `parsed_reduction_result` and `assembled_system` unset
- if execution succeeds, the wrapper parses `results/<family>/masters` and `kira_target.m` from the reducer execution working-directory artifact root, then assembles the eta `DESystem`
- successful process execution is not enough on its own: malformed parsed reductions or identity-fallback reductions for generated eta targets fail during the parse/assembly phase
- this batch does not add CLI, `SkipReduction`, invariant reduction orchestration, or broader end-to-end orchestration beyond the eta-only wrapper

The first invariant-generated reduction execution seam is also bootstrap-only:

- `RunInvariantGeneratedReduction(...)` keeps the existing seed-based overload intact and also exposes a one-invariant-at-a-time overload that takes `(ProblemSpec, ParsedMasterList, invariant_name, ReductionOptions, ArtifactLayout, kira_executable, fermat_executable)`
- the invariant-name overload is a thin wrapper: it composes the accepted automatic invariant-preparation seam from `BuildInvariantDerivativeSeed(...)` and `PrepareInvariantGeneratedReduction(...)`, then routes through the same post-preparation execution, parsing, and generated-row assembly logic as the seed-based overload
- reducer execution always runs after successful preparation; if execution fails, the wrapper returns cleanly with `parsed_reduction_result` and `assembled_system` unset
- if execution succeeds, both overloads parse `results/<family>/masters` and `kira_target.m` from the reducer execution working-directory artifact root using the original family name, then assemble the invariant `DESystem`
- invariant execution continues to use the original family and kinematics without eta insertion, and the automatic overload preserves the exact generated-target order from the reviewed automatic preparation path
- successful process execution is not enough on its own: malformed parsed reductions, identity-fallback reductions, or missing explicit rules for generated invariant targets fail during the parse/assembly phase
- this batch still does not claim the full upstream symbolic automatic-derivative solver; it remains one invariant at a time and does not add multi-invariant orchestration, CLI, `SkipReduction`, or broader symbolic-subset widening beyond the reviewed bootstrap path

The first invariant-generated `DESystem` consumer is also bootstrap-only:

- `BuildInvariantGeneratedDESystem(...)` is a library-only one-invariant convenience seam over the reviewed automatic `RunInvariantGeneratedReduction(...)` wrapper and returns the assembled `DESystem` directly on success
- it reuses the existing automatic preparation, reducer execution, parse, and generated-row assembly path instead of reimplementing reducer orchestration locally
- unsuccessful reducer execution is converted into a deterministic consumer-level failure that preserves the recorded execution status, exit code, and stderr log path in the diagnostic
- this batch does not add a seed-based consumer overload, solver invocation, CLI, multi-invariant orchestration, `SkipReduction`, or broader symbolic-subset widening

The first eta-generated `DESystem` consumer is also bootstrap-only:

- `BuildEtaGeneratedDESystem(...)` is a library-only convenience seam over the reviewed `RunEtaGeneratedReduction(...)` wrapper and returns the assembled `DESystem` directly on success
- it reuses the existing eta preparation, reducer execution, parse, and generated-row assembly path instead of reimplementing eta orchestration locally
- unsuccessful reducer execution is converted into a deterministic consumer-level failure that preserves the recorded execution status, exit code, and stderr log path in the diagnostic
- this batch does not add solver handoff, eta-mode expansion, CLI, multi-variable orchestration, `SkipReduction`, or broader end-to-end eta solving

The first invariant-generated solver handoff is also bootstrap-only:

- `SolveInvariantGeneratedSeries(...)` takes the same one-invariant automatic reduction inputs as `BuildInvariantGeneratedDESystem(...)`, plus an injected `const SeriesSolver&` and solver request fields excluding `DESystem`
- it is a thin wrapper: it calls `BuildInvariantGeneratedDESystem(...)`, populates `SolveRequest`, invokes `SeriesSolver::Solve(...)`, and returns the resulting `SolverDiagnostics` unchanged
- pre-solver failures preserve the existing `BuildInvariantGeneratedDESystem(...)` diagnostics unchanged and do not invoke the supplied solver
- this batch does not add solver-selection policy, a seed-based solver overload, CLI, multi-invariant orchestration, boundary generation, or algorithmic series solving

The first eta-generated solver handoff is also bootstrap-only:

- `SolveEtaGeneratedSeries(...)` takes the same eta-generated reduction inputs as `BuildEtaGeneratedDESystem(...)`, plus an injected `const SeriesSolver&`, explicit solver request fields excluding `DESystem`, and an optional trailing `eta_symbol`
- it is a thin wrapper: it calls `BuildEtaGeneratedDESystem(...)`, populates `SolveRequest`, invokes `SeriesSolver::Solve(...)`, and returns the resulting `SolverDiagnostics` unchanged
- pre-solver failures preserve the existing `BuildEtaGeneratedDESystem(...)` diagnostics unchanged and do not invoke the supplied solver
- this batch does not add solver-selection policy, CLI, eta-mode expansion, multi-variable orchestration, boundary generation, or algorithmic series solving

The first eta-mode-planned solver handoff is also bootstrap-only:

- `SolveEtaModePlannedSeries(...)` takes the same eta-generated solver inputs as `SolveEtaGeneratedSeries(...)`, except `EtaInsertionDecision` is replaced by an injected `const EtaMode&`
- it is a thin wrapper: it calls `EtaMode::Plan(spec)`, then forwards the resulting `EtaInsertionDecision` directly into `SolveEtaGeneratedSeries(...)`
- planning failures preserve the existing `EtaMode::Plan(...)` diagnostics unchanged and do not invoke the supplied solver
- downstream eta-generated `DESystem` construction failures also preserve the existing `SolveEtaGeneratedSeries(...)` diagnostics unchanged and do not invoke the supplied solver
- this batch does not add new builtin eta-mode semantics, cache policy, CLI, multi-variable orchestration, boundary generation, or algorithmic series solving

The first builtin eta-mode-name solver wrapper is also bootstrap-only:

- `SolveBuiltinEtaModeSeries(...)` takes the same eta solver inputs as `SolveEtaModePlannedSeries(...)`, except `const EtaMode&` is replaced by `const std::string& eta_mode_name`
- it is a thin wrapper: it calls `MakeBuiltinEtaMode(eta_mode_name)`, then forwards the resolved builtin mode directly into `SolveEtaModePlannedSeries(...)`
- builtin-name resolution failures preserve the existing `MakeBuiltinEtaMode(...)` diagnostics unchanged and do not invoke the supplied solver
- downstream builtin planning failures preserve the existing `EtaMode::Plan(...)` diagnostics unchanged and do not invoke the supplied solver
- downstream eta-generated `DESystem` construction failures also preserve the existing `SolveEtaModePlannedSeries(...)` / `SolveEtaGeneratedSeries(...)` diagnostics unchanged and do not invoke the supplied solver
- this batch does not add `AMFMode` list fallback, user-defined mode registration, new builtin eta-mode semantics, cache policy, CLI, multi-variable orchestration, boundary generation, or algorithmic series solving

The first builtin eta-mode-list solver wrapper is also bootstrap-only:

- `SolveBuiltinEtaModeListSeries(...)` takes the same eta solver inputs as `SolveBuiltinEtaModeSeries(...)`, except `const std::string& eta_mode_name` is replaced by a caller-supplied ordered `const std::vector<std::string>& eta_mode_names`
- it is a narrow ordered-selection wrapper: it resolves builtin names in caller order, probes planning in that same order, and delegates to `SolveBuiltinEtaModeSeries(...)` once for the first builtin whose planning step succeeds
- empty builtin-name lists fail locally with a deterministic argument error
- unknown builtin-name resolution failures preserve the existing `MakeBuiltinEtaMode(...)` diagnostics unchanged and stop selection immediately
- if no builtin in the caller-supplied list reaches solve selection, the final builtin planning failure is preserved unchanged and the supplied solver is not invoked
- downstream eta-generated `DESystem` construction failures from the selected builtin preserve the existing `SolveBuiltinEtaModeSeries(...)` / `SolveEtaGeneratedSeries(...)` diagnostics unchanged and do not trigger fallback to later builtin names
- this batch still does not inject the default `AMFMode` list from `AmfOptions`, add user-defined mode registration, add new builtin eta-mode semantics, add cache policy or CLI behavior, or widen into broader orchestration

The first `AmfOptions`-fed builtin eta-mode-list solver wrapper is also bootstrap-only:

- `SolveAmfOptionsEtaModeSeries(...)` takes the same eta solver inputs as `SolveBuiltinEtaModeListSeries(...)`, except the caller-supplied `const std::vector<std::string>& eta_mode_names` is replaced by `const AmfOptions& amf_options`
- it is a thin option-feed wrapper: it reads only `amf_options.amf_modes` and forwards that vector unchanged into `SolveBuiltinEtaModeListSeries(...)`
- the accepted behavior, validation, and fallback surface therefore remain exactly the reviewed ordered builtin-list semantics: caller/default order is preserved, empty lists still fail locally, unknown builtin names still stop selection immediately, final planning failures are preserved unchanged, and downstream eta-generated `DESystem` construction failures still do not trigger fallback
- non-`amf_modes` `AmfOptions` fields do not affect selection, diagnostics, solver invocation, or result shape at this seam
- this batch does not reinterpret any wider `AmfOptions` policy fields, does not add user-defined mode registration, does not add mixed builtin/user-defined fallback, and does not widen into cache, reducer, CLI, or broader orchestration behavior

The first mixed eta-mode single-name solver wrapper is also bootstrap-only:

- `SolveResolvedEtaModeSeries(...)` takes the same eta solver inputs as `SolveBuiltinEtaModeSeries(...)`, except builtin-only resolution is widened to `const std::vector<std::shared_ptr<EtaMode>>& user_defined_modes`
- it is a thin wrapper: it calls `ResolveEtaMode(eta_mode_name, user_defined_modes)`, then forwards the resolved mode directly into `SolveEtaModePlannedSeries(...)`
- resolution failures preserve the existing `ResolveEtaMode(...)` diagnostics unchanged and do not invoke the supplied solver
- downstream planning failures preserve the existing `EtaMode::Plan(...)` diagnostics unchanged and do not invoke the supplied solver
- downstream eta-generated `DESystem` construction failures also preserve the existing `SolveEtaModePlannedSeries(...)` / `SolveEtaGeneratedSeries(...)` diagnostics unchanged and do not invoke the supplied solver
- this batch does not add ordered list fallback, `AmfOptions` registry feed, CLI, cache policy, new builtin eta-mode semantics, or broader orchestration behavior

The first mixed eta-mode-list solver wrapper is also bootstrap-only:

- `SolveResolvedEtaModeListSeries(...)` takes the same eta solver inputs as `SolveResolvedEtaModeSeries(...)`, except `const std::string& eta_mode_name` is replaced by a caller-supplied ordered `const std::vector<std::string>& eta_mode_names`
- it is a narrow ordered-selection wrapper: it resolves builtin and user-defined names in caller order, probes planning in that same order, and carries the winning `EtaInsertionDecision` forward without re-planning the selected mode
- empty mixed-name lists fail locally with a deterministic argument error
- unknown-name or registry-validation failures from `ResolveEtaMode(...)` preserve the existing resolver diagnostics unchanged and stop selection immediately
- if no mode in the caller-supplied list reaches solve selection, the final planning failure from `EtaMode::Plan(...)` is preserved unchanged and the supplied solver is not invoked
- standard planning failures from `EtaMode::Plan(...)` are treated as ordered fallback misses until the caller-supplied list exhausts
- downstream eta-generated `DESystem` construction failures from the selected mode preserve the existing `SolveResolvedEtaModeSeries(...)` / `SolveEtaGeneratedSeries(...)` diagnostics unchanged and do not trigger fallback to later names
- this batch does not add `AmfOptions` default injection, CLI, cache policy, new builtin eta-mode semantics, or broader orchestration behavior

The first `AmfOptions`-fed mixed eta-mode solver wrapper is also bootstrap-only:

- `SolveAmfOptionsEtaModeSeries(...)` also exposes an overload that takes the same eta solver inputs as `SolveResolvedEtaModeListSeries(...)`, except the caller-supplied `const std::vector<std::string>& eta_mode_names` is replaced by `const AmfOptions& amf_options`
- it is a thin option-feed wrapper: it reads only `amf_options.amf_modes` and forwards that vector unchanged into `SolveResolvedEtaModeListSeries(...)`
- the accepted mixed resolution, validation, and fallback surface therefore remain exactly the reviewed ordered mixed-list semantics: caller/default order is preserved, the selected mode is planned at most once, empty lists still fail locally, resolver failures still stop selection immediately, standard planning failures still fall through in order until the list exhausts, final planning failures are preserved unchanged, and downstream eta-generated `DESystem` construction failures still do not trigger fallback
- non-`amf_modes` `AmfOptions` fields do not affect mixed selection, diagnostics, solver invocation, or result shape at this seam
- this batch does not reinterpret any wider `AmfOptions` policy fields, does not add CLI or cache behavior, and does not widen into broader orchestration

The first user-defined eta-mode resolver seam is also bootstrap-only:

- `ResolveEtaMode(...)` takes one eta-mode name plus a caller-supplied `const std::vector<std::shared_ptr<EtaMode>>& user_defined_modes`
- it validates the full supplied user-defined registry on every call before resolution proceeds: null entries, duplicate user-defined names anywhere in the registry, and user-defined names that collide with builtin eta-mode names anywhere in the registry fail locally with deterministic argument errors
- after registry validation, it is a one-name resolution hook: builtin names still resolve through the accepted builtin table when no user-defined mode claims the same name, while a unique non-builtin user-defined name returns the exact registered `EtaMode` instance unchanged
- resolution itself is name-only and does not call `EtaMode::Plan(...)`
- unresolved names preserve the existing `unknown eta mode: <name>` diagnostic surface
- this seam remains the typed runtime hook under the reviewed mixed solver wrappers and still does not itself add CLI, cache policy, or broader orchestration

The first user-defined ending-scheme resolver seam is also bootstrap-only:

- `ResolveEndingScheme(...)` takes one ending-scheme name plus a caller-supplied `const std::vector<std::shared_ptr<EndingScheme>>& user_defined_schemes`
- it validates the full supplied user-defined registry on every call before resolution proceeds: null entries, duplicate user-defined names anywhere in the registry, and user-defined names that collide with builtin ending-scheme names anywhere in the registry fail locally with deterministic argument errors
- after registry validation, it is a one-name resolution hook: builtin names still resolve through the accepted builtin table when no user-defined scheme claims the same name, while a unique non-builtin user-defined name returns the exact registered `EndingScheme` instance unchanged
- resolution itself is name-only and does not call `EndingScheme::Plan(...)`
- unresolved names preserve the existing `unknown ending scheme: <name>` diagnostic surface
- this seam does not yet produce boundary requests or couple ending planning into solver execution; it remains the typed runtime hook under the reviewed higher-level ending planners

The first single-name ending-scheme planning wrapper is also bootstrap-only:

- `PlanEndingScheme(...)` takes one ending-scheme name, a `ProblemSpec`, and a caller-supplied `const std::vector<std::shared_ptr<EndingScheme>>& user_defined_schemes`
- it is a thin wrapper: it calls `ResolveEndingScheme(...)`, then forwards the resolved scheme directly into `EndingScheme::Plan(...)`
- resolution failures preserve the existing `ResolveEndingScheme(...)` diagnostics unchanged
- planning failures preserve the existing `EndingScheme::Plan(...)` diagnostics unchanged
- current builtin ending schemes are deterministic placeholder planners only: they return `terminal_strategy = <builtin name>`, always include `<family>::eta->infinity` in `terminal_nodes`, and only builtin `Trivial` currently adds `<family>::trivial-region`
- this batch does not yet couple ending decisions into boundary providers, `DESystem`, solver execution, or CLI behavior
- this batch does not yet claim full upstream ending semantics for `Tradition`, `Cutkosky`, `SingleMass`, or `Trivial`

The first ordered ending-scheme selection wrapper is also bootstrap-only:

- `PlanEndingSchemeList(...)` takes the same planning inputs as `PlanEndingScheme(...)`, except `const std::string& ending_scheme_name` is replaced by a caller-supplied ordered `const std::vector<std::string>& ending_scheme_names`
- it is a narrow ordered-selection wrapper: it resolves builtin and user-defined names in caller order, probes planning in that same order, and carries the winning `EndingDecision` forward without re-planning the selected scheme
- empty ending-scheme lists fail locally with a deterministic argument error
- unknown-name or registry-validation failures from `ResolveEndingScheme(...)` preserve the existing resolver diagnostics unchanged and stop selection immediately
- if no scheme in the caller-supplied list reaches completion, the final planning failure from `EndingScheme::Plan(...)` is preserved unchanged
- standard planning failures from `EndingScheme::Plan(...)` are treated as ordered fallback misses until the caller-supplied list exhausts
- this batch does not yet couple ending decisions into boundary providers, `DESystem`, solver execution, or CLI behavior

The first `AmfOptions`-fed ending-scheme planning wrapper is also bootstrap-only:

- `PlanAmfOptionsEndingScheme(...)` takes the same planning inputs as `PlanEndingSchemeList(...)`, except the caller-supplied `const std::vector<std::string>& ending_scheme_names` is replaced by `const AmfOptions& amf_options`
- it is a thin option-feed wrapper: it reads only `amf_options.ending_schemes` and forwards that vector unchanged into `PlanEndingSchemeList(...)`
- the accepted resolution, validation, and fallback surface therefore remain exactly the reviewed ordered ending-list semantics: caller/default order is preserved, the selected scheme is planned at most once, empty lists still fail locally, resolver failures still stop selection immediately, standard planning failures still fall through in order until the list exhausts, and final planning failures are preserved unchanged
- non-`ending_schemes` `AmfOptions` fields do not affect ending selection, diagnostics, or result shape at this seam
- this batch does not reinterpret any wider `AmfOptions` policy fields and does not yet couple ending decisions into boundary providers, `DESystem`, solver execution, or CLI behavior

The first boundary-request and manual boundary-attachment seams are also bootstrap-only:

- `BoundaryRequest` is the typed manual boundary-request surface for the solver/runtime boundary; in this slice it carries only `variable`, `location`, and `strategy`, and single-request validation checks those fields plus declared-variable membership against the current `DESystem`
- Batch 32 removes `DESystem`-owned boundaries from the public contract: the reviewed `DESystem` surface is masters, variables, coefficient matrices, and singular points only
- `SolveRequest` now carries `boundary_requests` and `boundary_conditions` explicitly outside `DESystem`
- boundary requests remain explicit caller inputs in Batch 44: the library still does not infer requests from endings or eta modes
- `AttachManualBoundaryConditions(...)` is a thin attachment wrapper: it validates the current `SolveRequest` boundary-request list plus the caller-supplied explicit `BoundaryCondition` list, preserves non-boundary solve-request fields unchanged, preserves explicit boundary-condition order, and returns a copied `SolveRequest`
- duplicate request entries, missing explicit data, unmatched explicit data, start-location coverage gaps, strategy mismatches, boundary-value arity mismatches, and conflicting reattachment attempts surface as typed `boundary_unsolved` failures before numeric solving begins
- `BoundaryProvider` is a caller-supplied interface with one fixed `Strategy()` string and one `Provide(const DESystem&, const BoundaryRequest&)` hook that returns explicit `BoundaryCondition` data for one reviewed request at a time
- `AttachBoundaryConditionsFromProvider(...)` is a thin provider wrapper: it validates `SolveRequest.boundary_requests` deterministically first, rejects preexisting `boundary_conditions` as conflicting reattachment, requires every request strategy to match `provider.Strategy()`, calls `Provide(...)` once per request in request order, and then delegates the returned explicit boundary list back through `AttachManualBoundaryConditions(...)`
- provider-thrown `BoundaryUnsolvedError` values propagate unchanged, and provider-produced wrong variable/location/strategy data, empty values, wrong value counts, and duplicate loci all fail through the existing reviewed manual attachment validator rather than through duplicate provider-specific validation logic
- `GenerateBuiltinEtaInfinityBoundaryRequest(...)` is a pure library-only generator that takes `(const ProblemSpec&, const std::string& eta_symbol = "eta")`, validates `ProblemSpec` first, rejects empty `eta_symbol` as an argument error, and on the supported subset returns exactly `{variable = eta_symbol, location = "infinity", strategy = "builtin::eta->infinity"}`
- the supported Batch 45 subset is intentionally narrow: every propagator must be `Standard` and must have mass exactly `"0"`; well-formed specs outside that subset fail as typed `boundary_unsolved`, while malformed specs continue to fail as ordinary `invalid_argument` validation errors
- Batch 45 request generation is independent of ending planning and provider execution: it does not consume `EndingDecision`, does not look up or run any `BoundaryProvider`, does not compute boundary values or `BoundaryCondition` entries, and does not call the solver
- `GeneratePlannedEtaInfinityBoundaryRequest(...)` takes `(const ProblemSpec&, const std::string& ending_scheme_name, const std::vector<std::shared_ptr<EndingScheme>>& user_defined_schemes, const std::string& eta_symbol = "eta")`
- it is a thin single-name composition wrapper: it calls `PlanEndingScheme(...)` exactly once, accepts only the exact singleton supported terminal-node list `{<family>::eta->infinity}`, and then delegates into `GenerateBuiltinEtaInfinityBoundaryRequest(...)`
- ordinary ending resolution and planning failures preserve the existing `PlanEndingScheme(...)` diagnostics unchanged
- on success it returns the exact reviewed Batch 45 request shape `{variable = eta_symbol, location = "infinity", strategy = "builtin::eta->infinity"}`; `EndingDecision.terminal_strategy` is not reused as a boundary-request strategy
- any extra terminal node is currently unsupported and fails as typed `boundary_unsolved`, including the current builtin `Trivial` extra node `<family>::trivial-region`; missing the supported node or duplicating it also fails as typed `boundary_unsolved`
- `BootstrapSeriesSolver` now requires an explicit manual start boundary on its supported Batch 39 subset and returns typed `boundary_unsolved` for missing or incompatible explicit start-boundary attachment before continuation begins
- Batch 44 keeps the provider seam separate from solving: `BootstrapSeriesSolver` and `SolveDifferentialEquation(...)` remain unchanged and do not consult `BoundaryProvider`
- Batch 45 and Batch 46 still do not add builtin or registered boundary providers, ordered ending-list or `AmfOptions` wrappers, ending-to-extra-node generation beyond the exact supported singleton infinity node, eta-to-infinity boundary value computation, `BoundaryCondition` generation, `SolveRequest` construction, solver execution, or manual-versus-provider equivalence
- these batches also do not widen CLI behavior or claim any automatic boundary semantics beyond explicit request-and-attach behavior

The first numeric coefficient-evaluation seam is now reviewed:

- `EvaluateCoefficientExpression(...)` evaluates the current reviewed coefficient-string grammar exactly with rational arithmetic over literals, symbols, parentheses, unary sign, `+`, `-`, `*`, and `/`
- `EvaluateCoefficientMatrix(...)` selects one coefficient matrix by variable name from a reviewed `DESystem`, evaluates every cell at one explicit substitution point, and returns exact canonical rationals without mutating the source `DESystem`
- exact numeric bindings are still supplied as strings and are themselves parsed through the same exact rational grammar, so reviewed substitution values such as `-10/3` remain exact
- unknown variable names, unresolved symbols, and malformed expressions fail deterministically as argument errors; division by zero fails deterministically as plain evaluation failure without attempting singular-point classification
- this batch does not rewrite or canonicalize stored coefficient strings, does not classify singularities, does not generate local series, does not replace the scaffolded `BootstrapSeriesSolver`, and does not widen into automatic boundaries, continuation, or CLI behavior

The first singular-point detection and classification seam is now reviewed:

- `DetectFiniteSingularPoints(...)` analyzes one selected reviewed coefficient matrix for one selected variable and returns matrix-derived finite singular locations only
- this analysis is matrix-authoritative: `DESystem.singular_points` remains preserved legacy annotation/runtime metadata and is not treated as the computed singular set
- the seam reuses the reviewed exact evaluator for exact passive-symbol arithmetic and exact point resolution, while keeping Batch 35 finite-point-only and coarse: returned points are either detected finite singular locations or not, and classified points are `Regular` or `Singular`
- additive simple-pole terms are preserved across unsimplified sums, but Batch 35 support is defined on the exact canonical surviving expression after exact duplicate-term combination, zero-net cancellation, zero-term elimination, and matched-factor cancellation; grouped same-denominator normalization across multiple terms remains supported only when all surviving grouped numerators are linear, so grouped same-denominator linear-numerator cancellation participates before pole extraction/classification and regular cancellations do not false-positive as singular; surviving numerator-factor products remain supported only as single-term survivors when the surviving denominator support is regular or one finite simple pole, and any same-denominator denominator group is rejected as unsupported only when more than one surviving canonical term remains in that group and any surviving grouped numerator is nonlinear; exact additive cancellation of semantically identical dead branches and exact identical multi-term quotients whose shared expression itself stays within these reviewed supported shapes are eliminated to regular terms before unsupported-form rejection; zero numerators short-circuit only after parse-time symbol resolution and zero-divisor preservation, and divisor-side normalization or identical-quotient shared-expression normalization that proves zero still fails as plain `division by zero`; matched linear-factor cancellation removes removable singularities such as `(eta-s)/(eta-s)` from the computed finite singular set; the direct simple-difference divisor carveout is syntactic and limited to the literal `eta-c` shape needed for reviewed cases such as `1/(eta-s)`, so parenthesized or multi-term constant-only RHS forms under `eta-(...)` remain unsupported; exact-identical quotient collapse is the only allowed multi-term divisor exception, but shared higher-order poles, shared multi-factor denominators, and surviving non-identical multi-term divisors remain unsupported even inside `E/E`
- unknown variable names, missing passive bindings, malformed coefficient expressions, malformed point expressions, and unsupported singular-form analysis fail deterministically
- this batch does not widen the reviewed Batch 34 grammar, does not generate local series, does not add Frobenius/resonance data, does not perform continuation, does not replace the scaffolded `BootstrapSeriesSolver`, and does not add boundary generation or CLI behavior

The first scalar regular-point local-series patch seam is currently implemented and pending review:

- `GenerateScalarRegularPointSeriesPatch(...)` takes a reviewed `DESystem`, one selected `variable_name`, one `center_expression`, one non-negative `order`, and passive numeric bindings
- the seam is scalar-only in Batch 36: it requires exactly one master and a declared `1x1` coefficient matrix for the selected variable, and it rejects non-scalar systems or unsupported matrix shapes locally
- center resolution reuses the reviewed exact evaluator grammar, and `SeriesPatch.center` stores the resolved exact point expression `<variable>=<value>`
- regular-point gating reuses the reviewed `ClassifyFinitePoint(...)` seam at that resolved center; when Batch 35 rejects an otherwise exactly finite scalar center because the original divisor shape lies outside the reviewed Batch 35 grammar, Batch 36 re-checks `Regular` on a temporary exact-constant scalar probe instead of widening Batch 35 itself
- the returned `SeriesPatch` is normalized: `order` is preserved, `basis_functions` contains the degree-`0..order` monomials in the exact local shift `(x-c)` represented as repeated products, `coefficients.size() == order + 1`, and `coefficients[0] == "1"` on the happy path
- coefficient generation is exact and scalar-only: Batch 36 expands the selected scalar coefficient locally, applies the exact recurrence for `I' = a(x) I`, and runs an internal coefficient-level residual self-check through the generated regular-point order before returning
- negative orders, singular centers, malformed center expressions, unknown variable names, missing passive bindings, malformed coefficient expressions, and coefficient shapes whose local quotient would require negative powers or cancellation beyond the visible requested truncation fail deterministically
- this batch does not add triangular or block-matrix patch generation, Frobenius / regular-singular handling, overlap matching, continuation, standalone solver wrappers, `SolveRequest` changes, `BootstrapSeriesSolver` replacement, boundary generation, or CLI behavior

The first upper-triangular matrix regular-point local propagator seam is currently implemented and pending review:

- `GenerateUpperTriangularRegularPointSeriesPatch(...)` takes a reviewed `DESystem`, one selected `variable_name`, one `center_expression`, one non-negative `order`, and passive numeric bindings
- the seam is narrow in Batch 37: it requires a declared selected coefficient matrix that is square and dimension-matched to `masters.size()`, and it supports only systems that are already upper-triangular in the declared master order through the requested local degree
- center resolution and `basis_functions` reuse the reviewed Batch 36 exact grammar and monomial local-shift basis; `UpperTriangularMatrixSeriesPatch.center` stores the resolved exact point expression `<variable>=<value>`
- regular-point gating still reuses `ClassifyFinitePoint(...)` at that resolved center, and the same narrow raw-divisor fallback remains local: only when Batch 35 rejects the original divisor shape as unsupported does Batch 37 re-check `Regular` on a temporary exact-constant full-matrix probe at the resolved center instead of widening Batch 35 itself
- the returned `UpperTriangularMatrixSeriesPatch` is identity-normalized: `order` is preserved, `basis_functions` spans degree `0..order`, `coefficient_matrices.size() == order + 1`, and `coefficient_matrices[0]` is the exact identity matrix on the happy path
- coefficient generation is exact and matrix-valued: Batch 37 expands the selected matrix locally, rejects any strictly lower-triangular coefficient that survives through the requested order, applies the exact recurrence `(n+1) C_{n+1} = sum_{m=0}^n A_m C_{n-m}`, and runs an internal exact matrix residual self-check through degree `order-1` before returning
- negative orders, singular centers, malformed center expressions, unknown variable names, missing passive bindings, malformed coefficient expressions, unsupported local quotients, non-square or dimension-mismatched selected matrices, and surviving strictly lower-triangular local support fail deterministically
- this batch does not add automatic block discovery or permutation, general dense-matrix support, Frobenius / regular-singular handling, overlap matching, continuation, standalone solver wrappers, `SolveRequest` changes, `BootstrapSeriesSolver` replacement, boundary generation, or CLI behavior

The first scalar regular-singular / Frobenius local-series patch seam is currently implemented and reviewed:

- `GenerateScalarFrobeniusSeriesPatch(...)` takes a reviewed `DESystem`, one selected `variable_name`, one `center_expression`, one non-negative `order`, and passive numeric bindings
- the seam is scalar-only in Batch 41: it requires exactly one master and a declared `1x1` coefficient matrix for the selected variable, and it rejects non-scalar systems or unsupported matrix shapes locally
- center resolution reuses the reviewed exact evaluator grammar, and `ScalarFrobeniusSeriesPatch.center` stores the resolved exact point expression `<variable>=<value>`
- the seam is singular-only and local-simple-pole-gated: it rejects regular centers and higher-order poles locally from the exact Laurent expansion at the resolved center, accepts simple poles whose residue-stripped regular factor stays regular through the requested order, reuses `ClassifyFinitePoint(...)` when Batch 35 can classify the raw input directly, and still preserves the reviewed unsupported parenthesized direct-difference singular forms from Batch 35 such as `eta-(...)`, including internal-whitespace variants of that raw shape
- the returned `ScalarFrobeniusSeriesPatch` is normalized: `indicial_exponent` stores the exact simple-pole residue `rho`, `order` is preserved, `basis_functions` contains the degree-`0..order` monomials in the exact local shift `(x-c)` represented as repeated products, `coefficients.size() == order + 1`, and `coefficients[0] == "1"` on the happy path
- coefficient generation is exact and scalar-only: Batch 41 expands the selected scalar coefficient as `a(x) = rho / (x-c) + d(x)` around the resolved center, solves the reduced recurrence for `z_N` in `I_N(x) = (x-c)^rho z_N(x)`, and runs an internal exact residual self-check on the reduced regular factor through degree `order-1` before returning
- negative orders, regular centers, higher-order poles, malformed center expressions, unknown variable names, missing passive bindings, malformed coefficient expressions, and unsupported singular shapes fail deterministically
- this batch does not add logarithmic terms, resonance handling, multiple Frobenius branches, matrix or block singular patches, public Frobenius residual/overlap diagnostics, continuation changes, `SolveRequest` widening, passive-binding solve inputs, `BootstrapSeriesSolver` replacement, boundary generation, or CLI behavior

The first upper-triangular matrix regular-singular / Frobenius local propagator seam is currently implemented and reviewed:

- `GenerateUpperTriangularMatrixFrobeniusSeriesPatch(...)` takes a reviewed `DESystem`, one selected `variable_name`, one `center_expression`, one non-negative `order`, and passive numeric bindings
- the seam is narrow in Batch 42: it requires a declared selected coefficient matrix that is square and dimension-matched to `masters.size()`, and it supports only systems whose simple-pole residue matrix is already diagonal in the declared master order and whose residue-stripped regular tail is already upper-triangular through the requested local degree
- center resolution and `basis_functions` reuse the reviewed exact evaluator grammar and monomial local-shift basis; `UpperTriangularMatrixFrobeniusSeriesPatch.center` stores the resolved exact point expression `<variable>=<value>`
- the seam is singular-only and local-simple-pole-gated: it rejects regular centers and higher-order poles locally from the exact Laurent expansion at the resolved center, accepts only per-cell Laurent order `>= -1`, reuses `ClassifyFinitePoint(...)` when Batch 35 can classify the raw input directly, and still preserves the reviewed unsupported parenthesized direct-difference singular forms from Batch 35 such as `eta-(...)`, including internal-whitespace variants of that raw shape
- the returned `UpperTriangularMatrixFrobeniusSeriesPatch` is normalized: `indicial_exponents` stores the exact diagonal simple-pole residues in declared master order, `order` is preserved, `basis_functions` contains the degree-`0..order` monomials in the exact local shift `(x-c)` represented as repeated products, `coefficient_matrices.size() == order + 1`, and `coefficient_matrices[0]` is the exact identity matrix on the happy path
- coefficient generation is exact and matrix-valued on the reviewed diagonal-residue subset: Batch 42 expands the selected matrix locally as `A(x) = D / (x-c) + sum_{m>=0} B_m (x-c)^m`, requires `D` to be diagonal, rejects any strictly lower-triangular `B_m` that survives through the requested order, applies the exact reduced recurrence `(n+1) C_{n+1} + C_{n+1} D - D C_{n+1} = sum_{m=0}^n B_m C_{n-m}`, and runs an internal exact reduced-equation self-check through degree `order-1` before returning
- compatible resonances are deterministic and narrow: when one recurrence denominator entry vanishes and the exact right-hand side entry is also zero, Batch 42 sets that coefficient entry to zero and continues; when the denominator vanishes but the right-hand side does not, Batch 42 rejects deterministically as requiring logarithmic resonance handling / logarithmic Frobenius terms
- negative orders, regular centers, higher-order poles, malformed center expressions, unknown variable names, missing passive bindings, malformed coefficient expressions, unsupported singular shapes, non-square or dimension-mismatched selected matrices, off-diagonal residue simple poles, surviving strictly lower-triangular regular-tail support, and forced logarithmic resonances fail deterministically
- this batch does not add dense or automatically discovered block decomposition, Jordan or off-diagonal residue support, explicit logarithmic basis functions or multiple Frobenius branches, public Frobenius residual/overlap diagnostics, continuation changes, `SolveRequest` widening, passive-binding solve inputs, `BootstrapSeriesSolver` replacement, boundary generation, or CLI behavior

The first exact regular-patch residual and overlap diagnostics seams are currently implemented and pending review:

- `EvaluateScalarSeriesPatchResidual(...)` takes one reviewed scalar `DESystem`, one selected `variable_name`, one already-generated `SeriesPatch`, one explicit `point_expression`, and passive numeric bindings, then returns the exact scalar residual `p'(x) - a(x) p(x)` as `ExactRational`
- `MatchScalarSeriesPatches(...)` takes one selected `variable_name`, two compatible scalar `SeriesPatch` values, one explicit `match_point_expression`, one distinct explicit `check_point_expression`, and passive numeric bindings, then returns exact `lambda` and exact `mismatch` where `lambda = p_right(match) / p_left(match)` and `mismatch = p_right(check) - lambda * p_left(check)`
- `EvaluateUpperTriangularMatrixSeriesPatchResidual(...)` takes one reviewed `DESystem`, one selected `variable_name`, one already-generated `UpperTriangularMatrixSeriesPatch`, one explicit `point_expression`, and passive numeric bindings, then returns the exact matrix residual `Y'(x) - A(x) Y(x)` as `ExactRationalMatrix`
- `MatchUpperTriangularMatrixSeriesPatches(...)` takes one selected `variable_name`, two compatible `UpperTriangularMatrixSeriesPatch` values, one explicit `match_point_expression`, one distinct explicit `check_point_expression`, and passive numeric bindings, then returns the exact `match_matrix` and exact `mismatch` where `match_matrix = Y_right(match) * inverse(Y_left(match))` and `mismatch = Y_right(check) - match_matrix * Y_left(check)`
- the Batch 38 seams stay library-only and exact: they consume already-generated regular patches, parse points with the reviewed exact evaluator grammar, keep residual and mismatch outputs exact instead of floating, and do not consult `SolveRequest`, `PrecisionPolicy`, norms, tolerances, or continuation policy
- the seam validates public patch storage narrowly before use: scalar diagnostics require non-negative `order`, `basis_functions.size() == order + 1`, `coefficients.size() == order + 1`, and an exactly resolved `patch.center`; matrix diagnostics require the analogous size checks, an exactly resolved `patch.center`, square stored coefficient matrices of one consistent dimension, and matching matrix dimensions across overlap pairs or against `masters.size()` for residual checks
- scalar residual evaluation still requires a reviewed scalar `1x1` system matrix, and matrix residual evaluation still requires a reviewed selected coefficient matrix that is square and dimension-matched to `masters.size()`
- match and check points are caller-supplied and must be distinct after exact resolution; malformed point expressions, missing passive bindings, unknown selected variable names, malformed public patch centers, malformed patch storage sizes, and matrix dimension mismatches fail deterministically
- singular residual or overlap evaluations are not reclassified: if the selected coefficient matrix is singular at the requested residual point, if `p_left(match)` vanishes, or if `Y_left(match)` is singular, Batch 38 propagates the underlying exact `division by zero` failure directly instead of adding singular-path logic
- this batch does not add regular-patch continuation, automatic point selection, overlap norms or tolerances, `SolveRequest` integration, `BootstrapSeriesSolver` replacement, dense/non-triangular matrix diagnostics, Frobenius / regular-singular handling, boundary generation, or CLI behavior

The first exact one-hop continuation solver seam is currently implemented through the reviewed Batch 39 regular path plus the reviewed Batch 43 mixed extension:

- `BootstrapSeriesSolver::Solve(...)` keeps the public `SeriesSolver` / `SolveRequest` surface unchanged and supports only two exact one-hop paths on the reviewed upper-triangular subset: the reviewed Batch 39 regular-start to regular-target path and the reviewed Batch 43 regular-start to regular-singular-target mixed path
- the solver currently requires a well-formed `DESystem` with exactly one declared differentiation variable and one explicit manual start boundary attached through the existing `boundary_requests` plus `boundary_conditions` surface
- `start_location` and `target_location` are parsed exactly through the reviewed coefficient-evaluator grammar; malformed location expressions, malformed explicit boundary values, and unresolved symbols remain deterministic argument errors instead of solver-level failure codes
- the internal continuation order is fixed at `4`; Batch 39 and Batch 43 both choose `match = (start + target) / 2` and `check = (3*start + target) / 4` deterministically after exact point resolution
- the reviewed regular path reuses `GenerateUpperTriangularRegularPointSeriesPatch(...)`, `MatchUpperTriangularMatrixSeriesPatches(...)`, and `EvaluateUpperTriangularMatrixSeriesPatchResidual(...)` directly, with scalar systems treated only as the degenerate `1x1` upper-triangular case
- the reviewed Batch 43 mixed path keeps the same explicit manual start-boundary requirement and start-side regular patch seam, then generates the target-side singular patch internally with `GenerateUpperTriangularMatrixFrobeniusSeriesPatch(...)` on the reviewed Batch 42 diagonal-residue, no-log subset
- the reviewed Batch 43 mixed path is narrower than the reviewed Batch 42 local patch seam: continuation is attempted only when every target Frobenius indicial exponent is an exact integer, and fractional-exponent mixed requests return `failure_code = "unsupported_solver_path"` with an integral/integer-Frobenius-exponent summary instead of widening continuation semantics
- both supported paths use the deterministic `match`/`check` pair for exact acceptance; the regular path requires exact regular residuals plus exact overlap mismatch to vanish, while the mixed path requires the exact start-side regular residual, exact target-side Frobenius residual, and exact regular/Frobenius handoff mismatch to vanish entrywise
- on success the solver returns `diagnostics.success = true`, `residual_norm = 0.0`, `overlap_mismatch = 0.0`, empty `failure_code`, and a deterministic short success summary, while transported target values and the mixed handoff matrix remain internal and are not exposed on the public surface
- well-formed but unsupported or inexact one-hop requests return `failure_code = "unsupported_solver_path"`, including singular starts, unsupported matrix shape/support outside the reviewed upper-triangular subset, forced logarithmic resonances, fractional-exponent mixed targets, singular internal one-hop evaluations, and nonzero exact residual or handoff checks
- `PrecisionPolicy` and `requested_digits` remain accepted on `SolveRequest` but are explicitly unused on the reviewed exact Batch 39 and Batch 43 subsets
- these batches do not add singular-start boundary semantics, singular-to-regular or singular-to-singular continuation, multi-hop continuation, passive-binding solve inputs, precision-escalation semantics, automatic boundary generation, public Frobenius residual or handoff diagnostics, or public transported-target output

The first standalone differential-equation solver wrapper seam is currently implemented and pending review:

- `SolveDifferentialEquation(const SolveRequest& request)` is a library-only thin wrapper over the reviewed Batch 39 and Batch 43 exact continuation solver surface
- the wrapper constructs the default solver via `MakeBootstrapSeriesSolver()`, invokes `Solve(request)` once, and returns the resulting `SolverDiagnostics` unchanged
- it does not add any new request or result type, transported-target output, malformed-input translation, or new well-formed solving semantics; the reviewed Batch 39 and Batch 43 exceptions and diagnostics pass through unchanged
- this batch does not rewire existing injected solver wrappers, add no-request overloads, or widen into singular-start boundary semantics, broader singular/Frobenius continuation, multi-hop continuation, passive-binding solve inputs, precision-policy semantics, CLI behavior, or examples

The first auxiliary-family transformation seam is also intentionally narrow:

- `EtaInsertionDecision` now carries selected propagator indices as the canonical selection surface; copied propagator expressions remain informational only in this bootstrap
- `ApplyEtaInsertion(...)` returns a typed transformed-spec result and never mutates the input `ProblemSpec`
- only the selected propagators are rewritten, and the bootstrap rewrite is deterministic string-level logic of the form `(<old expression>) + eta`
- `kinematics.invariants` appends `eta` exactly once and preserves existing order otherwise
- the transform preserves family name, targets, top sectors, scalar-product rules, numeric substitutions, and propagator `kind`/`prescription`
- empty selections, duplicate indices, out-of-range indices, selected auxiliary propagators, and selected propagators with `mass != "0"` fail locally with deterministic diagnostics
- builtin eta mode `All` selects all non-auxiliary propagators by index; builtin modes `Prescription`, `Mass`, `Propagator`, `Branch`, and `Loop` are still explicit bootstrap stubs and fail as not implemented
- this batch does not generate derivatives, does not implement full builtin eta-mode semantics beyond `All`, and does not add file-backed eta manifests

## Upgrade Rules

- additive fields are allowed in specs and manifests
- existing field meaning cannot change silently
- new runtime modes require parity and diagnostics coverage
- new reducer backends must implement the same `ReductionBackend` contract
