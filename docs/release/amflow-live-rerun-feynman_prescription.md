# AMFlow Live Rerun: feynman_prescription

Date: 2026-06-12

## Example

- Example: `feynman_prescription`
- Upstream Mathematica script: `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260419-required-set/inputs/upstream/amflow/examples/feynman_prescription/run.wl`
- Live scratch script path: `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/live-reruns/lane6-amflow-live-rerun-feynman_prescription-20260612T042857Z/amflow/examples/feynman_prescription/run.wl`
- Retained full AMFlow golden manifest: `tools/reference-harness/specs/phase0/feynman_prescription.golden-manifest.json`
- Retained full AMFlow golden output directory: `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/work/goldens/phase0/feynman_prescription/captured`

The upstream example script was run unchanged. It evaluates the requested
phase-space family twice, first with `AMFlowInfo["Prescription"] = {1, -1, 0}`
and then with the opposite prescription `{-1, 1, 0}`, writing `sol1` and
`sol2`. The live AMFlow tree had only `ibp_interface/Kira/install.m` patched to
point at the cluster Kira/Fermat executables.

## Version Stack

- Mathematica: `13.3.0 for Linux x86 (64-bit) (June 3, 2023)`
- AMFlow: `1.1`, release date `5-Jun-2022`
- AMFlow tree commit: `775162498ab18493c45254b861669b4151b841ee`
- Kira: `3.1`, `/n/holylabs/schwartz_lab/Lab/obarrera/toolchains/autonomousIBP/kira/installs/kira-3.1/bin/kira-3.1`
- Fermat: `5.25`, `/n/holylabs/schwartz_lab/Lab/obarrera/toolchains/autonomousIBP/fermat/5.25/ferl64/fer64`

Live scratch hashes:

```text
run.wl:    1e3dd288e524c6f1d1d8fccc6ffc0b15a236a12177f669c6a04753c7a8770dc5
install.m: f890df0c410ba93c54915ccbfa3d5a8d4de0efd3b8daa98c0c089bbefaadce20
```

## Live Invocation

```bash
RUNROOT=/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/live-reruns/lane6-amflow-live-rerun-feynman_prescription-20260612T042857Z
source /etc/profile
module purge
module load mathematica/13.3.0-fasrc01
export FERMATPATH=/n/holylabs/schwartz_lab/Lab/obarrera/toolchains/autonomousIBP/fermat/5.25/ferl64/fer64
cd "$RUNROOT/amflow/examples/feynman_prescription"
rm -f sol1 sol2
rm -rf cache
timeout 5400s wolframscript -file "$RUNROOT/amflow/examples/feynman_prescription/run.wl" > "$RUNROOT/live.stdout" 2> "$RUNROOT/live.stderr"
```

Run metadata:

```text
live-start: 2026-06-12T00:31:15-04:00
live-end: 2026-06-12T00:52:12-04:00
live-exit: 0
```

## Live Output Digest

- Live `sol1` raw file: `7b2924b4cfd4a1f675157620540958e4b2a6c82ab12c147d7bb9fe857090160c`
- Live `sol2` raw file: `452d3c2d1de57cce7301535451f30fbe0789f69083478c20f621296e70ebe452`
- Live stdout: `c922342f1e725c63661f820fa51882807969bb1e29c113e8ed5bbece07ef9252`
- Live stderr: `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` (empty)

Sample stdout lines:

```text
SetReductionOptions -> {IBPReducer -> Kira, BlackBoxRank -> 3, BlackBoxDot -> 0, ComplexMode -> True, DeleteBlackBoxDirectory -> False}
Prescription -> {1, -1, 0}
AMFSystemSolution: system 1 solved in 210s.
BlackBoxAMFlow: finished in 518s.
SolveIntegrals: finished in 519s.
Prescription -> {-1, 1, 0}
AMFSystemSolution: system 20 solved in 406s.
BlackBoxAMFlow: finished in 734s.
SolveIntegrals: finished in 735s.
```

The upstream script leaves its final conjugacy expressions as bare Mathematica
expressions, so `wolframscript -file` did not echo final zero lines to stdout.
The same expressions were evaluated after the run over the saved `sol1`/`sol2`
outputs.

## Comparison

Compared live output against the retained full AMFlow packet:

- `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/work/goldens/phase0/feynman_prescription/captured/sol1`
- `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/work/goldens/phase0/feynman_prescription/captured/sol2`

Both raw files matched byte-for-byte:

```text
sol1 live and retained: 7b2924b4cfd4a1f675157620540958e4b2a6c82ab12c147d7bb9fe857090160c
sol2 live and retained: 452d3c2d1de57cce7301535451f30fbe0789f69083478c20f621296e70ebe452
```

Mathematica comparison output:

```text
sol1 SameQ=True LengthLive=16 LengthGolden=16
sol2 SameQ=True LengthLive=16 LengthGolden=16
conjReZero={0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
conjImZero={0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
```

Existing committed C++ comparison evidence remains scoped to the retained
`sol1` solve-series view:
`tools/reference-harness/specs/m5/comparisons/lane45/feynman_prescription.compare.json`
records 76/76 coefficients passing at 30 requested digits, with minimum digit
agreement 39. That comparator does not consume `sol2`, because the two
opposite-prescription outputs contain the same `loopxloop` integral keys and a
full comparator view needs prescription-aware output selection or namespacing.

## Status

`reproduced-fully-live`

This confirms the Mathematica 13.3 + AMFlow 1.1 + Kira 3.1 + Fermat stack can
freshly rerun the upstream `feynman_prescription` example and reproduce both
opposite-prescription retained AMFlow output files exactly. This is a live
retained-golden reproducibility claim for Mathematica+AMFlow data generation,
including the saved-output conjugacy check above. It does not claim new C++
`b63n` Cutkosky runtime execution or prescription-aware comparator
namespacing.
