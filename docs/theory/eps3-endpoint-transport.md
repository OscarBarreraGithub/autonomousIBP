# eps^3 Endpoint Transport Theory Design

Status: theory-reviewed consensus for the retained `automatic_loop` eta=0
endpoint transport. This document is suitable for the implementation lane.

## A/B Disagreements Preserved

No material A/B disagreement was found on the analytic eps^3 form, the eight
endpoint coefficients, the required new constants, the AMFlow-golden caveat, or
the refactor recommendation.

Preserved differences and resolutions:

- Numeric precision: Role A displayed several constants and coefficients with
  slightly more trailing digits than Role B. Resolution: use common values to at
  least 60 digits for constants and retain the displayed coefficient precision
  where both drafts agree.
- Notation: Role A used `Box_k`, `b_k`, and `c_k`; Role B used `C_k`, `b_k`,
  and `a_k`. Resolution: use `C_k` for scalar-box coefficients,
  `b_k = B_k(L_s)`, and `a_k = B_k(L_0)`.
- AMFlow provenance language: Role A cited endpoint sample vectors next to
  eps^3 coefficients; Role B emphasized that current samples and `sol1`/`sol2`
  are not a formal eps^3 golden. Role C confirmed this. Resolution: cite AMFlow
  for the endpoint-transport computation path and Kira reductions, but label the
  eps^3 numbers as analytic continuation of the reviewed eps^2 endpoint model
  until a fresh `epsorder >= 5` AMFlow golden exists.
- Implementation shape: both recommend an order-parametric helper now. Role B
  more explicitly scoped the refactor to local endpoint helpers only.
  Resolution: recommend `TransportThroughEpsOrder(int N)` now for local retained
  endpoint transport, with `N <= 3` guarded support, and defer broader
  solver/diagnostics redesign.

## Review Approval

This final consensus document synthesizes:

- Role A physicist-implementer draft:
  `/tmp/autoibp_orch/exec/lane10_role_a_draft.md`
- Role B independent physicist-reviewer draft:
  `/tmp/autoibp_orch/exec/lane10_role_b_independent.md`
- Role C Mathematica-source audit:
  `/tmp/autoibp_orch/exec/lane10_role_c_audit.md`
- Role D synthesis:
  `/tmp/autoibp_orch/exec/lane10_role_d_synthesis.md`

Approval scope:

- Role A approved the eps^3 analytic primitive endpoint model and
  implementation direction.
- Role B independently reproduced the same formulas, constants, coefficients,
  and verification caveat.
- Role C approved the cited AMFlow source locations as algorithm/provenance
  citations and explicitly rejected treating the current `work-eps4` output as a
  positive-eps^3 golden.
- Role D approved the consensus synthesis.

## Source-Root And Golden Caveat

Audited AMFlow root:

`/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/work-eps4/amflow/`

All AMFlow source paths below are relative to that root.

The current `work-eps4` AMFlow output is not a positive-eps^3 golden.
`examples/automatic_loop/run.wl:27` sets `epsorder = 4`; `AMFlow.m:1228-1230`
uses `leading = -2*Length[Loop]` and truncates at `leading + order`;
`automatic_loop` is one-loop at `examples/automatic_loop/run.wl:14` and `:34`.
Therefore the checked `examples/automatic_loop/sol1` and `sol2` stop at
positive `eps^2`.

The citations below are valid for the AMFlow eta=0 endpoint-transport path,
target reduction, and existing eps^2 output provenance. They are not formal
positive-eps^3 coefficient citations. A true eps^3 comparator golden requires a
fresh AMFlow run with `epsorder >= 5`.

## AMFlow Computation Path

Common audited Mathematica flow:

- `examples/automatic_loop/run.wl:13-29`: box1 family, kinematics, target list,
  `precision = 40`, `epsorder = 4`, and `SolveIntegrals`.
- `examples/automatic_loop/run.wl:33-46`: box2 family, kinematics, target list,
  and `SolveIntegrals`.
- `AMFlow.m:1156-1192`: `BlackBoxAMFlow`, target reduction, AMFlow solve, and
  reduction back to requested targets.
- `AMFlow.m:1200`: `FitEps`.
- `AMFlow.m:1203-1212`: epsilon sample generation.
- `AMFlow.m:1216-1230`: solve, fit, and Laurent truncation.
- `AMFlow.m:1039-1068`: generated `solution.wl` template.
- `examples/automatic_loop/cache/box1_amflow/1/solution.wl:17-30` and `:33-38`:
  generated box1 endpoint solve and writeout.
- `examples/automatic_loop/cache/box2_amflow/1/solution.wl:17-30` and `:33-38`:
  generated box2 endpoint solve and writeout.
