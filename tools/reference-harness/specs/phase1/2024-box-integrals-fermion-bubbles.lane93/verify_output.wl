(* ::Package:: *)

current = If[$FrontEnd === Null, $InputFileName, NotebookFileName[]] // DirectoryName;
solPath = FileNameJoin[{current, "sol-2024-box-integrals-fermion-bubbles-topology-h"}];
metaPath = solPath <> ".meta.wl";

If[!FileExistsQ[solPath],
  Print["missing solution: ", solPath];
  Exit[2];
];

sol = Get[solPath];
If[!FileExistsQ[metaPath],
  Print["missing metadata: ", metaPath];
  Exit[6];
];
meta = Get[metaPath];

expectedTargets = {
  j[boxFermionBubbleH, 0, 1, 0, 0, 2, 2, 0, 0, 0],
  j[boxFermionBubbleH, 1, 0, 1, 0, 1, 2, 0, 0, 0],
  j[boxFermionBubbleH, 1, 1, 1, 0, 1, 2, 0, 0, 0]
};

If[!ListQ[sol],
  Print["solution is not a list: ", InputForm[Head[sol]]];
  Exit[3];
];

lhs = sol[[All, 1]];
expectedTargetStrings = ToString[#, InputForm] & /@ expectedTargets;
lhsStrings = ToString[#, InputForm] & /@ lhs;
missingTargetStrings = Complement[expectedTargetStrings, lhsStrings];
If[missingTargetStrings =!= {},
  Print["missing expected solution targets: ", InputForm[missingTargetStrings]];
  Exit[4];
];

targetRules = Select[sol, MemberQ[expectedTargetStrings, ToString[#[[1]], InputForm]] &];
If[Length[targetRules] =!= Length[expectedTargets],
  Print["unexpected matched target rule count: ", InputForm[Length[targetRules]]];
  Exit[4];
];

rhs = targetRules[[All, 2]];
If[!And @@ (NumericQ[N[#, 40]] & /@ (rhs /. eps -> 1/37)),
  Print["solution contains non-numeric values after eps probe substitution"];
  Exit[5];
];

If[!AssociationQ[meta] ||
    Lookup[meta, "family", Missing[]] =!= "boxFermionBubbleH" ||
    Lookup[meta, "topology", Missing[]] =!= "H" ||
    Lookup[meta, "precisionGoal", Missing[]] =!= 50 ||
    Lookup[meta, "epsorder", Missing[]] =!= 4 ||
    Sort[ToString[#, InputForm] & /@ Lookup[meta, "targets", {}]] =!=
      Sort[ToString[#, InputForm] & /@ expectedTargets],
  Print["metadata does not match the lane93 topology-H capture contract: ", InputForm[meta]];
  Exit[7];
];

Print["verified target rule count: ", Length[targetRules]];
Print["solution rule count: ", Length[sol]];
Print["verified targets: ", InputForm[targetRules[[All, 1]]]];
Print["metadata present: ", FileExistsQ[metaPath]];
Print["metadata: ", InputForm[meta]];
Exit[0];
