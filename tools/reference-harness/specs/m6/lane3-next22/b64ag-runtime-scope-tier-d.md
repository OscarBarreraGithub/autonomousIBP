# Lane 3 Next 22 B64ag Runtime Scope Tier-D

Status: Tier-D runtime gap. This sidecar does not flip M6, does not promote
`linear_propagator`, does not add an optional capture packet, and does not set
`full_eta_zero_contour_applied=true`.

## Decision

The `b64ag_runtime_scope_not_full_contour` blocker is not a stale flag that can
be cleared after the 50-digit comparator passes. It is a real runtime and packet
qualification boundary. The current stripped b64ag runtime has advanced beyond
the old selected-endpoint scaffold and the comparator can pass the current
39-row eps^2 surface, but the evidence still does not satisfy the full
eta-zero contour contract enforced by the readiness helper.

The honest state remains:

- `linear_propagator`: pending on `b64ag`
- `b64ag`: blocked on full gauge-link contour evidence
- `full_eta_zero_contour_applied`: false
- M6 flipped: no

## Where The Blocker Is Reported

`tools/reference-harness/scripts/audit_b64ag_golden_recapture_readiness.py`
maps these runtime checks to `b64ag_runtime_scope_not_full_contour`:

- `continuation_present`
- `continuation_variable_gaugex`
- `continuation_start_one_fortieth`
- `continuation_target_zero`
- `continuation_singular_zero`
- `full_eta_zero_contour_applied`
- `blocked_reason_absent`
- `runtime_text_rejects_fake_scope_words`

The immediate checked condition for the flag is:

```text
continuation.full_eta_zero_contour_applied is True
```

The helper also requires an empty blocked reason, no selected/scaffold/deferred
wording in audited runtime text, a six-master retained gauge-link state, full
contour diagnostics, and a passing 50-digit comparison against the retained
phase-0 `linear_propagator` golden. Its synthetic pass fixture shows the intended
runtime shape: `transport_scope = gaugex-zero-full-contour-coefficients`,
`full_eta_zero_contour_applied = true`, full diagnostics, and no closure claim by
the helper itself.

## Full Contour Contract

For this b64ag lane, "full contour" means a live gauge-link endpoint packet:

```text
I_t(eps) =
  FP_{gaugex=0} gaugex^(-nu(t))
    sum_j R_tj(gaugex, eps) F_j(gaugex, eps)
```

where:

- `F_j` is the transported six-master gauge-link DE solution from the finite
  `gaugex -> 1/40` boundary to `gaugex=0`;
- `R_tj` is the target-reduction row for every accepted retained packet target;
- `nu(t)` is the affected D4,D5 power normalization;
- `FP_{gaugex=0}` is the AMFlow `PickZeroRuleS` finite-part extraction;
- Laurent coefficients are fit after endpoint extraction for the qualifying
  packet surface;
- the output publishes contour, pole, finite-part, target-reduction, precision,
  and provenance diagnostics sufficient for the readiness helper.

The helper's retained packet contract currently expects the nine target labels
and 57 detailed coefficient rows described by `EXPECTED_PACKET_TARGETS` and
`EXPECTED_ORDERS`.

## Current Runtime Scope

The current C++ runtime is useful but still narrower than that contract:

- `src/cli/main.cpp` dispatches stripped b64ag states through
  `EvaluateLightlikeGaugeLinkFullEndpointPacket(...)` when final solution
  samples are removed.
- `src/runtime/lightlike_propagator.cpp` now transports endpoint terms for all
  six reviewed gauge-link DE masters from finite boundary samples and records
  `frobenius_recurrence_applied=true`.
- The CLI applies retained target reduction and finite-part extraction and can
  emit a successful externally comparable packet for the current eps^2 surface.
- The runtime deliberately sets `diagnostics.full_eta_zero_contour_applied =
  false`.
- The runtime summary deliberately says
  `full_eta_zero_contour_applied=false pending external AMFlow packet comparison
  and qualifier promotion`.
- The JSON output does not publish the readiness helper's required
  `runtime_provenance` object or `full_contour_diagnostics` buckets.
- The downstream target publication path still uses reviewed Laurent row tables
  through `B64agReviewedDownstreamTargetCoefficients(...)` for several packet
  targets instead of deriving every published Laurent row solely from live
  post-endpoint samples in the runtime.
- The fresh comparator fact from lane3 next21 is a 39-row eps^2 pass. The
  readiness helper's current packet contract still requires the 57-row retained
  order surface before it can declare golden-recapture readiness.

Those facts make a flag-only change unsafe. A runtime cannot truthfully set
`full_eta_zero_contour_applied=true` merely because an external comparator later
passes one scoped surface; the runtime evidence itself must already represent
the full contour packet and expose the diagnostics/provenance needed for the
fail-closed helper.

## Required Tier-D Runtime Work

The next closure attempt needs actual code, not metadata promotion:

1. Replace the reviewed downstream Laurent row table path with live
   post-endpoint Laurent coefficients for every accepted retained target and
   required epsilon order.
2. Extend the stripped b64ag runtime and comparator surface from the current
   39 eps^2 rows to the readiness helper's retained 57-row target/order surface,
   or update the helper only with an explicitly reviewed packet-surface change.
3. Emit `full_contour_diagnostics` with contour fingerprint/waypoints, nonzero
   pole diagnostics, `PickZeroRuleS` finite-part extraction, dropped singular
   powers, target-reduction fingerprint and row count, precision metadata, and
   provenance fingerprints.
4. Emit top-level `runtime_provenance.final_solution_samples_used_as_input =
   false` and matching diagnostic provenance for the candidate, retained state,
   and AMFlow golden.
5. Only after the above pass the readiness helper and the 50-digit comparator
   should the runtime set `continuation.full_eta_zero_contour_applied=true` with
   an empty blocked reason.
6. Only after that should `linear_propagator` be promoted through the upstream
   phase-0 optional-packet path and the M6 qualifier be rerun.

## Four-Role Review

Role A, implementation: APPROVE Tier-D gap. The code has real six-master
endpoint transport but still explicitly leaves the full-contour flag false and
does not emit the helper's full evidence object.

Role B, harness: APPROVE Tier-D gap. The readiness helper reports
`b64ag_runtime_scope_not_full_contour` from runtime continuation checks before
any M6 promotion path is available.

Role C, numeric: APPROVE the narrow 50-digit comparator fact only. A 39/39 pass
is necessary evidence, but it is not the full retained packet contract checked
by the readiness helper.

Role D, anti-fake: REJECT flip. Setting `full_eta_zero_contour_applied=true`
without replacing the remaining reviewed-row publication path and publishing
complete full-contour diagnostics/provenance would fake the qualifying contour
scope.

Unanimity for M6 flip: no.

## Anti-Fake Constraints

This lane intentionally does not:

- change any `full_eta_zero_contour_applied` value;
- weaken the readiness helper;
- loosen comparator tolerances;
- convert a comparator result into a runtime flag outside the runtime evidence;
- hide `linear_propagator -> b64ag` in phase-0 metadata;
- claim M6 closure.
