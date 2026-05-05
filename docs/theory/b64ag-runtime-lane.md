# b64ag Runtime Lane Theory Design

Status: peer-reviewed design only. This document does not close `b64ag`,
does not remove `linear_propagator` from pending phase-0 metadata, and does not
claim M6 readiness.

## Review Approval

This document synthesizes the required four-role review:

- Role A, physicist-implementer: APPROVE. The lane requires a future full
  gauge-link `gaugex -> 0` endpoint runtime, not retained solution fitting.
- Role B, independent reviewer: APPROVE for design-only scope. Runtime closure
  remains blocked until live >=50 digit packet evidence exists.
- Role C, Mathematica-source auditor: APPROVE source audit. Existing C++ parity
  is retained-cache parity only, not full gauge-link reconstruction.
- Role D, synthesizer: APPROVE design-only scope, conditional on preserving the
  active `b64ag` blocker and reconciling the four AMFlow input targets with the
  nine-target retained packet surface.

The main resolved discrepancy is scope: Role B correctly blocks runtime and M6
closure today, while the lane request asks for a theory design. This document
therefore defines the future implementation and evidence contract without
claiming that the contract is already implemented.

## Source Roots

The prompt-named AMFlow path:

`/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/work/amflow/examples/`

is absent on this node. The audited equivalent AMFlow examples used here are:

- `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/lane101-gg-to-gammagamma-light-quark-mi/amflow/examples/linear_propagator/run.wl`
- `/n/holylabs/schwartz_lab/Lab/obarrera/reference-inputs/autonomousIBP/cpc/amflow-gitlab-1.1-extracted/examples/linear_propagator/run.wl`

The current in-repo retained phase-0 state is:

`tools/reference-harness/specs/phase0/linear_propagator.amflow-state.json`.

## Existing Evidence Boundary

Lane119, commit `2d35ce4`, found that `b64ag` is still open. The M6 survey says
the required evidence for `linear_propagator` is a live lightlike-linear or
gauge-link runtime capture accepted as an optional phase-0 packet with at least
50 correct digits and the required failure-code audit. It also says the current
route is retained finite solution-sample ingestion, with full loop-boundary
reconstruction and endpoint contour execution still deferred.

The qualification template keeps `linear_propagator` in
`current_evidence_state: cataloged-pending-capture`, assigns
`next_runtime_lane: b64ag`, and attaches the `core-package-family-default`
profile (`tools/reference-harness/templates/qualification-benchmarks.json:117-125`).
That profile has `minimum_correct_digits: 50`, and the default failure-code
profile requires explicit `insufficient_precision`, `master_set_instability`,
`boundary_unsolved`, and `continuation_budget_exhausted` reporting
(`tools/reference-harness/templates/qualification-benchmarks.json:9-35`).
The M6 composer only treats phase-0 runtime lanes as closed when
`phase0_pending_ids` and `blocked_phase0_examples` are both empty and the
phase-0 packet set is qualified
(`tools/reference-harness/scripts/qualify_milestone_m6.py:645-650`).

Current retained evidence must not be promoted to M6 closure:

- `linear_propagator.amflow-state.json` states that C++ can fit retained final
  solution samples, but that general gauge-link DE transport remains deferred
  (`linear_propagator.amflow-state.json:61-63`).
- The current retained run reports only retained solution-sample fitting:
  `Evaluated retained AMFlow solution samples for 9 master coefficient set(s)`
  (`tools/reference-harness/specs/m5/proof-runs/lane45/linear_propagator.cpp-result.json:997-1000`).
- The retained comparison passes at 30 digits with minimum digit agreement 31,
  not the M6 50-digit floor
  (`tools/reference-harness/specs/m5/comparisons/lane45/linear_propagator.compare.json:750-755`).

## AMFlow Reference Surface

The upstream example defines:

- family `gauge`;
- loop momenta `{l1, l2, l3}`;
- external leg `{n}`;
- replacement `{n^2 -> -1}`;
- nine propagators
  `{l1^2, l2^2, l3^2, 1 + l1*n, 1/2 + l1*n + l2*n + l3*n,
  (l1+l2)^2, (l1+l3)^2, (l2+l3)^2, -1 + l2^2 + 2*l2*n}`;
