# Workflow Reference

Current durable repo status:

- authoritative `main` base is `fdbceea3cb94ee2e811573ad446e5777917c1bb0`
- reviewed implementation is accepted through `Batch 46`
- `Milestone M0a` and `Operational Gate B0/G1` are accepted
- `K0-pre-spec` is accepted as a repo-local K0 smoke fixture freeze derived from preserved input;
  latest candidate-local smoke replay job `5356840` passed
- `K0-pre` is accepted as the narrow Kira kinematics YAML contract repair for that frozen smoke
  subset; latest clean-candidate build/test job `5356948` passed
- `K0` and `K0b` are still pending
- `K0b: Honest Bootstrap Manifest And Clean K0 Acceptance Packet` resumes next
- `Batch 47` remains pending; do not claim broader K0 acceptance or automatic boundary
  equivalence yet

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
