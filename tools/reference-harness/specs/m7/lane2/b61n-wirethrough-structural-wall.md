# Lane 2 B61n Wire-Through Structural Wall

Status: docs-only post-M7 quality record. This landing does not change runtime
numerics, does not promote `complex_kinematics`, does not set
`full_eta_zero_contour_applied=true`, and does not claim a direct
coefficient-state publication pass.

## Finding

The b61n row 5/6 path cannot be forward-rolled from sample-space publication to
direct coefficient-state publication with the data currently available in the
runtime.

The live path still publishes row 5/6 through sample-space transport:

1. `ApplyB61nCoupledRowContourTransport` builds and propagates one full
   retained-master vector for each clustered epsilon sample.
2. Rows 0 through 4 are replaced by reviewed source-anchor values.
3. Rows 5 and 6 are transported as ordinary sample values through the b61n
   contour and endpoint matcher.
4. The common solve-series loop later calls
   `FitBoundarySamplesAsLaurentCoefficients` and reconstructs Laurent
   coefficients from those transported samples.

The coefficient-state publication hook is intentionally diagnostic-only in the
current tree. `BuildB61nCoefficientStatePublicationAttempt` builds the row 5/6
target graph, evaluates the Laurent matrix support, audits finite-start
coefficient availability, and then asks `PublishB61nCoefficientStateTargets` to
fail closed. It does not materialize a `B61nFiniteStartCoefficientData` payload
for the real target graph and does not call `PropagateB61nCoefficientState` on
the runtime b61n path.

The hard blocker is missing input data: the runtime has no explicit
coefficient-level finite-start values for the row 5/6 coefficient-state graph.
The only finite-start values available today are clustered epsilon samples
stored by the eta-infinity initial-data path. Reconstructing coefficient values
from those samples would be another Vandermonde/sample fit, not direct
coefficient-state transport, and would violate the b61n plan's non-negotiable
guard against sample reconstruction.

The publisher is therefore correct to reject publication unless all of these
conditions are simultaneously true:

- the row 5/6 coefficient target graph is closed;
- finite-start coefficient data is materialized from explicit coefficient
  sources;
- endpoint coefficient-state transport has actually run;
- target coefficients were not reconstructed from epsilon samples;
- the unchanged stripped b61n AMFlow comparator is green at the required gate.

## Missing Input

The missing input is a finite-start coefficient vector for the closed b61n row
5/6 coefficient-state graph. For each required `(master, eps_order)` node, the
runtime needs a coefficient value at the chosen finite eta start, together with
provenance proving it came from an explicit coefficient source.

The current finite-start audit can prove that clustered samples are present, and
it can prove they were not used to populate coefficient data. It cannot produce
the coefficient vector needed to initialize the augmented coefficient-state ODE.

## Candidate Sources

Candidate upstream sources for the missing coefficient-level finite-start data:

- AMFlow Mathematica eta-infinity recurrence output. The best source would be a
  Mathematica-side export of the actual finite-start Laurent coefficient vector
  for the closed target graph, with the eta start, epsilon window, master order,
  numeric substitutions, and recurrence truncation recorded.
- A separate reviewed Frobenius or asymptotic series source. This could be a
  source that solves the eta-infinity expansion directly in coefficient space,
  or a local endpoint/series construction that provides the same
  `(master, eps_order)` coefficient nodes without reading retained final
  solution samples.
- Compute-from-scratch coefficient recurrence in the C++ runtime. This would
  derive the finite-start Laurent coefficient vector from the differential
  system and boundary regions, then feed `B61nFiniteStartCoefficientData`
  directly. It must not call `SolveVandermondeFit` on finite-start samples.
- Reviewed source-anchor reverse coefficient-state transport. For source rows
  that already have reviewed endpoint coefficient series, a reverse
  coefficient-state contour can be used as part of the input materialization,
  but only if its output is coefficient data and the provenance is recorded per
  required node.

Final AMFlow solution samples are not a candidate source. Clustered
finite-start epsilon samples are also not a candidate source unless a future
plan explicitly changes the non-reconstruction guard, which would be a separate
policy decision and not the current b61n wire-through plan.

## Next Attempt Shape

The next wire-through attempt should start as a data-materialization lane, not
as another publication patch.

Required preconditions:

- choose one upstream coefficient source and record its provenance contract;
- produce explicit finite-start coefficient values for every node in the closed
  row 5/6 coefficient target graph;
- keep `finite_start_samples_used=false`,
  `populated_from_finite_start_samples=false`, and
  `solve_vandermonde_fit_used=false`;
- close the target graph with no blocked dependency edges;
- run `PropagateB61nCoefficientState` through the actual b61n contour;
- apply coefficient-level endpoint matching or an equivalent endpoint solve in
  augmented-state coordinates;
- publish only endpoint values read from the coefficient-state result;
- keep the sample-space row 5/6 publication path diagnostically separate.

Validation strategy:

- add a synthetic finite-start coefficient fixture that proves the
  materialization path can initialize an augmented state without sample fitting;
- add a real b61n dry run that prints required, available, and missing
  coefficient nodes before any publication attempt;
- assert that any attempted publication fails if finite-start samples,
  retained final solution samples, or `SolveVandermondeFit` fed the coefficient
  start;
- assert that publication fails if `PropagateB61nCoefficientState` did not run
  or if endpoint coefficient vectors were not unflattened;
- only after those gates pass, compare the unchanged stripped
  `complex_kinematics` result against the AMFlow golden at the 50-digit gate.

Primary risks:

- an exported upstream coefficient vector may use a different eta start, master
  order, branch convention, or epsilon window than the runtime graph expects;
- source-anchor reverse transport may need additional coefficient nodes beyond
  the first row 5/6 public targets;
- a closed graph may require a wider epsilon window than the current diagnostic
  target set suggests;
- endpoint matching in coefficient-state space may expose more free coefficient
  constants than the existing two-free-row sample matcher handles;
- using a sample fit, even accidentally, can produce plausible-looking numbers
  while invalidating the direct-publication claim.

## Honest Status

`M7_PARITY_SIGNOFF_FLIPPED=false`

`B61N_WIRETHROUGH_BLOCKER=missing coefficient-level finite-start values for the closed row 5/6 coefficient-state graph`

`NEXT_FIX=materialize explicit finite-start coefficient data before attempting direct coefficient-state publication`
