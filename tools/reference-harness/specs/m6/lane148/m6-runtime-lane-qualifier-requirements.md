# Lane 148 M6 Runtime-Lane Qualifier Requirements

Status: requirements audit only. This sidecar does not flip M6, does not mark
any runtime lane closed, and does not add qualifier evidence.

## Bottom Line

`qualify_milestone_m6.py` has no coefficient-count threshold and does not read
`full_eta_zero_contour_applied` directly. Its runtime-lane row schema is only:

```json
{"id": "<upstream row id>", "next_runtime_lane": "<lane id>"}
```

M6 passes only when the upstream summaries satisfy all of:

- `phase0_packet_set_qualified == true`
- `phase0_pending_ids == []`
- `blocked_phase0_examples == []`
- `case_study_families_qualified == true`

The current canonical rerun using
`tools/reference-harness/specs/m7/lane133/phase0-qualification.json` and
`tools/reference-harness/specs/m7/lane115/case-study-qualification.json` still
reports `current_state = "blocked-on-phase0-runtime-lanes"` with
`blocked_runtime_lanes = ["b61n", "b63n", "b64ag"]`.

Extending scoped coefficient transport, including extending `b61n` from five to
all seven `complex_kinematics` masters, does not by itself flip M6. The M6
composer never consumes those selected-coefficient sidecars. A flip requires the
affected phase-0 row to leave both `phase0_pending_ids` and
`blocked_phase0_examples` through a coherent upstream phase-0 qualification
packet.

## Exact Row Preconditions

`b61n` maps to the `complex_kinematics` phase-0 row.

To disappear from the blocked M6 frontier, `complex_kinematics` must become a
captured optional phase-0 row: `current_evidence_state` must be
`reference-captured`, it must declare an `optional_capture_packet`, and it must
not carry `next_runtime_lane`. The corresponding optional packet root must then
pass readiness, packet comparison, correct-digit scoring, and failure-code audit
well enough for `qualify_phase0_packet_set.py` to keep
`phase0_packet_set_qualified=true`.

Truthful `b61n` runtime evidence still requires live complex eta-contour
transport, not only selected endpoint coefficients. The current code recognizes
the seven-master `complex_kinematics` surface and builds a contour scaffold, but
the runtime summary still says full eta-infinity-to-eta=0 ODE propagation and
Laurent fitting remain deferred. Required code:

- execute coupled complex `eta` ODE propagation for all seven retained masters
  from eta-infinity boundary samples to `eta=0`;
- extract endpoint coefficients, including the remaining coupled masters
  `box[1,0,1,1]` and `box[1,1,1,1]`;
- populate runtime coefficients without reading final AMFlow `solution`
  samples as input;
- emit contour, branch, local-model, extraction, precision, and provenance
  diagnostics with `full_eta_zero_contour_applied=true`;
- compare the accepted packet against a high-precision AMFlow golden at the
  M6 50-digit floor and publish the required failure-code audit.

`b63n` maps to two phase-0 rows: `automatic_phasespace` and
`feynman_prescription`.

Because M6 reports unique blocked lane ids, `b63n` remains blocked until both
rows leave the pending/blocker lists. Each row must become a captured optional
phase-0 row with an `optional_capture_packet`, no `next_runtime_lane`, coherent
packet manifests, passing comparison and digit scoring, and complete required
failure-code audit coverage.

Truthful `b63n` runtime evidence still requires real Cutkosky endpoint coverage
for both rows. Current selected `automatic_phasespace` evidence is scoped and
keeps `full_eta_zero_contour_applied=false`; there is no current
`feynman_prescription` M6 coefficient evidence under
`tools/reference-harness/specs/m6`. Required code:

- replace the deferred Cutkosky boundary provider with a reviewed live provider
  for the exact `automatic_phasespace` and `feynman_prescription` surfaces;
- derive propagator prescriptions from loop prescriptions and fail closed on
  conflicts;
- validate cut topology, unit cut powers, eta-on-cut rejection, phase-volume
  rank, and uncut denominator roles;
- expand the `K_r(eps)` Cutkosky prefactor and build endpoint residue models;
- implement prescription-aware uncut subintegral and branch-ledger handling,
  including the `feynman_prescription` conjugate pair;
- select unambiguous eta-zero terms from live propagated terms and publish
  coefficients without final AMFlow solution samples;
- emit passing diagnostics with no deferred blocked reason and
  `full_eta_zero_contour_applied=true` only after that live branch runs.

`b64ag` maps to the `linear_propagator` phase-0 row.

To disappear from the blocked M6 frontier, `linear_propagator` must become a
captured optional phase-0 row with `optional_capture_packet`, no
`next_runtime_lane`, coherent manifests, passing comparison and digit scoring,
and complete required failure-code audit coverage.

Truthful `b64ag` runtime evidence still requires full gauge-link endpoint
transport. Current evidence transports one selected lightlike master and keeps
`full_eta_zero_contour_applied=false`; the runtime result says full six-master
gauge-link endpoint transport remains deferred. Required code:

- execute full `gaugex -> 0` transport for the six-master gauge-link DE basis;
- solve or replay the finite `gaugex=1/40` boundary without consuming final
  endpoint solution samples;
- apply target reduction and affected-power normalization across the accepted
  retained packet target surface, not only the first selected master;
- perform finite-part `PickZeroRuleS` extraction and post-endpoint Laurent
  fitting for all accepted targets;
- emit contour, pole, finite-part, reduction, precision, and provenance
  diagnostics with `full_eta_zero_contour_applied=true`;
- compare the accepted packet against a sufficiently precise AMFlow golden at
  the M6 50-digit floor and publish the required failure-code audit.

## Current Evidence Inventory

The current sidecars are real scoped runtime evidence, but none is qualifying
M6 closure evidence:

| lane | row | current evidence | result |
| --- | --- | --- | --- |
| `b61n` | `complex_kinematics` | lane141: 1 master, `4/4` coefficients, min 54 digits; lane142: 5 masters, `20/20` coefficients, min 54 digits | scoped selected endpoint coefficients, `full_eta_zero_contour_applied=false` |
| `b61n` | `complex_kinematics` | lane124: 7 masters, `14/14` coefficients at 50 digits | retained solution-sample fitting, `transport_applied=false`, not closure evidence |
| `b63n` | `automatic_phasespace` | lane143: 1 master, `4/4` coefficients; lane146: 4 masters, `19/19` coefficients | scoped selected Cutkosky endpoint coefficients, `full_eta_zero_contour_applied=false` |
| `b63n` | `feynman_prescription` | no current M6 coefficient evidence file | still pending on `b63n` |
| `b64ag` | `linear_propagator` | lane145: 1 transported master, `4/4` coefficients, min 37 digits | scoped selected gauge-link endpoint coefficient, `full_eta_zero_contour_applied=false` |

## Anti-Fake Gate

Roles A/B/C/D agree on the same gate:

- APPROVE the requirements finding: M6 has no coefficient-count threshold and
  flips only from upstream phase-0/case-study summary state.
- APPROVE the current sidecars only as real scoped evidence.
- REJECT any current `b61n`, `b63n`, or `b64ag` qualifier flip.
- REJECT any scaffold/status promotion based only on selected or all-master
  endpoint coefficient transport while the runtime still reports
  `full_eta_zero_contour_applied=false` or deferred full-contour coverage.
