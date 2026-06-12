# AMFlow Live Rerun: differential_equation_solver

Date: 2026-06-12

## Example

- Example: `differential_equation_solver`
- Upstream Mathematica scripts:
  `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260422-de-d0-pair/inputs/upstream/amflow/examples/differential_equation_solver/run.wl`
  and
  `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260422-de-d0-pair/inputs/upstream/amflow/examples/differential_equation_solver/diffeq.wl`
- Live scratch root:
  `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/live-reruns/lane3-amflow-live-rerun-differential_equation_solver-20260612T042134Z`
- Retained full AMFlow golden manifest:
  `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260422-de-d0-pair/goldens/phase0/differential_equation_solver/golden-manifest.json`
- Retained phase-0 capture summary:
  `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260422-de-d0-pair/comparisons/phase0/differential_equation_solver.summary.json`

The live scratch AMFlow tree had only `ibp_interface/Kira/install.m` patched to
point at the cluster Kira/Fermat executables. The upstream
`examples/differential_equation_solver/run.wl` and `diffeq.wl` scripts were run
unchanged.

## Version Stack

- Mathematica: `13.3.0 for Linux x86 (64-bit) (June 3, 2023)`
- AMFlow: `1.1`, release date `5-Jun-2022`
- AMFlow tree commit: `775162498ab18493c45254b861669b4151b841ee`
- Kira: `3.1`, `/n/holylabs/schwartz_lab/Lab/obarrera/toolchains/autonomousIBP/kira/installs/kira-3.1/bin/kira-3.1`
- Fermat: `5.25`, `/n/holylabs/schwartz_lab/Lab/obarrera/toolchains/autonomousIBP/fermat/5.25/ferl64/fer64`

## Live Invocation

```bash
RUNROOT=/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/live-reruns/lane3-amflow-live-rerun-differential_equation_solver-20260612T042134Z
source /etc/profile
module purge
module load mathematica/13.3.0-fasrc01
export FERMATPATH=/n/holylabs/schwartz_lab/Lab/obarrera/toolchains/autonomousIBP/fermat/5.25/ferl64/fer64
cd "$RUNROOT/amflow/examples/differential_equation_solver"
rm -f redtable diffeq sol1 sol2 asyexp0 asyexp1 asyexp1-fit
rm -rf cache
timeout 5400s wolframscript -file "$RUNROOT/amflow/examples/differential_equation_solver/run.wl" > "$RUNROOT/live.stdout" 2> "$RUNROOT/live.stderr"
timeout 5400s wolframscript -file "$RUNROOT/amflow/examples/differential_equation_solver/diffeq.wl" > "$RUNROOT/diffeq.stdout" 2> "$RUNROOT/diffeq.stderr"
```

Run metadata:

```text
live-start: 2026-06-12T00:25:39-04:00
live-end: 2026-06-12T00:27:35-04:00
live-exit: 0
diffeq-start: 2026-06-12T00:28:17-04:00
diffeq-end: 2026-06-12T00:28:24-04:00
diffeq-exit: 0
```

## Live Output Digest

`run.wl`:

- Live stdout: `de26839d9004f079ec65a93cdee7c4ecb51874ed5224202a4bcf6807ceb4a4ce`
- Live stderr: `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` (empty)
- Live `redtable`: `29b2f80f64ae294bd9e6dfa3c1a31689b9bcbd82ce54c9bcf56d25ea7816453e`
- Live `diffeq`: `a2dc5890cec344b9af0dfeb91974c9d1e0ea37a72bcd6076b2365c2f749e0242`
- Live `sol1`: `acd37a21a4e45337556a3c0f54bc02ea2ba0c74bdb366712752b8092082c9b0a`
- Live `sol2`: `63b6e78351b7c2a81967c3ab9a89f195ed78c1263dfdd25ee63a97a29e38b2c2`

`diffeq.wl`:

- Live stdout: `8a87dcdd955dd736208b21fe0a78bc26cc4637f8fd1e322fca735d685fd530a0`
- Live stderr: `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` (empty)
- Live `asyexp0`: `b259a0d4deafcdb7423a7e30f4126086d00d770f5b1702b1ea1c2802be8cefdb`
- Live `asyexp1`: `382fc0c830c53ab3080e8f9a9b30df1a79eee1bfb107851b52566ecf608a4f07`
- Live `asyexp1-fit`: `d040e4d6ae5c11c253478d42c6bc7a0854be9b8923bc12eaa13fff1334bf08dd`

Sample `run.wl` stdout lines:

```text
SetReductionOptions -> {IBPReducer -> Kira, BlackBoxRank -> 3, BlackBoxDot -> 0, ComplexMode -> True, DeleteBlackBoxDirectory -> False}
CheckDep: dependencies of current reducer:
Kira executable -> True
Fermat executable -> True
BlackBoxAMFlow: finished in 49s.
```

Sample `diffeq.wl` stdout lines:

```text
system loaded: "check"
current regular point -> 0.5
current regular point -> 0.25
current regular point -> 0.125
system cleared: "check"
system loaded: "asyexp0"
NormalizeMat finished in 0s.
system loaded: "asyexp1"
```

## Comparison

The live `run.wl` outputs are byte-identical to the retained full AMFlow
`de-d0-pair` captured goldens:

```text
redtable byte-identical
diffeq byte-identical
sol1 byte-identical
sol2 byte-identical
```

The `diffeq.wl` asymptotic-expansion files are not byte-identical to the retained
Kira backup files because Mathematica printed high-precision numeric coefficients
with small last-digit drift. A direct Wolfram evaluation of live minus retained
backup at representative points bounded the drift:

```text
asyexp0 at s=1/10 absdiff=5.2493557977861311211e-32
asyexp0 at s=1/5 absdiff=7.7366761186670021485e-32
asyexp1 at s=1/2 absdiff=2.4509027565130701297e-27
asyexp1 at s=9/10 absdiff=1.5754105239170089318e-27
asyexp1-fit at s=1/2 absdiff=0
asyexp1-fit at s=9/10 absdiff=0
```

## Status

`reproduced-fully-live`

This confirms the Mathematica 13.3 + AMFlow 1.1 + Kira 3.1 + Fermat stack can
freshly rerun the upstream `differential_equation_solver` example, reproduce the
retained full AMFlow `redtable`/`diffeq`/`sol1`/`sol2` output surface
byte-for-byte, and rerun the upstream DESolver continuation plus asymptotic
expansion workflow with only sub-precision numeric printing drift in the
expansion files. This is a live AMFlow retained-golden reproducibility claim,
not a claim that the C++ runtime now implements the full DESolver workflow.

last-re-verified: 2026-06-12 (run.wl redtable/diffeq/sol1/sol2 matched; stdout diverged to 3637da422d9d90eb81ac6bc6b78e2c03d038d32be75f20340bf9595e2a43303e from timing-only log deltas; stderr matched)
