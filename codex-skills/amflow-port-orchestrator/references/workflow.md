# Workflow Reference

Current durable repo status:

- authoritative `main` base is `95f33f398bbdebf2084bf360a498fea3de89fc30`
- reviewed implementation is accepted through landed `Batch 50b`
- `Milestone M0a` and `Operational Gate B0/G1` are accepted
- `K0-pre-spec` is accepted as a repo-local K0 smoke fixture freeze derived from preserved input;
  latest candidate-local smoke replay job `5356840` passed
- `K0-pre` is accepted as the narrow Kira kinematics YAML contract repair for that frozen smoke
  subset; latest clean-candidate build/test job `5356948` passed
- `K0` and `K0b` are accepted only for the narrow repo-local frozen smoke subset via one coherent
  retained reducer-smoke packet with an honest bootstrap manifest on `main`
- `Batch 47` / `Milestone M2`, `Batch 48`, `Batch 49`, `Batch 49b`, `Batch 50a`, and `Batch 50b`
  are accepted narrowly on `main`
- `Batch 50b` is landed on `main` at `95f33f3`: internal topology-prerequisite bridge and prereq
  snapshot for `Branch` / `Loop` are accepted, and the current in-flight local worktree covers
  `Batch 50` through `Batch 54`
- local module-loaded configure/build/ctest passed for the landed `Batch 50b` packet; the current
  in-flight local worktree spans the selector slice, multi-invariant list wrappers, mass widening,
  multi-top-sector Kira, and precision preflight + retry controller
- clean-candidate attempt `5480669` is not acceptance evidence for landed `Batch 50b`; it
  OOM-killed during configure because the submission omitted an explicit memory request
- local `Batch 50` through `Batch 54` already exist; the next orchestration task is to land them
  on `main` in order

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
