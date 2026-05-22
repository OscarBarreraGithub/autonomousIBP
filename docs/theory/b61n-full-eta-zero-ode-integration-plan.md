# B61n Full Eta-Zero ODE Integration Plan

Status: design-only Tier C gap note. This document does not close `b61n`,
does not set `full_eta_zero_contour_applied=true`, does not edit phase-0
qualification metadata, and does not claim M6 readiness.

## Scope

The target surface is the retained `complex_kinematics` phase-0 row for the
single `box` family. The current runtime can already recognize the exact
seven-master state, parse the complex rational eta matrix, build a NegIm
lower-half-plane contour scaffold, certify eta-infinity initial data for a
controlled finite start, and transport the reviewed selected endpoint subset.
Those pieces are useful, but they still stop short of the production object
needed for M6:

```text
dF(eta, eps)/deta = A(eta, eps) F(eta, eps),
F(eta_start, eps) from retained eta-infinity asymptotics,
F(eta=0, eps) extracted on the reviewed NegIm branch for all seven masters.
```

The accepted result must not read final AMFlow `solution` samples as input. It
must execute live eta-infinity-to-eta=0 ODE propagation, endpoint extraction,
post-endpoint Laurent fitting, and then qualify through the lane148 phase-0
packet contract. A raw runtime flag flip is not a qualification route.

## Current Boundary

The current b61n evidence is scoped and honest:

- lane141 transports `box[0,0,0,1]` from eta-infinity data and passes its
  selected coefficient comparison while keeping `full_eta_zero_contour_applied=false`;
- the current propagator may mark that reviewed single-row `regular-taylor-r0`
  endpoint coefficient as published after live contour propagation and endpoint
  extraction, but that remains scoped single-master evidence and still withholds
  the full-contour flag;
- lane142 extends selected endpoint coverage to the four primitive bubble
  masters and passes a 20-coefficient comparison with minimum digit agreement
  54, still with `transport_scope=eta-zero-selected-endpoint-coefficients`;
- lane124 covers all seven masters only by fitting retained final solution
  samples, so it is retained-reference evidence, not runtime closure evidence;
- the current coupled-row audit identifies the two remaining rows,
  `box[1,0,1,1]` and `box[1,1,1,1]`, records their lower-triangular dependency
  order, and keeps coefficient publication disabled;
- the current trial coupled-row propagation path is intentionally
  non-publishing. It can expose a live propagation gate, but it still blocks on
  certified contour-start handoff or on an accepted relative endpoint error
  budget, and it never promotes the full-contour diagnostic.

The missing production step is therefore not another primitive endpoint table.
It is the reviewed, error-budgeted integration of the coupled seven-master ODE
from certified eta-infinity data to eta=0, followed by endpoint extraction and
packet-level comparison.

## Docs-Only Verification Surface

This commit is documentation-scoped. The runtime remains unchanged, so there is
no coefficient-level fail-before/pass-after claim in this note.

The concrete fail-on-head check is that `HEAD` does not contain
`docs/theory/b61n-full-eta-zero-ode-integration-plan.md`; equivalently,
`git cat-file -e HEAD:docs/theory/b61n-full-eta-zero-ode-integration-plan.md`
exits nonzero before this docs commit.

The concrete pass-on-worktree checks are:

- the repository doc exists and is non-empty;
- the required sections in this note are present;
- the anti-fake constraints below are present;
- `git diff --check -- docs/theory/b61n-full-eta-zero-ode-integration-plan.md`
  is clean.

Future slices below describe runtime tests that should fail before and pass
after each code slice. They are not claimed as passing evidence for this
docs-only Tier C packet.

## Integration Contract

The production b61n path should be a single full-contour path guarded by the
existing complex-kinematics predicate. It must require:

- `benchmark_id == "complex_kinematics"`, `family == "box"`,
  `variable == "eta"`, and `target_location == "eta=0"`;
- `boundary_state.direction == "NegIm"` and the reviewed lower-half-plane
  waypoint policy;
