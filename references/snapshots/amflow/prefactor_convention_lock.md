# AMFlow Prefactor Convention Lock

This markdown is the human-readable mirror for the narrow
prefactor/sign-convention evidence packet used by
`AmflowPrefactorConvention` and `BuildOverallAmflowPrefactor(...)` for
`Batch 58c`. The structured freeze artifact is
`specs/amflow-prefactor-reference.yaml`.

## Provenance

Retained phase-0 root:

- `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260419-required-set/inputs/upstream/amflow/README.md:73`
- `/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260419-required-set/inputs/upstream/amflow/AMFlow.m:223`

Repo-local snapshot:

- `references/snapshots/amflow/README.md:91`

## Exact Retained-Root Statement

`NOTE for prefactor: $1/(i \pi^{D/2})$ for each loop and $\delta_+(p^2-m^2)/(2 \pi)^{D-1}$ for each integrated final state particle, where $D = D_0-2\epsilon$ with $D_0 = 4$ by default is the spacetime dimension and $p (m)$ is the momentum (mass) of the corresponding particle. Please see also https://gitlab.com/multiloop-pku/amflow/-/issues/1 for clarification.`

## Exact Snapshot-Only Addition

`NOTE for prefactor: $1/(i \pi^{D/2})$ for each loop ($-1/(i \pi^{D/2})$ for each loop with -i0 prescription) and $\delta_+(p^2-m^2)/(2 \pi)^{D-1}$ for each integrated final state particle, where $D = D_0-2\epsilon$ with $D_0 = 4$ by default is the spacetime dimension and $p (m)$ is the momentum (mass) of the corresponding particle. Please see also https://gitlab.com/multiloop-pku/amflow/-/issues/1 for clarification.`

## Repository-Normalized Literals

- retained-root lock: `plus_i0_loop_prefactor = 1/(I*pi^(D/2))`
- retained-root lock: `cut_prefactor = delta_+(p^2-m^2)/(2*pi)^(D-1)`
- snapshot-backed only: `minus_i0_loop_prefactor = -1/(I*pi^(D/2))`

`AMFlow.m:223` freezes only the prescription vocabulary:
`1 means +i0`, `-1 means -i0`, and `0 means no prescription at all`.

## Explicit Non-Claims

- no retained-root proof is claimed here for the `-i0` loop-prefactor sign
- no Kira `insert_prefactors` wiring is claimed here
- no first-family reduction-span parity is claimed here
- no `Milestone M3` or `Milestone M4` closure is claimed here
