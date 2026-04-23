# Public Contract Bootstrap

This document defines the first stable C++ interface boundary for the port.

The Batch 32 and Batch 33 notes below remain the current reviewed boundary
split. The Batch 34 and Batch 35 notes record the current reviewed
coefficient-evaluation and singular-point seams. The Batch 36 note records the
current reviewed scalar regular-point local-series seam, and the Batch 37 note
records the current reviewed upper-triangular matrix regular-point patch seam.
The Batch 38 note records the current reviewed exact regular-patch residual and
overlap diagnostics. The Batch 39 note records the current reviewed exact
one-hop regular-point continuation solver surface on the reviewed
upper-triangular subset, without overclaiming later singular-path or multi-hop
solver semantics. The Batch 40 note records the current reviewed standalone
library-only default-solver wrapper over that reviewed Batch 39 continuation
surface. The Batch 41 note records the current reviewed scalar
regular-singular / Frobenius patch seam. The Batch 42 note records the current
reviewed upper-triangular matrix regular-singular / Frobenius patch seam on
the diagonal-residue, no-log subset, and the Batch 43 note records the current
reviewed exact mixed regular-start to regular-singular-target continuation
slice on the integer-exponent Frobenius subset. The Batch 44 note
records the current reviewed caller-supplied boundary-provider seam that
maps explicit `BoundaryRequest` entries to explicit `BoundaryCondition` data
without changing the solver path, and the Batch 45 note records the current
reviewed pure builtin `eta -> infinity` boundary-request generator over a
validated `ProblemSpec`. The Batch 46 note records the current reviewed
single-name ending-planned wrapper over that reviewed Batch 45 generator.

## Current Durable Status

- the last fully accepted public/runtime surface remains the reviewed `Batch 1` through `Batch 50a`
  boundary, carried on clean `main@bbd7b744b69a413bf34e4b706cd737e2b266256a`
- the actual SSH remote `main` state for this packet now runs through
  `main@2125db50adf91efb5033c5c4472b1792158dc48f`; that landed history includes `Batch 50b` at
  `95f33f398bbdebf2084bf360a498fea3de89fc30`, `Batch 50` through `Batch 58g` at
  `b40b0dccb1d286b287e2fcb45e5e554901223d63`, `08220d2569d1a60c9181f53d5e809f334dcfcd4e`,
  `95c2ebf6f7f7adb713c04625d9fccd3c1266eeb8`, `0f623d65e7e933d464deef3da4ea02efaf57a535`,
  `23b64404680fe0c5425d2261f6e776bd1f197794`, `4dcb17f6a4fd9d2ebf28e72922e74c06fb461d82`,
  `56e4f96d03b0b54f541122c0d59b2ed0cefc2b98`, `48686b6590df1f1c52f760913129f1bf0ad3ad0b`,
  `a5d627f906dfb2c5829bda88dce2407bfa67f043`, `2f2538b`, `7d3806a`, and `53ec6a4`, the landed
  Kira rational-function prefactor surface at `ab4a311`, the landed xints `insert_prefactors`
  packet at `b367daf`, landed `Batch 58h` at `53a6630`, landed `Batch 58i` at `9b619f1`, and the
  landed narrow `Milestone M3` closure packet at `7dee2a0`, plus the later landed narrow
  not-yet-accepted public/runtime seams described below through `Batch 64k`; latest head commit
  `2125db5` is the narrow `Batch 62i` raw `t`-segment continuation guardrail packet and does not
  widen the last fully accepted public/runtime contract;
  the current worktree now also carries the local-only narrow `Batch 62j` raw single-invariant
  `msq`-segment singular-crossing continuation guardrail packet, which likewise does not widen
  the last fully accepted public/runtime contract;
  the current worktree also carries the local-only narrow `Batch 64a` through `Batch 64k`
  linear-propagator packets, which likewise stay below the last fully accepted public/runtime
  contract;
  local tracking `origin/main` matches that same head;
  durable clean-candidate evidence is recorded here for `Batch 50b` via job `5482487` for candidate
  `/n/holylabs/schwartz_lab/Lab/obarrera/autonomousIBP-artifacts/candidates/b50b-final-clean-candidate-20260413T133615Z-775743d3`,
  and `Batch 50b` remains internal-only despite being landed on `main`
- current `main` now also carries the landed narrow public/runtime packets that follow that bridge:
  `Batch 50` adds the first supported `Branch` / `Loop` selector slice on the single-top-sector
  squared-linear-momentum subset, `Batch 51` adds ordered multi-invariant list wrappers over the
  reviewed one-invariant automatic path, `Batch 52` widens invariant-generated seed construction
  to invariant-independent identifier or rational-constant masses on the bootstrap subset,
  `Batch 53` adds first multiple-top-sector Kira target orchestration, `Batch 54` adds
  precision-budget preflight plus a generated-wrapper retry controller while direct
  `SolveDifferentialEquation(...)` remains passthrough, landed `Batch 55` adds typed
  generated-wrapper `master_set_instability` plus `continuation_budget_exhausted` diagnostics,
  landed `Batch 56` adds solved-path cache replay, landed `Batch 57` adds wrapper-only
  `SkipReduction` reuse, landed `Batch 58` wires the live `AmfOptions` solver-policy fields,
  and landed `Batch 58g` adds target-aware Kira reduction-span widening on exact-sector
  positive-support matches plus the first mandatory-family retained-span evidence packet
- `Milestone M1` is complete on that reviewed surface
- `Milestone M0a` is accepted as cluster/reference-harness bootstrap readiness only; it does not
  imply captured reference outputs, completed benchmark comparisons, or upstream parity claims
- `Milestone M0b` is accepted on the required phase-0 benchmark set only: retained root
  `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260419-required-set`,
  initial packet job `6721330` completed the `automatic_vs_manual` primary before walltime,
  resumed packet job `6732338` completed the packet via `--resume-existing`, and both required
  benchmark comparison summaries now pass
- the repo now also carries narrow M6 harness helpers only:
  `tools/reference-harness/scripts/qualification_readiness.py` aggregates the accepted M0b root
  plus the reviewed optional retained packet roots into one machine-readable evidence summary,
  validates that every observed captured phase-0 example still publishes promoted
  golden/result-manifest artifacts plus passing comparison summaries, and keeps the blocked
  `next_runtime_lane` hints visible for the remaining uncaptured phase-0 examples and the
  singular-endpoint qualification anchor.
  `tools/reference-harness/scripts/compare_phase0_results_to_reference.py` is then the first
  actual benchmark comparator on that retained packet shape only: it compares one candidate packet
  root against one retained reference packet root through exact canonical output-name/hash
  agreement while surfacing the frozen scaffold threshold/failure/regression metadata.
  `tools/reference-harness/scripts/score_phase0_correct_digits.py` is then the first narrow
  packet-level correct-digit scorer on that same retained packet shape only: it keeps the
  retained output-name set and nonnumeric canonical-text skeleton fixed, scores only approximate
  Mathematica numeric literals tokenwise against the frozen digit-threshold profiles, and leaves
  exact symbolic outputs structural-only on this reviewed path.
  `tools/reference-harness/scripts/compare_phase0_packet_set_to_reference.py` then composes that
  comparator across the retained `required-set`, `de-d0-pair`, and `user-hook-pair` packet split,
  requiring one unique reference packet label per pair, requiring each candidate packet root to
  publish exactly the retained benchmark split for that packet through `result-manifest.json`
  entries while ignoring uncaptured placeholder directories without manifests, and requiring the
  compared benchmark ids to match the scaffold's full current `reference-captured` phase-0 set
  exactly while preserving the same threshold/failure/regression metadata.
  `tools/reference-harness/scripts/score_phase0_packet_set_correct_digits.py` then composes the
  reviewed packet-level correct-digit scorer across that same retained packet split, requiring one
  unique reference packet label per pair, requiring each candidate packet root to publish exactly
  the retained benchmark split for that packet, and preserving the same threshold/failure/
  regression metadata alongside one machine-readable packet-set score summary that truthfully
  keeps the current retained packet split short of a full qualification verdict.
  `tools/reference-harness/scripts/audit_phase0_failure_codes.py` is then the first packet-level
  candidate failure-code audit on that same packet shape only: it consumes one candidate packet
  root on the existing manifest/run schema plus optional
  `results/phase0/<benchmark>/failure-code-audit.json` sidecars, surfaces the frozen required
  failure-code profile per benchmark, and reports which required codes are still missing or
  unexpectedly extra on the published candidate audit path.
  `tools/reference-harness/scripts/audit_phase0_packet_set_failure_codes.py` then composes that
  reviewed packet-level failure-code audit across the retained `required-set`, `de-d0-pair`, and
  `user-hook-pair` packet split, requiring one unique candidate packet label per root, requiring
  each candidate packet root to publish exactly the packet-summary benchmark split for that
  packet, and requiring the audited benchmark ids to match the scaffold's full current
  `reference-captured` phase-0 set exactly while preserving the same threshold/failure/regression
  metadata.
  `tools/reference-harness/scripts/qualify_phase0_packet_set.py` is then the first retained
  phase-0 packet-set qualification verdict over that same reviewed split: it consumes the
  reviewed `qualification_readiness.py` summary plus the packet-set comparison, correct-digit,
  and failure-code summaries, fail-closes unless the retained packet labels and captured phase-0
  ids stay synchronized across those prerequisite reports, and writes one blocked/pass verdict
  over the reviewed phase-0 packet set only.
  `tools/reference-harness/scripts/qualification_case_study_readiness.py` is then the first
  machine-readable case-study-family consumer of the same scaffold: it validates the selected
  literature anchors, parity labels, digit floors, failure/regression profiles, and the reviewed
  singular `next_runtime_lane` blocker plus its landed predecessor anchor against the frozen
  sources before later case-study qualification lanes widen into real numerics.
  Together these remain harness-only plumbing: they do not launch the C++ runtime, the comparator
  and scorer helpers still do not inspect candidate failure-code behavior, the packet-level and
  packet-set failure-code audits check only published audit sidecars against the frozen scaffold,
  the retained phase-0 packet-set qualification verdict keeps case-study numerics and full
  `Milestone M6` closure explicitly withheld, and none of them claim that `Milestone M6` is
  passing
- current worktree now also carries a narrow M7-groundwork follow-on release scaffold only:
  `tools/reference-harness/templates/release-signoff-checklist.json` extends the landed
  `docs/release-signoff-checklist.md` packet with the first machine-readable prerequisite/docs/
  diagnostics/performance/parity review buckets for later harness-side consumers. This is
  planning metadata only: it does not run any release review, does not claim that `Milestone M6`
  or `Milestone M7` is passing, and does not widen the last fully accepted public/runtime
  contract
- current worktree now also carries the first executable M7 helper on top of that scaffold only:
  `tools/reference-harness/scripts/release_signoff_readiness.py` consumes one machine-readable
  `qualification_readiness.py` summary plus the release-signoff checklist, audits the checklist
  source/doc targets inside the repo, preserves the blocked `b61n` / `b62n` / `b63k` / `b64k`
  runtime-lane frontier from the retained M6 evidence packet, and writes one blocked
  release-readiness summary. The current worktree now also lets that helper optionally consume
  the retained phase-0 packet-set qualification verdict so the release-readiness summary preserves
  phase-0 correct-digit and failure-code blockers explicitly, plus one performance-review summary
  sidecar so timing/scope/rebuild review blockers remain visible to M7, plus one
  diagnostic-review summary sidecar so typed-failure review blockers remain visible to M7, plus
  one docs-completion sidecar so docs-alignment blockers remain visible to M7. This remains
  release-prep plumbing only: it does not mark `Milestone M6` or `Milestone M7` complete, does
  not run performance, diagnostic, or docs completion review, and does not widen the last fully
  accepted public/runtime contract
- current worktree now also carries the first docs-completion sidecar producer for that M7
  scaffold only: `tools/reference-harness/scripts/review_release_docs_completion.py` audits the
  release-signoff checklist source paths, docs-completion target set, target marker anchors, and
  explicit non-claims, then writes the consumer-compatible `release-docs-completion` summary for
  `release_signoff_readiness.py`. This remains harness/release-prep bookkeeping only: it does
  not claim `Milestone M6` closure, `Milestone M7` closure, release readiness, new captured
  benchmark evidence, or any widened runtime/public behavior
- current worktree now also carries the first performance-review sidecar producer for that M7
  scaffold only: `tools/reference-harness/scripts/review_release_performance.py` audits the
  release-signoff checklist performance-review input/output contract plus the qualification
  scaffold benchmark-family scope, then writes the consumer-compatible
  `release-performance-review` summary for `release_signoff_readiness.py`. This remains
  harness/release-prep bookkeeping only: it does not run benchmark timings, does not review clean
  rebuild output for performance, does not claim `Milestone M6` closure, `Milestone M7` closure,
  release readiness, or any widened runtime/public behavior
- current worktree now also carries the first diagnostic-review sidecar producer for that M7
  scaffold only: `tools/reference-harness/scripts/review_release_diagnostic.py` audits the
  release-signoff checklist diagnostic-review input/output contract plus the qualification
  scaffold required failure-code and known-regression metadata, then writes the
  consumer-compatible `release-diagnostic-review` summary for `release_signoff_readiness.py`.
  This remains harness/release-prep bookkeeping only: it does not run runtime diagnostics, does
  not review retained unstable-run evidence, does not claim `Milestone M6` closure,
  `Milestone M7` closure, release readiness, or any widened runtime/public behavior
- `Operational Gate B0/G1` is accepted; GNU 8 `std::filesystem` linkage is restored and the clean
  `sapphire` build/test gate is green
- `K0-pre-spec` is accepted as a repo-local K0 smoke fixture freeze derived from preserved input;
  latest candidate-local smoke replay job `5356840` passed
- `K0-pre` is accepted as a narrow Kira kinematics YAML contract repair for that frozen smoke
  subset; latest clean-candidate build/test job `5356948` passed
- `K0b.1` is accepted on the frozen repo-local K0 smoke subset: clean-candidate job `5425248`
  passed, packet job `5425379` passed on `sapphire`, and the retained root
  `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/k0/reducer-smoke` is coherent and
  complete
- `K0` and `K0b` are therefore accepted only for that frozen repo-local K0 smoke subset: one
  coherent retained reducer-smoke packet with an honest file-backed bootstrap manifest now exists
  on `main`
- `Batch 47` / `Milestone M2` are now accepted narrowly as behavioral-equivalence evidence over
  that already-reviewed `Batch 46` surface, not as a new runtime/API seam: on the supported
  simple Euclidean massless sample subset, builtin `Tradition` plus one exact user-defined
  singleton `<family>::eta->infinity` path agree between manual and automatic attachment, and the
  reviewed pre-solve failures remain preserved with solver non-invocation
- `Batch 48` is accepted on `main`: final accepted clean-candidate `sapphire` job `5439311`
  cleared the landing packet and commit `f4bf8af2419a20f04ae40eceebbd5d12f3b2a92c` is the
  prior authoritative clean baseline
- `Batch 49` is accepted on `main`: commit `b0275a8d8ce3f33577629f44d7b168b4d4ef8bb2` landed the
  narrow builtin `Propagator` structural-selector packet
- `Batch 49b` is accepted on `main`: local module-loaded `cmake -S . -B build`,
  `cmake --build build --parallel 1`, and `ctest --test-dir build --output-on-failure` passed in
  `/tmp/autoIBP-b49b-mass`; final clean-candidate `sapphire` job `5457143` passed for candidate
  `/n/holylabs/schwartz_lab/Lab/obarrera/autonomousIBP-artifacts/candidates/b49b-final-clean-candidate-20260413T112519Z-Kabcrq`;
  and second-pass rereview cleared with no blocking or medium findings remaining
- `Batch 50a` is accepted on `main`: local module-loaded `cmake -S . -B build`,
  `cmake --build build --parallel 1`, and `ctest --test-dir build --output-on-failure` passed;
  final clean-candidate `sapphire` job `5465841` passed for candidate
  `/n/holylabs/schwartz_lab/Lab/obarrera/autonomousIBP-artifacts/candidates/b50a-final-clean-candidate-20260413T123155Z-1gWbow`;
  second-pass rereview cleared with no findings; and commit
  `bbd7b744b69a413bf34e4b706cd737e2b266256a` is the current clean release baseline
- the public contract on `main` still widens only narrowly: the accepted K0 smoke subset remains
  only the repo-local frozen fixture derived from preserved input plus the narrow `K0-pre`
  kinematics-YAML repair and accepted `K0b.1` bootstrap-manifest packet; the accepted `Batch 47` /
  `Milestone M2` packet remains evidence over the existing request / provider / solver seams rather
  than a new public surface; `Batch 48` adds only the narrow bootstrap `Prescription` alias over
  reviewed `All`; `Batch 49` adds only the narrow builtin `Propagator` structural selector over
  the reviewed local candidate surface; `Batch 49b` adds only the narrow local `Mass` selector plus
  the minimal eta-generated-path mass-coherence widening; `Batch 50a` adds only internal
  eta-topology preflight blocker telemetry for still-blocked `Branch` / `Loop` without truthful
  selector semantics; and `M0b` now supplies retained upstream goldens for the required phase-0
  benchmark set without widening the C++ runtime/API surface or accepting later parity milestones
- the durably evidenced but still-internal `Batch 50b` packet on current `main` is narrower still:
  it keeps `Branch` and `Loop` blocked, adds only an internal topology-prerequisite bridge/prereq
  snapshot over the current family/kinematics surface, and reports explicit available versus
  missing fields instead of fake selector behavior
- the landed `Batch 50` packet on current `main` is still narrow even where it succeeds: it is limited to the
  single-top-sector squared-linear-momentum subset, uses deterministic local size-then-declaration
  ordering plus repeated-first-choice deduplication, and still does not claim multi-top-sector
  coverage, broader propagator grammar support, linear-propagator support, full upstream
  `AnalyzeTopSector` / component-factorization parity, or `Propagator::prescription`
  interpretation
- landed `Batch 58e` adds only a narrow tests/docs packet on the resolved/user-defined
  plain `UseCache` replay path for `SolveAmfOptionsEtaModeSeries(...)`:
  `tests/amflow_tests.cpp` seeds one successful solved-path run, checks that the solved-path
  manifest records the resolved `solve_kind` and request fingerprint truthfully, and then
  verifies that a matching plain `UseCache` replay returns cached `SolverDiagnostics` without
  invoking the live solver. This does not widen runtime behavior, broader cache/restart
  semantics, or standalone `SolveDifferentialEquation(...)` / runtime-policy parity. The
  prefactor groundwork through landed `Batch 58d` plus the later landed Kira
  rational-function prefactor surface / xints `insert_prefactors` packets remains history only;
  landed `Batch 58g` supplies the retained `tt` reduction-span evidence packet, landed
  `Batch 58h` guards the same-path `tt` widening lane, landed `Batch 58i` supplies retained
  `automatic_loop` `box1` / `box2` stage-1 and stage-2 reduction-span/order evidence, and landed
  `main@7dee2a0` extends those retained `tt`, `box1`, and `box2` seams to compare normalized
  `integralfamilies.yaml` and `kinematics.yaml` against capture. Together with landed
  `Batch 58d` prefactor lock coverage, this closes `Milestone M3` narrowly on the first
  mandatory package families only
