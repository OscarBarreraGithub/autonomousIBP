(* ::Package:: *)

current = If[$FrontEnd === Null, $InputFileName, NotebookFileName[]] // DirectoryName;
solPath = FileNameJoin[{current, "sol-diphoton-npl-j39-j42"}];
metaPath = FileNameJoin[{current, "sol-diphoton-npl-j39-j42.meta.wl"}];

If[!FileExistsQ[solPath], Print["VERIFY FAIL: solution file missing"]; Exit[2]];
If[!FileExistsQ[metaPath], Print["VERIFY FAIL: metadata file missing"]; Exit[2]];

sol = Get[solPath];
meta = Get[metaPath];

keys = Keys[sol];
families = DeleteDuplicates[Cases[keys, j[f_, ___] :> f, Infinity]];
values = Values[sol];
coefficients = Flatten[CoefficientList[#, eps] & /@ values];
numericCoefficients = Select[coefficients, NumericQ[N[#]] &];
precisions = Quiet[Precision /@ N[numericCoefficients, 220]];

Print["VERIFY family symbols: ", InputForm[families]];
Print["VERIFY target count: ", Length[keys]];
Print["VERIFY numeric coefficient count: ", Length[numericCoefficients]];
Print["VERIFY min displayed precision: ", InputForm[Min[Cases[precisions, _Integer | _Real]]]];
Print["VERIFY precisionGoal metadata: ", meta["precisionGoal"]];
Print["VERIFY epsorder metadata: ", meta["epsorder"]];

If[families =!= {diphotonNPL}, Print["VERIFY FAIL: not the dedicated diphotonNPL family"]; Exit[3]];
If[Length[keys] =!= 4, Print["VERIFY FAIL: expected J39-J42 target count"]; Exit[4]];
If[Length[numericCoefficients] == 0, Print["VERIFY FAIL: no numeric coefficients"]; Exit[5]];
If[meta["precisionGoal"] =!= 200, Print["VERIFY FAIL: precision goal mismatch"]; Exit[6]];

Print["VERIFY PASS: real Mathematica numerics for dedicated diphotonNPL packet"];
Exit[0];
