# Lane 1 Next3 b61n Source-Anchored Matcher Still Blocked

Tier C.  This step added reviewed source-row endpoint anchors to the coupled
Frobenius matcher and wired the b61n production path to provide rows 0-4 as
fixed source endpoint data.  The publication guard remains closed because the
coupled rows are still matched from finite-start propagated row 5/6 values.

## Guarded Reproduction

The guarded run used:

```text
./build/amflow-cli solve-series \
  /tmp/autoibp_orch/exec/lane1_next2/complex_kinematics.stripped-state.json \
  --eps-order 0 --digits 80 \
  --out /tmp/autoibp_orch/exec/lane1_next3/complex_kinematics.stripped.eps0.cpp-result.json

python3 tools/reference-harness/scripts/compare_cpp_vs_amflow.py \
  --cpp-result /tmp/autoibp_orch/exec/lane1_next3/complex_kinematics.stripped.eps0.cpp-result.json \
  --amflow-golden tools/reference-harness/specs/phase0/complex_kinematics.golden-manifest.json \
  --tolerance-digits 50
```

Result:

```text
passed=false
compared_coefficient_count=14
passed_coefficient_count=10
minimum_digit_agreement=2
```

The runtime reached the new anchored matcher:

```text
coupled_frobenius_endpoint_matcher_applied=true
coupled_frobenius_source_anchored=true
coupled_frobenius_source_anchor_rows=[0, 1, 2, 3, 4]
coupled_frobenius_source_anchor_count=5
coupled_frobenius_source_anchor_residual_abs=0
coupled_frobenius_source_anchor_source=reviewed-endpoint-series
```

Publication still correctly blocked by the existing refinement guard.  The
production path also keeps endpoint-only source anchoring diagnostic-only, so a
future refinement-budget pass cannot publish row 5/6 without row-specific
source-anchored evolution and AMFlow comparator evidence.

```text
selected coupled-row refinement error 160.3761514976891537827532856897460208616
relative error 160.3761514976891537827532856897460208616
coefficient_publication=false
full_eta_zero_contour_applied=false
```

## Relaxed Guard Diagnostic

Before the final explicit diagnostic-only safety gate was added, the coupled-row
refinement guard was relaxed locally after source anchoring.  That run wrote
`/tmp/autoibp_orch/exec/lane1_next3/complex_kinematics.stripped.eps0.anchor-relaxed.cpp-result.json`.
The comparator improved but remained far below the 50-digit gate:

```text
passed=false
compared_coefficient_count=14
passed_coefficient_count=10
minimum_digit_agreement=11
```

Failing coupled coefficients had agreements:

```text
box[1,0,1,1] eps^0    11/11 digits
box[1,1,1,1] eps^-2   46/46 digits
box[1,1,1,1] eps^-1   12/13 digits
box[1,1,1,1] eps^0    12/12 digits
```

This reproduces the earlier warning that force-publishing is not a valid M6
fix.  Source endpoint anchoring removes the row 0-4 endpoint ambiguity, but the
row 5/6 match values are still inherited from the finite-start full-vector
propagation.

## Next Valid Fix Target

The next implementation should source-anchor the coupled-row evolution itself:

```text
1. Build a reviewed source trajectory for rows 0-4 from their endpoint series.
2. Propagate row 5 with row 0-4 source values supplied by that trajectory.
3. Propagate row 6 with row 0-4 plus the newly certified row 5 trajectory.
4. Keep the publication guard closed until row 5/6 Laurent coefficients clear
   the AMFlow comparator.
```
