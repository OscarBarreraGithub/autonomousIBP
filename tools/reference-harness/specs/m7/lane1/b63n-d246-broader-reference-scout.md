# B63n D246 Broader Reference Scout

Status: post-M7 quality scout. This note does not publish D2/D4/D6
coefficients, does not modify the D246 sidecar, and does not promote `b63n`.

## Result

No in-tree AMFlow Mathematica reference data was found for publishable
D2/D4/D6 weighted-residue coefficient values.

The existing sidecar remains the honest state:

```text
tools/reference-harness/specs/m6/lane146/b63n-d246-weighted-residue-reference-evidence.json
```

It is still `skeleton=true`, `passed=false`, and all D2, D4, and D6
`coefficients` arrays are empty.

## Inventory

The scout scanned 209 AMFlow/Mathematica-style data candidates under
`references/` and `tools/reference-harness/specs/`, using these file classes:

- `*.wl`
- `*.m`
- `*amflow-golden.txt`
- `*golden-manifest.json`
- `*amflow-state.json`
- `*compare*.json`
- `*cpp-result.json`
- `*evidence.json`

Literal `D2`, `D4`, `D6`, or `D7` labels appeared only in:

| Path | Finding |
| --- | --- |
| `tools/reference-harness/specs/m6/lane146/b63n-d246-weighted-residue-reference-evidence.json` | D246 skeleton and source/schema contract only; no coefficients. |
| `tools/reference-harness/specs/m6/lane1-next24/linear_propagator.b64ag-full-packet-finite-part.cpp-result.json` | Unrelated `b64ag` D4/D5 lightlike-propagator normalization; not phase-space and not b63n. |

Phase-space or `automatic_phasespace` candidate artifacts inspected:

| Path | Finding |
| --- | --- |
| `tools/reference-harness/specs/phase0/automatic_phasespace.amflow-state.json` | Retained AMFlow cache/state with selected masters and solution samples. It records phase-space metadata but not D2/D4/D6 selected-weight reference values. Retained final solution samples are disallowed as D246 publication input. |
| `tools/reference-harness/specs/phase0/automatic_phasespace.golden-manifest.json` | Pointer to the retained upstream AMFlow golden output. No inline D2/D4/D6 selected-weight decomposition. |
| `tools/reference-harness/specs/phase0/lane12-packet-validation-aggregate.json` | Aggregate mentions `phase[1,2,1,1,1,1,1]`, but lane12 had no C++ result JSON and no D2/D4/D6 decomposition. |
| `tools/reference-harness/specs/phase0/lane24-packet-validation-aggregate.json` | Same retained phase0 golden comparison path; `phase[1,2,1,1,1,1,1]` passes at about 40 real digits, below the 50-digit D246 sidecar floor and without D2/D4/D6 decomposition. |
| `tools/reference-harness/specs/phase0/lane32-packet-validation-aggregate.json` | Same retained phase0 golden comparison path; no independent D246 reference. |
| `tools/reference-harness/specs/m5/proof-runs/lane45/automatic_phasespace.cpp-result.json` | C++ retained-solution-sample ingest result. It contains the full target `phase[1,2,1,1,1,1,1]` through eps^-3..eps^0, but it is not upstream Mathematica raw output and does not split D2, D4, and D6 scoped weights. |
| `tools/reference-harness/specs/m5/comparisons/lane39/automatic_phasespace.compare.json` | Comparator against the retained phase0 AMFlow golden. No D2/D4/D6 selected-weight values. |
| `tools/reference-harness/specs/m5/comparisons/lane45/automatic_phasespace.compare.json` | Same retained phase0 AMFlow golden comparison. The full target is present at 30-digit tolerance only and is not D246 sidecar evidence. |
| `tools/reference-harness/specs/m6/lane143/automatic_phasespace.first-cutkosky.amflow-golden.txt` | First selected pure-cut coefficient for `phase[1,0,1,0,1,0,0]`; no D2/D4/D6 weighted residue. |
| `tools/reference-harness/specs/m6/lane143/automatic_phasespace.first-cutkosky.compare30.json` | Comparison for the lane143 selected pure-cut coefficient only. |
| `tools/reference-harness/specs/m6/lane143/automatic_phasespace.first-cutkosky.cpp-result.json` | Runtime selected pure-cut result only. |
| `tools/reference-harness/specs/m6/lane143/automatic_phasespace.first-cutkosky.stripped-result.json` | Stripped runtime selected pure-cut result only. |
| `tools/reference-harness/specs/m6/lane143/b63n-first-real-coefficient-evidence.json` | Evidence for the first pure-cut selected coefficient only. |
| `tools/reference-harness/specs/m6/lane146/automatic_phasespace.selected4-cutkosky.amflow-golden.txt` | Selected4 AMFlow golden slice for four selected masters, including D7-scoped `phase[1,1,1,0,1,0,1]`; no `phase[1,2,1,1,1,1,1]` D246 target and no D2/D4/D6 decomposition. |
| `tools/reference-harness/specs/m6/lane146/automatic_phasespace.selected4-cutkosky.compare30.json` | Selected4 comparison at 30-digit tolerance, not D246 sidecar evidence. |
| `tools/reference-harness/specs/m6/lane146/automatic_phasespace.selected4-cutkosky.cpp-result.json` | Runtime selected4 result; no D246 target. |
| `tools/reference-harness/specs/m6/lane146/automatic_phasespace.selected4-cutkosky.stripped-result.json` | Stripped selected4 result; no D246 target. |
| `tools/reference-harness/specs/m6/lane146/b63n-selected4-real-coefficients-evidence.json` | Evidence for lane146 selected4 only. It strengthens D7 scoped parity but does not solve D2/D4/D6. |

