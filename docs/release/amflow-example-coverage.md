# AMFlow Example Coverage Inventory

Date: 2026-06-12

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
| `differential_equation_solver` | `examples/differential_equation_solver/run.wl`; `examples/differential_equation_solver/diffeq.wl` | Live Mathematica+AMFlow rerun on 2026-06-12 matched the retained full AMFlow `de-d0-pair` `redtable`/`diffeq`/`sol1`/`sol2` packet byte-for-byte and reran the upstream DESolver `diffeq.wl` continuation/asymptotic-expansion workflow with only sub-precision printed-coefficient drift against retained Kira backup files; see `docs/release/amflow-live-rerun-differential_equation_solver.md`. This is a live AMFlow retained-golden reproduction claim, not C++ DESolver-runtime coverage. | `reproduced-fully-live` |
| `feynman_prescription` | `examples/feynman_prescription/run.wl` | `b63n` planned/pending; M5 lane39/lane45 compares retained `sol1` solution-sample output, 76/76 coefficients. No lane14x M6 coefficient evidence exists for this row, and the opposite-prescription `sol2` branch is not fully covered. | `reproduced-partial` |
| `linear_propagator` | `examples/linear_propagator/run.wl` | `b64ag`: M6 lane145/lane147 selected gauge-link endpoint evidence, 18/18 coefficients in lane147; M5 retained finite-solution-sample comparison, 57/57 coefficients. | `reproduced-partial` |
| `spacetime_dimension` | `examples/spacetime_dimension/run.wl` | Live Mathematica+AMFlow rerun on 2026-06-11 matched the retained full AMFlow `de-d0-pair` `sol73D`/`sol13D` packet at the upstream script's requested 20-digit precision and verified the dimensional-recurrence residual through `O[eps]^2`; see `docs/release/amflow-live-rerun-spacetime_dimension.md`. The raw files are not byte-identical because the retained packet stores promoted 60-precision text. This is a live retained-golden reproducibility claim, not a broader C++ nondefault-`D0` runtime claim. | `reproduced-fully-live` |
| `user_defined_amfmode` | `examples/user_defined_amfmode/run.wl` | Live Mathematica+AMFlow rerun on 2026-06-10 matched the committed M5 lane50 scoped AMFlow golden exactly for `j[box1,-2,1,1,2]`; see `docs/release/amflow-live-rerun-user_defined_amfmode.md`. This is a live retained-golden reproducibility claim, not a broader C++ eta=0 endpoint-runtime claim. | `reproduced-fully-live` |
| `user_defined_ending` | `examples/user_defined_ending/run.wl` | Live Mathematica+AMFlow rerun on 2026-06-10 completed the upstream script, wrote both `final_Tradition` and `final_usr`, and matched the committed M5 lane50 scoped `final_usr` golden exactly for `j[box1,-2,1,1,2]`; see `docs/release/amflow-live-rerun-user_defined_ending.md`. This is a live retained-golden reproducibility claim, not broader C++ ending-scheme runtime coverage. | `reproduced-fully-live` |

No upstream example is `upstream-only-no-data`: the upstream scripts are
fetchable from AMFlow tag `1.2`, and every example has at least some retained or
C++ comparison evidence in the current repository. The important limitation is
that only two rows have full compared-output C++ parity. Four additional rows
now have live Mathematica+AMFlow retained-golden rerun evidence; the other four
rows remain partial.

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

The remaining eight upstream examples are not full C++ runtime reproductions.
This list intentionally follows the release-facing order in
[`known-gaps.md`](known-gaps.md):

- `automatic_phasespace` needs full `b63n` live Cutkosky phase-space boundary
  reconstruction, weighted residue evaluation, endpoint propagation, and packet
  qualification.
- `complex_kinematics` needs full `b61n` live complex eta-contour propagation
  and endpoint extraction for the full seven-master surface without consuming
  final AMFlow solution samples.
