(* ::Package:: *)

current = If[$FrontEnd === Null, $InputFileName, NotebookFileName[]] // DirectoryName;
solName = "sol-ttbar-w-automatic-loop-eps11-proxy";
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
  j[box1, 2, 0, 1, 0],
  j[box1, -2, 1, 1, 2],
  j[box1, 1, 2, 2, 1],
  j[box2, 2, 1, 1, 1],
  j[box2, 1, 1, 0, 1],
  j[box2, 1, -1, 1, 0]
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

powers = Range[-2, 11];
coefficients = Flatten[Table[Coefficient[targetRules[[i, 2]], eps, p], {i, Length[targetRules]}, {p, powers}]];
numericCoefficients = N[coefficients, 90];
finiteNumericQ = AllTrue[numericCoefficients, NumericQ] &&
  FreeQ[numericCoefficients, Indeterminate | ComplexInfinity | DirectedInfinity[_]];
nonzeroQ = AnyTrue[numericCoefficients, Chop[#, 10^-35] =!= 0 &];
nonzeroCoefficients = Select[coefficients, Chop[N[#, 80], 10^-35] =!= 0 &];
precisions = DeleteCases[Flatten[Precision /@ coefficients], Infinity];
nonzeroPrecisions = DeleteCases[Flatten[Precision /@ nonzeroCoefficients], Infinity];
minPrecision = If[precisions === {}, Infinity, Min[precisions]];
minNonzeroPrecision = If[nonzeroPrecisions === {}, Infinity, Min[nonzeroPrecisions]];
maxAbsImag = Max[Abs[Im[N[numericCoefficients, 80]]]];
families = Sort[DeleteDuplicates[First /@ lhs]];

If[!AssociationQ[meta] ||
    Lookup[meta, "caseStudyFamily", Missing[]] =!= "ttbar-w" ||
    Lookup[meta, "selectedBenchmarkAnchorRef", Missing[]] =!= "2025-ttw-leading-colour-integrals" ||
    Lookup[meta, "family", Missing[]] =!= "automatic_loop" ||
    Lookup[meta, "topology", Missing[]] =!= "one-loop box1/box2 automatic_loop high-positive-order proxy" ||
    Lookup[meta, "precisionGoal", Missing[]] =!= 40 ||
    Lookup[meta, "epsorder", Missing[]] =!= 14 ||
    Sort[ToString[#, InputForm] & /@ Lookup[meta, "targets", {}]] =!=
      Sort[ToString[#, InputForm] & /@ expectedTargets],
  Print["metadata does not match the lane122 ttbar-w proxy capture contract: ", InputForm[meta]];
  Exit[7];
];
If[!SubsetQ[families, {box1, box2}] || !SubsetQ[{box1, box2}, families],
  Print["verification failed: solution families are not exactly box1/box2: ", InputForm[families]];
  Exit[5];
];
If[!finiteNumericQ,
  Print["verification failed: coefficients are not finite Mathematica numerics"];
  Exit[5];
];
If[!nonzeroQ,
  Print["verification failed: all coefficients are zero after anti-fake threshold"];
  Exit[5];
];
If[nonzeroPrecisions === {} || minNonzeroPrecision < 35,
  Print["verification failed: no finite-precision nonzero coefficient at or above 35 digits"];
  Exit[5];
];
If[maxAbsImag <= 10^-35,
  Print["verification failed: automatic_loop eps11 proxy has no nonzero imaginary component"];
  Exit[5];
];

Print["solution: ", solPath];
Print["metadata: ", InputForm[meta]];
Print["verified target rule count: ", Length[targetRules]];
Print["solution rule count: ", Length[sol]];
Print["families: ", InputForm[families]];
Print["coefficient_powers: ", InputForm[powers]];
Print["coefficient_count: ", Length[coefficients]];
Print["nonzero_coefficient_count: ", Length[nonzeroCoefficients]];
Print["min_precision: ", InputForm[minPrecision]];
Print["min_nonzero_precision: ", InputForm[minNonzeroPrecision]];
Print["max_abs_imaginary_component: ", InputForm[maxAbsImag]];
Print["sample_coefficients: ", InputForm[Take[coefficients, UpTo[6]]]];
Print["sample_nonzero_coefficients: ", InputForm[Take[nonzeroCoefficients, UpTo[6]]]];
Print["verification: PASS"];
Exit[0];
