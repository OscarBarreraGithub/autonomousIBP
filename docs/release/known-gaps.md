# Release Known Gaps

Date: 2026-06-12

This release note summarizes the AMFlow example reproduction gaps that remain
after the accepted M7 release-readiness packet. It is a release-facing summary
of the fuller inventory in [`amflow-example-coverage.md`](amflow-example-coverage.md),
which was introduced at `f0e5924` to distinguish retained evidence parity from
literal end-to-end reproduction of every upstream Mathematica example.

The accepted readiness proof is
[`m7-closure-evidence.md`](m7-closure-evidence.md). That proof remains valid,
but it should be read as a release signoff over the retained M5/M6/M7 evidence
surface, not as a claim that every upstream AMFlow example has been rewritten as
a live C++ workflow.

## Current Full Compared-Output Rows

Two upstream examples have full C++ parity for the currently accepted retained
comparison surface:

| AMFlow example | Accepted surface | Boundary |
| --- | --- | --- |
| `automatic_loop` | Fixed-epsilon solve-series comparison over the retained box1/box2 state surface. | This is not a fresh C++ rerun of the original upstream Mathematica script and reducer setup. |
| `automatic_vs_manual` | Retained `auto` output comparison; the manifest records `man` as the same integral surface. | This avoids duplicate integral keys and does not broaden beyond the retained comparator surface. |

## Not Fully Reproduced Rows

The remaining eight upstream examples have retained, selected-coefficient, or
scoped comparison evidence, but they are not full C++ runtime reproductions of
the upstream AMFlow scripts:

| AMFlow example | Remaining gap |
| --- | --- |
| `automatic_phasespace` | Needs full `b63n` live Cutkosky phase-space boundary reconstruction, weighted residue evaluation, endpoint propagation, and a qualified high-precision AMFlow packet. |
| `complex_kinematics` | Live AMFlow retained-golden coverage now exists for the seven-rule complex-mass box `sol` output; the remaining gap is full `b61n` live complex eta-contour propagation, eta=0 endpoint extraction, and packet qualification without consuming final AMFlow solution samples. |
| `differential_equation_solver` | Live AMFlow retained-golden coverage now exists for `run.wl` `redtable`/`diffeq`/`sol1`/`sol2` plus the upstream DESolver `diffeq.wl` continuation/asymptotic-expansion workflow; the remaining gap is C++ DESolver-runtime support for that workflow. |
| `feynman_prescription` | Live AMFlow retained-golden coverage now exists for both opposite-prescription `sol1`/`sol2` branches and the saved-output conjugacy check; the remaining gap is prescription-aware `b63n` Cutkosky C++ runtime coverage and comparator output namespacing. |
| `linear_propagator` | Needs full `b64ag` gauge-link transport, finite-part extraction, target reduction over the accepted surface, and a high-precision AMFlow comparison packet. |
| `spacetime_dimension` | Live AMFlow retained-golden coverage now exists for `D0 = 7/3`, `D0 = 1/3`, and the dimensional-recurrence check; the remaining gap is full C++ runtime support for the retained nondefault-`D0` workflow. |
| `user_defined_amfmode` | Needs end-to-end execution of the user-defined `AMFMode` hook through eta=0 endpoint extraction on the full requested target surface. |
| `user_defined_ending` | Live AMFlow rerun coverage now exists for both `final_Tradition` and `final_usr`, including manual boundary writes and the Gamma-ratio boundary path; the remaining release gap is C++ execution of both ending workflows. |

For the `linear_propagator` row, the lane 4 post-M7 guard records the
non-promotion boundary for existing selected `b64ag` evidence and the fixture
shape expected of a future fail-closed recapture/runtime-packet guard:
[`b64ag-post-m7-continuation-guard.md`](../../tools/reference-harness/specs/m7/lane4/b64ag-post-m7-continuation-guard.md).
It is a routing reference only, not new runtime evidence or a release-signoff
input.

For the `differential_equation_solver` row, the post-M7 live rerun note records
the closed upstream AMFlow data-generation side of the gap:
[`amflow-live-rerun-differential_equation_solver.md`](amflow-live-rerun-differential_equation_solver.md).
It is live AMFlow evidence only and does not claim C++ DESolver execution or
change the accepted M7 release-readiness inputs.

For the `complex_kinematics` row, the post-M7 live rerun note records the
closed upstream AMFlow data-generation side of the retained complex-mass box
output:
[`amflow-live-rerun-complex_kinematics.md`](amflow-live-rerun-complex_kinematics.md).
It is live AMFlow evidence only and does not claim C++ full complex eta-contour
execution, optional phase-0 packet qualification, or a change to the accepted
M7 release-readiness inputs.

For the `user_defined_ending` row, the post-M7 live rerun note records the
closed upstream AMFlow data-generation side of the gap:
[`amflow-live-rerun-user_defined_ending.md`](amflow-live-rerun-user_defined_ending.md).
It is live AMFlow evidence only and does not claim C++ ending-scheme execution
or change the accepted M7 release-readiness inputs.

For the `feynman_prescription` row, the post-M7 live rerun note records the
closed upstream AMFlow data-generation side of the two opposite-prescription
outputs and conjugacy check:
[`amflow-live-rerun-feynman_prescription.md`](amflow-live-rerun-feynman_prescription.md).
It is live AMFlow evidence only and does not claim C++ `b63n` Cutkosky runtime
execution, prescription-aware comparator namespacing, or a change to the
accepted M7 release-readiness inputs.

There are no zero-evidence upstream rows in the current inventory: all ten
upstream example classes have at least some retained or C++ comparison evidence
in the repository. The gap is the narrower and more important one: most rows do
not yet have literal all-example live C++ runtime parity.

## Release Boundary

The M7 release gate accepts the retained M5 feature-parity packet, the accepted
M6 qualification packet, and the reviewed M7 release sidecars. It does not
promote any unaccepted sidecar, create new runtime evidence, or close the full
AMFlow all-example reproduction backlog.

Use the sidecar inventory and queue helpers from [`tools.md`](tools.md) when
reviewing historical or unaccepted M7 sidecars. Use the fuller coverage
inventory when deciding which runtime lane owns each remaining AMFlow example
gap.
