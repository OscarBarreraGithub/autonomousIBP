# Lane 1 Next25 B64ag Full-Contour Readiness Semantics

Status: Tier-C investigation note. This sidecar does not flip M6, does not
promote `linear_propagator`, does not edit phase-0 packet manifests, and does
not claim `full_eta_zero_contour_applied=true`.

## Step Chosen

Lane 1 took the complementary M6 support slice for the b64ag contour gate:

- audited `tools/reference-harness/scripts/audit_b64ag_golden_recapture_readiness.py`;
- inspected the retained phase-0 state shape at
  `tools/reference-harness/specs/phase0/linear_propagator.amflow-state.json`;
- inspected the fresh lane1-next24 stripped b64ag runtime/comparison evidence;
- checked whether b61n and b63n expose the same gate or only analogous
  full-contour blockers.

## Helper Semantics

`b64ag_runtime_scope_not_full_contour` is a runtime-scope postmortem bucket, not
a digit-comparison bucket. The helper maps these checks into that code:

```text
continuation_present
continuation_variable_gaugex
continuation_start_one_fortieth
continuation_target_zero
continuation_singular_zero
full_eta_zero_contour_applied
blocked_reason_absent
runtime_text_rejects_fake_scope_words
```

The exact full-contour candidate expected by the helper is:

- top-level `runtime_lane == "b64ag"`;
- `benchmark_id == "linear_propagator"`;
- `runtime_provenance.final_solution_samples_used_as_input == false`;
- `continuation.variable == "gaugex"`;
- `continuation.start_location == "gaugex -> 1/40"`;
- `continuation.target_location == "gaugex=0"`;
- `continuation.singular_points == ["gaugex=0"]`;
- `continuation.full_eta_zero_contour_applied == true`;
- `continuation.blocked_reason` absent or empty;
- runtime text fields do not contain selected/scaffold/cache/solution-sample,
  deferred, or blocked wording;
- top-level `targets` and `results[*].integral` match the retained nine-target
  packet, in the helper's canonical order.

The helper then requires a separate full-contour diagnostics object. It accepts
that object from top-level `full_contour_diagnostics`,
`diagnostics.full_contour`, or `continuation.full_contour_diagnostics`. Required
buckets are:

- `contour`: fingerprint plus at least two waypoints;
- `poles`: at least one nonzero pole, not just `0` or `gaugex=0`;
- `finite_part_extraction`: `PickZeroRuleS`, `ir_subtraction_applied=true`,
  finite part order zero, and nonempty dropped singular powers;
- `target_reduction`: fingerprint plus `target_row_count == 9`;
- `precision`: at least 50 working digits and at least 31 epsilon samples;
- `provenance`: final solution samples false plus candidate, AMFlow state, and
  AMFlow golden fingerprints.

Only after those structural checks pass does the 50-digit comparator evidence
become sufficient for this helper. A 50-digit comparator pass with
`full_eta_zero_contour_applied=false` is still blocked by design.

## Current B64ag Evidence Shape

The lane1-next24 runtime result
`linear_propagator.b64ag-full-packet-finite-part.cpp-result.json` passes the
external comparator but still reports a non-promotable scope:

```text
status=success
benchmark_id=linear_propagator
continuation.variable=gaugex
continuation.start_location="gaugex -> 1/40"
continuation.target_location="gaugex=0"
continuation.transport_scope="eta-zero-b64ag-full-packet-finite-part-coefficients"
continuation.runtime_application="b64ag-gauge-link-full-packet-finite-part-coefficients"
continuation.eta_zero_endpoint_transported_master_count=6
continuation.eta_zero_endpoint_frobenius_recurrence_applied=true
continuation.full_eta_zero_contour_applied=false
continuation.blocked_reason is nonempty
runtime_provenance is absent
full_contour_diagnostics is absent
```

The paired comparator
`linear_propagator.b64ag-full-packet-finite-part.compare50.json` records:

```text
passed=true
matched_integral_count=9
compared_coefficient_count=39
passed_coefficient_count=39
minimum_digit_agreement=51
tolerance_digits=50
```

That is useful 50-digit evidence, but it is not the helper's full-contour
candidate. When the readiness helper is run against those lane1-next24 artifacts
and the retained phase-0 AMFlow state, it remains blocked with primary failure
code `b64ag_runtime_scope_not_full_contour`. The same run also reports missing
runtime provenance, missing full-contour diagnostics, and packet-contract
mismatches against the helper's stricter nine-target, 57-row recapture contract.

## Retained State Fields

There is no tracked `state_3.json` in this repository snapshot or under the
current `/tmp/autoibp_orch` and `/tmp/autoibp_worktrees` scratch roots. The
committed retained state with equivalent b64ag structure is
`tools/reference-harness/specs/phase0/linear_propagator.amflow-state.json`.

