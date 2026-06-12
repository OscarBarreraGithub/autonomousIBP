# AMFlow Live Rerun: complex_kinematics

Date: 2026-06-12

## Example

- Example: `complex_kinematics`
- Upstream Mathematica script: `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260419-required-set/inputs/upstream/amflow/examples/complex_kinematics/run.wl`
- Live scratch script path: `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/live-reruns/codex-amflow-live-rerun-complex_kinematics-20260612T054225Z/amflow/examples/complex_kinematics/run.wl`
- Retained full AMFlow golden manifest: `tools/reference-harness/specs/phase0/complex_kinematics.golden-manifest.json`
- Retained full AMFlow golden output directory: `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/work/goldens/phase0/complex_kinematics/captured`

The upstream example script was run unchanged. It evaluates the one-loop
complex-mass box at `m3sq -> 2 - I`, reduces the requested target integral to
seven master integrals, and writes one `sol` rule-list output. The live AMFlow
tree had only `ibp_interface/Kira/install.m` patched to point at the cluster
Kira/Fermat executables.

## Version Stack

- Mathematica: `13.3.0 for Linux x86 (64-bit) (June 3, 2023)`
- AMFlow: `1.1`, release date `5-Jun-2022`
- AMFlow tree commit: `775162498ab18493c45254b861669b4151b841ee`
- Kira: `3.1`, `/n/holylabs/schwartz_lab/Lab/obarrera/toolchains/autonomousIBP/kira/installs/kira-3.1/bin/kira-3.1`
- Fermat: `5.25`, `/n/holylabs/schwartz_lab/Lab/obarrera/toolchains/autonomousIBP/fermat/5.25/ferl64/fer64`

Live scratch hashes:

```text
run.wl:    716c8e3934492232bf7eee1bf1070d5d207644266472a8dc541689e66b89a77a
install.m: f890df0c410ba93c54915ccbfa3d5a8d4de0efd3b8daa98c0c089bbefaadce20
```

## Live Invocation

```bash
RUNROOT=/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/live-reruns/codex-amflow-live-rerun-complex_kinematics-20260612T054225Z
source /etc/profile
module purge
module load mathematica/13.3.0-fasrc01
export FERMATPATH=/n/holylabs/schwartz_lab/Lab/obarrera/toolchains/autonomousIBP/fermat/5.25/ferl64/fer64
cd "$RUNROOT/amflow/examples/complex_kinematics"
rm -f sol
rm -rf cache
timeout 5400s wolframscript -file "$RUNROOT/amflow/examples/complex_kinematics/run.wl" > "$RUNROOT/live.stdout" 2> "$RUNROOT/live.stderr"
```

Run metadata:

```text
live-start: 2026-06-12T01:44:17-04:00
live-end: 2026-06-12T01:45:26-04:00
live-exit: 0
```

## Live Output Digest

- Live `sol` raw file: `f5668551872500393407710619598aa196f08635c93d13c1aa5852aefd6e7207`
- Live stdout: `2752efd643a2572e1aa53ff75530d24426c35ef3a270081c1427b049c5d71ecd`
- Live stderr: `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` (empty)

Sample stdout lines:

```text
SetReductionOptions -> {IBPReducer -> Kira, BlackBoxRank -> 3, BlackBoxDot -> 0, ComplexMode -> True, DeleteBlackBoxDirectory -> False}
CheckDep: dependencies of current reducer:
Kira executable -> True
Fermat executable -> True
AnalyticReduction: target integrals reduced to 7 master integrals in 3s.
Numeric -> {s -> 496, t -> -39, p3sq -> 7/2, p4sq -> 8, m3sq -> 2 - I}
BlackBoxAMFlow: amflow systems constructed in 35s.
AMFSystemSolution: system 1 solved in 26s.
BlackBoxAMFlow: finished in 66s.
SolveIntegrals: finished in 66s.
```

## Comparison

Compared live output against the retained full AMFlow packet:

- `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/work/goldens/phase0/complex_kinematics/captured/sol`

The raw file matched byte-for-byte:

```text
sol live and retained: f5668551872500393407710619598aa196f08635c93d13c1aa5852aefd6e7207
```

Mathematica comparison output:

```text
SameQ=True
LengthLive=7 LengthGolden=7
KeysLive={j[box, 0, 0, 0, 1], j[box, 0, 0, 1, 1], j[box, 0, 1, 0, 1], j[box, 1, 0, 0, 1], j[box, 1, 0, 1, 0], j[box, 1, 0, 1, 1], j[box, 1, 1, 1, 1]}
```

Existing committed C++ comparison evidence remains scoped to retained or
selected-coefficient surfaces: M5 lane45 records 14/14 coefficients passing
against `tools/reference-harness/specs/phase0/complex_kinematics.golden-manifest.json`
with minimum digit agreement 41, and M6 lane142 records 20/20 selected endpoint
coefficients passing at 30 requested digits with minimum digit agreement 54.
Those surfaces do not close the full `b61n` runtime lane.

## Status

`reproduced-fully-live`

This confirms the Mathematica 13.3 + AMFlow 1.1 + Kira 3.1 + Fermat stack can
freshly rerun the upstream `complex_kinematics` example and reproduce the
retained full AMFlow seven-rule `sol` output exactly. This is a live retained
AMFlow reproducibility claim for Mathematica+AMFlow data generation. It does
not claim new C++ full complex eta-contour propagation, eta=0 endpoint
extraction, optional phase-0 packet qualification, or `b61n` runtime-lane
closure.

last-re-verified: 2026-06-12 (sol live and retained hashes matched f5668551872500393407710619598aa196f08635c93d13c1aa5852aefd6e7207 byte-for-byte; Wolfram SameQ=True for 7 rules; stdout diverged to dfc93dab3651a3a5bd7a89e7bf1798c7594234d79d8c4256462e65ec8bb2205f from timing-only log deltas; stderr matched)
