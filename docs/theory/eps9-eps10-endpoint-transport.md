# eps^9 / eps^10 Endpoint Transport Theory Design

Status: peer-reviewed consensus design for extending the retained
`automatic_loop` eta=0 endpoint transport through positive `eps^9` and
`eps^10`. This is a design document only. It is suitable for an implementation
lane after a fresh AMFlow recapture is available.

## Review Approval

This final consensus document synthesizes:

- Role A physicist-implementer draft:
  `/tmp/autoibp_orch/exec/lane41_roleA.md`
- Role B independent physicist-reviewer draft:
  `/tmp/autoibp_orch/exec/lane41_roleB.md`
- Role C Mathematica-source audit:
  `/tmp/autoibp_orch/exec/lane41_roleC.md`
- Role D synthesis:
  `/tmp/autoibp_orch/exec/lane41_roleD.md`

Approval scope:

- Role A approved the analytic eps^9/eps^10 extension, while flagging the
  zero-log guard needed for a full 12-integral eps^10 comparator surface.
- Role B independently reproduced the same endpoint structure, constants,
  implementation contract, and AMFlow recapture requirement.
- Role C approved the source-line map and verified that the latest local AMFlow
  capture does not contain positive eps^9 or eps^10 coefficients.
- Role D approved the analytic design, but blocked any current AMFlow parity
  claim until a fresh `epsorder >= 12` recapture exists.

## Source-Root And Golden Caveat

Audited current AMFlow root:

`/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/work-eps10/amflow/`

All AMFlow paths below are relative to that root unless absolute paths are
shown.

The current `work-eps10` AMFlow output is not a positive eps^9 or eps^10 golden.
`examples/automatic_loop/run.wl:27` sets `epsorder = 10`; `AMFlow.m:1227-1230`
uses `leading = -2*Length[Loop]` and truncates at `leading + order`. The
`automatic_loop` example is one-loop, with `Loop = {l}` at
`examples/automatic_loop/run.wl:14` and `:34`, so the current capture prints
through positive `eps^8`.

Role C confirmed that the latest fitted outputs stop at eps^8:

- box1 bubble `j[box1,1,0,1,0]`: `examples/automatic_loop/sol1:126-171`;
- box1 top-sector scalar box `j[box1,1,1,1,1]`:
  `examples/automatic_loop/sol1:171-221`;
- box2 bubble `j[box2,1,0,1,0]`: `examples/automatic_loop/sol2:108-153`;
- box2 top-sector scalar box `j[box2,1,1,1,1]`:
  `examples/automatic_loop/sol2:164-214`.

The citations below are therefore citations for the AMFlow endpoint-transport
path, target reductions, vector ordering, and currently audited capture depth.
The eps^9/eps^10 expressions are analytic endpoint-design targets until they
are validated by a fresh `epsorder = 12` AMFlow run.

## AMFlow Computation Path

Common audited Mathematica flow:

- `examples/automatic_loop/run.wl:13-29`: box1 family, kinematics, target list,
  `precision = 40`, `epsorder = 10`, and `SolveIntegrals`.
- `examples/automatic_loop/run.wl:33-46`: box2 family, kinematics, target list,
  and `SolveIntegrals`.
- `AMFlow.m:1156-1192`: `BlackBoxAMFlow`, target reduction, AMFlow solve, and
  reduction back to requested targets.
- `AMFlow.m:1200-1230`: epsilon-sample generation, `FitEps`, and final Laurent
  truncation.
- `AMFlow.m:1039-1068`: generated `solution.wl` template.
- `examples/automatic_loop/cache/box1_amflow/1/solution.wl:17-30` and `:33-38`:
  generated box1 endpoint solve and writeout.
- `examples/automatic_loop/cache/box2_amflow/1/solution.wl:17-30` and `:33-38`:
  generated box2 endpoint solve and writeout.
- `diffeq_solver/DESolver.m:916-927`: `CalcZero`.
- `diffeq_solver/DESolver.m:1040-1042`: `SolveAsyExp` calls `CalcZero`.
- `diffeq_solver/DESolver.m:1053-1061`: `PickZeroRuleS`, selecting eta-zero
  integer-power endpoint terms.
- `diffeq_solver/DESolver.m:1065-1100`: AMFlow transport, endpoint asymptotic
  solve, and final eta-zero extraction.

AMFlow/Kira ordering and reduction provenance:

