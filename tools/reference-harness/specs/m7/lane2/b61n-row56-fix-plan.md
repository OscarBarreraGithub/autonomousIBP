# Lane 2 B61n Row 5/6 Coefficient-Transport Fix Plan

Status: Tier C theory-planner sidecar. This document does not change runtime
code, does not flip M7 parity signoff, does not promote `complex_kinematics`,
and does not set `full_eta_zero_contour_applied=true`.

## Step Chosen

Chosen atomic step: option (b), produce a written implementation plan for the
coefficient-level endpoint transport fix described by the row 5/6 boundary
matching audit.

The implementation path is not unambiguous enough for a one-lane runtime patch.
Three contracts have to be made explicit first:

- the initial coefficient state must come from the eta-infinity recurrence or
  reviewed source endpoint series, not from a Vandermonde fit of the same
  clustered epsilon samples;
- the real eta matrix must be evaluated as epsilon Laurent coefficient
  matrices before propagation;
- the eta=0 endpoint matcher must solve coefficient-state boundary constants,
  not two free constants in per-epsilon sample space.

## Problem Contract

The current live path in `ApplyB61nCoupledRowContourTransport` does this:

1. build a seven-master vector for each retained epsilon sample;
2. replace rows 0 through 4 with reviewed source-anchor start values;
3. propagate the scalar sample vector through `BuildB61nCoupledRowMatrixEvaluator`;
4. write row 5 and row 6 endpoint samples back into `master_samples`;
5. let the common solve-series loop call
   `FitBoundarySamplesAsLaurentCoefficients` on every row.

The parity comparator checks Laurent coefficients. Therefore the fix is not a
new scalar endpoint tolerance, a different epsilon sample count, or a scaled
Vandermonde solve. The fix must publish the gated coefficients as endpoint
values of an augmented Laurent-coefficient ODE state.

The first target coefficients are:

- `box[1,0,1,1] eps^0`;
- `box[1,1,1,1] eps^-2`;
- `box[1,1,1,1] eps^-1`;
- `box[1,1,1,1] eps^0`.

The reviewed source-anchor rows are:

- `box[0,0,0,1]`;
- `box[1,0,1,0]`;
- `box[1,0,0,1]`;
- `box[0,1,0,1]`;
- `box[0,0,1,1]`.

## Non-Negotiable Guards

- Do not derive any target coefficient from
  `spec.boundary_epsilon_samples` after endpoint propagation.
- Do not move the same clustered-sample fit to the finite-start point and call
  it coefficient transport.
- Do not read retained AMFlow final solution samples as an input.
- Do not loosen `compare_cpp_vs_amflow.py` tolerance or compare against a
  generated self-result.
- Do not set `full_eta_zero_contour_applied=true` until the unchanged stripped
  `complex_kinematics` comparator passes the 50-digit gate with the direct
  coefficient path.
- Do not touch b63n files or release-signoff-readiness scope.

## Reviewable Implementation Slices

### 1. Coefficient Target Graph

Add a b61n-only target graph builder that starts from the four public target
nodes above, where each node is `(master_index, eps_order)`. It should inspect
the epsilon Laurent support of the real eta matrix and close the dependency
graph backwards:

```text
target(row, eps_order) depends on source(column, eps_order - matrix_eps_order)
```

This graph should include the reviewed source-anchor coefficient nodes needed
to drive row 5 and row 6. It should not hard-code one global epsilon window as
the proof. A global padded state may be used for the first transport if that is
the least risky way to reuse `BuildComplexContourCoefficientStateMatrix`, but
the diagnostic must still print the closed target-node set and matrix support
that justified it.

Initial tests:

- synthetic lower-triangular matrix where row 6 depends on row 5 through an
  `eps^1` matrix coefficient;
- real b61n support inventory that proves the graph contains the four public
  target nodes and the five reviewed source-anchor labels;
- fail-closed test for any missing master label, malformed matrix row, or empty
  matrix epsilon support.

