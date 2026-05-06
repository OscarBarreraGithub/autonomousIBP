# B61n Lane 137 Complex Contour Planning Scaffold

Lane 137 does not close `b61n`. It extends the lane 129 executable scaffold by
adding the next fail-closed contour-planning layer for the retained
`complex_kinematics` eta system while keeping
`full_eta_zero_contour_applied=false`.

Implemented:

- The stripped `complex_kinematics` state still runs without
  `boundary_state.files.solution`, so the final AMFlow solution samples are not
  used as input.
- The b61n guard still requires the reviewed benchmark, family, eta variable,
  `eta=0` target, `NegIm` direction, seven-master order, boundary ingredients,
  complex numeric substitutions, and 7x7 eta matrix.
- Each eta-matrix entry is parsed as a complex rational function in eta with
  `eps`, rationals, integer powers, and `I` bound from the AMFlow state.
- Denominator roots are extracted from the parsed rational matrix. For the
  retained sample this produces six unique complex eta poles:
  `-486.442201567... + I`, `-41 + I`, `-2.057798432... + I`,
  `-2 + I`, `3/2 + I`, and `6 + I`.
- A deterministic lower-half-plane waypoint plan is built for the `NegIm`
  contour and fingerprinted using the repository artifact fingerprint helper.
- The eta=0 endpoint is classified as a regular Taylor endpoint
  (`regular-taylor-r0`) for this retained matrix slice, with a
  PickZero-equivalent dropped-term audit explaining that the eta^0 term can only
  be selected after live contour propagation.

Still deferred:

- Live high-precision ODE propagation from the eta-infinity boundary to eta=0.
- Evaluation of the asymptotic infinity boundary into a full finite contour
  initial vector with enough expansion depth for parity.
- Laurent fitting from live contour endpoint samples.
- AMFlow line-by-line coefficient parity for all seven retained masters and the
  reconstructed target without retained final solution samples.

Therefore this lane is a material partial scaffold only. It must not be used to
set `transport_applied=true` or `full_eta_zero_contour_applied=true`.
