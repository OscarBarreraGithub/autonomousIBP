# AMFlow Live Rerun: user_defined_ending

Date: 2026-06-10

## Example

- Example: `user_defined_ending`
- Upstream Mathematica script: `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/live-reruns/lane6-amflow-examples-sweep-1781126308-user_defined_ending/amflow/examples/user_defined_ending/run.wl`
- Live scratch root: `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/live-reruns/lane6-amflow-examples-sweep-1781126308-user_defined_ending`
- Retained committed scoped AMFlow golden: `tools/reference-harness/specs/m5/lane50/goldens/user_defined_ending.final_usr.box1_m2_1_1_2.scoped-golden.txt`
- Retained full-output packet summary: `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260422-user-hook-pair/comparisons/phase0/user_defined_ending.summary.json`

The upstream example script was run unchanged. The live AMFlow tree had only
`ibp_interface/Kira/install.m` patched to point at the cluster Kira/Fermat
executables, and the run generated both upstream output files,
`final_Tradition` and `final_usr`.

## Version Stack

- Mathematica: `13.3.0 for Linux x86 (64-bit) (June 3, 2023)`
- AMFlow: `1.1`, `$PackageInfo = {"1.1", "5-Jun-2022"}`
- AMFlow tree commit: `775162498ab18493c45254b861669b4151b841ee`
- Kira: `3.1`, `/n/holylabs/schwartz_lab/Lab/obarrera/toolchains/autonomousIBP/kira/installs/kira-3.1/bin/kira-3.1`
- Fermat: `5.25`, `/n/holylabs/schwartz_lab/Lab/obarrera/toolchains/autonomousIBP/fermat/5.25/ferl64/fer64`

## Live Invocation

```bash
RUNROOT=/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/live-reruns/lane6-amflow-examples-sweep-1781126308-user_defined_ending
source /etc/profile
module purge
module load mathematica/13.3.0-fasrc01
export FERMATPATH=/n/holylabs/schwartz_lab/Lab/obarrera/toolchains/autonomousIBP/fermat/5.25/ferl64/fer64
cd "$RUNROOT/amflow/examples/user_defined_ending"
rm -f final_Tradition final_usr
rm -rf cache
timeout 5400s wolframscript -file "$RUNROOT/amflow/examples/user_defined_ending/run.wl" > "$RUNROOT/live.stdout" 2> "$RUNROOT/live.stderr"
```

Run metadata:

```text
live-start: 2026-06-10T17:18:59-04:00
live-end: 2026-06-10T17:21:13-04:00
live-exit: 0
```

## Live Output Digest

- Live stdout: `e6efc1fba802c172992f7e4ec99b939d2fcbc4c9bd9c5184f4bab1836ac02280`
- Live stderr: `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` (empty)
- Raw live `final_Tradition`: `6a7509776b4881aa81b94782b241f7f0af2bebfccb79e481bc2744f33118a96c`
- Raw live `final_usr`: `7fb1781a2b6a827a8ba0b34292eb61bc21816ed1479489039531df540b2db964`
- Extracted live `final_usr` scoped rule canonical `InputForm`: `64fa1cf501aead66f6c9abf5f47146248d4e64a26eec4758d00a87c19bf99035`
- Retained scoped golden canonical `InputForm`: `64fa1cf501aead66f6c9abf5f47146248d4e64a26eec4758d00a87c19bf99035`

Sample stdout lines:

```text
AMFSystemSolution: boundary integrals of system 3 should be input by hand using AMFSystemWrite[3, "Solution", sol].
AMFSystemSolution: boundary integrals of system 4 should be input by hand using AMFSystemWrite[4, "Solution", sol].
SetAMFOptions -> {AMFMode -> {Prescription, Mass, Propagator}, EndingScheme -> {usr}, D0 -> 4, WorkingPre -> 604, ChopPre -> 20, XOrder -> 302, ExtraXOrder -> 20, LearnXOrder -> -1, TestXOrder -> 5}
AMFSystemSolution: boundary integrals of system 2 should be input by hand using AMFSystemWrite[2, "Solution", sol].
AMFSystemSolution: system 1 solved in 26s.
```

Canonical scoped `final_usr` output:

```text
{j[box1, -2, 1, 1, 2] -> -43508.8323221788160591810350239026560213577729793552937452820209`50. - 20000.`50./eps^2 - 28254.6867019693427878697581983519513791568132812015280238846553`50./eps - 39906.6954953664349101884966953100498706501751562530096195214549`50.*eps - 46432.7406473369001033546548551889697086952421623446202136174167`50.*eps^2 - 38636.1298236311134102727877009871355109012672732826741766244219`50.*eps^3}
```

## Comparison

Compared live output against:

- `tools/reference-harness/specs/m5/lane50/goldens/user_defined_ending.final_usr.box1_m2_1_1_2.scoped-golden.txt`
- Existing committed comparison summary: `tools/reference-harness/specs/m5/comparisons/lane50/user_defined_ending.final_usr.scoped.compare.json`

The committed comparison summary already records 6/6 scoped `final_usr`
coefficients passing with minimum 36 digit agreement. The fresh live
Mathematica output was compared directly against the retained AMFlow golden by
extracting `j[box1,-2,1,1,2]` from the live `final_usr` file and evaluating the
rule lists in Mathematica:

```text
SameQ=True
LengthLive=1 LengthGolden=1
LiveMinusGolden=InputForm[0]
```

The retained full-output phase-0 packet summary for `user_defined_ending`
already records both `final_Tradition` and `final_usr` as present and matching
across its backup, primary, and rerun captures. The live scratch run above is a
separate upstream-script reproducibility check over the same two output names;
it does not promote a new committed comparator packet or alter accepted M7
release sidecars.

## Status

`reproduced-fully-live`

This confirms the Mathematica 13.3 + AMFlow 1.1 + Kira 3.1 + Fermat stack can
freshly rerun the upstream `user_defined_ending` example, including the built-in
`Tradition` ending workflow, the user-defined `usr` ending workflow, manual
boundary writes, and the Gamma-ratio boundary path. This status is limited to
the live AMFlow rerun and retained scoped/full-output evidence above; it does
not claim new C++ ending-scheme execution coverage.

last-re-verified: 2026-06-12 (final_Tradition/final_usr live hashes matched 6a7509776b4881aa81b94782b241f7f0af2bebfccb79e481bc2744f33118a96c/7fb1781a2b6a827a8ba0b34292eb61bc21816ed1479489039531df540b2db964 byte-for-byte; scoped canonical InputForm matched 64fa1cf501aead66f6c9abf5f47146248d4e64a26eec4758d00a87c19bf99035; Wolfram SameQ=True for scoped final_usr rule; stdout diverged to f0eb89e24978f2b267b674f56a6fe5d28b096053b4ec73a09703713fc225b25e from timing-only log deltas; stderr matched)
