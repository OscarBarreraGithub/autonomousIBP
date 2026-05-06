# Lane 168 b64ag CLI Wiring Gap

## Verdict

Tier C.  The production b64ag `linear_propagator` CLI path must keep
`full_eta_zero_contour_applied=false`.

Lane 165 resolved the retained inline reduction parser and the high-precision
multi-epsilon value carrier, but the first full CLI wiring attempt exposed a
remaining endpoint-transport blocker: the six-master transport primitive is
still restricted to integer scalar endpoint residues for the second and
downstream blocks.  The retained AMFlow `linear_propagator` DE matrix has
epsilon-dependent non-integer residues at the real retained epsilon samples, so
the runtime correctly fails closed before target reduction, finite-part
selection, Laurent fitting, or AMFlow comparison.

## Reproduction

After wiring the full retained packet path locally, the attempted production run
was:

```bash
./build/amflow-cli solve-series \
  tools/reference-harness/specs/phase0/linear_propagator.amflow-state.json \
  --eps-order 2 --digits 80 \
  --out /tmp/autoibp_orch/exec/lane168/linear_propagator.full-contour.cpp-result.json
```

The command returned `rc=4`, `status=failed`, `failure_code=boundary_unsolved`,
`transport_applied=false`, and `full_eta_zero_contour_applied=false`.  The
runtime summary was:

```text
b64ag finite-boundary endpoint transport rejected the reviewed second-block self Laurent coefficient; retained_solution_samples_used=false; full_eta_zero_contour_applied=false.
```

## Evidence

- The retained `linear_propagator` DE matrix carries the second-block diagonal
  cell
  `(-7 + 8*eps - 18*gaugex + ...)/(gaugex*(1 + 2*gaugex)*(1 + 4*gaugex))`,
  so its `gaugex^-1` residue is `-7 + 8 eps`.
- The retained epsilon samples are near zero.  For example:
  - `eps=101/208000` gives `-7 + 8 eps = -6.996115384615384615384615384615...`
  - `eps=1/2000` gives `-7 + 8 eps = -6.9960`
  - `eps=131/208000` gives `-7 + 8 eps = -6.994961538461538461538461538461...`
- The existing transport primitive checks the second-block self Laurent
  coefficient against an epsilon-independent integer expectation and then uses
  `RequireIntegerResidue(...)` before building scalar endpoint series.
- The current unit coverage exercises this block at epsilon samples `1` and
  `2/2`, where `-7 + 8 eps = 1`.  That validates the high-precision carrier but
  does not cover the retained small-epsilon production samples.
- Because transport fails before reduced target finite parts are available, no
  honest AMFlow digit comparison can be published for the full retained packet.

## Required Follow-Up

1. Extend the six-master gauge-link endpoint transport to handle
   epsilon-dependent Frobenius exponents for the second and downstream scalar
   blocks, not only integer-residue endpoint series.
2. Add unit coverage using retained-style small rational epsilon samples such as
   `101/208000`, not only `1` or `2/2`.
3. Re-run the full b64ag production CLI bridge:
   finite `gaugex=1/40 -> 0` transport for every retained epsilon sample,
   retained target reduction, D4/D5 normalization, PickZeroRuleS finite-part
   selection, and post-endpoint epsilon Laurent fitting for all nine retained
   targets.
4. Only after the full retained packet compares against the genuine AMFlow
   golden at at least the Tier B 30-digit floor may any production path set
   `full_eta_zero_contour_applied=true`.  M6 closure still requires the 50-digit
   floor, optional capture packet, and failure-code audit.

Until the non-integer Frobenius endpoint transport is implemented and compared,
`linear_propagator` remains blocked on `b64ag`.