- four handwritten requested integrals;
- `precision = 20`, `epsorder = 10`;
- `SolveIntegralsGaugeLink[integrals, precision, epsorder]`.

Those facts are in the reference `run.wl:13-29`. The phrase
"lightlike-linear" in current C++ helper names should not hide this source fact:
the pinned phase-0 AMFlow example declares `n^2 -> -1`. A b64ag implementation
therefore cannot be only the existing strict `n^2 == 0` auxiliary rewrite. It
needs a reviewed gauge-link generated-square endpoint path, or an explicitly
reviewed adapter proving equivalence on this source surface.

AMFlow's gauge-link algorithm is in `AMFlow.m:1238-1335`:

- `GenerateSquare` leaves already-quadratic propagators unchanged and rewrites
  linear denominators as `factor^2 + prop/symbol`; unsupported denominators
  abort (`AMFlow.m:349-357`).
- `SolveIntegralsGaugeLink` introduces `gaugex`, rewrites propagators, and finds
  affected propagator positions (`AMFlow.m:1281-1289`).
- If no affected propagator exists, it falls back to ordinary `SolveIntegrals`
  (`AMFlow.m:1289-1291`).
- It reduces the targets and multiplies each target-row reduction by
  `gaugex^(-sum affected powers)` (`AMFlow.m:1297-1299`).
- It builds a DE in `gaugex`, aborting if the DE master set does not contain the
  reduced master set (`AMFlow.m:1301-1303`).
- It chooses epsilon samples and working precision, chooses a finite boundary
  point one tenth of the nearest nonzero DE pole, solves the boundary masters,
  and writes `{gaugex -> point, boundary}` (`AMFlow.m:1305-1321`).
- `ExpandGaugeX` substitutes the gauge-link variable into DESolver's internal
  `eta`, calls `SolveAsyExp`, applies the target reduction to the master
  asymptotics, and applies `PickZeroRuleS` (`AMFlow.m:1238-1262`).
- Only after endpoint extraction does AMFlow fit the epsilon Laurent expansion
  (`AMFlow.m:1324-1329`).

The generated retained state mirrors this: its boundary point is
`gaugex -> 1/40`, target is `gaugex=0`, singular point is `gaugex=0`, and
`solution_sample_cache.enabled` is true
(`linear_propagator.amflow-state.json:66-68`, `:506-518`). The generated
`solve.wl` stored in the state shows the variable-name bridge: the physical
runtime variable is `gaugex`, while the DESolver local asymptotic variable is
named `eta` (`linear_propagator.amflow-state.json:54-57`).

## Structural Requirements

b64ag is not the b63n Cutkosky phase-space lane. It does not primarily require
reconstructing phase-space cut residues from a cut vector. It does require
endpoint residue data in the DE sense: local `gaugex=0` asymptotics, a branch or
finite-part ledger, and an explicit IR subtraction rule for singular endpoint
powers.

The runtime must perform these operations itself:

1. Build the gauge-link square-generated family from the original linear
   denominators.
2. Reduce the requested targets and record the affected-propagator normalization
   exponent for each target.
3. Build the `gaugex` differential equation.
4. Pick and audit a finite boundary point away from nonzero DE poles.
5. Solve or replay finite boundary master values without reading final endpoint
   `solution` samples as the source of truth.
6. Transport each epsilon sample to `gaugex=0`.
7. Apply the finite-part IR subtraction equivalent to AMFlow `PickZeroRuleS`.
8. Fit epsilon coefficients after the endpoint extraction.

Retained solution-sample fitting is insufficient because it starts after steps
1-7 have already been done by Mathematica. It can check that C++ parses and fits
the retained result list, but it cannot validate the gauge-link DE construction,
the boundary solve, the endpoint contour, the IR subtraction, or the target-row
normalization. It also emits retained-cache runtime wording and leaves
`full_eta_zero_contour_applied` false through the current result schema
(`src/cli/main.cpp:6642-6649`, `:6467-6500`).

