# B63n Weighted-Residue Pipeline

Status: post-M7 maintainer note. This document describes the committed b63n
weighted-residue architecture and its guardrails; it does not change runtime
behavior, publish new coefficients, close `b63n`, or claim M6/M7 readiness.

## Scope

The b63n weighted-residue path is the scoped Cutkosky phase-space endpoint
pipeline for the two pending phase-0 rows:

- `automatic_phasespace`, whose full target requires the D1,D3,D5 cut support
  weighted by the uncut D2,D4,D6,D7 denominator moments.
- `feynman_prescription`, whose full target requires the D9,D10 cut support plus
  the conjugate `{1,-1,0}` and `{-1,1,0}` loop-prescription ledgers.

The current architecture is narrower than full b63n closure. It has structural
Cutkosky plumbing, typed residue carriers, publication gates, automatic
phase-space moment seeds, a scoped reviewed D7 publication surface, and
fingerprint/status guards. It still blocks the remaining D2/D4/D6 weighted
automatic phase-space pieces, the full `feynman_prescription` weighted residue,
and packet-level M6 promotion.

The canonical six-stage shape is:

```text
source semantics and blocker
  -> structural Cutkosky scaffold
  -> residue carrier, prefactor, and publication gate
  -> weighted moment plan and cross-validation
  -> scoped D7 weighted-residue evidence
  -> fingerprint, audit, and blocker status guards
```

## Landing Map

| Stage | Landing commits | Maintainer contract |
| --- | --- | --- |
| 1. Source semantics and blocker | `32b1e9a` ("Add b63n runtime lane theory design"), `d9b063f` ("Document b63n live Cutkosky implementation gap"), `9f4e3fe` ("Tier C: Decompose b63n residue evaluator gap") | Keep the AMFlow Cutkosky semantics and false-positive boundary explicit: retained final `solution` samples identify examples but cannot close the lane. |
| 2. Structural Cutkosky scaffold | `a4aae73` ("Add b63n Cutkosky transport partial scaffold"), `157ab4d` ("Extend b63n scaffold with residue contour planning"), `2319535` ("Produce first b63n Cutkosky coefficient"), `0441da4` ("Extend b63n Cutkosky coefficient transport to 4 masters") | Preserve the reviewed phase-space topology, branch ledger, eta-contour, and selected pure-cut scaffolding without treating selected pure-cut evidence as full weighted-residue coverage. |
| 3. Residue carrier and publication gate | `af46d91` ("Add b63n Cutkosky K_r(eps) numerical prefactor primitive"), `3c654da` ("Add b63n residue-term data model and synthetic fixture coverage"), `de79dea` ("Add b63n residue publication gate") | Carry coefficients through typed residue terms with provenance, precision diagnostics, eta/log powers, and a publication validator that rejects synthetic or retained-sample inputs. |
| 4. Weighted plan, moment seeds, and cross-validation | `cb88915` ("Add b63n automatic-phasespace symbolic integrand assembly"), `1c3f8e8` ("Add b63n feynman_prescription symbolic subintegral assembly"), `6152924` ("Add b63n weighted residue evaluation plan"), `57006a1` ("Add b63n weighted moment seed gate"), `55676d6` ("Add b63n weighted moment seed packet"), `3e8c402` ("Add b63n residue moment cross-validation gate"), `758b86f` ("Plan b63n weighted residue evaluator") | Require the D2,D4,D6,D7 moment packet and keep it coefficient-free until external derivation/CAS and AMFlow comparison evidence exist. |
| 5. Scoped D7 weighted-residue evidence | `9c3444c` ("Add scoped b63n weighted residue gate"), `f983788` ("Publish scoped b63n D7 weighted residue"), `2c6517f` ("Publish b63n D7 eps1 scoped residue"), `6f6d325` ("Extend b63n D7 scoped residue through eps3"), `243164e` ("Sweep b63n lane146 selected coefficients"), `50aa543` ("Add b63n D7 AMFlow runtime parity test") | Allow only the reviewed `automatic_phasespace` D7 scoped surface through the gate, with AMFlow comparison evidence and `full_eta_zero_contour_applied=false`. |
| 6. Audit, fingerprint, and blocker guards | `b5103b2` ("Add b63n weighted residue audit fingerprints"), `0348e7c` ("Pin b63n weighted residue audit category fingerprints"), `b1e46f7` ("Cover b63n weighted residue audit fingerprint categories"), `04c07dc` ("Add b63n fingerprint regeneration helper"), `f783a6c` ("Add b63n scoped gate audit query check"), `337d4ef` ("Add b63n parity status summary"), `560285d` ("Document b63n D246 AMFlow blocker"), `a8f0772` ("Record b63n D246 Mathematica blocker") | Make drift observable in audit categories, scoped gate fields, regeneration helpers, status summaries, and the current D246 blocker rather than weakening the gate. |

## Stage Contracts

### 1. Source Semantics And Blocker