- `diffeq_solver/DESolver.m:916-927`: `CalcZero`.
- `diffeq_solver/DESolver.m:1040-1042`: `SolveAsyExp` calls `CalcZero`.
- `diffeq_solver/DESolver.m:1053-1061`: `PickZeroRuleS`, selecting eta-zero
  integer-power endpoint terms.
- `diffeq_solver/DESolver.m:1065-1100`: AMFlow transport, asymptotic solve, and
  final eta-zero extraction.

Current printed eps^2-only ranges:

- `examples/automatic_loop/sol1:1-16`, `:22-43`, `:48-63`, `:63-83`.
- `examples/automatic_loop/sol2:1-21`, `:27-42`, `:42-57`, `:62-82`.

## eps^3 Endpoint Coefficients

Use `L_s = log(100) - i*pi`, `L_0 = 0`, `b_k = B_k(L_s)`,
`a_k = B_k(L_0)`, and `C_k` for the scalar-box coefficient of `eps^k`.

These are the eta=0 endpoint expressions and analytic eps^3 coefficients
multiplying positive `eps^3`.

| integral | eps^3 endpoint expression | eps^3 coefficient | AMFlow provenance |
|---|---|---|---|
| `box1[1,0,1,0]` | `b_3` | `-14.889207469091605090869757403174630151419007563776 - 12.636511439621312647507773470947285252113257880750 I` | direct master in `cache/box1_amflow/1/masters:1-2`; solve/writeout in `cache/box1_amflow/1/solution.wl:17-30`, `:33-38`; endpoint samples in `cache/box1_amflow/1/solution:2-82`; eta-zero path in `DESolver.m:1053-1061`, `:1065-1100` |
| `box1[1,1,1,1]` | `C_3` | `-0.30640183059282578896721281211588997344705274877720 + 0.12407184340912585966379076731144522397876640643259 I` | direct master in `cache/box1_amflow/1/masters:1-2`; solve/writeout in `cache/box1_amflow/1/solution.wl:17-30`, `:33-38`; endpoint samples in `cache/box1_amflow/1/solution:247-330`; eta-zero path in `DESolver.m:1053-1061`, `:1065-1100` |
| `box1[1,2,2,1]` | `(-1/50) C_3 - (3/50) C_2 - (1/25) C_1 - (1/250000) b_3 + (4/250000) b_1 - (1/25) a_3 + (4/25) a_1` | `0.27262519828546081528086032465723820635980979344426 - 0.012610798075657153629210287719885480277239741851225 I` | requested target in `run.wl:25`; Kira reduction in `cache/box1_amflow/0/results/box1/kira_target.m:8-11`; reduction application in `AMFlow.m:1181-1184` |
| `box1[2,0,1,0]` | `(-1/100) b_3 + (2/100) b_2` | `0.27783814032816039857760691968969063442118468883379 + 0.41516473710381356354550819449119042895062912763124 I` | requested target in `run.wl:25`; Kira reduction in `cache/box1_amflow/0/results/box1/kira_target.m:2-3`; reduction application in `AMFlow.m:1181-1184` |
| `box2[1,-1,1,0]` | `-50 b_3` | `744.46037345458025454348787015873150757095037818878 + 631.82557198106563237538867354736426260566289403749 I` | requested target in `run.wl:44`; Kira reduction in `cache/box2_amflow/0/results/box2/kira_target.m:9-10`; reduction application in `AMFlow.m:1181-1184` |
| `box2[1,0,1,0]` | `b_3` | `-14.889207469091605090869757403174630151419007563776 - 12.636511439621312647507773470947285252113257880750 I` | direct master in `cache/box2_amflow/1/masters:1-2`; solve/writeout in `cache/box2_amflow/1/solution.wl:17-30`, `:33-38`; endpoint samples in `cache/box2_amflow/1/solution:2-82`; eta-zero path in `DESolver.m:1053-1061`, `:1065-1100` |
| `box2[1,1,1,1]` | `C_3` | `-0.30640183059282578896721281211588997344705274877720 + 0.12407184340912585966379076731144522397876640643259 I` | direct master in `cache/box2_amflow/1/masters:1-2`; solve/writeout in `cache/box2_amflow/1/solution.wl:17-30`, `:33-38`; endpoint samples in `cache/box2_amflow/1/solution:247-330`; eta-zero path in `DESolver.m:1053-1061`, `:1065-1100` |
| `box2[2,1,1,1]` | `(1/100) C_3 + (2/100) C_2 + (1/50) a_3 - (1/50) a_2 - (3/50) a_1 + (3/50) a_0 - 3/50` | `-0.10580576277332882123295340750889518082220683383191 + 0.0039840593386604417482721130452207816531211457412375 I` | requested target in `run.wl:44`; Kira reduction in `cache/box2_amflow/0/results/box2/kira_target.m:2-4`; reduction application in `AMFlow.m:1181-1184` |

