# M7 B63n D246 Mathematica Attempt

Status: blocked. This report records a successful fresh Mathematica/AMFlow
invocation for the `automatic_phasespace` target, but it does not publish
D2/D4/D6 weighted-residue coefficients and does not modify the D246 evidence
sidecar.

## Worktree And Source State

- Lane claim: `lane6 mathematica-b63n-d246 3923518 2026-06-10T15:43:36-04:00`.
- Detached worktree: `/tmp/autoibp_lane6_b63n_d246_3923518`.
- Worktree base: `origin/main` at
  `072955c3be989927a8afb30d8567bfb374ec174e`.
- Prompt head `89b61292cc231e8a73a5d1918a6b8b04002789b2` was not current after
  `git fetch origin`; no local branch rebase was needed because the worktree was
  created detached from current `origin/main`.

## AMFlow Inventory

The retained verification tree contains multiple AMFlow checkouts. The
invocable source matching the M7 plan is:

- `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260419-required-set/inputs/upstream/amflow`
- `/n/holylabs/schwartz_lab/Lab/obarrera/reference-inputs/autonomousIBP/cpc/amflow-gitlab-1.1-extracted`

The script that can invoke the b63n `automatic_phasespace` surface is:

- `examples/automatic_phasespace/run.wl`

It sets:

- family `phase`
- loops `{l1, l2}`
- legs `{p1, p2}`
- numeric substitutions `{s -> 100, msq -> 1}`
- prescription `{0, 0}`
- cut vector `{1, 0, 1, 0, 1, 0, 0}`
- target `{j[phase,1,2,1,1,1,1,1]}`

Pinned source hashes from the retained AMFlow 1.1 tree:

```text
f9f021e584334cc21fb682526eaf9f55de95b6f2a58b2971ae46a445fd46e2bf  examples/automatic_phasespace/run.wl
6fd47002b36399ee71c38e3e43e5e75541d1f2641966ca103fc8b8ce37dc7add  AMFlow.m
22c63b2aa4a4c8236a9593d39ba7ae8283efa12cb7730401e640ff1b43875585  diffeq_solver/DESolver.m
```

AMFlow version:

```text
AMFlow 1.1, package info {"1.1", "5-Jun-2022"}
```

Mathematica and reducer availability:

```text
Mathematica: 13.3.0 for Linux x86 (64-bit)
Kira: /n/holylabs/schwartz_lab/Lab/obarrera/toolchains/kira/bin/kira
Kira version: 3.1
Fermat: /n/holylabs/schwartz_lab/Lab/obarrera/toolchains/fermat/fer64
Fermat version: 5.25
```

The upstream AMFlow `ibp_interface/Kira/install.m` points at missing original
author paths under `/mnt/zfsusers/xiao6/...`; the fresh driver overrides
the Mathematica `Kira` package variables `$KiraExecutable` and
`$FermatExecutable` to the local toolchains above.

## Fresh Mathematica Attempt

Scratch directory:

```text
/tmp/autoibp_b63n_d246_amflow_3923518
```

Command:

```text
bash -lc 'module load mathematica/13.3.0-fasrc01; cd /tmp/autoibp_b63n_d246_amflow_3923518; math -script run_b63n_d246_amflow.wls > artifacts/mathkernel.stdout.log 2> artifacts/mathkernel.stderr.log'
```

Requested parameters:

```text
target: {j[phase,1,2,1,1,1,1,1]}
precision_requested_digits: 80
eps_order_requested: 4
AMFlow generated working precision: 612
AMFlow generated truncated order: 1224
epsilon sample count: 14
```

Artifacts:

```text
84fdd9cc1753ae0299f3b19e4ec4304aaa3143c3fb227f6d26f73618f4473b09  /tmp/autoibp_b63n_d246_amflow_3923518/artifacts/sol.inputform.txt
ec7f282e14a7e8ce5dc33f0d6c670423818c8d14335f878457f8c6ec5223d8ab  /tmp/autoibp_b63n_d246_amflow_3923518/artifacts/mathkernel.stdout.log
a1b023adacf3ebc304969c972ca4211e08d3c95bb01e04094c370b41d8fd4018  /tmp/autoibp_b63n_d246_amflow_3923518/run_b63n_d246_amflow.wls
```

`artifacts/mathkernel.stderr.log` is empty. The stdout log records:

```text
BlackBoxAMFlow: amflow systems constructed in 39s.
BlackBoxAMFlow: amflow systems solved in 105s.
BlackBoxAMFlow: finished in 151s.
SolveIntegrals: finished in 151s.
B63N_D246_DRIVER_END
```

The raw Mathematica result is one replacement list. It contains the requested
full target plus four returned master values:

