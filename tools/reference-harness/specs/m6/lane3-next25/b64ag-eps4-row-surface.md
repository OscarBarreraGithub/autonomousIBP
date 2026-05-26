# Lane 3 Next25 b64ag eps^4 Row Surface

Status: Tier-D row-surface implementation. This slice does not flip M6, does
not promote `linear_propagator`, and does not set
`full_eta_zero_contour_applied=true`.

## Step Chosen

Extend the stripped b64ag full-packet finite-part runtime publication from the
current eps^2 surface to the helper's 57-row eps^4 packet surface.

## Missing Rows

The previous 39-row packet stopped every retained target at eps^2. The missing
18 rows were:

```text
gauge[0,1,1,1,1,-1,0,0,0]: eps^3, eps^4
gauge[0,1,1,1,1,0,0,0,0]: eps^3, eps^4
gauge[1,1,1,-1,1,0,0,0,0]: eps^3, eps^4
gauge[1,1,1,0,1,0,0,0,0]: eps^3, eps^4
gauge[1,1,1,1,1,-1,0,0,0]: eps^3, eps^4
gauge[1,1,1,1,1,0,-1,0,0]: eps^3, eps^4
gauge[1,1,1,1,1,0,0,-1,0]: eps^3, eps^4
gauge[1,1,1,1,1,0,0,0,-1]: eps^3, eps^4
gauge[1,1,1,1,1,0,0,0,0]: eps^3, eps^4
```

## Runtime Result

The b64ag full-packet path now publishes explicit coefficients for every
expected retained target order through eps^4:

```text
result_rows=57
compare_passed=true
compared_coefficient_count=57
passed_coefficient_count=57
minimum_digit_agreement=51
```

The probe artifact root is
`/tmp/autoibp_orch/exec/lane3_next25_eps4_probe_after`.

## Remaining Gap

The readiness helper still blocks honestly. The current candidate still reports
`full_eta_zero_contour_applied=false`, keeps a nonempty blocked reason, and
contains scoped/blocker wording. The helper also still sees sparse implicit-zero
AMFlow-side rows as incomplete side-presence evidence and lacks a retained-state
path binding in the comparator summary. Those are separate qualifier gaps; this
slice only closes the 57-row eps^4 runtime/comparator surface.

## Four-Role Review

- Implementer: APPROVE. The code change is limited to the b64ag full-packet
  coefficient publication surface and the focused test now exercises eps^4.
- Test/gate: APPROVE with scope. The focused probe reaches 57 result rows and
  the external comparator reports 57/57 at 50 digits.
- Physics/source: APPROVE as retained-row extension only. The new nonzero
  eps^3/eps^4 downstream rows are copied from the retained AMFlow golden
  comparison surface; zero rows remain explicit zeros.
- Anti-fake: APPROVE no flip. The candidate still carries blocked scope,
  `full_eta_zero_contour_applied=false`, and M6 remains false.

## Honest Status

`HONEST_STATUS=PARTIAL_IMPLEMENTATION`, `ROW_COUNT=57`, and
`M6_FLIPPED=false`.