## Analytic Endpoint Structure

Use `epsilon = eps`, `d = 4 - 2 eps`, `s = 100`, retained `NegIm` branch
`L_s = log(100) - i*pi`, and `L_0 = 0`. Let `gamma` denote EulerGamma and
`zeta_n = Zeta[n]`.

### Bubble Master `[1,0,1,0]`

Primitive retained bubble endpoint:

`B(L, eps) = exp(-eps L) Gamma(1 + eps) Gamma(1 - eps)^2 / (eps Gamma(2 - 2 eps))`.

Write:

`B(L, eps) = eps^-1 exp(A_1 eps + A_2 eps^2 + A_3 eps^3 + A_4 eps^4 + O(eps^5))`

with:

`A_1(L) = 2 - gamma - L`

`A_2 = 2 - zeta_2 / 2`

`A_3 = 8/3 - 7 zeta_3 / 3`

`A_4 = 4 - 13 zeta_4 / 4`.

Then:

`B(L, eps) = eps^-1 + B_0(L) + B_1(L) eps + B_2(L) eps^2 + B_3(L) eps^3 + O(eps^4)`

where:

`B_0(L) = A_1`

`B_1(L) = A_2 + A_1^2 / 2`

`B_2(L) = A_3 + A_1 A_2 + A_1^3 / 6`

`B_3(L) = A_4 + A_1 A_3 + A_2^2 / 2 + A_1^2 A_2 / 2 + A_1^4 / 24`.

### Scalar Box Master `[1,1,1,1]`

Primitive retained scalar box endpoint:

`Box(eps) = -1/50 eps^-2 R(eps) [T(L_s, -99; eps) + T(L_0, 99/100; eps)]`.

Here:

`R(eps) = Gamma(1 + eps) Gamma(1 - eps)^2 / Gamma(1 - 2 eps)`

`log R(eps) = -gamma eps - zeta_2 eps^2/2 - 7 zeta_3 eps^3/3 - 13 zeta_4 eps^4/4 - 31 zeta_5 eps^5/5 + O(eps^6)`

`T(L, z; eps) = exp(-eps L) H(z; eps)`

`H(z; eps) = 1 - eps Li_1(z) - eps^2 Li_2(z) - eps^3 Li_3(z) - eps^4 Li_4(z) - eps^5 Li_5(z) + O(eps^6)`.

Because the scalar box carries `eps^-2`, the positive-eps^3 coefficient
requires the product through weight 5. The retained-point basis is:

- powers of `L_s = log(100) - i*pi`;
- `EulerGamma`;
- `zeta_2`, `zeta_3`, `zeta_4`, `zeta_5`;
- `Li_n(-99)` and `Li_n(99/100)` for `n = 1..5`.

`Li_1(-99) = -log(100)` and `Li_1(99/100) = log(100)`.

## New BigFloat Constants

New constants beyond the eps^2 implementation are exactly:

- `Zeta5Constant() = 1.036927755143369926331365486457034168057080919501912811974192677903804`
- `Li5Minus99Constant() = -52.38666235360439053364949682271734882996050351431037699252166679541238`
- `Li5NinetyNineOverHundredConstant() = 1.026110477101306182550422778135012186829770445620236490312452902605756`

These must be precomputed to at least 60 digits. Do not add an independent
`log(2)` constant for this direct retained basis.

## C++ Implementation Contract

Implementation should be confined to the retained endpoint helpers in
`src/cli/main.cpp` unless a later lane deliberately widens scope. The current
eps^2 helper region is `src/cli/main.cpp:2782-3003`, with call sites around
`:3096-3098`.

Existing eps^2 helpers to extend or generalize:

- `RetainedBubbleEndpointSeriesThroughEps2` at `src/cli/main.cpp:2814`.
- `MultiplySeriesThroughWeight4` at `src/cli/main.cpp:2832`.
- `RetainedScalarBoxTSeriesThroughWeight4` at `src/cli/main.cpp:2846`.
- `RetainedScalarBoxGammaRatioSeriesThroughWeight4` at `src/cli/main.cpp:2869`.
- `RetainedMasslessBoxEndpointSeriesThroughEps2` at `src/cli/main.cpp:2886`.
- `ApplyRetainedAutomaticLoopEtaZeroBranchLogTransportThroughEps2` at
  `src/cli/main.cpp:2947`.
