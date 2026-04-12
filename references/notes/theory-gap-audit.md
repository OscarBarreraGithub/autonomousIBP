# Theory Gap Audit

AMFlow, LINE, and IBP package docs are not sufficient by themselves for a robust rewrite. The missing areas below are now explicitly represented in this corpus.

## Gaps That Needed Filling

- boundary conditions at `eta -> infinity`
  AMFlow uses them, but the clean theory connection is expansion by regions.
- singular kinematics
  The package paper is not enough for thresholds, pseudo-thresholds, or degenerate limits.
- basis choice and conditioning
  Algebraically valid bases can still be numerically poor for flow evolution.
- analytic continuation and pinch structure
  Physical-region continuation needs more than a generic “follow a path” description.
- local series matching
  Practical solvers need patching rules near singular points and resonant exponents.
- arbitrary-precision error accounting
  “Use 100 digits” is not a stability argument by itself.
- non-canonical and elliptic sectors
  Canonical-basis intuition does not cover the full target space.

## Files That Close Those Gaps

- `theory/boundaries-and-regions/1997-hep-ph-9711391-expansion-by-regions.pdf`
- `theory/auxiliary-mass-flow/2024-2401.08226-singular-kinematics-amf.pdf`
- `theory/differential-equations/2013-1304.1806-henn-canonical-basis.pdf`
- `theory/differential-equations/2015-1411.0911-lee-epsilon-form.pdf`
- `theory/series-solvers/2018-1803.008-smirnov-lee-singular-points.pdf`
- `theory/analytic-continuation-and-singularities/2022-2205.03345-seasyde.pdf`
- `theory/precision-and-error-control/2017-1712.05173-dream.pdf`
- `theory/series-solvers/2025-2501.01943-line.pdf`

## Remaining Soft Spots

- a clean six-point multiscale physical-region benchmark set is still thinner than the five-point literature
- publicly documented high-precision error budgets remain sparse
- contour-routing theory is still more scattered than the DE/IBP literature

Those are now visible gaps instead of hidden ones.
