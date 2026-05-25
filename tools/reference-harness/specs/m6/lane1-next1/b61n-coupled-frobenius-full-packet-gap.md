# Lane 1 b61n Coupled Frobenius Full-Packet Gap

Tier C.  The b61n full stripped eps0 packet now reaches the reviewed
coupled-row Frobenius endpoint matcher from live eta-infinity finite-start
data, but the AMFlow comparator still blocks M6 promotion.  This note records
the attempted forward roll and does not change any packet-level
`full_eta_zero_contour_applied` flag.

## Implementation Slice

The runtime propagator now has a multi-row endpoint matcher for reviewed b61n
`regular-taylor-r0` lower-half-plane contours.  It integrates to the last
nonzero waypoint, fits the analytic tail of `eta * A(eta)` on a small circle,
constructs the lower-triangular Frobenius basis using the canonical indicial
null-vector recurrence, solves for the basis amplitudes at the match point, and
evaluates the endpoint vector at eta=0.

The solve-series b61n coupled-row path requires this matcher before it will
consider publishing the remaining coupled rows.  The packet-level full flag
remains fail-closed unless the publisher has promoted a complete seven-master
packet.

## Reproduction

The branch was finally rebased onto clean `origin/main` at:

```text
33477ead368887cbc6ddce4c5baaffadf49d6a16
```

The stripped state removed only `boundary_state.files.solution` from
`tools/reference-harness/specs/phase0/complex_kinematics.amflow-state.json` and
kept `solution_sample_cache.enabled=true`.  The final-code run used:

```text
amflow-cli solve-series complex_kinematics.stripped-state.json --eps-order 0 --digits 80
compare_cpp_vs_amflow.py --tolerance-digits 50
```

Artifacts were written under:

```text
/tmp/autoibp_orch/exec/lane1_next1/
```

## Observed Result

The committed code keeps the full packet blocked:

```text
transport_scope=eta-zero-selected-endpoint-coefficients
eta_zero_endpoint_transported_master_count=5
full_eta_zero_contour_applied=false
blocked_reason=full seven-master singular eta=0 complex contour execution remains deferred after reviewed selected b61n endpoint coefficient transport for 5 master(s)
```

The coupled matcher did execute inside the blocked coupled-row audit, but the
selected coupled-row refinement guard did not allow publication.  The AMFlow
comparator result for the full eps0 packet remains:

```text
passed=false
minimum_digit_agreement=2
```

Failing coefficients:

```text
box[1,0,1,1] eps^0    real/imag agreement 2/2
box[1,1,1,1] eps^-2   real/imag agreement 5/6
box[1,1,1,1] eps^-1   real/imag agreement 4/4
box[1,1,1,1] eps^0    real/imag agreement 3/4
```

## Diagnostic Force-Publish Check

A local, uncommitted force-publish experiment bypassed the coupled-row
refinement guard only to measure the matcher floor.  That diagnostic improved
the minimum agreement to 11 digits but still failed the 50-digit comparator:

```text
box[1,0,1,1] eps^0    real/imag agreement 11/11
box[1,1,1,1] eps^-2   real/imag agreement 46/46
box[1,1,1,1] eps^-1   real/imag agreement 12/13
box[1,1,1,1] eps^0    real/imag agreement 12/12
```

Because the honest packet result is still 2 digits and the diagnostic
force-publish floor is only 11 digits, M6 remains open for b61n.
