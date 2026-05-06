# Lane 162 B63n Residue Evaluator Decomposition

Status: Tier C gap analysis. This document does not implement a weighted
Cutkosky residue evaluator, does not publish new coefficients, does not consume
AMFlow final solution samples, and does not change `full_eta_zero_contour_applied`.

## Verdict

Rank 18 should stay blocked. The current runtime has useful fail-closed
recognition, contour-planning, prefactor, and selected pure-cut audit primitives,
but it still does not construct the automatic phase-space weighted residue or
the `feynman_prescription` prescription-aware subintegrals.

The honest next unit is not a full evaluator. It is a sequence of smaller,
reviewable tasks that separate code plumbing from new physics derivation.

## Existing Boundary

- Surface recognition is exact and narrow. The automatic phase-space path
  accepts family `phase`, loops `{l1,l2}`, cuts D1,D3,D5, target
  `phase[1,2,1,1,1,1,1]`, and the pinned kinematics
  (`src/runtime/cutkosky_transport.cpp:373-394`). The selected pure-cut audit is
  a different scoped surface, `phase[1,0,1,0,1,0,0]`
  (`src/runtime/cutkosky_transport.cpp:396-418`).
- The `feynman_prescription` path accepts family `loopxloop`, cuts D9,D10,
  target `loopxloop[0,1,1,1,1,0,1,1,1,1,0,0]`, and pinned
  `s=10, msq=1, m2sq=2/5` kinematics
  (`src/runtime/cutkosky_transport.cpp:420-447`).
- The automatic phase-space endpoint model records variables, domain, uncut
  denominator roles, factors, and the explicit coefficient gap
  (`src/runtime/cutkosky_transport.cpp:521-550`), but those roles are not turned
  into angular or invariant moment coefficients.
- The `feynman_prescription` endpoint model records the two conjugate loop
  ledgers, uncut denominator roles, factors, and its explicit unintegrated
  classification (`src/runtime/cutkosky_transport.cpp:551-588`).
- Endpoint pole extraction and contour planning are structural only
  (`src/runtime/cutkosky_transport.cpp:699-736`).
- `PickCutkoskyEtaZeroTerm` is a fail-closed selector over already propagated
  terms. It rejects empty input, logs, multiple regions, missing eta-zero terms,
  ambiguous eta-zero terms, and unlabeled selected terms
  (`src/runtime/cutkosky_transport.cpp:739-801`).
- The prefactor series and multiplication helpers are implemented as numerical
  primitives, but they require explicit residue terms and do not create those
  terms (`src/runtime/cutkosky_transport.cpp:804-879`,
  `src/runtime/cutkosky_transport.cpp:882-955`).
- The transport scaffold marks live coefficients unavailable, retained solution
  samples unused, and full contour false (`src/runtime/cutkosky_transport.cpp:992-1079`).
- The selected pure-cut coefficient audit is explicitly selected-master scoped:
  it marks a live selected pure-cut residue, keeps retained samples unused, keeps
  full contour false, and says weighted automatic phase-space and
  `feynman_prescription` residues remain deferred
  (`src/runtime/cutkosky_transport.cpp:1082-1155`).
- Runtime tests lock these boundaries for the b63n scaffold, selected pure-cut
  audit, and fail-closed mutations (`tests/singular_runtime_lane_tests.cpp:993-1238`).

## Subtask Ranking

Rank 1 is most code-tractable. Rank 9 is the physics blocker that must not be
implemented without external review.

