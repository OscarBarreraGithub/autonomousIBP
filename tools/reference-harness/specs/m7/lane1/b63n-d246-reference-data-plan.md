# B63n D2/D4/D6 Reference Data Plan

Status: theory planner only. This document does not add AMFlow reference
values, does not create or edit the D2/D4/D6 evidence JSON sidecar, does not
change runtime code, and does not promote `b63n`.

The promotion guard added by `9b86850` names the required evidence sidecar:

```text
tools/reference-harness/specs/m6/lane146/b63n-d246-weighted-residue-reference-evidence.json
```

That file is absent at the time of this plan. The current lane146 artifacts
remain scoped to selected Cutkosky endpoint evidence, and `50aa543` added a
D7 parity test tied to lane146 compare30 data. D2, D4, and D6 must not be
promoted from the current synthetic moment seeds.

## Confirmed Source Anchors

The in-tree AMFlow snapshot documents that AMFlow evaluates phase-space
integrals and ships the `automatic_phasespace` and `feynman_prescription`
examples (`references/snapshots/amflow/README.md:6-8`,
`references/snapshots/amflow/README.md:77-84`). The snapshot does not include
the example `run.wl` files themselves, so the executable Mathematica source
must be audited from the canonical extracted upstream tree:

```text
/n/holylabs/schwartz_lab/Lab/obarrera/reference-inputs/autonomousIBP/cpc/amflow-gitlab-1.1-extracted/
```

The canonical `automatic_phasespace/run.wl` has sha256
`f9f021e584334cc21fb682526eaf9f55de95b6f2a58b2971ae46a445fd46e2bf`, matching
the phase0 captured upstream copy at
`/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/reference-harness/phase0-reference-captured-20260419-required-set/inputs/upstream/amflow/examples/automatic_phasespace/run.wl`.

Required source lines for the future data producer:

- `examples/automatic_phasespace/run.wl:17-24`: family `phase`, loops
  `{l1,l2}`, legs `{p1,p2}`, replacement rules, propagators, and numeric
  substitutions `{s -> 100, msq -> 1}`.
- `examples/automatic_phasespace/run.wl:28-33`: phase-space prescription
  `{0,0}` and cut vector `{1,0,1,0,1,0,0}`.
- `examples/automatic_phasespace/run.wl:36-42`: target
  `j[phase,1,2,1,1,1,1,1]`, `precision = 20`, `epsorder = 4`, and
  `SolveIntegrals`.
- `AMFlow.m:461-481`: loop-prescription to propagator-prescription rules and
  fail-closed conflict behavior.
- `AMFlow.m:485-489`: `PhaseVolumeQ` definition.
- `AMFlow.m:874-881`: eta-continuation direction rule.
- `AMFlow.m:911-916`: Cutkosky ending selection.
- `AMFlow.m:941-950`: Cutkosky prefactor and final `Im` projection.
- `diffeq_solver/DESolver.m:916-927`: eta-zero local model construction.
- `diffeq_solver/DESolver.m:1053-1061`: `PickZeroRuleS` integer eta-zero term
  selection.
- `diffeq_solver/DESolver.m:1065-1095`: AMFlow contour run from infinity to
  the endpoint solution.

The package example uses `precision = 20`; that is source identity, not enough
for the M6 50-digit parity floor. Future reference data must rerun or recapture
the same source at a reviewed high precision.

## Physics Identity

The b63n automatic phase-space surface is:

```text
family = phase
loops = {l1, l2}
kinematics = {p1^2 -> 0, p2^2 -> 0, (p1+p2)^2 -> s, s -> 100, msq -> 1}
propagators:
  D1 = l1^2 - msq
  D2 = (l1+p1)^2
  D3 = l2^2
  D4 = (l1+l2+p1)^2
  D5 = (l1+l2+p1+p2)^2
  D6 = (l1+l2+p2)^2
  D7 = (l1+p2)^2
cut support = D1,D3,D5
target = j[phase,1,2,1,1,1,1,1]
```

After the three cuts, this is a two-loop one-mass three-body phase volume:

```text
dPhi_3(P;m,0,0) =
  dq2/(2*pi) * dPhi_2(P;m,sqrt(q2)) * dPhi_2(q;0,0)

P = p1 + p2
P^2 = 100
m^2 = 1
0 <= q2 <= 81
lambda(100,1,q2) = (q2 - 81) * (q2 - 121)
```

The Cutkosky prefactor for the two-loop phase-volume component is fixed by
`AMFlow.m:941-950`:

```text
K_2(eps) = -2 * (Pi^(2-eps) * (2 Pi)^(2 eps - 4))^2
```

The weighted endpoint functional is:

```text
B_auto(eps) =
  K_2(eps) * Int dPhi_3(P;m,0,0)
    [D2^-2 D4^-1 D6^-1 D7^-1]_sigma
```

Here `sigma` is the AMFlow branch/prescription ledger from prescription
`{0,0}`, the real phase-space cut support D1,D3,D5, the lower-half-plane
`NegIm` eta direction, and the final `Im` projection used by the Cutkosky
ending.

D2, D4, and D6 are the still-missing weighted denominator moments:

| Weight | Denominator index | Target power | Structural role |
| --- | ---: | ---: | --- |
| D2 | 1 | 2 | `(l1+p1)^2`; angular/invariant weight depending on `q2` and `cos_theta_a` |
| D4 | 3 | 1 | `(l1+l2+p1)^2`; angular/invariant weight depending on `q2`, `cos_theta_a`, and `cos_theta_b` |
| D6 | 5 | 1 | `(l1+l2+p2)^2`; angular/invariant weight depending on `q2`, `cos_theta_a`, and `cos_theta_b` |

D7 has scoped runtime parity evidence from lane146/`50aa543`, but that evidence
does not solve the D2/D4/D6 moments and does not close the full weighted
surface.

## AMFlow Data Required

Each D2/D4/D6 entry must come from the same upstream Mathematica surface, not
from the current C++ synthetic moment seeds and not from retained final
solution samples used as runtime input.

Required AMFlow parameter set for all three entries:

```mathematica
AMFlowInfo["Family"] = phase;
AMFlowInfo["Loop"] = {l1, l2};
AMFlowInfo["Leg"] = {p1, p2};
AMFlowInfo["Conservation"] = {};
AMFlowInfo["Replacement"] = {p1^2 -> 0, p2^2 -> 0, (p1+p2)^2 -> s};
AMFlowInfo["Propagator"] = {
  l1^2-msq,
  (l1+p1)^2,
  l2^2,
  (l1+l2+p1)^2,
  (l1+l2+p1+p2)^2,
  (l1+l2+p2)^2,
  (l1+p2)^2
};
AMFlowInfo["Numeric"] = {s -> 100, msq -> 1};
AMFlowInfo["Prescription"] = {0, 0};
AMFlowInfo["Cut"] = {1, 0, 1, 0, 1, 0, 0};
integrals = {j[phase,1,2,1,1,1,1,1]};
```

Required producer behavior for each entry:

- D2: extract and publish the `D2` selected-weight coefficient series for
  denominator index `1`, power `2`, from the AMFlow target above.
- D4: extract and publish the `D4` selected-weight coefficient series for
  denominator index `3`, power `1`, from the AMFlow target above.
- D6: extract and publish the `D6` selected-weight coefficient series for
  denominator index `5`, power `1`, from the AMFlow target above.

The reference producer must record the exact Mathematica source file, source
sha256, AMFlow package/source sha256, precision settings, epsilon order, command
log, output artifact path, and the extraction rule used to map the AMFlow
result to the D2/D4/D6 selected-weight entries. If the future implementation
instead computes separate scoped target integrals to isolate a single weight,
the sidecar must still state the full b63n target above and explain the exact
mathematical projection from that full target to the scoped value. This plan
does not approve any such projection by default.

## Sidecar Schema

The sidecar must be written at the exact path expected by the fail-loud guard:

```text
tools/reference-harness/specs/m6/lane146/b63n-d246-weighted-residue-reference-evidence.json
```

Minimum schema version:

```json
{
  "schema_version": 1,
  "runtime_lane": "b63n",
  "benchmark_id": "automatic_phasespace",
  "surface_label": "phase[1,2,1,1,1,1,1]",
  "source_kind": "upstream-amflow-mathematica",
  "source_root": "<absolute canonical AMFlow source root>",
  "source_files": {
    "automatic_phasespace_run_wl": {
      "path": "<absolute path to examples/automatic_phasespace/run.wl>",
      "sha256": "f9f021e584334cc21fb682526eaf9f55de95b6f2a58b2971ae46a445fd46e2bf",
      "line_ranges": ["17-24", "28-33", "36-42"]
    },
    "amflow_m": {
      "path": "<absolute path to AMFlow.m>",
      "sha256": "6fd47002b36399ee71c38e3e43e5e75541d1f2641966ca103fc8b8ce37dc7add",
      "line_ranges": ["461-481", "485-489", "874-881", "911-916", "941-950"]
    },
    "desolver_m": {
      "path": "<absolute path to diffeq_solver/DESolver.m>",
      "sha256": "22c63b2aa4a4c8236a9593d39ba7ae8283efa12cb7730401e640ff1b43875585",
      "line_ranges": ["916-927", "1053-1061", "1065-1095"]
    }
  },
  "amflow_parameter_set": {
    "family": "phase",
    "loops": ["l1", "l2"],
    "numeric": {"s": "100", "msq": "1"},
    "prescription": [0, 0],
    "cut": [1, 0, 1, 0, 1, 0, 0],
    "target": "j[phase,1,2,1,1,1,1,1]",
    "precision_requested_digits": "<integer >= 70 recommended>",
    "eps_order_requested": "<integer >= 4 recommended>",
    "working_precision_digits": "<integer>",
    "run_command": "<exact noninteractive Mathematica command>",
    "run_log": "<path>",
    "raw_output": "<path>",
    "raw_output_sha256": "<sha256>"
  },
  "cutkosky_structure": {
    "phase_volume_loop_count": 2,
    "cut_denominators": ["D1", "D3", "D5"],
    "uncut_weight_denominators": ["D2", "D4", "D6", "D7"],
    "prefactor_formula": "K_2(eps) = -2*(Pi^(2-eps)*(2*Pi)^(2*eps-4))^2",
    "eta_direction": "NegIm",
    "endpoint_selection_rule": "DESolver PickZeroRuleS integer eta-zero term",
    "final_projection": "AMFlow Cutkosky Im projection"
  },
  "weights": [
    {
      "denominator_id": "D2",
      "denominator_index": 1,
      "propagator_power": 2,
      "propagator": "(l1+p1)^2",
      "structural_form": "inverse_denominator_weight[D2(q2,cos_theta_a)]",
      "coefficients": [
        {
          "eps_order": 0,
          "eta_power": 0,
          "log_power": 0,
          "region_key": "integer",
          "real": "<AMFlow decimal string>",
          "imaginary": "<AMFlow decimal string>",
          "source": "<raw output path or extraction artifact>",
          "source_sha256": "<sha256>",
          "extraction_label": "<deterministic label>",
          "working_precision_digits": "<integer>",
          "agreement_digits": "<integer from independent validation>"
        }
      ],
      "reference_validation": {
        "passed": true,
        "minimum_digit_agreement": "<integer >= 50>",
        "comparison_artifact": "<path>",
        "final_solution_samples_used_as_input": false,
        "synthetic_fixture": false,
        "coefficient_published": true
      }
    },
    {
      "denominator_id": "D4",
      "denominator_index": 3,
      "propagator_power": 1,
      "propagator": "(l1+l2+p1)^2",
      "structural_form": "inverse_denominator_weight[D4(q2,cos_theta_a,cos_theta_b)]",
      "coefficients": [
        {
          "eps_order": 0,
          "eta_power": 0,
          "log_power": 0,
          "region_key": "integer",
          "real": "<AMFlow decimal string>",
          "imaginary": "<AMFlow decimal string>",
          "source": "<raw output path or extraction artifact>",
          "source_sha256": "<sha256>",
          "extraction_label": "<deterministic label>",
          "working_precision_digits": "<integer>",
          "agreement_digits": "<integer from independent validation>"
        }
      ],
      "reference_validation": {
        "passed": true,
        "minimum_digit_agreement": "<integer >= 50>",
        "comparison_artifact": "<path>",
        "final_solution_samples_used_as_input": false,
        "synthetic_fixture": false,
        "coefficient_published": true
      }
    },
    {
      "denominator_id": "D6",
      "denominator_index": 5,
      "propagator_power": 1,
      "propagator": "(l1+l2+p2)^2",
      "structural_form": "inverse_denominator_weight[D6(q2,cos_theta_a,cos_theta_b)]",
      "coefficients": [
        {
          "eps_order": 0,
          "eta_power": 0,
          "log_power": 0,
          "region_key": "integer",
          "real": "<AMFlow decimal string>",
          "imaginary": "<AMFlow decimal string>",
          "source": "<raw output path or extraction artifact>",
          "source_sha256": "<sha256>",
          "extraction_label": "<deterministic label>",
          "working_precision_digits": "<integer>",
          "agreement_digits": "<integer from independent validation>"
        }
      ],
      "reference_validation": {
        "passed": true,
        "minimum_digit_agreement": "<integer >= 50>",
        "comparison_artifact": "<path>",
        "final_solution_samples_used_as_input": false,
        "synthetic_fixture": false,
        "coefficient_published": true
      }
    }
  ],
  "anti_fake": {
    "retained_solution_samples_used_as_input": false,
    "synthetic_fixture": false,
    "self_comparison": false,
    "tolerance_digits": 50,
    "full_eta_zero_contour_applied": false,
    "notes": "D2/D4/D6 sidecar is scoped reference evidence only unless a later runtime lane independently proves full b63n eta-zero coverage."
  },
  "passed": true
}
```

