# Workflow Reference

The durable workflow for this repo now lives in:

- `docs/orchestration-workflow.md`

Use that document for:

- pass deployment order
- required planner/theory/verification/implementation/review inputs and outputs
- file ownership rules
- mandatory verification commands and exact verification-evidence capture
- review and fix-loop gates
- reviewer-quality checks
- roadmap dependency handling, including autonomous prerequisite backfilling for later-phase or
  later-milestone requests
- true external-blocker handling and the rule against claiming blocked or not-yet-reviewed targets
  as accepted
- explicit second-pass disposition capture in `docs/implementation-ledger.md`
- ledger and public-contract advancement rules

Compacted or restarted threads should also reload the compact-reuse roadmap and theory corpus when
the batch is full-parity planning, post-bootstrap solver/runtime work, or roadmap-owned
reference/qualification work:

- `docs/full-amflow-completion-roadmap.md`
- `references/README.md`
- `references/INDEX.md`
- `references/notes/porting-roadmap.md`
- `references/notes/theory-gap-audit.md`

This reference remains only as a pointer so compacted threads can recover the full workflow from
disk quickly, including the autonomous prerequisite backfill policy, the exact
verification-evidence requirements, and the second-pass recording requirements.
