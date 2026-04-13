# AMFlow Port Orchestration Workflow

This document is the durable continuation guide for incremental AMFlow C++ port batches in this repository. It defines when each subagent pass is deployed, what each pass must read and produce, how file ownership is frozen, how verification and review are gated, and when `docs/public-contract.md` and `docs/implementation-ledger.md` are allowed to advance.

This workflow complements, rather than replaces:

- `docs/parity-contract.md`
- `docs/public-contract.md`
- `docs/full-amflow-completion-roadmap.md`
- `docs/verification-strategy.md`
- `docs/reference-harness.md`
- `specs/parity-matrix.yaml`

## Scope

Use this workflow for any implementation batch that extends the accepted AMFlow C++ port. The default unit of work is one narrow, reviewable seam recorded as a single batch in `docs/implementation-ledger.md`.

The workflow is optimized for the current repo shape:

- production runtime is C++ only
- Kira and Fermat stay external
- Mathematica is validation-only and lives only in the reference harness
- accepted work is tracked batch-by-batch in `docs/implementation-ledger.md`
- public seams are frozen incrementally in `docs/public-contract.md`

## Current Durable Status

- authoritative `main` base is `bbd7b744b69a413bf34e4b706cd737e2b266256a`
- reviewed code remains accepted through landed `Batch 50a` on `main`; `Milestone M1` is complete
- `Milestone M0a` is accepted as reference-harness/bootstrap readiness only
- `Operational Gate B0/G1` is accepted; the clean-candidate `sapphire` verification packet passed
  at job `5305579`
- `K0-pre-spec` is accepted as a repo-local K0 smoke fixture freeze derived from preserved input;
  latest candidate-local smoke replay job `5356840` passed
- `K0-pre` is accepted as the narrow Kira kinematics YAML contract repair for that frozen smoke
  subset; latest clean-candidate build/test job `5356948` passed
- `K0b.1` is accepted on that frozen repo-local smoke subset: clean-candidate job `5425248`
  passed, coherent packet job `5425379` passed on `sapphire`, and the canonical retained root
  `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/k0/reducer-smoke` is coherent and complete
- `K0` and `K0b` are therefore closed only for that narrow repo-local frozen smoke subset: durable
  docs and review packets may claim one coherent retained reducer-smoke packet with an honest
  bootstrap manifest on `main`, but still may not widen into broader smoke or parity acceptance
- `Batch 47` / `Milestone M2` are now accepted narrowly as behavioral-equivalence evidence on the
  supported simple Euclidean massless sample subset only; this does not widen the accepted
  runtime/public surface beyond reviewed `Batch 46`
- `Batch 48` is accepted on `main`: final accepted clean-candidate `sapphire` job `5439311`
  cleared the landing packet and commit `f4bf8af2419a20f04ae40eceebbd5d12f3b2a92c` is the clean
  baseline
- `Batch 49` is accepted on `main`: commit `b0275a8d8ce3f33577629f44d7b168b4d4ef8bb2` landed the
  narrow builtin `Propagator` structural-selector packet
- `Batch 49b` is accepted on `main` on top of that clean baseline: local module-loaded
  configure/build/ctest passed in `/tmp/autoIBP-b49b-mass`, final clean-candidate `sapphire` job
  `5457143` passed for candidate
  `/n/holylabs/schwartz_lab/Lab/obarrera/autonomousIBP-artifacts/candidates/b49b-final-clean-candidate-20260413T112519Z-Kabcrq`,
  and second-pass rereview cleared with no blocking or medium findings remaining
- that landed `Batch 49b` packet is still narrow: the `Mass` preference is only a local token-based
  heuristic, selected-mass coherence is only outer-whitespace trimming on rewritten selected
  propagators, there is no broader topology/component-order or symbolic mass-canonicalization
  claim
