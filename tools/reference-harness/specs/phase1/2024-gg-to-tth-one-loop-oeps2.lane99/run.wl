(* ::Package:: *)

current = If[$FrontEnd === Null, $InputFileName, NotebookFileName[]] // DirectoryName;
$MaxExtraPrecision = 2000;
$MaxPrecision = 2000;

Get[FileNameJoin[{current, "package", "DESolver.m"}]];
Get[FileNameJoin[{current, "package", "OneLoopAMFlow.m"}]];

(* arXiv:2312.10015 / JHEP03(2024)093, Table 1 topology C. *)
Momenta[PentaboxGGtoTTHTopologyC] = {
  k1,
  k1 + p1,
  k1 + p1 + p2,
  k1 + p1 + p2 + p4,
  k1 + p1 + p2 + p3 + p4
};
Masses[PentaboxGGtoTTHTopologyC] = {mt2, mt2, mt2, 0, mt2};

Replacements = {
  p3*p4 -> (-2*mt2 + s34)/2,
  p2*p3 -> (-mt2 + s23)/2,
  p2*p4 -> (-mt2 + s24)/2,
  p1*p2 -> s12/2,
  p1*p3 -> (-mt2 + s13)/2,
  p1*p4 -> (-mt2 + s14)/2,
  p1^2 -> 0,
  p2^2 -> 0,
  p3^2 -> mt2,
  p4^2 -> mt2
};

Numerics = {
  s12 -> -3,
  s13 -> -5,
  s14 -> -7,
  s23 -> -11,
  s24 -> -13,
  s34 -> -17,
  mt2 -> 1
};

SymmetryRelations = {};
Masters = {
  Term[PentaboxGGtoTTHTopologyC, D, {0, 1, 1, 1, 1}],
  Term[PentaboxGGtoTTHTopologyC, D, {1, 1, 1, 1, 1}]
};

precisionGoal = 50;
epsorder = 2;
solName = "sol-2024-gg-to-tth-one-loop-oeps2-topology-c-euclidean";

Print["lane99 2024-gg-to-tth-one-loop-oeps2 topology C AMF packet"];
Print["source: arXiv:2312.10015 / JHEP03(2024)093, Table 1 and Sec. 7"];
Print["target integrals: ", InputForm[Masters]];
Print["numeric point: ", InputForm[Numerics]];
Print["precisionGoal: ", precisionGoal];
Print["epsorder: ", epsorder];
Print["generated config preview: ", InputForm[GenerateNumericalConfig[precisionGoal, epsorder]]];

sol = EvaluateMasters[precisionGoal, epsorder];

Put[sol, FileNameJoin[{current, solName}]];
Put[
  <|
    "packet" -> "lane99-2024-gg-to-tth-one-loop-oeps2",
    "source" -> "arXiv:2312.10015 / JHEP03(2024)093",
    "source_code" -> "https://github.com/p-a-kreer/TTH",
    "family" -> "PentaboxGGtoTTHTopologyC",
    "topology" -> "C",
    "kinematic_region" -> "Euclidean rational invariant point",
    "targets" -> Masters,
    "numeric" -> Numerics,
    "precisionGoal" -> precisionGoal,
    "epsorder" -> epsorder,
    "maxPrecision" -> $MaxPrecision,
    "maxExtraPrecision" -> $MaxExtraPrecision
  |>,
  FileNameJoin[{current, solName <> ".meta.wl"}]
];

Quit[];