- landed `Batch 58f` adds exact-subset tests plus mirrored docs and records the remaining direct
  precision-monotonicity gap as closed on the current reviewed exact subset only:
  `tests/amflow_tests.cpp` already includes
  `BootstrapSeriesSolverExactSubsetRequestedDigitsMonotonicityTest()` and
  `SolveDifferentialEquationExactSubsetRequestedDigitsMonotonicityTest()`, both using
  `ExpectRequestedDigitsMonotonicityOnReviewedExactSubset(...)` to check under-cap invariance on
  the requested-digits ladder `{11, 73, 145, 290}` over the reviewed exact scalar, exact
  upper-triangular, mixed scalar, mixed upper-triangular diagonal, and mixed upper-triangular
  zero-forcing-resonance paths. This is under-cap diagnostic invariance on the direct exact
  `BootstrapSeriesSolver` subset plus one representative `SolveDifferentialEquation(...)`
  passthrough over the same reviewed cases. The exact path still ignores precision fields under
  that cap, so this is not a broader monotone digit-refinement claim; the matching hard-ceiling
  threshold failure remains covered by `BootstrapSeriesSolverRejectsDigitsAboveConfiguredCeilingTest()` and
  `SolveDifferentialEquationInsufficientPrecisionPassthroughTest()`. With landed `Batch 54`
  through `Batch 58e` failure-code, cache, `UseCache`, and
  `SkipReduction` coverage, this closes `Milestone M4` narrowly on that implemented exact subset
  only. It does not widen runtime behavior, broader cache/restart semantics, or broader
  runtime-policy parity. The separate landed `Milestone M3` closure on landed `main@7dee2a0`
  remains limited to the first mandatory package families only
- broader automatic boundary execution/provider parity, broader ending semantics, broader Kira
  smoke, upstream `automatic_vs_manual` parity, full upstream topology/component `Mass` parity,
  truthful builtin `Branch` / `Loop` selector semantics, graph-polynomial availability,
  `AnalyzeTopSector` parity, and any `Propagator::prescription` metadata interpretation are still
  outside the last fully accepted public boundary
- that landed `Batch 49b` packet is still narrow: it adds only a bootstrap builtin `Mass` seam
  over the reviewed local candidate surface plus the minimal eta-generated-path coherence widening
  needed to keep selected equal-mass reducer-facing literals aligned with planner grouping. The
  remaining accepted gaps stay explicit: the independence preference is only a local token-based
  heuristic, selected-mass normalization is only outer-whitespace trimming on rewritten selected
  propagators, there is no broader topology/component-order parity, no broader same-priority
  tie-break parity, and no broader symbolic mass canonicalization claim. It does not by itself
  close `M0b` or imply broader upstream parity
- current worktree `Batch 59a`, `Batch 59b`, `Batch 59d`, `Batch 59e`, `Batch 59f`,
  `Batch 59g`, `Batch 59h`, and `Batch 60a` through `Batch 60k` are still narrow: the two
  `SolveAmfOptionsEtaModeSeries(...)` overloads may now carry wrapper-owned `amf_requested_d0`
  plus derived `amf_requested_dimension_expression` metadata on `SolveRequest`, solved-path
  request fingerprints, request summaries, and D0-sensitive cache identity now distinguish that
  metadata so stale cache replays are rejected when only `AmfOptions::d0` changes. The direct
  exact `BootstrapSeriesSolver` / `SolveDifferentialEquation(...)` path may additionally consume
  an exactly numeric `SolveRequest.amf_requested_dimension_expression` as a passive
  `dimension` binding during coefficient evaluation, center classification, residual checks, and
  explicit boundary-value parsing, and may derive passive exact `eps` only when both
  `amf_requested_d0` and `amf_requested_dimension_expression` evaluate exactly. When that same
  request-owned dimension expression stays symbolic, the direct exact solver now rewrites
  assembled standalone `dimension` identifiers in the `DESystem` onto the normalized symbolic
  carrier before the reviewed exact path runs, while manual boundary values stay exact-only. On
  the reviewed wrapper path, an exact `AmfOptions.fixed_eps` may
  collapse the derived `D0 - 2*eps` carrier down to an exact numeric dimension expression and
  participates in solved-path cache identity; when that derived carrier is itself exact, the same
  two wrappers now also pass
  `-sd=<dimension>` into live eta-generated Kira execution and record that exact override in
  wrapper-owned prepared-state validation so stale `skip_reduction` reuse is rejected across
  exact-dimension changes. The direct public eta-generated execution / `DESystem` helpers now also
  accept one explicit public dimension expression: exactly evaluable expressions prepend the same
  live `-sd=<dimension>` argument, while symbolic expressions keep reducer launch unchanged but
  rewrite assembled standalone `dimension` identifiers onto the normalized symbolic expression
  after parse/assembly. The direct public eta-generated solver handoffs, the planned-decision
  `AmfOptions` eta-mode helper, and the direct builtin / resolved / outer `AmfOptions`
  eta-mode wrappers now all accept that same explicit public dimension expression. Exactly
  evaluable expressions preserve the reviewed live `-sd=<dimension>` reducer override, while
  symbolic expressions keep reducer launch and prepared-state validation exact-only but now rewrite
  assembled standalone `dimension` identifiers onto the normalized symbolic expression before
  solver execution, including wrapper-owned live and `skip_reduction` assembly, while preserving
  the normalized symbolic request metadata on
  `SolveRequest.amf_requested_dimension_expression`. The same rewrite now also applies when the
  wrappers derive a symbolic `D0 - 2*eps` carrier from `AmfOptions.d0` / `fixed_eps` without an
  explicit public dimension-expression overload, and solved-path cache slot/input identity carries
  a dedicated symbolic rewrite epoch for those derived carriers so stale pre-`Batch 59h` artifacts
  fall back to live execution. This still falls well short of full `Batch 59` / `Batch 60`:
  broader Kira preparation artifacts, reducer-facing symbolic dimension overrides, and broader
  arbitrary symbolic runtime behavior remain deferred
- current worktree `Batch 61a` through `Batch 64k` are still narrow: explicit complex kinematics
  now stop at exact-complex evaluation plus reviewed contour-plan persistence; any non-opt-in
  solver surface still defers with explicit `unsupported_solver_path` diagnostics, and the
  reviewed helper deferred/cache replay remains limited to that non-opt-in subset, while the
  direct eta-generated, eta-mode-planned, and planned `AmfOptions` helper handoffs now add one
  still narrower live path only for injected solvers that explicitly opt into the reviewed
  complex-continuation surface by consuming a persisted `EtaContinuationPlan` on `SolveRequest`;
  the reviewed K0
  one-mass real guardrails now cover only the explicit `s`/`t` continuation-segment diagnostics
  described below; the reviewed local phase-space slice now narrows builtin `Prescription` to
  standard uncut `-i0` propagators (`Batch 63b`), emits explicit Kira `cut_propagators`
  (`Batch 63c`), adds Cutkosky phase-space request/attach wrappers, and adds a caller-supplied
  provider-registry seam without phase-space boundary values; the current worktree now also adds
  typed per-loop `family.loop_prescriptions` parsing/validation/serialization plus pure
  `DerivePropagatorPrescriptionFromLoopPrescriptions(...)` collapse logic, and the reviewed
  Cutkosky request/attach seam now consults that metadata only to refine builtin phase-space
  provider selection on cut propagators when one uniform loop-derived/raw-matching prescription
  survives. This still does not widen builtin `Prescription` selection, top-sector or Cutkosky
  topology analysis, or automatic phase-space boundary-value generation;
  and explicit linear variants now reach only the reviewed invariant seed/execution subsets, the
  stricter general Kira-preparation subset, the direct eta-generated plus eta-mode-planned solver
  handoffs on the reviewed direct-decision subset where only reviewed quadratic propagators are
  eta-shifted and the explicit linear slot stays passive, the narrow passive-linear
  `Prescription` wrapper surface through direct builtin, builtin-list, builtin-only `AmfOptions`,
  mixed/user-defined `AmfOptions` fallback, and the matching planned-`AmfOptions` helper tail on
  that same reviewed subset, plus one pure
  `BuildReviewedLightlikeLinearAuxiliaryPropagator(...)` helper plus the new
  `ApplyReviewedLightlikeLinearAuxiliaryTransform(...)` full-spec transform that rewrites one
  explicit `variant: "linear"` propagator on the one-external lightlike `n*n = 0` subset into
  one quadratic `x*((L)^2) + (original)` driver, preserves the original mass/prescription and
  every non-targeted `ProblemSpec` field, and appends the chosen `x` invariant only when absent
  while leaving the input `ProblemSpec` unchanged. Live complex contour execution, broader
  phase-space topology/provider behavior, broader non-invariant linear solver behavior, any
  Kira/preparation/reduction/solver-path consumer of that lightlike linear-driver transform,
  broader AMFlow-faithful `x` / gauge-link linear-driver parity, and wider symbolic runtime
  parity remain deferred

## Core Types

- `ProblemSpec`: family definition, propagators, cuts, conservation rules, invariants,
  prescriptions, targets, exact numeric substitutions, raw complex numeric substitutions, and
  dimensional settings
- `ProblemSpec` propagators now also admit an optional typed `variant` keyword with the frozen
  vocabulary `{quadratic, linear}`. On the current reviewed B64a surface it is preserved through
  file-backed YAML and resolved by `EffectivePropagatorVariant(...)`, but any explicit `variant`
  must still agree with the legacy `PropagatorKind`-backed linearity encoding because downstream
  eta-mode consumers have not yet migrated off `PropagatorKind::Linear`; the reviewed automatic
  invariant seed / preparation / execution / `DESystem` / solver path now keys on
  `EffectivePropagatorVariant(...)` for one invariant-independent linear-propagator subset only,
  while general `KiraBackend` preparation is now the first non-invariant consumer of explicit
  `variant: "linear"` on a stricter reviewed subset that requires the relevant external-symbol
  scalar-product surface to stay on identifier-free rational-constant expressions with nonzero
  denominators only; the direct eta-generated preparation / execution / `DESystem` seams now also
  consume that same explicit-variant subset only when a caller-supplied direct eta decision
  rewrites reviewed quadratic propagators and leaves the explicit linear slot passive, and the
  direct eta-generated plus eta-mode-planned solver handoffs now preserve that same reviewed
  direct-decision subset without widening builtin eta-mode selection or broader non-invariant
  linear solver behavior
- `FamilyDefinition.loop_prescriptions` plus
  `DerivePropagatorPrescriptionFromLoopPrescriptions(...)`: the first typed per-loop prescription
  metadata seam on the phase-space lane. File-backed `ProblemSpec` loading now preserves an
  optional integer list whose parsed values must canonicalize to the frozen loop-prescription
  vocabulary `-1`, `0`, or `1`; non-empty lists must stay parallel to `family.loop_momenta`,
  while an explicit empty list canonicalizes to omitted metadata. The pure collapse helper then
  inspects the referenced loop identifiers in one propagator expression and returns `PlusI0`,
  `MinusI0`, `None`, or unresolved `std::nullopt` by preserving one uniform surviving nonzero
  loop sign, ignoring zero-prescription loops when one nonzero sign survives, returning `None`
  only when every matched loop stays at explicit `0`, and rejecting mixed surviving nonzero
  signs or expressions that mention no declared loop momentum. The first live consumer remains
  narrow: on the reviewed Cutkosky phase-space boundary-request path only, cut propagators may
  consult this metadata to refine builtin provider strategy selection when every cut propagator
  derives the same prescription and that derived value matches the raw cut
  `Propagator::prescription`; broader builtin `Prescription`, top-sector / Cutkosky topology
  analysis, and automatic phase-space boundary-value generation remain deferred
- `AmflowLoopPrefactorSign`, `AmflowPrefactorConvention`, and `BuildOverallAmflowPrefactor(...)`: the first explicit in-repo prefactor/sign-convention helper surface, rendering a deterministic textual overall AMFlow prefactor from declared loop count plus cut propagator count without mutating the input `ProblemSpec`; the current default literals are frozen narrowly by `specs/amflow-prefactor-reference.yaml` and the human-readable mirror `references/snapshots/amflow/prefactor_convention_lock.md`, with retained-root backing for the `+i0` loop and cut prefactors while the explicit `-i0` loop-prefactor literal remains repo-snapshot backed only
- `KiraInsertPrefactorEntry`, `KiraInsertPrefactorsSurface`, `ValidateKiraInsertPrefactorsSurface(...)`, and `SerializeKiraInsertPrefactorsSurface(...)`: a deterministic repo-local Kira `insert_prefactors` surface over xints-like denominator entries, frozen by `specs/kira-insert-prefactors-surface.yaml` and `references/snapshots/kira/insert_prefactors_surface_lock.md`; validation rejects empty entry lists, empty families, cross-entry family mismatches, empty denominators, newline-containing denominators, and a first-entry denominator other than exact `"1"`, while serialization renders one line per entry as `<integral.Label()>*1/(<denominator>)\n`. This surface is intentionally distinct from `BuildOverallAmflowPrefactor(...)`, does not reuse that overall AMFlow loop-prefactor helper, and now feeds a narrow default-disabled `KiraBackend`/`jobs.yaml` emission path only when `ReductionOptions.kira_insert_prefactors == true`, an explicit `KiraInsertPrefactorsSurface` is supplied, the active `ReductionMode` emits `run_firefly`, the selected target list has exactly one integral, the family has no cut propagators, and the current family/arity/anchor validation passes. Explicit public emission calls through `KiraBackend::EmitJobFiles(...)` and `EmitJobFilesForTargets(...)` reject invalid opt-in requests deterministically instead of silently suppressing `xints`, while `Prepare(...)` and `PrepareForTargets(...)` preserve bootstrap preparation behavior by recording validation messages and omitting the companion file
- `AmfOptions`: AMFlow runtime controls, including optional exact `fixed_eps` metadata on the
  reviewed wrapper-owned `D0 -> dimension` carrier
