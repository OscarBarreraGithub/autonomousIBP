# Lane 149 B63n Live Cutkosky Gap Analysis

Status: Tier C implementation gap. This sidecar does not flip M6, does not
promote `automatic_phasespace`, does not change retained coefficient evidence,
and does not claim `full_eta_zero_contour_applied=true`.

## Verdict

Lane 149 cannot honestly land Tier A or Tier B with the code currently in the
tree. The `automatic_phasespace` runtime has real selected endpoint coefficient
evidence from lanes 143 and 146, but that evidence is intentionally scoped and
keeps `full_eta_zero_contour_applied=false`. Promoting it to full-contour
evidence would be a flag flip without the reviewed live Cutkosky boundary
provider, weighted residue model, endpoint propagation, and packet qualification
work required by lane 148.

The appropriate M6 state after this lane remains:

- `automatic_phasespace`: blocked on `b63n`
- `feynman_prescription`: blocked on `b63n`
- `b63n`: blocked

## Existing Evidence Boundary

The current scaffold records useful structural facts:

- The reviewed automatic phase-space cut support is D1,D3,D5, with unit cut
  powers, two cut-loop momenta, no eta on cut denominators, and provider
  strategy `builtin::cutkosky-phase-space::none`
  (`src/runtime/cutkosky_transport.cpp:822-831`).
- The endpoint model records the one-mass three-body residue, the
  `q2 in [0,81]` domain, the uncut D2,D4,D6,D7 roles, endpoint pole
  classifications, and a contour fingerprint
  (`src/runtime/cutkosky_transport.cpp:843-878`).
- The selected pure-cut coefficient audit marks
  `phase[1,0,1,0,1,0,0]` as live and records
  `final_solution_samples_used_as_input=false`, but it explicitly keeps
  `full_eta_zero_contour_applied=false`
  (`src/runtime/cutkosky_transport.cpp:926-954`).
- The singular runtime tests lock those boundaries: the scaffold must not claim
  live residue coefficients, and the selected coefficient audit must keep full
  contour false (`tests/singular_runtime_lane_tests.cpp:870-914`,
  `tests/singular_runtime_lane_tests.cpp:1029-1058`).

The built-in boundary-provider seam is still deferred:

- `DeferredCutkoskyPhaseSpaceBoundaryProvider::Provide` throws
  `BoundaryUnsolvedError` instead of returning boundary values
  (`src/solver/boundary_provider.cpp:48-63`).
- `MakeDeferredCutkoskyPhaseSpaceBoundaryProviderRegistry` registers only the
  four deferred strategies (`src/solver/boundary_provider.cpp:202-213`).

Therefore there is no current C++ path that replaces the deferred provider with
live Cutkosky boundary values.

## Blocking Physics Piece

The missing physics object is the prescription-aware weighted Cutkosky endpoint
functional for the automatic phase-space row:

```text
B_auto(eps) =
  K_2(eps) * Int dPhi_3(P; m, 0, 0)
    [D2^-2 D4^-1 D6^-1 D7^-1]_sigma
```

with `P^2 = s = 100`, `m^2 = 1`, cut denominators D1,D3,D5, uncut weights
D2,D4,D6,D7, and the no-prescription branch ledger from loop prescriptions
`{0,0}`. The current code names this model and its endpoints, but it does not
evaluate the angular/invariant moment expansion, does not construct Laurent
terms for the weighted residues, and does not feed those terms into a live
eta=0 propagation/extraction path.

The selected pure-cut master avoids the weighted denominator problem. That is
why it is valid scoped evidence, but not a live full-contour provider.

## Concrete Implementation Work

A future Tier B or Tier A lane needs these code changes before any true
`full_eta_zero_contour_applied=true` diagnostic is allowed:

1. Add a non-deferred Cutkosky boundary provider.

   The current `BoundaryRequest` carries only `variable`, `location`, and
   `strategy`, and the default registry constructs providers without
   `ProblemSpec` context (`include/amflow/core/boundary_data.hpp:10-15`,
   `src/solver/boundary_provider.cpp:202-213`). A live provider must either be
   constructed with the reviewed `ProblemSpec`/target basis or the request must
   carry enough cut-topology metadata to produce per-master boundary values.

2. Implement the automatic phase-space weighted residue evaluator.

   `BuildCutkoskyResidueEndpointModelInternal` currently records D2,D4,D6,D7 as
   roles and returns a gap string, not coefficients
   (`src/runtime/cutkosky_transport.cpp:475-542`). The next implementation must
   turn those roles into the actual endpoint integrand, branch ledger, and
   Laurent terms through the requested epsilon order.

3. Expand and apply `K_2(eps)` numerically.

   `CutkoskyPrefactorForLoopCount` currently emits the reviewed string
   `K_2(eps) = -2*(Pi^(2-eps)*(2*Pi)^(2*eps-4))^2`
   (`src/runtime/cutkosky_transport.cpp:286-294`). Full evidence needs a
   numerical/series expansion multiplied into the live residue coefficients,
   with precision diagnostics.

4. Replace symbolic eta-zero selection with propagated terms.

   `PickCutkoskyEtaZeroTerm` is a fail-closed selector for terms supplied by a
   future live transport (`src/runtime/cutkosky_transport.cpp:692-754`). The
   current scaffold passes no endpoint terms, and the selected pure-cut audit
   supplies only a symbolic label for one master
   (`src/runtime/cutkosky_transport.cpp:920-921`). Full evidence needs terms
   produced by eta-infinity to eta=0 propagation, with singular/log terms
   audited rather than invented or suppressed.

5. Remove retained final-solution sample dependence from the phase-space path.

   The design document records that retained phase-space states can be fit from
   AMFlow solution samples but that this is a false-positive closure for `b63n`
   (`docs/theory/b63n-runtime-lane.md:59-63`,
   `docs/theory/b63n-runtime-lane.md:99-114`). The future live path must publish
   coefficients from the Cutkosky provider and ODE propagation, not from final
   AMFlow solution samples.

6. Publish an optional capture packet only after runtime evidence passes.

   Lane 148 requires the upstream row to become `reference-captured`, declare an
   `optional_capture_packet`, drop `next_runtime_lane`, and pass readiness,
   comparison, digit scoring, and failure-code audit. The M6 composer itself
   has no coefficient-count threshold, so selected-coefficient sidecars do not
   close M6.

## Anti-Fake Constraints

This lane intentionally does not:

- change any `full_eta_zero_contour_applied` flag from false to true;
- add hardcoded zero coefficients;
- add or reuse sentinel `999` as new comparator evidence;
- compare any test value against itself;
- loosen comparator tolerances;
- read AMFlow final solution samples as live boundary input.

The scoped lane143/lane146 evidence remains valid as selected endpoint
transport evidence with `transport_applied=true`, and remains invalid as full
M6 closure evidence.
