# Lane 1 Next 22 b64ag First-Block Structure Gap

Status: Tier C diagnostic. This step does not flip M6, does not promote
`linear_propagator`, does not relax comparator tolerance, does not inject
retained final solution samples, and does not set
`full_eta_zero_contour_applied=true`.

## Step Chosen

Audit the structural endpoint surface for the two direct b64ag first-block
masters that still cap at 37 digits:

- `gauge[1,1,1,0,1,0,0,0,0]`
- `gauge[1,1,1,-1,1,0,0,0,0]`

This slice is intentionally separate from lane3's `b64ag_eps0_trace`: it does
not trace the epsilon-zero runtime values. It records the retained AMFlow
matrix entries, the `gaugex=0` pole orders, the reviewed sheared indicial roots,
and the final AMFlow reference shape.

The executable audit is:

```text
tools/reference-harness/scripts/audit_b64ag_first_block_structure.py
```

Run artifact:

```text
/tmp/autoibp_orch/exec/lane1_next22/b64ag-first-block-structure.json
```

## Findings

The AMFlow final reference for these rows is already post-`PickZeroRuleS`
numeric Laurent output. The retained final output has no explicit `eta`,
`gaugex`, `Log[...]`, `PolyLog`, or `HPL` tokens, so the current retained
reference does not expose a local symbolic log/polylog series to compare
directly.

The original first-block DE matrix involving the two masters is singular at
`gaugex=0`:

```text
A00 = (1+20*gaugex-18*eps*gaugex)/(gaugex*(1+2*gaugex))
A01 = (24*(-1+eps)*gaugex)/(1+2*gaugex)
A10 = (5-5*eps+22*gaugex-22*eps*gaugex)/(gaugex^2*(1+2*gaugex))
A11 = (2*(-3+3*eps-14*gaugex+14*eps*gaugex))/(gaugex*(1+2*gaugex))
```

Pole orders at `gaugex=0` are:

```text
A00: 1
A01: 0
A10: 2
A11: 1
```

The reviewed C++ first-block model shears the basis to:

```text
y = j[gauge,1,1,1,0,1,0,0,0,0] / gaugex
w = j[gauge,1,1,1,-1,1,0,0,0,0] - (5/6)*y
```

In that sheared model the endpoint is regular singular with residue roots:

```text
0
-6 + 6 eps
```

At fixed nonzero epsilon samples this is log-free and diagonal in the reviewed
model, but at `eps=0` the second root is integer-shifted by `-6`. Expanding
`gaugex^(-6+6 eps)` in epsilon generates powers of `log(gaugex)`. That is the
structural distinction for these two masters: coefficient-level parity depends
on how AMFlow's fixed-epsilon `SolveAsyExp` plus `PickZeroRuleS` treats the
sheared Frobenius branch, not on adding more local series terms.

The retained target reduction rows are direct first-block rows:

```text
gauge[1,1,1,0,1,0,0,0,0]    -> {gaugex^(-1),0,0,0,0,0}
gauge[1,1,1,-1,1,0,0,0,0]   -> {0,1,0,0,0,0}
```

That makes these two rows structurally different from the downstream rows that
now pass the 50-digit comparator.

## Required Follow-Up

The next non-duplicative check is to compare AMFlow `SolveAsyExp` /
`PickZeroRuleS` fixed-epsilon coefficients for the sheared singular branch
against the C++ first-block Frobenius branch that is currently carried for
boundary reconstruction but skipped before target finite-part extraction. The
highest-value term is the order-six sheared Frobenius term, because it is the
first term that can sit on the finite-part surface after the `gaugex^-1` target
row.

This iteration did not extract those per-order AMFlow `AsyExp` branch
coefficients, so it does not honestly identify a single wrong local coefficient
order. It narrows the structural target for that extraction.

## Four-Role Review

- Implementer: APPROVE Tier C. The audit is fail-closed and checks the retained
  matrix/reduction contract before reporting the structural interpretation.
- Independent reviewer: APPROVE Tier C. The result separates fixed-epsilon
  log-free Frobenius behavior from the unresolved epsilon-Laurent log surface
  and does not claim a coefficient fix.
- Source auditor: APPROVE Tier C. The retained AMFlow output is already
  post-`PickZeroRuleS` numeric Laurent output; the DE matrix itself carries the
  first-block double pole in the original basis.
- Anti-fake: APPROVE Tier C. The report leaves M6 blocked, keeps the current
  37-digit floor, and requires an explicit AMFlow branch-coefficient extraction
  before any promotion claim.

## Honest Status

`MIN_DIGIT_AGREEMENT_AFTER=37`, `M6_FLIPPED=false`.
