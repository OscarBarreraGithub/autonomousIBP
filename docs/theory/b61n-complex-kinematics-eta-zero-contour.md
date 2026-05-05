# B61n Complex-Kinematics Eta=0 Contour Transport Theory Design

Status: peer-reviewed design for a future implementation lane. This document does
not claim current b61n closure.

## Review Approval

This lane used the requested four-role review:

- Role A, physicist-implementer: APPROVE implementation planning for a
  complex-kinematics endpoint design, with the caveat that retained solution
  samples cannot close b61n.
- Role B, independent reviewer: APPROVE only a live AMFlow-like contour runtime
  contract; BLOCK any retained-solution-sample parity claim.
- Role C, Mathematica-source auditor: APPROVE the source-root caveat and
  alternative citations below.
- Role D, synthesizer: APPROVE this as a planning/contract document; BLOCK
  current closure evidence based on retained samples.

Role C's provenance correction is mandatory: the prompt-named source root

`/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/work/amflow/examples/complex_kinematics/`

does not exist on this node and is not cited as existing. The canonical audited
upstream source is recorded in
`/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/work/manifests/phase0-reference.json:43-50`
as
`/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260422-user-hook-pair/inputs/upstream/amflow/`.
The positive-order recapture under
`/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/work-complex-eps6-lane25/amflow/`
is valid verification evidence, but it is a recapture with `epsorder = 6` and
`NThread = 8`, not the original upstream source with `epsorder = 2` and
`NThread = 4`.

## Structural Differences

The reviewed `automatic_loop` eta=0 transport is selected primitive endpoint
coefficient transport. Its theory docs use fixed retained kinematics,
`L_s = log(100) - i*pi`, massless bubble and scalar-box endpoint formulas, and
order-parametric endpoint helpers for the `box1`/`box2` automatic-loop slice
(`docs/theory/eps2-endpoint-transport.md:138-227`,
`docs/theory/eps9-eps10-endpoint-transport.md:293-333`). The live C++ guard is
hard-coded to `benchmark_id == "automatic_loop"`, families `box1`/`box2`,
`NegIm`, and singular points `eta=0` and `eta=100`
(`src/cli/main.cpp:4733-4740`). It is not a general complex eta-contour
executor.

`complex_kinematics` is a single one-loop `box` family. The audited upstream
source sets legs `{p1,p2,p3,p4}`, conservation `p4 -> -p1-p2-p3`, replacements
`p1^2 -> 0`, `p2^2 -> 0`, `p3^2 -> p3sq`, `p4^2 -> p4sq`,
`(p1+p2)^2 -> s`, `(p1+p3)^2 -> t`, propagators
`{l^2, (l+p1)^2, (l+p1+p2)^2, (l+p1+p2+p4)^2-m3sq}`, numeric point
`{s -> 496, t -> -39, p3sq -> 7/2, p4sq -> 8, m3sq -> 2-I}`, and target
`j[box,1,1,1,1]`
(`/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260422-user-hook-pair/inputs/upstream/amflow/examples/complex_kinematics/run.wl:13-29`).
The current generated eta system inserts eta into the massive line as
`-eta - m3sq + (l - p3)^2` and carries the same numeric point
(`/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/work/generated-config/phase0/complex_kinematics/primary/cache/box_amflow/1/config:1-7`).

In the audited source the external invariants are real and the mass parameter is
complex. The implementation must nevertheless treat this as complex kinematics:
numeric invariants and masses must be parsed as `BigComplex`, the DE matrix
contains complex rational expressions with `I`, pole locations are complex, and
no real-axis pole ordering or real-only branch choice is valid. If a future
recapture promotes external invariant values themselves to complex numbers, the
same contract applies.

The s-channel branch cut remains a real physics feature because `s=496 > 0`.
With AMFlow's `NegIm` direction, the branch convention for the massless
s-channel bubble is `Log(-s-i0) = log(s) - i*pi`. The retained `box[1,0,1,0]`
AMFlow solution has finite imaginary part `+pi`, consistent with
`2 - EulerGamma - (log(496) - i*pi)`
(`/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/work-complex-eps6-lane25/amflow/examples/complex_kinematics/sol:76-86`).
Complex-mass thresholds add additional log/dilog branch points, so the branch
ledger must record the contour half-plane and any winding around complex poles
instead of relying on a single fixed `log(100) - i*pi` constant.

## Current Runtime Boundary

The committed `complex_kinematics.amflow-state.json` enables retained solution
samples and says C++ can fit those samples directly
(`tools/reference-harness/specs/phase0/complex_kinematics.amflow-state.json:127-130`,
`:336-339`). The current C++ retained-sample path parses
`boundary_state.files.solution`, fits epsilon samples, and explicitly appends
that full complex eta-contour endpoint reconstruction remains deferred
(`src/cli/main.cpp:5412-5582`, especially `:5557-5560`).

