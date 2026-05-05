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
  "BlackBoxRank" -> 1,
  "BlackBoxDot" -> 0
];
If[StringQ[Environment["KIRA_EXECUTABLE"]] && Environment["KIRA_EXECUTABLE"] =!= "",
  Kira`$KiraExecutable = Environment["KIRA_EXECUTABLE"];
];
If[StringQ[Environment["FERMAT_EXECUTABLE"]] && Environment["FERMAT_EXECUTABLE"] =!= "",
  Kira`$FermatExecutable = Environment["FERMAT_EXECUTABLE"];
];

AMFlowInfo["Family"] = tthLightQuarkT0;
AMFlowInfo["Loop"] = {l1};
AMFlowInfo["Leg"] = {p1, p2, p3, p4, p5};
AMFlowInfo["Conservation"] = {p5 -> -p1 - p2 - p3 - p4};
AMFlowInfo["Replacement"] = {
  p1^2 -> mt2,
  p2^2 -> q2,
  p3^2 -> mt2,
  p4^2 -> 0,
  p1*p2 -> v12/2,
  p2*p3 -> v23/2,
  p3*p4 -> v34/2,
  p1*p3 -> (v45 - v12 - v23 - 2*mt2 - q2)/2,
  p1*p4 -> (v23 - v45 - v15 + q2)/2,
  p2*p4 -> (v15 - v23 - v34 - q2)/2
};

(* DOI 10.1007/JHEP07(2024)084 / arXiv:2312.08131, Appendix C, T0 family. *)
AMFlowInfo["Propagator"] = {
  l1^2,
  (l1 + p1)^2 - mt2,
  (l1 + p1 + p2)^2 - mt2,
  (l1 + p1 + p2 + p3)^2,
  (l1 - p5)^2
};

AMFlowInfo["Numeric"] = {
  mt2 -> 1,
  q2 -> -2,
  v12 -> -3,
  v23 -> -5,
  v34 -> -7,
  v45 -> -11,
  v15 -> -13
};
AMFlowInfo["NThread"] = 4;

target = {
  j[tthLightQuarkT0, 1, 1, 1, 1, 1],
  j[tthLightQuarkT0, 1, 1, 1, 1, 0]
};

precisionGoal = 100;
epsorder = 2;
solName = "sol-2024-tth-light-quark-loop-mi-t0-euclidean";

Print["lane117 2024-tth-light-quark-loop-mi T0 AMFlow packet"];
Print["source: DOI 10.1007/JHEP07(2024)084 / arXiv:2312.08131, Appendix C one-loop T0 family"];
Print["target integrals: ", InputForm[target]];
Print["numeric point: ", InputForm[AMFlowInfo["Numeric"]]];
Print["precisionGoal: ", precisionGoal];
Print["epsorder: ", epsorder];
Print["kira executable: ", Kira`$KiraExecutable];
Print["fermat executable: ", Kira`$FermatExecutable];
Print["generated config preview: ", InputForm[GenerateNumericalConfig[precisionGoal, epsorder]]];

sol = SolveIntegrals[target, precisionGoal, epsorder];

Put[sol, FileNameJoin[{current, solName}]];
Put[
  <|
    "packet" -> "lane117-2024-tth-light-quark-loop-mi",
    "source" -> "DOI 10.1007/JHEP07(2024)084 / arXiv:2312.08131",
    "selected_benchmark_anchor_ref" -> "2024-tth-light-quark-loop-mi",
    "case_study_family" -> "ttbar-h",
    "family" -> "tthLightQuarkT0",
    "paper_family" -> "T0",
    "topology" -> "one-loop T0 subfamily from Appendix C",
    "kinematic_region" -> "Euclidean rational seven-scale point",
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
