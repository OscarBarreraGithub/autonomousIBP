# M6 Runtime-Lane Implementation Roadmap

Status: synthesis only. This document does not modify runtime code, does not
change qualifier verdicts, and does not flip any runtime flag.

## Scope

This roadmap synthesizes the three Tier C runtime gap analyses:

- `b63n` live Cutkosky, from
  `tools/reference-harness/specs/m6/lane149/b63n-live-cutkosky-gap-analysis.md`.
- `b64ag` full `gaugex -> 0` transport, from
  `tools/reference-harness/specs/m6/lane151/b64ag-full-contour-gap-analysis.md`.
- `b61n` full complex eta ODE transport, from
  `tools/reference-harness/specs/m6/lane153/b61n-full-contour-gap-analysis.md`.

It also applies the qualifier constraints in
`tools/reference-harness/specs/m6/lane148/m6-runtime-lane-qualifier-requirements.md`.
The qualifier is not a coefficient-count gate: M6 passes only when the phase-0
packet set is qualified and both `phase0_pending_ids` and
`blocked_phase0_examples` are empty (`tools/reference-harness/specs/m6/lane148/m6-runtime-lane-qualifier-requirements.md:8`,
`tools/reference-harness/specs/m6/lane148/m6-runtime-lane-qualifier-requirements.md:15`,
`tools/reference-harness/scripts/qualify_milestone_m6.py:645`).

## Shared Primitives

