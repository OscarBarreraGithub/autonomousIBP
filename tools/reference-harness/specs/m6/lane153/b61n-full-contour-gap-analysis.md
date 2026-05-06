# Lane 153 B61n Full-Contour Gap Analysis

Status: Tier C implementation gap. This sidecar does not flip M6, does not
promote `complex_kinematics`, does not alter retained coefficient evidence, and
does not claim `full_eta_zero_contour_applied=true`.

## Verdict

Lane 153 cannot honestly land Tier A or Tier B with the code currently in the
tree. The `complex_kinematics` runtime has real scoped b61n evidence from lanes
141 and 142, but that evidence is selected endpoint transport and explicitly
keeps `full_eta_zero_contour_applied=false`. Promoting it to full-contour
evidence would be a flag flip without the live coupled complex eta ODE
propagator, infinity-to-zero contour handoff, endpoint extraction, and M6 packet
qualification required by lane 148.

The appropriate M6 state after this lane remains:

- `complex_kinematics`: blocked on `b61n`
- `b61n`: blocked

## Existing Evidence Boundary

The current scaffold records useful structural facts:

- The retained b61n surface is recognized only for the seven exact
  `complex_kinematics` masters, including the unresolved
  `box[1,0,1,1]` and `box[1,1,1,1]` rows
  (`src/cli/main.cpp:4614-4634`).
- The contour audit validates the complex Numeric substitutions, checks the
  seven-by-seven eta matrix, extracts complex eta poles, fingerprints a
  lower-half-plane waypoint plan, and records
  `final_solution_samples_used_as_input=false`
  (`src/cli/main.cpp:4655-4792`).
- The first live endpoint transport is a scalar diagonal-only path for
  `box[0,0,0,1]`. It rejects off-diagonal rows and ends by saying full
  seven-master transport remains deferred
  (`src/cli/main.cpp:4841-4956`).
- The primitive bubble extension updates exactly four additional masters:
  `box[1,0,1,0]`, `box[1,0,0,1]`, `box[0,1,0,1]`, and `box[0,0,1,1]`
  (`src/cli/main.cpp:6650-6708`).
- The eta-infinity helper only fills missing leading asymptotic sample
  coefficients. It skips masters that already have a boundary sample and does
  not propagate any solution along the complex contour
  (`src/cli/main.cpp:4135-4210`).
- The AMFlow-state evaluator fits those boundary samples, then applies the
  scalar and primitive selected endpoint patches. No call performs a coupled
  ODE solve for rows 5 and 6
  (`src/cli/main.cpp:8115-8278`).
- JSON emission maps selected endpoint transport to
  `eta-zero-selected-endpoint-coefficients` and leaves
  `full_eta_zero_contour_applied` controlled only by a diagnostics bit that the
  b61n selected path never sets (`src/cli/main.cpp:9076-9129`).
- The deferred reason for b61n explicitly says full seven-master singular
  eta=0 complex contour execution remains deferred after selected transport
  (`src/cli/main.cpp:6557-6562`, `src/cli/main.cpp:9134-9164`).

Lane 142 remains valid scoped evidence. It reports five transported endpoint
integrals, a 20/20 coefficient comparison with minimum digit agreement 54, and
`full_eta_zero_contour_applied=false`
(`tools/reference-harness/specs/m6/lane142/b61n-selected5-real-coefficients-evidence.json:5-40`).
The singular runtime/CLI tests intentionally lock this boundary: stripped b61n
must not fall back to retained solution samples, must publish selected endpoint
scope, must count exactly five transported masters, and must keep the full
contour flag false
(`tests/amflow_tests.cpp:49120-49256`).

Lane 124 is also not closure evidence. It covers all seven masters, but only by
using the retained solution-sample cache. The phase-0 state still marks that
cache enabled
(`tools/reference-harness/specs/phase0/complex_kinematics.amflow-state.json:336-339`),
and the lane124 result reports
`retained-loop-solution-sample-cache-laurent-fit`, `transport_applied=false`,
and `full_eta_zero_contour_applied=false`
(`tools/reference-harness/specs/m6/lane124/complex_kinematics.digits80.cpp-result.json:10-33`).
Its summary says it evaluates retained AMFlow solution samples and that full
complex eta-contour endpoint reconstruction remains deferred
(`tools/reference-harness/specs/m6/lane124/complex_kinematics.digits80.cpp-result.json:359-361`).

## Blocking Physics And Runtime Pieces

The missing runtime object is the full seven-master complex contour transport:

```text
F'(eta, eps) = A(eta, eps) F(eta, eps),
F(eta -> infinity, eps) supplied by retained subsystem asymptotics,
F(eta = 0, eps) extracted on the reviewed lower-half-plane branch.
```

The unresolved rows are not primitive endpoint functions. The retained matrix
shows `box[1,0,1,1]` coupled to masters 0, 1, 2, 4, and itself, and
`box[1,1,1,1]` coupled to masters 0, 1, 2, 4, 5, and itself
(`tools/reference-harness/specs/phase0/complex_kinematics.amflow-state.json:107-124`).
Those rows require variation-of-constants or an equivalent live ODE transport
over the contour, including the eta-infinity boundary powers, branch choices,
and error/precision control. The current selected tables cover only the tadpole
and primitive bubbles, so they cannot justify endpoint coefficients for either
coupled row.