The non-sample eta-infinity path can parse boundary regions, boundary master
samples, and apply a first eta-infinity asymptotic DE transport before fitting
Laurent coefficients (`src/cli/main.cpp:5585-5664`). It then calls the
automatic-loop-only selected endpoint transport. JSON serialization can expose
`transport_scope`, `full_eta_zero_contour_applied`,
`eta_zero_endpoint_transport_applied`, and blocked reasons
(`src/cli/main.cpp:6448-6525`), but a retained-solution-sample result is
reported as `loop-solution-samples`, not full eta-zero contour execution.

Therefore b61n cannot be closed by another retained-sample fit, by hardcoded
endpoint constants, or by setting flags after cached-sample parity. The
qualifying path must execute an eta-contour and endpoint extraction from the
AMFlow state ingredients before any final solution samples are read.

## Eta=0 Analytic Structure

For each epsilon sample, AMFlow solves a complex rational system

`dF/deta = A(eta, eps) F`

from eta infinity to the endpoint. The endpoint theory is the local eta=0
expansion of this transported solution. In the reviewed subset, write the local
system near eta=0 as

`A(eta, eps) = A_-1(eps)/eta + A_0(eps) + A_1(eps) eta + ...`.

A qualifying implementation must construct a local model

`F(eta, eps) = H(eta, eps) eta^R(eps) C(eps)`,

where `R` is the residue/exponent data, `H(0, eps)` is nonsingular, and the
chosen value of `eta^R` uses the recorded `NegIm` endpoint branch. If the
residue is absent for the current generated matrix, this reduces to an ordinary
Taylor endpoint with `R = 0`; the same extraction code should still run and
record that the endpoint is regular after contour transport. If the local model
has unsupported resonances, fractional powers, multiple integer eta-zero terms,
or untracked logs, it must fail closed.

AMFlow's Mathematica path is the reference semantics. `SolveIntegrals` generates
epsilon samples, calls `BlackBoxAMFlow`, fits epsilon, and truncates the Laurent
series (`AMFlow.m:1203-1230` under the positive-order recapture root). The
generated `solution.wl` sets `RunDirection -> direction`, evaluates boundary
conditions, calls `AMFlow[de, bc]`, and writes master samples
(`/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/work-complex-eps6-lane25/amflow/examples/complex_kinematics/cache/box_amflow/1/solution.wl:17-38`).
`DESolver.m` computes the endpoint model through `CalcZero`, and
`PickZeroRuleS` selects the eta-zero term after the contour run
(`/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/work-complex-eps6-lane25/amflow/diffeq_solver/DESolver.m:916-927`,
`:1053-1100`).

Closed-form one-loop primitives are useful independent checks, not the
qualifying runtime mechanism:

- Tadpole `box[0,0,0,1]`: with `M = 2-I`,
  `T(M,eps) = -Gamma(eps - 1) M^(1-eps)`.
- Massless s-channel bubble `box[1,0,1,0]`: with
  `L_s = log(496) - i*pi`,
  `B_s(eps) = exp(-eps L_s) Gamma(1+eps) Gamma(1-eps)^2 /
  (eps Gamma(2-2eps))`.
- One-mass bubbles `box[1,0,0,1]`, `box[0,1,0,1]`, and `box[0,0,1,1]`:
  Feynman-parameter integrals
  `Gamma(eps) int_0^1 dx [x M - x(1-x) q2]^-eps`, with the log branch
  reached by the same `NegIm` contour.
- Triangle `box[1,0,1,1]` and box `box[1,1,1,1]`: standard one-loop
  complex-mass `C0` and `D0` Feynman-parameter primitives. Their log/dilog
  branches must be continued along the AMFlow contour and are not safe to
  replace with real-kinematics formulas.

These formulas can validate constants and branch signs. They must not become a
target-value hardcode path.

## C++ Implementation Contract

Add a dedicated full-contour endpoint path guarded by a new predicate such as
`IsComplexKinematicsFullEtaZeroContourState(spec)`. The guard should require:

- `benchmark_id == "complex_kinematics"`, `family == "box"`,
  `variable == "eta"`, `target_location == "eta=0"`;
- `boundary_state_direction == "NegIm"` and a marked `eta=0` endpoint;
- the seven retained masters in the AMFlow order
  `box[0,0,0,1]`, `box[1,0,1,0]`, `box[1,0,0,1]`,
  `box[0,1,0,1]`, `box[0,0,1,1]`, `box[1,0,1,1]`,
  `box[1,1,1,1]`
  (`tools/reference-harness/specs/phase0/complex_kinematics.amflow-state.json:134-198`);
- complex numeric substitutions for `s`, `t`, `p3sq`, `p4sq`, and `m3sq`;
- coefficient matrices, `boundary`, `boundarymi`, `bpattern`, `direction`, and
  `epslist` data sufficient to reproduce AMFlow.

Required new helpers:

- `ParseAmflowNumericSubstitutionsAsComplex(...)`: parse raw AMFlow numeric
  substitutions including `I`, rationals, and integers.
- `ParseComplexRationalEtaMatrix(...)`: build per-epsilon complex rational DE
  evaluators from the retained coefficient matrix
  (`tools/reference-harness/specs/phase0/complex_kinematics.amflow-state.json:60-125`).
- `BuildComplexEtaContourPlan(...)`: reproduce AMFlow pole extraction and
  `RunEta` semantics for `NegIm`, including pole list, waypoints, endpoint
  approach vector, and deterministic contour fingerprint.
