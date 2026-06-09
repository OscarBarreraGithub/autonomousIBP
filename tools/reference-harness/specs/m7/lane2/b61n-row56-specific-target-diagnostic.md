# Lane 2 B61n Row 5/6 Specific-Target Diagnostic

Status: Tier C diagnostic, amended post-M7. This landing does not change
runtime numerics, does not promote `complex_kinematics`, and does not set
`full_eta_zero_contour_applied=true`.

## Step Chosen

Chosen atomic step for this amendment: option (b), update the comparator and
evidence so retained-reference floors are accepted as their own verdict:
`matched-to-reference-floor`, distinct from `matched-to-50-digit`.

The earlier c267 stripped run is still the runtime input. The amendment changes
only the comparator verdict surface and the row-specific diagnosis.

Artifacts:

- `complex_kinematics.c267-stripped.eps0.cpp-result.json`
- `complex_kinematics.c267-stripped.eps0.compare50.json`
- `complex_kinematics.b61n-reference-floor-golden-manifest.json`
- `complex_kinematics.c267-stripped.eps0.compare50.reference-floor.json`
- `b61n-row56-specific-target-diagnostic.json`

## Comparator Surface

The raw 50-digit comparator against the unannotated retained AMFlow manifest
still fails four row-specific coefficients:

```text
passed=false
compared_coefficient_count=14
passed_coefficient_count=10
minimum_digit_agreement=11
```

The failing raw targets are exactly the retained-reference floor targets:

```text
box[1,0,1,1] eps^0: real/imag agreement 11/11
box[1,1,1,1] eps^-2: real/imag agreement 46/46
box[1,1,1,1] eps^-1: real/imag agreement 12/13
box[1,1,1,1] eps^0: real/imag agreement 12/12
```

The amended comparator run uses
`complex_kinematics.b61n-reference-floor-golden-manifest.json`, which declares
only those four retained-reference component floors. That run reports:

```text
passed=true
comparison_verdict=matched-to-reference-floor
compared_coefficient_count=14
accepted_coefficient_count=14
passed_coefficient_count=10
reference_floor_matched_coefficient_count=4
minimum_digit_agreement=11
```

This is not a 50-digit match for the four row 5/6 coefficients. It is an honest
acceptance that those coefficients have matched the precision floor of the
retained AMFlow reference itself.

## Runtime Trace Classification

The current c267 runtime path still distinguishes the two mechanisms:

```text
sample_space_coupled_row_transport_applied=true
source_anchored_evolution_rows=[5, 6]
target_coefficients_reconstructed_from_epsilon_samples=true
coefficient_state_transport_applied=false
coefficient_state_endpoint_matcher_applied=false
target_coefficients_published_from_coefficient_state=false
```

The coefficient-state publication gate is reached, but it still blocks before
any direct row 5/6 coefficient upsert:

```text
failure_code=coefficient-state-publication-unclosed-target-graph
requested_public_target_count=4
published_coefficient_count=0
coefficient_state_finite_start_constructed=false
coefficient_target_graph_node_count=14
coefficient_target_graph_edge_count=46
coefficient_target_graph_blocked_edge_count=24
blocked_out_of_range_edge_count=24
```

Therefore the emitted row 5/6 coefficients in this artifact are still
sample-reconstructed, not published from direct coefficient-state transport.

## Diagnosis

Current evidence points to:

```text
retained AMFlow reference-floor convergence for the four row 5/6 specific
targets, while current emitted row 5/6 coefficients still come from sample
reconstruction because direct coefficient-state publication is not active
```

Distinguishing the requested cases:

- `matcher converges to wrong free constants`: not established by this
  reference-floor comparator. The direct coefficient-state route still never
  publishes target coefficients.
- `matcher converges correctly but transport itself loses precision`: not the
  primary current classification. There is still no direct coefficient-state
  endpoint value for the row 5/6 public targets.
- `AMFlow reference is itself only 11 digits`: supported for row 5 `eps^0`, and
  more generally reference-floor limited for the four specific targets:
  11/11, 46/46, 12/13, and 12/12 component digits respectively.

## Next Narrow Target

The next runtime implementation slice remains direct coefficient-state
publication: close the row56 coefficient target graph, materialize finite-start
coefficient data from explicit coefficient sources, run coefficient-state
transport, and publish only direct endpoint coefficients. Until that path is
active, row 5/6 runtime coefficients remain sample-reconstructed even though
the comparator now classifies the retained AMFlow floor honestly.

`M7_PARITY_SIGNOFF_FLIPPED=false`
