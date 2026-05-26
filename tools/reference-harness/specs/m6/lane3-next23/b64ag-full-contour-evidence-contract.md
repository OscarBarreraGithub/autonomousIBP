# Lane 3 Next 23 b64ag Full-Contour Evidence Contract

Status: Tier-D requirements/provenance slice only. This sidecar does not flip
M6, does not promote `linear_propagator`, does not add an optional capture
packet, and does not set `full_eta_zero_contour_applied=true`.

## Step Chosen

Option (a): identify the specific full-contour diagnostics/provenance fields
required in b64ag runtime evidence per the lane148 qualifier and the
fail-closed b64ag golden-recapture readiness helper.

This iteration also makes
`tools/reference-harness/scripts/audit_b64ag_golden_recapture_readiness.py`
publish the same contract under `required_runtime_evidence`, so future runtime
packets can inspect the exact field paths instead of reverse-engineering them
from failed checks.

## Required Runtime Evidence Fields

The top-level candidate result must publish:

- `runtime_provenance.final_solution_samples_used_as_input = false`
- `continuation.variable = "gaugex"`
- `continuation.start_location = "gaugex -> 1/40"`
- `continuation.target_location = "gaugex=0"`
- `continuation.singular_points = ["gaugex=0"]`
- `continuation.full_eta_zero_contour_applied = true`
- `continuation.blocked_reason = ""`
- `targets` equal to the retained nine-target b64ag packet
- `results[].integral` equal to the retained nine-target b64ag packet
- nonempty `results[].epsilon_orders` for each target

The candidate result must also avoid selected/scaffold/cache/deferred/blocker
wording in audited runtime text fields. The helper rejects those words because
they indicate a scoped or placeholder runtime rather than the qualifying
full-contour packet.

## Required Full-Contour Diagnostics

The helper accepts the diagnostics object at one of:

- `full_contour_diagnostics`
- `diagnostics.full_contour`
- `continuation.full_contour_diagnostics`

The object must contain these buckets and fields:

- `contour.fingerprint`
- `contour.waypoints`
- `poles.nonzero_poles`
- `finite_part_extraction.rule` containing `PickZeroRuleS`
- `finite_part_extraction.ir_subtraction_applied = true`
- `finite_part_extraction.finite_part_order = 0`
- `finite_part_extraction.dropped_singular_powers`
- `target_reduction.fingerprint`
- `target_reduction.target_row_count = 9`
- `precision.working_digits >= 50`
- `precision.epsilon_sample_count >= 31`
- `provenance.final_solution_samples_used_as_input = false`
- `provenance.candidate_result_fingerprint`
- `provenance.amflow_state_fingerprint`
- `provenance.amflow_golden_fingerprint`

The retained AMFlow state binding must still prove the exact six-master
gauge-link DE basis, `gauge_link.boundary_point = "gaugex -> 1/40"`, and
`gauge_link.diffeq_variables = ["gaugex"]`.

## Required Comparison Evidence

The comparator must bind the candidate result, retained AMFlow state, and
retained phase-0 `linear_propagator` golden manifest. It must report:

- `comparison = "cpp-vs-amflow"`
- `benchmark_id = "linear_propagator"`
- `passed = true`
- `failures = []`
- `tolerance_digits >= 50`
- nine retained target integrals
- the expected retained per-target epsilon orders
- at least 57 detailed coefficient rows
- `cpp_present = true` and `amflow_present = true` for every coefficient
- real and imaginary digit evidence for every coefficient
- non-sentinel detailed digit evidence meeting the 50-digit floor

## Current Runtime Gap

The current stripped b64ag runtime is real but not qualifying Tier-D evidence.
It transports endpoint terms for the reviewed six-master gauge-link basis and
can pass the current 39-row eps^2 external comparator surface, but it still:

- sets `continuation.full_eta_zero_contour_applied = false`;
- emits no top-level `runtime_provenance` object in the C++ result JSON;
- emits no `full_contour_diagnostics` object with the required buckets;
- keeps `blocked_reason` populated;
- uses reviewed downstream Laurent row publication for part of the retained
  packet instead of deriving every published row solely from live post-endpoint
  samples;
- remains short of the helper's 57-row retained order contract.

Therefore the safe next Tier-D implementation work is still to replace the
remaining reviewed-row publication path, emit the full diagnostics/provenance
contract from live runtime state, and then rerun the readiness helper plus the
50-digit comparator before any flag promotion.

## Four-Role Review

Role A, implementation: APPROVE this contract slice. It adds a machine-readable
field contract to the readiness helper and does not change runtime behavior.

Role B, harness: APPROVE. The published contract mirrors the helper's existing
fail-closed checks and keeps `m6_closure_claimed=false`.

Role C, numeric: APPROVE only the requirements finding. This does not extend
the 39-row comparator surface or claim the 57-row 50-digit packet.

Role D, anti-fake: APPROVE Tier-D gap status and REJECT flip. The current
runtime still lacks qualifying diagnostics/provenance and keeps
`full_eta_zero_contour_applied=false`.
