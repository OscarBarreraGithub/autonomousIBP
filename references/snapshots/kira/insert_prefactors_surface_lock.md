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

## Non-Claims

- This retained lock is a prerequisite for later `KiraBackend` and `jobs.yaml`
  wiring only; it does not claim any live backend behavior change in the
  current workspace.
- This xints-like denominator surface is distinct from AMFlow's overall
  loop-prefactor helper `BuildOverallAmflowPrefactor(...)`.
- no Kira `insert_prefactors` wiring is claimed here
- The retained AMFlow `globalpreferred` artifact at
  `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260419-required-set/generated-config/phase0/automatic_vs_manual/primary/cache/tt_amflow/10/globalpreferred`
  is cited only as a distinction point and is not claimed here to already be a
  Kira `insert_prefactors` file.
