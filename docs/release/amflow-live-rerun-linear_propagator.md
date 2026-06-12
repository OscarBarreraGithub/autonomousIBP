# AMFlow Live Rerun: linear_propagator

Date: 2026-06-12

## Example

- Example: `linear_propagator`
- Upstream Mathematica script:
  `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260419-required-set/inputs/upstream/amflow/examples/linear_propagator/run.wl`
- Live scratch root:
  `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/live-reruns/codex-amflow-live-rerun-linear_propagator-20260612T051112Z`
- Retained committed AMFlow golden pointer:
  `tools/reference-harness/specs/phase0/linear_propagator.golden-manifest.json`
- Retained AMFlow `sol` golden:
  `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/work/goldens/phase0/linear_propagator/captured/sol`

The upstream script was run unchanged. It calls `SolveIntegralsGaugeLink` for
the lightlike `gauge` family with four requested gauge-link integrals,
`precision = 20`, and `epsorder = 10`, then writes the AMFlow rule list to
`sol`.

## Version Stack

- Mathematica: `13.3.0 for Linux x86 (64-bit) (June 3, 2023)`
- AMFlow: `1.1`, release date `5-Jun-2022`
- Kira: `3.1`, `/n/holylabs/schwartz_lab/Lab/obarrera/toolchains/autonomousIBP/kira/installs/kira-3.1/bin/kira-3.1`
- Fermat: `5.25`, `/n/holylabs/schwartz_lab/Lab/obarrera/toolchains/autonomousIBP/fermat/5.25/ferl64/fer64`

The live scratch AMFlow tree had only `ibp_interface/Kira/install.m` patched to
point at the cluster Kira/Fermat executables above. The upstream
`examples/linear_propagator/run.wl` script itself was run unchanged.

## Live Invocation

```bash
RUNROOT=/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/live-reruns/codex-amflow-live-rerun-linear_propagator-20260612T051112Z
source /etc/profile
module purge
module load mathematica/13.3.0-fasrc01
export FERMATPATH=/n/holylabs/schwartz_lab/Lab/obarrera/toolchains/autonomousIBP/fermat/5.25/ferl64/fer64
cd "$RUNROOT/amflow/examples/linear_propagator"
rm -f sol
rm -rf cache
timeout 5400s wolframscript -file "$RUNROOT/amflow/examples/linear_propagator/run.wl" > "$RUNROOT/live.stdout" 2> "$RUNROOT/live.stderr"
```

Run metadata:

```text
live-start: 2026-06-12T01:13:41-04:00
live-end: 2026-06-12T01:18:57-04:00
live-exit: 0
```

## Live Output Digest

- Live `sol` raw file: `364766c1d56f1fe9cced5f68e9c29ad345d28a2afc6fa8737c6040f6c20ff9bb`
- Retained `sol` raw file: `364766c1d56f1fe9cced5f68e9c29ad345d28a2afc6fa8737c6040f6c20ff9bb`
- Retained canonical `InputForm` without trailing newline: `d8bd47500b2bcadb7b1fe5d63e606714757ac7d93976f60c0bda74dd84ac4e52`
- Live stdout: `3c47c166119822a026515a96437030d830e8deeb0091b631a17130be685e9d24`
- Live stderr: `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` (empty)

Sample stdout lines:

```text
SetReductionOptions -> {IBPReducer -> Kira, BlackBoxRank -> 3, BlackBoxDot -> 0, ComplexMode -> True, DeleteBlackBoxDirectory -> False}
CheckDep: dependencies of current reducer:
Kira executable -> True
Fermat executable -> True
SolveIntegralsGaugeLink: integrals will be evaluated at 31 different eps values {101/208000, 51/104000, 103/208000, 1/2000, 21/41600, 53/104000, 107/208000, 27/52000, 109/208000, 11/20800, 111/208000, 7/13000, 113/208000, 57/104000, 23/41600, 29/52000, 9/16000, 59/104000, 119/208000, 3/5200, 121/208000, 61/104000, 123/208000, 31/52000, 1/1664, 63/104000, 127/208000, 1/1625, 129/208000, 1/1600, 131/208000}.
SolveIntegralsGaugeLink: working precision is 246 and truncated order is 492.
BlackBoxAMFlow: finished in 263s.
ExpandGaugeX: integrals expanded in 31s.
SolveIntegralsGaugeLink: finished in 313s.
```

The live `sol` contains nine AMFlow rules: the four requested gauge-link
targets plus lower-sector and zero-valued entries emitted by
`SolveIntegralsGaugeLink`.

## Comparison

Compared live output against:

- `tools/reference-harness/specs/phase0/linear_propagator.golden-manifest.json`
- `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/work/goldens/phase0/linear_propagator/captured/sol`
- Existing committed comparison summaries:
  `tools/reference-harness/specs/m5/comparisons/lane39/linear_propagator.compare.json`
  and
  `tools/reference-harness/specs/m5/comparisons/lane45/linear_propagator.compare.json`

The fresh live Mathematica output is byte-identical to the retained AMFlow
`sol` golden:

```text
raw-cmp-exit=0
```

The committed `tools/reference-harness/specs/m5/lane*/goldens/` directories do
not contain a `linear_propagator` golden file. At this commit, those directories
only contain scoped lane50 goldens for `spacetime_dimension`,
`user_defined_amfmode`, and `user_defined_ending`. The retained
`linear_propagator` golden is the phase0 manifest above, and both M5 lane39 and
lane45 comparison summaries point to that same manifest.

Both M5 comparison summaries record 57/57 coefficients compared across nine
AMFlow rules with no failures and minimum present coefficient agreement of 31
digits against the retained AMFlow surface. Those comparisons remain
retained-sample C++ evidence; this document adds a live Mathematica+AMFlow rerun
of the upstream script, not new C++ gauge-link transport coverage.

## Prior Attempt Check

An earlier detached worktree at `/tmp/autoibp-linear-propagator.OJshd5` contains
commit `701b9192b789193c980a2e2d7b89dc8f41fc908d` with a similar
`linear_propagator` live-rerun note. That commit was not replayed wholesale
because it is stale relative to current `origin/main` (`e6e1e18`): applying it
directly would remove the newer
`docs/release/amflow-live-rerun-differential_equation_solver.md` note and
rewind release-health documentation/script changes. The stale base explains why
the prior attempt did not land as-is; the rerun itself was not treated as a
failed Mathematica result.

## Status

`reproduced-fully-live`

This confirms the Mathematica 13.3 + AMFlow 1.1 + Kira 3.1 + Fermat stack can
freshly rerun the upstream `linear_propagator` example and reproduce the
retained phase0 AMFlow `sol` output byte-for-byte. This is a live
retained-golden reproducibility claim for the upstream script output, not a
broader claim that the C++ runtime now implements full `b64ag` gauge-link
transport, finite-part extraction, or high-precision endpoint recapture.