| Primitive | Exists now? | Evidence and implementation need |
| --- | --- | --- |
| Reviewed surface recognition and contour planning | Partial | `b61n` recognizes exactly the seven retained `complex_kinematics` masters (`src/cli/main.cpp:4614`) and validates the eta state (`src/cli/main.cpp:4627`). Its scaffold extracts complex poles and records a lower-half-plane plan, but ends by saying full eta-infinity-to-zero ODE propagation and Laurent fitting remain deferred (`src/cli/main.cpp:4720`, `src/cli/main.cpp:4790`). `b64ag` recognizes the reviewed nine-propagator gauge surface (`src/runtime/lightlike_propagator.cpp:234`) and builds a `gaugex` contour plan (`src/runtime/lightlike_propagator.cpp:1471`). `b63n` validates cut topology and records a Cutkosky contour audit (`src/runtime/cutkosky_transport.cpp:822`). These are planning/audit primitives, not live solvers. |
| Complex contour ODE transport | Missing for these AMFlow-state lanes | The closest general live endpoint extraction is the reviewed SRL-4 exact path, which builds exact regular/Frobenius patches, performs residual and overlap checks, and then sets `full_eta_zero_contour_applied=true` (`src/solver/series_solver.cpp:7148`, `src/solver/series_solver.cpp:7201`, `src/solver/series_solver.cpp:7223`). That path is exact-rational and upper-triangular; it is not wired to the retained `b61n` complex Numeric path or `b64ag` finite gauge-link samples. The retained `b61n` evaluator explicitly fits boundary samples, applies selected patches, and never solves rows 5 and 6 by a coupled contour ODE (`src/cli/main.cpp:8115`, `src/cli/main.cpp:8210`, `src/cli/main.cpp:8275`). A reusable numerical contour ODE layer still has to be built or narrowly specialized per lane. |
| Endpoint local model | Partial | The shared SRL local-model type records eta symbol, endpoint, residue matrix, basis functions, and `live_endpoint_extraction_ready` (`include/amflow/runtime/endpoint_local_model.hpp:14`). The analyzer constructs a restricted integer nonresonant Frobenius model (`src/runtime/endpoint_local_model.cpp:203`, `src/runtime/endpoint_local_model.cpp:240`) but sets `live_endpoint_extraction_ready=false` and states live extraction remains deferred (`src/runtime/endpoint_local_model.cpp:250`, `src/runtime/endpoint_local_model.cpp:260`). `b64ag` and `b63n` have local-model labels, but not produced endpoint terms. |
| Eta-zero finite-part or residue selection | Exists as fail-closed selectors | `b63n` has `PickCutkoskyEtaZeroTerm`, which rejects empty, logarithmic, multi-region, missing-zero, ambiguous, or unlabeled terms (`src/runtime/cutkosky_transport.cpp:692`, `src/runtime/cutkosky_transport.cpp:695`, `src/runtime/cutkosky_transport.cpp:707`, `src/runtime/cutkosky_transport.cpp:720`, `src/runtime/cutkosky_transport.cpp:727`, `src/runtime/cutkosky_transport.cpp:734`, `src/runtime/cutkosky_transport.cpp:740`). `b64ag` has a PickZeroRuleS-compatible finite-part selector that rejects missing endpoint terms, unresolved logs, multiple regions, implicit positive-start zeros, and absent power-zero terms (`src/runtime/lightlike_propagator.cpp:1141`, `src/runtime/lightlike_propagator.cpp:1144`, `src/runtime/lightlike_propagator.cpp:1154`, `src/runtime/lightlike_propagator.cpp:1161`, `src/runtime/lightlike_propagator.cpp:1174`, `src/runtime/lightlike_propagator.cpp:1193`). Producers for real transported terms remain missing. |
| Cutkosky residue model and branch metadata | Partial | The `b63n` endpoint model records automatic phase-space variables, domain, uncut roles, and factors (`src/runtime/cutkosky_transport.cpp:478`, `src/runtime/cutkosky_transport.cpp:485`, `src/runtime/cutkosky_transport.cpp:486`, `src/runtime/cutkosky_transport.cpp:493`), and likewise records the `feynman_prescription` two-body model (`src/runtime/cutkosky_transport.cpp:504`, `src/runtime/cutkosky_transport.cpp:520`, `src/runtime/cutkosky_transport.cpp:521`, `src/runtime/cutkosky_transport.cpp:530`). It also says live Cutkosky endpoint coefficients are deferred (`src/runtime/cutkosky_transport.cpp:474`) and the scaffold publishes no coefficients (`src/runtime/cutkosky_transport.cpp:835`). Weighted residue integration must still be built. |
| Cutkosky prefactor handling | Mostly missing as numeric series | The current helper emits reviewed strings for `K_1`, `K_2`, and `K_r` (`src/runtime/cutkosky_transport.cpp:283`). Lane 149 requires numerical or series expansion and multiplication into live residue coefficients (`tools/reference-harness/specs/m6/lane149/b63n-live-cutkosky-gap-analysis.md:99`). This is smaller than the residue derivation, but it is not yet an evaluated epsilon-series primitive. |
| Lightlike square-family and power normalization | Exists for reviewed surface | The `b64ag` runtime rewrites the affected D4,D5 propagators into the generated-square family (`src/runtime/lightlike_propagator.cpp:1095`, `src/runtime/lightlike_propagator.cpp:1107`) and audits `gaugex^(-sum affected powers)` target normalization (`src/runtime/lightlike_propagator.cpp:1118`, `src/runtime/lightlike_propagator.cpp:1125`). Full target-row reduction before finite-part extraction still has to use these primitives on transported six-master endpoint series. |
| Laurent fitting or epsilon-series fitting | Exists, but lacks lane-specific precision policy | Sample leading-order estimation exists (`src/cli/main.cpp:4224`). Boundary sample Laurent fitting exists (`src/cli/main.cpp:6881`), and solution sample fitting with guard terms exists (`src/cli/main.cpp:6906`). These can be reused after a live endpoint producer exists. The missing part is not the fit algorithm itself; it is producing accepted endpoint samples/terms and attaching precision diagnostics at the M6 floor. |
| Target reduction plumbing | Exists, but in the wrong order for full `b64ag` | The CLI can apply retained Kira target reduction after a successful solve (`src/cli/main.cpp:9300`). Lane 151 requires target reduction, affected-power normalization, finite-part extraction, and Laurent fitting to occur in that physics order for the endpoint series (`tools/reference-harness/specs/m6/lane151/b64ag-full-contour-gap-analysis.md:122`). This is a tractable wiring change once six-master endpoint series exist. |
| Boundary provider seam for Cutkosky | Partial and context-poor | `BoundaryRequest` carries only `variable`, `location`, and `strategy` (`include/amflow/core/boundary_data.hpp:11`). The Cutkosky registry still returns deferred providers (`src/solver/boundary_provider.cpp:202`), and the provider throws instead of returning values (`src/solver/boundary_provider.cpp:48`, `src/solver/boundary_provider.cpp:57`). A live provider must receive reviewed topology and target-basis context or extend the request contract. |

