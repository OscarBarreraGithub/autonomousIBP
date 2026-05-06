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

precisionGoal = 80;
chopPre = 40;
epsorder = 4;
SetAMFOptions["ChopPre" -> chopPre];

AMFlowInfo["Family"] = hToBbNnloSeed;
AMFlowInfo["Loop"] = {l};
AMFlowInfo["Leg"] = {p1, p2};
AMFlowInfo["Conservation"] = {};
AMFlowInfo["Replacement"] = {
  p1^2 -> mb2,
  p2^2 -> mb2,
  (p1 + p2)^2 -> mh2
};
AMFlowInfo["Propagator"] = {
  l^2 - mb2,
  (l + p1)^2 - mb2,
  (l + p1 + p2)^2 - mb2
};
AMFlowInfo["Numeric"] = {mh2 -> -7, mb2 -> 1};
AMFlowInfo["NThread"] = 4;

target = {
  j[hToBbNnloSeed, 1, 1, 1],
  j[hToBbNnloSeed, 2, 1, 1],
  j[hToBbNnloSeed, 1, 2, 1]
};

solName = "sol-2024-h-to-bb-nnlo-seed";

Print["lane154 2024-h-to-bb-nnlo AMFlow/Kira seed packet"];
Print["source anchor: DOI 10.1007/JHEP03(2024)068 / JHEP03(2024)068"];
Print["scope: one-loop equal-mass H-to-bb vertex seed, not full NNLO h-to-bb literature packet"];
Print["target integrals: ", InputForm[target]];
Print["numeric point: ", InputForm[AMFlowInfo["Numeric"]]];
Print["precisionGoal: ", precisionGoal];
Print["chopPre: ", chopPre];
Print["epsorder: ", epsorder];
Print["kira executable: ", Kira`$KiraExecutable];
Print["fermat executable: ", Kira`$FermatExecutable];
Print["generated config preview: ", InputForm[GenerateNumericalConfig[precisionGoal, epsorder]]];

sol = SolveIntegrals[target, precisionGoal, epsorder];

Put[sol, FileNameJoin[{current, solName}]];
Put[
  <|
    "packet" -> "lane154-2024-h-to-bb-nnlo",
    "source" -> "DOI 10.1007/JHEP03(2024)068 / JHEP03(2024)068",
    "selectedBenchmarkAnchorRef" -> "2024-h-to-bb-nnlo",
    "caseStudyRow" -> "h-to-bb",
    "family" -> "hToBbNnloSeed",
    "topology" -> "one-loop equal-mass H-to-bb vertex seed",
    "kinematicRegion" -> "Euclidean massive-bottom H-to-bb continuation",
    "targets" -> target,
    "numeric" -> AMFlowInfo["Numeric"],
    "propagatorMasses" -> <|"mb2" -> 1|>,
    "physicalScaleProxy" -> <|"mh2OverMb2Euclidean" -> -7|>,
    "precisionGoal" -> precisionGoal,
    "chopPre" -> chopPre,
    "epsorder" -> epsorder,
    "maxPrecision" -> $MaxPrecision,
    "maxExtraPrecision" -> $MaxExtraPrecision,
    "kiraWrapper" -> "lane95-imaginary-coefficient-retry"
  |>,
  FileNameJoin[{current, solName <> ".meta.wl"}]
];

Quit[];
