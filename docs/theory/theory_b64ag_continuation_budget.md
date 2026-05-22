# b64ag Continuation-Budget Theory Plan

Status: design-only Tier C gap note. This document does not close `b64ag`,
does not set `full_eta_zero_contour_applied=true`, does not edit phase-0
qualification metadata, and does not claim M6 readiness.

## Scope

The target surface is the reviewed `linear_propagator` gauge-link row. The
relevant baseline requested for this note is commit `774bce7`, which routes
stripped full b64ag states through the live six-master finite-boundary endpoint
transport and fails closed instead of falling back to retained AMFlow solution
samples. Parallel work may already prototype the first-block Frobenius branch;
this plan treats that work as Slice 2 when absent and as a prerequisite to
verify when present.

The theory trail is:

- `c638286`: retained inline reduction parsing and high-precision per-epsilon
  endpoint term carriers.
- `dc0f0c5`: epsilon-dependent Frobenius residues for the second and downstream
  scalar blocks.
- `e973257`: non-integer Frobenius finite-part bookkeeping for a reviewed
  single-region scaffold.
- `774bce7`: full stripped packet wiring up to the current
  `continuation_budget_exhausted` blocker.

The AMFlow reference surface is `SolveIntegralsGaugeLink` in the CPC snapshot:
it applies `GenerateSquare`, multiplies the target reduction by
`gaugex^(-sum affected powers)`, solves a `gaugex` DE from a finite boundary,
calls `ExpandGaugeX`, combines master asymptotics with the reduction through
`PlusAsyExp@MapThread[TimesAsyExp, ...]`, applies `PickZeroRuleS`, and only then
fits the epsilon Laurent series.

## Docs-Only Verification Surface

This commit's fail/pass evidence is intentionally documentation-scoped. The
runtime remains unchanged, so there is no coefficient-level fail-before/pass-after
claim in this note.

The concrete fail-on-head check is that `HEAD` does not contain
`docs/theory/theory_b64ag_continuation_budget.md`; equivalently,
`git cat-file -e HEAD:docs/theory/theory_b64ag_continuation_budget.md` exits
nonzero before the docs commit.

The concrete pass-on-worktree checks are:

- the repository doc exists and is non-empty;
- `/tmp/autoibp_orch/exec/theory_b64ag_continuation_budget.md` is byte-identical;
- the required sections below are present;
- the anti-fake constraints below are present;
- `git diff --check -- docs/theory/theory_b64ag_continuation_budget.md` is clean.

Future slices below describe the runtime tests that should fail before and pass
after each code slice; they are not claimed as passing evidence for this docs-only
Tier C packet.

## Why The Current Budget Exhausts

The runtime reaches the correct branch and fails in the right direction: it
parses the retained finite `gaugex -> 1/40` boundary vector, parses the six-by-six
DE matrix, and refuses retained final `solution` samples as input. The failure is
not a need to loosen tolerances. It is a missing endpoint-region budget and
reduction ledger.

At the requested `774bce7` baseline, the first visible blocker is the first DE
block. The code matches the regular first-block Frobenius solution against the
retained boundary vector and requires the non-regular branch coefficient to be
numerically zero. For the stripped full packet that coefficient is not zero, so
transport returns `continuation_budget_exhausted` before target reduction,
`PickZeroRuleS`, or Laurent fitting. If the first-block branch is carried by a
concurrent slice, the next blockers below become the production blockers.

The follow-on budget issue is independent of that first-block branch. Lanes 170
and 172 made the scalar second/downstream blocks carry reviewed non-integer
regions such as `-7 + 8 eps` and `-2 + 4 eps`, but the endpoint transport still
uses a fixed local-power cap of 8 for Frobenius finite-boundary transport. The
retained reduction contains rows with `gaugex^(-2)` and `gaugex^(-3)` shifts, and
the affected D4/D5 normalization subtracts additional powers for several packet
targets. A target-local demand around power 11 is therefore ordinary for the
retained packet; a hard-coded cap of 8 cannot be the production budget.

The cap must also be closed under DE recurrence dependencies. In the scalar
recurrence, an off-diagonal matrix term `gaugex^m` feeding a requested row power
`p` consumes source-column power `p - 1 - m`. A retained `gaugex^-3` source can
therefore make a row term through power 11 depend on an upstream source region
through power 13. A budget computed only from the final reduced publication slot
can still truncate live recurrence inputs.

The current reduced finite-part helper also remains narrower than AMFlow. AMFlow
combines every surviving master asymptotic region with the target reduction and
only then runs `PickZeroRuleS`. In `DESolver.m`, `PickZeroRuleS` selects integer
region keys, returns zero when there is no integer key, returns zero for a
positive integer key, aborts on multiple integer keys, and otherwise extracts the
coefficient at the selected non-positive integer key. Non-integer Frobenius
regions are not separate finite-part contributors after numeric epsilon
substitution; they are regions to retain long enough to prove the AMFlow key
selection outcome was not caused by truncation.

## Clean C++ Extension

The extension should be a typed asymptotic-region ledger and a dependency-closed
budget, not a larger magic integer.

Add a small b64ag endpoint-region model beside the existing
`LightlikeGaugeLinkFinitePartTerm` carrier:

- region id: `integer` or `frobenius`;
- exponent expression evaluated at the epsilon sample;
- reviewed provenance for non-integer bases and resonance audits;
- source block/master and branch provenance;
- local integer power and log power;
- coefficient at the requested working precision;
- continuation budget metadata: requested local power, dependency-expanded local
  power, produced local power, recurrence residual, and precision estimate.

Then split endpoint transport into four explicit stages.

