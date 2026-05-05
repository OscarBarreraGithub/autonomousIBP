(* ::Package:: *)

current = If[$FrontEnd === Null, $InputFileName, NotebookFileName[]] // DirectoryName;
$MaxExtraPrecision = 2000;
$MaxPrecision = 2000;

Get[FileNameJoin[{current, "..", "..", "AMFlow.m"}]];

SetReductionOptions[
  "IBPReducer" -> "Kira",
  "BlackBoxRank" -> 2,
  "BlackBoxDot" -> 1
];

AMFlowInfo["Family"] = boxFermionBubbleH;
AMFlowInfo["Loop"] = {k1, k2};
AMFlowInfo["Leg"] = {p1, p2, p3, p4};
AMFlowInfo["Conservation"] = {p4 -> -p1 - p2 - p3};
AMFlowInfo["Replacement"] = {
  p1^2 -> 0,
  p2^2 -> 0,
  p3^2 -> 0,
  p4^2 -> 0,
  (p1 + p2)^2 -> s,
  (p2 + p3)^2 -> t,
  (p1 + p3)^2 -> -s - t
};

(* arXiv:2312.06773 eq. (6), topology H: m2=m3=m4=0.
   The propagators intentionally keep the paper's P_j signs. *)
AMFlowInfo["Propagator"] = {
  -k1^2,
  -(k1 - p1)^2,
  -(k1 - p1 - p2)^2,
  -(k1 - p1 - p2 - p3)^2,
  -k2^2,
  -(k1 + k2 - p1 - p2 - p3)^2,
  -(k1 + k2 + p1 + p2)^2,
  -(k1 + k2 - p1)^2,
  -(k1 + k2)^2
};

AMFlowInfo["Numeric"] = {s -> 118/100, t -> -45/10000};
AMFlowInfo["NThread"] = 8;

target = {
  j[boxFermionBubbleH, 0, 1, 0, 0, 2, 2, 0, 0, 0],
  j[boxFermionBubbleH, 1, 0, 1, 0, 1, 2, 0, 0, 0],
  j[boxFermionBubbleH, 1, 1, 1, 0, 1, 2, 0, 0, 0]
};

precisionGoal = 50;
epsorder = 4;

Print["lane93 2024-box-integrals-fermion-bubbles topology H AMFlow packet"];
Print["source: arXiv:2312.06773, topology H, eqs. (6), (13), and (79)"];
Print["target integrals: ", InputForm[target]];
Print["numeric point: ", InputForm[AMFlowInfo["Numeric"]]];
Print["precisionGoal: ", precisionGoal];
Print["epsorder: ", epsorder];
Print["generated config preview: ", InputForm[GenerateNumericalConfig[precisionGoal, epsorder]]];

sol = SolveIntegrals[target, precisionGoal, epsorder];

Put[sol, FileNameJoin[{current, "sol-2024-box-integrals-fermion-bubbles-topology-h"}]];
Put[
  <|
    "packet" -> "lane93-2024-box-integrals-fermion-bubbles",
    "source" -> "arXiv:2312.06773 / Eur.Phys.J.C 84 (2024) 495",
    "family" -> "boxFermionBubbleH",
    "topology" -> "H",
    "topology_masses" -> <|"m2" -> 0, "m3" -> 0, "m4" -> 0|>,
    "targets" -> target,
    "numeric" -> AMFlowInfo["Numeric"],
    "precisionGoal" -> precisionGoal,
    "epsorder" -> epsorder,
    "maxPrecision" -> $MaxPrecision,
    "maxExtraPrecision" -> $MaxExtraPrecision
  |>,
  FileNameJoin[{current, "sol-2024-box-integrals-fermion-bubbles-topology-h.meta.wl"}]
];

Quit[];
