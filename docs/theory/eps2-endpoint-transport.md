# eps^2 Endpoint Transport Theory Design

Status: theory-reviewed consensus for the retained `automatic_loop` eta=0
endpoint transport. This document is suitable to check in as
`docs/theory/eps2-endpoint-transport.md`.

## Review Approval

This final consensus document synthesizes:

- Role A physicist-implementer draft:
  `/tmp/autoibp_orch/exec/lane4_role_a_draft.md`
- Role B independent physicist-reviewer summary:
  `/tmp/autoibp_orch/exec/lane4_role_b_independent.md`
- Role C Mathematica-source audit:
  `/tmp/autoibp_orch/exec/lane4_role_c_audit.md`
- Role D synthesis draft:
  `/tmp/autoibp_orch/exec/lane4_role_d_synthesis_draft.md`

All three required reviewers approved the synthesis before this final write:

- Role A approval:
  `/tmp/autoibp_orch/exec/lane4_role_a_synthesis_approval.md`
- Role B approval:
  `/tmp/autoibp_orch/exec/lane4_role_b_synthesis_approval.md`
- Role C approval:
  `/tmp/autoibp_orch/exec/lane4_role_c_synthesis_approval.md`

Approval scope:

- Role A found the source-root caveat, analytic endpoint structure, constants,
  C++ contract, and verification plan consistent with its implementation draft.
- Role B approved the design direction because it preserves nonzero eps^2
  transport, primitive endpoint derivation, Kira target reduction, and a real
  eps^2 AMFlow golden requirement.
- Role C approved source-location and provenance consistency. Role C did not
  independently validate the analytic C++ formulas beyond confirming that this
  document labels them as proposed implementation structure rather than stored
  symbolic AMFlow source content.

## Source-Root Caveat

The prompt-named source root:

`/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/work/amflow/`

does not exist on this node. All reviewed eps^2 AMFlow evidence is under:

`/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/work-eps4/amflow/`

All AMFlow source paths below are relative to that audited root unless otherwise
stated.

The current retained phase-0 golden under:

`/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/work/goldens/phase0/automatic_loop/`

is an older positive-eps^1 capture. It cannot validate positive eps^2.

Role C's audit correction is central: the audited AMFlow artifacts do not store
symbolic eta=0 closed forms for these masters. AMFlow computes high-precision
endpoint samples at fixed epsilon values, fits the epsilon Laurent expansion,
and writes numeric coefficients in `sol1` and `sol2`. This document cites AMFlow
for numeric fitted eta^0 Laurent coefficients and gives analytic formulas as the
proposed C++ implementation model.

## AMFlow Computation Path

Common audited Mathematica flow:

- `examples/automatic_loop/run.wl:23-29`: box1 target list, `epsorder = 4`,
  and `SolveIntegrals`.
- `examples/automatic_loop/run.wl:43-46`: box2 target list and `SolveIntegrals`.
- `AMFlow.m:1203-1212`: epsilon sample generation.
- `AMFlow.m:1216-1230`: `SolveIntegrals`, `BlackBoxAMFlow`, `FitEps`, and final
  Laurent `Series`.
- `AMFlow.m:1156-1192`: target reduction, AMFlow system solve, and reduction
  back to requested targets.
- `AMFlow.m:1039-1068`: generated `solution.wl` template.
- `examples/automatic_loop/cache/box1_amflow/1/solution.wl:17-30` and `:33-38`:
  generated box1 endpoint solve and writeout.
- `examples/automatic_loop/cache/box2_amflow/1/solution.wl:17-30` and `:33-38`:
  generated box2 endpoint solve and writeout.
- `diffeq_solver/DESolver.m:1065-1095`: AMFlow transport, asymptotic solve, and
  `PickZeroRuleS /@ AsyExp`.
- `diffeq_solver/DESolver.m:916-927`: `CalcZero`.
- `diffeq_solver/DESolver.m:820-878`: `Calcx00`.
- `diffeq_solver/DESolver.m:1053-1061`: `PickZeroRuleS`, selecting the eta^0
  endpoint term.

## A/B Disagreements Preserved

No fundamental A/B disagreement was found on the eight eps^2 numeric
coefficients. The independent tables agree to the displayed precision.

Preserved differences and resolutions:

