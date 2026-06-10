# AMFlow Example Coverage Inventory

Date: 2026-06-10

This inventory answers the narrow question "do we reproduce every upstream
AMFlow example notebook/script?" The honest answer is no if "reproduce" means a
full from-scratch C++ runtime implementation of each upstream Mathematica
example. The current release evidence covers all ten frozen AMFlow example IDs
at the M5/M7 feature-surface level, but most rows are retained-state,
solution-sample, selected-coefficient, or scoped-exception evidence rather than
full AMFlow-style runtime reproduction.

The cluster Mathematica smoke test passed with:

```text
13.3.0 for Linux x86 (64-bit) (June 3, 2023)
```

## Upstream Inventory

`references/snapshots/amflow/` is a partial snapshot. It contains README, FAQ,
CHANGELOG, option notes, `kira-interface.m`, and the CPC archive URL, but it
does not contain the upstream `examples/` directory.

The public upstream AMFlow `1.2` tag at
`https://gitlab.com/multiloop-pku/amflow` and the retained local AMFlow trees
under `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/.../amflow`
ship ten example directories. They contain eleven Mathematica script entry
files and no `.nb` notebooks:

| Upstream example | Mathematica entry file(s) | C++ lane / evidence in this repo | Status |
| --- | --- | --- | --- |
| `automatic_loop` | `examples/automatic_loop/run.wl` | Core solve-series evidence: M5 lane39/lane45 `automatic_loop.eps8`, 126/126 coefficients, min 41 digits; phase-0 retained state `tools/reference-harness/specs/phase0/automatic_loop.amflow-state.json`. | `reproduced-fully` |
| `automatic_phasespace` | `examples/automatic_phasespace/run.wl` | `b63n`: M6 lane143/lane146 selected Cutkosky endpoint evidence, 19/19 coefficients in lane146; M5 retained solution-sample comparison, 11/11 coefficients. | `reproduced-partial` |
| `automatic_vs_manual` | `examples/automatic_vs_manual/run.wl` | Core solve-series evidence: M5 lane39, 89/89 coefficients, min 36 digits; golden manifest compares `auto` and records `man` as the same target surface. | `reproduced-fully` |
| `complex_kinematics` | `examples/complex_kinematics/run.wl` | `b61n`: M6 lane141/lane142 selected endpoint evidence, 20/20 coefficients in lane142; M5 retained solution-sample comparison, 14/14 coefficients. | `reproduced-partial` |
| `differential_equation_solver` | `examples/differential_equation_solver/run.wl`; `examples/differential_equation_solver/diffeq.wl` | M5 lane39 compares the retained AMFlow `sol1` boundary-value surface, 3/3 coefficients; a `sol2` golden manifest exists, but the full `diffeq.wl` DESolver checks and asymptotic expansions are not reproduced end to end. | `reproduced-partial` |
| `feynman_prescription` | `examples/feynman_prescription/run.wl` | `b63n` planned/pending; M5 lane39/lane45 compares retained `sol1` solution-sample output, 76/76 coefficients. No lane14x M6 coefficient evidence exists for this row, and the opposite-prescription `sol2` branch is not fully covered. | `reproduced-partial` |
| `linear_propagator` | `examples/linear_propagator/run.wl` | `b64ag`: M6 lane145/lane147 selected gauge-link endpoint evidence, 18/18 coefficients in lane147; M5 retained finite-solution-sample comparison, 57/57 coefficients. | `reproduced-partial` |
| `spacetime_dimension` | `examples/spacetime_dimension/run.wl` | M5 lane50 direct nondefault-D flag comparison, 2/2 coefficients, min 51 digits; retained phase-0 packet exists. This is not full retained `sol13D`/`sol73D` plus recurrence-check reproduction. | `reproduced-partial` |
| `user_defined_amfmode` | `examples/user_defined_amfmode/run.wl` | Live Mathematica+AMFlow rerun on 2026-06-10 matched the committed M5 lane50 scoped AMFlow golden exactly for `j[box1,-2,1,1,2]`; see `docs/release/amflow-live-rerun-user_defined_amfmode.md`. This is a live retained-golden reproducibility claim, not a broader C++ eta=0 endpoint-runtime claim. | `reproduced-fully-live` |
| `user_defined_ending` | `examples/user_defined_ending/run.wl` | M5 lane50 scoped `final_usr` comparison, 6/6 coefficients, min 36 digits; current result is eta-infinity/asymptotic scoped and does not cover the full `final_Tradition` plus `final_usr` ending workflow. | `reproduced-partial` |

