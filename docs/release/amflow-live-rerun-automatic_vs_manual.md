# AMFlow Live Rerun: automatic_vs_manual

Date: 2026-06-12

## Example

- Example: `automatic_vs_manual`
- Upstream Mathematica script: `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260419-required-set/inputs/upstream/amflow/examples/automatic_vs_manual/run.wl`
- Live scratch root: `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/live-reruns/lane6-amflow-live-rerun-automatic_vs_manual-20260612T175542Z`
- Retained original capture summary: `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260419-required-set/comparisons/phase0/automatic_vs_manual.summary.json`
- Retained promoted AMFlow golden manifest: `tools/reference-harness/specs/phase0/automatic_vs_manual.golden-manifest.json`
- Retained C++ solve-series state bundle: `tools/reference-harness/specs/phase0/automatic_vs_manual.amflow-state.json`

The upstream script was run unchanged. It evaluates the two-loop `tt` family
with `precision = 20` and `epsorder = 4`, writes the automatic
`SolveIntegrals` output to `auto`, then reruns the same target set through the
manual `GenerateNumericalConfig` plus `BlackBoxAMFlow` plus `FitEps` path and
writes `man`. The live AMFlow tree had only `ibp_interface/Kira/install.m`
patched to point at the cluster Kira/Fermat executables.

The retained required-set `automatic_vs_manual` golden was later recaptured at
`precision = 60` with `ChopPre = 40`. This live rerun therefore checks the
original upstream-script requested-precision surface and records the promoted
golden boundary explicitly.

## Version Stack

- Mathematica: `13.3.0 for Linux x86 (64-bit) (June 3, 2023)`
- AMFlow: `1.1`, release date `5-Jun-2022`
- AMFlow tree commit: `775162498ab18493c45254b861669b4151b841ee`
- Kira: `3.1`, `/n/holylabs/schwartz_lab/Lab/obarrera/toolchains/autonomousIBP/kira/installs/kira-3.1/bin/kira-3.1`
- Fermat: `5.25`, `/n/holylabs/schwartz_lab/Lab/obarrera/toolchains/autonomousIBP/fermat/5.25/ferl64/fer64`

Live scratch hashes:

```text
run.wl:    909a383ad4fff796439ec5f6a1dc2a6a7f59f178f2db3c845115f504ba5e7920
install.m: f890df0c410ba93c54915ccbfa3d5a8d4de0efd3b8daa98c0c089bbefaadce20
```

## Live Invocation

```bash
RUNROOT=/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/live-reruns/lane6-amflow-live-rerun-automatic_vs_manual-20260612T175542Z
source /etc/profile
module purge
module load mathematica/13.3.0-fasrc01
export FERMATPATH=/n/holylabs/schwartz_lab/Lab/obarrera/toolchains/autonomousIBP/fermat/5.25/ferl64/fer64
cd "$RUNROOT/amflow/examples/automatic_vs_manual"
rm -f auto man
rm -rf cache
timeout 5400s wolframscript -file "$RUNROOT/amflow/examples/automatic_vs_manual/run.wl" > "$RUNROOT/live.stdout" 2> "$RUNROOT/live.stderr"
```

Run metadata:

```text
live-start: 2026-06-12T15:30:39-04:00
live-end: 2026-06-12T15:50:48-04:00
live-exit: 0
```

## Live Output Digest

- Live `auto` raw file: `db50ab0b4943d91dd1dd10ed590afd7e6051834bd6c77cc3927a4f351660cebf`
- Live `man` raw file: `3fcc46695be98566b17dc90c993349fa3967f31eef6df3fa40b48e1ca7a26d3e`
- Live `auto` canonical file: `77c17c56e247156c394aeca098b795caf6b4dbde3e9921e1886ac1465c3384de`
- Live `man` canonical file: `eb84e216a5f18799997df038af6f96f40969cd536aa428812ee80d9c01540c71`
- Live stdout: `38ed96e7321f998520b30caee0c5633478151d10bdfdcf149571224223cb3a13`
- Live stderr: `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` (empty)