The state fields read by the readiness helper are intentionally narrow:

- `gauge_link.diffeq_masters`: must be the exact six-master ordered DE basis;
- `gauge_link.boundary_point`: must be `gaugex -> 1/40`;
- `gauge_link.diffeq_variables`: must be `["gaugex"]`.

Other retained-state fields explain the runtime context but do not set contour
scope by themselves:

- `boundary_state.kind == "amflow_finite_solution_samples"`;
- `boundary_state.files.boundary` carries the finite `gaugex -> 1/40`
  boundary samples;
- `boundary_state.files.solve.wl` shows AMFlow's DESolver replay and
  `PickZeroRuleS` finite-part selection in the retained pipeline;
- `boundary_state.files.solution` is still present in the unstripped committed
  state;
- `solution_sample_cache.enabled == true`;
- top-level `masters` and `reduction.targets` list the retained nine target
  integrals.

The stripped runtime tests remove or avoid the final solution file before
calling `solve-series`, but the contour gate is controlled by the emitted C++
candidate result. A stripped input state can prove the runtime did not consume
final solution samples; it cannot by itself satisfy the helper unless the
candidate result also publishes `full_eta_zero_contour_applied=true`, no blocked
reason, full runtime provenance, full-contour diagnostics, and the expected
packet/result shape.

## B61n And B63n Comparison

The exact failure code `b64ag_runtime_scope_not_full_contour` is b64ag-specific.
It is defined only in `audit_b64ag_golden_recapture_readiness.py` and is not a
generic phase-0 qualifier code.

The same high-level anti-overclaiming gate exists for b61n and b63n:

- b61n's publication qualifier requires
  `m6_qualifier_hook.requires_full_eta_zero_contour_applied=true`; it only
  accepts promotion evidence when the referenced runtime continuation reports
  `full_eta_zero_contour_applied=true` and the 50-digit AMFlow comparator passes
  for the reviewed endpoints. Current committed b61n evidence remains selected
  or partially propagated and publishes `full_eta_zero_contour_applied=false`.
- b63n has selected `automatic_phasespace` Cutkosky evidence, but the committed
  lane143/lane146 outputs explicitly say the remaining residues and
  `feynman_prescription` are deferred and keep
  `full_eta_zero_contour_applied=false`. The b63n design document requires a
  full Cutkosky endpoint branch ledger/extraction before setting the flag.

So this is not b64ag-specific as a physics policy: no lane may promote scoped,
selected, or retained-cache evidence as full eta-zero contour evidence. It is
b64ag-specific as an audit-helper failure-code spelling and as a concrete
nine-target recapture contract.

## Precise Next Attack Surface

To clear the b64ag helper honestly, a future implementation should produce a
new candidate result, not edit the retained comparator alone. The candidate
must:

1. Set top-level `runtime_lane="b64ag"` and publish
   `runtime_provenance.final_solution_samples_used_as_input=false`.
2. Emit `continuation.full_eta_zero_contour_applied=true` only after a live
   six-master gauge-link contour/finite-part pipeline has run.
3. Remove `continuation.blocked_reason` and all selected/scaffold/cache/deferred
   language from the accepted candidate result.
4. Publish full-contour diagnostics with contour, pole, finite-part,
   target-reduction, precision, and provenance buckets.
5. Emit the helper's retained nine-target packet with all expected epsilon
   orders. The helper currently expects 57 detailed coefficient rows even though
   lane1-next24's 50-digit comparator covers 39 rows through eps^2.
6. Bind the comparator to the candidate result, the retained AMFlow state, and
   the phase-0 `linear_propagator.golden-manifest.json`.

## Four-Role Review

- Implementer: APPROVE Tier C. The sidecar is scoped to the readiness-helper
  semantics and does not modify runtime behavior, manifests, or M6 metadata.
- Test/gate: APPROVE Tier C. The helper was run against the lane1-next24 b64ag
  artifacts and returned the expected blocked primary failure code; this note
  records why the 50-digit comparison alone is insufficient.
- Physics/source: APPROVE Tier C. The document distinguishes retained finite
  `gaugex -> 1/40` boundary state, selected/full-packet finite-part evidence,
  and the still-missing full contour promotion signal.
- Anti-fake: APPROVE Tier C. The note explicitly forbids flipping
  `full_eta_zero_contour_applied` or M6 from comparator evidence alone and
  preserves the analogous b61n/b63n anti-overclaiming policy.

## Honest Status

`HONEST_STATUS=TIER_C_GAP_DOC`, `M6_FLIPPED=false`,
`B64AG_50_DIGIT_COMPARATOR_PASSED=true`, and
`B64AG_FULL_CONTOUR_READY=false`.