## Endpoint Analytic Structure

Let `x = gaugex`. Let `P` be the set of propagator positions whose denominator
is changed by `GenerateSquare`. For a target integral with index vector `a`,
define:

`nu(a) = sum_{i in P} a_i`.

AMFlow multiplies that target's reduction row by `x^(-nu(a))`
(`AMFlow.m:1297-1299`). For the reduced master vector `F(x, eps)`, the local
endpoint transport solves:

`dF/dx = A(x, eps) F`,

where `x=0` is a regular singular endpoint on the reviewed b64ag surface. The
closed-form local model is the Frobenius expansion:

`F_j(x, eps) = sum_r x^r sum_m f_{j,r,m}(eps) (log x)^m`.

The first reviewed implementation should accept the AMFlow-compatible subset:

- one integer-power region after target-row combination;
- no unresolved logarithmic endpoint ambiguity;
- finite endpoint value obtained from the `x^0` coefficient;
- fail-closed behavior if multiple integer regions, fractional powers, or
  unsupported logarithmic structures appear.

For a target `t`, with target reduction row `R_tj(x, eps)`, the endpoint value
is:

`I_t(eps) = FP_{x=0} x^(-nu(t)) sum_j R_tj(x, eps) F_j(x, eps)`.

Here `FP_{x=0}` is AMFlow's IR finite part. In `PickZeroRuleS` form
(`diffeq_solver/DESolver.m:1053-1061`):

- no integer-power key returns zero;
- more than one integer-power region aborts;
- a positive starting power returns zero;
- a zero starting power returns the zero-power coefficient;
- a negative starting power drops the singular coefficients and returns the
  coefficient at power zero, while logging the dropped terms.

That is the analytic eta-zero endpoint for b64ag: not a retained numeric sample
fit, but a finite-part coefficient of the gauge-link endpoint asymptotic series.
The source uses the name `eta` inside DESolver, but the externally audited
variable and packet field are `gaugex`.

## C++ Implementation Contract

The implementation must keep the existing retained `automatic_loop`
`TransportThroughEpsOrder` path distinct. That helper currently applies selected
primitive endpoint coefficients for `automatic_loop` only
(`src/cli/main.cpp:4691-4715`). b64ag needs a sibling full gauge-link endpoint
runtime, not another selected-coefficient upsert.

Recommended new helpers:

- `IsGaugeLinkEtaZeroRuntimeState(...)`: recognize `benchmark_id ==
  "linear_propagator"` or explicit `gauge_link` metadata, `variable ==
  "gaugex"`, finite start `gaugex -> ...`, target `gaugex=0`, and singular
  point `gaugex=0`.
- `ParseGaugeLinkRuntimeFiles(...)`: load audited DE, reduction, boundary,
  epsilon samples, target list, master lists, and source fingerprints. Reject a
  state that only offers final `solution` samples when the caller requests full
  b64ag runtime evidence.
- `BuildGaugeLinkSquareFamily(...)`: implement AMFlow `GenerateSquare`
  semantics for the reviewed denominator grammar. Reuse existing propagator
  parsing where possible, but do not require the current strict lightlike
  auxiliary selector when the source has `n^2 -> -1`.
- `ApplyGaugeLinkPowerNormalization(...)`: compute `nu(a)` and multiply target
  reduction rows by the required `x^(-nu(a))` factor before endpoint extraction.
- `SelectGaugeLinkBoundaryPoint(...)`: reproduce and audit the nonzero-pole
  boundary choice, including pole list, selected distance, rationalization, and
  boundary point.
- `SolveGaugeLinkFiniteBoundary(...)`: run or replay the finite boundary solve
  for the DE masters. Missing or incomplete boundary values must return
  `boundary_unsolved`.
- `ExtractGaugeLinkEndpointFinitePart(...)`: transport a single epsilon sample
  to `x=0` and apply the `PickZeroRuleS` finite-part rule. Multiple integer
  regions or unsupported local forms should return `continuation_budget_exhausted`
  unless a narrower typed code is reviewed.