## Tractability Ranking

The ranking below covers every named concrete missing piece in the three gap
docs. Rank 1 is the lowest-hanging implementation item; rank 18 is the hardest.

| Rank | Lane | Missing piece | Rationale |
| ---: | --- | --- | --- |
| 1 | `b61n` | Keep final AMFlow solution samples out of runtime input | The stripped path already records `final_solution_samples_used_as_input=false` in the contour audit (`src/cli/main.cpp:4762`) and selected endpoint summaries (`src/cli/main.cpp:4933`). The work is guardrail/test preservation, not new physics. |
| 2 | `b63n` | Remove retained final-solution sample dependence | The Cutkosky scaffold and selected coefficient audit already set `retained_solution_samples_used=false` (`src/runtime/cutkosky_transport.cpp:840`, `src/runtime/cutkosky_transport.cpp:928`). Future code must preserve this, not discover a new mechanism. |
| 3 | `b64ag` | Fit epsilon Laurent coefficients after endpoint extraction | Fit helpers already exist (`src/cli/main.cpp:6881`, `src/cli/main.cpp:6906`) and the selected b64ag path already calls them after endpoint matching (`src/cli/main.cpp:8059`). This is reusable once endpoint terms are live. |
| 4 | `b63n` | Expand and apply `K_2(eps)` numerically | The formula string exists (`src/runtime/cutkosky_transport.cpp:283`). Implementing an epsilon-series prefactor is small compared with deriving the weighted residue integrals, though it still needs precision diagnostics and multiplication into live terms. |
| 5 | `b64ag` | Apply target reduction before finite-part extraction | Reduction plumbing exists after diagnostics (`src/cli/main.cpp:9300`), and gauge-link normalization already records the affected powers (`src/runtime/lightlike_propagator.cpp:1118`). The main work is moving the algebra to endpoint series before PickZeroRuleS. |
| 6 | `b64ag` | Feed real endpoint terms into PickZeroRuleS | The finite-part selector is already implemented and fail-closed (`src/runtime/lightlike_propagator.cpp:1141`). It needs transported series producers, not a new selection rule. |
| 7 | `b63n` | Replace symbolic eta-zero selection with propagated terms | `PickCutkoskyEtaZeroTerm` already enforces the eta-zero selection contract (`src/runtime/cutkosky_transport.cpp:692`). The missing input is real propagated residue terms. |
| 8 | `b64ag` | Publish new optional phase-0 packet after runtime evidence passes | Packet qualification mechanics are already defined by lane 148 (`tools/reference-harness/specs/m6/lane148/m6-runtime-lane-qualifier-requirements.md:93`). It is low code complexity but strictly blocked until the runtime evidence exists. |
| 9 | `b61n` | Publish coherent optional phase-0 packet after runtime evidence passes | Same packet mechanics as rank 8, but the live seven-master evidence is harder to obtain first (`tools/reference-harness/specs/m6/lane148/m6-runtime-lane-qualifier-requirements.md:37`). |
| 10 | `b63n` | Publish optional capture packet after runtime evidence passes | The packaging work is straightforward, but `b63n` maps to both `automatic_phasespace` and `feynman_prescription`, so both rows must leave the blocker frontier (`tools/reference-harness/specs/m6/lane148/m6-runtime-lane-qualifier-requirements.md:64`). |
| 11 | `b64ag` | Add full b64ag endpoint transport evaluator | There is already a selected evaluator gate and fallback scaffold (`src/cli/main.cpp:8004`). Expanding it to all accepted targets is substantive but has the best local scaffolding among the three lanes. |
| 12 | `b61n` | Extract and fit endpoint Laurent coefficients from transported samples | The fit side exists, but rows 5 and 6 must first be transported to eta=0. This is mostly a consumer of the missing ODE work. |
| 13 | `b63n` | Add non-deferred Cutkosky boundary provider | The provider seam exists, but the request currently lacks `ProblemSpec` or target metadata (`include/amflow/core/boundary_data.hpp:11`) and the registry is deliberately deferred (`src/solver/boundary_provider.cpp:202`). This is code-tractable only after the boundary values are defined. |
| 14 | `b64ag` | Solve second DE block and coupled downstream masters | The selected path uses the first DE block only (`src/cli/main.cpp:8011`, `src/cli/main.cpp:8032`), and the remaining six-master transport is explicitly deferred (`src/runtime/lightlike_propagator.cpp:1604`). This is the central physics/runtime step for `b64ag`. |
| 15 | `b61n` | Construct valid eta-infinity initial data for every retained master | `ApplyEtaInfinityAsymptoticTransportFromDE` fills only missing leading sample coefficients and skips masters with existing boundary samples (`src/cli/main.cpp:4135`, `src/cli/main.cpp:4148`, `src/cli/main.cpp:4153`). A controlled finite-start or infinity-variable expansion with truncation/error diagnostics must be designed. |
| 16 | `b61n` | Transport coupled rows, not just selected primitives | The current scalar transport rejects off-diagonal coupling (`src/cli/main.cpp:4891`) and the primitive bubble extension writes only four reviewed masters (`src/cli/main.cpp:6650`, `src/cli/main.cpp:6666`). Rows `box[1,0,1,1]` and `box[1,1,1,1]` need inhomogeneous coupled transport. |
| 17 | `b61n` | Add live b61n complex contour propagator | The current contour scaffold is an audit, not a solver (`src/cli/main.cpp:4790`), and the available SRL-4 exact path is not the retained complex AMFlow-state path (`src/solver/series_solver.cpp:7115`). This is a shared missing engine with branch and precision risk. |
| 18 | `b63n` | Implement automatic phase-space weighted residue evaluator | This is the hardest item because the current code records only roles and endpoint classifications (`src/runtime/cutkosky_transport.cpp:486`, `src/runtime/cutkosky_transport.cpp:499`) while lane 149 needs angular/invariant moment expansion, branch ledger, and Laurent terms for weighted residues (`tools/reference-harness/specs/m6/lane149/b63n-live-cutkosky-gap-analysis.md:91`). The `feynman_prescription` companion row has the same new-physics character. |

