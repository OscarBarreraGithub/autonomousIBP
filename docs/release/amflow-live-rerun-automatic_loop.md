# AMFlow Live Rerun: automatic_loop

Date: 2026-06-12

## Example

- Example: `automatic_loop`
- Upstream Mathematica script: `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260419-required-set/inputs/upstream/amflow/examples/automatic_loop/run.wl`
- Live scratch root: `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/live-reruns/lane6-amflow-live-rerun-automatic_loop-20260612T175542Z`
- Retained original capture summary: `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260419-required-set/comparisons/phase0/automatic_loop.summary.json`
- Retained promoted AMFlow golden manifest: `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260419-required-set/goldens/phase0/automatic_loop/golden-manifest.json`
- Retained C++ solve-series state bundle: `tools/reference-harness/specs/phase0/automatic_loop.amflow-state.json`

The upstream script was run unchanged. It evaluates the `box1` and `box2`
one-loop families with `precision = 40` and `epsorder = 3`, then writes `sol1`
and `sol2`. The live AMFlow tree had only `ibp_interface/Kira/install.m`
patched to point at the cluster Kira/Fermat executables.

The retained required-set `automatic_loop` golden was later precision-promoted
to `precision = 80` and `ChopPre = 40`. This live rerun therefore checks the
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
run.wl:    9b87cc5511b280a6569d13a4fe22f6011bae97e1dc620c6d89dcf7c6020538a7
install.m: f890df0c410ba93c54915ccbfa3d5a8d4de0efd3b8daa98c0c089bbefaadce20
```

## Live Invocation

```bash
RUNROOT=/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/live-reruns/lane6-amflow-live-rerun-automatic_loop-20260612T175542Z
source /etc/profile
module purge
module load mathematica/13.3.0-fasrc01
export FERMATPATH=/n/holylabs/schwartz_lab/Lab/obarrera/toolchains/autonomousIBP/fermat/5.25/ferl64/fer64
cd "$RUNROOT/amflow/examples/automatic_loop"
rm -f sol1 sol2
rm -rf cache
timeout 5400s wolframscript -file "$RUNROOT/amflow/examples/automatic_loop/run.wl" > "$RUNROOT/live.stdout" 2> "$RUNROOT/live.stderr"
```

Run metadata:

```text
live-start: 2026-06-12T15:27:23-04:00
live-end: 2026-06-12T15:29:39-04:00
live-exit: 0
```

## Live Output Digest

- Live `sol1` raw file: `62279f48a2d40020430fba6cbea45372d5619a0325f8fa0634b638ce01bb17d7`
- Live `sol2` raw file: `e2e8ef1c0a3c961b31d8a134b09b5a9dbcb5d50a366ee3c9fc068760273afe27`
- Live `sol1` canonical file: `12a186ff4e0cd5eefc83d2ee75d011f5b660f6d71952e251a3731ef9b9301f53`
- Live `sol2` canonical file: `2a2a0117b3d15452ffff2cad94497147c0b696b994e84372b7efe44eaa694516`
- Live stdout: `1bd1ef3e78f912cda3c198b0642711b06481c40e2a601156a5eca524fddb36f8`
- Live stderr: `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` (empty)

Sample stdout lines:

```text
SetReductionOptions -> {IBPReducer -> Kira, BlackBoxRank -> 3, BlackBoxDot -> 0, ComplexMode -> True, DeleteBlackBoxDirectory -> False}
SolveIntegrals: integrals will be evaluated at 10 different eps values {101/3162277660100, 51/1581138830050, 103/3162277660100, 2/60813031925, 21/632455532020, 53/1581138830050, 107/3162277660100, 27/790569415025, 109/3162277660100, 11/316227766010}.
AMFSystemSolution: system 1 solved in 23s.
BlackBoxAMFlow: finished in 67s.
SolveIntegrals: finished in 67s.
AMFSystemSolution: system 1 solved in 23s.
BlackBoxAMFlow: finished in 67s.
SolveIntegrals: finished in 67s.
```

## Comparison

Canonicalizing the live `sol1`/`sol2` files with the repository's
`capture_phase0_reference.py` Wolfram canonicalizer reproduced the original
retained capture hashes from
`/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260419-required-set/comparisons/phase0/automatic_loop.summary.json`:

```text
sol1 canonical live and original capture: 12a186ff4e0cd5eefc83d2ee75d011f5b660f6d71952e251a3731ef9b9301f53
sol2 canonical live and original capture: 2a2a0117b3d15452ffff2cad94497147c0b696b994e84372b7efe44eaa694516
```

The live raw files are not byte-identical to the later promoted retained
required-set goldens:

```text
promoted sol1 raw hash: f13af62dd11019ecf093079f9b3e3c9a4515b40044e42e3aa53f5182541f685c
promoted sol2 raw hash: 2f9b5c6f3cb4e6da09dfa64a823f6094365d2ee2730304f63aec93205b61c731
sol1 raw-cmp-exit=1
sol2 raw-cmp-exit=1
```

The correct-digit scorer against the promoted required-set goldens kept the
nonnumeric skeleton fixed for both outputs but did not meet the later 50-digit
qualification threshold:

```text
candidate_numeric_literal_skeletons_match_reference=true
sol1 minimum_observed_correct_digits=38
sol2 minimum_observed_correct_digits=38
required_minimum_correct_digits=50
all_selected_benchmarks_meet_digit_thresholds=false
```

This is expected for an unchanged upstream script requesting `precision = 40`
when compared against the later promoted `precision = 80` retained text. The
existing committed C++ comparison evidence remains the accepted solve-series
surface, including M5 lane39/lane45 `automatic_loop.eps8` evidence over the
retained state bundle. This live note does not claim new C++ runtime coverage
beyond that accepted solve-series surface.

## Status

`reproduced-matches-golden-at-requested-precision`

This confirms the Mathematica 13.3 + AMFlow 1.1 + Kira 3.1 + Fermat stack can
freshly rerun the upstream `automatic_loop` example and reproduce the original
retained canonical AMFlow capture for `sol1` and `sol2`. It also records that
the unchanged upstream script does not reproduce the later promoted 50-digit
qualification golden byte-for-byte or at the 50-digit floor. This is a live
AMFlow retained-capture reproducibility claim, not a broader C++ runtime claim.

last-re-verified: 2026-06-12 (sol1/sol2 live canonical hashes matched original retained capture hashes 12a186ff4e0cd5eefc83d2ee75d011f5b660f6d71952e251a3731ef9b9301f53/2a2a0117b3d15452ffff2cad94497147c0b696b994e84372b7efe44eaa694516; promoted raw-cmp-exit=1/1; promoted-golden scorer skeleton matched with minimum observed digits 38/38 below the 50-digit floor; stdout hash 1bd1ef3e78f912cda3c198b0642711b06481c40e2a601156a5eca524fddb36f8; stderr matched empty hash)
