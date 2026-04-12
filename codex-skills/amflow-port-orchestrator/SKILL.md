---
name: amflow-port-orchestrator
description: Use this skill when planning or coordinating AMFlow migration work across multiple agents. It points to the durable repo workflow for planning, corpus scrutiny, verification design, implementation, review, and reference-harness bring-up.
---

# AMFlow Port Orchestrator

## Overview

Use this skill when the task is to plan, coordinate, or review the AMFlow C++ port as a multi-agent program. The purpose is to keep migration work aligned with the frozen parity contract and the reference corpus in `references/`.

## Workflow

Use the durable repo workflow in `docs/orchestration-workflow.md`.

For implementation batches, run these passes in order:

1. `migration-plan`
   Produce the exact batch scope, owned files, acceptance criteria, tests, and defers.
2. `corpus-scrutiny`
   Audit the batch against the AMFlow snapshot corpus and separate explicit behavior from inference.
3. `verification-design`
   Define exact expected outputs, edge cases, rejection cases, and verification commands.
4. `implementation`
   Edit only the frozen owned surface, update tests, and update ledger/contract docs as required.
5. `review`
   Run an independent findings-first review after local verification, then a second-pass review or review-quality audit before acceptance, with the second-pass disposition captured explicitly in `docs/implementation-ledger.md`.

For phase-wide or multi-item requests, produce a feasibility split before implementation:

- which requested items are directly executable now
- which unmet prerequisite batches or milestones must be backfilled first in dependency order
- which requested items are blocked by true external dependencies or artifacts
- which next atomic batch is the first feasible implementation step

If a later requested phase or milestone is not directly feasible because roadmap prerequisites are
not yet met, treat the request as authorization to backfill the missing locally feasible
prerequisites automatically in dependency order. Stop only when the requested target becomes
feasible or a true external blocker halts the dependency chain.

Do not overclaim acceptance while backfilling:

- each prerequisite still needs the normal verification, review, and ledger/public-contract updates
  before it counts as satisfied
- report backfilled prerequisite work separately from the originally requested phase or milestone
- if an external blocker prevents the dependency chain from reaching the requested target, report
  that blocker explicitly and do not claim the blocked target as complete

Run the `reference-harness` track when the batch touches upstream capture, pinned goldens, or harness scripts.

## Required Inputs

Always load:

- `docs/orchestration-workflow.md`
- `docs/implementation-ledger.md`
- `docs/public-contract.md`
- `docs/parity-contract.md`
- `docs/verification-strategy.md`
- `references/snapshots/amflow/*`
- `references/snapshots/kira/*`
- `specs/parity-matrix.yaml`

Additionally load these when the task is full-parity planning, post-bootstrap solver/runtime
work, or a roadmap-owned reference/qualification milestone:

- `docs/full-amflow-completion-roadmap.md`
- `references/README.md`
- `references/INDEX.md`
- `references/notes/porting-roadmap.md`
- `references/notes/theory-gap-audit.md`

## Required Outputs

- one implementation plan with atomic steps
- one scrutiny report listing must-cover subtleties and failure modes
- one verification plan with phase gates, acceptance thresholds, exact commands, exact outcomes,
  and the artifact or behavior each command confirmed
- one reference-harness plan or implementation update with pinned environment assumptions, only
  when the batch touches upstream capture, pinned goldens, or harness scripts
- one explicit feasibility and blocker split whenever a mixed phase or milestone request requires
  prerequisite backfill, is only partially feasible, or hits a true external blocker in the current
  environment

## Non-Negotiable Rules

- Mathematica is validation-only, never part of the production runtime.
- Kira is an external reducer boundary in v1.
- Singular kinematics and precision control are first-class workstreams.
- Canonical-basis assumptions are forbidden in the core solver design.
- Any new mode or option must update both the parity contract and verification coverage.

## Reuse

When continuing work after compaction or in a new Codex thread, point the new agent to:

- this skill
- `docs/orchestration-workflow.md`
- `docs/implementation-ledger.md`
- `docs/public-contract.md`
- `docs/parity-contract.md`
- `docs/verification-strategy.md`
- `specs/parity-matrix.yaml`

Also reload these when the task is full-parity planning, post-bootstrap solver/runtime work, or a
roadmap-owned reference/qualification milestone:

- `docs/full-amflow-completion-roadmap.md`
- `docs/reference-harness.md`
- `references/README.md`
- `references/INDEX.md`
- `references/notes/porting-roadmap.md`
- `references/notes/theory-gap-audit.md`

For invariant-generated or solver-adjacent batches, those `references/` documents are part of
the compact-reuse theory corpus, not optional background reading.
