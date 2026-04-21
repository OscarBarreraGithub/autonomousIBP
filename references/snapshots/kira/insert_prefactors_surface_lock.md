# Kira Insert Prefactors Surface Lock

This note freezes the upstream Kira `insert_prefactors` file shape used by the
repo-local prerequisite surface in `specs/kira-insert-prefactors-surface.yaml`.
The retained repo-local xints fixture is intentionally narrow and normalized to
the serializer contract, rather than a full verbatim copy of the upstream
example file.

## Source Pointers

- retained example file: `references/snapshots/kira/xints`
- retained example job: `references/snapshots/kira/jobs.yaml`
- retained option summary:
  `references/snapshots/kira/job_options_summary.txt:77`

## Locked Observations

- Kira uses an external xints-like text file for `insert_prefactors`; the
  example job points to it via `insert_prefactors: [xints]`.
- The upstream example file is one entry per line in the form
  `<integral>*1/(<denominator>)`.
- The retained repo-local xints fixture keeps a narrow reviewed subset of that
  shape and normalizes integral labels to the serializer form
  `<integral.Label()>*1/(<denominator>)`.
- The Kira option summary states that `insert_prefactors` is only available for
  `run_firefly`, that the first integral is the coefficient-1 anchor, and that
  later entries carry rational-function coefficients to divide out and restore.

## Current Scope

- Narrow opt-in `KiraBackend` wiring now consumes this exact surface and emits
  `insert_prefactors: [xints]` only on `run_firefly`-emitting paths.
- That wiring remains explicit and conservative: it requires an explicit
  repo-local `KiraInsertPrefactorsSurface`, preserves the serializer line
  shape, and rejects unsupported cuts, unresolved surfaces, family/arity
  mismatches, and multi-target preparation batches.
- Explicit public emission calls reject invalid opt-in requests
  deterministically instead of silently suppressing `xints`, while preparation
  paths keep the companion file absent and report validation messages.
- Parsed `kira2math` coefficients normalize Kira-local `prefactor[...]` and
  `prefactor(...)` wrappers away before they reach evaluator-facing reduction
  results.
- This xints-like denominator surface is distinct from AMFlow's overall
  loop-prefactor helper `BuildOverallAmflowPrefactor(...)`; the emitted payload
  is still distinct from `BuildOverallAmflowPrefactor(...)`.
- The retained AMFlow `globalpreferred` artifact at
  `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260419-required-set/generated-config/phase0/automatic_vs_manual/primary/cache/tt_amflow/10/globalpreferred`
  is cited only as a distinction point and is not claimed here to already be a
  Kira `insert_prefactors` file.
