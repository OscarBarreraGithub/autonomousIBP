# Lane 4 B64ag Runtime Packet Anti-Fake Audit Gap

Status: Tier C gap. This sidecar does not flip M6, does not promote
`linear_propagator`, does not add an optional capture packet, and does not claim
`full_eta_zero_contour_applied=true`.

## Intended Step

Lane 4 selected the b64ag runtime-packet anti-fake audit as a complement to
lane 1's b64ag CLI wiring claim. The intended code step was a reference-harness
audit that would reject any `linear_propagator` packet unless the runtime result
itself proves:

- `continuation.transport_applied=true`;
- `continuation.full_eta_zero_contour_applied=true`;
- `continuation.variable=gaugex` and `target_location=gaugex=0`;
- solver precision at or above the M6 50-digit floor;
- no selected-endpoint, scaffold, retained-cache, or solution-sample
  `runtime_application` / `transport_scope`;
- no nonempty `blocked_reason`;
- no deferred full-contour wording;
- explicit provenance that final AMFlow solution samples were not used as
  runtime input.

The audit should also reject the existing selected b64ag evidence under
`tools/reference-harness/specs/m6/lane147/linear_propagator.selected4-lightlike.cpp-result.json`,
because that result keeps `full_eta_zero_contour_applied=false`, uses selected
endpoint scope, and reports the full gauge-link endpoint transport as deferred.

## Blocker

The mandatory four-role sub-orchestration did not produce review output in any
of three rounds. Each worker was spawned with `bash ~/bin/codex_worker.sh`,
`--out` files under `/tmp/autoibp_orch/exec/`, backgrounded and disowned as
requested, but the workers stopped after the Codex stdin phase and wrote no
role reports:

- Role A implementer: no `APPROVE` / `REQUEST_CHANGES` output.
- Role B test writer: no post-implementation approval output.
- Role C AMFlow-parity / physics reviewer: no output.
- Role D numerical-soundness reviewer: no output.

Because unanimous approval was not available within three rounds, this lane
does not land the unreviewed audit code. The correct follow-up is to implement
the audit above and rerun the four-role review when sub-agent execution is
healthy.

## Anti-Fake Boundary

This lane intentionally does not:

- change any runtime result JSON;
- change packet qualification metadata;
- weaken comparison or digit-scoring gates;
- mark the selected b64ag lane145/lane147 evidence as full contour evidence;
- assert parity with AMFlow final solution samples;
- remove `linear_propagator -> b64ag` from the runtime-lane frontier.