- `Batch 50a` is accepted on `main`: clean-candidate `sapphire` job `5465841` passed for
  candidate
  `/n/holylabs/schwartz_lab/Lab/obarrera/autonomousIBP-artifacts/candidates/b50a-final-clean-candidate-20260413T123155Z-1gWbow`,
  final status `COMPLETED` on `holy8a32607` in `00:00:32`, second-pass rereview cleared, and the
  landing commit is `bbd7b744b69a413bf34e4b706cd737e2b266256a`
- that landed `Batch 50a` packet is still narrow: it adds only the internal eta-topology preflight
  seam and truthful early failure for `Branch` / `Loop`; it does not accept real selector
  semantics, `AnalyzeTopSector` parity, graph-polynomial availability, or broader topology
  analysis
- `Batch 50b` is the current local packet only: an internal topology-prerequisite bridge and
  prereq-snapshot seam for `Branch` / `Loop`, still blocked, still unaccepted, and still deferring
  truthful selector semantics to `Batch 50`
- local module-loaded configure/build/ctest passed for the current `Batch 50b` worktree packet
- clean-candidate attempt `5480669` is operational noise rather than acceptance evidence: it
  OOM-killed during configure because the submission omitted an explicit memory request
- `Batch 50` is now the next roadmap-owned selector-semantics implementation lane after `Batch 50b`
  while `M0b` remains separately open and still blocks broader parity claims

## Mandatory Read Set Before Planning

Every new orchestrator thread should read these before picking the next batch:

- `docs/implementation-ledger.md`
- `docs/public-contract.md`
- `docs/parity-contract.md`
- `docs/full-amflow-completion-roadmap.md` when the task is full-parity planning, any
  post-bootstrap solver/runtime work, or any roadmap-owned reference/qualification milestone
- `docs/verification-strategy.md`
- `specs/parity-matrix.yaml`

The orchestrator then loads the seam-specific corpus for the planned batch.

For current AMFlow reducer-interface work, this usually means:

- `references/snapshots/amflow/kira-interface.m`
- `references/snapshots/amflow/options_summary.txt`
- `references/snapshots/amflow/README.md`
- `references/snapshots/amflow/CHANGELOG.md`

For invariant-generated or solver-adjacent batches, the seam-specific read set must also include:

- `references/README.md`
- `references/INDEX.md`
- `references/notes/porting-roadmap.md`
- `references/notes/theory-gap-audit.md`

If the batch touches reference-harness behavior, also load:

- `docs/reference-harness.md`
- relevant scripts and templates under `tools/reference-harness/`

## Roadmap Blocker Handling

When a user requests multiple roadmap items, a full phase, or a phase plus a milestone, the
orchestrator must split the request by roadmap feasibility before implementation begins.

For this repo, a request to finish a later phase or milestone is also standing authorization to
backfill any unmet prerequisite batches or milestones automatically, in dependency order, whenever
that is the only locally feasible path to the requested target. Stop only for true external
blockers such as missing tools, runtimes, upstream captures, or other artifacts the workspace
cannot create on its own.

For each requested batch or milestone, check:

- dependency satisfaction against `docs/full-amflow-completion-roadmap.md`
- required external tools, runtimes, or captured artifacts
- local feasibility in the current workspace

If a roadmap-owned milestone is externally blocked, the orchestrator must:

- report the blocker explicitly with the missing tools or artifacts
- avoid claiming that milestone or any gate that depends on it
- continue with any independent locally feasible batch in dependency order when the roadmap allows
  it

If a requested phase or milestone is not yet directly feasible because upstream roadmap
prerequisites are still unmet, the orchestrator must:

- list the exact prerequisite batches or milestones that are still missing
- begin with the earliest unmet locally feasible prerequisite batch in dependency order
- continue backfilling unmet locally feasible prerequisites until the requested target becomes
  feasible or a true external blocker stops progress
- report the prerequisite backfill explicitly in status updates and in the final report rather than
  presenting it as if the originally requested later phase or milestone had already been completed