```text
j[phase,1,2,1,1,1,1,1] ->
  2.5708102516005612616396009246954781872595379415587437454231170779435821117181513316741766472781412640662e-12/eps^3
  -3.20653621041459341972315223841705226390966534602575492578230482068926724961439806940453600007443239162883e-11/eps^2
  -4.794511732939943431317694091963344787106375625701874167570292662413301379498516089700576882588919269928961e-10/eps
  +1.7713707267178957229366117703949801839756472969547511665631619777898393879122983480971141537575778148777772e-9

j[phase,1,0,1,0,1,0,0] -> 0.011436653587216170603488206474540781177498657609252633766297987379607035677471615396548747525141
j[phase,1,-1,1,0,1,0,0] -> -0.396032507988292047575620300057499042756886264273062865940575847048244521290460402877547369197601
j[phase,1,1,1,0,1,0,1] -> 0.000030720649006477414985084459782523348784663355628200670851742990260087739705186549604861991449
j[phase,1,1,1,1,1,1,1] ->
  -2.545102149084555649023204915448523405386942562143156307968885907164146290600969818357434880805359851425582e-10/eps^3
  +3.6834912781273586153305616991225864223479572049941286381182589539152038352384480523819776162347600379976591e-9/eps^2
  -1.13123798116573013708846911798682322411590354708314604382621158746544790835811771475484286871074883024953607e-8/eps
  -9.0912895525194362997602232214308375551339261057806089777934212760928858257929641724862050196575428817284917e-9
```

## Blocker

The AMFlow invocation succeeded, but the output is not sufficient to populate
`tools/reference-harness/specs/m6/lane146/b63n-d246-weighted-residue-reference-evidence.json`
with honest D2/D4/D6 entries.

Reasons:

1. `SolveIntegrals` returns the combined full target
   `j[phase,1,2,1,1,1,1,1]` and a master replacement list. It does not label or
   decompose selected-weight coefficient series for D2, D4, and D6.
2. The M7 plan requires each D2/D4/D6 sidecar entry to record an exact AMFlow
   expression element, epsilon order, eta-zero selection rule, and extraction
   label. No such per-weight extraction element appears in the raw AMFlow
   result.
3. The plan explicitly says a separate scoped-target shortcut needs a written
   derivation and is not approved by default. I did not invent a projection from
   the full target or run unsupported alternate targets.
4. Current runtime code still marks D2/D4/D6 as synthetic moment seeds:
   `src/runtime/cutkosky_transport.cpp` says the seed path has "no endpoint
   Laurent coefficient evaluated or published"; only D7 has a reviewed
   non-synthetic scoped series. Reusing those D2/D4/D6 seeds would fake the
   requested reference data.
5. Only one 80-digit AMFlow run was performed. Even if the full target series
   were the desired publication object, the sidecar requires a reviewed
   extraction and independent 50-digit agreement claims per D2/D4/D6 coefficient.

Therefore the D246 sidecar remains a skeleton, `passed` remains false, and no
`boundary_unsolved` to `qualified` or `solved` promotion is justified.

## Verifier Results

Existing skeleton sidecar:

```text
python3 tools/reference-harness/scripts/verify_b63n_d246_evidence.py \
  --sidecar-path tools/reference-harness/specs/m6/lane146/b63n-d246-weighted-residue-reference-evidence.json
```

Result: schema-valid skeleton, not published:

```text
published_evidence: false
skeleton_evidence: true
eps_orders_by_weight: D2=[], D4=[], D6=[]
minimum_digit_agreement: null
```

Published requirement:

```text
python3 tools/reference-harness/scripts/verify_b63n_d246_evidence.py \
  --sidecar-path tools/reference-harness/specs/m6/lane146/b63n-d246-weighted-residue-reference-evidence.json \
  --require-published
```

Result: expected failure:

```text
sidecar is schema-valid but does not contain published D2/D4/D6 evidence
```

Verifier self-check:

```text
python3 tools/reference-harness/scripts/verify_b63n_d246_evidence.py --self-check
```

Result: passed.

## Required Follow-Up

A coefficient-producing lane needs one of the following before the D246 sidecar
can be populated:

1. An AMFlow/Mathematica extraction script that emits labeled D2, D4, and D6
   selected-weight coefficient series from the full
   `j[phase,1,2,1,1,1,1,1]` target, with a documented mapping from each output
   element to the sidecar fields.
2. A reviewed derivation approving separate scoped AMFlow targets or another
   CAS projection, followed by independent high-precision agreement checks.

Until then, publishing the raw full-target coefficients above as D2/D4/D6
sidecar values would be a false claim.
