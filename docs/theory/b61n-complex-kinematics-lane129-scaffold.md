# B61n Lane 129 Partial Transport Scaffold

Lane 129 does not close b61n. It adds an executable preflight scaffold for the
`complex_kinematics` eta=0 contour lane and keeps
`full_eta_zero_contour_applied=false`.

Implemented:

- The retained `complex_kinematics` state still uses the old retained
  `boundary_state.files.solution` sample path and still reports that full
  complex eta-contour reconstruction is deferred.
- A stripped b61n state with the final `solution` file removed can now run the
  non-solution AMFlow boundary path even when the retained cache marker remains
  present.
- The scaffold validates the b61n guard surface: benchmark `complex_kinematics`,
  family `box`, variable `eta`, target `eta=0`, `NegIm`, the seven retained
  masters in AMFlow order, retained boundary ingredients, complex numeric
  substitutions, and a 7x7 complex eta matrix.
- The complex expression parser used by the scaffold handles AMFlow-style
  rationals, `I`, integer powers such as `eta^2`, endpoint `eta`, and epsilon
  bindings, then evaluates the matrix at the endpoint for preflight.
- The emitted summary records
  `final_solution_samples_used_as_input=false` for the stripped path.

Still deferred:

- Complex pole extraction from the rational eta matrix and contour waypoint
  planning equivalent to AMFlow `RunEta`.
- Live high-precision complex ODE propagation from eta infinity to the eta=0
  endpoint.
- Eta=0 local-model construction, branch ledger, resonance/log fail-closed
  checks, and `PickZeroRuleS`-equivalent endpoint extraction.
- Laurent fitting from live contour endpoint samples.
- The b61n acceptance evidence: the full-contour flag enabled and >=30-digit
  agreement against the AMFlow golden without retained final solution samples.

This scaffold is intentionally not a parity shortcut. It must not be used to set
the full-contour flag until the live contour propagation and endpoint extraction
steps above are implemented and independently tested.