Autonomous prerequisite backfilling does not relax acceptance. A prerequisite counts as satisfied
only after its own verification and review gates pass and its ledger state advances normally.
Never claim the originally requested later phase or milestone as accepted unless its own batch or
milestone gates have actually cleared.

The final report and any compactification handoff must separate:

- implemented batches
- autonomously backfilled prerequisite batches or milestones
- requested-but-blocked milestones
- requested-but-blocked phases
- exact missing external dependencies or artifacts

## Batch Lifecycle

Every implementation batch must pass through these stages in order:

1. baseline read and candidate batch selection
2. planner pass
3. theory/corpus scrutiny pass
4. verification-design pass
5. implementation pass
6. local verification
7. primary review
8. fix loop if needed
9. secondary review or reviewer-quality audit
10. ledger/public-contract finalization

No code batch is accepted until review is complete and the ledger status is advanced to `reviewed`.

## Deployment Order And Parallelism

### Planner

Deploy first on every batch, after the baseline read.

The planner may run in parallel with an early theory pass only if the orchestrator already has a credible candidate seam from the ledger and parity matrix. If planner and theory disagree on scope, freeze implementation until the scope is reconciled.

### Theory / Corpus Scrutiny

Deploy on every batch before implementation. This pass may run in parallel with verification design once the candidate scope is stable enough to evaluate.

Theory must distinguish:

- behavior explicitly shown in the snapshot corpus
- behavior inferred for the C++ port
- behavior that is out of scope and must be deferred

### Verification Design

Deploy on every batch before implementation. This pass can run in parallel with theory once the candidate scope is known.

Verification must define:

- exact positive expectations
- edge cases
- deterministic rejection cases
- required verification commands beyond the always-run build and test commands

### Implementer

Deploy only after planner, theory, and verification outputs are available and the write surface has been frozen.

Implementation should be a single owned patch surface. If the planned seam changes a public API, validation rule, ordering rule, or wrapper behavior, the write surface must include `docs/public-contract.md`. The write surface must always include `docs/implementation-ledger.md` for code batches.

### Primary Reviewer

Deploy only after the implementer has finished and local verification has run. The reviewer gets the frozen batch packet, the changed surface, and the exact verification evidence.

### Secondary Reviewer Or Reviewer-Quality Audit

Deploy after the primary reviewer reports no material findings, or after a fix loop closes. For code batches that touch `src/`, `include/`, `tests/`, or reducer orchestration, this should be a second reviewer agent whenever available. For docs-only maintenance, the orchestrator may perform the second-pass audit directly.

The secondary pass exists to catch:

- missed contract drift
- missing negative tests
- ownership violations
- acceptance claims that are broader than the actual implementation
- shallow first-pass reviews

## Required Inputs By Pass

### Planner Inputs

- `docs/implementation-ledger.md`
- `docs/public-contract.md`
- `docs/parity-contract.md`
- `specs/parity-matrix.yaml`
- last accepted batch summary
- likely next seam or candidate gap
- relevant source/test modules from the current implementation area

### Theory Inputs

- planner draft scope
- seam-specific snapshot files from `references/snapshots/amflow/`
- `references/README.md`, `references/INDEX.md`, `references/notes/porting-roadmap.md`, and
  `references/notes/theory-gap-audit.md` for invariant-generated or solver-adjacent batches
- any accepted local seam docs that constrain the batch
- relevant source files only as needed to compare local behavior against the snapshot

### Verification Inputs

- planner scope and acceptance criteria
- theory explicit-vs-inferred notes
- touched source and test modules
- relevant fixtures under `tests/data/` if the batch is not purely in-memory

### Implementer Inputs

- frozen planner output
- theory traps and defers
- verification expectations
- explicit owned file list
- explicit forbidden expansions for the batch

### Reviewer Inputs

- planner output
- theory report
- verification plan
- exact changed files
- verification commands and results
- any fix notes from prior review rounds

## Required Outputs By Pass

