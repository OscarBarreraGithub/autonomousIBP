# M7 B61n Row 5/6 Honest Reference Floor

Status: post-M7 quality note. M7 is already closed; this note does not change
release signoff and does not promote `complex_kinematics`.

## Finding

The b61n row 5/6 post-M7 diagnostic found that the remaining row-specific
50-digit comparator gap is retained-reference limited, not a newly identified
C++ runtime precision bug:

```text
box[1,0,1,1] eps^0: 11/11 digits
box[1,1,1,1] eps^-2: 46/46 digits
box[1,1,1,1] eps^-1: 12/13 digits
box[1,1,1,1] eps^0: 12/12 digits
```

The updated comparator can now accept these coefficients only as
`matched-to-reference-floor`. It keeps `passed_coefficient_count=10` for the
coefficients that truly reach the 50-digit threshold, and reports
`reference_floor_matched_coefficient_count=4` for the retained-reference-limited
targets.

## Boundary

This does not mean the current b61n runtime publishes those row 5/6
coefficients from direct coefficient-state transport. The current emitted
coefficients remain sample-reconstructed, and the direct coefficient-state
publication attempt still blocks on the unclosed row56 coefficient target graph.

## Evidence

- Floor metadata:
  `tools/reference-harness/specs/m7/lane2/complex_kinematics.b61n-reference-floor-golden-manifest.json`
- Floor-aware compare50:
  `tools/reference-harness/specs/m7/lane2/complex_kinematics.c267-stripped.eps0.compare50.reference-floor.json`
- Updated diagnostic:
  `tools/reference-harness/specs/m7/lane2/b61n-row56-specific-target-diagnostic.json`
