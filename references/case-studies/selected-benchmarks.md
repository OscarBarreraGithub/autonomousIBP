# Selected Benchmarks

## Direct AMFlow Use Or Clear Build-On

- `2023-ttj-planar-topology`
  Two-loop planar `pp -> ttbar j`; uses AMFlow for boundary values in a DE workflow.
- `2023-single-top-planar-nonplanar-topologies`
  Uses auxiliary-mass-flow numerics and cross-checks against AMFlow and pySecDec.
- `2023-gg-to-gammagamma-light-quark-mi`
  Uses AMFlow for numerical validation of analytic master integrals.
- `2023-diphoton-heavy-quark-form-factors`
  Reports agreement up to `200` digits. Strong precision benchmark.
- `2024-five-point-one-mass-scattering`
  Uses AMFlow near a singular surface. Strong physical-region stress test.
- `2024-tth-light-quark-loop-mi`
  Uses AMFlow for `100`-digit physical-region values.
- `2024-higgs-gluon-form-factor-three-scales`
  Uses AMFlow over thousands of kinematic points beyond series-convergence regions.
- `2024-gg-to-tth-one-loop-oeps2`
  Enhanced AMFlow implementation at amplitude level.
- `2024-h-to-bb-nnlo`
  High-precision reconstruction and validation use.
- `2024-n4-sym-three-loop-form-factor`
  Very-high-precision special-point benchmark.
- `2024-wpair-planar-mi`
  Planar `qqbar -> WW`; useful near elliptic/iterative structure.
- `2025-ttw-leading-colour-integrals`
  Uses AMFlow in physical-region and elliptic-sector checks.
- `2023-jpsi-etac-bfactories`
  Explicit `Kira + AMFlow` production example.

## Strong Adjacent Benchmarks

- `2024-box-integrals-fermion-bubbles`
  Compares against AMFlow to `50` digits and provides a specialized-performance baseline.
- `2025-moller-ew-double-box`
  Strong electroweak double-box target even without confirmed explicit AMFlow use.

## Qualification Scaffold IDs

- `package-double-box`
  Internal parity-matrix anchor for the baseline package family; no dedicated literature packet is frozen yet.
- `ttbar-j`
  Current literature anchor: `2023-ttj-planar-topology`.
- `ttbar-h`
  Current preferred precision anchor: `2024-tth-light-quark-loop-mi`, which carries the stronger `100`-digit floor.
- `five-point-one-mass-scattering`
  Current literature anchor: `2024-five-point-one-mass-scattering`.
- `ttbar-w`
  Current literature anchor: `2025-ttw-leading-colour-integrals`.
- `diphoton-heavy-quark-form-factors`
  Current literature anchor: `2023-diphoton-heavy-quark-form-factors`, which carries the stronger `200`-digit floor.
- `h-to-bb`
  Current literature anchor: `2024-h-to-bb-nnlo`.
- `n4-sym-three-loop-form-factor`
  Current literature anchor: `2024-n4-sym-three-loop-form-factor`.
- `single-top-planar-nonplanar`
  Current literature anchor: `2023-single-top-planar-nonplanar-topologies`.
- `one-singular-endpoint-case`
  Internal parity-matrix guardrail anchor for the singular-surface family; no dedicated literature packet is frozen yet.

Unless a stronger profile is named above, the qualification scaffold uses the default `50`-digit
core-family floor from `docs/verification-strategy.md`.

## Regression Gaps Still Worth Tracking

- six-point multiscale physical-region examples remain thinner than five-point examples
- public elliptic hard-region checkpoints are still relatively sparse
- independent implementation overlap is still mostly `AMFlow` vs `LINE`, not many-way