Sample stdout lines:

```text
SetReductionOptions -> {IBPReducer -> Kira, BlackBoxRank -> 3, BlackBoxDot -> 0, ComplexMode -> True, DeleteBlackBoxDirectory -> False}
SolveIntegrals: integrals will be evaluated at 14 different eps values {101/9999900, 17/1666650, 103/9999900, 26/2499975, 7/666660, 53/4999950, 107/9999900, 3/277775, 109/9999900, 11/999990, 37/3333300, 28/2499975, 113/9999900, 19/1666650}.
AMFSystemSolution: system 1 solved in 254s.
BlackBoxAMFlow: finished in 612s.
SolveIntegrals: finished in 612s.
SetAMFOptions -> {AMFMode -> {Prescription, Mass, Propagator}, EndingScheme -> {Tradition, Cutkosky, SingleMass}, D0 -> 4, WorkingPre -> 180, ChopPre -> 20, XOrder -> 360, ExtraXOrder -> 20, LearnXOrder -> -1, TestXOrder -> 5}
AMFSystemSolution: system 22 solved in 254s.
BlackBoxAMFlow: finished in 595s.
```

## Comparison

Canonicalizing the live `auto`/`man` files with the repository's
`capture_phase0_reference.py` Wolfram canonicalizer reproduced the original
retained capture hashes from
`/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260419-required-set/comparisons/phase0/automatic_vs_manual.summary.json`:

```text
auto canonical live and original capture: 77c17c56e247156c394aeca098b795caf6b4dbde3e9921e1886ac1465c3384de
man canonical live and original capture:  eb84e216a5f18799997df038af6f96f40969cd536aa428812ee80d9c01540c71
```

The live raw files are not byte-identical to the later promoted retained
required-set goldens:

```text
promoted auto raw hash: 6d453524ad2dab712cfdd3d97fc18162ca10426963796e201b32ac6ee42ba298
promoted man raw hash:  8731a2c92adaaa79a445979e8c59a1fa958811c963a3fc783440a2b8df008c12
auto raw-cmp-exit=1
man raw-cmp-exit=1
```

The correct-digit scorer against the promoted required-set goldens kept the
nonnumeric skeleton fixed for both outputs but did not meet the later 50-digit
qualification threshold:

```text
candidate_numeric_literal_skeletons_match_reference=true
auto minimum_observed_correct_digits=18
man minimum_observed_correct_digits=18
required_minimum_correct_digits=50
all_selected_benchmarks_meet_digit_thresholds=false
```

This is expected for an unchanged upstream script requesting `precision = 20`
when compared against the later promoted `precision = 60` retained text. The
existing committed C++ comparison evidence remains scoped to the retained
`auto` output because `auto` and `man` carry the same `tt` integral surface and
would otherwise duplicate integral keys. This live note does not claim new C++
runtime coverage beyond that accepted solve-series surface.

## Status

`reproduced-matches-golden-at-requested-precision`

This confirms the Mathematica 13.3 + AMFlow 1.1 + Kira 3.1 + Fermat stack can
freshly rerun the upstream `automatic_vs_manual` example and reproduce the
original retained canonical AMFlow capture for both automatic and manual
outputs. It also records that the unchanged upstream script does not reproduce
the later promoted 50-digit qualification golden byte-for-byte or at the
50-digit floor. This is a live AMFlow retained-capture reproducibility claim,
not a broader C++ runtime claim.

last-re-verified: 2026-06-12 (auto/man live canonical hashes matched original retained capture hashes 77c17c56e247156c394aeca098b795caf6b4dbde3e9921e1886ac1465c3384de/eb84e216a5f18799997df038af6f96f40969cd536aa428812ee80d9c01540c71; promoted raw-cmp-exit=1/1; promoted-golden scorer skeleton matched with minimum observed digits 18/18 below the 50-digit floor; stdout hash 38ed96e7321f998520b30caee0c5633478151d10bdfdcf149571224223cb3a13; stderr matched empty hash)
