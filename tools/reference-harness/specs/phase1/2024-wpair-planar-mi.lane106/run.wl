(* ::Package:: *)

current = If[$FrontEnd === Null, $InputFileName, NotebookFileName[]] // DirectoryName;
$MaxExtraPrecision = 2000;
$MaxPrecision = 2000;

Get[FileNameJoin[{current, "..", "..", "AMFlow.m"}]];

ClearAll[
  Lane95KiraCommandQ,
  Lane95TextFileQ,
  Lane95RewriteTextFile,
  Lane95PrepareKiraImaginaryRetry,
  Lane95RestoreKiraImaginaryOutputs,
  Lane95RunCommandRaw
];

Lane95KiraCommandQ[command_] := MatchQ[command, {_String, ___}] &&
  StringMatchQ[FileNameTake[First[command]], "kira" | "kira128" | "kira-" ~~ ___ | "kira128-" ~~ ___];

Lane95TextFileQ[file_] := FileType[file] === File &&
  FileExtension[file] =!= "db" &&
  Quiet[Check[FileByteCount[file] < 20000000, False]];

Lane95RewriteTextFile[file_, rules_] := Module[{text},
  text = Quiet[Check[Import[file, "Text"], $Failed]];
  If[StringQ[text],
    Export[file, StringReplace[text, rules], "Text"]
  ];
];