The `coefficients` arrays are schematic above. The real sidecar must include a
contiguous epsilon range at least matching the runtime publication scope. Empty
arrays are not valid publication evidence.

## Validation Strategy

Future implementers should validate the sidecar in this order:

1. Source identity: verify sha256s for `run.wl`, `AMFlow.m`, `DESolver.m`, and
   the captured raw AMFlow output. The source run must be the pinned
   `automatic_phasespace` surface with no changed propagator order, cut vector,
   prescription vector, numeric substitutions, or target.
2. Structural match: rebuild the C++ b63n automatic phase-space scaffold and
   confirm surface `phase[1,2,1,1,1,1,1]`, cuts D1,D3,D5, uncut weights
   D2,D4,D6,D7, `K_2(eps)`, `q2 in [0,81]`, and lower-half-plane `NegIm`
   direction.
3. Extraction audit: for each D2/D4/D6 coefficient, record the exact AMFlow
   expression element, epsilon order, eta-zero selection rule, and any
   projection from full AMFlow output to the selected-weight entry. Reject
   unlabeled selected coefficients.
4. Independent comparison: compare the candidate C++ publication term against
   the AMFlow-derived decimal value at 50 or more correct digits. Do this for
   real and imaginary parts and every published epsilon order.
5. Anti-fake checks: require `synthetic_fixture=false`,
   `retained_solution_samples_used_as_input=false`, `self_comparison=false`,
   and no sentinel-only digit claims. D2/D4/D6 cannot pass by carrying the
   current synthetic K2 seed.
6. Existing regression gate: `cutkosky-weighted-residue-tests` should see the
   sidecar only after the real evidence exists. The test's string-level guard is
   intentionally weaker than this schema; passing that guard is necessary but
   not sufficient.
7. Scope preservation: keep `full_eta_zero_contour_applied=false` unless a
   separate runtime lane proves all b63n automatic and companion
   `feynman_prescription` requirements. This sidecar may unblock D2/D4/D6
   scoped publication, not full M6 closure by itself.

## Unknowns And Risks

- The repo does not currently contain the executable `automatic_phasespace`
  `run.wl`; it must be audited from the canonical external AMFlow extraction or
  a captured upstream copy with matching sha256.
- The current D2/D4/D6 runtime state is synthetic moment-seed plumbing. It
  carries the reviewed K2 prefactor seed, not AMFlow endpoint Laurent
  coefficients.
- This plan has not established whether D2, D4, and D6 should be extracted as
  three projections from the full target `j[phase,1,2,1,1,1,1,1]` or via a
  reviewed set of separate scoped AMFlow targets. The default requirement is the
  full upstream target. Any separate-target shortcut needs a written derivation.
- The upstream example precision is 20. M6 parity needs high-precision reruns,
  stable raw outputs, and a comparison artifact with at least 50 correct digits.
- Endpoint singular-region handling remains a physics risk: `q2 = 0`,
  `q2 = 81`, angular collinear endpoints, and possible dropped terms from
  `PickZeroRuleS` must be recorded rather than silently suppressed.
- D7 scoped parity is not evidence for D2/D4/D6. Lane146 selected4 compare30
  data must not be copied into the D2/D4/D6 sidecar unless the AMFlow extraction
  explicitly proves the corresponding weight and epsilon order.
- The sidecar path lives under `m6/lane146` even though this is an M7 planning
  lane. A future evidence-producing lane should be careful not to overwrite
  lane146 selected4 artifacts or imply that this planner created reference
  values.
- `feynman_prescription` remains part of the broader b63n closure risk. Even a
  valid D2/D4/D6 sidecar should leave full b63n closure blocked until the
  conjugate prescription row has its own reviewed evidence.