### 2. Real Matrix Laurent Evaluator

Add a real b61n Laurent matrix evaluator parallel to
`BuildB61nCoupledRowMatrixEvaluator`. It should evaluate each eta-matrix cell
as an epsilon Laurent series at a supplied eta and numeric substitution packet,
then return the ordered matrix coefficient list consumed by
`MakeComplexContourCoefficientStateMatrixEvaluator`.

The implementation should reuse or extract the existing Laurent parsing
machinery instead of adding ad hoc string manipulation. The output contract must
record:

- minimum and maximum matrix epsilon order;
- matrix coefficient count;
- nonzero `(matrix_eps_order, row, column)` support;
- a fingerprint over the raw matrix, substitutions, support, and target graph.

Initial tests:

- synthetic `eps^0 + eps^1` matrix fixture already covered by
  `B61nCoefficientStateTransportPropagatesLaurentOrdersBeforeEndpointFitTest`;
- real b61n matrix support fingerprint remains stable;
- nonfinite or unparsable Laurent coefficients fail closed before propagation.

### 3. Coefficient-Level Finite-Start Data

Extend the eta-infinity finite-start construction so it can return Laurent
coefficient vectors at the chosen finite eta start. The current
`EtaInfinityInitialDataAudit` stores `finite_start_samples`, which are sample
values. That is not enough for this fix.

The new helper should solve or expose the eta-infinity recurrence in coefficient
space for the closed target graph. Source-anchor rows should use their reviewed
endpoint series and reverse contour transport in coefficient space, not a
per-epsilon reverse sample trajectory.

Fail closed if a required coefficient is not available from either:

- the controlled eta-infinity recurrence at the finite start; or
- the reviewed source-anchor endpoint series transported through the same
  coefficient-state matrix.

Initial tests:

- synthetic eta-infinity fixture that returns a known finite-start coefficient
  vector without invoking `SolveVandermondeFit`;
- real b61n dry-run diagnostic that prints required and available coefficient
  nodes but publishes nothing;
- explicit regression test that the coefficient-start path is not populated by
  fitting `finite_start_samples`.

### 4. Augmented Coefficient-State Transport

Wire the closed coefficient state through the existing runtime primitives:

- `FlattenComplexContourCoefficientState`;
- `BuildComplexContourCoefficientStateMatrix`;
- `MakeComplexContourCoefficientStateMatrixEvaluator`;
- `PropagateComplexContourVector`;
- `UnflattenComplexContourCoefficientState`.

The first safe version may use a padded seven-master by contiguous-order state
if that avoids a new sparse augmented-matrix implementation. If so, the
publication layer must still only read coefficients present in the target graph.
A later cleanup can compact the state once the parity gate is green.

The transport diagnostics should distinguish:

- `sample_space_coupled_row_transport_applied`;
- `coefficient_state_transport_applied`;
- `coefficient_state_endpoint_matcher_applied`;
- `target_coefficients_published_from_coefficient_state`;
- `target_coefficients_reconstructed_from_epsilon_samples`.

The last field must be `false` before any target coefficient can be published.

### 5. Coefficient-Level Endpoint Matching

Do not reuse the current two-free-row interpretation blindly. In the augmented
state, the free boundary constants are coefficient nodes, not just original
master rows. The target set contains at least four public coefficient nodes, and
the dependency closure may add more.

Implement one of these two endpoint strategies, with tests deciding which is
sufficient:

- a generalized anchored coefficient-state Frobenius matcher that solves all
  free coefficient constants after anchoring reviewed source coefficient nodes;
- an ordinary eta=0 endpoint boundary solve when the coefficient-state local
  model is regular for the closed target graph.

Either strategy must accept anchor indices in augmented-state coordinates. The
summary must report the number of anchored coefficient nodes, free coefficient
nodes, recurrence order, residual, and endpoint fingerprint.

Initial tests:

- synthetic four-target lower-triangular system where sample-space matching
  passes but coefficient-level publication fails unless the augmented endpoint
  matcher is used;
