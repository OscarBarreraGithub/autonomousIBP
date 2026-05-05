(* ::Package:: *)

current = If[$FrontEnd === Null, $InputFileName, NotebookFileName[]] // DirectoryName;
solName = "sol-2024-wpair-planar-t1-euclidean";
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
  j[wpairPlanarT1, 1, 1, 1, 0, 1, 1, 1, 0, 1],
  j[wpairPlanarT1, 0, 1, 1, 0, 1, 1, 1, 0, 1]
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

powers = Range[-4, 2];
coefficients = Flatten[Table[Coefficient[targetRules[[i, 2]], eps, p], {i, Length[targetRules]}, {p, powers}]];
numericCoefficients = N[coefficients, 90];
finiteNumericQ = AllTrue[numericCoefficients, NumericQ] &&
  FreeQ[numericCoefficients, Indeterminate | ComplexInfinity | DirectedInfinity[_]];
nonzeroQ = AnyTrue[numericCoefficients, Chop[#, 10^-45] =!= 0 &];
nonzeroCoefficients = Select[coefficients, Chop[N[#, 80], 10^-45] =!= 0 &];
precisions = DeleteCases[Flatten[Precision /@ coefficients], Infinity];
minPrecision = If[precisions === {}, Infinity, Min[precisions]];
maxAbsImag = Max[Abs[Im[N[numericCoefficients, 80]]]];

If[!AssociationQ[meta] ||
    Lookup[meta, "family", Missing[]] =!= "wpairPlanarT1" ||
    Lookup[meta, "paper_family", Missing[]] =!= "F_A" ||
    Lookup[meta, "topology", Missing[]] =!= "T1" ||
    Lookup[meta, "precisionGoal", Missing[]] =!= 50 ||
    Lookup[meta, "epsorder", Missing[]] =!= 2 ||
    Sort[ToString[#, InputForm] & /@ Lookup[meta, "targets", {}]] =!=
      Sort[ToString[#, InputForm] & /@ expectedTargets],
  Print["metadata does not match the lane106 W-pair T1 capture contract: ", InputForm[meta]];
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
If[minPrecision < 45,
  Print["verification failed: minimum finite precision below 45 digits"];
  Exit[5];
];

Print["solution: ", solPath];
Print["metadata: ", InputForm[meta]];
Print["verified target rule count: ", Length[targetRules]];
Print["solution rule count: ", Length[sol]];
Print["coefficient_powers: ", InputForm[powers]];
Print["coefficient_count: ", Length[coefficients]];
Print["nonzero_coefficient_count: ", Length[nonzeroCoefficients]];
Print["min_precision: ", InputForm[minPrecision]];
Print["max_abs_imaginary_component: ", InputForm[maxAbsImag]];
Print["sample_coefficients: ", InputForm[Take[coefficients, UpTo[6]]]];
Print["sample_nonzero_coefficients: ", InputForm[Take[nonzeroCoefficients, UpTo[6]]]];
Print["verification: PASS"];
Exit[0];
