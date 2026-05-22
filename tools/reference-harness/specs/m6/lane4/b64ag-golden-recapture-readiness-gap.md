# Lane 4 B64ag Golden Recapture Readiness Gap

Status: Tier C gap. This sidecar does not add a recapture helper, does not
recapture `linear_propagator` goldens, does not promote an optional phase-0
packet, and does not claim `full_eta_zero_contour_applied=true`.

## Intended Step

Lane 4 selected b64ag golden-recapture harness wiring as a different
b64ag-adjacent step from the lane 1 CLI-wiring claim and the previous lane 4
runtime-packet anti-fake audit gap. The intended helper would consume:

- one candidate `linear_propagator` C++ result;
- one C++-vs-AMFlow comparison summary;
- the retained `linear_propagator` AMFlow state.

The helper would report whether the candidate is ready to be used as a
golden-recapture packet candidate. It must not qualify M6 by itself. M6 still
requires the upstream phase-0 packet set to become qualified and the
`linear_propagator -> b64ag` runtime-lane blocker to disappear through coherent
phase-0 metadata.

## Required Fail-Closed Checks

The review converged on these minimum checks before such a helper can land:

- require explicit `runtime_lane = "b64ag"`;
- require explicit `final_solution_samples_used_as_input = false` at top-level
  runtime provenance and in the runtime diagnostic provenance object;
- reject selected-endpoint, scaffold, retained-cache, solution-sample, deferred,
  or blocked wording in runtime application, transport scope, boundary provider,
  summary, status, and continuation status text;
- require `continuation.variable = "gaugex"`, finite start
  `gaugex -> 1/40`, target `gaugex=0`, and singular point `gaugex=0`;
- require the retained AMFlow state to publish the exact ordered six-master
  gauge-link DE basis:
  `gauge[1,1,1,0,1,0,0,0,0]`,
  `gauge[1,1,1,-1,1,0,0,0,0]`,
  `gauge[0,1,1,1,1,0,0,0,0]`,
  `gauge[0,1,1,1,1,-1,0,0,0]`,
  `gauge[1,1,1,1,1,0,0,0,0]`,
  `gauge[1,1,1,1,1,-1,0,0,0]`;
- require the exact retained nine-target packet surface across runtime targets,
  runtime results, and comparison integrals;
- require full-contour diagnostic buckets for contour, poles, finite-part
  extraction, target reduction, precision, and provenance, with non-placeholder
  fields such as contour fingerprint, nonempty nonzero pole list,
  `PickZeroRuleS`, `ir_subtraction_applied=true`, finite-part order zero,
  dropped singular powers, reduction fingerprint, target-row count, working
  digits, and epsilon sample count;
- recompute comparison digit readiness from detailed per-coefficient entries,
  not only top-level summary fields;
- require at least the retained packet coefficient coverage, currently 57
  detailed coefficient rows, with every retained target carrying coefficient
  details;
- require both real and imaginary digit fields per compared coefficient,
  `passed=true` on each coefficient, and consistency with top-level passed and
  compared counts;
- reject all-`999` sentinel-only digit evidence;
- bind comparison provenance to the exact candidate C++ result and retained
  AMFlow golden/state inputs so summaries from another packet cannot be mixed
  into a b64ag recapture verdict.

## Existing Evidence Rejection

The current selected b64ag evidence under
`tools/reference-harness/specs/m6/lane147/linear_propagator.selected4-lightlike.cpp-result.json`
must remain rejected for golden recapture readiness. It has selected endpoint
scope, only four transported targets, a nonempty deferred full-transport
blocked reason, no full-contour diagnostic packet, comparison tolerance 30,
and minimum digit agreement 37. It is valid scoped selected-coefficient
evidence, but not a `linear_propagator` optional phase-0 recapture packet.

## Four-Role Review Result

Three usable sub-agent review rounds were completed after two initial empty
spawn attempts. Reports are retained under `/tmp/autoibp_orch/exec/`:

- Round 1: `lane4_next2_subagent_A_3.md`,
  `lane4_next2_subagent_B_3.md`, `lane4_next2_subagent_C_3.md`,
  `lane4_next2_subagent_D_3.md`.
- Round 2: `lane4_next2_subagent_A_4.md`,
  `lane4_next2_subagent_B_4.md`, `lane4_next2_subagent_C_4.md`,
  `lane4_next2_subagent_D_4.md`.
- Round 3: `lane4_next2_subagent_A_5.md`,
  `lane4_next2_subagent_B_5.md`, `lane4_next2_subagent_C_5.md`,
  `lane4_next2_subagent_D_5.md`.

Role B and Role D approved the final numerical and self-check shape. Role A
and Role C still requested changes in the final round: comparison provenance
binding, both-real-and-imag per-coefficient validation, selected/solution-sample
token scanning of boundary provider and status text, exact `gaugex -> 1/40`
state boundary pinning, and nonempty pole diagnostics.

Because unanimous approval was not available within three usable rounds, the
unapproved helper code was not landed. This Tier C sidecar records the exact
remaining contract so a later lane can implement the helper without weakening
the b64ag anti-fake boundary.

## Anti-Fake Boundary

This lane intentionally does not:

- change any runtime result JSON;
- add a new reference-harness recapture helper;
- edit phase-0 qualification metadata;
- change any `full_eta_zero_contour_applied` flag;
- recapture or promote `linear_propagator` goldens;
- weaken comparison, digit-scoring, or failure-code gates;
- remove `linear_propagator -> b64ag` from the runtime-lane frontier.
