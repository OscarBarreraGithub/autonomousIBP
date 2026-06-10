# Architecture Notes

Status: post-M7 docs landing. This page indexes the committed architecture
notes under `docs/architecture/`; it does not change runtime behavior, publish
new coefficients, close any phase-0 lane, or claim M6/M7/release readiness.

## Scope

Architecture notes are maintainer-facing maps for already landed implementation
surfaces. They describe the reviewed pipeline shape, the guardrails that keep
status honest, and the non-claim boundaries that future lanes must preserve.
They are not qualification artifacts by themselves.

Use this directory when a change needs to answer one of these questions:

- Which stages make up the current reviewed pipeline?
- Which landed commits define the maintainer contract for each stage?
- Which evidence is scoped support, and which evidence is still blocked from
  promotion?
- Which audit, fingerprint, status, or release-sidecar guard must move with an
  intentional architecture change?

## Current Notes

### B61n Publication Pipeline

[B61n Publication Pipeline](b61n-publication-pipeline.md) maps the b61n row 5/6
coefficient-state publication path. Use it when reviewing the Laurent-matrix,
finite-start, coefficient-state transport, endpoint-matcher, and
publication-gate stages.

Non-claim boundary: selected endpoint or coefficient-state evidence does not
remove the b61n blocker or claim full phase-0 packet qualification.

### B63n Weighted-Residue Pipeline

[B63n Weighted-Residue Pipeline](b63n-weighted-residue-pipeline.md) maps the
b63n Cutkosky weighted-residue publication path. Use it when reviewing the
source-semantics, structural scaffold, residue carrier, weighted moment plan,
scoped D7 evidence, and audit/fingerprint guard stages.

Non-claim boundary: scoped D7 evidence does not imply D2/D4/D6 coverage,
`feynman_prescription` closure, D246 retirement, or M6/M7 readiness.

## Maintainer Rules

- Keep architecture notes descriptive unless the same landing also changes the
  implementation, tests, and evidence needed to justify a new claim.
- Update the relevant architecture note when a future lane intentionally changes
  a pipeline stage, expected audit category, fingerprint surface, status field,
  or publication gate.
- Preserve blocker wording in docs, status summaries, and release notes until
  the matching phase-0 packet evidence and qualification sidecars are accepted.
- Prefer one focused note per pipeline. Add a new architecture note only when
  there is a stable, reviewed pipeline to map.

## Related Docs

- [M7 Closure Plan](../milestones/m7-closure-plan.md) records the closure
  criteria and blocker-preserving readiness checks.
- [M7 Parity Signoff Gap Report](../milestones/m7-parity-signoff-gap-report.md)
  records post-M7 gap classifications without changing runtime behavior.
- [Release Known Gaps](../release/known-gaps.md) keeps release-facing blockers
  visible separately from architecture-maintainer notes.

## Verification

Docs-only changes in this directory should at minimum verify that the linked
paths exist and that no wording promotes scoped evidence into closure. Runtime
or qualification changes need the relevant build, CTest, reference-harness, and
release-sidecar checks in the same reviewable unit.