### Planner Output

The planner must produce:

- exact batch title
- exact scope
- acceptance criteria
- owned files/modules
- required tests/fixtures
- explicit defers
- the reason this is the correct next seam

### Theory Output

The theory pass must produce:

- explicit snapshot behavior with citations
- inferred behavior with clear labeling
- main correctness traps
- a recommendation on whether the batch is still too wide or too narrow

### Verification Output

The verification pass must produce:

- exact expected outputs or observable effects
- concrete happy-path tests
- concrete negative tests
- edge-case expectations
- required local verification commands

### Implementer Output

The implementer must produce:

- code and test changes restricted to the owned surface
- any required `docs/public-contract.md` updates
- a `docs/implementation-ledger.md` update with batch status at `implemented` before review
- a concise note describing any assumption or micro-adjustment outside the initial plan
- local verification results

### Reviewer Output

The reviewer must produce findings first. A valid review contains:

- material findings first, with exact file references and tight line ranges when possible
- severity or blocker assessment
- explicit note on whether the diff respected file ownership
- explicit note on whether the contract and ledger updates match the implementation
- residual risks or coverage gaps after findings
- an unambiguous sign-off statement if there are no material findings

Reviews that only provide a high-level summary without inspecting correctness, ownership, tests, and docs are not sufficient.

## File Ownership Rules

- The planner freezes the owned file list before implementation starts.
- The implementer edits only the owned surface unless a tiny declaration-only spillover is unavoidable.
- Any spillover outside the owned surface must be called out before the edit and justified as necessary to compile or expose the seam cleanly.
- The implementer must not widen the batch with side features, cleanup work, or deferred roadmap items.
- Unrelated user changes are never reverted.
- The reviewer checks that the edited files stayed within the planned surface.

In practice, most batches should own:

- one or two `src/` modules
- the matching `include/` declaration surface if needed
- `tests/amflow_tests.cpp` or a narrow fixture area
- `docs/public-contract.md` if the public seam changed
- `docs/implementation-ledger.md`

## Verification Rules

Light git/doc inspection may happen on the login node. Real builds, tests, reducer jobs,
reference-harness jobs, benchmark captures, and provisioning probes must run through fresh Slurm
jobs. For the current cluster-controlled repo state, `sapphire` is the canonical acceptance lane
unless a narrower retained packet explicitly documents a different partition and why that packet is
non-canonical.

The following commands are mandatory for every code batch:

- `cmake --build build`
- `ctest --test-dir build --output-on-failure`

The following configure/build/test triplet is mandatory for milestone-grade packets and any
repo-wide or build-system change:

- `cmake -S . -B build`
- `cmake --build build --parallel 1`
- `ctest --test-dir build --output-on-failure`

These commands must be run against the same worktree and the same `build/` directory, in
sequence. Do not overlap the build and test commands on the same tree.

The verification pass may add seam-specific commands, for example:

- targeted CLI invocations under `./build/amflow-cli`
- parser or wrapper checks against `tests/data/kira-results/*`
- reference-harness helper `--self-check` runs

Verification evidence must include:

- the exact commands run
- the exact worktree and `build/` directory used
- whether each command succeeded or failed
- any targeted artifact, file, or behavior that the command confirmed
- the exact rerun commands and outcomes after any fix that changes code or tests

The implementer runs verification before review. If a later fix changes code or tests, the verification set must be rerun before re-review.
Post-fix reruns must use the same tree and the same `build/` directory and must again run
`cmake --build build` before `ctest --test-dir build --output-on-failure`.

## Review And Fix Loop

