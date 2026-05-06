# Lane 159 B64ag Finite-Gaugex Transport Gap

Status: Tier B substantive partial.

This lane adds a runtime primitive that consumes finite `gaugex=1/40`
boundary values for the reviewed six-master b64ag gauge-link basis and
publishes live endpoint terms for the first reviewed DE block only:

- `gauge[1,1,1,0,1,0,0,0,0]`
- `gauge[1,1,1,-1,1,0,0,0,0]`

The primitive is
`TransportLightlikeGaugeLinkFiniteBoundaryEndpointTerms`, declared in
`include/amflow/runtime/lightlike_propagator.hpp` and implemented in
`src/runtime/lightlike_propagator.cpp`. It reuses the existing retained-state
gate, the parsed six-by-six `gaugex` matrix audit, and the selected first-block
Frobenius connection. It does not read retained final AMFlow solution samples,
does not set `full_eta_zero_contour_applied=true`, and returns
`partial_success=true` with `success=false`.

## Remaining Blocker

The remaining four masters are still unresolved:

- `gauge[0,1,1,1,1,0,0,0,0]`
- `gauge[0,1,1,1,1,-1,0,0,0]`
- `gauge[1,1,1,1,1,0,0,0,0]`
- `gauge[1,1,1,1,1,-1,0,0,0]`

The code-level blocker is the same second-DE-block and coupled downstream
endpoint extraction identified by lane 151. The selected first-block recurrence
has a reviewed local connection, but there is still no reviewed transformation,
local model, recurrence, or finite-part term extraction for the second block or
the downstream rows that couple to both upstream blocks. Publishing endpoint
terms for those four rows would require new physics beyond this lane.

The primitive also intentionally accepts only one synthetic epsilon sample for
published endpoint terms. Multi-sample epsilon Laurent fitting belongs after the
full six-master endpoint producer exists, so this lane does not wire the CLI or
alter M6 qualification outputs.

## Anti-Fake Boundary

This lane does not:

- set `full_eta_zero_contour_applied=true`;
- read retained final AMFlow solution samples as runtime boundary input;
- manufacture endpoint coefficients for the unresolved four masters;
- add sentinel `999` evidence;
- loosen comparator tolerances;
- edit lane148-lane157 sidecars or any M6 qualifier output.