- `differential_equation_solver` now has live Mathematica+AMFlow retained-golden
  reproduction of `run.wl` plus the upstream DESolver `diffeq.wl` continuation
  and asymptotic-expansion workflow, but the C++ runtime still needs DESolver
  coverage for that workflow rather than only retained-state `sol1`/`sol2`
  solve-series comparisons.
- `feynman_prescription` needs the same `b63n` live Cutkosky/prescription-aware
  runtime work plus coverage of both opposite-prescription branches and the
  conjugacy check.
- `linear_propagator` needs full `b64ag` gauge-link transport, finite-part
  extraction, target reduction over the accepted surface, and a high-precision
  AMFlow comparison packet.
- `spacetime_dimension` now has live Mathematica+AMFlow retained-golden
  reproduction of the full `D0 = 7/3` and `D0 = 1/3` output surface plus the
  dimensional-recurrence check, but the C++ runtime still needs full retained
  nondefault-`D0` semantics for both dimensions and the recurrence workflow.
- `user_defined_amfmode` needs end-to-end execution of the user-defined
  `AMFMode` hook through eta=0 endpoint extraction on the full requested target
  surface.
- `user_defined_ending` has a fresh live AMFlow rerun for both
  `final_Tradition` and `final_usr`; it still needs end-to-end C++ ending-scheme
  execution, including the manual boundary writes and Gamma-ratio boundary
  handling.

There are no zero-evidence `not-reproduced` rows in this inventory. For the
not-full rows, the concrete missing work is:

| Example | Mathematica data generation needed? | New C++ runtime needed? |
| --- | --- | --- |
| `automatic_phasespace` | Yes. A qualified high-precision AMFlow packet is needed after the live Cutkosky path is implemented. | Yes. Implement the live `b63n` Cutkosky boundary/residue/endpoint path. |
| `complex_kinematics` | Yes. A qualified full-contour AMFlow packet is needed for the final seven-master surface. | Yes. Implement full `b61n` complex eta-contour propagation and endpoint extraction. |
| `differential_equation_solver` | No for live AMFlow retained-golden reproduction: `run.wl` was rerun byte-identically for `redtable`/`diffeq`/`sol1`/`sol2`, and `diffeq.wl` was rerun with sub-precision numeric drift against retained Kira backup `asyexp0`/`asyexp1`/`asyexp1-fit` files. A repo-local comparator-ready manifest for the DESolver expansion files is still absent. | Yes. Implement the DESolver continuation and asymptotic-expansion workflow. |
| `feynman_prescription` | Yes. Generate/promote both `sol1` and `sol2` with a qualified conjugacy/comparator packet. | Yes. Implement prescription-aware `b63n` Cutkosky runtime coverage. |
| `linear_propagator` | Yes. Recapture/promote a high-precision gauge-link packet for the full target surface. | Yes. Implement full `b64ag` gauge-link transport and finite-part extraction. |
| `spacetime_dimension` | No for live AMFlow retained-golden reproduction: the full `sol13D`/`sol73D` packet and recurrence check were rerun live on 2026-06-11. A repo-local comparator-ready manifest is still absent. | Yes. Implement full retained D0 semantics for both dimensions and the recurrence check. |
| `user_defined_amfmode` | Yes or promote the full retained three-target user-mode output into a comparator packet. | Yes. Execute the user-defined `AMFMode` hook through eta=0 endpoint extraction. |
| `user_defined_ending` | No fresh AMFlow script rerun is required for the two-output upstream surface; a live rerun now exists, while broader committed comparator promotion remains separate from the scoped M5 `final_usr` golden. | Yes. Execute both ending workflows, including manual boundary writes and Gamma-ratio handling. |

For `linear_propagator`, the post-M7 lane 4 continuation guard is
[`b64ag-post-m7-continuation-guard.md`](../../tools/reference-harness/specs/m7/lane4/b64ag-post-m7-continuation-guard.md).
It records the non-promotion boundary for existing selected `b64ag` evidence
and the expected fail-closed fixture shape for a future runtime-packet guard. It
is routing documentation only; it does not add runtime evidence, promote the
optional phase-0 packet, or change the accepted M7 release-readiness inputs.

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
