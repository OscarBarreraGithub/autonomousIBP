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

## Helper Fixture Contract

This docs-only follow-up does not replace the helper above. It freezes the
minimum fixture surface that the future helper should cover before any b64ag
packet can be proposed for promotion.

The positive fixture must be one coherent candidate packet with these
repo-local inputs:

- a `linear_propagator` C++ result whose runtime diagnostics name lane `b64ag`
  and publish full `gaugex -> 0` contour evidence;
- a comparison summary bound to that exact result and to the retained AMFlow
  state used as the golden source;
- a retained AMFlow state that exposes the six-master gauge-link basis and the
  finite `gaugex -> 1/40` boundary provenance;
- a packet manifest whose target list, result integrals, comparison integrals,
  coefficient rows, and provenance hashes all describe the same surface.

The negative fixtures must include the current selected evidence plus small
mutations that prove each anti-fake guard is active:

- selected endpoint scope with `full_eta_zero_contour_applied=false`;
- any final solution sample marked or implied as runtime input;
- missing contour, pole, finite-part, target-reduction, precision, or
  provenance diagnostics;
- mismatched candidate-result, comparison, retained-state, or manifest
  provenance;
- top-level comparison pass with failing, missing, real-only, imag-only, or
  all-`999` sentinel coefficient rows;
- digit evidence below the required optional packet floor or disconnected from
  the detailed coefficient table.

Passing these fixtures would only show that the helper rejects fake promotion
paths. It would not by itself recapture goldens, qualify the phase-0 packet set,
close `linear_propagator -> b64ag`, change M7 release evidence, or prove release
readiness.

## Handoff Checks

Before a future code lane attempts to promote a `linear_propagator` packet, it
should first prove that this post-M7 routing record is still present and that the
release-facing docs still route `linear_propagator -> b64ag` through this guard:

```sh
git cat-file -e HEAD:tools/reference-harness/specs/m7/lane4/b64ag-post-m7-continuation-guard.md
rg -n "b64ag-post-m7-continuation-guard.md" docs/release/known-gaps.md docs/release/amflow-example-coverage.md
python3 tools/reference-harness/scripts/validate_release_markdown.py
git diff --check -- tools/reference-harness/specs/m7/lane4/b64ag-post-m7-continuation-guard.md docs/release/known-gaps.md docs/release/amflow-example-coverage.md
```

The expected result is: the guard exists at `HEAD`, both release-doc routes are
still present, the release markdown validator passes, and whitespace validation
is clean. If any of those checks fails, the next lane should repair the routing
or docs validation first and still treat the selected lane147 evidence as
non-promotable.

Until such a helper or packet lands and passes review, the honest post-M7 lane 4
status remains:

`M7_PARITY_SIGNOFF_FLIPPED=false`

`B64AG_OPTIONAL_PACKET_CAPTURED=false`

`NEXT_FIX=implement a fail-closed b64ag recapture/runtime-packet guard before any promotion`
