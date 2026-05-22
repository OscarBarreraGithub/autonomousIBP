# Lane 169 B63n Weighted-Residue Evaluator Plan

Status: Tier C theory and implementation plan. This sidecar does not implement
the `b63n` weighted-residue evaluator, does not publish new coefficients, does
not consume AMFlow final solution samples as runtime input, does not change
qualification metadata, and does not claim `full_eta_zero_contour_applied=true`.

## Verdict

The next `b63n` blocker is not another selected pure-cut coefficient. It is the
weighted Cutkosky residue evaluator for the two full phase-space rows:

- `automatic_phasespace`, where the cut support is D1,D3,D5 and the missing
  object is the one-mass three-body phase-space residue weighted by
  D2,D4,D6,D7.
- `feynman_prescription`, where the cut support is D9,D10 and the missing
  object is the prescription-aware two-particle cut subintegral with the two
  conjugate loop ledgers.

Lane 162 ranked that evaluator as the hard physics blocker. This lane turns
that rank into a staged acceptance plan. The plan is intentionally fail-closed:
the first implementation work should add typed carriers, synthetic tests, and
review hooks before any non-zero benchmark coefficient is allowed to land.

## Inputs Already Frozen

The plan starts from existing reviewed boundaries:

- `docs/theory/b63n-runtime-lane.md` defines the source semantics, examples,
  Cutkosky prefactors, and the false-positive boundary around retained AMFlow
  final solution samples.
- `tools/reference-harness/specs/m6/lane149/b63n-live-cutkosky-gap-analysis.md`
  keeps `automatic_phasespace`, `feynman_prescription`, and `b63n` blocked until
  a live Cutkosky provider, weighted residue model, endpoint propagation, and
  packet qualification exist.
- `tools/reference-harness/specs/m6/lane162/b63n-residue-evaluator-decomposition.md`
  separates code-tractable guardrails from the Rank 9 weighted evaluator.
- `tools/reference-harness/specs/m6/lane148/m6-runtime-lane-qualifier-requirements.md`
  defines the later packet-level promotion conditions. This lane does not alter
  those conditions.

Any future evaluator must preserve the current selected pure-cut evidence as
scoped evidence only. It cannot reuse selected pure-cut parity as evidence for
the weighted automatic phase-space target or for the `feynman_prescription`
target.

## Evaluator Boundary

The future evaluator should be split into three layers with separate acceptance
criteria.

### 1. Structural Surface Layer

This layer recognizes only the reviewed surfaces and rejects everything else:

- `automatic_phasespace`: family `phase`, pinned kinematics `s=100`, `msq=1`,
  cut denominators D1,D3,D5, target `phase[1,2,1,1,1,1,1]`, and no
  prescription-sensitive branch imported from `automatic_loop`.
- `feynman_prescription`: family `loopxloop`, pinned kinematics `s=10`,
  `msq=1`, `m2sq=2/5`, cut denominators D9,D10, target
  `loopxloop[0,1,1,1,1,0,1,1,1,1,0,0]`, plus both conjugate loop-prescription
  ledgers `{1,-1,0}` and `{-1,1,0}`.

Acceptance for this layer is code-tractable and coefficient-free:

- rejected-fixture tests for wrong cuts, wrong kinematics, wrong target powers,
  missing conjugate ledger, and unsupported family names;
- no AMFlow final `solution` samples accepted as live boundary input;
- all rejected surfaces return a typed blocker such as `boundary_unsolved`,
  `master_set_instability`, or `continuation_budget_exhausted`.

### 2. Residue-Term Carrier Layer

This layer should introduce an inert, typed series carrier before real physics
coefficients are computed. Each term needs at least:

- epsilon order and requested working precision;
- eta power and log power;
- region key and branch/prescription ledger;
- complex coefficient with precision diagnostics;
- provenance string that distinguishes synthetic fixture, external CAS review,
  fresh AMFlow comparison, and runtime-derived coefficient.

Acceptance for this layer is also coefficient-free:

- synthetic-only tests prove that prefactor multiplication and eta-zero
  selection do not invent missing powers, suppress logs, choose among multiple
  regions, or promote sentinel values;
- synthetic fixtures must not be wired to `qualification_readiness.py`,
  `qualify_milestone_m6.py`, or any phase-0 packet promotion path;
- JSON diagnostics must keep `live_coefficients_available=false` and
  `full_eta_zero_contour_applied=false` unless a later coefficient-producing
  lane satisfies the physics gates below.

### 3. Weighted Physics Layer

This layer is the hard blocker and must not be implemented from local
intuition. It needs a reviewed derivation or external CAS artifact for each
published coefficient.

For `automatic_phasespace`, the missing calculation is the Laurent expansion of