1. The implementer finishes the planned patch and updates the ledger entry to `implemented`.
2. The implementer runs the mandatory verification commands plus any targeted checks from the verification pass.
3. The primary reviewer inspects the changed surface against the planner, theory, and verification packets.
4. If the reviewer finds material issues, the batch status moves to `changes-requested` in the ledger or is treated that way in the working notes until fixed.
5. The orchestrator routes the findings back to the implementer without dilution. The fix packet should preserve the reviewer wording, affected files, and expected behavior.
6. The implementer fixes only the findings and any directly necessary follow-on adjustments.
7. The implementer reruns verification.
8. The reviewer re-reviews the changed surface.
9. After the primary reviewer reports no material findings, the secondary reviewer or reviewer-quality audit runs and records an explicit disposition.
10. Only after both review gates pass may the batch be treated as accepted and the ledger advanced to `reviewed`.

## Reviewer Quality Checks

The orchestrator is responsible for screening the review itself. A reviewer pass is insufficient if it:

- does not reference the actual changed files
- does not compare the implementation to the planner acceptance criteria
- does not check the theory pass correctness traps
- does not verify that tests cover the claimed seam
- ignores `docs/public-contract.md` drift when the public seam changed
- ignores `docs/implementation-ledger.md` status and verification evidence
- gives a blanket approval without explaining why no material findings remain

When a review looks shallow, deploy a second reviewer or request a stricter rereview before accepting the batch.

## Routing Findings Back To Implementers

Reviewer findings should be passed back as a narrow fix packet:

- quote or summarize the finding faithfully
- identify the affected file surface
- restate the expected behavior or contract
- preserve the original batch scope

The implementer should reply with:

- the files changed to address each finding
- any additional verification added because of the finding
- any remaining open question or blocker

The reviewer then re-checks the exact changed surface rather than re-litigating the entire batch from scratch.

## Ledger And Public-Contract Gating

### `docs/implementation-ledger.md`

The ledger is the acceptance record for incremental batches.

- add or update the batch entry during the implementation pass
- mark the batch `implemented` before review
- record the actual verification commands run
- if review finds material issues, mark `changes-requested` or reflect that state clearly before the next review round
- mark the batch `reviewed` only after reviewer sign-off and the second-pass audit
- record the second-pass disposition explicitly in the ledger notes, for example
  `second-pass: approved` or `second-pass: changes-requested`

The ledger note should capture the key acceptance caveat or the main fix that was required before sign-off.

### `docs/public-contract.md`

Update the public contract in the same batch whenever the implementation changes any of the following:

- a public type
- a typed seam or wrapper
- validation behavior
- deterministic ordering behavior
- explicit failure behavior
- CLI surface

If a batch intentionally does not change the public seam, the reviewer should still confirm that the contract remains accurate.

## Compactification Handoff Packet

When a thread is likely to compact, the orchestrator should leave enough on-disk state that the next thread can continue from documents rather than chat history. The minimum handoff packet is:

- this document
- `docs/implementation-ledger.md`
- `docs/public-contract.md`
- the current accepted batch and next-batch recommendation
- verification commands and results for the active batch
- reviewer findings and whether they were fixed

The final user-facing report for each batch should also include:

- implemented batch number
- which agent handled planner, theory, verification, implementation, primary review, and second review
- what review findings were raised and how they were resolved
- exact verification commands and outcomes
- the explicit second-pass disposition
- the recommended next batch

## Current Default For The AMFlow Port

Until the solver and reference harness broaden substantially, the default implementation workflow for a new batch is:

1. read `docs/implementation-ledger.md` and `docs/public-contract.md`
2. choose the next narrow seam or operational milestone implied by the accepted batch state, the
   roadmap gates, and the active blocker chain; on the current baseline that means start
   `Batch 50b`, not reopen `K0b`, `Batch 47`, landed `Batch 48`, landed `Batch 49`, landed
   `Batch 49b`, or landed `Batch 50a`
3. run planner, theory, and verification passes
4. freeze the owned surface
5. implement narrowly
6. run `cmake --build build`
7. run `ctest --test-dir build --output-on-failure`
8. run any targeted seam checks
9. run primary review
10. run second review or reviewer-quality audit
11. only then mark the batch `reviewed`
