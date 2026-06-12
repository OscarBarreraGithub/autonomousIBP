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

The 2026-06-12 upstream-completeness audit found no AMFlow package exposed by
the Mathematica `13.3.0-fasrc01` `$Path`: `FindFile["AMFlow`"]` and
`FindFile["AMFlow.m"]` both returned `$Failed`. The module system likewise has
no `amflow` module, and a direct `*amflow*` name search under the Mathematica
install-tree roots emitted no matches. Broader `/n/sw` name scans were bounded
negative probes: they timed out with no AMFlow matches emitted, so they are not
claimed as exhaustive over every unreadable `/n/sw` path. The audited cluster
source is the retained clean upstream Git checkout at
`/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260419-required-set/inputs/upstream/amflow`,
pinned at AMFlow tag `1.1` commit
`775162498ab18493c45254b861669b4151b841ee` with `AMFlow.m` SHA-256
`6fd47002b36399ee71c38e3e43e5e75541d1f2641966ca103fc8b8ce37dc7add`.
The same checkout's tag `1.2` / `origin/master` has different file contents but
the identical `.wl`/`.nb` entry-path set. The deterministic registry is
[`amflow-upstream-example-inventory.registry.json`](../../tools/reference-harness/specs/release/amflow-upstream-example-inventory.registry.json).

That audited stock upstream set ships ten example directories. They contain
eleven Mathematica script entry files and no `.nb` notebooks:

