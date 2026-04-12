# AMFlow Port Reference Corpus

This folder is a curated source pack for a full AMFlow reimplementation in C++ or Rust, with Mathematica removed from the runtime path.

The current working assumption is:

- preferred reduction backend: `Kira 3`
- preferred implementation language: `C++`
- required numeric capability: well beyond `double`, including `~100` decimal digits when needed

This corpus is organized for implementation, not just citation. It mixes papers, repo docs, package snapshots, and benchmark/use-case material.

## How To Use This Folder

Start with these files:

1. [`INDEX.md`](./INDEX.md): human-readable map of the corpus.
2. [`manifest.csv`](./manifest.csv): sortable machine-readable inventory.
3. [`notes/porting-roadmap.md`](./notes/porting-roadmap.md): implementation-facing reading order.
4. [`notes/theory-gap-audit.md`](./notes/theory-gap-audit.md): theory topics that AMFlow papers alone do not cover.

## Priority Tags

- `P0`: read before designing architecture.
- `P1`: strong supporting material.
- `P2`: benchmark or adjacent material.

## Layout

- `theory/`: AMFlow foundations, DE method, regions, singularities, series solvers, precision.
- `ibp/`: Kira, FIRE, LiteRed, Blade references.
- `finite-fields/`: rational reconstruction and finite-field infrastructure.
- `deps/`: arbitrary-precision and CAS dependencies.
- `case-studies/`: downstream use papers and benchmark notes.
- `notes/`: cross-cutting implementation notes and theory coverage.

## Acquisition Notes

- Open-access papers are downloaded locally where possible.
- Living codebase docs are saved as snapshots plus linked in the manifest.
- AMFlow citation/use-case coverage includes a generated OpenAlex snapshot dated `2026-04-10`.

## Intended Outcome

After this corpus is loaded, the porting problem should be constrained enough to answer:

- how to couple to Kira cleanly
- how to replace Mathematica-side orchestration
- how to design the `eta`-flow solver and its series engine
- how to handle singular kinematics and contour/branch-cut issues
- how to support arbitrary precision without hand-waving
