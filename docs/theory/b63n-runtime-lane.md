# B63n Cutkosky Phase-Space Eta=0 Runtime Lane Theory Design

Status: peer-reviewed design only. This document does not close `b63n`,
does not remove `automatic_phasespace` or `feynman_prescription` from pending
phase-0 metadata, and does not claim M6 readiness.

## Review Approval

This lane used the requested four-role review:

- Role A, physicist-implementer: APPROVE a future implementation plan for live
  Cutkosky eta=0 endpoint reconstruction, with the caveat that retained AMFlow
  solution samples cannot close `b63n`.
- Role B, independent reviewer: APPROVE design-only scope after clarifying that
  its initial BLOCKED verdict applied to runtime closure, not to this theory
  document.
- Role C, Mathematica-source auditor: APPROVE the source-audit basis and the
  source-root caveat below. This does not approve runtime closure.
- Role D, synthesizer: APPROVE design-only scope, conditional on preserving the
  active `b63n` blocker and treating endpoint formulas as analytic structure and
  implementation contract, not as already-implemented numeric coefficients.

The resolved discrepancy is scope. The reviewers agree that `b63n` remains
runtime-blocked today, while this lane is allowed to land a design document that
defines the future live Cutkosky provider and verification contract.

## Source Roots

The prompt-named AMFlow path:

`/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/work/amflow/examples/`

is absent on this node. The audited AMFlow source root used for line citations
is the canonical extracted upstream tree:

`/n/holylabs/schwartz_lab/Lab/obarrera/reference-inputs/autonomousIBP/cpc/amflow-gitlab-1.1-extracted/`

The corresponding `work-eps34` copies are byte-identical for the two relevant
example inputs:

- `automatic_phasespace/run.wl` sha256
  `f9f021e584334cc21fb682526eaf9f55de95b6f2a58b2971ae46a445fd46e2bf`;
- `feynman_prescription/run.wl` sha256
  `1e3dd288e524c6f1d1d8fccc6ffc0b15a236a12177f669c6a04753c7a8770dc5`.

This is a provenance caveat, not a physics discrepancy.

## Current Evidence Boundary

Lane119, commit `2d35ce4`, classifies both `automatic_phasespace` and
`feynman_prescription` as `b63n` examples with state
`cataloged-pending-capture` and the `core-package-family-default` profile
(`docs/milestones/m6-runtime-lanes-survey.md:34-39`). That profile requires at
least 50 correct digits and the default failure-code profile keeps
`insufficient_precision`, `master_set_instability`, `boundary_unsolved`, and
`continuation_budget_exhausted` visible
(`docs/milestones/m6-runtime-lanes-survey.md:41-57`).

The same survey states the current defect precisely: C++ parses phase-space
metadata and can fit retained solution samples, but full Cutkosky phase-space
boundary reconstruction from cut propagators is deferred
(`docs/milestones/m6-runtime-lanes-survey.md:68-77`). Therefore a retained
solution-sample fit is explicitly a false-positive closure for this lane.

The qualification scaffold and M6 scripts keep that fail-closed:

- `qualification-benchmarks.json:63-70` and `:106-113` keep the two examples in
  `cataloged-pending-capture`, assign `next_runtime_lane: b63n`, and attach the
  `core-package-family-default` digit profile plus
  `default-required-failure-codes`.
- `qualification-benchmarks.json:9-35` defines the 50-digit floor and the four
  required failure codes.
- `validate_qualification_scaffold.py:85-102` requires captured optional rows
  to carry an `optional_capture_packet` and no `next_runtime_lane`, while
  pending rows must do the opposite.
- `qualify_phase0_packet_set.py:275-345` requires candidate result manifests,
  primary run manifests, matching output names and numeric literal skeletons,
  profile metadata, and all digit thresholds to pass.
- `qualify_phase0_packet_set.py:349-456` requires complete failure-code audits,
  required-code reporting, and no unexpected failure-code drift.
- `qualify_milestone_m6.py:645-650` closes phase-0 runtime lanes only when both
  `phase0_pending_ids` and `blocked_phase0_examples` are empty and the phase-0
  packet set is qualified.

## Current C++ Surface

The current direct solve-series state records enough metadata to identify the
surface, but not enough implementation to close it:

- `DirectSolveSeriesSpec` stores `phase_space_prescription` and
  `phase_space_cut` (`src/cli/main.cpp:390-417`).
- AMFlow-state ingestion reads `phase_space.prescription`,
  `phase_space.cut`, and `phase_space.output_masters`; if `integral_kind` is
  absent, the parser sets it to `phase_space`, then marks the state as
  retained-solution-sample input (`src/cli/main.cpp:1581-1598`).
- The parser validates prescription entries in `{-1,0,1}`, cut entries in
  `{0,1}`, and cut length against the master index width
  (`src/cli/main.cpp:1600-1646`).
