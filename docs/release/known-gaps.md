# Release Known Gaps

Date: 2026-06-10

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
| `complex_kinematics` | Needs full `b61n` live complex eta-contour propagation and eta=0 endpoint extraction for the full seven-master surface without consuming final AMFlow solution samples. |
| `differential_equation_solver` | Needs C++ coverage for the `diffeq.wl` continuation and asymptotic-expansion workflow, not only the retained AMFlow `sol1` boundary-value comparison. |
| `feynman_prescription` | Needs prescription-aware `b63n` Cutkosky runtime coverage, both opposite-prescription branches, and the conjugacy check. |
| `linear_propagator` | Needs full `b64ag` gauge-link transport, finite-part extraction, target reduction over the accepted surface, and a high-precision AMFlow comparison packet. |
| `spacetime_dimension` | Needs full retained `D0 = 7/3` and `D0 = 1/3` output coverage plus the dimensional-recurrence check. |
| `user_defined_amfmode` | Needs end-to-end execution of the user-defined `AMFMode` hook through eta=0 endpoint extraction on the full requested target surface. |
| `user_defined_ending` | Needs both `final_Tradition` and `final_usr` ending workflows, including manual boundary writes and Gamma-ratio boundary handling. |

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