| Rank | Subtask | Current code anchor | Tractability | Required review before coefficients |
| ---: | --- | --- | --- | --- |
| 1 | Preserve anti-fake invariants while adding any future hooks: no AMFlow final solution sample input, no implicit coefficients, no full-contour flag flip. | `src/runtime/cutkosky_transport.cpp:1035-1041`, `src/runtime/cutkosky_transport.cpp:1062-1078`, `tests/singular_runtime_lane_tests.cpp:997-1041`, `tests/singular_runtime_lane_tests.cpp:1156-1185` | Code-tractable now. Add regression tests before touching evaluator code. | No physics review needed; this is guardrail coverage. |
| 2 | Keep surface classifiers exact and add rejected-fixture coverage for any new accepted topology. | `src/runtime/cutkosky_transport.cpp:373-447`, `src/runtime/cutkosky_transport.cpp:590-594`, `tests/singular_runtime_lane_tests.cpp:1188-1238` | Code-tractable now. The accepted topology set should stay narrow. | Human review only if a new topology is admitted. |
| 3 | Normalize an explicit residue-term data model: epsilon order, eta power, log power, region key, complex coefficient, precision diagnostics, and provenance. | `include/amflow/runtime/cutkosky_transport.hpp:44-78`, `src/runtime/cutkosky_transport.cpp:739-801`, `src/runtime/cutkosky_transport.cpp:882-955` | Code-tractable now. This can be introduced without computing coefficients. | No formula review if the model is inert and tests use synthetic coefficients. |
| 4 | Add a non-publishing fixture path that feeds synthetic residue terms through prefactor multiplication and `PickCutkoskyEtaZeroTerm`. | `src/runtime/cutkosky_transport.cpp:804-955`, `tests/singular_runtime_lane_tests.cpp:890-991`, `tests/singular_runtime_lane_tests.cpp:1115-1147` | Code-tractable now. This validates algebra and fail-closed selection only. | No physics review if fixtures are visibly synthetic and not benchmark parity. |
| 5 | Extend branch-ledger representation from strings to structured prescriptions, cut support, eta half-plane, and branch provenance. | `src/runtime/cutkosky_transport.cpp:112-125`, `src/runtime/cutkosky_transport.cpp:182-239`, `src/runtime/cutkosky_transport.cpp:649-689`, `include/amflow/runtime/cutkosky_transport.hpp:80-113` | Mostly code-tractable. It should remain non-publishing until checked against AMFlow source behavior. | AMFlow-source review for prescription mapping and contour direction. |
| 6 | Build automatic phase-space symbolic integrand assembly for D2,D4,D6,D7 weights without evaluating endpoint Laurent coefficients. | `src/runtime/cutkosky_transport.cpp:525-550`, `tests/singular_runtime_lane_tests.cpp:1017-1027` | Medium. Code can map roles to a structured integrand, but must not emit numeric coefficients. | Physics review for denominator-to-angular-variable mapping before evaluation. |
| 7 | Build `feynman_prescription` symbolic subintegral assembly for the plus-minus and minus-plus ledgers without evaluating endpoint Laurent coefficients. | `src/runtime/cutkosky_transport.cpp:551-588`, `src/runtime/cutkosky_transport.cpp:663-688`, `tests/singular_runtime_lane_tests.cpp:1044-1087` | Medium. Code can make the ledger structured, but coefficient evaluation is blocked. | Physics review for prescription-aware one-loop subintegrals and conjugacy. |
| 8 | Define the CAS/paper-check contract and golden-generation protocol for one accepted non-zero coefficient. | `src/runtime/cutkosky_transport.cpp:1126-1155`, `tests/singular_runtime_lane_tests.cpp:1149-1185` | Process-tractable, not coefficient-producing by itself. | Independent paper derivation or external CAS notebook, with no AMFlow final solution samples as inputs. |
| 9 | Implement the weighted residue evaluator for automatic phase-space and the companion `feynman_prescription` evaluator. | `src/runtime/cutkosky_transport.cpp:521-588`, `src/runtime/cutkosky_transport.cpp:992-1079`, `tools/reference-harness/specs/m6/lane149/b63n-live-cutkosky-gap-analysis.md:56-75`, `docs/milestones/m6-runtime-lane-implementation-roadmap.md:64` | Hardest. This is the actual Rank 18 blocker. | Required before any coefficient claim: reviewed derivation, paper reference, external CAS check, or human physics review; Role D must independently verify any non-zero claim. |

## Code-Tractable Work Now

The safest near-term implementation work is infrastructure that remains
coefficient-free:

1. Add a richer residue-series carrier in
   `include/amflow/runtime/cutkosky_transport.hpp`, keeping the current
   `CutkoskyEtaZeroTerm` selector contract intact
   (`include/amflow/runtime/cutkosky_transport.hpp:44-78`,
   `src/runtime/cutkosky_transport.cpp:739-801`).
2. Add synthetic-only tests proving that prefactor multiplication never invents
   missing orders, zero terms, logs, or region choices
   (`src/runtime/cutkosky_transport.cpp:882-955`,
   `tests/singular_runtime_lane_tests.cpp:890-991`).