1. First-block branch transport.

   `EvaluateGaugeLinkFirstBlockEndpointCoefficients` should not treat the
   non-regular branch as an implicit zero. It should return both the regular
   branch and the Frobenius branch, with branch exponent
   `lambda = 6*(eps - 1)`, as ledger terms. If the branch is resonant,
   logarithmic, complex, or lacks reviewed provenance, the existing
   `continuation_budget_exhausted` failure remains correct.

2. AMFlow-key demand calculation.

   Before recurrence, compute target-local demands from the actual retained
   reduction table and affected-power normalization. The direct row demand for a
   master term is:

   `target_local_power = amflow_key - reduction.gaugex_power_shift
       + affected_power_sum`.

   For AMFlow parity, the reducer should request the integer-key window that
   `PickZeroRuleS` can observe: every non-positive integer key needed for
   extraction or dropped-term audit, every positive integer key needed to prove
   the zero-vs-abort decision, and enough neighboring terms to detect unresolved
   singular or logarithmic structure. One positive integer key gives AMFlow zero
   only after the ledger proves no second integer key is present. Non-integer
   Frobenius regions are carried through the same recurrence budget for audit,
   but they do not create their own finite-part key.

3. Dependency-closed recurrence budget.

   Expand the direct target-local demands to a fixed point over the DE Laurent
   stencil. If row `r` needs region `R` through local power `p`, then every
   source column `c` connected by a nonzero Laurent matrix power `m` must be
   available through `p - 1 - m` for the same region. Iterate that back-propagation
   over the six-master graph until no demanded power increases, then add a small
   guard used for residual checks. If the fixed point crosses a reviewed ceiling,
   fail with `continuation_budget_exhausted` and publish the demanded edge that
   exceeded the ceiling.

4. AMFlow-order reduced finite-part selection.

   Replace the single-region shortcut with a reducer that first applies target
   reduction and affected-power normalization to all endpoint ledger terms, then
   groups the reduced expression by AMFlow key after numeric epsilon
   substitution. Safe outcomes are exactly the `PickZeroRuleS` outcomes:
   no integer key gives audited zero, one positive integer key gives audited zero,
   one non-positive integer key gives the selected coefficient while recording
   dropped lower terms, and multiple integer keys fail closed. Any unresolved
   logarithm, unknown region provenance, or dependency-truncated ledger remains
   `continuation_budget_exhausted`.

The production CLI must continue to keep `full_eta_zero_contour_applied=false`
until an independent AMFlow comparison and phase-0 packet evidence exist. A
successful internal transport is necessary but not sufficient for M6 closure.

## Reviewer-Acceptable Slicing Plan

Slice 1: budget and parity audit only.

Add a non-publishing audit that parses the retained target reduction, evaluates
AMFlow-key demands per target/master, expands them through the DE dependency
graph, and reports direct power, dependency-expanded power, reduction shift,
affected-power sum, region provenance, and blocking edge. Tests should prove
that retained `gaugex^-2` and `gaugex^-3` rows can demand powers above the old
cap of 8, including a dependency-expanded demand past the direct target slot. No
endpoint coefficients are published.

Slice 2: first-block region ledger.

Extend or verify the first-block transport result so both regular and
non-regular branches are carried as endpoint-region terms. Synthetic tests should
reconstruct the finite `gaugex=1/40` boundary from both branches at
retained-style epsilon samples. The path still fails closed before packet
publication if downstream reduction cannot consume the ledger.

Slice 3: dynamic recurrence order.

Replace the fixed Frobenius transport cap with the dependency-closed
required-power calculation. Tests should cover retained reduction shifts `-2`
and `-3`, affected-power normalization, off-diagonal `gaugex^-3` recurrence
dependencies, and residual reporting. Failure mode for unsupported or excessive
regions remains `continuation_budget_exhausted`.

Slice 4: AMFlow-order `PickZeroRuleS` reducer.

Implement the reducer that combines target reduction and normalization first,
then applies integer-key `PickZeroRuleS` semantics. Tests should include one
selected non-positive integer key, one no-integer-key audited zero, one
positive-integer-key audited zero, one multiple-integer-key rejection, one
logarithmic rejection, and one dependency-truncated rejection.

Slice 5: stripped-packet integration.

Run the full stripped `linear_propagator` packet through finite boundary
transport, dependency-closed recurrence, target reduction, `PickZeroRuleS`
selection, and post-endpoint Laurent fitting without retained final solution
samples. This slice may report `success=true` for internal runtime transport,
but it must keep `full_eta_zero_contour_applied=false` until external comparison
evidence exists.

Slice 6: qualification evidence.

Recapture an AMFlow golden at a precision sufficient for the M6 50-digit floor,
compare the C++ packet against that golden, publish the failure-code audit, and
only then consider an optional phase-0 packet with
`full_eta_zero_contour_applied=true`. M6 can close only through the lane148
phase-0 packet contract, not through a raw runtime flag flip.

## Anti-Fake Constraints

- Do not consume retained AMFlow final `solution` samples as boundary input.
- Do not replace the first-block Frobenius branch by an implicit zero.
- Do not increase a fixed local-power cap and call that AMFlow parity.
- Do not treat non-integer Frobenius regions as finite-part contributors unless
  the AMFlow integer-key selector can actually observe them after reduction.
- Do not publish zero unless the ledger proves the relevant AMFlow integer-key
  outcome and the recurrence budget was dependency complete.
- Do not collapse multiple integer endpoint keys; fail closed as AMFlow aborts.
- Do not set `full_eta_zero_contour_applied=true` without a full packet
  comparison at the required digit floor and the phase-0 qualification sidecars.
