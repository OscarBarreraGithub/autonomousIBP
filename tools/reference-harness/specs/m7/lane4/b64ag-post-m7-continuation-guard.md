# Lane 4 B64ag Post-M7 Continuation Guard

Status: docs-only post-M7 continuation record. This landing does not change
runtime code, does not edit evidence JSON, does not recapture
`linear_propagator` goldens, does not promote `b64ag`, and does not claim
`Milestone M6`, `Milestone M7`, or release readiness.

## Scope

This note carries the lane 4 `b64ag` blocker forward after M7 so the next
attempt starts from the already-reviewed boundary instead of reopening the same
selected-evidence question.

Durable inputs:

- `tools/reference-harness/specs/m6/lane4/b64ag-runtime-packet-anti-fake-audit-gap.md`
- `tools/reference-harness/specs/m6/lane4/b64ag-golden-recapture-readiness-gap.md`
- `tools/reference-harness/specs/m6/lane148/m6-runtime-lane-qualifier-requirements.md`
- `tools/reference-harness/specs/m6/lane147/linear_propagator.selected4-lightlike.cpp-result.json`
- `tools/reference-harness/specs/m6/lane147/linear_propagator.selected4-lightlike.compare30.json`
- `tools/reference-harness/specs/m6/lane147/b64ag-selected4-real-coefficients-evidence.json`

## Current Boundary

The existing lane147 artifacts remain scoped selected-coefficient evidence only.
They are useful as regression inputs, but they are not a captured optional
phase-0 packet for `linear_propagator`, not a full gauge-link endpoint transport
packet, and not a release-signoff input.

The selected evidence still has the same non-promotion constraints recorded in
the M6 lane 4 notes:

- `full_eta_zero_contour_applied` must stay false for the selected packet;
- selected-endpoint, scaffold, retained-cache, solution-sample, deferred, or
  blocked wording must not be reinterpreted as full contour evidence;
- final AMFlow solution samples must not be used as runtime input;
- the retained comparison remains a 30-digit selected-scope comparison, not the
  M6 50-digit optional packet floor;
- `linear_propagator -> b64ag` must stay on the runtime-lane frontier until a
  coherent optional phase-0 packet removes it.

## Next Landing Shape

The next code landing should be an explicit fail-closed helper or runtime packet
guard, not another prose-only promotion. A narrow acceptable helper would audit
one candidate `linear_propagator` packet and reject it unless all of these are
true:

- runtime lane is exactly `b64ag`;
- transport is the full `gaugex -> 0` six-master gauge-link path, not selected
  endpoint transport;
- `full_eta_zero_contour_applied=true` is paired with contour, pole,
  finite-part, target-reduction, precision, and provenance diagnostics;
- finite `gaugex=1/40` boundary provenance is explicit and does not read final
  endpoint solution samples;
- candidate result, comparison summary, retained AMFlow state, and packet
  manifest are provenance-bound to the same packet surface;
- detailed coefficient rows recompute the top-level comparison verdict and
  reject sentinel-only digit evidence;
- the packet can satisfy the phase-0 qualification path without weakening
  comparison, digit-scoring, failure-code, or sidecar-root guards.

Until such a helper or packet lands and passes review, the honest post-M7 lane 4
status remains:

`M7_PARITY_SIGNOFF_FLIPPED=false`

`B64AG_OPTIONAL_PACKET_CAPTURED=false`

`NEXT_FIX=implement a fail-closed b64ag recapture/runtime-packet guard before any promotion`