- `ibp_interface/Kira/interface.m:410-432`: `AnalyticReduction` reads
  `kira_target.m`, builds coefficient arrays against the master list, applies
  `d -> 4 - 2 ep`, appends identity master rows, and returns reduction rules.
- `ibp_interface/Kira/interface.m:436-460`: `DifferentialEquation` sorts
  masters and permutes the DE matrices consistently.
- `AMFlow.m:1181-1184`: solved master vectors are transposed in master order
  and target reductions are applied by matrix multiplication over epsilon
  samples.

## Notation

Use `epsilon = eps`, `d = 4 - 2 eps`, retained `NegIm` branch
`L_s = log(100) - i*pi`, and zero-log channel `L_0 = 0`.

Define:

- `b_n = B_n(L_s)`, the coefficient of `eps^n` in the branch-log bubble.
- `a_n = B_n(L_0)`, the coefficient of `eps^n` in the zero-log bubble.
- `C_n`, the coefficient of `eps^n` in the retained scalar-box endpoint.
- `a_-1 = b_-1 = 1`, from the bubble pole.

The strict top-sector endpoint rows are:

- primitive scalar-box masters: `box1[1,1,1,1]`, `box2[1,1,1,1]`;
- top-sector reduced requested rows: `box1[1,2,2,1]`,
  `box2[2,1,1,1]`.

## eps^9 / eps^10 Top-Sector Endpoint Expressions

The direct primitive top-sector master expressions are:

| integral | eps^9 expression | eps^10 expression | analytic coefficient target |
| --- | --- | --- | --- |
| `box1[1,1,1,1]` | `C_9` | `C_10` | eps^9 `-0.824323435389015517076867222208622085321029547815021317135335524498706 + 0.1252542379017241010817189612709844586287192147988150988260935823114777 I`; eps^10 `-0.9023607307025446856939796817769381710724968709405719313585928151707907 + 0.1248174329721514501825538673586015515706343426590051416453600326660559 I` |
| `box2[1,1,1,1]` | `C_9` | `C_10` | same as `box1[1,1,1,1]` |

AMFlow provenance for these primitive rows:

- box1 direct-system master list:
  `examples/automatic_loop/cache/box1_amflow/1/masters:1-2`;
- box2 direct-system master list:
  `examples/automatic_loop/cache/box2_amflow/1/masters:1-2`;
- generated endpoint solve and writeout:
  `examples/automatic_loop/cache/box1_amflow/1/solution.wl:17-38` and
  `examples/automatic_loop/cache/box2_amflow/1/solution.wl:17-38`;
- eta-zero extraction:
  `diffeq_solver/DESolver.m:1053-1061`, `:1065-1100`.

The reduced top-sector requested-row expressions are:

| integral | eps^9 expression | eps^10 expression | analytic coefficient target |
| --- | --- | --- | --- |
| `box1[1,2,2,1]` | `-C_9/50 - 3 C_8/50 - C_7/25 - b_9/250000 + b_7/62500 - a_9/25 + 4 a_7/25` | `-C_10/50 - 3 C_9/50 - C_8/25 - b_10/250000 + b_8/62500 - a_10/25 + 4 a_8/25` | eps^9 `0.3506234760308494007164321826054930915383606543469009111512467701743581 - 0.01518879049293318965738013864558614647821128136665671813123774780540956 I`; eps^10 `0.3189265209388786482135014851682014833750935694031685393840261204172018 - 0.0150577987242137205433374216985381674552283598572371199733167929279114 I` |
| `box2[2,1,1,1]` | `C_9/100 + C_8/50 + a_9/50 - a_8/50 - 3a_7/50 + 3a_6/50 - 3a_5/50 + 3a_4/50 - 3a_3/50 + 3a_2/50 - 3a_1/50 + 3a_0/50 - 3/50` | `C_10/100 + C_9/50 + a_10/50 - a_9/50 - 3a_8/50 + 3a_7/50 - 3a_6/50 + 3a_5/50 - 3a_4/50 + 3a_3/50 - 3a_2/50 + 3a_1/50 - 3a_0/50 + 3/50` | eps^9 `-0.1778570661901553362271522438584409559747515232757363276181083013596571 + 0.003776029276392182217792783245232009410396068342535257691990749960257222 I`; eps^10 `0.01849698575846618077305995824349536607222390353171034505116902922330648 + 0.003753259087755996523459917899005704688280727722566353392975471972890113 I` |

AMFlow/Kira provenance:

- box1 target list: `examples/automatic_loop/run.wl:25`;
- box1 Kira reduction:
  `examples/automatic_loop/cache/box1_amflow/0/results/box1/kira_target.m:8-11`;
