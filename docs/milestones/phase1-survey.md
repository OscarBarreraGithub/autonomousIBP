# Phase-1 / Beyond-Phase-0 Survey

## Scope

This is a design-only survey for the next coverage horizon after the closed M5
work and the still-blocked M6 qualification work. It does not modify runtime
behavior and does not claim new parity, M6 closure, M7 closure, or release
readiness.

The roadmap uses "phase-1" most narrowly for loop-core parity evidence: the port
must show that it constructs the right DE systems and Kira reduction spans on
mandatory loop-core families (`docs/full-amflow-completion-roadmap.md:293-295`).
The wider post-phase-0 capture horizon is the `M0b` rolling harness widening from
the required phase-0 captures to the remaining frozen example classes, benchmark
families, and upstream regression surfaces before any later reference-match claim
(`docs/full-amflow-completion-roadmap.md:458-460`).

Important terminology correction: the durable accepted `M0b` phase-0 capture set
is `automatic_vs_manual` and `automatic_loop`, not the six-feature list in the
lane prompt (`docs/full-amflow-completion-roadmap.md:74-78`,
`docs/verification-strategy.md:271-274`). The prompt's six
(`automatic_loop`, `automatic_phasespace`, `complex_kinematics`,
`differential_equation_solver`, `linear_propagator`, `feynman_prescription`) are
mostly M5/Phase-F feature-surface examples. The full M5 frozen example surface is
ten rows: those six plus `automatic_vs_manual`, `spacetime_dimension`,
`user_defined_amfmode`, and `user_defined_ending`
(`docs/full-amflow-completion-roadmap.md:654`,
`docs/milestones/m5-m6-closure-plan.md:63-69`).

## Status Terms

- `CAPTURED`: a retained upstream AMFlow output, promoted golden, accepted exact
  golden, or current qualification sidecar exists for the row's current evidence
  contract.
- `NOT-CAPTURED`: the row is cataloged or scaffolded, but no retained packet or
  accepted current-contract golden exists.
- `INGEST-IMPLEMENTED`: current C++/harness tooling can consume the retained
  state or sidecar used by that row's current evidence contract.
- `NOT-IMPLEMENTED`: no current C++/harness ingest state exists for the row.
- `COMPARED`: a comparator summary exists for the row's current evidence
  contract. A failed comparison is still `COMPARED`; the note records whether it
  passes.
- `NOT-COMPARED`: no comparator summary exists.

Proxy evidence is called out explicitly. A proxy can qualify the current
machine-readable row, but it is not a dedicated literature-packet capture.

## Frozen Example Rows Beyond The Prompt's Six

| Row | Capture | Ingest | Compare | Current evidence |
| --- | --- | --- | --- | --- |
| `automatic_vs_manual` | CAPTURED | INGEST-IMPLEMENTED | COMPARED | Required `M0b` retained capture; C++ state ingest is supported; M5 comparison passes `89/89` coefficients (`docs/verification-strategy.md:271-274`, `docs/milestones/m5-m6-closure-plan.md:91-102`). |
| `spacetime_dimension` | CAPTURED | INGEST-IMPLEMENTED | COMPARED | Optional `de-d0-pair` capture exists; lane50 uses a live solver-path direct D flag probe and passes `2/2` coefficients (`docs/verification-strategy.md:275-279`, `docs/milestones/m5-m6-closure-plan.md:100`). |
| `user_defined_amfmode` | CAPTURED | INGEST-IMPLEMENTED | COMPARED | Optional `user-hook-pair` capture exists; lane50 scoped retained-state evidence passes `6/6` coefficients (`docs/verification-strategy.md:280-284`, `docs/milestones/m5-m6-closure-plan.md:101`). |
| `user_defined_ending` | CAPTURED | INGEST-IMPLEMENTED | COMPARED | Optional `user-hook-pair` capture exists; lane50 scoped retained-state evidence passes `6/6` coefficients (`docs/verification-strategy.md:280-284`, `docs/milestones/m5-m6-closure-plan.md:102`). |

Fixed-`eps` workflow coverage and multiple-top-sector coverage are also named in
the runtime-feature and mandatory-gate lists, but they are not standalone
benchmark IDs in the selected benchmark scaffold
(`docs/full-amflow-completion-roadmap.md:839-849`).

## Qualification Rows Beyond Phase-0

The roadmap's later qualification benchmark gates are: package double box,
planar `ttbar j`, `ttbar H`, five-point one-mass scattering, `ttbar W`,
diphoton heavy-quark form factors, `h -> bb`, `N=4` SYM three-loop form factor,
single-top planar/nonplanar, and at least one singular-endpoint case
(`docs/full-amflow-completion-roadmap.md:856-867`). The frozen scaffold maps
those rows to selected literature anchors where available
(`references/case-studies/selected-benchmarks.md:39-63`,
`tools/reference-harness/templates/qualification-benchmarks.json:161-267`).

