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

SetReductionOptions["IBPReducer" -> "Kira"];
If[StringQ[Environment["KIRA_EXECUTABLE"]] && Environment["KIRA_EXECUTABLE"] =!= "",
  Kira`$KiraExecutable = Environment["KIRA_EXECUTABLE"];
];
If[StringQ[Environment["FERMAT_EXECUTABLE"]] && Environment["FERMAT_EXECUTABLE"] =!= "",
  Kira`$FermatExecutable = Environment["FERMAT_EXECUTABLE"];
];

precisionGoal = 40;
epsorder = 14;
solName = "sol-ttbar-w-automatic-loop-eps11-proxy";

box1Targets = {j[box1, 2, 0, 1, 0], j[box1, -2, 1, 1, 2], j[box1, 1, 2, 2, 1]};
box2Targets = {j[box2, 2, 1, 1, 1], j[box2, 1, 1, 0, 1], j[box2, 1, -1, 1, 0]};

Print["lane122 ttbar-w automatic_loop eps11 proxy AMFlow packet"];
Print["source anchor: arXiv:2412.13876 / ttbar-W leading-colour selected benchmark"];
Print["proxy surface: retained automatic_loop eps11 high-positive-order matrix"];
Print["precisionGoal: ", precisionGoal];
Print["epsorder: ", epsorder];
Print["kira executable: ", Kira`$KiraExecutable];
Print["fermat executable: ", Kira`$FermatExecutable];

AMFlowInfo["Family"] = box1;
AMFlowInfo["Loop"] = {l};
AMFlowInfo["Leg"] = {p1, p2, p3, p4};
AMFlowInfo["Conservation"] = {p4 -> -p1 - p2 - p3};
AMFlowInfo["Replacement"] = {
  p1^2 -> 0,
  p2^2 -> 0,
  p3^2 -> 0,
  p4^2 -> 0,
  (p1 + p2)^2 -> s,
  (p1 + p3)^2 -> t
};
AMFlowInfo["Propagator"] = {l^2, (l + p1)^2, (l + p1 + p2)^2, (l + p1 + p2 + p4)^2};
AMFlowInfo["Numeric"] = {s -> 100, t -> -1};
AMFlowInfo["NThread"] = 4;

Print["box1 targets: ", InputForm[box1Targets]];
Print["box1 numeric point: ", InputForm[AMFlowInfo["Numeric"]]];
Print["box1 generated config preview: ", InputForm[GenerateNumericalConfig[precisionGoal, epsorder]]];
sol1 = SolveIntegrals[box1Targets, precisionGoal, epsorder];
Put[sol1, FileNameJoin[{current, solName <> ".box1"}]];

AMFlowInfo["Family"] = box2;
AMFlowInfo["Loop"] = {l};
AMFlowInfo["Leg"] = {p1, p2, p3, p4};
AMFlowInfo["Conservation"] = {p4 -> -p1 - p2 - p3};
AMFlowInfo["Replacement"] = {
  p1^2 -> 0,
  p2^2 -> 0,
  p3^2 -> 0,
  p4^2 -> 0,
  (p1 + p2)^2 -> s,
  (p1 + p3)^2 -> t
};
AMFlowInfo["Propagator"] = {l^2, (l + p1)^2, (l + p1 + p2)^2, (l + p1 + p2 + p3)^2};
AMFlowInfo["Numeric"] = {s -> 100, t -> -99};
AMFlowInfo["NThread"] = 4;

Print["box2 targets: ", InputForm[box2Targets]];
Print["box2 numeric point: ", InputForm[AMFlowInfo["Numeric"]]];
Print["box2 generated config preview: ", InputForm[GenerateNumericalConfig[precisionGoal, epsorder]]];
sol2 = SolveIntegrals[box2Targets, precisionGoal, epsorder];
Put[sol2, FileNameJoin[{current, solName <> ".box2"}]];

Put[Join[sol1, sol2], FileNameJoin[{current, solName}]];
Put[
  <|
    "packet" -> "lane122-ttbar-w",
    "source" -> "arXiv:2412.13876 / JHEP03(2025)070",
    "caseStudyFamily" -> "ttbar-w",
    "selectedBenchmarkAnchorRef" -> "2025-ttw-leading-colour-integrals",
    "sourceReference" -> "tools/reference-harness/specs/case-studies/ttbar-w.numeric-evidence.json",
    "family" -> "automatic_loop",
    "topology" -> "one-loop box1/box2 automatic_loop high-positive-order proxy",
    "profile" -> "retained automatic_loop eps11 AMFlow precision profile",
    "box1Targets" -> box1Targets,
    "box2Targets" -> box2Targets,
    "targets" -> Join[box1Targets, box2Targets],
    "box1Numeric" -> {s -> 100, t -> -1},
    "box2Numeric" -> {s -> 100, t -> -99},
    "precisionGoal" -> precisionGoal,
    "epsorder" -> epsorder,
    "maxPrecision" -> $MaxPrecision,
    "maxExtraPrecision" -> $MaxExtraPrecision,
    "kiraWrapper" -> "lane95-imaginary-coefficient-retry"
  |>,
  FileNameJoin[{current, solName <> ".meta.wl"}]
];

Quit[];
