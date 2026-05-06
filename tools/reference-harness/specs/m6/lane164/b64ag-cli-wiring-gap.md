# Lane 164 b64ag CLI Wiring Gap

## Verdict

Tier C.  The b64ag `linear_propagator` CLI path must keep
`full_eta_zero_contour_applied=false`.

The lane 159/161 six-master endpoint transport and lane 157 reduced finite-part
primitive are present, but wiring them into the production CLI cannot honestly
publish a full-contour result yet.  The current full-transport primitive is
single-epsilon scoped and serializes endpoint terms through a long-double,
24-significant-digit carrier before the post-endpoint Laurent fit.  That cannot
support the required independent AMFlow comparison at the Tier B floor of 30
digits, let alone the M6 50-digit floor.

## Evidence

- The production CLI still routes b64ag AMFlow-state input to the selected
  endpoint path.  Non-selected full packet input falls back to the scaffold, and
  the scaffold reports `boundary_unsolved` with
  `full_eta_zero_contour_applied=false`.
- A solution-stripped full `linear_propagator` state was exercised with:

  ```bash
  ./build/amflow-cli solve-series \
    /tmp/autoibp_orch/lane164/linear_full_stripped.json \
    --eps-order 2 --digits 60 \
    --out /tmp/autoibp_orch/lane164/current_full_stripped.json
  ```

  The command returned `rc=4`, `status=failed`, `failure_code=boundary_unsolved`,
  `transport_applied=false`, and `full_eta_zero_contour_applied=false`.  The
  summary showed the retained b64ag contour scaffold, not full endpoint
  coefficient publication.
- `TransportLightlikeGaugeLinkFiniteBoundaryEndpointTerms` rejects more than one
  boundary epsilon sample in a call with `insufficient_precision`, so the CLI
  would need a reviewed multi-sample wrapper before a post-endpoint Laurent fit
  can be a production result.
- The same primitive uses the runtime scalar path and formats endpoint terms at
  24 significant digits.  Downstream target reduction and Laurent fitting cannot
  recover 30 verified digits from that carrier.
- The retained b64ag target-reduction data is an inline AMFlow reduction table in
  `boundary_state.files.reduction.raw`, while `reduction.target_reduction_path`
  is empty.  The generic `kira_target.m` reducer therefore cannot supply the
  required pre-finite-part target reduction; a narrow parser for that retained
  table is still needed.
- The M6 qualifier removes `linear_propagator` from `blocked_phase0_examples`
  only through a coherent captured optional phase-0 packet.  A raw runtime flag
  flip alone is insufficient and would violate the lane 148 contract.

## Required Follow-Up

1. Add a production b64ag evaluator that parses the retained inline reduction
   table into `LightlikeGaugeLinkTargetReductionTerm` rows.
2. Promote the six-master endpoint transport to a high-precision multi-epsilon
   carrier, preserving `retained_solution_samples_used=false`.
3. For every retained epsilon sample, run finite `gaugex=1/40 -> 0` transport,
   apply the retained target reduction and D4,D5 normalization, run the
   PickZeroRuleS-compatible finite-part selector, then fit each accepted target's
   epsilon Laurent series.
4. Compare the resulting full packet against the genuine AMFlow
   `linear_propagator` golden.  Only if the comparison reaches at least 30
   digits may the CLI promote the full-contour flag; M6 promotion still requires
   the optional capture packet and failure-code audit.

Until those steps are complete and independently verified, the fail-safe state is
to keep `linear_propagator` blocked on `b64ag`.