The phase1 and case-study `run.wl` / `verify_output.wl` files are AMFlow
capture harness scripts for other loop-integral examples. They do not define
the `phase` family, the `automatic_phasespace` cut support, or D2/D4/D6
weighted-residue outputs.

## D7 Same-Source Check

The existing D7 scoped publication is corroborated by the older retained
`automatic_phasespace` comparison artifacts:

```text
phase[1,1,1,0,1,0,1] eps^0
= 0.00003072064900647741498508445978252335...
```

This value appears in `m5/comparisons/lane39` and `m5/comparisons/lane45` and
matches the lane146 D7 eps^0 literal. That is not a second independent
reference source: the m5 comparison and lane146 selected4 data trace back to
the retained phase0 AMFlow capture/golden path. It should be treated only as
same-source corroboration.

## External Data Request

Ask the AMFlow upstream maintainers for a high-precision Mathematica reference
run for exactly this surface:

```mathematica
AMFlowInfo["Family"] = phase;
AMFlowInfo["Loop"] = {l1, l2};
AMFlowInfo["Leg"] = {p1, p2};
AMFlowInfo["Conservation"] = {};
AMFlowInfo["Replacement"] = {p1^2 -> 0, p2^2 -> 0, (p1+p2)^2 -> s};
AMFlowInfo["Propagator"] = {
  l1^2-msq,
  (l1+p1)^2,
  l2^2,
  (l1+l2+p1)^2,
  (l1+l2+p1+p2)^2,
  (l1+l2+p2)^2,
  (l1+p2)^2
};
AMFlowInfo["Numeric"] = {s -> 100, msq -> 1};
AMFlowInfo["Prescription"] = {0, 0};
AMFlowInfo["Cut"] = {1, 0, 1, 0, 1, 0, 0};
integrals = {j[phase,1,2,1,1,1,1,1]};
```

Request the Cutkosky ending / eta-zero selected integer-term Laurent series for
the same one-mass three-body phase-space surface, with these scoped uncut
weight entries:

| Weight | Denominator index | Power | Structural form |
| --- | ---: | ---: | --- |
| D2 | 1 | 2 | `inverse_denominator_weight[D2(q2,cos_theta_a)]` |
| D4 | 3 | 1 | `inverse_denominator_weight[D4(q2,cos_theta_a,cos_theta_b)]` |
| D6 | 5 | 1 | `inverse_denominator_weight[D6(q2,cos_theta_a,cos_theta_b)]` |

For each of D2, D4, and D6, request:

1. The complex decimal coefficients for a shared contiguous epsilon range,
   preferably eps^0..eps^3 or broader if AMFlow naturally returns poles.
2. The eta power, log power, and region key for each selected term; the
   sidecar currently expects `eta_power=0`, `log_power=0`, and
   `region_key="integer"` unless upstream demonstrates otherwise.
3. The exact AMFlow version or source commit, source-file hashes, precision
   requested, working precision, epsilon order, and noninteractive command.
4. The raw Mathematica output file and sha256 before any C++ post-processing.
5. The run log and any warning/error output.
6. A short extraction note explaining whether the numbers are direct
   projections from `j[phase,1,2,1,1,1,1,1]` or a reviewed set of separate
   scoped targets. If separate scoped targets are used, include the mathematical
   projection back to the full target above.
7. Confirmation that no retained final solution samples from this C++ port were
   used as input and that the values are not synthetic fixtures.

Do not accept only the existing full-target retained-sample comparison for
`phase[1,2,1,1,1,1,1]`. It lacks the D2/D4/D6 scoped decomposition, is below the
50-digit sidecar floor in the stored m5 comparisons, and is explicitly outside
the anti-fake contract for D246 publication.