- `TransportGaugeLinkThroughEpsOrder(...)`: sample-wise endpoint extraction,
  followed by the existing Laurent fit machinery used for result emission.
- `MakeGaugeLinkEtaZeroAudit(...)`: emit the endpoint audit fields needed to
  distinguish live gauge-link runtime from retained cache ingestion.

The result contract should use existing `SolverDiagnostics` fields where
possible:

- `success: true`;
- `full_eta_zero_contour_applied: true`;
- `eta_endpoint_contour_fingerprint` or a b64ag-specific contour fingerprint;
- `eta_endpoint_extraction_fingerprint`;
- `target_epsilon_coefficients` populated after endpoint extraction and epsilon
  fitting;
- no deferred blocked reason.

The CLI JSON should report:

- `transport_applied: true`;
- `transport_scope: "eta-zero-full-contour-endpoint-coefficients"` or a
  b64ag-specific full gauge-link spelling;
- `runtime_application: "full-gauge-link-gaugex-zero-contour-endpoint-extraction"`
  or a compatible reviewed spelling;
- `full_eta_zero_contour_applied: true`;
- `ir_subtraction_applied: true`;
- variable `gaugex`, DESolver local variable `eta`, start point, target point,
  singular endpoint, finite boundary point, pole list, dropped singular powers,
  finite-part order `0`, epsilon samples, precision settings, DE/reduction
  fingerprints, and source AMFlow/Kira provenance.

Failure codes must align with the M6 profile:

- `boundary_unsolved`: finite boundary solve/replay data are missing or
  incomplete.
- `master_set_instability`: DE masters do not contain reduced masters, matching
  the AMFlow abort condition at `AMFlow.m:1303`.
- `continuation_budget_exhausted`: endpoint contour, local expansion, or
  finite-part extraction exceeds the reviewed subset or budget.
- `insufficient_precision`: requested 50-digit threshold cannot be met.

## Qualification Surface

The AMFlow source file lists four handwritten input integrals (`run.wl:25`), but
the retained phase-0 packet currently compares nine `gauge[...]` targets and 57
coefficients. The nine-target list is visible in the retained state reduction
targets (`linear_propagator.amflow-state.json:376-502`) and in the lane45
comparison summary (`linear_propagator.compare.json:750-755`).

For M6, the qualifying surface should be the packet surface consumed by the
phase-0 harness, not merely the four handwritten inputs. A future implementation
may choose to regenerate the packet surface, but the closure rule is:

- every target in the accepted optional capture packet must be produced by the
  live C++ gauge-link endpoint runtime;
- the packet must be compared against an AMFlow golden captured with enough
  precision for the M6 threshold;
- no unqualified leftover retained-only target may remain hidden in the packet.

## Verification Plan

1. Baseline guard: run the current retained `linear_propagator` path and confirm
   it still reports retained solution-sample fitting and not
   `full_eta_zero_contour_applied: true`.
2. Unit tests: cover gauge-link state detection, `GenerateSquare` parity,
   affected-position normalization, missing-boundary failure, master-set drift,
   `PickZeroRuleS` finite-part behavior, multiple-region rejection, and the
   `gaugex` versus DESolver `eta` naming bridge.
3. Integration test: execute the new b64ag runtime from raw DE/reduction/boundary
   inputs without consuming the final retained `solution` samples.
4. Fresh AMFlow golden: recapture `linear_propagator` with a precision goal high
   enough for >=50 correct digits. The upstream example's `precision = 20` is
   not a sufficient M6 floor by itself (`run.wl:26`).
5. Compare C++ to AMFlow at `--tolerance-digits 50` across the full accepted
   packet target set and requested epsilon-order surface.
6. Publish packet evidence: canonical result manifest, primary run manifest,
   provenance, comparison summary, correct-digit scoring, and required
   failure-code audit.
7. Run `qualify_phase0_packet_set.py`, then `qualify_milestone_m6.py`. M6 closure
   is allowed only when the phase-0 packet set is qualified and both pending and
   blocked phase-0 runtime-lane lists are empty.

Until those steps land, `linear_propagator` must remain pending on `b64ag`.