- `EvaluateEtaInfinityBoundaryFromRegions(...)`: extend the existing boundary,
  `boundarymi`, and `bpattern` ingestion used by
  `EvaluateAmflowStateEtaInfinityBoundary`, but keep the full sample vector
  available for live contour transport.
- `RunComplexEtaContour(...)`: perform the regular-point propagation with
  precision, step, retry, and failure diagnostics.
- `BuildEtaZeroLocalModel(...)`: compute residue/exponent data at eta=0 and
  classify regular, regular-singular, logarithmic, resonant, and unsupported
  cases.
- `ExtractEtaZeroTermPickZeroEquivalent(...)`: implement the `PickZeroRuleS`
  semantics, including dropped-term audit and fail-closed behavior for ambiguous
  integer-power regions.
- `EvaluateComplexKinematicsFullEtaZeroContour(...)`: drive the full sequence
  for each epsilon sample, fit Laurent coefficients, and populate diagnostics.

Existing code to extend or reuse:

- Extend `ParseAmflowSolveSeriesStateJsonRoot` only to preserve the raw data
  needed by the full-contour path; do not make final `solution` samples
  mandatory for this mode (`src/cli/main.cpp:1485-1691`).
- Reuse `EvaluateAmflowStateEtaInfinityBoundary` parsing concepts, but do not
  stop after first asymptotic transport (`src/cli/main.cpp:5585-5664`).
- Reuse Laurent fitting and JSON result serialization, while setting
  `full_eta_zero_contour_applied=true` only after live contour plus endpoint
  extraction succeeds (`src/cli/main.cpp:6448-6525`).
- Do not extend `TransportThroughEpsOrder` by adding complex-kinematics
  target-value formulas. That helper is automatic-loop selected primitive
  transport and should remain a separate reviewed slice.

Diagnostics required for acceptance:

- `runtime_boundary_provider` must not be
  `retained-loop-solution-sample-cache-laurent-fit`.
- `transport_applied` must be `true`.
- `transport_scope` must be `eta-zero-full-contour-endpoint-coefficients`.
- `full_eta_zero_contour_applied` must be `true`.
- Emit contour waypoints, complex pole list, branch direction, endpoint local
  model, extraction order, dropped-term audit, epsilon sample count, and a flag
  confirming final AMFlow `solution` samples were not used as input.

## Verification Plan

Use three evidence tiers.

First, keep provenance explicit. The original upstream reference is the
reference-harness source copy cited above. The derived phase-0 generated config
and golden paths are valid reproducibility artifacts, not original source
(`/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/work/goldens/phase0/complex_kinematics/golden-manifest.json:1-14`,
`/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/work/comparisons/phase0/complex_kinematics.summary.json:1-40`).
The positive-order validation surface is
`tools/reference-harness/specs/phase0/complex_kinematics.eps2-golden-manifest.json:1-20`,
which points at the `work-complex-eps6-lane25` recapture.

Second, add anti-fallback tests before numeric acceptance:

- a stripped or override-mode state must run without reading
  `boundary_state.files.solution`;
- the old retained-sample path must still report deferred full contour when used;
- attempts to set `full_eta_zero_contour_applied=true` without contour and
  endpoint-audit data must fail tests;
- hardcoded seven-master endpoint tables, zero filling, tolerance loosening, and
  output suppression must be rejected by audit.

Third, compare numerics against AMFlow:

1. Smoke test against the retained phase-0 golden through eps^0 for all seven
   retained masters and the target output. Current source precision is 40 digits,
   so use the existing 50-digit b61n threshold only where the retained golden has
   already demonstrated enough digits.
2. Compare positive orders against the `epsorder = 6` recapture, which evaluates
   17 epsilon samples at working precision 238 and prints through positive
   eps^4
   (`/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/work-complex-eps6-lane25/logs/local-cap-complex-eps6.20260505T042129.out:46-49`).
   Require all seven retained masters through eps^2 to match at the existing
   reviewed threshold; check eps^3/eps^4 as advisory unless the comparator and
   manifest explicitly gate them.
3. For a b61n closure claim, produce a fresh AMFlow recapture with precision
   higher than 40, preferably `precision >= 80`, and `epsorder >= requested + 2`
   in AMFlow's convention because one loop has `leading = -2`
   (`AMFlow.m:1227-1230`). Preserve Wolfram, Kira, Fermat, host, logs, stderr,
   `sol` hashes, and generated-config hashes.
4. Run C++ with the full-contour mode at the same requested epsilon order and
   digits. Require unchanged comparator tolerance, all seven retained masters,
   reconstructed target output, and diagnostics proving full contour execution.

## Consensus

The accepted design is live complex eta-contour execution followed by
AMFlow-equivalent eta=0 endpoint extraction. Analytic one-loop endpoint formulas
and retained solution samples are useful checks, but neither is sufficient to
close b61n.

APPROVE this theory design for an implementation lane.

BLOCK any current b61n closure claim while runtime evidence still reports
retained loop solution-sample fitting or
`full_eta_zero_contour_applied=false`.