3. Replace string-only branch ledger details with structured fields while
   serializing the same audit summaries
   (`src/runtime/cutkosky_transport.cpp:649-689`,
   `include/amflow/runtime/cutkosky_transport.hpp:80-113`).
4. Add documentation and tests that any future non-zero coefficient path must
   cite an external derivation/CAS artifact and must preserve
   `full_eta_zero_contour_applied=false` until both b63n rows are coefficient
   complete (`src/runtime/cutkosky_transport.cpp:1035-1041`,
   `src/runtime/cutkosky_transport.cpp:1152-1154`).

## Physics-Blocked Work

These tasks must not be coded as numeric coefficient producers from local
intuition:

1. Automatic phase-space weighted moments for D2,D4,D6,D7. The runtime only
   records those denominator roles today (`src/runtime/cutkosky_transport.cpp:532-545`).
   The missing derivation is the angular/invariant moment expansion and its
   endpoint Laurent series.
2. Endpoint branch and singular-region accounting for the automatic path. The
   runtime records candidate endpoint poles (`src/runtime/cutkosky_transport.cpp:450-482`)
   but does not decide which singular terms survive after the weighted
   integration.
3. Prescription-aware `feynman_prescription` loop subintegrals. The runtime
   records `T_l1` and `T_l2` ledgers (`src/runtime/cutkosky_transport.cpp:570-583`)
   but does not evaluate them.
4. AMFlow-parity coefficient publication. The only live audit is the selected
   pure-cut endpoint coefficient (`src/runtime/cutkosky_transport.cpp:1126-1155`);
   it is not evidence for the weighted target or for the companion
   `feynman_prescription` row.

## External Review Inputs

Any future Tier B or Tier A coefficient claim should attach at least one of the
following review artifacts in the commit body or sidecar, before Role D approval:

- R. E. Cutkosky, "Singularities and Discontinuities of Feynman Amplitudes",
  Journal of Mathematical Physics 1, 429 (1960), DOI
  `10.1063/1.1703676`, for the cut/discontinuity foundation.
- Xiao Liu, Yan-Qing Ma, Wei Tao, Peng Zhang, "Calculation of Feynman loop
  integration and phase-space integration via auxiliary mass flow",
  arXiv:`2009.07987`, DOI `10.1088/1674-1137/abc538`, for phase-space AMF and
  eta-flow validation.
- Xiao Liu and Yan-Qing Ma, "AMFlow: a Mathematica package for Feynman integrals
  computation via Auxiliary Mass Flow", arXiv:`2201.11669`, CPC 283 (2023)
  108565, for AMFlow implementation semantics and benchmark expectations.
- E. Byckling and K. Kajantie, "Particle Kinematics", Phys. Rev. 187, 2008
  (1969), DOI `10.1103/PhysRev.187.2008`, or an equivalent reviewed
  phase-space factorization reference for the automatic phase-space angular and
  invariant moment reduction.
- An external CAS notebook that derives at least one non-zero weighted residue
  coefficient without reading AMFlow final solution samples, together with a
  reproducible comparison against fresh AMFlow output.

This lane does not use those references to assert a coefficient formula. They
are the minimum review inputs for a later coefficient-producing lane.

## Tier B Breakout Candidate

The least risky Tier B follow-up is not the full Rank 18 evaluator. It is:

1. Implement the inert residue-series carrier and synthetic transport chain.
2. Pick exactly one weighted automatic phase-space master and one epsilon order.
3. Obtain an independent paper/CAS derivation for that single non-zero
   coefficient.
4. Compare the C++ result against fresh AMFlow output at 30 or more correct
   digits, without using AMFlow final solution samples as runtime input.
5. Keep every other automatic phase-space and `feynman_prescription` target
   documented as gap.

Without step 3, the only honest result remains Tier C.

## Anti-Fake Gate

For this Tier C lane, the acceptable diff is a documentation-only sidecar. A
future coefficient-producing diff must be rejected if it contains any of:

- hardcoded benchmark zeros or sentinel `999` digit evidence;
- `full_eta_zero_contour_applied=true` without complete automatic phase-space and
  `feynman_prescription` coefficient evidence;
- self-comparison tests;
- AMFlow final solution sample reads as runtime inputs;
- tolerance loosening;
- selected pure-cut evidence reused as full weighted-residue parity.