- box2 target list: `examples/automatic_loop/run.wl:44`;
- box2 Kira reduction:
  `examples/automatic_loop/cache/box2_amflow/0/results/box2/kira_target.m:2-4`;
- reduction application: `AMFlow.m:1181-1184`.

These formulas use:

- `(-d^2 + 11d - 30)/100 = -1/50 - 3 eps/50 - eps^2/25`;
- `(d^2 - 8d + 15) = -1 + 4 eps^2`;
- `(-d + 5)/100 = 1/100 + eps/50`;
- `(d^2 - 8d + 15)/(25d - 150) =
  1/50 - eps/50 - 3 eps^2/50 + 3 eps^3/50 - ...`.

## Full Retained Target Surface

The established retained `automatic_loop` comparator surface compares 12
integrals. If an implementation lane keeps that full surface for eps^9/eps^10,
the remaining requested and direct endpoint rows follow:

| integral | eps^9 expression | eps^10 expression |
| --- | --- | --- |
| `box1[1,0,1,0]` | `b_9` | `b_10` |
| `box1[0,1,0,1]` | `a_9` | `a_10` |
| `box1[2,0,1,0]` | `-b_9/100 + b_8/50` | `-b_10/100 + b_9/50` |
| `box1[-2,1,1,2]` | `-20000 a_10 + 201 a_9 - a_8` | `-20000 a_11 + 201 a_10 - a_9` |
| `box2[1,0,1,0]` | `b_9` | `b_10` |
| `box2[0,1,0,1]` | `a_9` | `a_10` |
| `box2[1,1,0,1]` | `-a_10 + 2 a_9` | `-a_11 + 2 a_10` |
| `box2[1,-1,1,0]` | `-50 b_9` | `-50 b_10` |

The two rows containing `a_11` are not top-sector rows, but they are part of
the existing full retained comparator surface. They require an internal
zero-log bubble guard one order above the public request. Without this guard,
full all-12 eps^10 comparison is expected to fail in `box1[-2,1,1,2]` and
`box2[1,1,0,1]`.

Kira provenance:

- `box1[2,0,1,0]`: `kira_target.m:2-3`,
  `(-d+3)/100 = -1/100 + eps/50`;
- `box1[-2,1,1,2]`: `kira_target.m:5-6`,
  `(d^2+394d+78408)/(2d-8) = -20000/eps + 201 - eps`;
- `box2[1,1,0,1]`: `kira_target.m:6-7`,
  `(2d-6)/(d-4) = -1/eps + 2`;
- `box2[1,-1,1,0]`: `kira_target.m:9-10`, coefficient `-50`.

## Analytic Endpoint Structure

### Bubble Master

Primitive retained bubble endpoint:

`B(L, eps) = exp(-eps L) Gamma(1 + eps) Gamma(1 - eps)^2 / (eps Gamma(2 - 2 eps))`.

Write:

`B(L, eps) = eps^-1 exp(sum_{n>=1} A_n(L) eps^n)`.

Then:

- `A_1(L) = 2 - EulerGamma - L`;
- for `n >= 2`, `A_n = 2^n/n - c_n zeta_n/n`;
- `c_n = 2^n - 1` for odd `n`;
- `c_n = 2^n - 3` for even `n`.

For public eps^9 and eps^10:

- eps^9 needs branch-log bubble weight 10:
  `A_10 = 1024/10 - 1021 zeta_10/10`;
- eps^10 needs branch-log bubble weight 11:
  `A_11 = 2048/11 - 2047 zeta_11/11`.

Numerically:

- `A_10 = 0.19845387944977348707739759627742836368540582726684548797974364763464140798968736...`;
- `A_11 = -0.0010549156938676319694156665167236319306703633684581129893199724373327642687236997...`.

For a full 12-integral eps^10 comparator, also provide internal zero-log guard
weight 12:

- `A_12 = 4096/12 - 4093 zeta_12/12`
  `= 0.16606397810917985947288949921679389658474582844933877052194857654995695348465357419...`.

### Scalar Box Master

Primitive retained scalar-box endpoint:

`Box(eps) = -1/50 eps^-2 R(eps) [T(L_s, -99; eps) + T(L_0, 99/100; eps)]`.

Here:

- `R(eps) = Gamma(1 + eps) Gamma(1 - eps)^2 / Gamma(1 - 2 eps)`;
- `log R(eps) = -EulerGamma eps - sum_{n>=2} c_n zeta_n eps^n/n`;
- `T(L, z; eps) = exp(-eps L) H(z; eps)`;
- `H(z; eps) = 1 - sum_{n>=1} eps^n Li_n(z)`.

Because the scalar box carries `eps^-2`, positive eps^9 needs product weight
11 and positive eps^10 needs product weight 12. The retained-point analytic
basis is:

- powers of `L_s = log(100) - i*pi`;
- `EulerGamma`;
- `zeta_n` through `zeta_12`;
- `Li_n(-99)` and `Li_n(99/100)` through `n = 12`.

The new gamma-ratio log weights are:

- weight 11: `-2047 zeta_11 / 11`;
- weight 12: `-4093 zeta_12 / 12`.

No independent `log(2)` constant is required for this direct retained basis.
That matches the eps^2 and eps^3 endpoint theory decisions.

## New BigFloat Constants

Add or review these constants with at least 60 significant decimal digits.
Using around 100 digits keeps the current `cpp_dec_float_100` margin consistent
with the existing `Li9` and `Li10` constants.

```text
Zeta11Constant =
1.00049418860411946455870228252646993646860643575820861711914143610005405979821981470259184302356063

Zeta12Constant =
1.000246086553308048298637998047739670960416088458003404533040952133252019681940913049042808551900699

Li11Minus99Constant =
-95.94475042926686243705900658765214484333960551469067910854460595518692129877783751549097403827834293

Li11NinetyNineOverHundredConstant =
0.9904842935368423654949405909839237400726502620852199952610101082857955521475173935684635379900631966

Li12Minus99Constant =
-97.29646765714049961892582889612343082322827407975061397327992152191519559915728564377122688234847276

Li12NinetyNineOverHundredConstant =
0.9902411696844198432561406494614234339241430808875851724807984633004269624005375582871990145629041027
```

`Zeta10Constant`, `Li10Minus99Constant`, and
`Li10NinetyNineOverHundredConstant` already exist for the eps^8 scalar-box
surface and the internal eps^9 zero-log guard. eps^9/eps^10 public support
promotes the existing `A_10` use from internal guard data to reviewed public
bubble support and adds `A_11`; the full comparator guard adds `A_12`.

## C++ Implementation Contract

Implementation should remain local to the retained automatic-loop endpoint
helpers in `src/cli/main.cpp`. Do not hardcode target endpoint values. Upsert
primitive endpoint series and let the retained Kira reduction machinery
propagate dependent target rows.

Current helper locations:

- `src/cli/main.cpp:1796`: `BigFloat = cpp_dec_float_100`;
- `src/cli/main.cpp:3119-3186`: existing `zeta9`, `zeta10`, `Li9`, and `Li10`
  constants;
- `src/cli/main.cpp:3193-3211`: public endpoint guard and internal zero-log
  bubble guard;
- `src/cli/main.cpp:3213-3245`: order-parametric multiplication and exponential
  helpers;
- `src/cli/main.cpp:3247-3316`: bubble endpoint helper;
- `src/cli/main.cpp:3318-3396`: scalar-box polylog tables;
- `src/cli/main.cpp:3419-3462`: scalar-box gamma-ratio helper;
- `src/cli/main.cpp:3464-3495`: scalar-box endpoint helper with
  `max_weight = epsilon_order + 2`;
- `src/cli/main.cpp:3570-3583`: zero-log bubble guard;
- `src/cli/main.cpp:3609-3633`: `TransportThroughEpsOrder`.

Required public eps^9/eps^10 changes:

- Add `Zeta11Constant()` and `Zeta12Constant()`.
- Add `Li11Minus99Constant()`, `Li11NinetyNineOverHundredConstant()`,
  `Li12Minus99Constant()`, and `Li12NinetyNineOverHundredConstant()`.
- Raise `kMaxReviewedEndpointTransportEpsilonOrder` from `8` to `10` and update
  the guard message to `0..10`.
- Keep the scalar-box helper shape unchanged; eps^9 and eps^10 correspond to
  `max_weight = 11` and `max_weight = 12`.
- Extend both scalar-box polylog tables through weights 11 and 12.
- Extend the scalar-box gamma-ratio log table through weights 11 and 12.
- Extend the bubble log table through `A_11` for public eps^10 primitive
  support.
- For the established full 12-integral comparator surface, extend the separate
  zero-log bubble guard to public max plus one, `0..11`, and add `A_12`.
- Keep `TransportThroughEpsOrder(int N)` fail-closed for `N > 10`.
- Do not change comparator tolerance, output suppression, or status semantics.