## Free Wins

- Do not build a new Laurent fitter first. Boundary and solution sample Laurent
  fitting already exist (`src/cli/main.cpp:6881`, `src/cli/main.cpp:6906`).
- Do not build a new PickZeroRuleS selector for `b64ag`. The fail-closed
  finite-part selector exists and already audits dropped singular powers
  (`src/runtime/lightlike_propagator.cpp:1141`, `src/runtime/lightlike_propagator.cpp:1188`).
- Do not build a new Cutkosky eta-zero selector for `b63n`. The selector exists;
  it just lacks live propagated terms (`src/runtime/cutkosky_transport.cpp:692`).
- Do not rederive the reviewed `b64ag` source surface, square-family rewrite, or
  D4,D5 power normalization. Those are already implemented
  (`src/runtime/lightlike_propagator.cpp:234`,
  `src/runtime/lightlike_propagator.cpp:1095`,
  `src/runtime/lightlike_propagator.cpp:1118`).
- Do not spend a lane proving that `full_eta_zero_contour_applied` by itself
  flips M6. Lane 148 and the composer make clear that the blocker retires through
  qualified phase-0 packet state, not by a raw flag alone
  (`tools/reference-harness/specs/m6/lane148/m6-runtime-lane-qualifier-requirements.md:8`,
  `tools/reference-harness/scripts/qualify_milestone_m6.py:645`).

## True Blockers

- `b63n` weighted Cutkosky residue construction is a real physics blocker. The
  code knows the variables, endpoint roles, and prescription bookkeeping, but it
  does not evaluate weighted angular/invariant moments or the two-body
  `feynman_prescription` subintegrals. This needs a reviewed derivation, paper
  reference, external CAS check, or human physics review before code can honestly
  publish coefficients.
- A robust high-precision complex contour ODE engine is not present for retained
  AMFlow states. The existing exact SRL-4 path is valuable but too narrow for
  `b61n` and `b64ag` retained numeric paths (`src/solver/series_solver.cpp:7148`,
  `src/solver/series_solver.cpp:7223`). Implementing a generic solver without
  an external numerical method review would be risky.