- Source root: Roles A and B noted that `/work/amflow/` is absent and used
  `/work-eps4/amflow/`; Role C audited and confirmed this. This document cites
  only the audited `/work-eps4/amflow/` root.
- Symbolic versus numeric AMFlow evidence: Role A gave closed-form analytic
  endpoint building blocks. Role C found that the audited AMFlow artifacts
  contain numeric sample vectors and fitted Laurent coefficients, not symbolic
  closed forms. Resolution: cite AMFlow for numeric eps^2 eta^0 coefficients and
  use analytic formulas as the C++ design.
- Bubble formulation: Role A wrote an absolute closed form using
  `L_s = log(100) - i*pi`; Role B described the same one-order-higher branch
  transport as a convolution with `q = -log(100) + i*pi`. Resolution: implement
  the absolute endpoint series through eps^2, and treat the convolution form as
  a consistency check with the eps^1 pattern.
- Scalar-box polylog variables: Role A used `Li_n(-99)` and `Li_n(99/100)`;
  Role B described equivalent finite-box-ratio variables. Resolution: for the
  retained automatic-loop kinematics, use the explicit `Li_n(-99)` and
  `Li_n(99/100)` basis because it reproduces AMFlow. Equivalent transformed
  bases require a separate numerical cross-check.
- `log(2)`: the lane prompt mentioned constants such as `log(2)`, but Roles A
  and B found no independent `log(2)` requirement for the direct retained basis.
  Do not add `log(2)` unless a future transformed polylog basis introduces it
  and is separately validated.

## eps^2 Endpoint Coefficients

These are the eta^0 endpoint Laurent coefficients multiplying positive
`eps^2`. The cited `sol1` and `sol2` entries are numeric fitted AMFlow output
from `epsorder = 4`.

| integral | AMFlow source location | eps^2 coefficient |
|---|---|---|
| `box1[1,0,1,0]` | direct system master; `cache/box1_amflow/1/masters:1-2`, `cache/box1_amflow/1/solution.wl:17-30`, `:33-38`, endpoint samples `cache/box1_amflow/1/solution:2-82`, final expansion `examples/automatic_loop/sol1:48-63`, coefficient on `:59-63` | `6.44730328186221738344546728289721664535 + 14.43998113538002185352152298908587882147 I` |
| `box1[1,1,1,1]` | direct system master; `cache/box1_amflow/1/masters:1-2`, `cache/box1_amflow/1/solution.wl:17-30`, `:33-38`, endpoint samples `cache/box1_amflow/1/solution:247-330`, final expansion `examples/automatic_loop/sol1:63-83`, coefficient on `:79-83` | `-0.16089951816601819118756768300565063456 + 0.13716704522845915758171026860531647067 I` |
| `box1[1,2,2,1]` | reduced target; target list `run.wl:25`, Kira reduction `cache/box1_amflow/0/results/box1/kira_target.m:8-11`, master basis `masters.final:1-3`, reduction application `AMFlow.m:1181-1184`, final expansion `examples/automatic_loop/sol1:22-43`, coefficient on `:39-43` | `0.14735168712079518453252473680205772348 - 0.00688641710961387697481219724101344315 I` |
| `box1[2,0,1,0]` | reduced target; target list `run.wl:25`, Kira reduction `cache/box1_amflow/0/results/box1/kira_target.m:2-3`, reduction application `AMFlow.m:1181-1184`, final expansion `examples/automatic_loop/sol1:1-16`, coefficient on `:12-16` | `-0.03834262045857324823862860591333565607 - 0.34435501155465915148857404719557111833 I` |
| `box2[1,-1,1,0]` | reduced target; target list `run.wl:44`, Kira reduction `cache/box2_amflow/0/results/box2/kira_target.m:9-10`, reduction application `AMFlow.m:1181-1184`, final expansion `examples/automatic_loop/sol2:27-42`, coefficient on `:38-42` | `-322.36516409311086917227336414486083227 - 721.99905676900109267607614945429394107 I` |
| `box2[1,0,1,0]` | direct system master; `cache/box2_amflow/1/masters:1-2`, `cache/box2_amflow/1/solution.wl:17-30`, `:33-38`, endpoint samples `cache/box2_amflow/1/solution:2-82`, final expansion `examples/automatic_loop/sol2:42-57`, coefficient on `:53-57` | `6.44730328186221738344546728289721664535 + 14.43998113538002185352152298908587882147 I` |
| `box2[1,1,1,1]` | direct system master; `cache/box2_amflow/1/masters:1-2`, `cache/box2_amflow/1/solution.wl:17-30`, `:33-38`, endpoint samples `cache/box2_amflow/1/solution:247-330`, final expansion `examples/automatic_loop/sol2:62-82`, coefficient on `:78-82` | `-0.16089951816601819118756768300565063456 + 0.13716704522845915758171026860531647067 I` |
| `box2[2,1,1,1]` | reduced target; target list `run.wl:44`, Kira reduction `cache/box2_amflow/0/results/box2/kira_target.m:2-4`, master basis `masters.final:1-3`, reduction application `AMFlow.m:1181-1184`, final expansion `examples/automatic_loop/sol2:1-21`, coefficient on `:17-21` | `-0.03046660578951938362705668487815131849 + 0.00226663064196703411844670283474196812 I` |

