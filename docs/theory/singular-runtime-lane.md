# Singular Runtime Lane Design

Status: lane47 failure-safe design artifact. This is not a runtime
implementation and does not retire the current `one-singular-endpoint-case`
`b62p` blocker.

## Review Consensus

Lane47 used four roles:

- Role A audited milestone and case-study docs and confirmed the blocker is the
  `one-singular-endpoint-case` runtime lane, currently published as `b62p`.
- Role B audited the C++ runtime and rejected a small retirement patch: direct
  real singular endpoints have a narrow Frobenius path, but complex
  branch-aware `eta -> 0` endpoint extraction remains explicitly deferred.
- Role C audited the qualification scripts and confirmed the gate is
  metadata-driven: while readiness lists the singular family as runtime-blocked,
  `qualify_case_study_families.py` blocks before numeric evidence can qualify.
- Role D audited Lane47 anti-fake claims and approved the design-only outcome:
  future implementation claims must be checked against real C++ runtime output
  and comparator sidecars, not synthetic summaries or scaffold-only metadata.

## Current Blocker

The singular runtime lane is the missing live runtime path for complex
auxiliary-mass-flow continuation into a singular `eta = 0` endpoint, followed by
branch-aware endpoint extraction and coefficient-bearing comparison evidence.

The current C++ runtime has useful pieces, but not the whole lane:

- `BootstrapSeriesSolver` supports a narrow direct-real regular-start to
  regular-singular-target path on reviewed Frobenius subsets.
- Complex eta continuation planning exists, but
  `MaybeMakeComplexEtaZeroBranchDeferredDiagnostics(...)` returns
  `unsupported_solver_path` when the reviewed complex target evaluates to
  `eta=0`.
- `SolveWithReviewedLiveComplexEtaContinuationPlan(...)` checks the same
  deferral before manifest writing or solver handoff.
- `FinalizeEtaContinuationContour(...)` rejects ordinary contour points that
  land on evaluated singular points, so a target singular endpoint needs a
  separate endpoint contract, not ordinary waypoint treatment.
- The retained `automatic_loop` CLI path still reports
  `full_eta_zero_contour_applied: false`; selected endpoint coefficient
  transport is retained evidence, not full singular contour execution.

Therefore the concrete code/evidence needed to retire the lane is not another
guardrail-only slice. It is a live complex `eta -> 0` endpoint runtime path with
accepted branch semantics, endpoint local analysis, and numeric evidence for the
case-study family.

## Non-Retirement Criteria

These artifacts do not retire the singular runtime lane:

- deleting `next_runtime_lane` metadata from the qualification scaffold;
- adding a synthetic numeric sidecar without a C++ runtime run and accepted
  comparison;
- reusing retained `automatic_loop` selected endpoint coefficients as if they
  were full complex contour execution;
- adding only physical-region guardrails or near-singular typed failures outside
  the complex endpoint path;
- changing `qualify_case_study_families.py` to ignore
  `runtime_blocked_case_study_ids`.

## Acceptance Evidence

A future retirement lane must publish all of the following:

- a C++ runtime path that does not return the current complex `eta=0`
  `unsupported_solver_path` diagnostic for the reviewed singular endpoint case;
- a continuation/endgame audit object that records half-plane or prescription,
  endpoint singular identity, branch winding or equivalent branch ledger,
  endpoint local model, and extraction order;
- comparator-readable C++ output for one singular-endpoint case-study family;
- an AMFlow or otherwise accepted golden for that same singular endpoint case;
- `compare_case_study_numeric_results.py` evidence for
  `one-singular-endpoint-case` meeting the default `>=50` digit threshold;
- `qualify_case_study_families.py` evidence where the singular family is no
  longer in `runtime_blocked_case_study_ids`;
- unchanged tolerance policy and explicit non-claims for M6 unless every
  phase-0 and case-study prerequisite also passes.

## Incremental Milestones

### SRL-0: Preserve Fail-Closed Baseline

Keep the current complex `eta=0` deferral and tests intact while the design is
implemented in slices. The expected diagnostic remains explicit
`unsupported_solver_path`, and no cache or continuation-plan manifest should be
written on the deferred endpoint path.

Exit evidence:

- focused tests proving complex `target_location=eta=0` still fail before
  manifest/cache writes;
- qualification summaries still preserve `one-singular-endpoint-case -> b62p`.

### SRL-1: Endpoint-Aware Contour Contract

Extend the contour data model so a singular target endpoint can be represented
as an endpoint, not an ordinary contour point. The contour validator should keep
rejecting interior singular crossings, but accept a reviewed target endpoint
only when it is explicitly marked as the endpoint singular.

Exit evidence:

- manifest schema with endpoint-singular fields;
- tests rejecting unmarked singular endpoints and accepting the marked reviewed
  endpoint without solver execution;
- deterministic contour fingerprint including endpoint-singular metadata.

### SRL-2: Endpoint Local Model

Add endpoint local analysis for the reviewed complex `eta=0` generated system.
This must identify the residue/local exponent data needed for Frobenius or
logarithmic endpoint extraction and must fail closed on resonances or matrix
forms outside the reviewed subset.

Exit evidence:

- local-model sidecar for the reviewed singular endpoint;
- typed failures for unsupported fractional/logarithmic/resonant cases;
- tests showing direct-real Frobenius support is not silently reused for complex
  branch-sensitive extraction.

### SRL-3: Branch And Prescription Ledger

Define how the selected half-plane or Feynman prescription fixes logs, powers,
and endpoint approach direction. The ledger must be attached to the solve
request and emitted in runtime diagnostics/results.

Exit evidence:

- branch ledger fields in the continuation/endgame audit;
- tests distinguishing upper/lower or plus/minus prescription when they produce
  different branch data;
- fail-closed behavior for missing or contradictory prescription metadata.

### SRL-4: Live Endpoint Extraction

Execute the reviewed complex contour plus endpoint local extraction to produce
coefficients at `eta=0`. This is the first point where the current complex
`eta=0` deferral can be removed for the reviewed subset.

Exit evidence:

- C++ solve output with `full_eta_zero_contour_applied: true` or an equivalent
  reviewed runtime flag for the singular endpoint path;
- no synthetic coefficient insertion or zero-fill fallback;
- regression tests covering cache replay, manifest mismatch, and branch-ledger
  mismatch.

### SRL-5: Case-Study Numeric Qualification Row

Freeze one accepted singular-endpoint golden and publish the matching C++
numeric evidence sidecar for `one-singular-endpoint-case`.

Exit evidence:

- explicit sidecar consumed by `compare_case_study_numeric_results.py`;
- minimum observed correct digits at least `50`;
- `qualify_case_study_families.py` no longer reports the singular runtime lane
  as a blocker for that family, while still reporting any missing non-singular
  case-study numerics.

### SRL-6: M6 Composer Integration

Only after SRL-5 and the independent phase-0/case-study prerequisites pass,
compose the final M6 verdict.

Exit evidence:

- no pending phase-0 runtime lanes;
- passing phase-0 packet-set verdict;
- passing case-study-family verdict for all selected families;
- `qualify_milestone_m6.py` passes without claiming M7 or release readiness.

## Lane47 Outcome

Lane47 lands this design because the remaining singular runtime lane requires
multi-slice physics/runtime work. The current repository state should continue
to preserve the `b62p` blocker until SRL-4 and SRL-5 provide real runtime and
numeric evidence.
