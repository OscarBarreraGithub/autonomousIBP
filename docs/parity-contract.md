# Parity Contract Freeze

This document freezes the AMFlow 1.2 runtime surface that the C++ port must match or intentionally supersede.

## Sources

- `references/snapshots/amflow/README.md`
- `references/snapshots/amflow/FAQ.md`
- `references/snapshots/amflow/CHANGELOG.md`
- `references/snapshots/amflow/options_summary.txt`
- `references/snapshots/amflow/kira-interface.m`

## Required Public Features

- loop-integral evaluation
- phase-space integration support
- linear-propagator support
- complex kinematics
- arbitrary `D0` support via `D = D0 - 2 eps`
- fixed-`eps` numeric mode
- multiple top-sector workflows
- standalone DE solver
- manual and automatic solution workflows
- cache/restart semantics

## Frozen Option Surface

### `SetAMFOptions`

- `AMFMode`
- `EndingScheme`
- `D0`
- `WorkingPre`
- `ChopPre`
- `XOrder`
- `ExtraXOrder`
- `LearnXOrder`
- `TestXOrder`
- `RationalizePre`
- `RunLength`
- `UseCache`
- `SkipReduction`

### `SetReductionOptions`

- `IBPReducer`
- `BlackBoxRank`
- `BlackBoxDot`
- `ComplexMode`
- `DeleteBlackBoxDirectory`

### `SetReducerOptions` for `Kira`

- `IntegralOrder`
- `ReductionMode`
- `PermutationOption`
- `MasterRank`
- `MasterDot`

## Frozen Example Classes

- `automatic_loop`
- `automatic_phasespace`
- `automatic_vs_manual`
- `complex_kinematics`
- `differential_equation_solver`
- `feynman_prescription`
- `linear_propagator`
- `spacetime_dimension`
- `user_defined_amfmode`
- `user_defined_ending`

## Frozen Behavioral Constraints

- Kira remains an external reducer boundary in v1.
- AMFlow prefactors and sign conventions must be explicit and testable.
- Failures must be explicit; silent numerical degradation is not acceptable.
- Singular kinematics, mixed large-`eta` regions, and non-canonical DE systems are in scope from the start.
- User-defined `AMFMode` and `EndingScheme` survive as extension interfaces in C++.

## Known Upstream Regression Surfaces To Carry Forward Into Tests

- asymptotic-series overflow behavior
- incorrect `Mass`-mode insertion
- boundary-generation errors for unnormalized loop momenta
- `AnalyzeBlock` on non-block-triangular systems
- Kira interface failures with unexpected master sets