- regression test that the old two-free-row helper is not selected when the
  augmented free-node count differs from two;
- real b61n nonpublishing dry run that reaches the matcher and reports the
  target graph, even if later numeric accuracy still blocks publication.

### 6. Publication Surface

Add a b61n-only publication path after the normal sample fit has built
`diagnostics.target_epsilon_coefficients`. This path should upsert only the
target coefficients obtained directly from the coefficient-state endpoint
values:

- row 5 `eps^0`;
- row 6 `eps^-2`, `eps^-1`, and `eps^0`.

All other coefficients should remain on the existing path until a broader
full-contour implementation owns them. Publication should be guarded by:

- successful coefficient-state finite-start construction;
- successful coefficient-state propagation;
- successful coefficient-level endpoint matching or endpoint solve;
- finite target coefficients;
- unchanged AMFlow final solution samples not used as input;
- unchanged 50-digit comparator command passing on the stripped state.

Before the comparator is green, the runtime may expose a blocked diagnostic, but
it must not silently mix sample-fitted target coefficients with direct
coefficient-state claims.

### 7. Acceptance Tests And Gate

Unit and integration coverage should land before or with publication:

- coefficient target graph unit tests;
- real matrix Laurent evaluator support/fingerprint tests;
- finite-start coefficient-source tests that forbid sample fitting;
- augmented endpoint matcher tests with more than two free coefficient nodes;
- a current-contract mismatch test marked XFAIL or SKIP until the fix lands,
  with the reason spelling out that row 5 `eps^0` and row 6 subleading terms are
  still reconstructed from clustered endpoint samples;
- stripped `complex_kinematics` comparator at `--tolerance-digits 50`.

The final parity-closing run should use the same public commands as earlier
lanes:

```text
jq '.boundary_state.files |= (if has("solution") then del(.solution) else . end) |
    .solution_sample_cache.enabled = true' \
  tools/reference-harness/specs/phase0/complex_kinematics.amflow-state.json \
  > /tmp/autoibp_orch/exec/<lane>/complex_kinematics.stripped-state.json

./build/amflow-cli solve-series \
  /tmp/autoibp_orch/exec/<lane>/complex_kinematics.stripped-state.json \
  --eps-order 0 --digits 80 \
  --out /tmp/autoibp_orch/exec/<lane>/complex_kinematics.coeff-state.eps0.cpp-result.json

python3 tools/reference-harness/scripts/compare_cpp_vs_amflow.py \
  --cpp-result /tmp/autoibp_orch/exec/<lane>/complex_kinematics.coeff-state.eps0.cpp-result.json \
  --amflow-golden tools/reference-harness/specs/phase0/complex_kinematics.golden-manifest.json \
  --tolerance-digits 50 \
  > /tmp/autoibp_orch/exec/<lane>/complex_kinematics.coeff-state.eps0.compare50.json
```

Acceptance requires:

```text
passed=true
compared_coefficient_count=14
passed_coefficient_count=14
minimum_digit_agreement>=50
final_solution_samples_used_as_input=false
target_coefficients_reconstructed_from_epsilon_samples=false
```

## Suggested Landing Order

1. Land the target graph and matrix Laurent evaluator with synthetic and real
   support tests, no propagation.
2. Land coefficient-level finite-start data diagnostics, no publication.
3. Land coefficient-state propagation through the real contour with publication
   disabled and target endpoint values fingerprinted.
4. Land the generalized coefficient endpoint matcher or ordinary endpoint solve
   with synthetic tests and real b61n blocked diagnostics.
5. Land target-coefficient publication and the stripped 50-digit comparator
   only when the gate is honestly green.

## Honest Status

`M7_PARITY_SIGNOFF_FLIPPED=false`

`B61N_FIX_PLAN_ONLY=true`

`NEXT_IMPLEMENTATION_SLICE=target graph plus real epsilon-Laurent matrix evaluator, with no coefficient publication`
