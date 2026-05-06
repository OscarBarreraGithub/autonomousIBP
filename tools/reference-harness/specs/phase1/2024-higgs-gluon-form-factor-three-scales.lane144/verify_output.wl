(* ::Package:: *)

current = If[$FrontEnd === Null, $InputFileName, NotebookFileName[]] // DirectoryName;
solName = "sol-2024-higgs-gluon-form-factor-three-scales-seed";
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
  j[hggThreeScaleSeed, 1, 1, 1],
  j[hggThreeScaleSeed, 2, 1, 1],
  j[hggThreeScaleSeed, 1, 2, 1]
};
expectedTargetStrings = ToString[#, InputForm] & /@ expectedTargets;

If[!ListQ[sol],
  Print["solution is not a list: ", InputForm[Head[sol]]];
  Exit[3];
];
If[!AllTrue[sol, MatchQ[#, Rule[_, _]] &],
  Print["solution contains non-rule entries: ", InputForm[Select[sol, !MatchQ[#, Rule[_, _]] &, UpTo[3]]]];
  Exit[3];
];

lhs = sol[[All, 1]];
lhsStrings = ToString[#, InputForm] & /@ lhs;
duplicateSelectedTargetStrings = Select[Tally[Select[lhsStrings, MemberQ[expectedTargetStrings, #] &]], #[[2]] > 1 &][[All, 1]];
missingTargetStrings = Complement[expectedTargetStrings, lhsStrings];
If[missingTargetStrings =!= {},
  Print["missing expected solution targets: ", InputForm[missingTargetStrings]];
  Exit[4];
];
If[duplicateSelectedTargetStrings =!= {},
  Print["duplicate expected solution targets: ", InputForm[duplicateSelectedTargetStrings]];
  Exit[4];
];

targetRules = Select[sol, MemberQ[expectedTargetStrings, ToString[#[[1]], InputForm]] &];
If[Length[targetRules] =!= Length[expectedTargets],
  Print["unexpected matched target rule count: ", InputForm[Length[targetRules]]];
  Exit[4];
];

If[!AssociationQ[meta] ||
    Lookup[meta, "selectedBenchmarkAnchorRef", Missing[]] =!= "2024-higgs-gluon-form-factor-three-scales" ||
    Lookup[meta, "family", Missing[]] =!= "hggThreeScaleSeed" ||
    Lookup[meta, "topology", Missing[]] =!= "one-loop two-mass Higgs-gluon form-factor seed" ||
    Lookup[meta, "kinematicRegion", Missing[]] =!= "Euclidean q2 below two-mass thresholds" ||
    Lookup[meta, "numeric", Missing[]] =!= {q2 -> -3, mt2 -> 1, mb2 -> 4/25} ||
    Lookup[meta, "precisionGoal", Missing[]] =!= 50 ||
    Lookup[meta, "epsorder", Missing[]] =!= 4 ||
    Sort[ToString[#, InputForm] & /@ Lookup[meta, "targets", {}]] =!= Sort[expectedTargetStrings],
  Print["metadata does not match the lane144 hgg seed capture contract: ", InputForm[meta]];
  Exit[7];
];

powers = Range[-1, 4];
coefficientsByTarget = Table[
  Coefficient[targetRules[[i, 2]], eps, p],
  {i, Length[targetRules]},
  {p, powers}
];
coefficients = Flatten[coefficientsByTarget];
numericCoefficients = N[coefficients, 90];
finiteNumericQ = AllTrue[numericCoefficients, NumericQ] &&
  FreeQ[numericCoefficients, Indeterminate | ComplexInfinity | DirectedInfinity[_]];
nonzeroCoefficients = Select[coefficients, Chop[N[#, 80], 10^-45] =!= 0 &];
targetNonzeroCounts = Count[#, value_ /; Chop[N[value, 80], 10^-45] =!= 0] & /@ coefficientsByTarget;
precisions = DeleteCases[Flatten[Precision /@ coefficients], Infinity];
nonzeroPrecisions = DeleteCases[Flatten[Precision /@ nonzeroCoefficients], Infinity];
minPrecision = If[precisions === {}, Infinity, Min[precisions]];
minNonzeroPrecision = If[nonzeroPrecisions === {}, Infinity, Min[nonzeroPrecisions]];
maxAbsImag = Max[Abs[Im[N[numericCoefficients, 80]]]];

If[!finiteNumericQ,
  Print["verification failed: coefficients are not finite Mathematica numerics"];
  Exit[5];
];
If[!AllTrue[targetNonzeroCounts, # > 0 &],
  Print["verification failed: at least one expected target is all zero after anti-fake threshold: ", InputForm[targetNonzeroCounts]];
  Exit[5];
];
If[nonzeroPrecisions === {} || minNonzeroPrecision < 45,
  Print["verification failed: no finite-precision nonzero coefficient at or above 45 digits"];
  Exit[5];
];
If[maxAbsImag > 10^-45,
  Print["verification failed: Euclidean hgg seed has an unexpected imaginary component: ", InputForm[maxAbsImag]];
  Exit[5];
];

Print["solution: ", solPath];
Print["metadata: ", InputForm[meta]];
Print["verified target rule count: ", Length[targetRules]];
Print["solution rule count: ", Length[sol]];
Print["expected_targets: ", InputForm[expectedTargetStrings]];
Print["coefficient_powers: ", InputForm[powers]];
Print["coefficient_count: ", Length[coefficients]];
Print["target_nonzero_counts: ", InputForm[targetNonzeroCounts]];
Print["nonzero_coefficient_count: ", Length[nonzeroCoefficients]];
Print["min_precision: ", InputForm[minPrecision]];
Print["min_nonzero_precision: ", InputForm[minNonzeroPrecision]];
Print["max_abs_imaginary_component: ", InputForm[maxAbsImag]];
Print["sample_coefficients: ", InputForm[Take[coefficients, UpTo[6]]]];
Print["sample_nonzero_coefficients: ", InputForm[Take[nonzeroCoefficients, UpTo[6]]]];
Print["verification: PASS"];
Exit[0];
