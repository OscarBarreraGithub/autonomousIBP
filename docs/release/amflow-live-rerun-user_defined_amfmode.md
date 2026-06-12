# AMFlow Live Rerun: user_defined_amfmode

Date: 2026-06-10

## Example

- Example: `user_defined_amfmode`
- Upstream Mathematica script: `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260422-user-hook-pair/inputs/upstream/amflow/examples/user_defined_amfmode/run.wl`
- Live scratch script path: `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/live-reruns/lane6-amflow-examples-sweep-3988578/user_defined_amfmode/amflow/examples/user_defined_amfmode/run.wl`
- Retained committed AMFlow golden: `tools/reference-harness/specs/m5/lane50/goldens/user_defined_amfmode.box1_m2_1_1_2.scoped-golden.txt`

`spacetime_dimension` was inspected first, but its committed `tools/reference-harness/specs/` golden is the narrower direct-D flag probe rather than the full upstream `sol13D`/`sol73D` output surface. `user_defined_amfmode` was therefore the easiest tractable example with a committed retained golden that directly lines up with the upstream Mathematica output.

## Version Stack

- Mathematica: `13.3.0 for Linux x86 (64-bit) (June 3, 2023)`
- AMFlow: `1.1`, `$PackageInfo = {"1.1", "5-Jun-2022"}`
- Kira: `3.1`, `/n/holylabs/schwartz_lab/Lab/obarrera/toolchains/autonomousIBP/kira/installs/kira-3.1/bin/kira-3.1`
- Fermat: `5.25`, `/n/holylabs/schwartz_lab/Lab/obarrera/toolchains/autonomousIBP/fermat/5.25/ferl64/fer64`

The live scratch AMFlow tree had only `ibp_interface/Kira/install.m` patched to point at the cluster Kira/Fermat executables above. The upstream example script itself was run unchanged.

## Live Invocation

```bash
RUNROOT=/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/live-reruns/lane6-amflow-examples-sweep-3988578/user_defined_amfmode
source /etc/profile
module purge
module load mathematica/13.3.0-fasrc01
export FERMATPATH=/n/holylabs/schwartz_lab/Lab/obarrera/toolchains/autonomousIBP/fermat/5.25/ferl64/fer64
cd "$RUNROOT/amflow/examples/user_defined_amfmode"
rm -f sol
rm -rf cache
timeout 5400s wolframscript -file "$RUNROOT/amflow/examples/user_defined_amfmode/run.wl" > "$RUNROOT/live.stdout" 2> "$RUNROOT/live.stderr"
```

Run metadata:

```text
live-start: 2026-06-10T16:29:59-04:00
live-end: 2026-06-10T16:31:29-04:00
live-exit: 0
```

## Live Output Digest

- Full live `sol`: `bbb03a1ff78f37aabe1dcb3d505d8958c90311814d983baa46ab02665e5967a8`
- Extracted live scoped rule, raw Mathematica formatting: `444ef847ec8217f6a82a2ee3913a588720f47e4bc49937a2609de3dd4062e16c`
- Live scoped rule canonical `InputForm`: `64fa1cf501aead66f6c9abf5f47146248d4e64a26eec4758d00a87c19bf99035`
- Retained scoped golden canonical `InputForm`: `64fa1cf501aead66f6c9abf5f47146248d4e64a26eec4758d00a87c19bf99035`
- Live stdout: `609117fe6944e37d85992e4a384efce0d612137c5f758e860dbab2519684fa95`
- Live stderr: `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` (empty)

Sample stdout lines:

```text
SetReductionOptions -> {IBPReducer -> Kira, BlackBoxRank -> 3, BlackBoxDot -> 0, ComplexMode -> True, DeleteBlackBoxDirectory -> False}
CheckDep: dependencies of current reducer:
Kira executable -> True
Fermat executable -> True
SetAMFOptions -> {AMFMode -> {usr}, EndingScheme -> {Tradition, Cutkosky, SingleMass}, D0 -> 4, WorkingPre -> 100, ChopPre -> 40, XOrder -> 100, ExtraXOrder -> 20, LearnXOrder -> -1, TestXOrder -> 5}
BlackBoxAMFlow: finished in 87s.
SolveIntegrals: finished in 87s.
```

Canonical scoped output:

```text
{j[box1, -2, 1, 1, 2] -> -43508.8323221788160591810350239026560213577729793552937452820209`50. - 20000.`50./eps^2 - 28254.6867019693427878697581983519513791568132812015280238846553`50./eps - 39906.6954953664349101884966953100498706501751562530096195214549`50.*eps - 46432.7406473369001033546548551889697086952421623446202136174167`50.*eps^2 - 38636.1298236311134102727877009871355109012672732826741766244219`50.*eps^3}
```

## Comparison

Compared live output against:

- `tools/reference-harness/specs/m5/lane50/goldens/user_defined_amfmode.box1_m2_1_1_2.scoped-golden.txt`
- Existing committed comparison summary: `tools/reference-harness/specs/m5/comparisons/lane50/user_defined_amfmode.scoped.compare.json`

The committed comparison summary already records 6/6 coefficients passing with minimum 42 digit agreement. The fresh live Mathematica output was compared directly against the retained AMFlow golden by extracting `j[box1,-2,1,1,2]` from the live `sol` file and evaluating both rule lists in Mathematica:

```text
SameQ=True
LengthLive=1 LengthGolden=1
LiveMinusGolden=InputForm[0]
```

The raw live scoped file and raw retained golden have different sha256 values because Mathematica wrapped the freshly extracted rule across lines. Their canonical `InputForm` text hashes are identical.

## Status

`reproduced-matches-golden`

This confirms the Mathematica 13.3 + AMFlow 1.1 + Kira 3.1 + Fermat stack can freshly rerun the upstream `user_defined_amfmode` example and reproduce the committed retained AMFlow scoped golden exactly. This status is limited to the retained scoped golden surface above and does not claim new C++ eta=0 endpoint-runtime coverage.

last-re-verified: 2026-06-12 (scoped canonical InputForm matched 64fa1cf501aead66f6c9abf5f47146248d4e64a26eec4758d00a87c19bf99035; full sol matched bbb03a1ff78f37aabe1dcb3d505d8958c90311814d983baa46ab02665e5967a8; stdout diverged to ac3dd45b707ccfd97d1586c712c92cad6ce3e59181ee9e36de10b24d33f9fc9a from timing-only log deltas; stderr matched)