## Analytic Endpoint Structure

Use `epsilon = eps`, `d = 4 - 2 eps`, `s = 100`, `t = -1`, and the retained
`NegIm` branch:

`L_s = log(100) - i*pi`.

The second bubble channel appearing in target reductions has:

`L_0 = 0`.

Let `gamma` denote EulerGamma and `zeta_n = Zeta[n]`.

### Bubble Master `[1,0,1,0]`

For the primitive retained bubble endpoint:

`B(L, eps) = exp(-eps L) Gamma(1 + eps) Gamma(1 - eps)^2 / (eps Gamma(2 - 2 eps))`.

Write:

`B(L, eps) = eps^-1 + b0(L) + b1(L) eps + b2(L) eps^2 + O(eps^3)`.

Then:

`A1(L) = 2 - gamma - L`

`A2 = 2 - zeta_2 / 2`

`A3 = 8/3 - 7 zeta_3 / 3`

`b0(L) = A1(L)`

`b1(L) = A2 + A1(L)^2 / 2`

`b2(L) = A3 + A1(L) A2 + A1(L)^3 / 6`.

For `L_s`:

`b2(L_s) = 6.44730328186221738344546728289721664534973065980177252856366 + 14.4399811353800218535215229890858788214748274411869798619906 i`.

For `L_0`:

`b2(L_0) = 2.01727002606826746014944022123213915045325181100324524012973`.

This generalizes the eps^1 branch-log implementation pattern from commits
`5e28469`, `84e34ac`, and `4781a81`. In convolution form, one more order would
have:

`c2' = c2 + delta c1 + q c0 + r c[-1]`.

The implementation should not guess `r` from the old eps^1 correction path.
Compute the absolute endpoint series above and upsert all orders through eps^2.

### Scalar Box Master `[1,1,1,1]`

For the retained scalar box, use the standard one-loop massless box structure:

`Box(eps) = 2/(s t) eps^-2 R(eps) [T(L_s, z_s; eps) + T(L_0, z_t; eps)]`

with:

`z_s = 1 + s/t = -99`

`z_t = 1 + t/s = 99/100`

`R(eps) = Gamma(1 + eps) Gamma(1 - eps)^2 / Gamma(1 - 2 eps)`

`log R(eps) = -gamma eps - zeta_2 eps^2/2 - 7 zeta_3 eps^3/3 - 13 zeta_4 eps^4/4 + O(eps^5)`

`T(L, z; eps) = exp(-eps L) H(z; eps)`

`H(z; eps) = 2F1(1, -eps; 1 - eps; z)`

`H(z; eps) = 1 - eps Li_1(z) - eps^2 Li_2(z) - eps^3 Li_3(z) - eps^4 Li_4(z) + O(eps^5)`.

Because the scalar box carries `eps^-2`, the positive-eps^2 coefficient requires
the bracket through weight four. The retained-point analytic basis is:

- powers of `L_s = log(100) - i*pi`;
- `EulerGamma`;
- `zeta_2`, `zeta_3`, `zeta_4`;
- `Li_n(-99)` and `Li_n(99/100)` for `n = 1..4`.

`Li_1(-99) = -log(100)` and `Li_1(99/100) = log(100)`.

The scalar-box positive-eps^2 coefficient is:

`Box_2 = -0.1608995181660181911875676830056506345633261485296289699711 + 0.137167045228459157581710268605316470666674083845577944337 i`.

