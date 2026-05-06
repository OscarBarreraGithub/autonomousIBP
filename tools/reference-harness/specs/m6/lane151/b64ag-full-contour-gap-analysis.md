# Lane 151 B64ag Full-Contour Gap Analysis

Status: Tier C implementation gap. This sidecar does not flip M6, does not
promote `linear_propagator`, does not alter retained coefficient evidence, and
does not claim `full_eta_zero_contour_applied=true`.

## Verdict

Lane 151 cannot honestly land Tier A or Tier B with the code currently in the
tree. The `linear_propagator` runtime has real scoped b64ag evidence from lanes
145 and 147, but that evidence is selected-endpoint transport and explicitly
keeps `full_eta_zero_contour_applied=false`. Promoting it to full-contour
evidence would be a flag flip without the six-master gauge-link endpoint
transport, target reduction, finite-part extraction, Laurent fitting, and M6
packet qualification required by lane 148.

The appropriate M6 state after this lane remains:

- `linear_propagator`: blocked on `b64ag`
- `b64ag`: blocked

## Existing Evidence Boundary

The current scaffold records useful structural facts:

- The reviewed b64ag source surface is recognized only for family `gauge`, loop
  momenta `{l1,l2,l3}`, external leg `{n}`, AMFlow replacement `n^2 -> -1`, and
  the exact nine-propagator surface
  (`src/runtime/lightlike_propagator.cpp:234-275`).
- `BuildLightlikeGaugeLinkSquareFamily` rewrites the two affected linear
  denominators into the generated-square family and records affected positions
  D4,D5 (`src/runtime/lightlike_propagator.cpp:1095-1116`).
- `ApplyLightlikeGaugeLinkPowerNormalization` audits the target-row
  `gaugex^(-sum affected powers)` factor for each retained target
  (`src/runtime/lightlike_propagator.cpp:1118-1138`).
- The retained-state scaffold parses the six-by-six `gaugex` DE matrix,
  extracts endpoint and nonzero pole candidates, builds a deterministic lower
  half-plane contour plan, and records a contour fingerprint
  (`src/runtime/lightlike_propagator.cpp:1459-1497`).
- The finite-part helper implements the fail-closed PickZeroRuleS-compatible
  local selection rule for supplied endpoint terms, including no implicit zero
  publication when the power-zero term is absent
  (`src/runtime/lightlike_propagator.cpp:1141-1209`).

The same code deliberately refuses to publish full-contour evidence:

- The retained-state audit sets `live_coefficients_available=false`,
  `full_eta_zero_contour_applied=false`, and `ir_subtraction_applied=false`
  (`src/runtime/lightlike_propagator.cpp:1413-1420`).
- Its coefficient gap says live gauge-link endpoint coefficients are not
  implemented and that final solution samples remain legacy evidence only
  (`src/runtime/lightlike_propagator.cpp:1441-1444`).
- The direct b64ag scaffold returns `success=false`, `failure_code =
  "boundary_unsolved"`, and says finite boundary replay, endpoint transport,
  PickZeroRuleS application, and Laurent fitting are still deferred
  (`src/cli/main.cpp:7728-7742`).
- The selected path is gated to a prefix of at most four selected endpoint
  masters, not the retained nine-target packet or all six DE masters
  (`src/cli/main.cpp:4575-4599`).
- If that selected-prefix gate is not met, `EvaluateLightlikeGaugeLinkFirstEndpointCoefficient`
  falls back to the non-publishing scaffold (`src/cli/main.cpp:8004-8009`).
- The selected path computes the first two endpoint coefficients from retained
  finite `gaugex=1/40` boundary samples, then uses reviewed selected endpoint
  tables for the final two selected masters; it never claims the missing second
  DE block or accepted packet target surface (`src/cli/main.cpp:8023-8100`).
- The JSON result contract labels selected b64ag transport as
  `b64ag-gauge-link-selected-endpoint-coefficients`, and the blocked reason
  says full b64ag gauge-link endpoint transport remains deferred
  (`src/cli/main.cpp:9120-9163`).

Lane 147 remains valid scoped evidence. It reports `transport_applied=true` for
four selected masters, `full_eta_zero_contour_applied=false`, 18 compared
coefficients, and minimum digit agreement 37
(`tools/reference-harness/specs/m6/lane147/b64ag-selected4-real-coefficients-evidence.json:6-30`,
`tools/reference-harness/specs/m6/lane147/b64ag-selected4-real-coefficients-evidence.json:86-97`).
This lane keeps that evidence intact.

## Blocking Physics And Runtime Pieces

The missing physics object is the full b64ag gauge-link endpoint functional:

```text
I_t(eps) =
  FP_{gaugex=0} gaugex^(-nu(t))
    sum_j R_tj(gaugex, eps) F_j(gaugex, eps)
```

where `F_j` is the six-master DE solution transported from the finite
`gaugex=1/40` boundary, `R_tj` is the target-reduction row for every accepted
retained target, and `nu(t)` is the affected D4,D5 power sum. The current code
audits each ingredient separately, but does not combine them into endpoint
terms for all accepted targets.

