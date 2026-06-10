# B61n Publication Pipeline

Status: post-M7 maintainer note. This document describes the committed b61n
publication architecture and its guardrails; it does not change runtime behavior
or claim new b61n closure evidence.

## Scope

The b61n publication path is the scoped `complex_kinematics` row 5/6 coefficient
pipeline built after the M7 signoff work. It is deliberately narrower than a
general full-contour claim: the runtime may expose reviewed selected endpoint
evidence and a coefficient-state publication surface, but publication remains
guarded by source evidence, endpoint matching, AMFlow comparator agreement, and
the publication audit trail.

The canonical six-stage shape is:

```text
row 5/6 audit
  -> Laurent matrix evaluator
  -> finite-start coefficient audit
  -> coefficient-state transport wrapper
  -> endpoint matcher
  -> publication gate
```

## Landing Map

| Stage | Landing commits | Maintainer contract |
| --- | --- | --- |
| 1. Audit and target graph | `8aa73a3` ("Audit b61n row 5/6 boundary matching"), `09cc7bd` ("Add b61n row56 coefficient target graph") | Establish the root cause as coefficient-level row 5/6 transport rather than sample-space Frobenius matching, then freeze the row 5/6 target graph so later stages can fail closed on unclosed dependencies. |
| 2. Laurent matrix evaluator | `b33d748` ("Add b61n Laurent matrix evaluator") | Evaluate the retained b61n eta matrix as Laurent coefficients before endpoint fitting, with deterministic diagnostics and tests that forbid replacing the matrix path with endpoint constants. |
| 3. Finite-start coefficient audit | `a82c14e` ("Add b61n finite-start coefficient audit") | Bind the coefficient path to certified eta-infinity finite-start data and keep the publication surface closed when the start state or its precision guard is not sufficient. |
| 4. Transport wrapper | `358d23d` ("Add b61n coefficient-state transport wrapper") | Wrap the Laurent coefficients as the transported ODE state so the row 5/6 comparison target is carried directly, instead of reconstructing coefficients from clustered epsilon samples after propagation. |
| 5. Endpoint matcher | `3d47e0d` ("Add b61n coefficient-state endpoint matcher") | Match the transported coefficient state to the reviewed eta=0 endpoint structure and expose explicit success/failure metadata for the coefficient-state path. |
| 6. Publication gate | `c267900` ("Add b61n coefficient-state publication gate") | Publish only through the coefficient-state gate, with blocked diagnostics when the target graph is unclosed or comparator evidence has not accepted every reviewed endpoint variant. |

The first stage has two commits because the architecture starts with both a
human-readable audit (`8aa73a3`) and the executable target graph (`09cc7bd`).
Maintainers should treat them as one stage: the audit explains why the old
sample-space path is insufficient, and the graph gives the later code a stable
object to verify.

## Stage Contracts

### 1. Audit and Target Graph

The audit landing records that rows 5 and 6 are the remaining b61n coefficient
wall: row 5 `eps^0` is the digit ceiling, and row 6 subleading coefficients
inherit the same lower-triangular dependency. The target graph landing turns
that diagnosis into an executable dependency surface. A publication attempt must
not silently skip a row, downgrade a target, or infer closure from a successful
sample-space residual.

Maintainer rule: when a b61n change touches publication diagnostics, check that
the row 5/6 target graph still reports every required target and still fails
closed on unclosed dependencies.

### 2. Laurent Matrix Evaluator

The Laurent evaluator is the boundary between matrix semantics and coefficient
transport. It reads the b61n eta matrix into ordered Laurent terms so the
runtime can propagate the coefficient orders that feed the comparator wall. This
keeps the pipeline tied to the retained AMFlow-state matrix rather than to
hardcoded endpoint values.

Maintainer rule: do not add a shortcut that emits row 5/6 coefficients without
first passing through the Laurent matrix evaluator or an explicitly equivalent
reviewed matrix path.

### 3. Finite-Start Coefficient Audit