| Upstream example | Mathematica entry file(s) | C++ lane / evidence in this repo | Status |
| --- | --- | --- | --- |
| `automatic_loop` | `examples/automatic_loop/run.wl` | Live Mathematica+AMFlow rerun on 2026-06-12 matched the original retained canonical AMFlow `sol1`/`sol2` capture hashes exactly; against the later promoted 80-precision retained goldens, the canonical skeleton matched with 38/38 minimum observed digits, below the promoted 50-digit floor; see `docs/release/amflow-live-rerun-automatic_loop.md`. Existing C++ evidence remains the M5 lane39/lane45 `automatic_loop.eps8` solve-series surface, 126/126 coefficients, min 41 digits, plus phase-0 retained state `tools/reference-harness/specs/phase0/automatic_loop.amflow-state.json`. This is live retained-capture reproducibility, not broader C++ runtime evidence beyond the accepted solve-series surface. | `reproduced-matches-golden-at-requested-precision` |
| `automatic_phasespace` | `examples/automatic_phasespace/run.wl` | `b63n`: M6 lane143/lane146 selected Cutkosky endpoint evidence, 19/19 coefficients in lane146; M5 retained solution-sample comparison, 11/11 coefficients. | `reproduced-partial` |
| `automatic_vs_manual` | `examples/automatic_vs_manual/run.wl` | Live Mathematica+AMFlow rerun on 2026-06-12 matched the original retained canonical AMFlow `auto`/`man` capture hashes exactly; against the later promoted 60-precision retained goldens, the canonical skeleton matched with 18/18 minimum observed digits, below the promoted 50-digit floor; see `docs/release/amflow-live-rerun-automatic_vs_manual.md`. Existing C++ evidence remains M5 lane39, 89/89 coefficients, min 36 digits, with comparator ingestion scoped to `auto` because `auto` and `man` carry the same target surface. This is live retained-capture reproducibility, not broader C++ runtime evidence beyond the accepted solve-series surface. | `reproduced-matches-golden-at-requested-precision` |
| `complex_kinematics` | `examples/complex_kinematics/run.wl` | Live Mathematica+AMFlow rerun on 2026-06-12 matched the retained full AMFlow seven-rule complex-mass box `sol` packet byte-for-byte; see `docs/release/amflow-live-rerun-complex_kinematics.md`. Existing C++ evidence remains scoped to M5 retained solution-sample comparison and M6 lane141/lane142 selected endpoint coefficients. This is live retained-golden reproducibility, not broader `b61n` complex eta-contour runtime coverage. | `reproduced-fully-live` |
| `differential_equation_solver` | `examples/differential_equation_solver/run.wl`; `examples/differential_equation_solver/diffeq.wl` | Live Mathematica+AMFlow rerun on 2026-06-12 matched the retained full AMFlow `de-d0-pair` `redtable`/`diffeq`/`sol1`/`sol2` packet byte-for-byte and reran the upstream DESolver `diffeq.wl` continuation/asymptotic-expansion workflow with only sub-precision printed-coefficient drift against retained Kira backup files; see `docs/release/amflow-live-rerun-differential_equation_solver.md`. This is a live AMFlow retained-golden reproduction claim, not C++ DESolver-runtime coverage. | `reproduced-fully-live` |
| `feynman_prescription` | `examples/feynman_prescription/run.wl` | Live Mathematica+AMFlow rerun on 2026-06-12 wrote both opposite-prescription outputs, matched the retained full AMFlow `sol1`/`sol2` packet exactly, and verified the conjugacy deltas over the saved outputs; see `docs/release/amflow-live-rerun-feynman_prescription.md`. Existing C++ comparator evidence remains scoped to `sol1` because prescription-aware output namespacing is separate. This is live retained-golden reproducibility, not broader `b63n` Cutkosky runtime coverage. | `reproduced-fully-live` |
| `linear_propagator` | `examples/linear_propagator/run.wl` | Live Mathematica+AMFlow rerun on 2026-06-12 matched the retained phase0 AMFlow `sol` golden byte-for-byte for the upstream lightlike gauge-link `SolveIntegralsGaugeLink` output; see `docs/release/amflow-live-rerun-linear_propagator.md`. Existing `b64ag` C++ evidence remains selected/retained-sample evidence: M6 lane145/lane147 selected gauge-link endpoint evidence, 18/18 coefficients in lane147; M5 retained finite-solution-sample comparison, 57/57 coefficients. This is a live retained-golden reproducibility claim, not broader C++ gauge-link runtime coverage. | `reproduced-fully-live` |
| `spacetime_dimension` | `examples/spacetime_dimension/run.wl` | Live Mathematica+AMFlow rerun on 2026-06-11 matched the retained full AMFlow `de-d0-pair` `sol73D`/`sol13D` packet at the upstream script's requested 20-digit precision and verified the dimensional-recurrence residual through `O[eps]^2`; see `docs/release/amflow-live-rerun-spacetime_dimension.md`. The raw files are not byte-identical because the retained packet stores promoted 60-precision text. This is a live retained-golden reproducibility claim, not a broader C++ nondefault-`D0` runtime claim. | `reproduced-fully-live` |
| `user_defined_amfmode` | `examples/user_defined_amfmode/run.wl` | Live Mathematica+AMFlow rerun on 2026-06-10 matched the committed M5 lane50 scoped AMFlow golden exactly for `j[box1,-2,1,1,2]`; see `docs/release/amflow-live-rerun-user_defined_amfmode.md`. This is a live retained-golden reproducibility claim, not a broader C++ eta=0 endpoint-runtime claim. | `reproduced-fully-live` |
| `user_defined_ending` | `examples/user_defined_ending/run.wl` | Live Mathematica+AMFlow rerun on 2026-06-10 completed the upstream script, wrote both `final_Tradition` and `final_usr`, and matched the committed M5 lane50 scoped `final_usr` golden exactly for `j[box1,-2,1,1,2]`; see `docs/release/amflow-live-rerun-user_defined_ending.md`. This is a live retained-golden reproducibility claim, not broader C++ ending-scheme runtime coverage. | `reproduced-fully-live` |

