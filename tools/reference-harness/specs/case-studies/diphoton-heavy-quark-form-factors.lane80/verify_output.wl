(* ::Package:: *)

current = If[$FrontEnd === Null, $InputFileName, NotebookFileName[]] // DirectoryName;
solPath = FileNameJoin[{current, "sol-diphoton-npl-j39-j42"}];
metaPath = FileNameJoin[{current, "sol-diphoton-npl-j39-j42.meta.wl"}];

If[!FileExistsQ[solPath], Print["VERIFY FAIL: solution file missing"]; Exit[2]];
If[!FileExistsQ[metaPath], Print["VERIFY FAIL: metadata file missing"]; Exit[2]];

sol = Get[solPath];
meta = Get[metaPath];

expectedTargets = {
  j[diphotonNPL, 1, 1, 1, 1, 1, 1, 1, 0, 0],
  j[diphotonNPL, 1, 1, 1, 1, 1, 1, 1, -1, 0],
  j[diphotonNPL, 1, 1, 1, 1, 2, 1, 1, 0, 0],
  j[diphotonNPL, 1, 1, 1, 1, 1, 1, 1, -1, -1]
};
expectedPrecisionGoal = 200;
expectedEpsOrder = 4;
poleOrder = 4;
minRequiredPrecision = 190;
powers = Range[-poleOrder, expectedEpsOrder];
nonzeroThreshold = 10^-180;

If[AssociationQ[sol],
  rules = Normal[sol],
  If[ListQ[sol] && AllTrue[sol, MatchQ[#, _Rule | _RuleDelayed] &],
    rules = sol,
    Print["VERIFY FAIL: solution is not an association or rule list: ", InputForm[Head[sol]]];
    Exit[3]
  ]
];

lhs = rules[[All, 1]];
expectedTargetStrings = ToString[#, InputForm] & /@ expectedTargets;
lhsStrings = ToString[#, InputForm] & /@ lhs;
missingTargetStrings = Complement[expectedTargetStrings, lhsStrings];
If[missingTargetStrings =!= {},
  Print["VERIFY FAIL: missing J39-J42 target rules: ", InputForm[missingTargetStrings]];
  Exit[4]
];

targetRules = Select[rules, MemberQ[expectedTargetStrings, ToString[#[[1]], InputForm]] &];
targetRuleStrings = ToString[#[[1]], InputForm] & /@ targetRules;
duplicateTargetStrings = Cases[Tally[targetRuleStrings], {target_, count_} /; count > 1 :> target];
If[duplicateTargetStrings =!= {},
  Print["VERIFY FAIL: duplicate J39-J42 target rules: ", InputForm[duplicateTargetStrings]];
  Exit[4]
];
If[Length[targetRules] =!= Length[expectedTargets],
  Print["VERIFY FAIL: unexpected matched J39-J42 target count: ", InputForm[Length[targetRules]]];
  Exit[4]
];

families = DeleteDuplicates[Cases[targetRules[[All, 1]], j[f_, ___] :> f, Infinity]];
rhs = targetRules[[All, 2]];
actualPowers = Sort[DeleteDuplicates[Flatten[Exponent[#, eps, List] & /@ rhs]]];
If[!SubsetQ[powers, actualPowers],
  Print["VERIFY FAIL: target Laurent powers outside declared range: ", InputForm[actualPowers]];
  Exit[5]
];
coefficientMatrix = Table[Coefficient[targetRules[[i, 2]], eps, p], {i, Length[targetRules]}, {p, powers}];
coefficients = Flatten[coefficientMatrix];
numericCoefficients = N[coefficients, 220];
numericCoefficientMatrix = N[coefficientMatrix, 220];
finiteNumericQ = AllTrue[numericCoefficients, NumericQ] &&
  FreeQ[numericCoefficients, Indeterminate | ComplexInfinity | DirectedInfinity[_]];
nonzeroTargetsQ = finiteNumericQ &&
  AllTrue[numericCoefficientMatrix, AnyTrue[#, Chop[#, nonzeroThreshold] =!= 0 &] &];
precisions = DeleteCases[Flatten[Precision /@ coefficients], Infinity];
minPrecision = If[precisions === {}, Infinity, Min[precisions]];
maxAbsImag = If[finiteNumericQ, Max[Abs[Im[N[numericCoefficients, 80]]]], Missing["NonNumeric"]];

Print["VERIFY family symbols: ", InputForm[families]];
Print["VERIFY target rule count: ", Length[targetRules]];
Print["VERIFY solution rule count: ", Length[rules]];
Print["VERIFY target Laurent powers: ", InputForm[actualPowers]];
Print["VERIFY coefficient count: ", Length[coefficients]];
Print["VERIFY min displayed precision: ", InputForm[minPrecision]];
Print["VERIFY max abs imaginary component: ", InputForm[maxAbsImag]];
Print["VERIFY precisionGoal metadata: ", Lookup[meta, "precisionGoal", Missing[]]];
Print["VERIFY epsorder metadata: ", Lookup[meta, "epsorder", Missing[]]];

If[families =!= {diphotonNPL}, Print["VERIFY FAIL: not the dedicated diphotonNPL family"]; Exit[3]];
If[!AssociationQ[meta] ||
    Lookup[meta, "family", Missing[]] =!= "diphotonNPL" ||
    Lookup[meta, "precisionGoal", Missing[]] =!= expectedPrecisionGoal ||
    Lookup[meta, "epsorder", Missing[]] =!= expectedEpsOrder ||
    Sort[ToString[#, InputForm] & /@ Lookup[meta, "targets", {}]] =!=
      Sort[ToString[#, InputForm] & /@ expectedTargets],
  Print["VERIFY FAIL: metadata does not match dedicated diphotonNPL J39-J42 contract: ", InputForm[meta]];
  Exit[6]
];
If[!finiteNumericQ, Print["VERIFY FAIL: target coefficients are not finite Mathematica numerics"]; Exit[5]];
If[!nonzeroTargetsQ, Print["VERIFY FAIL: at least one target has only zero coefficients after anti-fake threshold"]; Exit[5]];
If[minPrecision < minRequiredPrecision, Print["VERIFY FAIL: minimum target coefficient precision below ", minRequiredPrecision, " digits"]; Exit[5]];

Print["VERIFY PASS: real Mathematica numerics for dedicated diphotonNPL J39-J42 targets"];
Exit[0];
