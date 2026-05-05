(* ::Package:: *)

current = If[$FrontEnd === Null, $InputFileName, NotebookFileName[]] // DirectoryName;
$MaxExtraPrecision = 6000;
$MaxPrecision = 6000;

Get[FileNameJoin[{current, "..", "..", "AMFlow.m"}]];

SetReductionOptions[
  "IBPReducer" -> "Kira",
  "BlackBoxRank" -> 3,
  "BlackBoxDot" -> 1
];
SetReducerOptions["ReductionMode" -> "Kira", "IntegralOrder" -> 5];

SetAMFOptions[
  "AMFMode" -> {"Prescription", "Mass", "Propagator", "Branch", "Loop"},
  "EndingScheme" -> {"Tradition", "Cutkosky", "SingleMass"},
  "ChopPre" -> 80
];

AMFlowInfo["Family"] = diphotonNPL;
AMFlowInfo["Loop"] = {k1, k2};
AMFlowInfo["Leg"] = {p1, p2, p3, p4};
AMFlowInfo["Conservation"] = {p4 -> -p1 - p2 - p3};
AMFlowInfo["Replacement"] = {
  p1^2 -> 0,
  p2^2 -> 0,
  p3^2 -> 0,
  p4^2 -> 0,
  (p1 + p2)^2 -> y,
  (p1 + p3)^2 -> z,
  (p2 + p3)^2 -> -y - z
};

AMFlowInfo["Propagator"] = {
  k1^2,
  (k1 - p1)^2,
  (k1 + p2)^2,
  k2^2 - mt2,
  (k1 + k2 - p1)^2 - mt2,
  (k2 - p1 - p2 + p3)^2 - mt2,
  (k1 + k2 - p1 + p3)^2 - mt2,
  (k1 + p3)^2,
  (k1 + k2)^2
};

(* Table-2 point from arXiv:2308.11412 ancillary DIFFEXP_run.wl:
   y = s/mt^2 = 1/11 and z = t/mt^2 = -199/2200, corresponding to
   sqrt(s) = 52.1615 GeV and cos(theta) = -0.99 for mt near 173 GeV. *)
AMFlowInfo["Numeric"] = {y -> 1/11, z -> -199/2200, mt2 -> 1};
AMFlowInfo["NThread"] = 8;
AMFlowInfo["Prescription"] = {1, 1};

target = {
  j[diphotonNPL, 1, 1, 1, 1, 1, 1, 1, 0, 0],
  j[diphotonNPL, 1, 1, 1, 1, 1, 1, 1, -1, 0],
  j[diphotonNPL, 1, 1, 1, 1, 2, 1, 1, 0, 0],
  j[diphotonNPL, 1, 1, 1, 1, 1, 1, 1, -1, -1]
};

precisionGoal = 200;
epsorder = 4;

Print["lane80 diphoton dedicated AMFlow packet"];
Print["target integrals: ", InputForm[target]];
Print["numeric point: ", InputForm[AMFlowInfo["Numeric"]]];
Print["precisionGoal: ", precisionGoal];
Print["epsorder: ", epsorder];
Print["generated config preview: ", InputForm[GenerateNumericalConfig[precisionGoal, epsorder]]];

sol = SolveIntegrals[target, precisionGoal, epsorder];

Put[sol, FileNameJoin[{current, "sol-diphoton-npl-j39-j42"}]];
Put[
  <|
    "packet" -> "lane80-diphoton-heavy-quark-form-factors",
    "source" -> "arXiv:2308.11412 Table 2 / ancillary DIFFEXP_run.wl",
    "family" -> "diphotonNPL",
    "targets" -> target,
    "numeric" -> AMFlowInfo["Numeric"],
    "precisionGoal" -> precisionGoal,
    "epsorder" -> epsorder,
    "amfMode" -> $AMFMode,
    "endingScheme" -> $EndingScheme,
    "maxPrecision" -> $MaxPrecision,
    "maxExtraPrecision" -> $MaxExtraPrecision
  |>,
  FileNameJoin[{current, "sol-diphoton-npl-j39-j42.meta.wl"}]
];

Quit[];