Precision contract:

- The current endpoint path uses `boost::multiprecision::cpp_dec_float_100`,
  which should have enough headroom for 40-digit solve output and the existing
  30-digit comparator threshold at eps^9/eps^10.
- There is no current evidence that polylog precision breaks at eps^9 or
  eps^10. The new `Li11` and `Li12` constants should nevertheless be
  independently regenerated before implementation.
- If the fresh epsorder-12 comparison loses digits only in high-order reduced
  rows, especially `box1[-2,1,1,2]`, first test higher internal precision before
  changing formulas. A reasonable escalation target is 150 to 200 decimal
  digits for the endpoint helper, or an MPFR backend with at least 512 bits
  during the endpoint transport calculation.

## Verification Plan

Mirror the Lane 30 and Lane 35 recapture pattern:

1. Create a fresh `work-eps12` AMFlow tree from the latest clean automatic-loop
   capture layout.
2. Remove stale `examples/automatic_loop/sol1`, `sol2`, and copied cache state
   before running.
3. Set `examples/automatic_loop/run.wl:27` to `epsorder = 12`.
4. Preserve kinematics and target lists from `run.wl:13-29` and `:33-46`.
5. Run under a guarded SLURM job with WolframKernel, Kira, and Fermat metadata
   recorded, matching the provenance style of the eps7/eps8 capture.
6. Require kernel exit 0 and empty stderr.
7. Record run logs, host, toolchain, `sol1`/`sol2` sha256 values, and direct
   parser ingest counts.
8. Expected per-order counts for full retained output: `eps^-2: 6`,
   `eps^-1: 12`, and `eps^0` through `eps^10: 12` each.
9. Add `automatic_loop.eps9-golden-manifest.json` and
   `automatic_loop.eps10-golden-manifest.json` pointing at the real
   `epsorder = 12` outputs.

Implementation verification gates for a future code lane:

- Before implementation, confirm `--eps-order 9` fails honestly under the
  current reviewed guard `0..8`.
- Build `amflow-cli` and `amflow-tests`.
- Run `./build/amflow-tests`.
- Run `ctest --test-dir build --output-on-failure`.
- Run `python3 tools/reference-harness/scripts/compare_cpp_vs_amflow.py --self-check`.
- Compare `--eps-order 9` and `--eps-order 10` against the fresh manifests at
  unchanged `--tolerance-digits 30`.
- Preserve lower-order automatic-loop surfaces through eps^8.
- Anti-fake audit: grep for hardcoded eps9/eps10 target values, hardcoded zero
  payloads, output suppression, comparator tolerance changes, and sentinel-only
  agreement.

## Convergence And Special-Structure Assessment

For this retained one-loop massless box endpoint slice, there is no finite eps
order where elliptic, modular, or other non-classical structures appear. The
all-order endpoint basis remains gamma functions, zeta values, powers of
`log(100) - i*pi`, and classical polylogarithms `Li_n(-99)` and
`Li_n(99/100)` at increasing weight.

The practical growth and special-handling points are:

- scalar-box eps^N needs polylog and gamma-ratio weights `N + 2`;
- full retained-target eps^N needs zero-log bubble data through `a_{N+1}`
  because of the two `1/eps` Kira prefactors;
- reduced rows with large rational multipliers, especially
  `box1[-2,1,1,2] = (-20000/eps + 201 - eps) a`, amplify coefficient and
  precision errors;
- AMFlow's `GenerateNumericalConfig` has a source-level sample-count abort when
  the generated sample count exceeds 100 (`AMFlow.m:1203-1208`), so much higher
  recaptures may need source or workflow adjustments even though the analytic
  endpoint basis is still classical.

As a function of epsilon, the gamma/polylog representation has the usual nearby
gamma-function singularities and logarithmic branch choices. That is not a
blocker for the tiny epsilon samples used by AMFlow or for the formal Laurent
coefficients through eps^10. The first required special handling for the current
roadmap is not a new analytic function class; it is the zero-log guard and
precision discipline described above.

## Consensus Verdict

APPROVE implementation planning for eps^9 and eps^10 endpoint transport as an
analytic extension of the retained eps^8 model.

BLOCK any claim of current AMFlow eps^9/eps^10 parity. The current capture stops
at positive eps^8.

BLOCK full all-12-integral eps^10 parity unless the internal zero-log bubble
guard is extended through `a_11`, requiring `A_12` and `zeta_12`.