| Row | Capture | Ingest | Compare | Current evidence |
| --- | --- | --- | --- | --- |
| `package-double-box` | CAPTURED | INGEST-IMPLEMENTED | COMPARED | Internal matrix anchor using retained `automatic_loop` eps2 state; comparison passes `54/54` at 50 digits (`tools/reference-harness/specs/case-studies/package-double-box.numeric-evidence.json:3-25`). |
| `ttbar-j` | CAPTURED | INGEST-IMPLEMENTED | COMPARED | Current row uses a retained `automatic_vs_manual` tt-family precision-cache state against a precision-60 AMFlow output; comparison passes `89/89` at 50 digits (`tools/reference-harness/specs/case-studies/ttbar-j.numeric-evidence.json:3-33`). |
| `ttbar-h` | CAPTURED | INGEST-IMPLEMENTED | COMPARED | Current row is proxy evidence: an SRL exact endpoint comparison at the 100-digit profile, not a dedicated `2024-tth-light-quark-loop-mi` packet. |
| `five-point-one-mass-scattering` | CAPTURED | INGEST-IMPLEMENTED | COMPARED | Current row is proxy evidence through retained `complex_kinematics`; comparison passes `14/14` at 50 digits, but no dedicated five-point literature packet is frozen (`tools/reference-harness/specs/case-studies/five-point-one-mass-scattering.numeric-evidence.json:3-32`). |
| `ttbar-w` | CAPTURED | INGEST-IMPLEMENTED | COMPARED | Current row is proxy evidence through retained `automatic_loop` eps11; comparison passes at the default profile, but no dedicated `ttbar-W` packet is frozen. |
| `diphoton-heavy-quark-form-factors` | NOT-CAPTURED | NOT-IMPLEMENTED | COMPARED | The current sidecar is a failed proxy scaffold: no dedicated diphoton AMFlow packet, no C++ diphoton state, and the automatic-loop proxy reaches only 56 digits against the 200-digit profile (`tools/reference-harness/specs/case-studies/diphoton-heavy-quark-form-factors.numeric-evidence.json:3-40`). |
| `h-to-bb` | CAPTURED | INGEST-IMPLEMENTED | COMPARED | Current row is proxy evidence through retained `automatic_loop` eps9; comparison passes at the default profile, but no dedicated `h -> bb` packet is frozen. |
| `n4-sym-three-loop-form-factor` | CAPTURED | INGEST-IMPLEMENTED | COMPARED | Current row is proxy evidence through retained `automatic_loop` eps10; comparison passes at the default profile, but no dedicated N=4 packet is frozen. |
| `single-top-planar-nonplanar` | CAPTURED | INGEST-IMPLEMENTED | COMPARED | Current row is proxy evidence through retained `automatic_loop` eps12; comparison passes at the default profile, but no dedicated single-top packet is frozen. |
| `one-singular-endpoint-case` | CAPTURED | INGEST-IMPLEMENTED | COMPARED | Internal SRL-5 singular endpoint row with accepted exact golden; comparison passes `1/1` with 999 observed digits (`tools/reference-harness/specs/case-studies/one-singular-endpoint-case.numeric-evidence.json:3-45`). |

## Additional Selected Candidates

`references/case-studies/selected-benchmarks.md` also records direct or
clear-build-on candidates that are not currently frozen as qualification scaffold
rows: `2023-gg-to-gammagamma-light-quark-mi`,
`2024-higgs-gluon-form-factor-three-scales`,
`2024-gg-to-tth-one-loop-oeps2`, `2024-wpair-planar-mi`, and
`2023-jpsi-etac-bfactories` (`references/case-studies/selected-benchmarks.md:9-30`).
It also records strong adjacent candidates:
`2024-box-integrals-fermion-bubbles` and `2025-moller-ew-double-box`
(`references/case-studies/selected-benchmarks.md:32-37`).

Current status for all seven candidate rows is:

| Rows | Capture | Ingest | Compare |
| --- | --- | --- | --- |
| Non-scaffold direct and adjacent candidates listed above | NOT-CAPTURED | NOT-IMPLEMENTED | NOT-COMPARED |

These are tracking candidates, not the shortest route to the next closure
horizon. They become useful after the frozen qualification surface is no longer
blocked.

## Shortest Phase-1 Starter Chain

The shortest useful next chain is to turn
`diphoton-heavy-quark-form-factors` from a failed proxy scaffold into a dedicated
case-study packet:

1. Complete post-processing for the dedicated AMFlow capture of the selected
   diphoton heavy-quark form-factor family at the frozen 200-digit profile. The
   external lane80 SLURM job `10204166` produced a solution and metadata, as
   recorded in
   `tools/reference-harness/specs/case-studies/diphoton-heavy-quark-form-factors.lane102-final-status.json`,
   but the verifier failed on the 74-rule AMFlow output and no dedicated output
   is committed or paired with a C++ diphoton state.
2. Promote the dedicated AMFlow output into a retained golden manifest with
   canonical output hashes, run metadata, selected family/target metadata, and
   the exact tolerance profile.
3. Extract or author the C++ ingest state for the same diphoton family and
   target list. This is the current hard blocker: the existing sidecar states
   that no C++ runtime state exists for the diphoton non-planar double-box
   master-integral family J39-J42.
4. Run the existing solve/compare path against that dedicated state at the
   200-digit threshold. Do not reuse the failed automatic-loop eps16 proxy as a
   closure substitute.
5. Update the diphoton numeric sidecar, then rerun the case-study numeric
   summary, case-study-family qualifier, and M6 composer. Claim only the gates
   whose fail-closed summaries pass.

This chain is shorter than opening a new unscaffolded candidate because the row,
threshold, selected anchor, failed proxy evidence, run scripts, and missing
artifacts are already named. It is also higher leverage than adding another
default-50-digit proxy row because the diphoton row is the remaining explicit
200-digit gap in the frozen selected benchmark surface.

## Non-Claims

- No `src/` files are touched by this survey.
- No build or runtime capture is required for this design-only lane.
- Existing proxy evidence is preserved as current-contract evidence, not
  re-labeled as dedicated literature capture.
- M6, M7, release readiness, and any broader AMFlow parity claim remain outside
  this document.
