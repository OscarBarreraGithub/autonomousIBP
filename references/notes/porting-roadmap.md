# Porting Roadmap

## Recommended Baseline

For a first serious rewrite, the cleanest target is:

- orchestration language: `C++`
- IBP backend: `Kira 3`
- precision layer: `MPFR` through either direct bindings or `Boost.Multiprecision`
- optional reconstruction layer: leave to Kira/FireFly first, own later

This is the lowest-risk path away from Mathematica while keeping the strongest external solver.

## Read In This Order

1. AMFlow method papers: `1711.09572`, `2009.07987`, `2107.01864`, `2201.11669`.
2. Kira stack: `1705.05610`, `2008.06494`, `2505.20197`, plus Kira README/docs snapshot.
3. DE foundations: Remiddi, Gehrmann-Remiddi, Henn, Lee.
4. Series / continuation: DiffExp, SeaSyde, LINE.
5. Boundary and singular-limit material: expansion by regions, singular kinematics AMF.
6. Precision discipline: DREAM, MPFR/FLINT docs.
7. Case studies and regression targets.

## Subsystems To Design

- reduction adapter
  Input family definition, kinematics, integral targets, and Kira job generation.
- `eta`-flow core
  Differential-equation construction, system decomposition, path planning, boundary propagation.
- local solver
  Series expansions, patch matching, singular-point handling, residual checks.
- precision manager
  Dynamic precision escalation, cancellation detection, truncation estimates.
- validation layer
  Cross-checks against DE residuals, symmetry relations, known analytic points, and AMFlow/LINE outputs.

## What To Avoid In V1

- reimplementing a full IBP reducer
- building a custom finite-field reconstruction engine before the flow solver is stable
- assuming canonical bases are always available
- assuming Euclidean-point behavior extrapolates safely to thresholds

## Mandatory Regression Families

- AMFlow double-box example from the package paper
- planar `ttbar j`
- `ttbar H` light-quark-loop master integrals
- five-point one-mass scattering
- `ttbar W`
- at least one singular-kinematics case from `2401.08226`

## Rust Note

Rust remains viable for orchestration and high-level safety, but the mature arbitrary-precision and reduction ecosystem is still more C++-native. A realistic Rust path today still leans on `rug`/`gmp-mpfr-sys` or C/C++ FFI.