- `ReductionOptions`: backend and reducer controls
- `DESystem`: ordered masters, differentiation variables, exact coefficient matrices, singular-point annotations
- `BoundaryRequest`, `BoundaryCondition`, `BoundaryUnsolvedError`, `AttachManualBoundaryConditions(...)`, `BoundaryProvider`, `AttachBoundaryConditionsFromProvider(...)`, and `AttachBoundaryConditionsFromProviderRegistry(...)`: typed boundary request, explicit boundary data, failure, manual attachment surface, and caller-supplied provider/provider-registry seams decoupled from `DESystem`-owned boundary storage
- `GenerateBuiltinEtaInfinityBoundaryRequest(...)`: pure builtin boundary-request generation over a validated bootstrap `ProblemSpec` subset, returning one explicit `BoundaryRequest` without boundary values or solver execution
- `GeneratePlannedEtaInfinityBoundaryRequest(...)`: single-name ending-planned wrapper that accepts only the exact singleton `<family>::eta->infinity` terminal-node decision and then returns the reviewed builtin `eta -> infinity` `BoundaryRequest`
- `GenerateAmfOptionsEndingSchemeEtaInfinityBoundaryRequest(...)`: standalone `AmfOptions::ending_schemes` eta->infinity boundary-request selector that preserves the reviewed ordered fallback semantics without attaching boundary data or invoking the solver
- `GenerateBuiltinCutkoskyPhaseSpaceBoundaryRequest(...)`, `GeneratePlannedCutkoskyPhaseSpaceBoundaryRequest(...)`, and `GenerateAmfOptionsEndingSchemeCutkoskyPhaseSpaceBoundaryRequest(...)`: standalone reviewed Cutkosky phase-space boundary-request generators over the same narrow standard/cut-only subset, without boundary values or provider registries
- `PlanBuiltinAmfOptionsEtaMode(...)`: standalone builtin-only `AmfOptions::amf_modes` eta-mode decision helper that performs only reviewed ordered builtin selection and planning, returning the winning `EtaInsertionDecision` without touching solver policy, cache, `skip_reduction`, or `D0` metadata
- `PlanAmfOptionsEtaMode(...)`: standalone `AmfOptions::amf_modes` mixed eta-mode decision helper that performs only reviewed ordered mixed builtin/user-defined selection and planning, returning the winning `EtaInsertionDecision` without touching solver policy, cache, `skip_reduction`, or `D0` metadata
- `ExactRational`, `ExactComplexRational`, `BuildComplexNumericEvaluationPoint(...)`, `EvaluateCoefficientExpression(...)`, `EvaluateComplexCoefficientExpression(...)`, `EvaluateCoefficientMatrix(...)`, `EvaluateComplexCoefficientMatrix(...)`, and `EvaluateComplexPointExpression(...)`: exact rational coefficient evaluation plus a separate exact-complex helper layer over one explicit substitution point, including merged `ProblemSpec` exact/complex kinematic bindings and standalone complex point-expression parsing
- `SeriesPatch` plus `GenerateScalarRegularPointSeriesPatch(...)`: the first scalar-only regular-point local-series patch seam over one selected reviewed `DESystem` variable
- `ScalarFrobeniusSeriesPatch` plus `GenerateScalarFrobeniusSeriesPatch(...)`: the first scalar-only regular-singular / Frobenius local-series patch seam over one selected reviewed `DESystem` variable
- `UpperTriangularMatrixSeriesPatch` plus `GenerateUpperTriangularRegularPointSeriesPatch(...)`: the first upper-triangular matrix regular-point local propagator seam over one selected reviewed `DESystem` variable
- `UpperTriangularMatrixFrobeniusSeriesPatch` plus `GenerateUpperTriangularMatrixFrobeniusSeriesPatch(...)`: the first upper-triangular matrix regular-singular / Frobenius local propagator seam over one selected reviewed `DESystem` variable on the diagonal-residue, no-log subset
- `ScalarSeriesPatchOverlapDiagnostics`, `EvaluateScalarSeriesPatchResidual(...)`, and `MatchScalarSeriesPatches(...)`: exact scalar patch residual and overlap diagnostics over already-generated regular patches
- `UpperTriangularMatrixSeriesPatchOverlapDiagnostics`, `EvaluateUpperTriangularMatrixSeriesPatchResidual(...)`, and `MatchUpperTriangularMatrixSeriesPatches(...)`: exact upper-triangular matrix patch residual and overlap diagnostics over already-generated regular patches
- `SolveRequest`, `SolverDiagnostics`, `SeriesSolver`, `BootstrapSeriesSolver`, `MakeBootstrapSeriesSolver()`, and `SolveDifferentialEquation(...)`: the library-only exact one-hop continuation solver surface plus default bootstrap-solver construction and standalone wrapper over one declared reviewed `DESystem` variable with explicit manual start-boundary attachment, covering the reviewed regular/regular path and the reviewed Batch 43 mixed regular-start to regular-singular-target path on the integer-exponent Frobenius subset; `SolveRequest` may also carry an optional wrapper-owned `AmfSolveRuntimePolicy`, `amf_requested_d0`, explicit-or-derived `amf_requested_dimension_expression`, and on the current worktree one optional reviewed `eta_continuation_plan` that is attached only by the live complex eta solver wrappers after manifest persistence. `SeriesSolver` also exposes one reviewed capability hook so injected solvers may opt into that live complex eta plan handoff while the default bootstrap solver keeps the deferred path unchanged. On the current worktree, the same reviewed dimension-expression execution surface now spans both direct and generated solver entry points: exactly numeric `amf_requested_dimension_expression` values remain passive exact `dimension` bindings and may derive passive exact `eps` only when both `amf_requested_d0` and `amf_requested_dimension_expression` evaluate exactly, while symbolic expressions rewrite assembled standalone `dimension` identifiers in the live `DESystem` onto the normalized symbolic carrier before solver execution instead of remaining request metadata only. Manual boundary values stay on the reviewed exact-only parsing path. Generated eta-mode solver handoffs still keep live reducer `-sd=<dimension>` plus wrapper-owned prepared-state validation exact-only, but both explicit public symbolic expressions and wrapper-derived symbolic `D0 - 2*eps` carriers now rewrite the assembled eta-generated `DESystem` before solver execution, including the reviewed `AmfOptions` wrapper-owned live and `skip_reduction` assembly paths. Broader Kira preparation artifacts, reducer-facing symbolic overrides, and broader arbitrary symbolic runtime parity remain deferred
- `SolveRequest`, `SolverDiagnostics`, `SeriesSolver`, `BootstrapSeriesSolver`, `MakeBootstrapSeriesSolver()`, and `SolveDifferentialEquation(...)`: the library-only exact one-hop continuation solver surface plus default bootstrap-solver construction and standalone wrapper over one declared reviewed `DESystem` variable with explicit manual start-boundary attachment, covering the reviewed regular/regular path and the reviewed Batch 43 mixed regular-start to regular-singular-target path on the integer-exponent Frobenius subset; `SolveRequest` may also carry an optional wrapper-owned `AmfSolveRuntimePolicy`, `amf_requested_d0`, explicit-or-derived `amf_requested_dimension_expression`, and on the current worktree one optional reviewed `eta_continuation_plan` that is attached only by the live complex eta solver wrappers after manifest persistence. `SeriesSolver` also exposes one reviewed capability hook so injected solvers may opt into that live complex eta plan handoff, while the default bootstrap solver still does not execute complex contours: direct requests that carry `eta_continuation_plan` now fail closed with explicit `unsupported_solver_path` diagnostics carrying the contour fingerprint instead of silently ignoring the plan. On the current worktree, the same reviewed dimension-expression execution surface now spans both direct and generated solver entry points: exactly numeric `amf_requested_dimension_expression` values remain passive exact `dimension` bindings and may derive passive exact `eps` only when both `amf_requested_d0` and `amf_requested_dimension_expression` evaluate exactly, while symbolic expressions rewrite assembled standalone `dimension` identifiers in the live `DESystem` onto the normalized symbolic carrier before solver execution instead of remaining request metadata only. Manual boundary values stay on the reviewed exact-only parsing path. Generated eta-mode solver handoffs still keep live reducer `-sd=<dimension>` plus wrapper-owned prepared-state validation exact-only, but both explicit public symbolic expressions and wrapper-derived symbolic `D0 - 2*eps` carriers now rewrite the assembled eta-generated `DESystem` before solver execution, including the reviewed `AmfOptions` wrapper-owned live and `skip_reduction` assembly paths. Broader Kira preparation artifacts, reducer-facing symbolic overrides, and broader arbitrary symbolic runtime parity remain deferred
- `PrecisionPolicy`: precision and stability controls
- `AmfSolveRuntimePolicy`: narrow typed carrier for the currently wrapper-owned `AmfOptions` runtime fields `ExtraXOrder`, `LearnXOrder`, `TestXOrder`, and `RunLength`
- `ArtifactManifest`: reproducibility metadata for reducer-run artifacts
- `SolvedPathCacheManifest`: deterministic solved-path cache metadata for the reviewed `UseCache` replay slice on the `AmfOptions` eta solve wrappers, including the current-worktree replayable deferred complex-continuation diagnostics on the planned helper path and the direct eta-generated handoff path
- `ParsedMasterList` and `ParsedReductionResult`: deterministic typed views of Kira `masters` and `kira_target.m` artifacts for the bootstrap reducer boundary; parsed coefficient normalization strips Kira-local `prefactor[...]` and `prefactor(...)` wrappers before the expressions flow into the evaluator-facing reduction result
- `ReducedDerivativeVariableInput` plus `AssembleReducedDESystem(...)`: the first typed ingestion path from already-reduced derivative targets into a `DESystem`
- `GeneratedDerivativeVariable` plus `GenerateEtaDerivativeVariable(...)`: typed eta-only unreduced derivative rows built from the accepted auxiliary-family transform
- `InvariantDerivativeSeed` plus `GenerateInvariantDerivativeVariable(...)`: typed invariant unreduced derivative rows generated from explicit precomputed denominator-derivative expressions
- `BuildInvariantDerivativeSeed(...)`: typed one-invariant-at-a-time construction of `InvariantDerivativeSeed` from a validated bootstrap `ProblemSpec` subset
- `GeneratedDerivativeVariableReductionInput` plus `AssembleGeneratedDerivativeDESystem(...)`: typed ingestion of generated rows plus parsed reductions into a `DESystem`
- `EtaGeneratedReductionPreparation` plus `PrepareEtaGeneratedReduction(...)`: typed eta-generated target preparation from the accepted auxiliary-family and eta-derivative seams into the Kira reducer boundary
- `InvariantGeneratedReductionPreparation` plus `PrepareInvariantGeneratedReduction(...)`: typed invariant-generated target preparation from the accepted invariant-derivative seam into the Kira reducer boundary, with overloads for either a precomputed `InvariantDerivativeSeed` or one invariant name on a validated bootstrap `ProblemSpec`
- `InvariantGeneratedReductionBatchPreparation` plus `PrepareInvariantGeneratedReductionList(...)`: typed ordered multi-invariant preparation that batches generated variables and one shared reducer packet over the original `ArtifactLayout`
- `EtaGeneratedReductionExecution` plus `RunEtaGeneratedReduction(...)`: typed eta-only orchestration over preparation, execution, parsing, and generated-row assembly, with a narrow overload that also accepts one explicit public dimension expression alongside `eta_symbol`
- `BuildEtaGeneratedDESystem(...)`: the first library-only eta-generated `DESystem` consumer over the reviewed eta-generated execution wrapper, with a matching narrow public dimension-expression overload
- `InvariantGeneratedReductionExecution` plus `RunInvariantGeneratedReduction(...)`: typed invariant-only orchestration over preparation, execution, parsing, and generated-row assembly, with overloads for either a precomputed `InvariantDerivativeSeed` or one invariant name on a validated bootstrap `ProblemSpec`
- `BuildInvariantGeneratedDESystem(...)`: the first library-only one-invariant `DESystem` consumer over the reviewed automatic invariant-generated execution wrapper
- `BuildInvariantGeneratedDESystemList(...)`: the first ordered multi-invariant `DESystem` consumer over the reviewed batched automatic invariant-generated preparation/execution path, returning one combined assembled multi-variable `DESystem`
- `SolveInvariantGeneratedSeries(...)`: the first library-only one-invariant solver handoff from the reviewed automatic invariant-generated `DESystem` consumer into an injected `SeriesSolver`
- `SolveInvariantGeneratedSeriesList(...)`: the first ordered multi-invariant solver handoff over the reviewed batched automatic invariant-generated `DESystem` consumer
- `SolveEtaGeneratedSeries(...)`: the first library-only eta-generated solver handoff from the reviewed eta-generated `DESystem` consumer into an injected `SeriesSolver`, with a matching narrow public dimension-expression overload
- `SolveEtaModePlannedSeries(...)`: the first library-only eta-mode-planned solver handoff that composes `EtaMode::Plan(...)` with the reviewed eta-generated solver wrapper, with a matching narrow public dimension-expression overload
- `SolvePlannedAmfOptionsEtaModeSeries(...)`: standalone planned-decision `AmfOptions` eta-mode execution helper over the already-landed wrapper-owned live policy, cache, `skip_reduction`, and requested-`D0` metadata tail
- `SolveBuiltinEtaModeSeries(...)`: the first builtin eta-mode-name library-only solve wrapper that resolves `MakeBuiltinEtaMode(...)` and reuses the reviewed eta-mode-planned solver handoff, with a matching narrow public dimension-expression overload
- `SolveBuiltinEtaModeListSeries(...)`: the first caller-supplied ordered builtin eta-mode-list library-only solve wrapper that selects the first planning-successful builtin and reuses the reviewed single-name builtin solver handoff, with a matching narrow public dimension-expression overload
- `SolveAmfOptionsEtaModeSeries(...)`: the first `AmfOptions`-fed eta-mode solver-wrapper surface, with reviewed builtin-only and mixed builtin/user-defined overloads plus matching narrow public dimension-expression overloads
- `SolveAmfOptionsEndingSchemeEtaInfinitySeries(...)`: the first narrow `AmfOptions::ending_schemes`-fed eta->infinity boundary attach-and-solve wrapper that delegates through the standalone `GenerateAmfOptionsEndingSchemeEtaInfinityBoundaryRequest(...)` seam, then uses a caller-supplied `BoundaryProvider` and injected `SeriesSolver`
- `SolveAmfOptionsEndingSchemeCutkoskyPhaseSpaceSeries(...)`: the narrow `AmfOptions::ending_schemes`-fed Cutkosky phase-space boundary attach-and-solve wrapper family that delegates through the standalone `GenerateAmfOptionsEndingSchemeCutkoskyPhaseSpaceBoundaryRequest(...)` seam, then uses either one caller-supplied `BoundaryProvider` or a caller-supplied provider registry plus an injected `SeriesSolver`
- `SolveResolvedEtaModeSeries(...)`: the first single-name library-only eta-mode solver wrapper that resolves one name against builtin plus user-defined registrations and then reuses the reviewed eta-mode-planned solver handoff, with a matching narrow public dimension-expression overload
- `SolveResolvedEtaModeListSeries(...)`: the first ordered mixed builtin-plus-user-defined eta-mode-list library-only solve wrapper that probes planning in caller order and carries the winning planned eta decision forward without re-planning, with a matching narrow public dimension-expression overload
- `ResolveEtaMode(...)`: the first builtin-or-user-defined one-name eta-mode resolver hook over the existing `EtaMode` interface
- `ResolveEndingScheme(...)`: the first builtin-or-user-defined one-name ending-scheme resolver hook over the existing `EndingScheme` interface
- `PlanEndingScheme(...)`: the first single-name ending-scheme planning wrapper that resolves one ending name and returns a typed `EndingDecision`
- `PlanEndingSchemeList(...)`: the first ordered builtin-plus-user-defined ending-scheme selection wrapper that probes planning in caller order and carries the winning ending decision forward without re-planning
- `PlanAmfOptionsEndingScheme(...)`: the first `AmfOptions`-fed ending-scheme planner wrapper that reads only `AmfOptions::ending_schemes` and reuses the reviewed ordered ending-selection wrapper
- `EtaInsertionDecision` plus `ApplyEtaInsertion(...)`: the first typed auxiliary-family transformation seam from an immutable `ProblemSpec` to an eta-shifted auxiliary family
- `BuildReviewedLightlikeLinearAuxiliaryPropagator(...)`: the first pure one-propagator lightlike gauge-link linear-driver helper in `runtime/auxiliary_family`, rewriting one explicit `variant: "linear"` propagator on the reviewed one-external `n*n = 0` subset into one quadratic auxiliary-family driver without mutating the input `ProblemSpec`
- `ApplyReviewedLightlikeLinearAuxiliaryTransform(...)`: the first full-spec consumer of that same reviewed helper, rewriting one explicit `variant: "linear"` propagator on the reviewed one-external `n*n = 0` subset, preserving all non-targeted `ProblemSpec` fields, recording the rewritten propagator index plus trimmed `x_symbol`, and appending that invariant only when absent without mutating the input `ProblemSpec`

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

The bootstrap CLI supports a deterministic YAML subset for `ProblemSpec` loading. The supported
shape matches the checked-in example spec: nested `family`, `kinematics`, and `targets` sections;
bracketed scalar lists; block lists for propagators, preferred masters, scalar-product rules, and
targets; scalar maps for exact `numeric_substitutions` and raw
`complex_numeric_substitutions`; and top-level `dimension`, `complex_mode`, and `notes`.

Within `family`, the current reviewed `Batch 63j` surface also accepts an optional
`loop_prescriptions` bracketed integer list. Canonical loading accepts integer spellings whose
parsed values are the frozen loop-prescription vocabulary `-1`, `0`, and `1`; equivalent forms
such as `+1` or `00` therefore parse and canonicalize back to `1` and `0`. Non-empty lists must
match the `loop_momenta` arity, and an explicit empty list is treated as omitted metadata and
canonicalized back out of the serialized YAML.

Within each propagator entry, the current reviewed B64a surface also accepts an optional typed
`variant` scalar with the frozen keywords `"quadratic"` or `"linear"`. The file-backed loader
preserves that field through canonicalization, but loaded-spec validation still requires any
explicit `variant` to agree with the legacy `kind`-backed linearity encoding because downstream
eta-mode/runtime consumers still key on `PropagatorKind::Linear`, even though the reviewed
automatic invariant seed / preparation / execution / `DESystem` / solver path now consumes
`EffectivePropagatorVariant(...)` on one narrow linear subset, and general `KiraBackend`
preparation now consumes explicit `variant: "linear"` on a stricter reviewed subset whose
relevant external-symbol scalar-product surface is limited to identifier-free rational-constant
expressions with nonzero denominators.

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

The bootstrap manifest surface is intentionally split:

- `write-manifest <dir>` writes a sample/demo manifest only; it is not a file-backed K0 packet and
  must not be treated as retained reducer evidence
- `run-kira-from-file <file> <kira> <fermat> [dir]` writes `manifests/bootstrap-run.yaml` from the
  actual file-backed run facts for that artifact root
- the file-backed manifest records the loaded spec provenance boundary, exact spec path,
  deterministic spec fingerprint, family name, target count, artifact root, execution working
  directory, explicit Kira/Fermat executable paths, rendered command text, execution status,
  exit code, repository commit/status snapshot when available, and the exact family-results tree
  that the parser contract would consume
- when reducer outputs already exist under both accepted parser roots, the manifest records the
  same preferred family-results tree that the parser uses: the most complete tree, with ties
  favoring `generated-config/results/<family>/`
- `non_default_options` records only actual deviations from the bootstrap reducer defaults; the
  snapshot defaults such as `IntegralOrder=5` and `ReductionMode=Kira` are not reported there as
  if they were overrides

The solved-path cache surface is also intentionally narrow:

- successful live solves through the two `SolveAmfOptionsEtaModeSeries(...)` overloads persist one
  `SolvedPathCacheManifest` under `layout.root/cache/solved-paths/<slot>.yaml`; on the current
  worktree, the shared planned helper and the direct eta-generated complex deferral path also
  persist one reviewed deferred complex-continuation diagnostic there on distinct complex-only
  slots
- the slot is deterministic from the wrapper kind, `spec.family.name`, the selected
  `EtaInsertionDecision.mode_name`, `eta_symbol`, and optional wrapper-owned
  `amf_requested_d0`, so D0-only changes do not alias the same solved-path cache slot; the
  reviewed deferred complex-continuation helper path also appends the dedicated
  `complex-continuation-deferred-v1` cache epoch so those deferred complex artifacts do not alias
  the live real-kinematics slots, and that epoch is the explicit replay-version fence for later
  replay-affecting contour-planning or eta-generated-system changes on the non-`skip_reduction`
  deferred-complex path
- the manifest records an input fingerprint over the actual wrapper-owned solve inputs:
  `ProblemSpec`, the ordered parsed master basis, the selected `EtaInsertionDecision`,
  `ReductionOptions`, the concrete supplied `SeriesSolver` type used for replay, start and target
  locations, `requested_digits`, the full live `PrecisionPolicy`, and the optional live
  `AmfSolveRuntimePolicy`; it also records a post-build `SolveRequest` fingerprint plus a short
  request summary that stay truthful to wrapper-owned requested-`D0` metadata, and the reviewed
  deferred complex helper path additionally folds in the planned contour fingerprint before replay
- `amf_options.use_cache == true` enables replay only on those two `AmfOptions` eta wrappers:
  matching successful manifests replay the stored `SolverDiagnostics` without invoking the
  supplied solver; on the current reviewed deferred complex-continuation subset, the same cache
  flag may also replay the stored `unsupported_solver_path` diagnostics only while the matching
  continuation-plan manifest still exists under `layout.manifests_dir/` and still carries the
  cached contour fingerprint. On the non-`skip_reduction` deferred-complex path, that replay
  happens before rebuilding the live reduced `DESystem`, so the dedicated
  `complex-continuation-deferred-v1` cache epoch is the version fence for later replay-affecting
  contour-planning or eta-generated-system changes on that path. The direct
  `SolveEtaGeneratedSeries(...)` complex-deferral path now reuses the same deferred-diagnostic
  replay shape unconditionally on matching complex-only cache artifacts because that direct public
  handoff has no separate cache-control flag
- when `amf_options.skip_reduction == true` is also set on those wrappers, replay is still gated
  on rebuilding and validating the current wrapper-owned prepared state first, so missing or
  mismatched prepared reducer inputs or reduction artifacts fail explicitly instead of replaying
  stale solved-path output; the replay validation request fingerprint includes the same live
  wrapper-owned `PrecisionPolicy` and `AmfSolveRuntimePolicy` inputs
- stale, malformed, version-mismatched, or otherwise non-replayable cache manifests are rejected
  explicitly by falling back to live execution; incompatible artifacts are never replayed
- this is not an interruption-resume or reducer-state restart surface: it replays only previously
  recorded successful `SolverDiagnostics` on matching solved-path inputs, plus the one reviewed
  deferred complex-continuation `unsupported_solver_path` diagnostic shape described below
- successful live solves still refresh the slot even when `amf_options.use_cache == false`, so a
  later matching `amf_options.use_cache == true` call may reuse that solved-path diagnostic
  artifact; publication is atomic and best-effort, so refresh failures leave any previously
  published manifest untouched

The current `SkipReduction` reuse surface is also intentionally narrow:

- `amf_options.skip_reduction == true` is honored only on the two
  `SolveAmfOptionsEtaModeSeries(...)` overloads
- after wrapper-owned eta-mode selection succeeds, the wrapper rebuilds the current
  eta-generated preparation, requires matching prepared reducer inputs under the current
  `ArtifactLayout`, parses already-present matching reduction artifacts from that layout, and then
  continues through the normal solver handoff without launching the reducer
- when `amf_options.use_cache == true` is also set, that same rebuild-and-validate step still
  runs before any solved-path cache hit is returned
- missing, malformed, identity-fallback, or otherwise mismatched prepared state is rejected
  explicitly with `skip_reduction requested but no matching eta-generated reduction state is available`
- direct `RunEtaGeneratedReduction(...)`, `BuildEtaGeneratedDESystem(...)`,
  `SolveEtaGeneratedSeries(...)`, invariant-generated wrappers,
  `SolveDifferentialEquation(...)`, and other non-wrapper entry points do not gain new
  `skip_reduction` semantics