- `UsesRetainedSolutionSamples` treats every phase-space AMFlow state as a
  retained-sample state (`src/cli/main.cpp:3393-3400`).
- `EvaluateAmflowStateRetainedSolutionSamples` reads
  `boundary_state.files.solution`, fits epsilon samples as Laurent
  coefficients, and appends that full phase-space boundary reconstruction from
  cut propagators remains deferred (`src/cli/main.cpp:5412-5556`).
- JSON diagnostics report the successful provider as
  `retained-phase-space-solution-sample-cache-laurent-fit`, the transport scope
  as `phase-space-solution-samples`, and the application as
  `phase-space-solution-sample-laurent-fit`
  (`src/cli/main.cpp:6390-6500`). The blocked reason remains full Cutkosky
  reconstruction deferred after retained fitting (`src/cli/main.cpp:6502-6507`).
- The built-in solver provider for `builtin::cutkosky-phase-space` currently
  throws `BoundaryUnsolvedError` (`src/solver/boundary_provider.cpp:48-63`);
  its registry only installs deferred strategies
  (`src/solver/boundary_provider.cpp:203-213`).

The existing `TransportThroughEpsOrder` framework is useful as an integration
pattern but is not a `b63n` implementation. It is hardwired to the retained
`automatic_loop` eta=0 endpoint slice, selected primitive bubble and box
endpoint helpers, and a reviewed epsilon-order guard
(`src/cli/main.cpp:3919-3933`, `:4598-4715`). The non-sample eta-infinity path
fits retained boundary samples and then calls that selected endpoint transport
(`src/cli/main.cpp:5585-5664`).

## AMFlow Cutkosky Semantics

The future C++ contract must match the upstream Mathematica semantics, not just
the JSON field names.

AMFlow derives propagator prescriptions from loop prescriptions. For a
propagator, all loop prescriptions equal to zero give prescription zero; all
nonzero prescriptions must have the same sign; mixed signs are a conflict and
abort (`AMFlow.m:461-481`). This distinction matters for
`feynman_prescription`, where loop-level prescription signs are not identical
to the cut vector.

Cutkosky ending selection is structural. `PhaseVolumeQ` requires the component
variables to be exactly the cut variables and the number of cuts to be one more
than the phase-volume loop count (`AMFlow.m:485-489`). Region enumeration removes
cut branches and rejects prescription-incompatible regions
(`AMFlow.m:574-587`). `AMFSystemEndingQ[..., "Cutkosky"]` requires all
components to be ending components and exactly one phase-volume component
(`AMFlow.m:911-916`).

The continuation direction is prescription-driven. AMFlow computes a
self-consistent prescription for the eta-shifted propagators and selects
`"Im"` if that prescription is `-1`, otherwise `"NegIm"`
(`AMFlow.m:874-881`).

For a Cutkosky ending, AMFlow unsets `Prescription` and `Cut`, builds the eta
system, and records the global-preferred prefactor and final `Im` projection:

```text
K_r(eps) = 2 * (Pi^(2-eps) * (2 Pi)^(2 eps - 4))^r * (-1)^(r+1)
```

where `r` is the loop count of the phase-volume component
(`AMFlow.m:941-950`). Endpoint extraction then follows the ordinary AMFlow
endpoint solve: build the local zero model (`DESolver.m:916-927`), run the eta
contour from infinity through regular points (`DESolver.m:1065-1095`), and
apply `PickZeroRuleS` to select the integer eta-zero term
(`DESolver.m:1053-1061`).

The C++ implementation must therefore reproduce the structural endpoint
functional:

```text
B_cut(eps) = K_r(eps) * Im_sigma[ I_uncut_eta0(eps) ]
```

`sigma` is the branch and prescription ledger selected by the loop
prescriptions and eta contour. `I_uncut_eta0` is the eta=0 local solution after
cut residues and uncut-propagator prescriptions have been applied. It is not a
fit of the final AMFlow `solution` file.

## Example Endpoint Structures

### automatic_phasespace

The audited source defines family `phase`, loops `{l1,l2}`, legs `{p1,p2}`,
kinematics `{s -> 100, msq -> 1}`, and seven propagators
(`examples/automatic_phasespace/run.wl:17-24`). It declares phase-space
prescription `{0,0}`, cut vector `{1,0,1,0,1,0,0}`, target
`j[phase,1,2,1,1,1,1,1]`, `precision = 20`, and `epsorder = 4`
(`examples/automatic_phasespace/run.wl:28-42`).

The three cut propagators are:

```text
D1 = l1^2 - msq
D3 = l2^2
D5 = (l1 + l2 + p1 + p2)^2
```

At `s = 100`, `msq = 1`, this is a two-loop three-particle phase-volume
component with one massive and two massless final-state lines. The reviewed
`b63n` subset must verify `r = 2` and use

