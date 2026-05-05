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

If[!ListQ[sol] || Length[sol] != Length[expectedTargets],
  Print["unexpected solution rule count: ", InputForm[Length[sol]]];
  Exit[3];
];

lhs = sol[[All, 1]];
If[Sort[ToString[#, InputForm] & /@ lhs] =!= Sort[ToString[#, InputForm] & /@ expectedTargets],
  Print["unexpected solution targets: ", InputForm[lhs]];
  Exit[4];
];

rhs = sol[[All, 2]];
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

Print["verified rule count: ", Length[sol]];
Print["verified targets: ", InputForm[lhs]];
Print["metadata present: ", FileExistsQ[metaPath]];
Print["metadata: ", InputForm[meta]];
Exit[0];
