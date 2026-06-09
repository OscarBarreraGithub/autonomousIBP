# Lane 2 B61n Row 5/6 Specific-Target Diagnostic

Status: Tier C diagnostic. This landing does not change runtime numerics, does
not loosen the AMFlow comparator, does not promote `complex_kinematics`, and
does not set `full_eta_zero_contour_applied=true`.

## Step Chosen

Chosen atomic step: option (d), with a fresh c267 stripped run and a
machine-readable row-specific analyzer.

Artifacts:

- `complex_kinematics.c267-stripped.eps0.cpp-result.json`
- `complex_kinematics.c267-stripped.eps0.compare50.json`
- `b61n-row56-specific-target-diagnostic.json`

Command surface:

```text
./build/amflow-cli solve-series \
  <stripped complex_kinematics AMFlow state> \
  --eps-order 0 --digits 80 \
  --out tools/reference-harness/specs/m7/lane2/complex_kinematics.c267-stripped.eps0.cpp-result.json

python3 tools/reference-harness/scripts/compare_cpp_vs_amflow.py \
  --cpp-result tools/reference-harness/specs/m7/lane2/complex_kinematics.c267-stripped.eps0.cpp-result.json \
  --amflow-golden tools/reference-harness/specs/phase0/complex_kinematics.golden-manifest.json \
  --tolerance-digits 50 \
  > tools/reference-harness/specs/m7/lane2/complex_kinematics.c267-stripped.eps0.compare50.json
```

## Comparator Surface

The fresh stripped comparator remains blocked:

```text
passed=false
compared_coefficient_count=14
passed_coefficient_count=10
minimum_digit_agreement=11
```

The same four row-specific targets fail:

```text
box[1,0,1,1] eps^0: real/imag agreement 11/11, relative error 2.207652221222609e-9
box[1,1,1,1] eps^-2: real/imag agreement 46/46, relative error 5.108346412241022e-42
box[1,1,1,1] eps^-1: real/imag agreement 12/13, relative error 2.319846390858817e-9
box[1,1,1,1] eps^0: real/imag agreement 12/12, relative error 4.081975844856257e-9
```

Row 6 `eps^-2` is not an 11-digit coefficient in this fresh run: it reaches 46
digits but still fails the 50-digit publication floor. The minimum remains 11
because row 5 `eps^0` is the floor.

## Runtime Trace Classification

The current c267 runtime path distinguishes the two mechanisms:

```text
sample_space_coupled_row_transport_applied=true
source_anchored_evolution_rows=[5, 6]
target_coefficients_reconstructed_from_epsilon_samples=true
coefficient_state_transport_applied=false
coefficient_state_endpoint_matcher_applied=false
target_coefficients_published_from_coefficient_state=false
```

The c267 coefficient-state publication gate is reached, but it blocks before any
direct row 5/6 coefficient upsert:

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

This means the failing public coefficients in the result table are still from
the sample-space/post-endpoint Laurent extraction path, not from the new direct
coefficient-state publication route.

## Diagnosis

Current evidence points to:

```text
unclosed coefficient target graph plus sample-reconstructed row 5/6 target
coefficients before direct coefficient-state publication
```

Distinguishing the requested cases:

- `matcher converges to wrong free constants`: plausible for the emitted
  sample-reconstructed row 5/6 coefficients, but not isolated by c267 because
  the direct coefficient-state route never publishes target coefficients.
- `matcher converges correctly but transport itself loses precision`: not the
  primary current classification. The coefficient-state transport and endpoint
  matcher flags are false on the publication path, so there is no direct
  coefficient-state endpoint value whose precision can be judged.
- `AMFlow reference is itself only 11 digits`: not supported. AMFlow entries are
  present for all four failing targets, ten neighboring coefficients pass the
  50-digit gate, and row 6 `eps^-2` agrees to 46 digits in the same comparator.

## Next Narrow Target

The next implementation slice should close the row56 coefficient target graph
before attempting another precision or RK move. The present graph closes 14
nodes from the four public targets and reviewed source anchors, but the declared
coefficient support leaves 24 dependency edges out of range. Until that graph is
closed and finite-start coefficient data is materialized from explicit
coefficient sources, the publication gate will correctly block and the emitted
row 5/6 coefficients will remain sample-reconstructed.

`M7_PARITY_SIGNOFF_FLIPPED=false`