The first selected block avoids the full problem by matching a reviewed local
Frobenius basis for the first two DE masters only. That is valid scoped
evidence, but not a full-contour provider for the six-master basis or the
retained nine-target packet.

## Concrete Implementation Work

A future Tier B or Tier A lane needs these code changes before any true
`full_eta_zero_contour_applied=true` diagnostic is allowed:

1. Add a full b64ag endpoint transport evaluator.

   The current selected evaluator is `EvaluateLightlikeGaugeLinkFirstEndpointCoefficient`
   and is intentionally constrained by `IsB64agSelectedEndpointState`
   (`src/cli/main.cpp:4575-4599`, `src/cli/main.cpp:8004-8010`). A full evaluator
   must accept the reviewed six-master DE basis and produce endpoint terms for
   every accepted target, not only the selected prefix.

2. Solve the second DE block and coupled downstream masters from finite boundary
   samples.

   The selected implementation only evaluates the first block from boundary
   samples (`src/cli/main.cpp:7866-7896`, `src/cli/main.cpp:8032-8041`). The
   current code has no analogous live local model, recurrence, contour handoff,
   or extraction for the `gauge[0,1,1,1,1,0,0,0,0]` /
   `gauge[0,1,1,1,1,-1,0,0,0]` block, and no full six-master propagation through
   the coupled rows that feed the packet targets.

3. Apply target reduction before finite-part extraction.

   The CLI can apply a retained Kira target reduction after successful
   diagnostics (`src/cli/main.cpp:9300-9308`), and the JSON reports
   `applied-after-eta-zero-selected-endpoint-transport` for selected endpoint
   cases (`src/cli/main.cpp:9172-9194`). Full b64ag evidence needs target rows
   multiplied by the affected-power normalization and applied to the live
   six-master endpoint series before PickZeroRuleS selection, not only after a
   selected master-value diagnostic exists.

4. Feed real endpoint terms into PickZeroRuleS.

   `ExtractLightlikeGaugeLinkEndpointFinitePart` is currently a selector over
   terms supplied by a future live transport (`src/runtime/lightlike_propagator.cpp:1141-1209`).
   The retained-state contour audit explicitly says those endpoint terms are not
   yet produced (`src/runtime/lightlike_propagator.cpp:1373-1376`). Full evidence
   must publish the dropped singular powers and power-zero coefficients produced
   by transport, not symbolic labels or implicit zeros.

5. Fit epsilon Laurent coefficients after endpoint extraction.

   The selected path fits the first-block samples after endpoint matching
   (`src/cli/main.cpp:8059-8063`). A full path must do the same for every
   accepted target after reduction and finite-part extraction, with enough
   working precision for the M6 50-digit floor.

6. Publish a new optional phase-0 packet only after runtime evidence passes.

   Lane 148 requires `linear_propagator` to become a captured optional phase-0
   row with coherent manifests, no `next_runtime_lane`, passing comparison and
   digit scoring, and complete failure-code audit coverage
   (`tools/reference-harness/specs/m6/lane148/m6-runtime-lane-qualifier-requirements.md:93-115`).
   The M6 composer flips only when the phase-0 packet set is qualified and both
   `phase0_pending_ids` and `blocked_phase0_examples` are empty
   (`tools/reference-harness/scripts/qualify_milestone_m6.py:645-650`).

## Four-Role Review

- Role A, implementer: APPROVE Tier C. The implementable code in this lane is a
  gap sidecar; the live C++ path still returns the b64ag scaffold or selected
  prefix transport, not a six-master full-contour endpoint evaluator.
- Role B, test: APPROVE Tier C. Existing singular runtime tests intentionally
  lock the scaffold false flag and selected-prefix false flag
  (`tests/singular_runtime_lane_tests.cpp:1232-1241`,
  `tests/singular_runtime_lane_tests.cpp:1415-1423`).
- Role C, physics: APPROVE Tier C. The missing object is the full reduced
  endpoint finite-part functional across the accepted target surface, not
  another selected coefficient.
- Role D, anti-fake: APPROVE Tier C only with no runtime flag flip, no comparator
  tolerance change, no new sentinel matches, no self-comparison assertions, and
  no final AMFlow solution samples fed back as boundary input.

## Anti-Fake Constraints

This lane intentionally does not:

- change any `full_eta_zero_contour_applied` flag from false to true;
- add hardcoded zero coefficients;
- add or reuse sentinel `999` as new comparator evidence;
- compare any test value against itself;
- loosen comparator tolerances;
- read AMFlow final solution samples as live b64ag boundary input;
- edit M6 phase-0 qualification metadata to hide the `linear_propagator -> b64ag`
  blocker.

The scoped lane145/lane147 evidence remains valid as selected endpoint transport
evidence with `transport_applied=true`, and remains invalid as full M6 closure
evidence.