The bootstrap also exposes a deterministic parsed-result surface for Kira artifacts:

- accepted parser entry roots are either a direct family-results tree rooted at
  `results/<family>/` or an outer reducer root that resolves one family-results tree under either
  `generated-config/results/<family>/` or `results/<family>/`
- when both accepted family-results locations exist under an outer reducer root, the parser prefers
  the more complete tree; ties favor `generated-config/results/<family>/`
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
- it is one-invariant-at-a-time and supports only a validated bootstrap subset: quadratic propagators on the existing `Standard` path with mass entries `0`, invariant-independent identifiers, or invariant-independent rational constants with nonzero denominators; arithmetic with `+`, `-`, `*`, parentheses, integer or rational constants, invariant symbols, and squared linear momentum combinations such as `(k-p1-p2)^2`; the reviewed linear-propagator subset resolved through `EffectivePropagatorVariant(...)`, where the propagator expression is invariant-independent and built only from rational constants plus loop-external bilinears such as `k*n` or `1 + k*n + l*n`, and each external symbol that appears there must remain invariant-independent under the declared scalar-product-rule derivatives; and scalar-product rules whose left side is an external-momentum pair such as `p1*p2` and whose right side is a linear scalar expression in the declared invariants
- auto-built derivatives are converted only when every differentiated term can be represented as a coefficient times a same-family propagator-factor product on the current propagator table, with bootstrap matching over exact normalized propagator subexpressions
- the seam rejects empty, unknown, or `eta` invariant names; cut/auxiliary propagator kinds; invariant-dependent propagator masses; broader invariant-independent mass grammar beyond exact identifiers or rational constants with nonzero denominators; linear propagator expressions outside the reviewed invariant-independent loop-external subset; unsupported propagator or scalar-product-rule grammar; unknown momentum symbols; incomplete scalar-product data; normalized duplicate propagator expressions; and derivative terms that cannot be represented on the family propagator table
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
- the same general `KiraBackend` preparation path is now the first truthful non-invariant linear-propagator consumer after B64c, but only for propagators that explicitly declare `variant: "linear"`: on that narrow reviewed subset each accepted expression is an additive sum of rational constants plus loop-external bilinears such as `k*n`, and every external symbol that appears there has complete scalar-product-rule coverage whose entire declared external-pair surface stays on identifier-free rational-constant expressions with nonzero denominators; this is intentionally stricter than the reviewed one-invariant automatic path, and legacy kind-only linear metadata remains outside this claimed consumer surface
- `PrepareEtaGeneratedReduction(...)` composes the reviewed eta insertion seam, eta-derivative generation seam, and explicit-target Kira preparation seam into one typed preparation bundle
- eta-generated target preparation preserves the exact `reduction_targets` order from `GenerateEtaDerivativeVariable(...)`
- eta-generated preparation rejects empty generated target lists locally before reducer execution
- this batch stops at reducer preparation and fake-execution compatibility; it does not yet add automatic post-run parsing or end-to-end DE assembly orchestration

The first invariant-generated target reducer preparation seam is also bootstrap-only:

- `PrepareInvariantGeneratedReduction(...)` keeps the existing seed-based overload intact and also exposes a one-invariant-at-a-time overload that takes `(ProblemSpec, ParsedMasterList, invariant_name, ReductionOptions, ArtifactLayout)` and composes `BuildInvariantDerivativeSeed(...)` with the existing seed-based preparation path
- `PrepareInvariantGeneratedReductionList(...)` takes `(ProblemSpec, ParsedMasterList, invariant_names, ReductionOptions, ArtifactLayout)`, builds the ordered seed list once, preserves that generated-variable order for later assembly, deduplicates generated reduction targets across the batch in first-encounter order, and prepares one shared reducer packet on the original `layout.root` rather than per-invariant child roots
- it consumes the original `ProblemSpec` unchanged; there is no eta insertion, no spec mutation, and the emitted family/kinematics YAML must still reflect the original family and invariant list
- preparation rejects `seed.family != spec.family.name` and `seed.propagator_derivatives.size() != spec.family.propagators.size()` locally before derivative generation
- the invariant-name overload and the ordered list overload both preserve the existing `BuildInvariantDerivativeSeed(...)` diagnostics for empty, unknown, or `eta` invariant names and for unsupported bootstrap-subset masses or symbolic forms
- invariant-generated target preparation preserves the exact `reduction_targets` order from `GenerateInvariantDerivativeVariable(...)`, except that the ordered list path deduplicates the shared reducer target list across the whole batch before preparation
- on the reviewed linear-propagator subset inherited from `BuildInvariantDerivativeSeed(...)`, that same preparation path preserves the original family/kinematics YAML unchanged and emits the exact generated target list without introducing any special linear-only reducer metadata
- the single-invariant path rejects empty generated target lists locally before reducer execution; the ordered list path instead returns a synthetic identity-fallback reduction override only when the entire batch generates no explicit reduction targets
- this seam remains automatic-only and preparation-only: it does not add a public seed-based list overload, CLI, `SkipReduction`, or broader symbolic parity beyond the reviewed bootstrap subset

The first eta-generated reduction execution seam is also bootstrap-only:

- `RunEtaGeneratedReduction(...)` composes the accepted eta preparation seam, existing Kira execution boundary, parsed-result ingestion, and generated-row DE assembly into one typed eta-only flow
- the retained overload that takes only `eta_symbol` stays unchanged; the matching overload that
  also takes one explicit public dimension expression normalizes that expression first, prepends
  `-sd=<dimension>` on the live Kira command only when it evaluates exactly without additional
  symbols, and otherwise keeps reducer launch unchanged
- reducer execution always runs after successful preparation; if execution fails, the wrapper returns cleanly with `parsed_reduction_result` and `assembled_system` unset
- if execution succeeds, the wrapper resolves the accepted family-results tree from the reducer
  execution working-directory artifact root, then parses `masters` and `kira_target.m` from that
  chosen tree before assembling the eta `DESystem`
- when that explicit public dimension expression is symbolic, the wrapper rewrites standalone
  assembled `dimension` identifiers onto the normalized symbolic expression after parse/assembly
  while keeping the parsed reduction result itself unchanged
- on the reviewed explicit-linear subset inherited from `KiraBackend` preparation, the same
  wrapper now also preserves the transformed family / kinematics YAML and parses `results/<family>/`
  unchanged when the direct eta decision rewrites only reviewed quadratic propagators and leaves
  the explicit `variant: "linear"` slot passive, then assembles the ordinary eta row through the
  same generated-row reduction path without a linear-specific execution branch
- successful process execution is not enough on its own: malformed parsed reductions or identity-fallback reductions for generated eta targets fail during the parse/assembly phase
- this batch does not add CLI, `SkipReduction`, invariant reduction orchestration, or broader end-to-end orchestration beyond the eta-only wrapper

The first invariant-generated reduction execution seam is also bootstrap-only:

- `RunInvariantGeneratedReduction(...)` keeps the existing seed-based overload intact and also exposes a one-invariant-at-a-time overload that takes `(ProblemSpec, ParsedMasterList, invariant_name, ReductionOptions, ArtifactLayout, kira_executable, fermat_executable)`
- the invariant-name overload is a thin wrapper: it composes the accepted automatic invariant-preparation seam from `BuildInvariantDerivativeSeed(...)` and `PrepareInvariantGeneratedReduction(...)`, then routes through the same post-preparation execution, parsing, and generated-row assembly logic as the seed-based overload
- reducer execution always runs after successful preparation; if execution fails, the wrapper returns cleanly with `parsed_reduction_result` and `assembled_system` unset
- if execution succeeds, both overloads resolve the accepted family-results tree from the reducer
  execution working-directory artifact root using the original family name, then parse `masters`
  and `kira_target.m` from that chosen tree before assembling the invariant `DESystem`
- invariant execution continues to use the original family and kinematics without eta insertion, and the automatic overload preserves the exact generated-target order from the reviewed automatic preparation path
- on the reviewed linear-propagator subset inherited from that preparation path, the same execution wrapper now also preserves the original family/kinematics YAML unchanged, parses the reducer output from `results/<family>/`, and assembles the resulting invariant row through the ordinary generated-row reduction path without any linear-specific execution branch
- successful process execution is not enough on its own: malformed parsed reductions, identity-fallback reductions, or missing explicit rules for generated invariant targets fail during the parse/assembly phase
- this batch still does not claim the full upstream symbolic automatic-derivative solver; it remains one invariant at a time and does not add multi-invariant orchestration, CLI, `SkipReduction`, or broader symbolic-subset widening beyond the reviewed bootstrap path

The first invariant-generated `DESystem` consumer is also bootstrap-only:

- `BuildInvariantGeneratedDESystem(...)` is a library-only one-invariant convenience seam over the reviewed automatic `RunInvariantGeneratedReduction(...)` wrapper and returns the assembled `DESystem` directly on success
- it reuses the existing automatic preparation, reducer execution, parse, and generated-row assembly path instead of reimplementing reducer orchestration locally
- on the reviewed linear-propagator subset inherited from the automatic execution wrapper, it returns the same assembled one-variable `DESystem` unchanged, including the ordinary generated invariant matrix entries
- unsuccessful reducer execution is converted into a deterministic consumer-level failure that preserves the recorded execution status, exit code, and stderr log path in the diagnostic
- this batch does not add a seed-based consumer overload, solver invocation, CLI, `SkipReduction`, or broader symbolic-subset widening

The current local multi-invariant `DESystem` consumer extension is still narrow:

- `BuildInvariantGeneratedDESystemList(...)` takes an ordered list of invariant names, routes them through one shared `PrepareInvariantGeneratedReductionList(...)` batch on the original `layout.root`, then performs one reducer execution/parse pass whose reduction result is reused across the ordered generated variables while assembling one combined multi-variable `DESystem`
- it preserves caller order in the assembled multi-variable `DESystem`, keeps the original family/kinematics YAML unchanged, and on the reviewed linear-propagator subset returns the same ordinary generated invariant matrix entries without introducing a special linear-only assembly path
- empty invariant lists fail locally before reducer execution, and any hard batch-preparation, reducer-execution, or parse/assembly failure preserves the upstream diagnostic without inventing aggregate partial-success semantics
- this local packet does not add a seed-based list consumer, CLI, `SkipReduction`, or broader symbolic-subset widening beyond the reviewed automatic list path

The first eta-generated `DESystem` consumer is also bootstrap-only:

- `BuildEtaGeneratedDESystem(...)` is a library-only convenience seam over the reviewed `RunEtaGeneratedReduction(...)` wrapper and returns the assembled `DESystem` directly on success
- the matching public dimension-expression overload remains equally narrow: exactly evaluable
  expressions route through the reviewed exact `-sd=<dimension>` execution path, while symbolic
  expressions keep reducer launch unchanged and return an assembled `DESystem` whose standalone
  `dimension` identifiers have been rewritten onto the normalized symbolic expression
- it reuses the existing eta preparation, reducer execution, parse, and generated-row assembly path instead of reimplementing eta orchestration locally
- on the reviewed explicit-linear subset inherited from the eta-generated execution wrapper, it
  returns the same assembled one-variable eta `DESystem` unchanged when the direct eta decision
  rewrites only reviewed quadratic propagators and leaves the explicit linear slot passive,
  including the ordinary eta matrix entries
- unsuccessful reducer execution is converted into a deterministic consumer-level failure that preserves the recorded execution status, exit code, and stderr log path in the diagnostic
- this batch does not add solver handoff on that linear subset, eta-mode expansion, CLI,
  multi-variable orchestration, `SkipReduction`, or broader end-to-end eta solving

The first invariant-generated solver handoff remains narrow:

- `SolveInvariantGeneratedSeries(...)` takes the same one-invariant automatic reduction inputs as `BuildInvariantGeneratedDESystem(...)`, plus an injected `const SeriesSolver&` and solver request fields excluding `DESystem`
- after `BuildInvariantGeneratedDESystem(...)`, it populates `SolveRequest` and routes the solve through `SolveWithPrecisionRetry(...)` rather than a raw single `SeriesSolver::Solve(...)` call (`src/solver/series_solver.cpp:2687-2712`; retry loop at `src/solver/series_solver.cpp:1904-1928`)
- on retryable `failure_code == "insufficient_precision"`, that internal loop keeps `requested_digits` fixed, retries only when `EvaluatePrecision(...)` suggests a larger `working_precision` or `x_order`, and otherwise stops deterministically when the request is already covered or escalation is rejected (`src/solver/series_solver.cpp:1904-1928`, `src/solver/precision_policy.cpp:8-37`)
- on the reviewed linear-propagator subset inherited from `BuildInvariantGeneratedDESystem(...)`, the same wrapper forwards the assembled invariant `DESystem`, start/target locations, precision policy, and requested digits unchanged into the injected solver
- pre-solver failures preserve the existing `BuildInvariantGeneratedDESystem(...)` diagnostics unchanged and do not invoke the supplied solver
- this batch does not add solver-selection policy, a seed-based solver overload, CLI, boundary generation, or algorithmic series solving

The current local multi-invariant solver handoff extension is still narrow:

- `SolveInvariantGeneratedSeriesList(...)` takes an ordered list of invariant names plus the same explicit solver-request fields as `SolveInvariantGeneratedSeries(...)`
- after the reviewed physical-kinematics preflight, it builds one shared invariant-generated `DESystem` through `BuildInvariantGeneratedDESystemList(...)`, then routes one solve through `SolveWithPrecisionRetry(...)` with the same fixed-`requested_digits` retry behavior used by the single-invariant wrapper (`src/solver/series_solver.cpp:4429-4479`, `src/solver/series_solver.cpp:1904-1928`)
- pre-solver `MasterSetInstabilityError` is converted into returned `master_set_instability` diagnostics, while the remaining upstream list-validation or reducer-construction failures still propagate as throwing failures; none of those pre-solver paths invoke the supplied solver
- this local packet does not add solver-selection policy, a seed-based list overload, CLI, boundary generation, or broader symbolic-subset widening beyond the reviewed automatic list path

The first eta-generated solver handoff remains narrow:

- `SolveEtaGeneratedSeries(...)` takes the same eta-generated reduction inputs as `BuildEtaGeneratedDESystem(...)`, plus an injected `const SeriesSolver&`, explicit solver request fields excluding `DESystem`, and an optional trailing `eta_symbol`
- a matching overload now also accepts one explicit public dimension expression after
  `eta_symbol`; when present it is normalized before DE construction, exactly evaluable
  expressions route through the reviewed exact
  `BuildEtaGeneratedDESystem(..., eta_symbol, exact_dimension_override)` path and copy the
  canonical exact value onto `SolveRequest.amf_requested_dimension_expression`, while symbolic
  expressions keep reducer launch unchanged but rewrite assembled standalone `dimension`
  identifiers on this direct solver handoff before solver execution and still carry the normalized
  symbolic request metadata on `SolveRequest.amf_requested_dimension_expression`
- when `ProblemSpec.kinematics.complex_numeric_substitutions` is non-empty, the wrapper now
  validates that merged exact/complex binding map and exact-complex expression grammar before the
  retained physical-kinematics guardrails or DE construction run, so malformed complex numeric
  bindings fail explicitly with the underlying parser diagnostic instead of collapsing into the
  retained unsupported-complex-kinematics summary
- when complex numeric bindings are present on a surface that still reaches
  `BuildEtaGeneratedDESystem(...)`, the wrapper now treats that request as one reviewed complex
  continuation candidate: on the reviewed finite-horizontal auto-planning subset it plans one
  reviewed upper-half-plane contour through `PlanEtaContinuationContour(...)`, persists an
  `EtaContinuationPlanManifest` under `layout.manifests_dir/`; injected solvers that explicitly
  advertise the reviewed live complex-continuation capability then receive one live
  `SolveRequest` carrying that same `eta_continuation_plan`, while non-opt-in solvers keep the
  retained explicit `unsupported_solver_path` diagnostic carrying the contour fingerprint plus
  the persisted manifest path instead of being invoked
- unsupported or malformed contour-planning inputs on that same reviewed complex-continuation path
  still fail explicitly with the underlying `invalid_argument` rather than being relabeled as a
  solver-path diagnostic
- if reviewed contour planning succeeds but manifest persistence fails, the wrapper still returns
  explicit `unsupported_solver_path` diagnostics that report the contour fingerprint, intended
  manifest path, and artifact-store failure without invoking the supplied solver
- on the current-worktree reviewed deferred-complex replay surface only, the direct
  `SolveEtaGeneratedSeries(...)` pre-build replay gate additionally checks content fingerprints of
  the concrete supplied `kira_executable` and `fermat_executable` that were recorded in the
  cached deferred diagnostic, so changed reducer executable content does not replay stale
  deferred complex diagnostics on that fast path
- otherwise, after `BuildEtaGeneratedDESystem(...)`, it populates `SolveRequest` and routes the
  solve through `SolveWithPrecisionRetry(...)` rather than a raw single
  `SeriesSolver::Solve(...)` call (`src/solver/series_solver.cpp:2753-2780`; retry loop at
  `src/solver/series_solver.cpp:1904-1928`)
- on the reviewed explicit-linear subset inherited from `BuildEtaGeneratedDESystem(...)`, the
  same direct solver wrapper now forwards the assembled one-variable eta `DESystem`, start/target
  locations, precision policy, requested digits, and injected solver diagnostics unchanged when a
  caller-supplied direct eta decision rewrites only reviewed quadratic propagators and leaves the
  explicit linear slot passive
- on retryable `failure_code == "insufficient_precision"`, the same internal loop keeps `requested_digits` fixed, retries only when `EvaluatePrecision(...)` suggests a larger `working_precision` or `x_order`, and otherwise stops deterministically when the request is already covered or escalation is rejected (`src/solver/series_solver.cpp:1904-1928`, `src/solver/precision_policy.cpp:8-37`)
- pre-solver failures preserve the existing `BuildEtaGeneratedDESystem(...)` diagnostics unchanged and do not invoke the supplied solver
- malformed or exact-arithmetic-invalid public dimension expressions still fail explicitly with
  `invalid_argument` or the underlying exact-arithmetic diagnostic, and the retained overload
  without that extra argument stays unchanged
- this batch does not add solver-selection policy, CLI, builtin eta-mode linear selection,
  eta-mode expansion, multi-variable orchestration, boundary generation, or algorithmic series
  solving

The first eta-mode-planned solver handoff stays narrow:

- `SolveEtaModePlannedSeries(...)` takes the same eta-generated solver inputs as `SolveEtaGeneratedSeries(...)`, except `EtaInsertionDecision` is replaced by an injected `const EtaMode&`
- a matching overload now also accepts one explicit public dimension expression after `eta_symbol`,
  with the same normalization and exact-vs-symbolic downstream rule as
  `SolveEtaGeneratedSeries(...)`
- it is a thin wrapper: it calls `EtaMode::Plan(spec)`, then forwards the resulting `EtaInsertionDecision` directly into `SolveEtaGeneratedSeries(...)`, so the same generated-wrapper retry behavior applies downstream
- planning failures preserve the existing `EtaMode::Plan(...)` diagnostics unchanged and do not invoke the supplied solver
- downstream eta-generated `DESystem` construction failures also preserve the existing `SolveEtaGeneratedSeries(...)` diagnostics unchanged and do not invoke the supplied solver
- when complex numeric bindings are present, the same malformed-binding preflight now happens only
  after exactly one retained `EtaMode::Plan(spec)` call because this wrapper still delegates
  directly into `SolveEtaGeneratedSeries(...)`