The source-semantics stage fixes what b63n means in this repository. AMFlow
derives cut prescriptions from loop prescriptions, chooses a Cutkosky endpoint
direction, applies the `K_r(eps)` prefactor, and then extracts the eta-zero term
from a local endpoint model. The C++ lane must reproduce that live endpoint
functional; fitting retained final `solution` samples is a diagnostic fallback,
not runtime closure evidence.

Maintainer rule: any status, release, or qualification text must keep
`automatic_phasespace` and `feynman_prescription` pending until live
weighted-residue packet evidence replaces the retained-sample path.

### 2. Structural Cutkosky Scaffold

The structural scaffold recognizes the reviewed phase-space surfaces, cut
support, phase-volume loop count, provider strategy, branch ledger, and eta
contour. It also produced selected pure-cut evidence that proved the plumbing
could carry reviewed coefficients, but that evidence does not include the full
D2,D4,D6,D7 automatic phase-space weights or the `feynman_prescription`
conjugate subintegrals.

Maintainer rule: do not promote selected pure-cut transport, selected-master
telemetry, or retained-sample Laurent fitting as weighted-residue evidence.

### 3. Residue Carrier And Publication Gate

The residue carrier is the boundary between symbolic/analytic residue work and
publication. It records epsilon order, eta power, log power, region key,
complex coefficient, precision diagnostics, and provenance. The publication
gate exists so a coefficient-like object cannot publish when it is synthetic,
retained-sample-backed, below the required precision, or missing reviewed
derivation provenance.

Maintainer rule: every coefficient-producing b63n diff must pass through the
publication gate and preserve provenance that distinguishes synthetic fixtures,
external derivations, fresh AMFlow comparisons, and runtime-derived values.

### 4. Weighted Plan, Moment Seeds, And Cross-Validation

The weighted plan decomposes the `automatic_phasespace` target into D2,D4,D6,D7
moment weights and records that `feynman_prescription` still needs a separate
prescription-aware subintegral derivation. The moment seeds are deliberately
inert: they validate denominator identity, prefactor application, eta-zero
selection, and provenance shape without publishing benchmark coefficients. The
cross-validation gate checks the plan against the seed packet while keeping the
publication gate blocked for synthetic seeds.

Maintainer rule: changes to D2/D4/D6/D7 identities, structural forms, seed
labels, or provenance strings must update the cross-validation and fingerprint
pins together.

### 5. Scoped D7 Weighted-Residue Evidence

The scoped D7 surface is the only weighted-residue coefficient currently allowed
through the b63n publication gate. It is constrained to the reviewed
`automatic_phasespace` kinematics, the D7 denominator role, the accepted epsilon
orders, and the recorded AMFlow comparison evidence. It is useful as a reviewed
coefficient path, but it is not the full automatic phase-space row and it says
nothing by itself about the `feynman_prescription` row.

Maintainer rule: keep D7 evidence scoped. Do not infer D2/D4/D6 evidence,
feynman conjugacy, phase-0 packet qualification, or M6/M7 release readiness from
the D7 gate passing.

### 6. Audit, Fingerprint, And Blocker Guards

The post-M7 guards make the scoped surface auditable. The fingerprint pins cover
the weighted-residue evaluation plan, D7 moment seed, full moment seed packet,
moment cross-validation gate, blocked D2 scoped evaluation, and published D7
scoped evaluation. The regeneration helper must find the built
`cutkosky-weighted-residue-tests` binary and verify the pinned audit categories.
The status and D246 blocker docs keep the remaining AMFlow/Mathematica
obstacles visible.

Maintainer rule: if a future lane intentionally changes an audit category,
expected field, selected coefficient, or blocker status, regenerate the
fingerprints and update the status/blocker docs in the same reviewable unit.

## Maintainer Checklist

- Keep the six-stage order intact: semantics, structural scaffold, residue
  carrier and gate, weighted moment plan, scoped D7 evidence, audit guards.
- Treat retained AMFlow final `solution` samples as non-closing diagnostics.
- Keep selected pure-cut and scoped D7 evidence separate from full weighted
  automatic phase-space coverage.
- Require a reviewed derivation or external CAS artifact before publishing any
  new D2/D4/D6 or `feynman_prescription` coefficient.
- Require fresh AMFlow comparison evidence before packet-level promotion.
- Preserve `live_coefficients_available`, `retained_solution_samples_used`, and
  `full_eta_zero_contour_applied` as truthful status fields.
- Run the fingerprint regeneration self-check whenever audit categories,
  labels, field ordering, or scoped evaluation payloads intentionally change.
- Leave `b63n` promotion blocked until both pending phase-0 rows have accepted
  result manifests, digit scoring, failure-code audits, and M6 composer output.

## Non-Claims

This note does not add a runtime stage, does not edit b63n implementation, does
not update qualification metadata, does not publish D2/D4/D6 or
`feynman_prescription` coefficients, does not remove the D246 blocker, and does
not mark `b63n`, M6, M7, or release readiness as closed. It documents the
current maintainer contract for the post-M7 b63n weighted-residue architecture
so future lanes can change one stage at a time without weakening the gate.
