# Index

## P0 Core AMFlow Sources

- `theory/auxiliary-mass-flow/2017-1711.09572-systematic-efficient-method.pdf`
  Original auxiliary-mass-flow paper. This is the method root.
- `theory/auxiliary-mass-flow/2020-2009.07987-amf-phase-space.pdf`
  Extends AMF to phase-space integrals and is more pedagogical than the package paper.
- `theory/auxiliary-mass-flow/2021-2107.01864-collider-processes-amf.pdf`
  Practical collider-scale AMF paper; best early benchmark source.
- `theory/auxiliary-mass-flow/2022-2201.11636-linear-propagators-amf.pdf`
  Required if the port should handle linear/eikonal propagators.
- `theory/auxiliary-mass-flow/2022-2201.11637-linear-algebra-only.pdf`
  Clarifies what depends only on linear relations, not full symbolic machinery.
- `theory/auxiliary-mass-flow/2022-2201.11669-amflow-package.pdf`
  Main AMFlow package paper.
- `theory/auxiliary-mass-flow/2024-2401.08226-singular-kinematics-amf.pdf`
  Highest-priority follow-on paper for singular limits and boundary data.
- `theory/series-solvers/2025-2501.01943-line.pdf`
  Independent C++ implementation family. Closest external comparator.

## P0 Backend Sources

- `ibp/kira/2025-2505.20197-kira3.pdf`
  Current Kira paper. Read this first if Kira is the chosen backend.
- `ibp/kira/2020-2008.06494-kira2.pdf`
  Finite-field and FireFly integration in Kira.
- `ibp/kira/2017-1705.05610-kira.pdf`
  Baseline Kira architecture.
- `ibp/fire/2019-1901.07808-fire6.pdf`
  FIRE modular-arithmetic paper.
- `ibp/fire/2023-2311.02370-fire6.5.pdf`
  FIRE simplifier abstraction and FLINT/Symbolica story.
- `ibp/litered/2012-1212.2685-litered.pdf`
  Original LiteRed capabilities.
- `ibp/litered/2013-1310.1145-litered-1.4.pdf`
  Mature LiteRed feature map.
- `ibp/blade/2024-2405.14621-blade.pdf`
  Block-triangular reduction ideas from the AMFlow group.

## P0 Theory Outside AMFlow

- `theory/differential-equations/1997-hep-th-9711188-remiddi.pdf`
- `theory/differential-equations/1999-hep-ph-9912329-gehrmann-remiddi.pdf`
- `theory/differential-equations/2013-1304.1806-henn-canonical-basis.pdf`
- `theory/differential-equations/2015-1411.0911-lee-epsilon-form.pdf`
- `theory/boundaries-and-regions/1997-hep-ph-9711391-expansion-by-regions.pdf`
- `theory/series-solvers/2018-1803.008-smirnov-lee-singular-points.pdf`
- `theory/analytic-continuation-and-singularities/2022-2205.03345-seasyde.pdf`
- `theory/precision-and-error-control/2017-1712.05173-dream.pdf`

These are the main gap-fillers for basis choice, asymptotic boundaries, local series matching, analytic continuation, and error control.

## P1 Finite-Field / Reconstruction Layer

- `finite-fields/foundations/2016-peraro-scattering-amplitudes-over-finite-fields.pdf`
- `finite-fields/finiteflow/2019-1905.08019-finiteflow.pdf`
- `finite-fields/firefly/2019-1904.00009-firefly.pdf`
- `finite-fields/firefly/2020-2004.01463-firefly-improvements.pdf`

These matter if the port grows toward its own reconstruction layer or if Kira/FireFly behavior needs to be mirrored.

## P1 Precision Layer

- `deps/numeric/`
  Official GMP, MPFR, FLINT, Boost.Multiprecision, and `rug` snapshots and links.

This is the precision substrate for a 100-digit C++ build and the reality check for a Rust path.

## P1 Case Studies

- `case-studies/selected-benchmarks.md`
  Curated downstream uses and regression targets.
- `case-studies/amflow-citations-openalex.tsv`
  Citation-landscape snapshot for AMFlow as of `2026-04-10`.

## P1 Notes

- `notes/porting-roadmap.md`
  Suggested reading order and subsystem split.
- `notes/theory-gap-audit.md`
  Missing theory that AMFlow + Kira docs alone do not cover.