- when that downstream reviewed complex-continuation deferral applies on the reviewed
  finite-horizontal auto-planning subset, the same single retained `EtaMode::Plan(spec)` call
  still happens first, then this wrapper inherits the direct eta-generated contour-plan manifest
  persistence and either the same live reviewed `eta_continuation_plan` solver handoff for
  opt-in solvers or the same explicit `unsupported_solver_path` diagnostics for non-opt-in
  solvers
- unsupported or malformed contour-planning inputs on that downstream reviewed continuation path
  still fail explicitly with the underlying `invalid_argument` after exactly one retained
  `EtaMode::Plan(spec)` call
- if reviewed contour planning succeeds but manifest persistence fails downstream, this wrapper
  still preserves the direct eta-generated explicit `unsupported_solver_path` diagnostics after
  exactly one retained `EtaMode::Plan(spec)` call
- on the reviewed explicit-linear subset inherited from the direct eta-generated solver handoff,
  the same thin wrapper now performs exactly one retained `EtaMode::Plan(spec)` call, then
  forwards the assembled one-variable eta `DESystem`, start/target locations, precision policy,
  requested digits, and injected solver diagnostics unchanged when that planned decision rewrites
  only reviewed quadratic propagators and leaves the explicit linear slot passive
- malformed or exact-arithmetic-invalid public dimension expressions still fail downstream after
  exactly one `EtaMode::Plan(spec)` call and still do not invoke the supplied solver
- this batch does not add new builtin eta-mode semantics for linear propagators, cache policy,
  CLI, multi-variable orchestration, boundary generation, or algorithmic series solving

The first builtin eta-mode-name solver wrapper is also bootstrap-only:

- `SolveBuiltinEtaModeSeries(...)` takes the same eta solver inputs as `SolveEtaModePlannedSeries(...)`, except `const EtaMode&` is replaced by `const std::string& eta_mode_name`
- a matching overload now also accepts one explicit public dimension expression after `eta_symbol`,
  with the same normalization and exact-vs-symbolic downstream rule as
  `SolveEtaModePlannedSeries(...)`
- it is a thin wrapper: it calls `MakeBuiltinEtaMode(eta_mode_name)`, then forwards the resolved builtin mode directly into `SolveEtaModePlannedSeries(...)`
- builtin-name resolution failures preserve the existing `MakeBuiltinEtaMode(...)` diagnostics unchanged and do not invoke the supplied solver
- downstream builtin planning failures preserve the existing `EtaMode::Plan(...)` diagnostics unchanged and do not invoke the supplied solver
- downstream eta-generated `DESystem` construction failures also preserve the existing `SolveEtaModePlannedSeries(...)` / `SolveEtaGeneratedSeries(...)` diagnostics unchanged and do not invoke the supplied solver
- when complex numeric bindings are present, malformed complex-binding failures now also surface
  downstream after builtin-name resolution and retained planning because this wrapper still
  delegates into `SolveEtaModePlannedSeries(...)` / `SolveEtaGeneratedSeries(...)`
- malformed or exact-arithmetic-invalid public dimension expressions still fail downstream after
  builtin-name resolution and still do not invoke the supplied solver
- this batch does not add `AMFMode` list fallback, user-defined mode registration, new builtin eta-mode semantics, cache policy, CLI, multi-variable orchestration, boundary generation, or algorithmic series solving

The first builtin eta-mode-list solver wrapper is also bootstrap-only:

- `SolveBuiltinEtaModeListSeries(...)` takes the same eta solver inputs as `SolveBuiltinEtaModeSeries(...)`, except `const std::string& eta_mode_name` is replaced by a caller-supplied ordered `const std::vector<std::string>& eta_mode_names`
- a matching overload now also accepts one explicit public dimension expression after `eta_symbol`,
  with the same normalization and exact-vs-symbolic downstream rule as
  `SolveBuiltinEtaModeSeries(...)`
- it is a narrow ordered-selection wrapper: it resolves builtin names in caller order, probes planning in that same order, and delegates to `SolveBuiltinEtaModeSeries(...)` once for the first builtin whose planning step succeeds
- on the reviewed passive-linear subset inherited from `Batch 64g`,
  `SolveBuiltinEtaModeListSeries(...)` forwards the same eta-generated `DESystem`, locations,
  precision policy, requested digits, and solver diagnostics as the direct
  `SolveBuiltinEtaModeSeries("Prescription", ...)` path when the selected builtin is
  `Prescription`, while leaving the explicit `variant: "linear"` slot passive
- empty builtin-name lists fail locally with a deterministic argument error
- unknown builtin-name resolution failures preserve the existing `MakeBuiltinEtaMode(...)` diagnostics unchanged and stop selection immediately
- builtin `Branch` / `Loop` planning failures remain immediate terminal failures and do not fall through to later builtin names
- if no builtin in the caller-supplied list reaches solve selection, the final builtin planning failure is preserved unchanged and the supplied solver is not invoked
- downstream eta-generated `DESystem` construction failures from the selected builtin preserve the existing `SolveBuiltinEtaModeSeries(...)` / `SolveEtaGeneratedSeries(...)` diagnostics unchanged and do not trigger fallback to later builtin names
- when complex numeric bindings are present, malformed complex-binding failures from the selected
  builtin now also preserve that downstream failure path and do not trigger fallback to later
  builtin names
- malformed or exact-arithmetic-invalid public dimension expressions still fail downstream after
  builtin-list selection and still do not trigger fallback to later builtin names or invoke the
  supplied solver
- this batch still does not inject the default `AMFMode` list from `AmfOptions`, add user-defined mode registration, add new builtin eta-mode semantics, add cache policy or CLI behavior, or widen into broader orchestration

The first `AmfOptions`-fed builtin eta-mode-list solver wrapper is also bootstrap-only:

- `SolveAmfOptionsEtaModeSeries(...)` takes the same eta solver inputs as `SolveBuiltinEtaModeListSeries(...)`, except the caller-supplied `const std::vector<std::string>& eta_mode_names` is replaced by `const AmfOptions& amf_options`
- a matching overload now also accepts one explicit public dimension expression after
  `eta_symbol`, with the same public normalization and exact-vs-symbolic downstream rule as
  `SolvePlannedAmfOptionsEtaModeSeries(...)`
- it is still a thin option-feed wrapper for eta-mode selection: it reads `amf_options.amf_modes`, preserves the reviewed ordered builtin-list semantics, and keeps caller/default order, empty-list rejection, immediate unknown-name failure, preserved final planning failure, and no downstream fallback widening unchanged
- after builtin planning succeeds, the wrapper rebuilds a live wrapper-owned solve policy from `AmfOptions`: `WorkingPre`, `ChopPre`, `XOrder`, and `RationalizePre` overwrite the live `PrecisionPolicy` fields passed into the solver handoff, while `ExtraXOrder`, `LearnXOrder`, `TestXOrder`, and `RunLength` are attached to `SolveRequest` through `AmfSolveRuntimePolicy`
- after builtin planning succeeds, the wrapper also copies `amf_options.d0` into `SolveRequest.amf_requested_d0` and populates the derived `SolveRequest.amf_requested_dimension_expression`; when `amf_options.fixed_eps` is present and exact, that derived carrier collapses to an exact numeric dimension expression, otherwise it remains a symbolic wrapper carrier. The direct exact solver may use an already-exactly-numeric `amf_requested_dimension_expression` as a passive `dimension` binding, and on these two wrappers the live eta-generated Kira execution plus wrapper-owned prepared-state validation still treat `-sd=<dimension>` as exact-only. When there is no explicit public dimension-expression overload in use, that same derived symbolic carrier now also rewrites assembled standalone `dimension` identifiers on the eta-generated `DESystem` before solver execution instead of staying metadata-only. Separately, direct public generated-DE helpers now expose their own public dimension-expression overloads; the retained overloads without that extra argument stay unchanged
- this wrapper now also reads `amf_options.use_cache` as a narrow solved-path diagnostic replay flag only: after builtin planning succeeds it computes one deterministic solved-path slot plus an input fingerprint over the wrapper-owned solve inputs and current concrete solver type, replays only matching successful cache artifacts, rejects stale or malformed artifacts in favor of live execution, refreshes the slot after any successful live solve, and still rebuilds and validates the current prepared eta-generated DE first whenever `amf_options.skip_reduction == true`
- this wrapper now also reads `amf_options.skip_reduction` as a wrapper-owned reducer-reuse flag only: after builtin planning succeeds it rebuilds the current eta-generated preparation, requires matching prepared reducer inputs and parseable matching reduction artifacts under the current `ArtifactLayout`, and then continues through the same solver handoff without launching the reducer; missing or mismatched state fails explicitly
- the live `PrecisionPolicy`, `AmfSolveRuntimePolicy`, wrapper-owned requested-`D0`,
  and exact `fixed_eps`
  metadata now participate in solved-path cache slotting plus request fingerprinting,
  solved-path request-summary truthfulness, and `skip_reduction` replay validation on this
  wrapper
- symbolic derived `D0 - 2*eps` carriers now also participate in a dedicated solved-path cache
  rewrite epoch so stale pre-`Batch 59h` artifacts cannot replay the old metadata-only behavior
- the current bootstrap solver still does not implement the broader upstream algorithmic effects of `ExtraXOrder`, `LearnXOrder`, `TestXOrder`, or `RunLength`; on the reviewed subset those fields are carried and fingerprinted rather than given broader standalone semantics
- direct `SolveEtaGeneratedSeries(...)`, public eta-helper surfaces, and direct
  `SolveDifferentialEquation(...)` remain unchanged, while direct
  `SolveBuiltinEtaModeListSeries(...)` and direct `SolveResolvedEtaModeListSeries(...)` now each
  expose separate public dimension-expression overloads without gaining cache or `skip_reduction`
  behavior
- this batch still does not add interruption-resume behavior, user-defined mode registration, mixed builtin/user-defined fallback, public eta-helper `skip_reduction` semantics, CLI behavior, or broader orchestration widening

The first mixed eta-mode single-name solver wrapper is also bootstrap-only:

- `SolveResolvedEtaModeSeries(...)` takes the same eta solver inputs as `SolveBuiltinEtaModeSeries(...)`, except builtin-only resolution is widened to `const std::vector<std::shared_ptr<EtaMode>>& user_defined_modes`
- a matching overload now also accepts one explicit public dimension expression after `eta_symbol`,
  with the same normalization and exact-vs-symbolic downstream rule as
  `SolveEtaModePlannedSeries(...)`
- it is a thin wrapper: it calls `ResolveEtaMode(eta_mode_name, user_defined_modes)`, then forwards the resolved mode directly into `SolveEtaModePlannedSeries(...)`
- resolution failures preserve the existing `ResolveEtaMode(...)` diagnostics unchanged and do not invoke the supplied solver
- downstream planning failures preserve the existing `EtaMode::Plan(...)` diagnostics unchanged and do not invoke the supplied solver
- downstream eta-generated `DESystem` construction failures also preserve the existing `SolveEtaModePlannedSeries(...)` / `SolveEtaGeneratedSeries(...)` diagnostics unchanged and do not invoke the supplied solver
- when complex numeric bindings are present, malformed complex-binding failures now also surface
  downstream after single-name resolution and retained planning because this wrapper still
  delegates into `SolveEtaModePlannedSeries(...)` / `SolveEtaGeneratedSeries(...)`
- malformed or exact-arithmetic-invalid public dimension expressions still fail downstream after
  single-name resolution and still do not invoke the supplied solver
- this batch does not add exact-only `AmfOptions` overloads, CLI, cache policy, new builtin eta-mode semantics, or broader orchestration behavior

The first mixed eta-mode-list solver wrapper is also bootstrap-only:

- `SolveResolvedEtaModeListSeries(...)` takes the same eta solver inputs as `SolveResolvedEtaModeSeries(...)`, except `const std::string& eta_mode_name` is replaced by a caller-supplied ordered `const std::vector<std::string>& eta_mode_names`
- a matching overload now also accepts one explicit public dimension expression after `eta_symbol`,
  with the same normalization and exact-vs-symbolic downstream rule as
  `SolveEtaGeneratedSeries(...)`
- it is a narrow ordered-selection wrapper: it resolves builtin and user-defined names in caller order, probes planning in that same order, and carries the winning `EtaInsertionDecision` forward without re-planning the selected mode
- empty mixed-name lists fail locally with a deterministic argument error
- unknown-name or registry-validation failures from `ResolveEtaMode(...)` preserve the existing resolver diagnostics unchanged and stop selection immediately
- if no mode in the caller-supplied list reaches solve selection, the final planning failure from `EtaMode::Plan(...)` is preserved unchanged and the supplied solver is not invoked
- standard planning failures from `EtaMode::Plan(...)` are treated as ordered fallback misses until the caller-supplied list exhausts
- downstream eta-generated `DESystem` construction failures from the selected mode preserve the existing `SolveResolvedEtaModeSeries(...)` / `SolveEtaGeneratedSeries(...)` diagnostics unchanged and do not trigger fallback to later names
- when complex numeric bindings are present, malformed complex-binding failures from the selected
  mode now also preserve that downstream failure path and do not trigger fallback to later names
- malformed or exact-arithmetic-invalid public dimension expressions still fail downstream after
  mixed ordered selection and still do not trigger fallback to later names or invoke the
  supplied solver
- this batch does not add exact-only `AmfOptions` overloads, CLI, cache policy, new builtin eta-mode semantics, or broader orchestration behavior

The first `AmfOptions`-fed eta-mode decision and execution helpers are also bootstrap-only:

- `PlanBuiltinAmfOptionsEtaMode(...)` takes `(const ProblemSpec&, const AmfOptions&)` and returns `EtaInsertionDecision`
- it is a standalone ordered builtin-selection helper: it reads only `amf_options.amf_modes`, resolves one builtin mode at a time through `MakeBuiltinEtaMode(...)`, probes planning in that same order, carries the winning `EtaInsertionDecision` forward without re-planning, and remains strictly pre-policy, pre-cache, pre-`skip_reduction`, and pre-`D0`
- empty `amf_options.amf_modes` lists fail locally with `invalid_argument("builtin eta-mode list must not be empty")`
- unknown builtin names preserve the existing `MakeBuiltinEtaMode(...)` diagnostics unchanged and stop selection immediately
- standard builtin planning failures from `EtaMode::Plan(...)` are treated as ordered fallback misses until the list exhausts
- builtin `Branch` / `Loop` planning failures remain immediate terminal failures and do not fall through to later entries
- if the list exhausts, the final builtin planning failure is rethrown unchanged; the defensive exhaustion `runtime_error` remains only for the impossible no-failure path
- this helper does not read or rebuild `PrecisionPolicy`, does not attach `AmfSolveRuntimePolicy`, does not touch solved-path cache slotting or fingerprints, does not validate `skip_reduction`, and does not thread `amf_options.d0` into `SolveRequest`
- `PlanAmfOptionsEtaMode(...)` takes `(const ProblemSpec&, const AmfOptions&, const std::vector<std::shared_ptr<EtaMode>>& user_defined_modes)` and returns `EtaInsertionDecision`
- it is a standalone ordered mixed-selection helper: it reads only `amf_options.amf_modes`, resolves one builtin-or-user-defined mode at a time through `ResolveEtaMode(...)`, probes planning in that same order, carries the winning `EtaInsertionDecision` forward without re-planning, and remains strictly pre-policy, pre-cache, pre-`skip_reduction`, and pre-`D0`
- empty `amf_options.amf_modes` lists fail locally with `invalid_argument("eta-mode list must not be empty")`
- unknown-name or registry-validation failures from `ResolveEtaMode(...)` preserve the existing resolver diagnostics unchanged and stop selection immediately
- standard planning failures from `EtaMode::Plan(...)` are treated as ordered fallback misses until the list exhausts
- builtin `Branch` / `Loop` planning failures remain immediate terminal failures and do not fall through to later entries
- if the list exhausts, the final recorded planning failure is rethrown unchanged; the defensive exhaustion `runtime_error` remains only for the impossible no-failure path
- this helper does not read or rebuild `PrecisionPolicy`, does not attach `AmfSolveRuntimePolicy`, does not touch solved-path cache slotting or fingerprints, does not validate `skip_reduction`, and does not thread `amf_options.d0` into `SolveRequest`
- `SolvePlannedAmfOptionsEtaModeSeries(...)` takes `(const ProblemSpec&, const ParsedMasterList&, const EtaInsertionDecision&, const AmfOptions&, const std::string& solve_kind, const ReductionOptions&, const ArtifactLayout&, const std::filesystem::path& kira_executable, const std::filesystem::path& fermat_executable, const SeriesSolver&, const std::string& start_location, const std::string& target_location, const PrecisionPolicy&, int requested_digits, const std::string& eta_symbol = "eta")`
- a matching overload also takes `const std::optional<std::string>& exact_dimension_override`
  after `eta_symbol`
- it is a standalone planned-decision execution helper: after a caller has already selected and planned one eta mode, it rebuilds the same live wrapper-owned solve tail from `AmfOptions` that the reviewed `SolveAmfOptionsEtaModeSeries(...)` overloads already owned, including live `PrecisionPolicy`, `AmfSolveRuntimePolicy`, wrapper-owned requested-`D0` metadata plus derived dimension-expression, optional exact `fixed_eps` collapse on that dimension carrier, solved-path cache setup, `use_cache`, and `skip_reduction`
- when the overload with `exact_dimension_override` is used, that explicit public dimension
  expression is normalized once at the public boundary and then replaces the otherwise derived
  live dimension carrier on this helper only; exact expressions still drive live
  `-sd=<dimension>` plus wrapper-owned prepared-state validation, while symbolic expressions keep
  reducer launch exact-only but rewrite assembled standalone `dimension` identifiers on the live
  or `skip_reduction`-replayed eta-generated `DESystem` before solver execution. The helper still
  preserves wrapper-owned requested-`D0`, live `PrecisionPolicy`, live runtime policy,
  `solve_kind`, `use_cache`, and `skip_reduction` semantics unchanged
- this helper does not re-read `amf_options.amf_modes`, does not perform eta-mode resolution or
  planning, and does not change direct `SolveDifferentialEquation(...)` behavior. The reviewed
  widening stays narrow: symbolic explicit public dimension expressions and symbolic derived
  `D0 - 2*eps` carriers are now consumed only on the generated eta-mode solver handoff /
  wrapper-owned assembled `DESystem`, while reducer-facing symbolic overrides and broader
  arbitrary symbolic runtime parity remain deferred
- on that explicit-override overload, the normalized dimension carrier also participates in
  solved-path cache slot naming and input fingerprinting, solved-path request-summary truthfulness,
  and `skip_reduction` replay validation on this helper; symbolic explicit public dimension
  expressions also carry a dedicated solved-path cache epoch so stale pre-rewrite cache artifacts
  fall back to live execution rather than replaying across the B59g behavior change