```text
K_2(eps) = -2 * (Pi^(2-eps) * (2 Pi)^(2 eps - 4))^2.
```

A closed-form endpoint model can be built by resolving the three cuts into a
dimensionally regularized phase-space measure and leaving the four uncut
denominators as angular and invariant weights:

```text
B_auto(eps) =
  K_2(eps) * Int dPhi_3(P; m, 0, 0) *
    [D2^-2 D4^-1 D6^-1 D7^-1]_sigma,

P = p1 + p2, P^2 = s = 100, m^2 = 1.
```

Using the standard recursive factorization,

```text
dPhi_3(P; m, 0, 0) =
  (dq^2 / 2 Pi) * dPhi_2(P; m, sqrt(q^2)) * dPhi_2(q; 0, 0),

0 <= q^2 <= (sqrt(s) - m)^2 = 81.
```

The endpoint series is an Euler/Gegenbauer moment expansion in `eps`: soft
massless limits and collinear angular endpoints generate the Laurent poles,
while the finite letters come from the thresholds and uncut angular denominators
of `D2`, `D4`, `D6`, and `D7`. Because the loop prescriptions are `{0,0}`, the
branch ledger must record prescription-insensitive real phase-space cuts rather
than importing the `automatic_loop` `log(100) - i*pi` convention.

### feynman_prescription

The audited source defines family `loopxloop`, loops `{l1,l2,q}`, kinematics
`{s -> 10, msq -> 1, m2sq -> 2/5}`, and twelve propagators
(`examples/feynman_prescription/run.wl:12-20`). It first uses prescription
`{1,-1,0}`, cut vector `{0,0,0,0,0,0,0,0,1,1,0,0}`, target
`j[loopxloop,0,1,1,1,1,0,1,1,1,1,0,0]`, `precision = 20`, and
`epsorder = 8` (`examples/feynman_prescription/run.wl:23-37`). It then repeats
the solve with opposite prescription `{-1,1,0}` and checks that the two
solutions are complex conjugates at the finite coefficient
(`examples/feynman_prescription/run.wl:40-48`).

The two cut propagators are:

```text
D9  = q^2 - msq
D10 = (p1 + p2 - q)^2 - m2sq
```

This is a one-loop two-particle phase-volume component in the `q` loop, so the
reviewed subset must verify `r = 1` and use

```text
K_1(eps) = 2 * Pi^(2-eps) * (2 Pi)^(2 eps - 4).
```

The Kallen discriminant at the pinned kinematic point is

```text
lambda(s, msq, m2sq) = lambda(10, 1, 2/5) = 1809 / 25 > 0.
```

After the `q` cut, the endpoint has the closed-form structure

```text
B_fp^(+,-)(eps) =
  K_1(eps) * Int dPhi_2(P; sqrt(msq), sqrt(m2sq)) *
    T_l1^+(q, eps) * T_l2^-(q, eps),

P = p1 + p2, P^2 = 10.
```

`T_l1^+` is the one-loop uncut subintegral built from the `l1` denominators
present in the target, with `+i0` prescription; `T_l2^-` is the corresponding
`l2` subintegral with `-i0` prescription. The remaining phase-space angular
integral is a beta/hypergeometric moment over the physical two-body angle. The
opposite prescription must produce

```text
B_fp^(-,+)(eps) = conjugate(B_fp^(+,-)(eps))
```

coefficient by coefficient, up to the requested epsilon order and numeric
precision. A sign error in the loop-to-propagator prescription map, AMFlow
direction selection, or final `Im` projection will generally break this
conjugacy before it shows up as a low-level parser error.

## C++ Implementation Contract

The future implementation should be a narrow reviewed provider for the exact
`b63n` surface, not a permissive phase-space catch-all. The minimum helper set is:

- `ParsePhaseSpaceTopologyFromAmflowConfigRaw`: extract family, loops,
  propagators, replacements, numeric kinematics, targets, and raw eta insertions
  from `amflow_config_raw` or the generated AMFlow-state sidecar.
- `DerivePropagatorPrescriptionFromLoopPrescriptions`: implement AMFlow's
  `PrescriptionOf` rules, including fail-closed conflict reporting.
- `AnalyzeCutkoskyPhaseVolumeComponent`: validate that cut support is one
  phase-volume component, cut powers are unit powers for the reviewed examples,
  no eta mass insertion is applied to cut denominators, and all non-cut
  components are acceptable ending components.
- `BuildCutkoskyPrefactorSeries(r, eps_order)`: expand `K_r(eps)` through the
  requested order using the same series helpers already used by endpoint
  transport.
- `BuildAutomaticPhaseSpaceEndpointModel`: construct the one-mass three-body
  phase-space residue and uncut angular denominator moments for the exact
  `automatic_phasespace` topology.