The reviewed SRL-4 solver does set `full_eta_zero_contour_applied=true`, but it
is a separate exact-rational direct-real endpoint-extraction path. It builds
regular/Frobenius patches between rational start, match, and endpoint points and
publishes one coefficient per master only after exact residual and overlap
checks pass (`src/solver/series_solver.cpp:7115-7244`). That is not wired to
the AMFlow-state b61n complex eta-infinity boundary path and does not provide
the complex lower-half-plane integration needed here.

A lane153 stripped replay was run against the eps-through-two golden after
removing `boundary_state.files.solution`. It preserved the existing selected
evidence but failed exactly on the coupled rows: `box[1,0,1,1]` eps^0..eps^2
had only 1-2 digit agreement, and `box[1,1,1,1]` eps^-2..eps^2 had only 3-6
digit agreement at a 30-digit comparison floor
(`/tmp/autoibp_orch/exec/lane153/complex-stripped.compare30.json`). That is the
expected failure mode when the remaining rows are just leading boundary fits
rather than propagated endpoint coefficients.

## Concrete Implementation Work

A future Tier B or Tier A lane needs these code changes before any true
`full_eta_zero_contour_applied=true` diagnostic is allowed:

1. Add a live b61n complex contour propagator.

   The current b61n AMFlow-state path builds only a contour audit and selected
   endpoint patches. A full implementation needs a propagator for the parsed
   complex rational eta matrix along the audited lower-half-plane waypoints,
   with provenance tying the matrix, Numeric substitutions, pole list, contour
   fingerprint, branch policy, and precision budget to the produced samples.

2. Construct valid eta-infinity initial data for every retained master.

   `EvaluateLeadingBoundarySamples` and
   `ApplyEtaInfinityAsymptoticTransportFromDE` provide leading asymptotic sample
   coefficients, not finite-start values on the contour. The future solver must
   evaluate a controlled asymptotic expansion at a finite start point or solve
   in an infinity variable with truncation/error diagnostics before handoff.

3. Transport the coupled rows, not just selected primitives.

   `ApplyB61nFirstScalarContourEndpointTransport` accepts only a diagonal scalar
   equation, and `ApplyB61nComplexKinematicsPrimitiveBubbleEndpointTransportThroughEpsOrder`
   writes reviewed primitive endpoint tables for the first five masters. The
   future evaluator must solve rows 5 and 6 with their inhomogeneous couplings,
   including the row-6 dependence on row 5.

4. Extract and fit endpoint Laurent coefficients from transported samples.

   The accepted packet must replace the current row-5/row-6 boundary-fit
   coefficients with eta=0 endpoint coefficients through the requested epsilon
   order, then fit Laurent coefficients with enough samples and precision for
   the M6 50-digit floor.

5. Keep final AMFlow solution samples out of the runtime input.

   The retained phase-0 state still carries `boundary_state.files.solution`, and
   the unstripped path intentionally identifies
   `retained-loop-solution-sample-cache-laurent-fit` as legacy retained
   evidence. The full b61n path must continue to work from the stripped state
   and publish `final_solution_samples_used_as_input=false`.

6. Publish a coherent optional phase-0 packet only after runtime evidence
   passes.

   Lane 148 requires `complex_kinematics` to become a captured optional phase-0
   row with coherent manifests, no `next_runtime_lane`, passing comparison and
   digit scoring, and complete failure-code audit coverage
   (`tools/reference-harness/specs/m6/lane148/m6-runtime-lane-qualifier-requirements.md:37-62`).
   It also records that extending b61n selected coefficients to all seven
   masters would not itself flip M6
   (`tools/reference-harness/specs/m6/lane148/m6-runtime-lane-qualifier-requirements.md:28-33`).

## Four-Role Review

- Role A, implementer: APPROVE Tier C. The implementable change in this lane is
  a gap sidecar; the current C++ path has contour planning and selected
  endpoint patches, but no live seven-master b61n complex ODE propagator.
- Role B, test: APPROVE Tier C. Existing CLI tests require stripped b61n to
  publish selected endpoint scope, count five transported masters, and keep
  `full_eta_zero_contour_applied=false`
  (`tests/amflow_tests.cpp:49120-49256`).
- Role C, physics: APPROVE Tier C. The missing physics object is the coupled
  lower-half-plane endpoint functional for `box[1,0,1,1]` and
  `box[1,1,1,1]`, not another primitive endpoint table.
- Role D, anti-fake: APPROVE Tier C only with no runtime flag flip, no
  comparator tolerance change, no new sentinel matches, no self-comparison
  assertions, and no final AMFlow solution samples fed back as boundary input.

## Anti-Fake Constraints

This lane intentionally does not:

- change any `full_eta_zero_contour_applied` flag from false to true;
- add hardcoded zero coefficients;
- add or reuse sentinel `999` as new comparator evidence;
- compare any test value against itself;
- loosen comparator tolerances;
- read AMFlow final solution samples as live b61n boundary input;
- edit M6 phase-0 qualification metadata to hide the
  `complex_kinematics -> b61n` blocker.

The scoped lane141/lane142 evidence remains valid as selected endpoint
transport evidence with `transport_applied=true`, and remains invalid as full
M6 closure evidence.
