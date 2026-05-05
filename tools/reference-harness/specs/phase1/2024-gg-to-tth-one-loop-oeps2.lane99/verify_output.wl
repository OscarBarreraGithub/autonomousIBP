(* ::Package:: *)

current = If[$FrontEnd === Null, $InputFileName, NotebookFileName[]] // DirectoryName;
solName = "sol-2024-gg-to-tth-one-loop-oeps2-topology-c-euclidean";
solPath = FileNameJoin[{current, solName}];
metaPath = solPath <> ".meta.wl";

If[!FileExistsQ[solPath],
  Print["missing solution: ", solPath];
  Exit[2]
];
If[!FileExistsQ[metaPath],
  Print["missing metadata: ", metaPath];
  Exit[2]
];

sol = Get[solPath];
meta = Get[metaPath];
targets = meta["targets"];
powers = Range[-2, 2];
coefficients = Flatten[Table[Coefficient[sol[[i]], eps, p], {i, Length[sol]}, {p, powers}]];
finiteNumericQ = AllTrue[coefficients, NumericQ] &&
  FreeQ[coefficients, Indeterminate | ComplexInfinity | DirectedInfinity[_]];
nonzeroQ = AnyTrue[coefficients, Chop[#, 10^-45] =!= 0 &];
precisions = DeleteCases[Flatten[Precision /@ coefficients], Infinity];
minPrecision = If[precisions === {}, Infinity, Min[precisions]];
maxAbsImag = Max[Abs[Im[N[coefficients, 80]]]];

Print["solution: ", solPath];
Print["metadata: ", InputForm[meta]];
Print["target_count: ", Length[targets]];
Print["solution_count: ", Length[sol]];
Print["coefficient_powers: ", InputForm[powers]];
Print["coefficient_count: ", Length[coefficients]];
Print["min_precision: ", InputForm[minPrecision]];
Print["max_abs_imaginary_tail: ", InputForm[maxAbsImag]];
Print["sample_coefficients: ", InputForm[Take[coefficients, UpTo[6]]]];

If[Length[sol] =!= Length[targets],
  Print["verification failed: solution/target length mismatch"];
  Exit[3]
];
If[!finiteNumericQ,
  Print["verification failed: coefficients are not finite Mathematica numerics"];
  Exit[3]
];
If[!nonzeroQ,
  Print["verification failed: all coefficients are zero after anti-fake threshold"];
  Exit[3]
];
If[minPrecision < 45,
  Print["verification failed: minimum precision below 45 digits"];
  Exit[3]
];
If[maxAbsImag > 10^-35,
  Print["verification failed: Euclidean run has unexpectedly large imaginary tail"];
  Exit[3]
];

Print["verification: PASS"];
Exit[0];
