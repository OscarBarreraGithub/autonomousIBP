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
If[StringQ[Environment["KIRA_EXECUTABLE"]] && Environment["KIRA_EXECUTABLE"] =!= "",
  Kira`$KiraExecutable = Environment["KIRA_EXECUTABLE"];
];
If[StringQ[Environment["FERMAT_EXECUTABLE"]] && Environment["FERMAT_EXECUTABLE"] =!= "",
  Kira`$FermatExecutable = Environment["FERMAT_EXECUTABLE"];
];

AMFlowInfo["Family"] = ggGammaGammaLightQuarkPI;
AMFlowInfo["Loop"] = {l1, l2};
AMFlowInfo["Leg"] = {p1, p2, p3, p4};
AMFlowInfo["Conservation"] = {p4 -> -p1 - p2 - p3};
AMFlowInfo["Replacement"] = {
  p1^2 -> 0,
  p2^2 -> 0,
  p3^2 -> 0,
  p4^2 -> 0,
  (p1 + p2)^2 -> s12,
  (p2 + p3)^2 -> s23,
  (p1 + p3)^2 -> -s12 - s23
};

(* DOI 10.1140/epjc/s10052-023-12032-6, topology P_I, eqs. (3.1)-(3.3). *)
AMFlowInfo["Propagator"] = {
  l1^2,
  l2^2,
  (l1 + l2)^2 - mw2,
  (l1 - p1)^2,
  (l2 + p1)^2,
  (l1 - p1 - p2)^2,
  (l2 + p1 + p2)^2,
  (l1 - p1 - p2 - p3)^2,
  (l2 + p1 + p2 + p3)^2
};

AMFlowInfo["Numeric"] = {
  s12 -> -1/2,
  s23 -> -1/3,
  mw2 -> 1
};
AMFlowInfo["NThread"] = 4;

target = {
  j[ggGammaGammaLightQuarkPI, 1, 1, 1, 0, 0, 0, 0, 0, 0],
  j[ggGammaGammaLightQuarkPI, 1, 0, 1, 0, 0, 1, 0, 0, 0],
  j[ggGammaGammaLightQuarkPI, 0, 1, 1, 0, 0, 1, 0, 0, 0]
};

precisionGoal = 50;
epsorder = 2;
solName = "sol-2023-gg-to-gammagamma-light-quark-pi-region-b";

Print["lane101 2023-gg-to-gammagamma-light-quark-mi topology P_I AMFlow packet"];
Print["source: DOI 10.1140/epjc/s10052-023-12032-6, topology P_I, eqs. (3.1)-(3.3)"];
Print["target integrals: ", InputForm[target]];
Print["numeric point: ", InputForm[AMFlowInfo["Numeric"]]];
Print["region variables: x=1/2, y=1/3 (paper region B)"];
Print["precisionGoal: ", precisionGoal];
Print["epsorder: ", epsorder];
Print["kira executable: ", Kira`$KiraExecutable];
Print["fermat executable: ", Kira`$FermatExecutable];
Print["generated config preview: ", InputForm[GenerateNumericalConfig[precisionGoal, epsorder]]];

sol = SolveIntegrals[target, precisionGoal, epsorder];

Put[sol, FileNameJoin[{current, solName}]];
Put[
  <|
    "packet" -> "lane101-2023-gg-to-gammagamma-light-quark-mi",
    "source" -> "DOI 10.1140/epjc/s10052-023-12032-6 / Eur.Phys.J.C 83 (2023) 906",
    "family" -> "ggGammaGammaLightQuarkPI",
    "topology" -> "P_I",
    "kinematic_region" -> "paper region B rational point",
    "region_variables" -> <|"x" -> 1/2, "y" -> 1/3|>,
    "targets" -> target,
    "numeric" -> AMFlowInfo["Numeric"],
    "precisionGoal" -> precisionGoal,
    "epsorder" -> epsorder,
    "maxPrecision" -> $MaxPrecision,
    "maxExtraPrecision" -> $MaxExtraPrecision
  |>,
  FileNameJoin[{current, solName <> ".meta.wl"}]
];

Quit[];