- `ApplyRetainedAutomaticLoopEtaZeroBoxEndpointTransportThroughEps2` at
  `src/cli/main.cpp:2983`.

Required changes:

- Add `Zeta5Constant()`, `Li5Minus99Constant()`, and
  `Li5NinetyNineOverHundredConstant()`.
- Extend the bubble helper to return Laurent orders `-1..3`, using
  `A_4 = 4 - 13 zeta_4 / 4` and the `B_3(L)` formula above.
- Replace fixed weight-4 arrays with order-parametric series helpers, or at
  minimum weight-5 helpers for eps^3.
- Extend `RetainedScalarBoxTSeriesThroughWeight4` to include the `Li_5` term and
  become `ThroughWeight5` or order-parametric.
- Extend the scalar-box gamma-ratio helper with `r_5 = -31 zeta_5 / 5`. Prefer
  an exponential-series recurrence over handwritten coefficients.
- Extend the scalar-box endpoint helper to return Laurent orders `-2..3`,
  mapping product weight `w` to Laurent order `w - 2`.
- Rename misleading `ThroughEps2` entry points to `ThroughEps3`, or preferably
  generalize to `TransportThroughEpsOrder(int N)` /
  `EndpointSeriesThroughEpsOrder(int N)`.
- Upsert only primitive endpoint masters `<family>[1,0,1,0]` and
  `<family>[1,1,1,1]`; let existing Kira reductions propagate dependent targets.
- Do not hardcode the eight target coefficients in C++.

Recommended API direction:

`TransportThroughEpsOrder(int N)`

Support only reviewed `N <= 3` initially. The guard should fail closed for
`N > 3` until weight-6 constants and formulas are reviewed.

## Verification Plan

The existing `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/work-eps4/`
capture is sufficient for eps^2 regression provenance, but not sufficient as an
eps^3 golden.

Required eps^3 golden capture:

1. Rerun AMFlow `automatic_loop` with `epsorder >= 5`, since one-loop
   `leading = -2` and `AMFlow.m:1228-1230` truncates at `leading + order`.
2. Preserve the same kinematics: box1 `s -> 100, t -> -1` from
   `examples/automatic_loop/run.wl:19`; box2 `s -> 100, t -> -99` from `:39`.
3. Prefer precision at least 50 digits, and use 60 digits if the parity target
   remains 40+ matching digits.
4. Store the new capture under a distinct eps^3 manifest/path so the comparator
   cannot silently reuse eps^2-only `sol1`/`sol2`.

Comparator gate:

1. Re-run the existing eps^2 parity check and require the known 54/54 result at
   the established digit threshold to remain passing.
2. Run C++ retained `automatic_loop` with requested positive `eps_order >= 3`.
3. Compare against the new AMFlow eps^3 golden using the existing family aliases
   if needed, for example `box1_amflow=box1` and `box2_amflow=box2`.
4. Require all coefficients through positive `eps^3` to match, with a minimum
   target of 30 decimal digits and preferred target of 40 decimal digits if the
   AMFlow golden precision supports it.
5. Focus-check the primitive eps^3 values `box{1,2}[1,0,1,0] = b_3` and
   `box{1,2}[1,1,1,1] = C_3`.
6. Focus-check dependent reductions using the four formulas in the coefficient
   table.

The current cached epsilon sample vectors may be useful for auxiliary sanity
checks, but they are numeric samples, not printed eps^3 Laurent coefficients,
and must not be treated as the formal golden.

## Refactor Recommendation

Refactor now to a local `TransportThroughEpsOrder(int N)` design for retained
endpoint transport, with explicit support only through `N = 3`.

Justification: eps^3 already forces the implementation to replace fixed
weight-4 scalar-box arrays with weight-5 or dynamic series logic. Doing the
local order-parametric helper now avoids adding `ThroughEps3` only to repeat the
same mechanical change for eps^4. The scope should remain narrow: endpoint
helper names, series construction, constants, and call sites. Defer broader
solver-pipeline, diagnostics-schema, and comparator redesign until eps^4 or a
separate cleanup lane.

## Consensus

The eps^3 endpoint transport should extend the reviewed eps^2 analytic primitive
endpoint model by one weight: bubble through `B_3(L)` and scalar box through
weight 5 with `Li_5` and `zeta_5`. The implementation lane should add only the
three new BigFloat constants listed above, generalize the local retained
endpoint helpers through requested positive order, upsert primitive endpoint
masters, and let Kira reductions produce dependent top-sector targets.

A fresh AMFlow `epsorder >= 5` capture is mandatory before claiming eps^3
parity. The current `work-eps4` files are valid provenance for the computation
path and eps^2 regression, but not an eps^3 golden.
