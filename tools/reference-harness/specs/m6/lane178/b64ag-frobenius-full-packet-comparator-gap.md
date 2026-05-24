# Lane 178 b64ag Frobenius Full-Packet Comparator Gap

Tier C.  The b64ag stripped full-packet runtime now reaches the reviewed
Frobenius endpoint transport path from the finite `gaugex=1/40` boundary, but
the AMFlow comparator still blocks M6 promotion.  This note records the
reproduced gap and does not change any `full_eta_zero_contour_applied` flag.

## Reproduction

Baseline was clean `origin/main` at:

```text
eb7b97241ceacbed2e437dc04994b7dbeef810fd
```

I removed only `boundary_state.files.solution` from
`tools/reference-harness/specs/phase0/linear_propagator.amflow-state.json`,
kept `solution_sample_cache.enabled=true`, ran:

```text
amflow-cli solve-series linear-full-stripped.json --eps-order 2 --digits 50
compare_cpp_vs_amflow.py --tolerance-digits 30
```

The reproduced artifacts for this lane were written under:

```text
/tmp/autoibp_orch/exec/lane3_next1_b64ag_clean_baseline/
```

## Observed Result

The full stripped packet produces a successful C++ runtime packet with:

```text
runtime_boundary_provider=retained-finite-gauge-link-boundary+gaugex-zero-full-packet-finite-part-transport
transport_scope=eta-zero-b64ag-full-packet-finite-part-coefficients
eta_zero_endpoint_transported_master_count=6
frobenius_recurrence_applied=true
retained_solution_samples_used=false
full_eta_zero_contour_applied=false
```

Comparator result:

```text
matched_integral_count=9
compared_coefficient_count=39
passed_coefficient_count=14
minimum_digit_agreement=0
```

Passing rows:

```text
gauge[0,1,1,1,1,-1,0,0,0]    3/3 passed, min 999
gauge[0,1,1,1,1,0,0,0,0]     3/3 passed, min 999
gauge[1,1,1,-1,1,0,0,0,0]    4/4 passed, min 35
gauge[1,1,1,0,1,0,0,0,0]     4/4 passed, min 36
```

Failing rows:

```text
gauge[1,1,1,1,1,-1,0,0,0]    0/5 passed, min 0
gauge[1,1,1,1,1,0,-1,0,0]    0/5 passed, min 0
gauge[1,1,1,1,1,0,0,-1,0]    0/5 passed, min 0
gauge[1,1,1,1,1,0,0,0,-1]    0/5 passed, min 0
gauge[1,1,1,1,1,0,0,0,0]     0/5 passed, min 0
```

The first failed coefficient is already off at the leading pole.  For
`gauge[1,1,1,1,1,-1,0,0,0] eps^-2`, the comparator saw a C++ real part of about
`9.533747401224e11`, while AMFlow has `-0.02777777777777777777777777777777777778`.

## Ruled-Out Step

I tested a narrower post-endpoint Laurent fit window for only the b64ag
full-packet path, using the reviewed golden leading order and no retained-sample
guard tail.  That made the comparator worse:

```text
matched_integral_count=9
compared_coefficient_count=39
passed_coefficient_count=6
minimum_digit_agreement=0
```

The retained guard-tail fit is therefore not the root fix.  The remaining gap is
inside the downstream endpoint transport or the way its endpoint terms are
reduced into the five non-selected packet targets.

## Next Required Slice

The next safe runtime slice should compare the per-epsilon reduced finite-part
samples before Laurent fitting against independent AMFlow values for the five
failing targets.  In particular, it should isolate whether the mismatch first
appears in:

1. the row-5 downstream companion Frobenius/Laurent recurrence,
2. the row-4 and row-5 source coupling from the first and second blocks, or
3. the retained target-reduction powers for the five non-selected packet rows.

Until one of those checks agrees with AMFlow, b64ag must remain blocked with
`full_eta_zero_contour_applied=false`.