- `BuildFeynmanPrescriptionEndpointModel`: construct the two-body cut residue,
  prescription-aware `l1`/`l2` uncut subintegrals, and conjugacy ledger for the
  exact `feynman_prescription` topology.
- `PickCutkoskyEtaZeroTerm`: reproduce AMFlow's integer eta-zero selection rule
  and fail if the local model has no unambiguous eta^0 coefficient.
- `ApplyCutkoskyResidueEndpointTransportThroughEpsOrder`: insert the endpoint
  Laurent series into `SolverDiagnostics`, with an audit object carrying
  benchmark id, cut vector, prescription vector, phase-volume rank, prefactor,
  direction, endpoint exponent, IR pole classification, branch ledger, and
  precision budget.

The integration point should mirror `TransportThroughEpsOrder` but not reuse its
retained automatic-loop predicate. One viable structure is:

```text
TransportThroughEpsOrder(...)
  existing retained automatic_loop endpoint branch
  reviewed b63n phase_space Cutkosky endpoint branch
```

or a sibling dispatcher called before the phase-space retained-sample path. In
either case, the live `b63n` branch must execute before any read of
`boundary_state.files.solution` is allowed to satisfy the benchmark. The current
retained-sample path can remain as a diagnostic fallback, but it must not emit
closure evidence for `automatic_phasespace` or `feynman_prescription`.

Successful `b63n` diagnostics must differ from today's retained output:

- `runtime_boundary_provider` should identify the reviewed Cutkosky eta=0
  residue provider, not
  `retained-phase-space-solution-sample-cache-laurent-fit`.
- `transport_scope` should be `eta-zero-full-contour-endpoint-coefficients` or a
  stricter Cutkosky-specific value consumed by the harness.
- `runtime_application` should be `full-eta-zero-contour-endpoint-extraction` or
  an equivalent Cutkosky-specific application name.
- `full_eta_zero_contour_applied` must be true only when the branch ledger,
  endpoint model, and coefficient extraction all ran.
- `blocked_reason` must be empty for the supported passing cases and explicit
  for unsupported topology.

Unsupported or ambiguous cases must fail closed. Required typed failures include
prescription conflict, eta inserted on a cut denominator, non-phase-volume cut
support, non-unit cut powers in the reviewed subset, unresolved IR or threshold
classification, ambiguous eta^0 selection, insufficient precision, master-set
drift, and continuation-budget exhaustion. These failures should map back to the
existing required failure-code profile rather than adding silent success modes.

## Verification Plan

Verification must use fresh high-precision AMFlow runs and C++ runtime output,
not the retained `solution` cache as the implementation source. The pinned
upstream examples use `precision = 20`; that is sufficient for source
identification, not for the M6 50-digit floor. The closure lane should rerun
both examples at a reviewed higher precision and preserve AMFlow inputs,
outputs, command logs, sha256s, and primary run manifests.

Required positive checks:

- Run `automatic_phasespace` and compare the C++ live Cutkosky provider output
  against AMFlow through the requested epsilon order, with at least 50 correct
  digits under `core-package-family-default`.
- Run `feynman_prescription` for both `{1,-1,0}` and `{-1,1,0}`. The packet must
  either namespace `sol1` and `sol2` explicitly or otherwise prove
  prescription-aware output selection; comparing only a retained scoped `sol1`
  packet is not enough for full lane closure.
- Check coefficient-by-coefficient conjugacy between the two prescription
  choices before packet qualification.
- Confirm result JSON no longer reports
  `retained-phase-space-solution-sample-cache-laurent-fit` or the deferred
  Cutkosky blocked reason for the supported passing cases.
- Produce comparator-readable candidate result manifests and primary run
  manifests, then run the phase-0 packet comparison, correct-digit scoring, and
  failure-code audit.
- Update the qualification scaffold only after accepted evidence exists:
  captured rows need `current_evidence_state: reference-captured`,
  `optional_capture_packet`, and no `next_runtime_lane`.
- Run `qualify_phase0_packet_set.py` and `qualify_milestone_m6.py`; `b63n` is
  closed only when `automatic_phasespace` and `feynman_prescription` leave
  `phase0_pending_ids` and `blocked_phase0_examples`.

Required negative checks:

- Corrupt the cut vector, remove the prescription vector, or introduce mixed
  loop prescription signs in one propagator; the runtime must fail before
  coefficient publication.
- Force eta insertion onto a cut denominator; the runtime must reject the input
  rather than silently moving the cut.
- Flip the continuation direction for the first `feynman_prescription` solve;
  the conjugacy check must fail.
- Remove the live endpoint provider and verify that the benchmark remains
  `boundary_unsolved`, not retained-sample-successful for closure purposes.

This design intentionally lands before implementation. The next implementation
lane should be considered successful only when these checks are backed by real
C++ runtime packets and AMFlow high-precision comparisons.
