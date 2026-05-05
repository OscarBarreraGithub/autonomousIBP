(* ::Package:: *)

current = If[$FrontEnd === Null, $InputFileName, NotebookFileName[]] // DirectoryName;
solPath = FileNameJoin[{current, "sol-single-top-pl1-euclidean"}];
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
  j[singleTopPL1, 0, 0, 0, 0, 1, 0, 0, 2, 2],
  j[singleTopPL1, 1, 1, 1, 0, 1, 1, 0, 1, 1]
};
expectedPrecisionGoal = 50;
expectedEpsOrder = 4;
poleOrder = 4;
minRequiredPrecision = 45;
powers = Range[-poleOrder, expectedEpsOrder];
nonzeroThreshold = 10^-45;

If[!ListQ[sol] || !AllTrue[sol, MatchQ[#, _Rule | _RuleDelayed] &],
  Print["solution is not a rule list: ", InputForm[Head[sol]]];
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
targetRuleStrings = ToString[#[[1]], InputForm] & /@ targetRules;
duplicateTargetStrings = Cases[Tally[targetRuleStrings], {target_, count_} /; count > 1 :> target];
If[duplicateTargetStrings =!= {},
  Print["duplicate expected solution targets: ", InputForm[duplicateTargetStrings]];
  Exit[4];
];
If[Length[targetRules] =!= Length[expectedTargets],
  Print["unexpected matched target rule count: ", InputForm[Length[targetRules]]];
  Exit[4];
];

rhs = targetRules[[All, 2]];
actualPowers = Sort[DeleteDuplicates[Flatten[Exponent[#, eps, List] & /@ rhs]]];
If[!SubsetQ[powers, actualPowers],
  Print["target Laurent powers outside declared range: ", InputForm[actualPowers]];
  Exit[5];
];
coefficientMatrix = Table[Coefficient[targetRules[[i, 2]], eps, p], {i, Length[targetRules]}, {p, powers}];
coefficients = Flatten[coefficientMatrix];
numericCoefficients = N[coefficients, 90];
numericCoefficientMatrix = N[coefficientMatrix, 90];
finiteNumericQ = AllTrue[numericCoefficients, NumericQ] &&
  FreeQ[numericCoefficients, Indeterminate | ComplexInfinity | DirectedInfinity[_]];
nonzeroTargetsQ = finiteNumericQ &&
  AllTrue[numericCoefficientMatrix, AnyTrue[#, Chop[#, nonzeroThreshold] =!= 0 &] &];
precisions = DeleteCases[Flatten[Precision /@ coefficients], Infinity];
minPrecision = If[precisions === {}, Infinity, Min[precisions]];
maxAbsImag = If[finiteNumericQ, Max[Abs[Im[N[numericCoefficients, 80]]]], Missing["NonNumeric"]];

If[!finiteNumericQ,
  Print["target coefficients are not finite Mathematica numerics"];
  Exit[5];
];
If[!nonzeroTargetsQ,
  Print["at least one target has only zero coefficients after anti-fake threshold"];
  Exit[5];
];
If[minPrecision < minRequiredPrecision,
  Print["minimum target coefficient precision below ", minRequiredPrecision, " digits"];
  Exit[5];
];

If[!AssociationQ[meta] ||
    Lookup[meta, "family", Missing[]] =!= "singleTopPL1" ||
    Lookup[meta, "topology", Missing[]] =!= "PL1" ||
    Lookup[meta, "kinematic_region", Missing[]] =!= "Euclidean" ||
    Lookup[meta, "kinematic_point", <||>] =!= <|"x" -> 45/11, "S12" -> -968/45, "S23" -> 704/45, "mw2" -> 974|> ||
    Lookup[meta, "precisionGoal", Missing[]] =!= expectedPrecisionGoal ||
    Lookup[meta, "epsorder", Missing[]] =!= expectedEpsOrder ||
    Sort[ToString[#, InputForm] & /@ Lookup[meta, "targets", {}]] =!=
      Sort[ToString[#, InputForm] & /@ expectedTargets],
  Print["metadata does not match the lane96 single-top PL1 capture contract: ", InputForm[meta]];
  Exit[7];
];

Print["verified target rule count: ", Length[targetRules]];
Print["solution rule count: ", Length[sol]];
Print["verified targets: ", InputForm[targetRules[[All, 1]]]];
Print["target Laurent powers: ", InputForm[actualPowers]];
Print["coefficient powers: ", InputForm[powers]];
Print["coefficient count: ", Length[coefficients]];
Print["min precision: ", InputForm[minPrecision]];
Print["max abs imaginary component: ", InputForm[maxAbsImag]];
Print["metadata present: ", FileExistsQ[metaPath]];
Print["metadata: ", InputForm[meta]];
Exit[0];