- when the helper instead derives a symbolic dimension carrier from `amf_options.d0` /
  `fixed_eps`, the same solved-path cache epoch now participates in slot naming and input
  fingerprinting so stale pre-`Batch 59h` artifacts fall back to live execution rather than
  replaying across the old metadata-only derived-carrier behavior
- any `ProblemSpec` complex-mode request now hits the existing complex-binding preflight plus the
  reviewed Batch 62 physical guardrails before solved-path cache replay or solver execution on
  this helper; bare `complex_mode` requests without explicit complex substitutions therefore stay
  on the reviewed `physical_kinematics_not_supported` rejection path
- when `ProblemSpec.kinematics.complex_numeric_substitutions` is non-empty, this helper also
  first rebuilds or `skip_reduction`-assembles the wrapper-owned eta-generated `DESystem`,
  applies the existing wrapper-owned symbolic-dimension rewrite, and then splits on the injected
  solver capability. If `SeriesSolver::SupportsReviewedComplexEtaContinuation()` stays false, the
  helper preserves the reviewed deferred/cache complex-continuation seam: it plans and persists
  one reviewed upper-half-plane continuation contour under `layout.manifests_dir/`, writes one
  distinct solved-path cache artifact keyed on the dedicated
  `complex-continuation-deferred-v1` slot plus the planned contour fingerprint, and stops with
  explicit `unsupported_solver_path` diagnostics that report the contour fingerprint plus manifest
  path instead of invoking the supplied solver
- if that same helper path sees an injected solver that explicitly opts into
  `SeriesSolver::SupportsReviewedComplexEtaContinuation()`, it instead persists the same
  reviewed contour-plan manifest, attaches the reviewed `EtaContinuationPlan` onto
  `SolveRequest.eta_continuation_plan`, and routes one live solve through
  `SolveWithPrecisionRetry(...)`; this still stays on the reviewed finite-horizontal
  upper-half-plane subset only, and it does not write solved-path cache artifacts on that live
  opt-in path
- on that deferred complex subset, `amf_options.use_cache == true` now enables replay only for
  that one reviewed deferred diagnostic shape. On the non-`skip_reduction` path, replay now
  happens before rebuilding the live reduced `DESystem`, and that fast replay also requires the
  reducer executable content fingerprints recorded in the cached deferred diagnostic to still
  match the current `kira_executable` / `fermat_executable`; on the `skip_reduction` path replay
  stays reducer-independent and still runs only after the existing prepared-state validation
  rebuilds the cached `SolveRequest` fingerprint. In both cases the matching continuation-plan
  manifest must still exist and still match the cached contour fingerprint, while bare
  `complex_mode` requests without explicit complex substitutions and manifest-write failures
  remain uncached for replay purposes
- it preserves the caller-supplied solved-path/cache identity string verbatim through slot naming, input fingerprinting, request fingerprinting, request-summary truthfulness, and manifest `solve_kind`; the reviewed `AmfOptions` wrappers keep carrying `"amf-options-builtin-eta-mode-series"` and `"amf-options-resolved-eta-mode-series"` unchanged
- `SolveAmfOptionsEtaModeSeries(...)` also exposes matching public dimension-expression overloads
  on both its builtin-only and mixed entrypoints; each takes one explicit
  `exact_dimension_override` after `eta_symbol`
- the builtin-only `SolveAmfOptionsEtaModeSeries(...)` overload remains a thin option-feed wrapper
  for builtin eta-mode selection: it keeps the ordered builtin selection step local through
  `PlanBuiltinAmfOptionsEtaMode(...)`, then delegates the shared downstream wrapper-owned
  execution tail through `SolvePlannedAmfOptionsEtaModeSeries(...)` with the preserved builtin
  solved-path identity; on the explicit-dimension overload, that same wrapper-local selection
  still happens before downstream dimension-expression normalization runs
- the mixed `SolveAmfOptionsEtaModeSeries(...)` overload remains a thin option-feed wrapper for mixed eta-mode selection: it keeps the ordered mixed builtin/user-defined selection step local through `PlanAmfOptionsEtaMode(...)`, then delegates that same shared downstream wrapper-owned execution tail through `SolvePlannedAmfOptionsEtaModeSeries(...)` with the preserved resolved/mixed solved-path identity; on the explicit-dimension overload, that same ordered selection still happens before downstream dimension-expression normalization runs
- both outer `SolveAmfOptionsEtaModeSeries(...)` overloads therefore inherit the current-worktree
  complex-continuation deferral seam after their retained local planning step only: reviewed
  complex candidates now return explicit `unsupported_solver_path` diagnostics without live
  contour execution or eta-to-zero branch handling; when continuation-plan manifest persistence
  succeeds on that reviewed subset, they also persist one continuation-plan manifest plus the
  reviewed complex-specific solved-path cache artifact
- the live `PrecisionPolicy`, `AmfSolveRuntimePolicy`, wrapper-owned requested-`D0`, and exact
  `fixed_eps`
  metadata now participate in solved-path cache slotting plus request fingerprinting,
  solved-path request-summary truthfulness, and `skip_reduction` replay validation on those
  wrappers through the shared planned-decision execution helper; when the explicit
  public-dimension-expression overload is used, that normalized dimension carrier also
  participates in those same identities instead of the helper's otherwise derived live dimension
  carrier
- the current bootstrap solver still does not implement the broader upstream algorithmic effects of `ExtraXOrder`, `LearnXOrder`, `TestXOrder`, or `RunLength`; on the reviewed subset those fields are carried and fingerprinted rather than given broader standalone semantics
- direct `SolveEtaGeneratedSeries(...)` now also persists and replays the same reviewed deferred
  complex-continuation diagnostic on a direct `eta-generated-series` solved-path identity only:
  after the existing complex-binding preflight and reviewed Batch 62 guardrails pass, replay on
  that complex-only direct cache slot happens before rebuilding the live reduced `DESystem`,
  while a miss still rebuilds the live `DESystem`, applies any reviewed symbolic-dimension
  rewrite, persists one continuation-plan manifest plus one direct solved-path cache artifact,
  and stops on the same explicit deferred diagnostic instead of invoking the supplied solver
- direct `SolveEtaModePlannedSeries(...)` inherits that same direct deferred-complex cache
  surface only after exactly one retained `EtaMode::Plan(spec)` call, so its replay still
  preserves the original planning input and short-circuits only the downstream live reduction /
  exact-solver path
- public eta-helper surfaces other than those two direct solver handoffs, direct
  `SolveDifferentialEquation(...)`, and direct `SolveResolvedEtaModeListSeries(...)` remain
  unchanged, while direct `SolveResolvedEtaModeListSeries(...)` still has no cache behavior and
  now only adds its separate public dimension-expression overload
- this batch still does not add interruption-resume behavior, direct `SolveResolvedEtaModeListSeries(...)` cache behavior, public eta-helper `skip_reduction` semantics, CLI behavior, or broader orchestration widening

The first user-defined eta-mode resolver seam is also bootstrap-only:

- `ResolveEtaMode(...)` takes one eta-mode name plus a caller-supplied `const std::vector<std::shared_ptr<EtaMode>>& user_defined_modes`
- it validates the full supplied user-defined registry on every call before resolution proceeds: null entries, duplicate user-defined names anywhere in the registry, and user-defined names that collide with builtin eta-mode names anywhere in the registry fail locally with deterministic argument errors
- after registry validation, it is a one-name resolution hook: builtin names still resolve through the accepted builtin table when no user-defined mode claims the same name, while a unique non-builtin user-defined name returns the exact registered `EtaMode` instance unchanged
- resolution itself is name-only and does not call `EtaMode::Plan(...)`
- unresolved names preserve the existing `unknown eta mode: <name>` diagnostic surface
- this seam remains the typed runtime hook under `PlanAmfOptionsEtaMode(...)` and the reviewed mixed solver wrappers and still does not itself add CLI, cache policy, or broader orchestration

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
- current builtin ending schemes are still intentionally narrow, but no longer uniform placeholders: `Tradition` remains the loop-only `eta->infinity` planner and now rejects cut-marked specs on the reviewed local phase-space subset; `Cutkosky` is the first truthful phase-space ending consumer on that same reviewed subset and returns the distinct terminal node `<family>::cutkosky-phase-space`; `SingleMass` still keeps the placeholder singleton `<family>::eta->infinity`; and builtin `Trivial` still adds the unsupported extra node `<family>::trivial-region`
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
- `AttachBoundaryConditionsFromProviderRegistry(...)` takes `(const SolveRequest&, const std::vector<std::shared_ptr<BoundaryProvider>>&)`
- it is the first reviewed caller-supplied provider-registry seam: it validates `SolveRequest.boundary_requests` deterministically first, rejects preexisting `boundary_conditions` as conflicting reattachment, preserves the reviewed empty-request fast path unchanged, and otherwise validates the full supplied registry before provider routing, rejecting null entries and duplicate `Strategy()` strings as `invalid_argument`; it then routes each request to the unique provider whose `Strategy()` matches `BoundaryRequest.strategy`, calls `Provide(...)` once per request in request order, and delegates the returned explicit boundary list back through `AttachManualBoundaryConditions(...)`
- missing registry coverage for one request strategy fails as typed `boundary_unsolved`, and provider-thrown or provider-produced downstream failures still preserve the reviewed manual-attachment diagnostics unchanged
- `GenerateBuiltinEtaInfinityBoundaryRequest(...)` is a pure library-only generator that takes `(const ProblemSpec&, const std::string& eta_symbol = "eta")`, validates `ProblemSpec` first, rejects empty `eta_symbol` as an argument error, and on the supported subset returns exactly `{variable = eta_symbol, location = "infinity", strategy = "builtin::eta->infinity"}`
- the supported Batch 45 subset is intentionally narrow: every propagator must be `Standard` and must have mass exactly `"0"`; well-formed specs outside that subset fail as typed `boundary_unsolved`, while malformed specs continue to fail as ordinary `invalid_argument` validation errors
- Batch 45 request generation is independent of ending planning and provider execution: it does not consume `EndingDecision`, does not look up or run any `BoundaryProvider`, does not compute boundary values or `BoundaryCondition` entries, and does not call the solver
- `GeneratePlannedEtaInfinityBoundaryRequest(...)` takes `(const ProblemSpec&, const std::string& ending_scheme_name, const std::vector<std::shared_ptr<EndingScheme>>& user_defined_schemes, const std::string& eta_symbol = "eta")`
- it is a thin single-name composition wrapper: it calls `PlanEndingScheme(...)` exactly once, accepts only the exact singleton supported terminal-node list `{<family>::eta->infinity}`, and then delegates into `GenerateBuiltinEtaInfinityBoundaryRequest(...)`
- ordinary ending resolution and planning failures preserve the existing `PlanEndingScheme(...)` diagnostics unchanged
- on success it returns the exact reviewed Batch 45 request shape `{variable = eta_symbol, location = "infinity", strategy = "builtin::eta->infinity"}`; `EndingDecision.terminal_strategy` is not reused as a boundary-request strategy
- any extra terminal node is currently unsupported and fails as typed `boundary_unsolved`, including the current builtin `Trivial` extra node `<family>::trivial-region` and the reviewed builtin `Cutkosky` phase-space terminal node `<family>::cutkosky-phase-space`; missing the supported node or duplicating it also fails as typed `boundary_unsolved`
- `GenerateAmfOptionsEndingSchemeEtaInfinityBoundaryRequest(...)` takes `(const ProblemSpec&, const AmfOptions&, const std::vector<std::shared_ptr<EndingScheme>>& user_defined_schemes, const std::string& eta_symbol = "eta")`
- it is a standalone ordered-list boundary-request seam: it reads only `amf_options.ending_schemes`, delegates ordered fallback exactly once through `PlanAmfOptionsEndingScheme(...)`, and then validates only that selected `EndingDecision` against the reviewed singleton infinity-node subset before delegating into `GenerateBuiltinEtaInfinityBoundaryRequest(...)`
- ordered planning-time fallback therefore remains exactly the reviewed `PlanAmfOptionsEndingScheme(...)` surface: empty lists still fail locally, resolver/registry `invalid_argument` still stop selection immediately, and standard planning failures still fall through in caller/default order until one scheme reaches a selected `EndingDecision` or the list exhausts
- once one ending scheme has been selected, eta->infinity request generation no longer falls through to later endings on boundary-subset mismatch; a reviewed non-infinity selection such as builtin `Cutkosky` therefore surfaces its own typed `boundary_unsolved` terminal-node diagnostic instead of being silently replaced by a later placeholder ending
- on success it returns the exact reviewed Batch 45 request shape `{variable = eta_symbol, location = "infinity", strategy = "builtin::eta->infinity"}`; selected-terminal-node mismatch and reviewed builtin-request subset failures remain typed `boundary_unsolved`, while builtin-request `invalid_argument` such as empty `eta_symbol` still stop immediately
- `GenerateBuiltinCutkoskyPhaseSpaceBoundaryRequest(...)` is the first standalone reviewed phase-space boundary-request generator; it takes `(const ProblemSpec&, const std::string& eta_symbol = "eta")`, validates `ProblemSpec` first, rejects empty `eta_symbol` as an argument error, and on the supported subset returns `{variable = eta_symbol, location = "cutkosky-phase-space", strategy = ...}` without boundary values
- the supported Cutkosky request-generation subset is intentionally narrow: every propagator must be `Standard` or `Cut`, and at least one propagator must be `Cut`; well-formed specs outside that subset fail as typed `boundary_unsolved`, while malformed specs continue to fail as ordinary `invalid_argument` validation errors. When `family.loop_prescriptions` is absent, the reviewed request keeps the legacy strategy `builtin::cutkosky-phase-space`. When that metadata is present, the reviewed provider-selection subset narrows further: every cut propagator must derive the same loop-backed prescription through `DerivePropagatorPrescriptionFromLoopPrescriptions(...)`, and that derived value must match the raw cut `Propagator::prescription`; the request strategy then becomes exactly one of `builtin::cutkosky-phase-space::plus_i0`, `builtin::cutkosky-phase-space::minus_i0`, or `builtin::cutkosky-phase-space::none`. Mixed or mismatched cut surfaces fail closed as typed `boundary_unsolved`
- `GeneratePlannedCutkoskyPhaseSpaceBoundaryRequest(...)` takes `(const ProblemSpec&, const std::string& ending_scheme_name, const std::vector<std::shared_ptr<EndingScheme>>& user_defined_schemes, const std::string& eta_symbol = "eta")`
- it is the phase-space analogue of the reviewed single-name infinity helper: it calls `PlanEndingScheme(...)` exactly once, accepts only the exact singleton supported terminal-node list `{<family>::cutkosky-phase-space}`, and then delegates into `GenerateBuiltinCutkoskyPhaseSpaceBoundaryRequest(...)`
- ordinary ending resolution and planning failures preserve the existing `PlanEndingScheme(...)` diagnostics unchanged, while any non-phase-space selected node such as the reviewed loop-only `<family>::eta->infinity` selection from `Tradition` fails as typed `boundary_unsolved`
- `GenerateAmfOptionsEndingSchemeCutkoskyPhaseSpaceBoundaryRequest(...)` takes `(const ProblemSpec&, const AmfOptions&, const std::vector<std::shared_ptr<EndingScheme>>& user_defined_schemes, const std::string& eta_symbol = "eta")`
- it is the phase-space analogue of the reviewed ordered-list infinity helper: it reads only `amf_options.ending_schemes`, delegates ordered fallback exactly once through `PlanAmfOptionsEndingScheme(...)`, and then validates only that selected `EndingDecision` against the reviewed singleton phase-space-node subset before delegating into `GenerateBuiltinCutkoskyPhaseSpaceBoundaryRequest(...)`
- ordered planning-time fallback therefore remains exactly the reviewed `PlanAmfOptionsEndingScheme(...)` surface, while once one ending scheme has been selected the Cutkosky phase-space request helper no longer falls through to later endings on terminal-node mismatch; a reviewed loop-only selection such as builtin `Tradition` therefore surfaces its own typed `boundary_unsolved` terminal-node diagnostic instead of being silently replaced by a later phase-space ending
- `SolveAmfOptionsEndingSchemeEtaInfinitySeries(...)` takes `(const ProblemSpec&, const AmfOptions&, const std::vector<std::shared_ptr<EndingScheme>>& user_defined_schemes, const SolveRequest& request_template, const BoundaryProvider&, const SeriesSolver&, const std::string& eta_symbol = "eta")`
- it is a thin attach-and-solve wrapper over existing seams only: it delegates ordered-list request selection to `GenerateAmfOptionsEndingSchemeEtaInfinityBoundaryRequest(...)`, copies `request_template`, overwrites only `boundary_requests` with that singleton request, attaches through `AttachBoundaryConditionsFromProvider(...)`, and then calls the supplied `SeriesSolver` exactly once
- ordered ending planning is therefore preserved unchanged through the standalone helper, while the first successfully selected ending now also owns the resulting eta->infinity boundary-subset diagnostic; provider attachment diagnostics still propagate unchanged
- non-`ending_schemes` `AmfOptions` fields remain inert at this seam, and this wrapper does not widen into builtin provider registries, direct `SolveDifferentialEquation(...)` provider consultation, CLI behavior, or broader ending semantics beyond the exact singleton `<family>::eta->infinity` request
- `SolveAmfOptionsEndingSchemeCutkoskyPhaseSpaceSeries(...)` takes `(const ProblemSpec&, const AmfOptions&, const std::vector<std::shared_ptr<EndingScheme>>& user_defined_schemes, const SolveRequest& request_template, const BoundaryProvider&, const SeriesSolver&, const std::string& eta_symbol = "eta")`
- it is the phase-space analogue of the reviewed eta->infinity wrapper: it delegates ordered-list request selection to `GenerateAmfOptionsEndingSchemeCutkoskyPhaseSpaceBoundaryRequest(...)`, copies `request_template`, overwrites only `boundary_requests` with that singleton request, attaches through `AttachBoundaryConditionsFromProvider(...)`, and then calls the supplied `SeriesSolver` exactly once
- `SolveAmfOptionsEndingSchemeCutkoskyPhaseSpaceSeries(...)` also takes `(const ProblemSpec&, const AmfOptions&, const std::vector<std::shared_ptr<EndingScheme>>& user_defined_schemes, const SolveRequest& request_template, const std::vector<std::shared_ptr<BoundaryProvider>>& providers, const SeriesSolver&, const std::string& eta_symbol = "eta")`
- that overload is the first reviewed phase-space provider-registry wrapper: it preserves the same ordered ending-selection and request-copying behavior, then attaches through `AttachBoundaryConditionsFromProviderRegistry(...)` before calling the supplied `SeriesSolver` exactly once
- ordered ending planning is therefore preserved unchanged through the standalone helper, while the first successfully selected phase-space ending now also owns the resulting phase-space boundary-subset diagnostic; provider attachment diagnostics still propagate unchanged
- non-`ending_schemes` `AmfOptions` fields remain inert at this seam, and these wrappers do not widen into builtin provider registries, direct `SolveDifferentialEquation(...)` provider consultation, CLI behavior, or broader ending semantics beyond the exact singleton `<family>::cutkosky-phase-space` request
- the reviewed phase-space request/attach wrappers now consult `family.loop_prescriptions` only for builtin provider-strategy refinement on cut propagators, while topological cut analysis and automatic boundary-value generation remain deferred
- `BootstrapSeriesSolver` now requires an explicit manual start boundary on its supported Batch 39 subset and returns typed `boundary_unsolved` for missing or incompatible explicit start-boundary attachment before continuation begins
- Batch 44 keeps the provider seam separate from solving: `BootstrapSeriesSolver` and `SolveDifferentialEquation(...)` remain unchanged and do not consult `BoundaryProvider`
- Batch 45 and Batch 46 still do not add builtin or registered boundary providers, eta-to-infinity or phase-space boundary value computation, or automatic `BoundaryCondition` generation. The current reviewed runtime now also generates one reviewed Cutkosky phase-space request shape, one single-provider `AmfOptions` phase-space attach-and-solve wrapper, and one caller-supplied provider-registry attach seam plus a matching phase-space wrapper overload, but automatic boundary-value generation and direct `SolveDifferentialEquation(...)` provider consultation remain deferred
- these batches also do not widen CLI behavior or claim any automatic boundary semantics beyond explicit request-and-attach behavior

