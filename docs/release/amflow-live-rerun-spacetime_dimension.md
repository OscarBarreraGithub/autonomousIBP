# AMFlow Live Rerun: spacetime_dimension

Date: 2026-06-11

## Example

- Example: `spacetime_dimension`
- Upstream Mathematica script: `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260422-de-d0-pair/inputs/upstream/amflow/examples/spacetime_dimension/run.wl`
- Live scratch script path: `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/live-reruns/lane6-amflow-examples-sweep-2494429-spacetime_dimension/amflow/examples/spacetime_dimension/run.wl`
- Retained full AMFlow golden manifest: `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260422-de-d0-pair/goldens/phase0/spacetime_dimension/golden-manifest.json`

The committed `tools/reference-harness/specs/m5/lane50/goldens/spacetime_dimension.direct-nondefault-d.amflow-golden.txt`
golden is only the narrow direct-D flag probe. This live rerun instead checks
the full upstream `spacetime_dimension` script surface from the retained
`de-d0-pair` AMFlow packet: `sol73D`, `sol13D`, and the dimensional-recurrence
residual through the script's checked order.

## Version Stack

- Mathematica: `13.3.0 for Linux x86 (64-bit) (June 3, 2023)`
- AMFlow: `1.1`, release date `5-Jun-2022`
- Kira: `3.1`, `/n/holylabs/schwartz_lab/Lab/obarrera/toolchains/autonomousIBP/kira/installs/kira-3.1/bin/kira-3.1`
- Fermat: `5.25`, `/n/holylabs/schwartz_lab/Lab/obarrera/toolchains/autonomousIBP/fermat/5.25/ferl64/fer64`

The live scratch AMFlow tree had only `ibp_interface/Kira/install.m` patched to
point at the cluster Kira/Fermat executables above. The upstream
`examples/spacetime_dimension/run.wl` script itself was run unchanged.

## Live Invocation

```bash
RUNROOT=/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/live-reruns/lane6-amflow-examples-sweep-2494429-spacetime_dimension
source /etc/profile
module purge
module load mathematica/13.3.0-fasrc01
export FERMATPATH=/n/holylabs/schwartz_lab/Lab/obarrera/toolchains/autonomousIBP/fermat/5.25/ferl64/fer64
cd "$RUNROOT/amflow/examples/spacetime_dimension"
rm -f sol73D sol13D
rm -rf cache
timeout 5400s wolframscript -file "$RUNROOT/amflow/examples/spacetime_dimension/run.wl" > "$RUNROOT/live.stdout" 2> "$RUNROOT/live.stderr"
```

Run metadata:

```text
live-start: 2026-06-11T21:37:17-04:00
live-end: 2026-06-11T21:39:25-04:00
live-exit: 0
```

## Live Output Digest

- Live `sol73D` raw file: `c9633bb7addf81289bb58b4dce4af537bf400f94b0a6f246bcd80e9f848056aa`
- Live `sol13D` raw file: `4d3d2e46359e6368455d7374f256f7eea1aa51f9310341c1c8a8d02d0ecce9e2`
- Live stdout: `473847f7a0652b552a5fb6277d946980fe7ee22b5cbebddf3c0bc686083ce9cf`
- Live stderr: `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` (empty)

Sample stdout lines:

```text
SetAMFOptions -> {AMFMode -> {Prescription, Mass, Propagator}, EndingScheme -> {Tradition, Cutkosky, SingleMass}, D0 -> 7/3, WorkingPre -> 100, ChopPre -> 20, XOrder -> 100, ExtraXOrder -> 20, LearnXOrder -> -1, TestXOrder -> 5}
SolveIntegrals: integrals will be evaluated at 17 different eps values {11849503/6463200, 5924753/3231600, 11849509/6463200, 1481189/807900, 2369903/1292640, 5924759/3231600, 11849521/6463200, 2962381/1615800, 11849527/6463200, 1184953/646320, 11849533/6463200, 370298/201975, 11849539/6463200, 5924771/3231600, 2369909/1292640, 2962387/1615800, 11849551/6463200}.
BlackBoxAMFlow: finished in 62s.
SolveIntegrals: finished in 62s.
```

Live `sol73D`:

```text
{j[bubble, 1, 1, 0] -> 45.86976571999710432109017717317929903595`20. +
   468.72208968159076588399455270665355064333`20.*eps,
 j[bubble, 1, 1, 1] -> -4.2703302806472433284240369999446674787`20. +
   19.14401488014631111597553226035949564813`20.*eps}
```

Live `sol13D`:

```text
{j[bubble, 1, 1, 0] -> 1.27416015888880845336361603258831386211`20. -
   2.26986385995484683247465481587494549379`20.*eps,
 j[bubble, 1, 1, 1] -> -0.95783939735938302162850218074056071554`20. -
   0.09717360094056361946961505525049880874`20.*eps}
```

## Comparison

Compared live output against the retained full AMFlow packet:

- `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260422-de-d0-pair/goldens/phase0/spacetime_dimension/captured/sol73D`
- `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260422-de-d0-pair/goldens/phase0/spacetime_dimension/captured/sol13D`

The raw files are not byte-identical: the upstream script requests
`precision = 20`, while the retained packet stores promoted 60-precision text.
The live values nevertheless agree with the retained packet to the live script
precision. Mathematica comparison output:

```text
sol73D keys: {j[bubble, 1, 1, 0], j[bubble, 1, 1, 1]}
sol73D differences: {j[bubble, 1, 1, 0] -> 0``18.338473478023072, j[bubble, 1, 1, 1] -> 0``19.369538533987757}
sol73D maxAbsDifference: ScientificForm[0``17.329084578801616, 20]
sol13D keys: {j[bubble, 1, 1, 0], j[bubble, 1, 1, 1]}
sol13D differences: {j[bubble, 1, 1, 0] -> 0``19.89477597879036, j[bubble, 1, 1, 1] -> 0``19.89477597879036}
sol13D maxAbsDifference: ScientificForm[0``19.644000189786354, 20]
recurrenceResidual: {SeriesData[eps, 0, {}, 2, 2, 1], SeriesData[eps, 0, {}, 2, 2, 1]}
```

The recurrence residual is `{O[eps]^2, O[eps]^2}`, matching the upstream script
check:

```text
(integrals /. sol13D) - (A /. d -> 7/3 - 2 eps) . (integrals /. sol73D) + O[eps]^2
```

## Status

`reproduced-matches-golden-at-requested-precision`

This confirms the Mathematica 13.3 + AMFlow 1.1 + Kira 3.1 + Fermat stack can
freshly rerun the upstream `spacetime_dimension` example and reproduce the
retained full AMFlow `sol73D`/`sol13D` output surface at the script's requested
20-digit precision, including the dimensional-recurrence check. This is a live
retained-golden reproducibility claim, not a broader C++ runtime claim for
nondefault-`D0` semantics.

last-re-verified: 2026-06-12 (sol73D/sol13D live hashes matched c9633bb7addf81289bb58b4dce4af537bf400f94b0a6f246bcd80e9f848056aa/4d3d2e46359e6368455d7374f256f7eea1aa51f9310341c1c8a8d02d0ecce9e2; retained comparison stayed zero at requested precision with maxAbsDifference 0``17.329084578801616 and 0``19.644000189786354; recurrence residual matched O[eps]^2; stdout diverged to 806e0565d8c3f25ae9cdc3859e66cd72629a9be29657a229e06b0557b9e892da from timing-only log deltas; stderr matched)
