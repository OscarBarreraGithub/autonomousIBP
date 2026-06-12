# AMFlow C++ Port Bootstrap

This repository now contains the bootstrap for a full C++ port of AMFlow with Mathematica removed from the production runtime path.

The current state is intentionally a first executable slice:

- the AMFlow parity contract is frozen in repo-local specs
- the public C++ interfaces and runtime boundaries are scaffolded
- Kira remains an external reduction backend boundary
- the reference-harness layout for upstream AMFlow validation is documented and scripted
- the reusable orchestration workflow is captured as a workspace-local Codex skill

This is not yet a full mathematical implementation of AMFlow. It is the foundation for Phases 0 and 1 of the migration plan, with compileable code, tests, and explicit extension points for the remaining solver work.

## Layout

- `docs/`: migration-facing documentation for parity, verification, reference harness, and public contracts
- `specs/`: machine-readable bootstrap specs and parity matrix
- `include/`, `src/`: C++17 project scaffold
- `tests/`: initial CTest-based validation
- `tools/reference-harness/`: scripts and templates for standing up the upstream AMFlow baseline
- `codex-skills/amflow-port-orchestrator/`: reusable workspace-local orchestration skill
- `references/`: curated corpus used to define and validate the port

## Release Docs

The release-facing files under `docs/release/` are part of the top-level
documentation surface:

- [AMFlow example coverage inventory](docs/release/amflow-example-coverage.md)
- [AMFlow live rerun: complex kinematics](docs/release/amflow-live-rerun-complex_kinematics.md)
- [AMFlow live rerun: differential equation solver](docs/release/amflow-live-rerun-differential_equation_solver.md)
- [AMFlow live rerun: feynman prescription](docs/release/amflow-live-rerun-feynman_prescription.md)
- [AMFlow live-rerun freshness registry](docs/release/amflow-live-rerun-freshness-registry.json)
- [AMFlow live rerun: linear propagator](docs/release/amflow-live-rerun-linear_propagator.md)
- [AMFlow live rerun: spacetime dimension](docs/release/amflow-live-rerun-spacetime_dimension.md)
- [AMFlow live rerun: user-defined AMF mode](docs/release/amflow-live-rerun-user_defined_amfmode.md)
- [AMFlow live rerun: user-defined ending](docs/release/amflow-live-rerun-user_defined_ending.md)
- [Release known gaps](docs/release/known-gaps.md)
- [M7 closure evidence](docs/release/m7-closure-evidence.md)
- [Re-run M7 release readiness](docs/release/re-run-release-readiness.md)
- [Release tooling catalog](docs/release/tools.md)

## Build

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Optional external dependencies are modeled in CMake but disabled by default so the scaffold builds on a clean machine. The production target remains:

- `GiNaC/CLN` for exact symbolic algebra
- `Boost.Multiprecision` with `MPFR`
- `yaml-cpp`
- `Kira 3.1 + Fermat` as external processes

## CLI

The bootstrap CLI exposes a few inspection commands:

```bash
./build/amflow-cli sample-problem
./build/amflow-cli emit-kira
./build/amflow-cli show-defaults
./build/amflow-cli write-manifest ./artifacts
```

## Next Implementation Targets

- replace placeholder exact-expression storage with the chosen symbolic backend
- replace stub solver/continuation implementations with the real AMFlow algorithms
- wire the reference harness to a pinned upstream AMFlow install and frozen CPC examples
- graduate the parity matrix from frozen contract to enforced qualification suite