The first numeric coefficient-evaluation seam is now reviewed:

- `EvaluateCoefficientExpression(...)` evaluates the current reviewed coefficient-string grammar exactly with rational arithmetic over literals, symbols, parentheses, unary sign, `+`, `-`, `*`, and `/`
- `EvaluateCoefficientMatrix(...)` selects one coefficient matrix by variable name from a reviewed `DESystem`, evaluates every cell at one explicit substitution point, and returns exact canonical rationals without mutating the source `DESystem`
- exact numeric bindings are still supplied as strings and are themselves parsed through the same exact rational grammar, so reviewed substitution values such as `-10/3` remain exact
- unknown variable names, unresolved symbols, and malformed expressions fail deterministically as argument errors; division by zero fails deterministically as plain evaluation failure without attempting singular-point classification
- this batch does not rewrite or canonicalize stored coefficient strings, does not classify singularities, does not generate local series, does not replace the scaffolded `BootstrapSeriesSolver`, and does not widen into automatic boundaries, continuation, or CLI behavior

On the current worktree, that exact evaluator now has a separate narrow complex helper companion:

- `BuildComplexNumericEvaluationPoint(...)` merges `ProblemSpec.kinematics.numeric_substitutions` with the reviewed raw `complex_numeric_substitutions` surface, rejects invalid overlap, and still requires `complex_mode: true` when complex bindings are present
- `EvaluateComplexCoefficientExpression(...)` evaluates the same reviewed arithmetic grammar plus the literal imaginary unit `I`, returning exact real and imaginary rational parts in `ExactComplexRational`; caller-supplied binding names may not reuse the reserved symbol `I`
- `EvaluateComplexCoefficientMatrix(...)` evaluates one selected reviewed `DESystem` coefficient matrix at one explicit exact-complex substitution point without mutating the source `DESystem`
- `EvaluateComplexPointExpression(...)` parses one explicit point expression in the same `x` or `eta=x` style already used by the reviewed exact path, but resolves its RHS through the separate exact-complex helper bindings
- the original exact `EvaluateCoefficientExpression(...)` and `EvaluateCoefficientMatrix(...)` surfaces remain exact-only and still reject unresolved `I` input rather than silently switching semantics
- this helper slice does not yet widen singular-point detection/classification, regular or Frobenius series generation, continuation, solver execution, cache identity, contour planning, branch bookkeeping, Kira emission, or CLI behavior

On the current worktree, the first separate eta-contour and branch-ledger planning surface is now implemented on top of that exact-complex helper layer:

- `EtaContourHalfPlane`, `EtaContourSingularPoint`, `EtaContinuationPlan`, `FinalizeEtaContinuationContour(...)`, and `PlanEtaContinuationContour(...)` form a library-only contour-planning seam over reviewed exact-complex point evaluation
- both helpers first evaluate `DESystem.singular_points` through `BuildComplexNumericEvaluationPoint(...)` plus `EvaluateComplexPointExpression(...)`, so the legacy singular-point annotation list becomes a first live complex consumer without widening singular-point discovery itself
- `FinalizeEtaContinuationContour(...)` takes one explicit ordered contour-point list, resolves every point exactly at the requested complex kinematic binding, canonicalizes evaluated singular points by location using a deterministic representative expression and sorted evaluated-location order, rejects any contour point that lands on an evaluated singular point, rejects any straight contour segment that crosses an evaluated singular point, computes one integer `branch_winding` per unique evaluated singular point through the reviewed `long double` angle-projection subset, and fails explicitly when the exact rational coordinates fall outside that moderate-size reviewed projection range before fingerprinting the resulting contour packet deterministically
- `PlanEtaContinuationContour(...)` is the narrower automatic planner over that same finalized packet: on the reviewed subset it requires distinct finite start/target locations on one horizontal line, detects evaluated singular points that lie strictly between them on that line, chooses the selected `upper` or `lower` half-plane, inserts one deterministic three-point vertical detour per on-path singular point with clearance equal to one quarter of the minimum reviewed horizontal gap, and then finalizes the resulting contour through the same explicit validator/ledger path
- `EtaContinuationPlanManifest`, `MakeEtaContinuationPlanManifest(...)`, `SerializeEtaContinuationPlanManifestYaml(...)`, and `WriteEtaContinuationPlanManifest(...)` persist that reviewed contour packet atomically under `layout.manifests_dir/` using a caller-supplied run id that must remain a simple filename stem; the manifest records the selected half-plane, exact contour points, canonicalized exact evaluated singular points, per-singular branch winding on that reviewed projection subset, and the deterministic contour fingerprint
- this slice still does not integrate with live solver execution, `SolveRequest`, solved-path cache replay, typed runtime diagnostics, eta-to-zero endgame handling, branch-aware `pow/log` evaluation, Kira emission, CLI behavior, non-horizontal automatic planning, clearance heuristics near thresholds, or nontrivial winding support beyond the reviewed simple detour classes

On the current worktree, the first singular-kinematics physical-region guardrail seam is now implemented on the reviewed repo-local K0 one-mass real subset:

- `DescribeReviewedPhysicalKinematicsSubset()` freezes the reviewed subset identifier as `k0_one_mass_2to2_real_v1`
- `AssessPhysicalKinematicsForBatch62(...)` is a fail-closed exact-point recognizer on that same subset only: it matches the frozen K0 momentum/invariant/propagator/scalar-product surface, requires exact real `s`, `t`, and `msq` bindings with `msq > 0`, accepts only the reviewed open real region, returns `SingularSurface` on the reviewed threshold `s = 4*msq` or endpoint surface `t^2 - (2*msq - s)*t + msq^2 = 0`, and otherwise returns `UnsupportedSurface` or `SupportedReviewedSubset` without widening beyond that frozen subset
- `AssessInvariantGeneratedPhysicalKinematicsSegmentForBatch62(...)` is the first continuation-segment consumer on top of that point seam: it acts when invariant-generated `start_location` and `target_location` determine one reviewed real `s` segment, either through explicit real `s=...` assignments or, on the unambiguous single-invariant reviewed `s` surface only, through the same raw exact point-expression grammar accepted elsewhere on the solve surface. It also now acts on one reviewed real `t` continuation surface, either through explicit `t=...` assignments or, on the unambiguous single-invariant reviewed `t` surface only, through that same raw exact point-expression grammar; both forms define one closed real `t` segment. It also now acts on one reviewed `msq` singular-crossing continuation surface, either through explicit `msq=...` assignments or, on the unambiguous single-invariant reviewed `msq` surface only, through that same raw exact point-expression grammar; both forms define one closed real `msq` segment for the same reviewed threshold/endpoint crossing checks. The helper reuses the exact complementary `ProblemSpec` bindings, upgrades the assessment to `SingularSurface` when the closed real `s` segment crosses the reviewed threshold or endpoint surface, upgrades the assessment to `NearSingularSurface` when the closed real `s` segment stays within the frozen conservative exact guard band `msq/4` of either reviewed singular `s` locus, upgrades the assessment to `SingularSurface` when the closed real explicit or raw single-invariant `t` segment crosses the reviewed endpoint surface `t^2 - (2*msq - s)*t + msq^2 = 0`, and upgrades the assessment to `SingularSurface` when the closed real explicit or raw single-invariant `msq` segment crosses either the reviewed pair-production threshold `s = 4*msq` or that same reviewed endpoint polynomial
- `SolveInvariantGeneratedSeries(...)` and `SolveInvariantGeneratedSeriesList(...)` now call those guardrails before invariant-generated DE construction and return typed `physical_kinematics_not_supported`, `physical_kinematics_singular`, or `physical_kinematics_near_singular` diagnostics; on the reviewed single-invariant `s` surface that preflight now also covers raw point expressions, on the reviewed single-invariant `t` surface it now also covers both explicit `t=...` assignments and raw point expressions, and on the reviewed single-invariant `msq` surface it now also covers both explicit `msq=...` assignments and raw point expressions for those reviewed threshold/endpoint singular-crossing checks. The invariant-list wrapper now also opts into the same reviewed explicit `t` endpoint-crossing guardrail whenever the invariant list includes `t` and both continuation locations spell explicit `t=...` assignments, fail-closes the narrower non-`s` multi-invariant raw `t` surface when both continuation locations remain non-explicit and require callers to spell the reviewed `t` segment as `t=...`, and likewise keeps the reviewed explicit `msq` threshold/endpoint crossing checks when the invariant list includes `msq` and both continuation locations spell explicit `msq=...` assignments, even on broader reviewed multi-invariant requests. On the narrower reviewed mixed explicit/raw multi-invariant `msq` seam, when exactly one continuation location spells `msq=...` and the other remains a raw exact point expression evaluable on the reviewed subset, the invariant-list wrapper also routes that request into the same reviewed `msq` guardrail before DE construction; malformed mixed explicit/raw requests on that seam now fail closed with explicit `msq=...` guidance instead of falling through to downstream invariant generation. The singular summary still covers both singular physical points and reviewed continuation-segment crossings, while the near-singular summary remains limited to the reviewed exact `msq/4` `s`-segment margin only
- same-side reviewed interior `s` segments remain pass-through to the next invariant-generated validation layer instead of being rejected at the physical-kinematics preflight, as long as they also stay outside the frozen `msq/4` near-singular guard band
- this seam still does not widen eta-generated segment checking, ambiguous unlabeled multi-invariant continuation formats beyond the reviewed fail-closed `s=...`, non-`s` `t=...`, and malformed mixed explicit/raw `msq=...` guidance, fully non-explicit raw multi-invariant `msq` parsing or broader non-explicit multi-invariant `msq` continuation semantics beyond the reviewed explicit and mixed explicit/raw threshold/endpoint crossing checks, near-singular `t` margins, full reviewed open-region certification for arbitrary explicit or raw `s` segments, or any generic Landau/singularity analysis outside the frozen repo-local K0 subset

The first singular-point detection and classification seam is now reviewed:

- `DetectFiniteSingularPoints(...)` analyzes one selected reviewed coefficient matrix for one selected variable and returns matrix-derived finite singular locations only
- this analysis is matrix-authoritative: `DESystem.singular_points` remains preserved legacy annotation/runtime metadata and is not treated as the computed singular set
- the seam reuses the reviewed exact evaluator for exact passive-symbol arithmetic and exact point resolution, while keeping Batch 35 finite-point-only and coarse: returned points are either detected finite singular locations or not, and classified points are `Regular` or `Singular`
- additive simple-pole terms are preserved across unsimplified sums, but Batch 35 support is defined on the exact canonical surviving expression after exact duplicate-term combination, zero-net cancellation, zero-term elimination, and matched-factor cancellation; grouped same-denominator normalization across multiple terms remains supported only when all surviving grouped numerators are linear, so grouped same-denominator linear-numerator cancellation participates before pole extraction/classification and regular cancellations do not false-positive as singular; surviving numerator-factor products remain supported only as single-term survivors when the surviving denominator support is regular or one finite simple pole, and any same-denominator denominator group is rejected as unsupported only when more than one surviving canonical term remains in that group and any surviving grouped numerator is nonlinear; exact additive cancellation of semantically identical dead branches and exact identical multi-term quotients whose shared expression itself stays within these reviewed supported shapes are eliminated to regular terms before unsupported-form rejection; zero numerators short-circuit only after parse-time symbol resolution and zero-divisor preservation, and divisor-side normalization or identical-quotient shared-expression normalization that proves zero still fails as plain `division by zero`; matched linear-factor cancellation removes removable singularities such as `(eta-s)/(eta-s)` from the computed finite singular set; the direct simple-difference divisor carveout is syntactic and limited to the literal `eta-c` shape needed for reviewed cases such as `1/(eta-s)`, so parenthesized or multi-term constant-only RHS forms under `eta-(...)` remain unsupported; exact-identical quotient collapse is the only allowed multi-term divisor exception, but shared higher-order poles, shared multi-factor denominators, and surviving non-identical multi-term divisors remain unsupported even inside `E/E`
- unknown variable names, missing passive bindings, malformed coefficient expressions, malformed point expressions, and unsupported singular-form analysis fail deterministically
- this batch does not widen the reviewed Batch 34 grammar, does not generate local series, does not add Frobenius/resonance data, does not perform continuation, does not replace the scaffolded `BootstrapSeriesSolver`, and does not add boundary generation or CLI behavior

The first scalar regular-point local-series patch seam is now reviewed:

- `GenerateScalarRegularPointSeriesPatch(...)` takes a reviewed `DESystem`, one selected `variable_name`, one `center_expression`, one non-negative `order`, and passive numeric bindings
- the seam is scalar-only in Batch 36: it requires exactly one master and a declared `1x1` coefficient matrix for the selected variable, and it rejects non-scalar systems or unsupported matrix shapes locally
- center resolution reuses the reviewed exact evaluator grammar, and `SeriesPatch.center` stores the resolved exact point expression `<variable>=<value>`
- regular-point gating reuses the reviewed `ClassifyFinitePoint(...)` seam at that resolved center; when Batch 35 rejects an otherwise exactly finite scalar center because the original divisor shape lies outside the reviewed Batch 35 grammar, Batch 36 re-checks `Regular` on a temporary exact-constant scalar probe instead of widening Batch 35 itself
- the returned `SeriesPatch` is normalized: `order` is preserved, `basis_functions` contains the degree-`0..order` monomials in the exact local shift `(x-c)` represented as repeated products, `coefficients.size() == order + 1`, and `coefficients[0] == "1"` on the happy path
- coefficient generation is exact and scalar-only: Batch 36 expands the selected scalar coefficient locally, applies the exact recurrence for `I' = a(x) I`, and runs an internal coefficient-level residual self-check through the generated regular-point order before returning
- negative orders, singular centers, malformed center expressions, unknown variable names, missing passive bindings, malformed coefficient expressions, and coefficient shapes whose local quotient would require negative powers or cancellation beyond the visible requested truncation fail deterministically
- this batch does not add triangular or block-matrix patch generation, Frobenius / regular-singular handling, overlap matching, continuation, standalone solver wrappers, `SolveRequest` changes, `BootstrapSeriesSolver` replacement, boundary generation, or CLI behavior

The first upper-triangular matrix regular-point local propagator seam is now reviewed:

- `GenerateUpperTriangularRegularPointSeriesPatch(...)` takes a reviewed `DESystem`, one selected `variable_name`, one `center_expression`, one non-negative `order`, and passive numeric bindings
- the seam is narrow in Batch 37: it requires a declared selected coefficient matrix that is square and dimension-matched to `masters.size()`, and it supports only systems that are already upper-triangular in the declared master order through the requested local degree
- center resolution and `basis_functions` reuse the reviewed Batch 36 exact grammar and monomial local-shift basis; `UpperTriangularMatrixSeriesPatch.center` stores the resolved exact point expression `<variable>=<value>`
- regular-point gating still reuses `ClassifyFinitePoint(...)` at that resolved center, and the same narrow raw-divisor fallback remains local: only when Batch 35 rejects the original divisor shape as unsupported does Batch 37 re-check `Regular` on a temporary exact-constant full-matrix probe at the resolved center instead of widening Batch 35 itself
- the returned `UpperTriangularMatrixSeriesPatch` is identity-normalized: `order` is preserved, `basis_functions` spans degree `0..order`, `coefficient_matrices.size() == order + 1`, and `coefficient_matrices[0]` is the exact identity matrix on the happy path
- coefficient generation is exact and matrix-valued: Batch 37 expands the selected matrix locally, rejects any strictly lower-triangular coefficient that survives through the requested order, applies the exact recurrence `(n+1) C_{n+1} = sum_{m=0}^n A_m C_{n-m}`, and runs an internal exact matrix residual self-check through degree `order-1` before returning
- negative orders, singular centers, malformed center expressions, unknown variable names, missing passive bindings, malformed coefficient expressions, unsupported local quotients, non-square or dimension-mismatched selected matrices, and surviving strictly lower-triangular local support fail deterministically
- this batch does not add automatic block discovery or permutation, general dense-matrix support, Frobenius / regular-singular handling, overlap matching, continuation, standalone solver wrappers, `SolveRequest` changes, `BootstrapSeriesSolver` replacement, boundary generation, or CLI behavior

The first scalar regular-singular / Frobenius local-series patch seam is now reviewed:

- `GenerateScalarFrobeniusSeriesPatch(...)` takes a reviewed `DESystem`, one selected `variable_name`, one `center_expression`, one non-negative `order`, and passive numeric bindings
- the seam is scalar-only in Batch 41: it requires exactly one master and a declared `1x1` coefficient matrix for the selected variable, and it rejects non-scalar systems or unsupported matrix shapes locally
- center resolution reuses the reviewed exact evaluator grammar, and `ScalarFrobeniusSeriesPatch.center` stores the resolved exact point expression `<variable>=<value>`
- the seam is singular-only and local-simple-pole-gated: it rejects regular centers and higher-order poles locally from the exact Laurent expansion at the resolved center, accepts simple poles whose residue-stripped regular factor stays regular through the requested order, reuses `ClassifyFinitePoint(...)` when Batch 35 can classify the raw input directly, and still preserves the reviewed unsupported parenthesized direct-difference singular forms from Batch 35 such as `eta-(...)`, including internal-whitespace variants of that raw shape
- the returned `ScalarFrobeniusSeriesPatch` is normalized: `indicial_exponent` stores the exact simple-pole residue `rho`, `order` is preserved, `basis_functions` contains the degree-`0..order` monomials in the exact local shift `(x-c)` represented as repeated products, `coefficients.size() == order + 1`, and `coefficients[0] == "1"` on the happy path
- coefficient generation is exact and scalar-only: Batch 41 expands the selected scalar coefficient as `a(x) = rho / (x-c) + d(x)` around the resolved center, solves the reduced recurrence for `z_N` in `I_N(x) = (x-c)^rho z_N(x)`, and runs an internal exact residual self-check on the reduced regular factor through degree `order-1` before returning
- negative orders, regular centers, higher-order poles, malformed center expressions, unknown variable names, missing passive bindings, malformed coefficient expressions, and unsupported singular shapes fail deterministically
- this batch does not add logarithmic terms, resonance handling, multiple Frobenius branches, matrix or block singular patches, public Frobenius residual/overlap diagnostics, continuation changes, `SolveRequest` widening, passive-binding solve inputs, `BootstrapSeriesSolver` replacement, boundary generation, or CLI behavior

