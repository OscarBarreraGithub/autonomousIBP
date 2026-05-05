# M6 b61n Runtime Lane 124

Lane: `lane124`
Runtime lane: `b61n`
Benchmark: `complex_kinematics`
Status: partial runtime evidence only. No M6 closure is claimed.

## Evidence Captured

Lane124 ran the current C++ `solve-series` path on the retained
`complex_kinematics` AMFlow state at 80 digits:

```bash
./build/amflow-cli solve-series \
  tools/reference-harness/specs/phase0/complex_kinematics.amflow-state.json \
  --eps-order 0 \
  --digits 80 \
  --out tools/reference-harness/specs/m6/lane124/complex_kinematics.digits80.cpp-result.json
```

The fresh comparator then passed the retained AMFlow golden at the M6
50-digit floor:

```bash
python3 tools/reference-harness/scripts/compare_cpp_vs_amflow.py \
  --cpp-result tools/reference-harness/specs/m6/lane124/complex_kinematics.digits80.cpp-result.json \
  --amflow-golden tools/reference-harness/specs/phase0/complex_kinematics.golden-manifest.json \
  --tolerance-digits 50 \
  > tools/reference-harness/specs/m6/lane124/complex_kinematics.digits80.compare50.json
```

The comparison passed `14/14` coefficients with minimum digit agreement `59`.

## Failure-Safe Gate

This evidence is real runtime output, but it is not enough to close `b61n`.
The output reports:

- `runtime_boundary_provider: retained-loop-solution-sample-cache-laurent-fit`
- `runtime_application: loop-solution-sample-laurent-fit`
- `transport_applied: false`
- `full_eta_zero_contour_applied: false`
- `blocked_reason: full complex eta-contour endpoint reconstruction remains deferred after retained loop solution-sample coefficient fitting`

Because the `b61n` requirement is a live complex-kinematics eta-continuation
runtime capture accepted as an optional phase-0 packet, lane124 leaves
`complex_kinematics` pending in the M6 scaffold. No `next_runtime_lane` value is
removed, and no `optional_capture_packet` is added.

`qualify_milestone_m6.py` was re-run against the current retained lane115
phase-0 and case-study summaries. It still reports `b61n` in
`blocked_runtime_lanes`, with `complex_kinematics` in `phase0_pending_ids`.
That is the expected failure-safe result for this partial evidence packet.

## Next Step

Close `b61n` only after the C++ runtime can execute the reviewed complex
eta-contour endpoint path for `complex_kinematics` and produce packet-set
evidence that passes the M6 readiness, packet comparison, correct-digit, and
failure-code audit tools together.
