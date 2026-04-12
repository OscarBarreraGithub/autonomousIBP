# Workflow Reference

Current durable repo status:

- authoritative `main` base is `fdbceea3cb94ee2e811573ad446e5777917c1bb0`
- reviewed implementation is accepted through `Batch 46`
- `Milestone M0a` and `Operational Gate B0/G1` are accepted
- `K0a` is accepted via clean-candidate `sapphire` job `5315267`
- `K0` is still pending
- the next atomic milestone is `K0b: Honest Bootstrap Manifest And Clean K0 Acceptance Packet`
- `Batch 47` remains pending; do not claim automatic boundary equivalence yet

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