- `b61n` eta-infinity finite-start data is not just a parser task. The current
  helper fills leading asymptotic samples (`src/cli/main.cpp:4135`) but does not
  provide controlled finite-start values with truncation and error diagnostics.
- `b64ag` full six-master endpoint transport is the least blocked of the three
  substantive physics lanes, but the second DE block and target-surface finite
  parts still need review against the retained AMFlow equations before any M6
  packet promotion.

## Recommended Next Tier B Lane

Spawn exactly one substantive code lane next:
`lane156-b64ag-reduced-finite-part-functional`.

The lane should implement the reduced `b64ag` gauge-link endpoint finite-part
functional as a runtime primitive, without wiring CLI full-contour success and
without touching M6 metadata. It should consume synthetic or fixture endpoint
terms for the reviewed six-master gauge-link basis, apply D4,D5 affected-power
normalization, apply retained target reduction before finite-part extraction, and
use the existing PickZeroRuleS-compatible selector. Full endpoint transport from
finite `gaugex=1/40` boundary samples remains the following lane.

This is the best next lane because it reuses the most existing infrastructure:
the reviewed surface gate (`src/runtime/lightlike_propagator.cpp:234`), square
family builder (`src/runtime/lightlike_propagator.cpp:1095`), target power
normalization (`src/runtime/lightlike_propagator.cpp:1118`), contour audit
(`src/runtime/lightlike_propagator.cpp:1471`), finite-part selector
(`src/runtime/lightlike_propagator.cpp:1141`), selected endpoint evaluator
(`src/cli/main.cpp:8004`), Laurent fitting (`src/cli/main.cpp:6906`), and target
reduction call site (`src/cli/main.cpp:9300`). `b61n` needs a broader complex
ODE/asymptotic handoff first, and `b63n` needs new residue physics.

Exact file targets:

- `include/amflow/runtime/lightlike_propagator.hpp`: add data structures for
  six-master endpoint terms, target-reduction terms, reduced finite-part target
  terms, and per-target failure diagnostics.
- `src/runtime/lightlike_propagator.cpp`: implement the reduced finite-part
  functional over supplied endpoint terms, reuse
  `ApplyLightlikeGaugeLinkPowerNormalization` and
  `ExtractLightlikeGaugeLinkEndpointFinitePart`, and preserve fail-closed
  diagnostics when target reduction is incomplete, multiple endpoint regions are
  present, or a power-zero term is absent.
- `tests/singular_runtime_lane_tests.cpp`: add unit coverage for target
  reduction before PickZeroRuleS, D4,D5 normalization, missing-term rejection,
  multiple-region rejection, retained-solution-sample non-consumption, and no
  selected-prefix false promotion.

Exact physics piece:

```text
FP_{gaugex=0} gaugex^(-nu(t))
  sum_j R_tj(gaugex, eps) F_j(gaugex, eps)
```

for every accepted retained `linear_propagator` target `t`, where `F_j` is the
six-master solution transported from finite `gaugex=1/40`, `R_tj` is the retained
target-reduction row, and `nu(t)` is the D4,D5 affected power sum.

Acceptance test:

```text
cmake --build build --target amflow-tests --parallel 1
./build/amflow-tests
./build/singular-runtime-lane-tests
```

Add a named singular-runtime test such as
`B64agGaugeLinkReducedFinitePartAppliesTargetReductionBeforePickZeroRuleSTest`.
The lane should pass only if the runtime primitive produces the expected reduced
finite part from fixture endpoint terms, rejects missing or ambiguous terms, and
all existing b64ag scaffold tests still keep `full_eta_zero_contour_applied=false`.
No CLI result should report full-contour success in this first reduced-functional
lane.

## Guardrails For The Next Lane

- Do not edit `tools/reference-harness/specs/m6/lane148` or any M6 qualifier
  output until a coherent optional phase-0 packet exists.
- Do not change comparator tolerances, add sentinel `999` matches, compare a
  value against itself, or hardcode zeros.
- Do not read retained final AMFlow solution samples as runtime boundary input.
- Do not set `full_eta_zero_contour_applied=true` on selected endpoint
  coefficient paths. The current result writer distinguishes full contour from
  selected endpoint scope (`src/cli/main.cpp:9076`, `src/cli/main.cpp:9087`,
  `src/cli/main.cpp:9095`).
