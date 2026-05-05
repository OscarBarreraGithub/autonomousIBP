# M6 Runtime Lanes Survey

Lane: `lane119`
Scope: `b61n`, `b63n`, `b64ag`
Verdict: survey-only fallback. No runtime lane is closed.

## Closure Rule

The M6 qualifier closes the phase-0 runtime-lane surface only when the
phase-0 packet-set qualifier reports both:

- `phase0_pending_ids: []`
- `blocked_phase0_examples: []`

The M6 composer then requires the phase-0 packet set to be qualified before it
can report `milestone-m6-qualified`. If pending phase-0 examples remain, the M6
summary reports `blocked-on-phase0-runtime-lanes`; if the packet set itself is
not qualified, it reports `blocked-on-phase0-packet-set`.

The qualification scaffold also makes the metadata contract fail-closed:

- captured optional examples must use `current_evidence_state:
  reference-captured` and carry an `optional_capture_packet`;
- pending examples must carry `next_runtime_lane`;
- captured examples must not retain `next_runtime_lane`.

Therefore a real closure requires a reviewed optional capture packet and
accepted packet-set evidence, not just removal of `next_runtime_lane`.

## Exact Evidence Required

The relevant template entries are:

| Example | Runtime lane | Current state | Required profile |
| --- | --- | --- | --- |
| `automatic_phasespace` | `b63n` | `cataloged-pending-capture` | `core-package-family-default` |
| `complex_kinematics` | `b61n` | `cataloged-pending-capture` | `core-package-family-default` |
| `feynman_prescription` | `b63n` | `cataloged-pending-capture` | `core-package-family-default` |
| `linear_propagator` | `b64ag` | `cataloged-pending-capture` | `core-package-family-default` |

`core-package-family-default` requires at least `50` correct digits. The
required failure-code profile must preserve explicit reporting for:

- `insufficient_precision`
- `master_set_instability`
- `boundary_unsolved`
- `continuation_budget_exhausted`

For each lane, the minimum acceptable evidence is:

- a C++ runtime-produced candidate packet or reviewed optional capture packet;
- canonical result manifests and primary run manifests with provenance showing
  the data came from the runtime path under review;
- packet comparison, correct-digit scoring, failure-code audit, and
  `qualify_phase0_packet_set.py` passing together;
- an M6 summary with no pending phase-0 IDs and no blocked runtime-lane entries
  for the closed example.

M5 lane45/lane50 evidence is not sufficient for M6 closure. Those rows used the
M5 feature-parity rule and a 30-digit tolerance, while M6 requires packet-set
qualification at the 50-digit floor plus the required failure-code profile.
Lane108 raw phase-0 validation is useful numeric context, but it is not a
runtime-lane closure unless the M6 packet-set tools consume it as a reviewed
capture and the blocked phase-0 entries disappear through the qualifier.

## Lane Findings

| Lane | Examples | Evidence needed | Missing `src/cli/main.cpp` path | Complexity |
| --- | --- | --- | --- | --- |
| `b61n` | `complex_kinematics` | A live complex-kinematics/eta-continuation runtime capture accepted as an optional phase-0 packet, with >=50 correct digits and required failure-code audit. | `solve-series` can ingest retained loop solution-sample cache values, but the retained-state path reports that full complex eta-contour endpoint reconstruction remains deferred. The solver interface still defaults `SupportsReviewedComplexEtaContinuation()` to false. | Hard |
| `b63n` | `automatic_phasespace`, `feynman_prescription` | Live Cutkosky phase-space and prescription-aware runtime captures accepted as optional phase-0 packets, with >=50 correct digits and required failure-code audit. | The CLI parses phase-space metadata and can fit retained solution samples, but the runtime path reports that full Cutkosky phase-space boundary reconstruction from cut propagators remains deferred. The built-in phase-space boundary provider still throws a deferred `boundary_unsolved` error. | Hard overall |
| `b64ag` | `linear_propagator` | A live lightlike-linear/gauge-link propagator runtime capture accepted as an optional phase-0 packet, with >=50 correct digits and required failure-code audit. | `solve-series` has no public route from AMFlow-state input to a reviewed lightlike-linear/gauge-link runtime path. The available route is retained finite solution-sample ingestion, with full AMFlow loop-boundary reconstruction and endpoint contour execution still deferred. | Medium |

`b63n` has one narrower-looking subcase in `feynman_prescription`, but closing
the lane would still leave `automatic_phasespace` blocked on the same lane and
would require real phase-space boundary reconstruction evidence. Treating only a
scoped retained solution as closure would be a false-positive.

## Smallest-Lane Decision

No runtime lane was closed in lane119. `b64ag` looks smaller than `b61n` and the
full `b63n` surface, but it still needs a real runtime path and an accepted M6
packet. `b61n` and `b63n` require deeper continuation or phase-space boundary
work. Because all three lanes require runtime physics implementation plus M6
packet evidence, the correct failure-safe action is to ship this survey only.

No `next_runtime_lane` values were removed, no pending phase-0 entries were
marked `reference-captured`, and no M6/M7/release-readiness claim is made here.

## Role Review

All four lane119 roles approved the survey-only conclusion:

| Role | Result | Summary |
| --- | --- | --- |
| A | APPROVE | M6 closure depends on empty phase-0 pending and blocked runtime-lane lists plus packet-set qualification. |
| B | APPROVE | Existing M5/lane108 evidence is useful context but does not satisfy M6 runtime-lane closure requirements. |
| C | APPROVE | CLI/runtime survey found deferred paths for complex eta-continuation, Cutkosky phase-space reconstruction, and lightlike-linear runtime execution. |
| D | APPROVE | Anti-fake-parity gate rejected metadata-only or retained-state closure claims for all three lanes. |

## Gate Results

Lane119 changes documentation only. The required gate passed on 2026-05-05:

- `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`: passed
- `cmake --build build --parallel 1`: passed
- `./build/amflow-tests`: passed
- `ctest --test-dir build --output-on-failure`: passed

Prior parity is unchanged.