Lane95PrepareKiraImaginaryRetry[dir_] := Module[{kinematics, text, files},
  kinematics = FileNameJoin[{dir, "config", "kinematics.yaml"}];
  If[FileExistsQ[kinematics],
    text = Import[kinematics, "Text"];
    If[!StringContainsQ[text, "- [im, 2]"],
      text = StringReplace[text, "kinematic_invariants:\n" -> "kinematic_invariants:\n    - [im, 2]\n", 1];
      Export[kinematics, text, "Text"];
    ];
  ];

  files = Select[FileNames["*", FileNameJoin[{dir, "sectormappings"}], Infinity], Lane95TextFileQ];
  Scan[Lane95RewriteTextFile[#, RegularExpression["\\bI\\b"] -> "im"] &, files];

  If[DirectoryQ[FileNameJoin[{dir, "results"}]],
    DeleteDirectory[FileNameJoin[{dir, "results"}], DeleteContents -> True]
  ];
  If[DirectoryQ[FileNameJoin[{dir, "tmp"}]],
    DeleteDirectory[FileNameJoin[{dir, "tmp"}], DeleteContents -> True]
  ];
];

Lane95RestoreKiraImaginaryOutputs[dir_] := Module[{files},
  files = Select[FileNames["*", FileNameJoin[{dir, "results"}], Infinity], Lane95TextFileQ];
  Scan[Lane95RewriteTextFile[#, RegularExpression["\\bim\\b"] -> "I"] &, files];
];

Options[Lane95RunCommandRaw] = Options[AMFlow`RunCommand];
Lane95RunCommandRaw[command: _String | _List, opts: OptionsPattern[]] := Module[
  {runProcessOptions, time, process, log, fp},
  runProcessOptions = #[[1]] -> OptionValue[Lane95RunCommandRaw, {opts}, #[[1]]] & /@ Options[RunProcess];
  log = OptionValue[Lane95RunCommandRaw, {opts}, "log"];

  {time, process} = AbsoluteTiming[RunProcess[command, Sequence @@ runProcessOptions]];

  If[log =!= False,
    fp = OpenWrite[log <> ".out"];
    WriteString[fp, process["StandardOutput"]];
    Close[fp];
    fp = OpenWrite[log <> ".err"];
    WriteString[fp, process["StandardError"]];
    Close[fp];
  ];

  {process["ExitCode"], time, process}
];

Clear[AMFlow`RunCommand];
Options[AMFlow`RunCommand] = Options[Lane95RunCommandRaw];
AMFlow`RunCommand[command: _String | _List, opts: OptionsPattern[]] := Module[
  {first, second, dir, combined},
  dir = OptionValue[AMFlow`RunCommand, {opts}, ProcessDirectory];
  first = Lane95RunCommandRaw[command, opts];
  If[first[[1]] === 0,
    If[Lane95KiraCommandQ[command] && StringQ[dir], Lane95RestoreKiraImaginaryOutputs[dir]];
    Return[first[[2]]];
  ];

  combined = first[[3]]["StandardOutput"] <> first[[3]]["StandardError"];
  If[Lane95KiraCommandQ[command] && StringQ[dir] && StringContainsQ[combined, "invalid coefficient string \"I\""],
    Print["Lane95KiraImaginaryRetry: retrying Kira with generated I coefficients represented as im."];
    Lane95PrepareKiraImaginaryRetry[dir];
    second = Lane95RunCommandRaw[command, opts];
    If[second[[1]] === 0,
      Lane95RestoreKiraImaginaryOutputs[dir];
      Return[second[[2]]];
    ];
  ];

  Print["RunCommand: process failed" -> command];
  Abort[]
];

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

AMFlowInfo["Family"] = wpairPlanarT1;
AMFlowInfo["Loop"] = {l1, l2};
AMFlowInfo["Leg"] = {p1, p2, p3, p4};
AMFlowInfo["Conservation"] = {p4 -> -p1 - p2 - p3};
AMFlowInfo["Replacement"] = {
  p1^2 -> 0,
  p2^2 -> 0,
  p3^2 -> mw2,
  p4^2 -> mw2,
  (p1 + p2)^2 -> s,
  (p2 + p3)^2 -> t,
  (p1 + p3)^2 -> 2*mw2 - s - t,
  (p1 + p2 + p3)^2 -> mw2
};

(* DOI 10.1007/JHEP12(2024)136 / arXiv:2409.08879, family F_A, topology T1. *)
AMFlowInfo["Propagator"] = {
  l1^2,
  (l1 + p1)^2,
  (l1 - p2)^2,
  l2^2,
  (l2 + p1)^2,
  (l2 - p2)^2,
  (l1 - l2)^2,
  (l1 - p2 - p3)^2,
  (l2 - p2 - p3)^2 - mt2
};

AMFlowInfo["Numeric"] = {
  mt2 -> 1,
  mw2 -> -5/3,
  s -> -17/2,
  t -> -1/3
};
AMFlowInfo["NThread"] = 4;

target = {
  j[wpairPlanarT1, 1, 1, 1, 0, 1, 1, 1, 0, 1],
  j[wpairPlanarT1, 0, 1, 1, 0, 1, 1, 1, 0, 1]
};

precisionGoal = 50;
epsorder = 2;
solName = "sol-2024-wpair-planar-t1-euclidean";

Print["lane106 2024-wpair-planar-mi topology T1 AMFlow packet"];
Print["source: DOI 10.1007/JHEP12(2024)136 / arXiv:2409.08879, family F_A, topology T1"];
Print["target integrals: ", InputForm[target]];
Print["numeric point: ", InputForm[AMFlowInfo["Numeric"]]];
Print["paper variables: x=17/2, y=1/3, z=5/3, with x=-s/mt^2, y=-t/mt^2, z=-mw^2/mt^2"];
Print["precisionGoal: ", precisionGoal];
Print["epsorder: ", epsorder];
Print["kira executable: ", Kira`$KiraExecutable];
Print["fermat executable: ", Kira`$FermatExecutable];
Print["generated config preview: ", InputForm[GenerateNumericalConfig[precisionGoal, epsorder]]];

sol = SolveIntegrals[target, precisionGoal, epsorder];

Put[sol, FileNameJoin[{current, solName}]];
Put[
  <|
    "packet" -> "lane106-2024-wpair-planar-mi",
    "source" -> "DOI 10.1007/JHEP12(2024)136 / arXiv:2409.08879",
    "family" -> "wpairPlanarT1",
    "paper_family" -> "F_A",
    "topology" -> "T1",
    "kinematic_region" -> "Euclidean paper check point",
    "dimensionless_variables" -> <|"x" -> 17/2, "y" -> 1/3, "z" -> 5/3|>,
    "targets" -> target,
    "numeric" -> AMFlowInfo["Numeric"],
    "precisionGoal" -> precisionGoal,
    "epsorder" -> epsorder,
    "maxPrecision" -> $MaxPrecision,
    "maxExtraPrecision" -> $MaxExtraPrecision,
    "kiraWrapper" -> "lane95-imaginary-coefficient-retry"
  |>,
  FileNameJoin[{current, solName <> ".meta.wl"}]
];

Quit[];
