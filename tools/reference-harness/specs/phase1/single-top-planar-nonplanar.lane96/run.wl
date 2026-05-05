(* ::Package:: *)

current = If[$FrontEnd === Null, $InputFileName, NotebookFileName[]] // DirectoryName;
$MaxExtraPrecision = 2000;
$MaxPrecision = 2000;

Get[FileNameJoin[{current, "..", "..", "AMFlow.m"}]];

SetReductionOptions[
  "IBPReducer" -> "Kira",
  "BlackBoxRank" -> 2,
  "BlackBoxDot" -> 2
];

AMFlowInfo["Family"] = singleTopPL1;
AMFlowInfo["Loop"] = {k1, k2};
AMFlowInfo["Leg"] = {p1, p2, p3, p4};
AMFlowInfo["Conservation"] = {p4 -> -p1 - p2 - p3};
AMFlowInfo["Replacement"] = {
  p1^2 -> 0,
  p2^2 -> 0,
  p3^2 -> 0,
  p4^2 -> 0,
  (p1 + p2)^2 -> S12,
  (p2 + p3)^2 -> S23,
  (p1 + p3)^2 -> -S12 - S23,
  (p1 + p2 + p3)^2 -> 0
};

(* arXiv:2302.02729 / JHEP05(2023)131 Table 2, PL1 family after
   the x-dependent loop-momentum redefinition.  The first target is
   the single-scale seed from eq. (4.4); the second is the scalar
   top-sector support integral for the PL1 Euclidean benchmark surface. *)
AMFlowInfo["Propagator"] = {
  (k1 - x p1 - x p2)^2,
  (k2 + x p1 + x p2)^2,
  (k1 - x p1)^2,
  (k2 + x p1)^2,
  k1^2,
  k2^2,
  (k1 + p1 + p2 + p3)^2,
  (k2 - p1 - p2 - p3)^2 - mw2,
  (k1 + k2)^2
};

AMFlowInfo["Numeric"] = {
  x -> 45/11,
  S12 -> -968/45,
  S23 -> 704/45,
  mw2 -> 974
};
AMFlowInfo["NThread"] = 8;

target = {
  j[singleTopPL1, 0, 0, 0, 0, 1, 0, 0, 2, 2],
  j[singleTopPL1, 1, 1, 1, 0, 1, 1, 0, 1, 1]
};

precisionGoal = 50;
epsorder = 4;

Print["lane96 single-top-planar-nonplanar PL1 Euclidean AMFlow packet"];
Print["source: arXiv:2302.02729 / JHEP05(2023)131, Table 2 and Table 4 E1"];
Print["target integrals: ", InputForm[target]];
Print["numeric point: ", InputForm[AMFlowInfo["Numeric"]]];
Print["precisionGoal: ", precisionGoal];
Print["epsorder: ", epsorder];
Print["generated config preview: ", InputForm[GenerateNumericalConfig[precisionGoal, epsorder]]];

sol = SolveIntegrals[target, precisionGoal, epsorder];

Put[sol, FileNameJoin[{current, "sol-single-top-pl1-euclidean"}]];
Put[
  <|
    "packet" -> "lane96-single-top-planar-nonplanar",
    "source" -> "arXiv:2302.02729 / JHEP05(2023)131",
    "family" -> "singleTopPL1",
    "topology" -> "PL1",
    "kinematic_region" -> "Euclidean",
    "kinematic_point" -> <|"x" -> 45/11, "S12" -> -968/45, "S23" -> 704/45, "mw2" -> 974|>,
    "targets" -> target,
    "numeric" -> AMFlowInfo["Numeric"],
    "precisionGoal" -> precisionGoal,
    "epsorder" -> epsorder,
    "maxPrecision" -> $MaxPrecision,
    "maxExtraPrecision" -> $MaxExtraPrecision
  |>,
  FileNameJoin[{current, "sol-single-top-pl1-euclidean.meta.wl"}]
];

Quit[];
