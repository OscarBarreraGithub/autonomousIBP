# B61n Lane 141 First Complex-Kinematics Endpoint Coefficient

Lane 141 produces the first live b61n eta=0 endpoint coefficient for the
retained `complex_kinematics` state without reading final AMFlow `solution`
samples.

Implemented:

- The stripped `complex_kinematics` state still runs with
  `boundary_state.files.solution` removed, so retained endpoint samples are not
  available to the runtime.
- The existing lane129/lane137 guard still validates the benchmark, family,
  eta variable, `NegIm` direction, seven-master order, complex numeric
  substitutions, complex eta matrix, complex pole extraction, lower-half-plane
  contour plan, and regular Taylor endpoint audit.
- The simplest master, `box[0,0,0,1]`, is recognized as a decoupled scalar
  eta equation from the parsed matrix row:
  `dF/deta = (1 - eps)/(eta + 2 - I) F`.
- The runtime applies the corresponding lower-branch endpoint factor to the
  eta-infinity boundary samples and fits the resulting epsilon coefficients.
- The solve result reports `transport_applied=true` only with
  `transport_scope=eta-zero-selected-endpoint-coefficients`,
  `eta_zero_endpoint_transport_applied=true`,
  `eta_zero_endpoint_transported_integrals=["box[0,0,0,1]"]`, and
  `full_eta_zero_contour_applied=false`.

Evidence:

- `tools/reference-harness/specs/m6/lane141/complex_kinematics.box0001.stripped-result.json`
  is the stripped-state runtime output.
- `tools/reference-harness/specs/m6/lane141/complex_kinematics.box0001.cpp-result.json`
  is the single-master comparator slice.
- `tools/reference-harness/specs/m6/lane141/complex_kinematics.box0001.amflow-golden.txt`
  is the matching AMFlow positive-order golden slice derived from the reviewed
  `complex_kinematics.eps2-golden-manifest.json` surface.
- `tools/reference-harness/specs/m6/lane141/complex_kinematics.box0001.compare30.json`
  passes `4/4` compared coefficients through eps^2 with minimum 54-digit
  agreement at a 30-digit gate.
- `tools/reference-harness/specs/m6/lane141/b61n-first-real-coefficient-evidence.json`
  summarizes the anti-fake gate metadata.

Still deferred:

- Full seven-master complex eta-contour propagation.
- Coupled endpoint extraction for the remaining masters.
- `full_eta_zero_contour_applied=true` for b61n closure.

This lane is therefore a real selected-coefficient transport milestone, not full
b61n closure.