No upstream example is `upstream-only-no-data`: the upstream scripts are
fetchable from AMFlow tag `1.2`, and every example has at least some retained or
C++ comparison evidence in the current repository. The important limitation is
that only two rows are full for the currently compared output surface; the other
eight rows are partial.

## Full Parity Rows

Current full compared-output C++ parity is limited to:

- `automatic_loop`: accepted fixed-epsilon solve-series comparison over the
  retained box1/box2 state surface.
- `automatic_vs_manual`: accepted comparison over the retained `auto` output;
  the manifest records `man` as the same integral surface and avoids duplicate
  integral keys.

This is not the same as rerunning the original Mathematica scripts from scratch,
including reducer setup, inside C++. It is full only for the accepted retained
state and comparator surface.

## Not Full Rows

The remaining eight upstream examples are not full C++ runtime reproductions:

- `complex_kinematics` needs full `b61n` live complex eta-contour propagation
  and endpoint extraction for the full seven-master surface without consuming
  final AMFlow solution samples.
- `automatic_phasespace` needs full `b63n` live Cutkosky phase-space boundary
  reconstruction, weighted residue evaluation, endpoint propagation, and packet
  qualification.
- `feynman_prescription` needs the same `b63n` live Cutkosky/prescription-aware
  runtime work plus coverage of both opposite-prescription branches and the
  conjugacy check.
- `linear_propagator` needs full `b64ag` gauge-link transport, finite-part
  extraction, target reduction over the accepted surface, and a high-precision
  AMFlow comparison packet.
- `differential_equation_solver` needs C++ DESolver coverage for the `diffeq.wl`
  continuation and asymptotic-expansion workflow, not only the retained AMFlow
  `sol1` boundary-value comparison.
- `spacetime_dimension` needs full retained `D0 = 7/3` and `D0 = 1/3` example
  output coverage plus the dimensional-recurrence check, not only the current
  direct nondefault-D flag slice.
- `user_defined_amfmode` needs end-to-end execution of the user-defined
  `AMFMode` hook through eta=0 endpoint extraction on the full requested target
  surface.
- `user_defined_ending` needs end-to-end user-defined ending-scheme execution
  for both `final_Tradition` and `final_usr`, including the manual boundary
  writes and Gamma-ratio boundary handling.

There are no zero-evidence `not-reproduced` rows in this inventory. For the
not-full rows, the concrete missing work is:

| Example | Mathematica data generation needed? | New C++ runtime needed? |
| --- | --- | --- |
| `automatic_phasespace` | Yes. A qualified high-precision AMFlow packet is needed after the live Cutkosky path is implemented. | Yes. Implement the live `b63n` Cutkosky boundary/residue/endpoint path. |
| `complex_kinematics` | Yes. A qualified full-contour AMFlow packet is needed for the final seven-master surface. | Yes. Implement full `b61n` complex eta-contour propagation and endpoint extraction. |
| `differential_equation_solver` | Yes or promote existing retained backup data for `sol2`, `asyexp0`, `asyexp1`, and `asyexp1-fit` into comparator-ready goldens. | Yes. Implement the DESolver continuation and asymptotic-expansion workflow. |
| `feynman_prescription` | Yes. Generate/promote both `sol1` and `sol2` with a qualified conjugacy/comparator packet. | Yes. Implement prescription-aware `b63n` Cutkosky runtime coverage. |
| `linear_propagator` | Yes. Recapture/promote a high-precision gauge-link packet for the full target surface. | Yes. Implement full `b64ag` gauge-link transport and finite-part extraction. |
| `spacetime_dimension` | Yes or promote retained `sol13D`/`sol73D` outputs and recurrence-check data into comparator-ready goldens. | Yes. Implement full retained D0 semantics for both dimensions and the recurrence check. |
| `user_defined_amfmode` | Yes or promote the full retained three-target user-mode output into a comparator packet. | Yes. Execute the user-defined `AMFMode` hook through eta=0 endpoint extraction. |
| `user_defined_ending` | Yes or promote both retained `final_Tradition` and `final_usr` outputs into comparator packets. | Yes. Execute both ending workflows, including manual boundary writes and Gamma-ratio handling. |

## M7 Scope Statement

The accepted M7 release-readiness path consumes an accepted M5 feature-parity
packet whose required example classes are all ten upstream AMFlow examples. That
is a feature-surface gate, not a literal claim that every upstream Mathematica
example script has been fully reimplemented as a live C++ runtime workflow.

Therefore, "feature parity with AMFlow" in the current M7 evidence covers the
frozen example-class set through retained goldens, accepted exceptions, scoped
C++ comparisons, and release-review sidecars. It does not cover full
from-scratch C++ reproduction of every AMFlow example. Literal all-example full
runtime parity remains incomplete.
