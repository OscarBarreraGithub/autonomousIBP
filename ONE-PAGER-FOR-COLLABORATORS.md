# autoIBP

**A high-performance C++ implementation of auxiliary mass flow — reproducing AMFlow's reduction of Feynman integrals to master integrals, and extending it to physics the original cannot reach.**

## What it is

autoIBP is a from-scratch C++17 reimplementation of AMFlow, the Mathematica package that reduces multi-loop Feynman integrals to master integrals using the auxiliary mass flow method. Its goal is an independent, fast, and rigorously tested implementation that reproduces AMFlow's results and extends them to physics regimes the original does not cover. The build is complete and stable, with no outstanding release blockers.

## Current status

We track all 10 example calculations shipped with upstream AMFlow. For 9 of them, we re-ran the original AMFlow Mathematica scripts live on our cluster (Mathematica 13.3, AMFlow 1.1, Kira 3.1, Fermat 5.25) and confirmed that our reference results match the freshly computed output at the requested precision (agreement at the 50-digit level). Each example carries a dated re-verification record, kept current by an automated freshness check.

An honest boundary: "verified against live AMFlow" means our reference results faithfully reproduce live AMFlow output — a reference-fidelity claim. It does not yet mean that every example is computed end-to-end by a fully independent C++ runtime path. The depth of independent C++ reconstruction varies by example and is part of the remaining work.

The 10th example, a phase-space (Cutkosky-cut) case, is the single open item. It requires a per-spacetime-dimension decomposition that AMFlow's output does not expose directly, so it needs a genuine theory step. We deliberately did not fabricate those numbers.

## How it compares to other C++ AMFlow efforts

The most prominent public C++ port, amflow-cpp, is strong on breadth of standard cases: roughly 549 unit tests and 228 numerical oracle benchmarks spanning one-loop through four-loop, agreeing with reference values to a tolerance near 1e-30, and it is MIT-licensed. It explicitly places complex external kinematics, effective-field-theory structures (HQET/SCET), and gauge-link cases out of scope.

autoIBP is complementary in focus. Our distinguishing strength is precisely the extended physics that port excludes: complex external kinematics, Cutkosky and phase-space cut contributions (weighted residues and endpoints), and lightlike linear propagators and gauge-link (Wilson-line) structures relevant to effective field theories — together with live verification of our reference results against the original Mathematica AMFlow. Our current relative gap is breadth: we do not yet ship as large a standard one-loop-to-four-loop numerical oracle suite. This is a contrast of focus, not a claim of superiority on any single axis; the precision figures quoted for each project measure different things (requested example precision versus oracle-benchmark tolerance) and are not a direct head-to-head.

## How it was built

Development followed a disciplined, multi-stage review process. Every change was built in an isolated, disposable copy of the repository, had to pass the full build and roughly 145 automated regression checks before it could be merged, and was independently cross-reviewed. Crucially, any change that could not be honestly verified was rejected — including the refusal to fabricate numbers for the one theoretically blocked case. Reference data is pinned at the byte level and re-verified against live AMFlow on a schedule, so regressions and silent drift are caught automatically. Beyond numerical agreement, those checks also validate the provenance and internal consistency of the verification evidence itself, so a passing result means not just "the numbers match" but "we can show why we trust them."

## Next steps

1. **Close the open phase-space example** — implement the cut-contribution path and the per-dimension projection, or formally document it as out of scope with full theoretical justification.
2. **Deepen the extended-physics cases** — advance them from "matches reference output" to fully independent C++ runtime computation.
3. **Add a broad standard benchmark suite** — a one-loop-to-four-loop numerical oracle suite, so the project matches other ports on breadth while retaining its unique extended-physics coverage.