- the retained seven-master order:
  `box[0,0,0,1]`, `box[1,0,1,0]`, `box[1,0,0,1]`,
  `box[0,1,0,1]`, `box[0,0,1,1]`, `box[1,0,1,1]`,
  `box[1,1,1,1]`;
- retained `boundary`, `boundarymi`, `bpattern`, epsilon samples, complex
  Numeric substitutions, and the full 7x7 eta matrix;
- a matrix fingerprint, contour fingerprint, initial-data fingerprint, endpoint
  model fingerprint, and Laurent-fit fingerprint in the produced diagnostics;
- `final_solution_samples_used_as_input=false` for every accepted path.

The selected primitive endpoint implementation should become an internal oracle
for the first five masters, not the source of a full-contour claim. A publishing
full-contour path should propagate the full seven-vector and require the first
five live endpoint samples to agree with the reviewed selected endpoint values
at a stricter internal threshold before rows 5 and 6 are trusted.

## Reviewer-Acceptable Slicing Plan

Slice 1: certified contour-start handoff.

Replace the current "no closer finite-start point certified" blocker with an
explicit handoff object. The handoff may be either:

- a closer eta-infinity finite-start vector certified directly by the
  infinity-variable expansion, with the same 70-digit guard and radius audit; or
- a radial lower-half-plane bridge from the already certified finite start to
  the contour-entry radius, propagated with the same matrix evaluator and a
  published absolute plus relative error budget.

The handoff must record the chosen start eta, x-start radius, nearest
infinity-plane singularity radius, truncation and overcheck orders, path
segments, tail bound, residual bound, roundoff bound, and total certified-digit
estimate. If either handoff mode crosses an eta singularity, leaves the NegIm
branch, or cannot certify the requested digit floor, it must fail closed before
endpoint propagation.

Slice 2: production coupled-row propagation policy.

Turn the current trial propagation into a non-publishing but meaningful
propagation verdict. The placeholder-style tolerance must be replaced by a
dimension-aware absolute and relative budget:

```text
accepted_error <= max(abs_floor, rel_floor * max(1, endpoint_vector_norm))
```

The verdict should be computed from step-doubling or an equivalent independent
refinement check for every epsilon sample. Diagnostics must include step counts,
segment counts, max selected-row and full-vector refinement errors, tolerances,
working precision, failure code, and the exact transport order
`box[1,0,1,1] -> box[1,1,1,1]`.

This slice may prove that the live propagation reaches eta=0, but it must keep
`coefficient_publication=false` and `full_eta_zero_contour_applied=false`.

Slice 3: endpoint model and extraction gate.

Promote endpoint extraction only after the propagated eta=0 vector is validated
against the local eta=0 model. For the current retained matrix the scaffold
classifies the endpoint as `regular-taylor-r0`; the production code should still
recompute that model from the matrix, reject unsupported resonances, reject
untracked logarithms, and fail closed if multiple integer eta-zero terms would
make `PickZeroRuleS` ambiguous.

For a regular endpoint, extraction is the eta^0 endpoint vector reached by the
contour. For any future non-regular endpoint, extraction must use the same
AMFlow key semantics documented in the earlier b61n contour design. This slice
can publish endpoint samples internally only if all epsilon samples pass the
handoff, propagation, and local-model gates.

Slice 4: full seven-master Laurent fitting.

Fit Laurent coefficients from the live endpoint samples, not from final AMFlow
solution samples. The fit must cover the epsilon order required by the candidate
packet, record the sample count and fit residuals per master, and replace the
current selected-scope diagnostics only when all seven masters have fitted
coefficients.

The first five live coefficients must be cross-checked against lane141/lane142
selected endpoint evidence. Rows 5 and 6 must be cross-checked against an
independent AMFlow golden slice before any packet is considered qualifying. If
the first five live coefficients disagree with the selected endpoint oracles,
the full-contour path must fail closed rather than mixing propagated and
hardcoded endpoint values.

Slice 5: stripped packet integration.

Run the stripped `complex_kinematics` state through the full pipeline:

```text
retained eta-infinity boundary regions
  -> certified finite-start handoff
  -> lower-half-plane seven-master ODE propagation
  -> eta=0 local-model extraction
  -> Laurent fitting
  -> target reduction
  -> JSON diagnostics
```

The JSON result may report `transport_applied=true` and
`transport_scope=eta-zero-full-contour-endpoint-coefficients` only when the live
endpoint coefficients are the values being serialized. It must still keep
`full_eta_zero_contour_applied=false` until AMFlow parity and phase-0 packet
sidecars exist.

Slice 6: M6 qualification packet.

After the stripped runtime result passes against a sufficiently precise AMFlow
golden at the M6 digit floor, promote `complex_kinematics` through the lane148
packet route:

- create a coherent optional phase-0 capture packet for the row;
- remove the `next_runtime_lane` blocker only in that packet surface;
- publish comparison, correct-digit scoring, and failure-code audit sidecars;
- require `phase0_packet_set_qualified=true` with no pending phase-0 ids;
- only then allow `full_eta_zero_contour_applied=true` in the qualifying packet
  diagnostics.

## Runtime Tests Future Code Must Satisfy

The first runtime slice should add a fail-on-head stripped fixture proving the
current path stops at the coupled-row gate. After implementation, the same
fixture should pass the certified handoff and still avoid final solution
samples.

The propagation slice should add synthetic lower-triangular systems with known
analytic endpoints, plus retained-shape b61n tests that fail closed on:

- missing matrix, contour, initial-data, or endpoint-model fingerprints;
- non-NegIm waypoints or interior real-axis waypoints;
- uncertified finite-start data;
- refinement error above the combined absolute and relative budget;
- attempted coefficient publication when one epsilon sample fails.

The endpoint and Laurent slices should add tests that:

- reject unsupported local logarithms, resonances, and multiple integer
  eta-zero keys;
- prove the first five live endpoint samples agree with selected endpoint
  oracles before rows 5 and 6 are serialized;
- prove a stripped state never mentions
  `retained-loop-solution-sample-cache-laurent-fit`;
- prove the accepted full-contour diagnostic names all seven transported
  masters, the contour branch, the endpoint local model, and the fit residual
  summary.

The qualification slice should add packet-level tests only after the AMFlow
golden comparison exists. Until then, tests must assert that the runtime can
reach no higher than scoped transport evidence.

## Anti-Fake Constraints

- Do not consume retained AMFlow final `solution` samples as boundary input.
- Do not set `full_eta_zero_contour_applied=true` from selected endpoint
  coefficients, retained-sample fitting, or a trial propagation diagnostic.
- Do not publish rows 5 and 6 from eta-infinity leading boundary samples without
  live propagation to eta=0.
- Do not mix selected primitive endpoint tables with propagated coupled rows and
  call the result full-contour evidence unless the live seven-vector path also
  agrees with the selected primitive oracles.
- Do not use an infinite or placeholder refinement tolerance in a publishing
  path.
- Do not turn a failed certified-start search into a looser digit guard.
- Do not suppress row-specific failures behind a successful target reduction.
- Do not remove the b61n `next_runtime_lane` blocker except through a coherent
  optional phase-0 packet with passing comparison and audit sidecars.

## Acceptance Checklist

A future implementation is reviewer-acceptable only when all of these are true:

- stripped `complex_kinematics` runs without final solution samples;
- eta-infinity finite-start handoff certifies the requested guard or fails
  closed;
- the seven-master ODE is propagated over the reviewed lower-half-plane contour
  for every epsilon sample;
- endpoint extraction is derived from the local eta=0 model and records
  dropped-term behavior;
- all seven endpoint Laurent series are fitted from live endpoint samples;
- the first five live coefficients agree with reviewed selected endpoint
  evidence;
- rows 5 and 6 agree with independent AMFlow golden data at the required floor;
- packet comparison, correct-digit scoring, and failure-code audit sidecars pass;
- only the qualifying packet reports `full_eta_zero_contour_applied=true`.