The finite-start audit binds the coefficient-state path to the eta-infinity
initial-data contract. It exists because a formally correct coefficient ODE is
not publishable unless the starting data and precision guard are themselves
audited. The audit should remain separate from endpoint publication: start-state
certification is necessary evidence, not a publication result.

Maintainer rule: if a future lane changes the start radius, precision budget, or
eta-infinity handoff, update the audit evidence before touching the publication
gate.

### 4. Transport Wrapper

The transport wrapper carries Laurent coefficients as state variables through
the lower-triangular b61n contour problem. Its purpose is to avoid the known
failure mode where the runtime transports epsilon sample values first and only
then tries to fit row 5/6 coefficients from clustered endpoint samples.

Maintainer rule: publication diagnostics should make it clear whether a result
came from coefficient-state transport or from legacy sample-to-Laurent fitting.
The latter must not be promoted as coefficient-state evidence.

### 5. Endpoint Matcher

The endpoint matcher connects the transported coefficient state to the reviewed
eta=0 endpoint model. It is the point where a transported state becomes a
candidate coefficient publication value, and it must preserve enough metadata to
distinguish a matched endpoint from a bypassed or ambiguous endpoint term.

Maintainer rule: endpoint matching must stay fail-closed for unsupported local
models, missing row 5/6 coefficient orders, and mismatch between variant ids and
reviewed endpoint integrals.

### 6. Publication Gate

The publication gate is the only place where the b61n coefficient-state path may
turn into a publication claim. It is intentionally stricter than "the runtime
ran": it requires the target graph, source evidence, endpoint matcher, reviewed
endpoint variant surface, and AMFlow cross-comparator gate to agree.

Maintainer rule: do not set `full_eta_zero_contour_applied=true`, remove the
b61n blocker, or claim M6/M7 promotion from the coefficient-state surface alone.
Those promotions require the separate phase-0 packet comparison and release
sidecars.

## Post-M7 Guardrail Landings

The six architecture stages are now backed by later quality landings. These are
not extra pipeline stages; they are regression guards that keep the six-stage
path auditable.

| Guardrail | Landing commits | What it protects |
| --- | --- | --- |
| Reference-floor and precision gates | `dfcd15c`, `245a205`, `70ae6a8`, `4f560c8` | Prevent b61n publication evidence from drifting below the accepted comparator floor or relying on unstable exact-rational extension behavior. |
| Determinism and precision evidence binding | `05180f3`, `601b70b`, `0befd92` | Require repeatable publication evidence and bind the publication audit to the precision sidecar instead of letting the sidecar be optional decoration. |
| Inventory and diagnostic-field guards | `dac466f`, `679bec1`, `68a7f12`, `7cbde43` | Keep the reviewed endpoint inventory and diagnostic field set stable so downstream release audits can query the same shape over time. |
| Audit-trail queries and fingerprint pins | `cb86c16`, `47d0bb8`, `2871bc1` | Verify that the publication audit trail exposes the required keys and category fingerprints for reproducible regeneration. |
| Status reporting | `9ff785d` | Summarize b61n parity status without changing the publication gate or treating scoped evidence as full closure. |

## Maintainer Checklist

- Keep the six-stage order intact: audit, Laurent evaluator, finite-start audit,
  transport wrapper, endpoint matcher, publication gate.
- Treat row 5/6 coefficient transport as the publication object; do not infer it
  from sample-space endpoint residuals.
- Preserve the distinction between selected endpoint evidence, coefficient-state
  evidence, and full phase-0 packet qualification.
- Require AMFlow comparator agreement before any coefficient publication claim.
- Regenerate and review fingerprint pins whenever an audit category or expected
  diagnostic field set intentionally changes.
- Leave b61n promotion blocked unless the phase-0 packet comparison, correct
  digit scoring, failure-code audit, and release sidecars are all coherent.

## Non-Claims

This note does not add a runtime stage, does not edit the b61n implementation,
does not alter release scripts, and does not mark b61n as closed. It documents
the current maintainer contract for the post-M7 publication architecture so
future lanes can change one stage at a time without weakening the gate.