## Reduction Formulas for Dependent Targets

The implementation should make the primitive endpoint masters correct and then
let existing Kira target reduction propagate. The formulas below are for
verification and focused fallback tests; they should not become hardcoded
target-value transport.

Let `Box_k` be the scalar-box coefficient at `eps^k`.

- `box1[2,0,1,0] eps^2 = (-1/100) b2(L_s) + (2/100) b1(L_s)`.
- `box1[1,2,2,1] eps^2 = (-1/50) Box_2 + (-3/50) Box_1 + (-1/25) Box_0 + (-1/250000) b2(L_s) + (4/250000) b0(L_s) + (-1/25) b2(L_0) + (4/25) b0(L_0)`.
- `box2[1,-1,1,0] eps^2 = -50 b2(L_s)`.
- `box2[2,1,1,1] eps^2 = (1/100) Box_2 + (2/100) Box_1 + (1/50) b2(L_0) - (1/50) b1(L_0) - (3/50) b0(L_0) + (3/50)`.

## C++ Implementation Contract

Implementation should be confined to `src/cli/main.cpp` unless a separate
implementation lane deliberately widens scope.

Existing relevant functions in `src/cli/main.cpp`:

- `UpsertEpsilonCoefficient`
- `ApplyRetainedAutomaticLoopEtaZeroBranchLogTransportThroughEps1`
- `ApplyRetainedAutomaticLoopEtaZeroBoxEndpointTransportThroughEps1`
- `IsRetainedAutomaticLoopEtaZeroEndpointTransportState`
- `SuppressRetainedAutomaticLoopOrdersAboveEps1ForOutput`

The eps^2 implementation must remove or bypass suppression-only behavior for
the eps^2 golden comparison. It must not repeat the failed approaches from
commits `91f49c9` or `9ad4773`: suppressing output or hardcoding zero eps^2
values.

Recommended helper functions in `src/cli/main.cpp`:

- `BigFloat EulerGammaConstant()`.
- `BigFloat Zeta2Constant()`.
- `BigFloat Zeta3Constant()`.
- `BigFloat Zeta4Constant()`.
- `BigComplex RetainedAutomaticLoopNegImLogS()`, returning `log(100) - i*pi`.
- `std::map<int, BigComplex> RetainedBubbleEndpointSeriesThroughEps2(const BigComplex& L)`, returning orders `-1,0,1,2`.
- `std::map<int, BigComplex> RetainedMasslessBoxEndpointSeriesThroughEps2()`, returning orders `-2,-1,0,1,2`.
- `void UpsertEndpointSeries(...)`, a thin loop over `UpsertEpsilonCoefficient`.
- A small Laurent polynomial multiplication helper only if direct reduction
  needs local verification of target formulas. It must not hardcode final
  requested target master values.

Required hardcoded BigFloat constants are constants of the analytic basis, not
the eight requested master values:

- `EulerGamma = 0.5772156649015328606065120900824024310421593359399235988057672348848677`
- `zeta2 = 1.644934066848226436472415166646025189218949901206798437735558229370007`
- `zeta3 = 1.202056903159594285399738161511449990764986292340498881792271555341838`
- `zeta4 = 1.082323233711138191516003696541167902774750951918726907682976215444121`
- `pi` from `boost::math::constants::pi<BigFloat>()`
- `log100` computed as `log(BigFloat(100))`
- `Li2(-99) = -12.1924216690331713481545622511685878511398598894077433729929727577187`
- `Li2(99/100) = 1.588625448076375327031229473980552467944959731142123890278173449470347`
- `Li3(-99) = -23.73984691525023320027138380461145071816813102113563379173614436988083`
- `Li3(99/100) = 1.185832933645036934334943631307684427770200339548001769407386971861236`
- `Li4(-99) = -37.82748993315647215577219195660396537384734170803750232204987259645817`
- `Li4(99/100) = 1.070324146165229151869669275527449622472652092285159704680565216346799`

Do not add a `log(2)` constant for this direct basis. If a future implementer
chooses a transformed polylog basis that introduces `log(2)`, that basis must be
separately documented and numerically cross-checked against the AMFlow
coefficients above.

Recommended call structure for
`ApplyRetainedAutomaticLoopEtaZeroBranchLogTransportThroughEps2`:

