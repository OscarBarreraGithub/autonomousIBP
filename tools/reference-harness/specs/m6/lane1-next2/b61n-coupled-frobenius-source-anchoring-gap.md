# Lane 1 Next2 b61n Coupled Frobenius Source-Anchoring Gap

Tier C.  The b61n selected endpoint path is still numerically preserved, but
the full stripped eps0 packet remains blocked.  This diagnostic does not change
any packet-level `full_eta_zero_contour_applied` flag.

## Fresh Reproduction

All commands were run from repository baseline
`9c3444c3fff91826ddc8cfa9692d3d18df5b558c` with a stripped copy of
`tools/reference-harness/specs/phase0/complex_kinematics.amflow-state.json`
under `/tmp/autoibp_orch/exec/lane1_next2/`.

The selected b61n endpoint replay was regenerated at eps order 2 and filtered
to the five reviewed endpoint rows:

```text
./build/amflow-cli solve-series \
  /tmp/autoibp_orch/exec/lane1_next2/complex_kinematics.stripped-state.json \
  --eps-order 2 --digits 80 \
  --out /tmp/autoibp_orch/exec/lane1_next2/complex_kinematics.stripped.eps2.cpp-result.json

python3 tools/reference-harness/scripts/compare_cpp_vs_amflow.py \
  --cpp-result /tmp/autoibp_orch/exec/lane1_next2/complex_kinematics.selected5.eps2.cpp-result.json \
  --amflow-golden tools/reference-harness/specs/m6/lane142/complex_kinematics.selected5.amflow-golden.txt \
  --tolerance-digits 30
```

Result:

```text
passed=true
compared_coefficient_count=20
minimum_digit_agreement=54
```

The full stripped eps0 packet was replayed with the same stripped state:

```text
./build/amflow-cli solve-series \
  /tmp/autoibp_orch/exec/lane1_next2/complex_kinematics.stripped-state.json \
  --eps-order 0 --digits 80 \
  --out /tmp/autoibp_orch/exec/lane1_next2/complex_kinematics.stripped.eps0.cpp-result.json

python3 tools/reference-harness/scripts/compare_cpp_vs_amflow.py \
  --cpp-result /tmp/autoibp_orch/exec/lane1_next2/complex_kinematics.stripped.eps0.cpp-result.json \
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

Failing coefficients:

```text
box[1,0,1,1] eps^0    C++ 0 + 0 I; agreement 2/2
box[1,1,1,1] eps^-2   C++ coefficient absent; agreement 5/6
box[1,1,1,1] eps^-1   C++ coefficient absent; agreement 4/4
box[1,1,1,1] eps^0    C++ 0 + 0 I; agreement 3/4
```

## Runtime Evidence

The coupled-row runtime path is reached on the full stripped packet.  The
summary records:

```text
coupled_frobenius_endpoint_matcher_applied=true
coupled_frobenius_recurrence_count=7
coupled_frobenius_order=32
coupled_frobenius_sample_count=192
coupled_frobenius_match_eta=0 - 0.0625*I
coupled_frobenius_tail_sample_radius=0.125
coupled_frobenius_basis_residual_abs=3.638937829647242401644103119618586577852e-94
```

Publication is still blocked by the coupled-row refinement guard:

```text
selected coupled-row refinement error 160.3761514976891537827532856897460208616
relative error 160.3761514976891537827532856897460208616
endpoint_refinement_integrator=fehlberg-rk78-adaptive
finite_start_selection=closer-certified-eta-infinity-start
closer_start_root_causes={slot-incompatibility:0, divisibility:0, recurrence-depth:0, precision:2}
coefficient_publication=false
full_eta_zero_contour_applied=false
```

The published result therefore remains the five selected endpoint rows only:

```text
eta_zero_endpoint_transported_master_count=5
transport_order=[box[1,0,1,1] -> box[1,1,1,1]]
coupled_row_dependencies={
  box[1,0,1,1]<-[box[0,0,0,1], box[1,0,1,0], box[1,0,0,1], box[0,0,1,1]];
  box[1,1,1,1]<-[box[0,0,0,1], box[1,0,1,0], box[1,0,0,1],
                  box[0,0,1,1], box[1,0,1,1]]
}
```

## Structural Diagnosis

The evidence does not support a scalar-reducible classifier failure.  The
classifier added in `eb7b972` is not called by production `solve-series`, and
the real b61n coupled rows are visibly non-scalar: row 5 depends on rows
0, 1, 2, and 4, while row 6 depends on rows 0, 1, 2, 4, and 5.

The evidence also does not show a simple Frobenius sign or root-indexing error
for the active eps0 regular endpoint.  The retained b61n eta matrix is regular
at eta=0 for this path, the indicial residue is zero, and the coupled matcher
builds seven rho=0 ordinary Frobenius branches with a tiny basis residual.

The structural problem is that the coupled matcher is full-vector matched from
the numerically propagated finite-start ODE state.  Rows 0-4 already have
reviewed analytic endpoint values, but the coupled matcher does not anchor
those source rows to the reviewed endpoint series before solving rows 5 and 6.
The refinement guard then measures a large full-vector step-doubling error at
the last nonzero waypoint and correctly blocks publication.  The previous
lane1_next1 force-publish diagnostic only reached 11 digits, so bypassing the
guard is not a valid fix.

## Next Valid Fix Target

The next implementation should solve the coupled subsystem in source-anchored
triangular form:

```text
1. Preserve the reviewed rows 0-4 endpoint series as fixed source data.
2. Integrate or recur row 5 with those source rows held to the reviewed series.
3. Integrate or recur row 6 using rows 0-4 plus the newly certified row 5.
4. Compare the resulting row 5 and row 6 Laurent coefficients before allowing
   any packet-level full-contour promotion.
```

Until that exists, the honest status remains blocked on the coupled-row
source-anchoring gap, and `full_eta_zero_contour_applied` must stay false.
