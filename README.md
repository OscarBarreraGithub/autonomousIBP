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
