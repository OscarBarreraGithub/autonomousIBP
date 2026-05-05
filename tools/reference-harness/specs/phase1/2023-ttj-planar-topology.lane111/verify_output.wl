(* ::Package:: *)

current = If[$FrontEnd === Null, $InputFileName, NotebookFileName[]] // DirectoryName;
solName = "sol-2023-ttj-planar-topology-tt-family";
solPath = FileNameJoin[{current, solName}];
metaPath = solPath <> ".meta.wl";

If[!FileExistsQ[solPath],
  Print["missing solution: ", solPath];
  Exit[2];
];
If[!FileExistsQ[metaPath],
  Print["missing metadata: ", metaPath];
  Exit[2];
];

sol = Get[solPath];
meta = Get[metaPath];
expectedTargets = {
  j[tt, 1, 1, 1, 1, 1, 1, 1, -3, 0],
  j[tt, 1, 1, 1, 1, 1, 1, 1, -2, -1],
  j[tt, 1, 1, 1, 1, 1, 1, 1, -1, -2],
  j[tt, 1, 1, 1, 1, 1, 1, 1, 0, -3]
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

powers = Range[-5, 0];
coefficients = Flatten[Table[Coefficient[targetRules[[i, 2]], eps, p], {i, Length[targetRules]}, {p, powers}]];
numericCoefficients = N[coefficients, 90];
finiteNumericQ = AllTrue[numericCoefficients, NumericQ] &&
  FreeQ[numericCoefficients, Indeterminate | ComplexInfinity | DirectedInfinity[_]];
nonzeroQ = AnyTrue[numericCoefficients, Chop[#, 10^-45] =!= 0 &];
precisions = DeleteCases[Flatten[Precision /@ coefficients], Infinity];
minPrecision = If[precisions === {}, Infinity, Min[precisions]];
maxAbsImag = Max[Abs[Im[N[numericCoefficients, 80]]]];

If[!AssociationQ[meta] ||
    Lookup[meta, "family", Missing[]] =!= "tt" ||
    Lookup[meta, "selected_benchmark_anchor_ref", Missing[]] =!= "2023-ttj-planar-topology" ||
    Lookup[meta, "case_study_family", Missing[]] =!= "ttbar-j" ||
    Lookup[meta, "precisionGoal", Missing[]] =!= 60 ||
    Lookup[meta, "epsorder", Missing[]] =!= 4 ||
    Sort[ToString[#, InputForm] & /@ Lookup[meta, "targets", {}]] =!=
      Sort[ToString[#, InputForm] & /@ expectedTargets],
  Print["metadata does not match the lane111 ttj-planar capture contract: ", InputForm[meta]];
  Exit[7];
];
If[!finiteNumericQ,
  Print["verification failed: coefficients are not finite Mathematica numerics"];
  Exit[5];
];
If[!nonzeroQ,
  Print["verification failed: all coefficients are zero after anti-fake threshold"];
  Exit[5];
];
If[minPrecision < 55,
  Print["verification failed: minimum finite precision below 55 digits"];
  Exit[5];
];

Print["solution: ", solPath];
Print["metadata: ", InputForm[meta]];
Print["verified target rule count: ", Length[targetRules]];
Print["solution rule count: ", Length[sol]];
Print["coefficient_powers: ", InputForm[powers]];
Print["coefficient_count: ", Length[coefficients]];
Print["min_precision: ", InputForm[minPrecision]];
Print["max_abs_imaginary_component: ", InputForm[maxAbsImag]];
Print["sample_coefficients: ", InputForm[Take[coefficients, UpTo[6]]]];
Print["verification: PASS"];
Exit[0];