The first upper-triangular matrix regular-singular / Frobenius local propagator seam is now reviewed:

- `GenerateUpperTriangularMatrixFrobeniusSeriesPatch(...)` takes a reviewed `DESystem`, one selected `variable_name`, one `center_expression`, one non-negative `order`, and passive numeric bindings
- the seam is narrow in Batch 42: it requires a declared selected coefficient matrix that is square and dimension-matched to `masters.size()`, and it supports only systems whose simple-pole residue matrix is already diagonal in the declared master order and whose residue-stripped regular tail is already upper-triangular through the requested local degree
- center resolution and `basis_functions` reuse the reviewed exact evaluator grammar and monomial local-shift basis; `UpperTriangularMatrixFrobeniusSeriesPatch.center` stores the resolved exact point expression `<variable>=<value>`
- the seam is singular-only and local-simple-pole-gated: it rejects regular centers and higher-order poles locally from the exact Laurent expansion at the resolved center, accepts only per-cell Laurent order `>= -1`, reuses `ClassifyFinitePoint(...)` when Batch 35 can classify the raw input directly, and still preserves the reviewed unsupported parenthesized direct-difference singular forms from Batch 35 such as `eta-(...)`, including internal-whitespace variants of that raw shape
- the returned `UpperTriangularMatrixFrobeniusSeriesPatch` is normalized: `indicial_exponents` stores the exact diagonal simple-pole residues in declared master order, `order` is preserved, `basis_functions` contains the degree-`0..order` monomials in the exact local shift `(x-c)` represented as repeated products, `coefficient_matrices.size() == order + 1`, and `coefficient_matrices[0]` is the exact identity matrix on the happy path
- coefficient generation is exact and matrix-valued on the reviewed diagonal-residue subset: Batch 42 expands the selected matrix locally as `A(x) = D / (x-c) + sum_{m>=0} B_m (x-c)^m`, requires `D` to be diagonal, rejects any strictly lower-triangular `B_m` that survives through the requested order, applies the exact reduced recurrence `(n+1) C_{n+1} + C_{n+1} D - D C_{n+1} = sum_{m=0}^n B_m C_{n-m}`, and runs an internal exact reduced-equation self-check through degree `order-1` before returning
- compatible resonances are deterministic and narrow: when one recurrence denominator entry vanishes and the exact right-hand side entry is also zero, Batch 42 sets that coefficient entry to zero and continues; when the denominator vanishes but the right-hand side does not, Batch 42 rejects deterministically as requiring logarithmic resonance handling / logarithmic Frobenius terms
- negative orders, regular centers, higher-order poles, malformed center expressions, unknown variable names, missing passive bindings, malformed coefficient expressions, unsupported singular shapes, non-square or dimension-mismatched selected matrices, off-diagonal residue simple poles, surviving strictly lower-triangular regular-tail support, and forced logarithmic resonances fail deterministically
- this batch does not add dense or automatically discovered block decomposition, Jordan or off-diagonal residue support, explicit logarithmic basis functions or multiple Frobenius branches, public Frobenius residual/overlap diagnostics, continuation changes, `SolveRequest` widening, passive-binding solve inputs, `BootstrapSeriesSolver` replacement, boundary generation, or CLI behavior

The first exact regular-patch residual and overlap diagnostics seams are now reviewed:

- `EvaluateScalarSeriesPatchResidual(...)` takes one reviewed scalar `DESystem`, one selected `variable_name`, one already-generated `SeriesPatch`, one explicit `point_expression`, and passive numeric bindings, then returns the exact scalar residual `p'(x) - a(x) p(x)` as `ExactRational`
- `MatchScalarSeriesPatches(...)` takes one selected `variable_name`, two compatible scalar `SeriesPatch` values, one explicit `match_point_expression`, one distinct explicit `check_point_expression`, and passive numeric bindings, then returns exact `lambda` and exact `mismatch` where `lambda = p_right(match) / p_left(match)` and `mismatch = p_right(check) - lambda * p_left(check)`
- `EvaluateUpperTriangularMatrixSeriesPatchResidual(...)` takes one reviewed `DESystem`, one selected `variable_name`, one already-generated `UpperTriangularMatrixSeriesPatch`, one explicit `point_expression`, and passive numeric bindings, then returns the exact matrix residual `Y'(x) - A(x) Y(x)` as `ExactRationalMatrix`
- `MatchUpperTriangularMatrixSeriesPatches(...)` takes one selected `variable_name`, two compatible `UpperTriangularMatrixSeriesPatch` values, one explicit `match_point_expression`, one distinct explicit `check_point_expression`, and passive numeric bindings, then returns the exact `match_matrix` and exact `mismatch` where `match_matrix = Y_right(match) * inverse(Y_left(match))` and `mismatch = Y_right(check) - match_matrix * Y_left(check)`
- the Batch 38 seams stay library-only and exact: they consume already-generated regular patches, parse points with the reviewed exact evaluator grammar, keep residual and mismatch outputs exact instead of floating, and do not consult `SolveRequest`, `PrecisionPolicy`, norms, tolerances, or continuation policy
- the seam validates public patch storage narrowly before use: scalar diagnostics require non-negative `order`, `basis_functions.size() == order + 1`, `coefficients.size() == order + 1`, and an exactly resolved `patch.center`; matrix diagnostics require the analogous size checks, an exactly resolved `patch.center`, square stored coefficient matrices of one consistent dimension, and matching matrix dimensions across overlap pairs or against `masters.size()` for residual checks
- when callers supply passive bindings, public regular-patch centers, stored scalar coefficients, stored upper-triangular matrix coefficients, and match/check point expressions resolve against those same bindings before the exact residual or overlap arithmetic runs
- scalar residual evaluation still requires a reviewed scalar `1x1` system matrix, and matrix residual evaluation still requires a reviewed selected coefficient matrix that is square and dimension-matched to `masters.size()`
- match and check points are caller-supplied and must be distinct after exact resolution; malformed point expressions, missing passive bindings, unknown selected variable names, malformed public patch centers, malformed patch storage sizes, and matrix dimension mismatches fail deterministically
- singular residual or overlap evaluations are not reclassified: if the selected coefficient matrix is singular at the requested residual point, if `p_left(match)` vanishes, or if `Y_left(match)` is singular, Batch 38 propagates the underlying exact `division by zero` failure directly instead of adding singular-path logic
- this batch does not add regular-patch continuation, automatic point selection, overlap norms or tolerances, `SolveRequest` integration, `BootstrapSeriesSolver` replacement, dense/non-triangular matrix diagnostics, Frobenius / regular-singular handling, boundary generation, or CLI behavior

The first exact one-hop continuation solver seam is now reviewed through the reviewed Batch 39 regular path plus the reviewed Batch 43 mixed extension:

- `BootstrapSeriesSolver::Solve(...)` keeps the public `SeriesSolver` / `SolveRequest` surface unchanged and supports only two exact one-hop paths on the reviewed upper-triangular subset: the reviewed Batch 39 regular-start to regular-target path and the reviewed Batch 43 regular-start to regular-singular-target mixed path
- the solver currently requires a well-formed `DESystem` with exactly one declared differentiation variable and one explicit manual start boundary attached through the existing `boundary_requests` plus `boundary_conditions` surface
- when `SolveRequest.amf_requested_dimension_expression` is itself exactly numeric, the solver binds that exact value to the passive symbol name `dimension` on its reviewed exact path; when the normalized expression stays symbolic, it rewrites assembled standalone `dimension` identifiers on the live `DESystem` onto that symbolic carrier before exact coefficient evaluation, center classification, and residual checks, while explicit boundary-value parsing stays on the reviewed exact-only passive-binding path
- `start_location` and `target_location` are parsed exactly through the reviewed coefficient-evaluator grammar; malformed location expressions, malformed explicit boundary values, and unresolved symbols remain deterministic argument errors instead of solver-level failure codes
- the internal continuation order is fixed at `4`; Batch 39 and Batch 43 both choose `match = (start + target) / 2` and `check = (3*start + target) / 4` deterministically after exact point resolution
- the reviewed regular path reuses `GenerateUpperTriangularRegularPointSeriesPatch(...)`, `MatchUpperTriangularMatrixSeriesPatches(...)`, and `EvaluateUpperTriangularMatrixSeriesPatchResidual(...)` directly, with scalar systems treated only as the degenerate `1x1` upper-triangular case
- the reviewed Batch 43 mixed path keeps the same explicit manual start-boundary requirement and start-side regular patch seam, then generates the target-side singular patch internally with `GenerateUpperTriangularMatrixFrobeniusSeriesPatch(...)` on the reviewed Batch 42 diagonal-residue, no-log subset
- the reviewed Batch 43 mixed path is narrower than the reviewed Batch 42 local patch seam: continuation is attempted only when every target Frobenius indicial exponent is an exact integer, and fractional-exponent mixed requests return `failure_code = "unsupported_solver_path"` with an integral/integer-Frobenius-exponent summary instead of widening continuation semantics
- both supported paths use the deterministic `match`/`check` pair for exact acceptance; the regular path requires exact regular residuals plus exact overlap mismatch to vanish, while the mixed path requires the exact start-side regular residual, exact target-side Frobenius residual, and exact regular/Frobenius handoff mismatch to vanish entrywise
- on success the solver returns `diagnostics.success = true`, `residual_norm = 0.0`, `overlap_mismatch = 0.0`, empty `failure_code`, and a deterministic short success summary, while transported target values and the mixed handoff matrix remain internal and are not exposed on the public surface
- well-formed but unsupported or inexact one-hop requests return `failure_code = "unsupported_solver_path"`, including singular starts, unsupported matrix shape/support outside the reviewed upper-triangular subset, forced logarithmic resonances, fractional-exponent mixed targets, singular internal one-hop evaluations, and nonzero exact residual or handoff checks
- `PrecisionPolicy` and `requested_digits` remain accepted on `SolveRequest`; on the landed
  `Batch 54` surface, `EvaluatePrecisionBudget(...)` enforces the hard-cap preflight before patch
  generation, so requests whose `requested_digits` exceed `max_working_precision` return
  `failure_code = "insufficient_precision"` immediately
- invariant- and eta-generated solver handoffs layer a narrow internal retry controller on top of
  that surface: on retryable `insufficient_precision`, `requested_digits` stays fixed and the
  wrapper retries only when `EvaluatePrecision(...)` suggests a larger `working_precision` or
  `x_order`; on the landed `Batch 55` surface, generated-wrapper master-basis drift during
  DE construction returns `failure_code = "master_set_instability"` and exhausted monotone retry
  progress returns `failure_code = "continuation_budget_exhausted"` instead of falling back to the
  raw terminal retry diagnostic
- these batches do not add singular-start boundary semantics, singular-to-regular or singular-to-singular continuation, multi-hop continuation, passive-binding solve inputs, standalone `SolveDifferentialEquation(...)` retry integration, automatic boundary generation, public Frobenius residual or handoff diagnostics, or public transported-target output

The first standalone differential-equation solver wrapper seam is now reviewed:

- `SolveDifferentialEquation(const SolveRequest& request)` is a library-only thin wrapper over the reviewed Batch 39 and Batch 43 exact continuation solver surface
- unlike the generated handoffs, the wrapper still constructs the default solver via `MakeBootstrapSeriesSolver()`, invokes `Solve(request)` exactly once, and returns the resulting `SolverDiagnostics` unchanged (`src/solver/series_solver.cpp:3000-3002`)
- it does not add any new request or result type, transported-target output, malformed-input translation, or new well-formed solving semantics; the reviewed Batch 39 and Batch 43 exceptions and diagnostics pass through unchanged
- this batch does not rewire existing injected solver wrappers, add no-request overloads, or widen the standalone wrapper into the generated-handoff retry behavior, singular-start boundary semantics, broader singular/Frobenius continuation, multi-hop continuation, passive-binding solve inputs beyond the current local precision-budget preflight, CLI behavior, or examples

The first auxiliary-family transformation seam is also intentionally narrow:

- `EtaInsertionDecision` now carries selected propagator indices as the canonical selection surface; copied propagator expressions remain informational only in this bootstrap
- `ApplyEtaInsertion(...)` returns a typed transformed-spec result and never mutates the input `ProblemSpec`
- only the selected propagators are rewritten, and the bootstrap rewrite is deterministic string-level logic of the form `(<old expression>) + eta`
- `kinematics.invariants` appends `eta` exactly once and preserves existing order otherwise
- the transform preserves family name, targets, top sectors, scalar-product rules, numeric substitutions, and propagator `kind`/`variant`/`prescription`
- empty selections, duplicate indices, out-of-range indices, and selected auxiliary propagators fail locally with deterministic diagnostics; selected nonzero-mass propagators are now allowed on this reviewed eta-generated path, and rewritten selected propagators carry `Trim(original.mass)` in the transformed copy so the reducer-facing equal-mass surface stays coherent with planner grouping. This is only outer-whitespace trimming on selected rewritten literals, not broader mass canonicalization
- builtin eta mode `All` selects all non-auxiliary propagators by index; builtin mode `Prescription` is now the first truthful `Propagator::prescription` consumer on the reviewed local phase-space subset only: it validates the frozen raw `-1/0/1` vocabulary across the current propagator table, then scans the current declaration-order surface and selects only standard uncut `-i0` propagators. It preserves `mode_name == "Prescription"` and uses a distinct explanation string for that filtered subset. On the reviewed passive-linear subset inherited from `Batch 64f`, the same builtin planner now also stays truthful when one explicit `variant: "linear"` propagator remains in the family: it still selects only the standard uncut `-i0` quadratic slots, leaves the explicit linear slot passive, the direct `SolveBuiltinEtaModeSeries(...)` wrapper and `SolveBuiltinEtaModeListSeries(...)` forward the resulting eta-generated `DESystem`, locations, precision policy, requested digits, and solver diagnostics unchanged on that one-master fixture when `Prescription` is the selected builtin, the builtin-only `SolveAmfOptionsEtaModeSeries(...)` wrapper forwards that same reviewed passive-linear `DESystem`, locations, requested digits, and solver diagnostics unchanged while preserving the already-reviewed wrapper-owned live precision/runtime policy plus requested-`D0` / dimension-carrier metadata, and the mixed/user-defined `SolveAmfOptionsEtaModeSeries(...)` overload now matches the planned-`AmfOptions` helper tail on that same passive-linear solve path after ordered fallback from reviewed user-defined planning failures into builtin `Prescription`. This does not yet claim top-sector-aware or loop-derived upstream `AMFlowInfo["Prescription"]` parity, any linear rewrite semantics for the selected set itself, broader linear-propagator support, broader phase-space boundary semantics, or broader AMFlow-faithful `x` / gauge-link driver parity
- `BuildReviewedLightlikeLinearAuxiliaryPropagator(...)` is a second still-narrower helper in the same runtime surface: it takes one explicit `variant: "linear"` propagator index on the reviewed one-external lightlike subset only, requires exactly one declared external momentum symbol together with one unique exact-zero `n*n` rule for that symbol, parses only rational constants plus loop-`n` bilinears with rational coefficient factors, and returns one rewritten quadratic `Standard` propagator of the form `x*((L)^2) + (original)` while preserving the original mass literal and raw prescription and leaving the input `ProblemSpec` unchanged
- `ApplyReviewedLightlikeLinearAuxiliaryTransform(...)` is the first still-narrow full-spec consumer of that helper: on the same reviewed one-external lightlike subset it rewrites exactly one selected propagator through the helper, preserves every non-targeted `ProblemSpec` field, records the rewritten index plus trimmed `x_symbol`, and appends that invariant only when absent while leaving the input `ProblemSpec` unchanged
- neither the helper nor the full-spec transform touches eta planning, Kira preparation, reduction execution, or solver/wrapper behavior, and neither claims broader multi-external grammar, loop/external denominators, or otherwise more general AMFlow-faithful linear-driver parity
- on the current reviewed local phase-space subset, `KiraBackend` family YAML is now the first truthful Kira cut-surface consumer: emitted `integralfamilies.yaml` always carries an explicit 1-based `cut_propagators` list derived from `PropagatorKind::Cut`, including after the reviewed `Prescription` eta-insertion path, while reviewed loop-only surfaces stay on the explicit empty list. This still does not widen into Cutkosky boundary generation, phase-space boundary attachment, or broader upstream AMFlow/Kira parity
- builtin mode `Propagator` is a narrow bootstrap structural selector on the current reviewed subset only: it selects all non-auxiliary propagators in declaration order, preserves `mode_name == "Propagator"`, carries matching informational propagator-expression copies, and fails deterministically when that structural selection is empty; on the reviewed eta-generated path it now reuses the widened `ApplyEtaInsertion(...)` transform, including the same selected-propagator outer-whitespace trim on emitted mass literals and no broader mass canonicalization claim
- builtin mode `Mass` is a narrow bootstrap selector on the current reviewed subset only: it starts from the current local non-auxiliary declaration-order candidate surface, groups propagators by exact trimmed equal nonzero `mass` string, and then uses a narrow local syntactic analogue of the recovered upstream reduced-variable preference rather than full semantic analysis. Concretely, it tokenizes `scalar_product_rules` right-hand sides, prefers groups whose `mass` expression does not depend on those RHS identifiers after carving out exact standalone nonzero propagator-mass labels as local mass-parameter-like tokens, preserves declaration order within the chosen group, preserves `mode_name == "Mass"` plus matching informational propagator-expression copies, and falls back deterministically to the first equal nonzero group when no such locally independent group exists
- builtin modes `Branch` and `Loop` remain explicit bootstrap stubs on the last fully accepted
  baseline: the landed `Batch 50a` runtime carries an internal eta-topology preflight snapshot over
  the current family/kinematics surface, but the topology-analysis/candidate-analysis needed for
  real Branch/Loop selector semantics is still deferred. The landed `Batch 50b` packet on current
  `main` narrows that internal work into a topology-prerequisite bridge/prereq snapshot with
  explicit available-versus-missing field reporting over the same current surface. Durable
  clean-candidate evidence is now recorded for that internal packet. The landed `Batch 50`
  packet then adds a first supported selector slice over the single-top-sector
  squared-linear-momentum subset only: it derives first-Symanzik support from active non-auxiliary
  propagators, computes Branch/Loop groups, intersects with uncut candidates, and selects unique
  propagators deterministically before eta insertion. Broader topology parity and boundary claims
  remain deferred. Neither the landed `50a`/`50b` states nor the landed narrow `Batch 50` packet
  widens the last fully accepted public API
- the landed `Batch 49b` packet widens the bootstrap built-in planner seam only to that narrow local `Mass` selector plus the minimal downstream eta-insertion mass coherence needed for the reviewed eta-generated path. It does not claim full upstream topology/component-order `Mass` parity, broader same-priority tie-break parity, broader symbolic mass canonicalization, `Propagator::prescription` metadata interpretation, derivative generation changes, or broader orchestration/parity closure

## Upgrade Rules

- additive fields are allowed in specs and manifests
- existing field meaning cannot change silently
- new runtime modes require parity and diagnostics coverage
- new reducer backends must implement the same `ReductionBackend` contract
