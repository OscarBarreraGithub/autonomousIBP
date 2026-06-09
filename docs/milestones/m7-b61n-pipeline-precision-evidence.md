# M7 B61n Pipeline Precision Evidence

Status: post-M7 quality note. M7 remains closed; this note does not promote
`complex_kinematics` and does not change release signoff.

## Question

The post-M7 row 5/6 comparator now reports the four remaining b61n targets as
`matched-to-reference-floor` rather than true 50-digit matches:

```text
box[1,0,1,1] eps^0: 11/11 retained AMFlow floor digits
box[1,1,1,1] eps^-2: 46/46 retained AMFlow floor digits
box[1,1,1,1] eps^-1: 12/13 retained AMFlow floor digits
box[1,1,1,1] eps^0: 12/12 retained AMFlow floor digits
```

The alternative-path question was whether our pipeline is itself capped at that
floor, or whether the retained AMFlow reference is the ceiling in the external
comparison.

## Attempted Full Recompute

A fresh stripped b61n solve-series recomputation was attempted from a detached
worktree:

```text
./build/amflow-cli solve-series build/b61n-precision-evidence/complex_kinematics.stripped.json --eps-order 0 --digits 80 --out build/b61n-precision-evidence/complex_kinematics.stripped.eps0.digits80.cpp-result.json
```

That full recomputation stayed CPU-bound for more than 10 minutes and did not
write an output file before this lane scoped down under the task instruction.
No precision claim is made from that interrupted attempt.

## Scoped Evidence

The successful c267 stripped C++ pipeline artifact already contains exact
rational payloads for the same four row 5/6 coefficients:

```text
tools/reference-harness/specs/m7/lane2/complex_kinematics.c267-stripped.eps0.cpp-result.json
```

Those exact C++ payloads were re-rendered without AMFlow reference values at 80,
120, and 160 fractional digits. The compact machine-readable evidence is:

```text
tools/reference-harness/specs/m7/lane2/b61n-pipeline-precision-evidence.json
```

Summary:

```text
target_count=4
all_standard_serializations_match_exact_rational_80=true
all_uplifted_160_serializations_extend_standard_80=true
all_uplifted_tail_after_standard_80_has_nonzero_digit=true
all_uplifted_160_fraction_digits_exceed_reference_floor=true
minimum_reference_floor_digits=11
minimum_uplifted_fraction_digits=160
```

Per-target result:

```text
box[1,0,1,1] eps^0: AMFlow floor 11/11; our exact payload renders 160/160 fractional digits
box[1,1,1,1] eps^-2: AMFlow floor 46/46; our exact payload renders 160/160 fractional digits
box[1,1,1,1] eps^-1: AMFlow floor 12/13; our exact payload renders 160/160 fractional digits
box[1,1,1,1] eps^0: AMFlow floor 12/12; our exact payload renders 160/160 fractional digits
```

## Interpretation

This evidence supports the reference-floor classification for the external
comparison: the retained AMFlow reference does not expose enough digits to test
the four row 5/6 targets beyond its declared floor, while the C++ c267 pipeline
artifact itself carries coefficient payloads that can be serialized beyond that
floor.

This is narrower than a fresh `cpp_dec_float_200` or halved-RK-step recompute.
It proves that the currently emitted c267 row 5/6 coefficient payload is not
publication-serialization-limited to the AMFlow floor. It does not prove direct
coefficient-state publication, and it does not remove the existing structural
blocker:

```text
failure_code=coefficient-state-publication-unclosed-target-graph
target_coefficients_reconstructed_from_epsilon_samples=true
target_coefficients_published_from_coefficient_state=false
```

`M7_PARITY_SIGNOFF_FLIPPED=false`