```text
K_2(eps) * Int dPhi_3(P; m, 0, 0)
  [D2^-2 D4^-1 D6^-1 D7^-1]_sigma
```

with `P^2=100`, `m^2=1`, and cut denominators D1,D3,D5. The review artifact
must show how the angular and invariant moments map onto D2,D4,D6,D7, which
endpoint regions contribute, and how logs or singular powers are handled before
eta-zero selection.

For `feynman_prescription`, the missing calculation is the prescription-aware
two-particle cut subintegral with D9,D10 cut and uncut loop weights carried by
the plus-minus `{1,-1,0}` and minus-plus `{-1,1,0}` ledgers. The review artifact
must show the conjugacy relation before coefficient publication, not only after
comparison.

## Promotion Gates

The staged gates are:

| Gate | Allowed diff | Required evidence | Honest status |
| --- | --- | --- | --- |
| C0 | Documentation plan only. | Four-role review and no runtime metadata edits. | Tier C, blocked. |
| C1 | Inert residue carrier and synthetic tests. | Synthetic fixtures only; no benchmark coefficient output. | Tier C, blocked. |
| B1 | One reviewed non-zero automatic-phase-space coefficient. | External derivation or CAS artifact, fresh AMFlow comparison at 30 or more digits, and no final-solution-sample runtime input. | Tier B scoped evidence only. |
| B2 | One reviewed `feynman_prescription` coefficient with conjugacy check. | External derivation or CAS artifact, both ledgers checked, fresh AMFlow comparison at 30 or more digits. | Tier B scoped evidence only. |
| A1 | Full reviewed coefficient coverage for both rows through the required epsilon orders. | Fresh C++ runtime output, AMFlow comparison, digit score, and failure-code audits for both rows. | Candidate phase-0 packet evidence, not M6 by itself. |
| M6 | Packet-set qualification consumes the accepted rows. | No `phase0_pending_ids`, no `blocked_phase0_examples`, and passing M6 composer output. | M6 closure can be considered outside this sidecar. |

The first coefficient-producing lane must stop at B1 or B2 unless both rows are
complete. A single coefficient is useful as a scoped proof, but it does not
remove `automatic_phasespace` or `feynman_prescription` from pending phase-0
metadata.

## Required Failure Modes

The evaluator should make blocker reasons observable rather than silently
falling back:

- `boundary_unsolved`: no reviewed residue coefficients, incomplete external
  derivation, missing finite boundary values, or final AMFlow solution samples
  offered as the only boundary source.
- `master_set_instability`: unrecognized family, wrong target basis, unexpected
  master ordering, wrong cut support, or missing conjugate
  `feynman_prescription` ledger.
- `continuation_budget_exhausted`: multiple eta-zero regions, unresolved logs,
  unsupported branch structure, prescription conflict, or ambiguous endpoint
  selection.
- `insufficient_precision`: fresh C++ versus AMFlow comparison exists but fails
  the active digit threshold.

The failure-code audit must keep these codes visible for the pending rows until
the packet-level evidence genuinely passes.

## Anti-Fake Constraints

A future coefficient-producing diff must be rejected if it contains any of:

- hardcoded benchmark zeros or sentinel `999` digit evidence;
- self-comparison tests;
- tolerance loosening;
- final AMFlow `solution` sample reads as live boundary input;
- selected pure-cut evidence reused as full weighted-residue evidence;
- `full_eta_zero_contour_applied=true` before both reviewed rows have complete
  coefficient evidence;
- qualification metadata edits that hide the `b63n` pending rows without
  packet-set qualification evidence.

## Four-Role Review

This lane used the requested four-role review:

- Role A, physicist-implementer: APPROVE. The staged split is implementable
  without publishing unreviewed coefficients, preserves the exact
  `automatic_phasespace` and `feynman_prescription` surfaces, and keeps M6
  closure and coefficient publication out of scope.
- Role B, independent tester: APPROVE. The C0/C1 gates are non-publishing, B1
  and B2 are scoped Tier B evidence only, A1 keeps packet qualification
  separate, and the typed failure modes are testable and fail-closed.
- Role C, Mathematica/physics auditor: APPROVE. The plan correctly treats the
  automatic phase-space D1,D3,D5 cut with D2,D4,D6,D7 weights, the
  `feynman_prescription` D9,D10 cut with conjugate ledgers, external
  derivation/CAS requirements, and the retained-solution-sample boundary.
- Role D, anti-fake synthesizer: APPROVE. The plan cannot be mistaken for M6
  closure, blocks selected pure-cut evidence reuse, rejects sentinel,
  self-comparison, tolerance-loosening, and final-solution-sample shortcuts, and
  preserves the pending `b63n` rows until packet qualification.