No upstream example is `upstream-only-no-data`: the upstream scripts are
fetchable from the audited AMFlow tags `1.1` and `1.2`, and every example has
at least some retained or C++ comparison evidence in the current repository.
The important limitation is that only two rows have full compared-output C++
parity. Nine rows now have live Mathematica+AMFlow retained-golden or
retained-capture rerun evidence; the remaining row remains partial.

The broader
`/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/.../amflow` area also
contains copied AMFlow workspaces with untracked local case-study example
directories such as `ttj_planar_topology_tt_family`,
`wpair_planar_t1_euclidean`, and `moller_ew_double_box_seed`. Those directories
are not present in either audited upstream tag `1.1` or `1.2`, so they are
excluded from this upstream example completeness claim rather than counted as
missed upstream AMFlow examples.

## Full Parity Rows

Current full compared-output C++ parity is limited to:

- `automatic_loop`: accepted fixed-epsilon solve-series comparison over the
  retained box1/box2 state surface. It now also has a live AMFlow rerun of the
  upstream script at the script's requested 40-digit precision.
- `automatic_vs_manual`: accepted comparison over the retained `auto` output;
  the manifest records `man` as the same integral surface and avoids duplicate
  integral keys. It now also has a live AMFlow rerun of both the automatic and
  manual upstream outputs at the script's requested 20-digit precision.

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
- `complex_kinematics` now has live Mathematica+AMFlow retained-golden
  reproduction of the seven-rule complex-mass box `sol` output, but it still
  needs full `b61n` live complex eta-contour propagation and endpoint
  extraction without consuming final AMFlow solution samples.
- `differential_equation_solver` now has live Mathematica+AMFlow retained-golden
  reproduction of `run.wl` plus the upstream DESolver `diffeq.wl` continuation
  and asymptotic-expansion workflow, but the C++ runtime still needs DESolver
  coverage for that workflow rather than only retained-state `sol1`/`sol2`
  solve-series comparisons.
- `feynman_prescription` now has live Mathematica+AMFlow retained-golden
  reproduction for both opposite-prescription `sol1`/`sol2` outputs and the
  saved-output conjugacy check, but it still needs `b63n` live
  Cutkosky/prescription-aware C++ runtime coverage and comparator output
  namespacing.
- `linear_propagator` now has live Mathematica+AMFlow retained-golden
  reproduction of the upstream lightlike gauge-link `sol` output, but still
  needs full C++ `b64ag` gauge-link transport, finite-part extraction, target
  reduction over the accepted surface, and a high-precision AMFlow comparison
  packet.
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
| `complex_kinematics` | No for live AMFlow retained-golden reproduction: the upstream `run.wl` was rerun byte-identically for the retained seven-rule `sol` output on 2026-06-12. A qualified optional packet for full `b61n` runtime-lane promotion remains separate. | Yes. Implement full `b61n` complex eta-contour propagation and endpoint extraction. |
| `differential_equation_solver` | No for live AMFlow retained-golden reproduction: `run.wl` was rerun byte-identically for `redtable`/`diffeq`/`sol1`/`sol2`, and `diffeq.wl` was rerun with sub-precision numeric drift against retained Kira backup `asyexp0`/`asyexp1`/`asyexp1-fit` files. A repo-local comparator-ready manifest for the DESolver expansion files is still absent. | Yes. Implement the DESolver continuation and asymptotic-expansion workflow. |
| `feynman_prescription` | No for live AMFlow retained-golden reproduction: both opposite-prescription `sol1`/`sol2` outputs and the conjugacy deltas were rerun live on 2026-06-12. A prescription-aware comparator packet/namespacing remains separate. | Yes. Implement prescription-aware `b63n` Cutkosky runtime coverage. |
| `linear_propagator` | No for live AMFlow retained-golden reproduction: the upstream `run.wl` was rerun live on 2026-06-12 and matched the retained phase0 `sol` byte-for-byte. A future high-precision `b64ag` runtime packet remains separate. | Yes. Implement full `b64ag` gauge-link transport and finite-part extraction. |
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