1. Reuse `IsRetainedAutomaticLoopEtaZeroEndpointTransportState(spec)` or the
   same guard: benchmark `automatic_loop`, family `box1` or `box2`, variable
   `eta`, direction `NegIm`, singular points `eta=0` and `eta=100`, and
   requested epsilon order at least positive eps^2.
2. Locate primitive master `<family>[1,0,1,0]`. If absent, return `0`.
3. Build `bubble = RetainedBubbleEndpointSeriesThroughEps2(RetainedAutomaticLoopNegImLogS())`.
4. Upsert orders `-1,0,1,2`.
5. Update `target_values[master_index]` from order `0`, matching the existing
   eps^1 path's behavior.
6. Append the transported integral label once and update transport diagnostics
   without duplicates.

Recommended call structure for scalar-box eps^2 transport:

1. Use the same retained-state guard.
2. Locate primitive master `<family>[1,1,1,1]`. If absent, return `0`.
3. Build `box = RetainedMasslessBoxEndpointSeriesThroughEps2()`.
4. Upsert orders `-2,-1,0,1,2`.
5. Update diagnostics without duplicate transported labels.

After primitive endpoint series are upserted, existing direct target reduction
should produce:

- `box1[2,0,1,0]`
- `box1[1,2,2,1]`
- `box2[1,-1,1,0]`
- `box2[2,1,1,1]`

The implementation should not add a target-level function that hardcodes the
eight final eps^2 master values or hardcodes zeros.

## Verification Plan

Use an eps^2 AMFlow reference, not the current retained phase-0 golden.

Audited evidence that the current retained golden is insufficient:

- Historical captured `run.wl` uses `epsorder = 3`, which for one loop and
  `leading = -2` stops at positive `eps^1`.
- Current captured `sol1` and `sol2` stop at `*eps` for the requested entries.
- Current logs show 10 epsilon samples, consistent with order 3 rather than the
  12 samples seen for `epsorder = 4`.

Eps^2-capable AMFlow source/reference:

- `work-eps4/amflow/examples/automatic_loop/run.wl:23-29` and `:43-46`, with
  `epsorder = 4`.
- `work-eps4/amflow/examples/automatic_loop/sol1`
- `work-eps4/amflow/examples/automatic_loop/sol2`
- `work-eps4/capture_automatic_loop_eps4.sbatch:10-13` and `:39-42`
- `work-eps4/logs/cap-autoloop-eps4.9923516.out:9-11`, `:46-49`, and `:203-206`

Recommended verification sequence:

1. Recompute the C++ retained automatic-loop solve with positive eps^2 enabled,
   equivalent to `--eps-order 2 --digits 40` if that is the current CLI spelling.
2. Compare against the `work-eps4` AMFlow `sol1` and `sol2`, or capture a fresh
   golden from an AMFlow run with `epsorder >= 4` in AMFlow's convention.
3. Keep the eps^2 golden manifest separate from the older phase-0 retained
   golden until reviewed.
4. Use a comparison tolerance of at least 30 matching decimal digits for these
   retained constants.
5. Include any needed family aliases, for example `box1_amflow=box1` and
   `box2_amflow=box2`, if the C++ output and AMFlow bundle labels differ.
6. Require the eight eps^2 coefficients in this document to match. Also require
   all previously passing coefficients through eps^1 to remain unchanged.

Focused tests should fail against the fake implementations:

- `box1[1,0,1,0] eps^2` has nonzero real and imaginary parts matching the bubble
  formula.
- `box1[1,1,1,1] eps^2` has the nonzero scalar-box value above.
- `box2[1,-1,1,0] eps^2` equals `-50 b2(L_s)`.
- The two dotted targets `box1[1,2,2,1]` and `box2[2,1,1,1]` are produced by
  target reduction, not by zero filling or target-value hardcoding.

## Consensus

The real eps^2 endpoint transport should implement analytic primitive endpoint
series for the retained bubble and scalar-box masters through positive eps^2,
then let Kira target reduction propagate the dependent requested integrals.

The AMFlow reference for the eps^2 values is the audited `work-eps4/amflow`
capture. The absent `/work/amflow` path and the numeric, not symbolic, nature of
the AMFlow endpoint artifacts are mandatory caveats for any implementation lane.
