(* ::Package:: *)

(* ::Subsubsection::Closed:: *)
(*begin*)


BeginPackage["Kira`", {"AMFlow`"}];


$KiraExecutable::usage = "path to kira executable.";
$FermatExecutable::usage = "path to fermat executable.";


Begin["`Private`"];


(*Enable the use of WSL within Windows operation system*)
wslQ:=StringMatchQ[$KiraExecutable,"\\\\wsl.localhost\\"~~___];
wsl:=If[wslQ,"wsl",Nothing];
(*In Windows, WSL files are in the director: "\\\\wsl.localhost\\Ubuntu\\..."*)
pathChange:=If[wslQ,"/"<>FileNameJoin[FileNameSplit[#][[2;;-1]],OperatingSystem->"Unix"]&,Identity];
(*In WSL, Windows files "X:\\..." is in the directory: "/mnt/x/..."*)
fileNameJoin:=If[wslQ,
	StringReplace[FileNameJoin[{#[[1]]//FileNameSplit,#[[2;;-1]]}//Flatten,OperatingSystem->"Unix"],
		c_~~":":>"/mnt/"<>ToLowerCase[c]]&,
	FileNameJoin
];

kiraExecutable:=$KiraExecutable//pathChange;
fermatExecutable:=$FermatExecutable//pathChange;

runCommand[comm_, opts___]:=If[wslQ,
	RunCommand["Sequence"["wsl",All,"export FERMATPATH=\""<>fermatExecutable<>"\""<>"
"<>StringJoin[Riffle[comm," "]]],opts],
	RunCommand[comm, opts]
];
(*Must use OpenWrite[...,BinaryFormat\[Rule]True], orelse \n will be changed to \r\n in Windows*)


Family := AMFlowInfo["Family"];
Loop := AMFlowInfo["Loop"];
IndepLeg := Select[AMFlowInfo["Leg"], !MemberQ[Keys[AMFlowInfo["Conservation"]], #]&];
SPToSTU:= Module[{complete,keys,values,bare,full},
If[IndepLeg === {}, Return[{}]];
complete = Outer[Times, IndepLeg, IndepLeg]//Flatten//DeleteDuplicates;
keys = Expand[Keys[AMFlowInfo["Replacement"]]/.AMFlowInfo["Conservation"]];
values = Values[AMFlowInfo["Replacement"]];
bare = Coefficient[#, complete]&/@keys;
full = Append[Transpose[bare],values]//Transpose;
If[MatrixRank[bare]<Length[complete], Print["SPToSTU: insufficient replacement rules for all independent external scalar products."]; Abort[]];
If[MatrixRank[full]>Length[complete], Print["SPToSTU: inconsistent replacement rules."]; Abort[]];
Thread[complete -> RowReduce[full][[;;Length[complete], -1]]]
]/.IBPRule;
Propagator := AMFlowInfo["Propagator"]/.AMFlowInfo["Conservation"]/.IBPRule;
Cut := If[Head[AMFlowInfo["Cut"]]===List, AMFlowInfo["Cut"], ConstantArray[0, Length[Propagator]]];
IBPRule := If[!TrueQ[ComplexMode], AMFlowInfo["Numeric"], Select[AMFlowInfo["Numeric"], Im[#[[2]]]===0&]];
CompensateRule := Complement[AMFlowInfo["Numeric"], IBPRule];
NThread := AMFlowInfo["NThread"];


$AuxLeg:=Symbol[ToStringInput[Family]<>"AuxLeg"];
Leg:=If[IndepLeg=!={}, Append[IndepLeg, $AuxLeg], {}];
Conservation:=If[IndepLeg=!={}, {$AuxLeg -> -Total[IndepLeg]}, {{}}];


STUToSP:=Block[{right, var, col, mat, group},
right = Values[SPToSTU];
var = Variables[right];
If[var==={}, Return[{}]];
{col, mat} = CoefficientArrays[right, var]//Normal;
group = MaximalGroup[mat];
Solve[Thread[Keys[SPToSTU]==Values[SPToSTU]][[group]], var][[1]]//Expand];


Momentum:=ToSquareAll[Propagator][[1]]/.IBPRule;
Mass:=ToSquareAll[Propagator][[2]]/.IBPRule;


MassScale:=Join[Variables[Values[SPToSTU]], Complement[Variables[Propagator], Loop, IndepLeg]]//DeleteDuplicates;


$CTX = $Context;
ep = Symbol["Global`eps"];


(* ::Subsubsection::Closed:: *)
(*config*)


ListToString[list_]:=StringJoin@Riffle[ToStringInput/@list,", "];


Config[dir_]:=Module[{templatefam,templatekin,top,props,cut,kin,kirarule,rep,rule,configdir},
templatefam = If[$PermutationOption===None, 
StringToTemplate[{
"integralfamilies:
  - name: \"`name`\"
    loop_momenta: [`loop`]
    top_level_sectors: [`top`]
    propagators:`props`
    cut_propagators: [`cut`]"
}],

StringToTemplate[{
"integralfamilies:
  - name: \"`name`\"
    loop_momenta: [`loop`]
    top_level_sectors: [`top`]
    permutation_option: `perm`
    propagators:`props`
    cut_propagators: [`cut`]"
}]];


templatekin = StringToTemplate[{
"kinematics:
  incoming_momenta: [`leg`]
  outgoing_momenta: []
  momentum_conservation: [`momcon`]
  kinematic_invariants:`kin`
  scalarproduct_rules:`rep`"
}];

top = ToStringInput[Total@Table[If[TopSector[[i]]===_,Power[2,i-1],0],{i,Length@TopSector}]];
(*props = StringJoin@Table["\n      - [ \""<>ToStringInput[Momentum[[i]]]<>"\", "<>ToStringInput[Expand[-Mass[[i]]]]<>" ]",{i,Length@Momentum}];*)
props = StringJoin["\n      - [ \""<>ToStringInput[#]<>"\", 0 ]"&/@Propagator];
cut = ListToString[Flatten[Position[Cut,1]]];
kin = StringJoin["\n    - ["<>ToStringInput[#]<>", 2]"&/@MassScale];

kirarule[rule0_]:=Module[{str,p1,p2},
Which[
Head[rule0[[1]]]===Power, p1 = ToStringInput[rule0[[1,1]]];
str = "\n    - [["<>p1<>","<>p1<>"], "<>ToStringInput[Expand[rule0[[2]]]]<>"]",

Head[rule0[[1]]]===Times, p1 = ToStringInput[rule0[[1,1]]]; p2 = ToStringInput[rule0[[1,2]]];
str = "\n    - [["<>p1<>","<>p2<>"], "<>ToStringInput[Expand[rule0[[2]]]]<>"]",

True, Print["Config: unspecified rule of Kira" -> rule0]; Abort[]
];
str
];
rep = StringJoin[kirarule/@SPToSTU];

rule = <|
"name" -> ToStringInput[Family],
"loop" -> ListToString[Loop],
"top" -> top,
"perm" -> ToStringInput[$PermutationOption],
"props" -> props,
"cut" -> cut,
"leg" -> ListToString[Leg],
"momcon" -> ListToString[List@@Conservation[[1]]],
"kin" -> kin,
"rep" -> rep
|>;

configdir = FileNameJoin[{dir,"config"}];
CreateDir[configdir];
FileTemplateApply[templatefam,rule,FileNameJoin[{configdir,"integralfamilies.yaml"}]];
FileTemplateApply[templatekin,rule,FileNameJoin[{configdir,"kinematics.yaml"}]];
];


Preferred[preferred_,dir_]:=Module[{kpre,fp},
kpre = preferred/.Symbol["j"][fam_,a___]:>fam[a];

fp = OpenWrite[FileNameJoin[{dir,"preferred"}],BinaryFormat->True];
Table[WriteLine[fp,kpre[[i]]]; WriteLine[fp,""],{i,Length@kpre}];
Close[fp];
];


(* ::Subsubsection::Closed:: *)
(*job*)


ReductionJob[dir_]:=Module[{target,red,out,template,top,rule},
target = Which[
$ReductionMode === "Masters", {
"    select_integrals:
      select_mandatory_recursively:
        - {topologies: [`fam`], sectors: [`top`], r: `r`, s: `s`, d: `d`}"
},

True, {
"    select_integrals:
      select_mandatory_list:
        - [`fam`, target]"
}];

red = Which[
$ReductionMode === "Masters", {
"    run_initiate: masters"
},

$ReductionMode === "Kira", {
"    run_initiate: true
    run_triangular: true
    run_back_substitution: true"
},

$ReductionMode === "FireFly", {
"    run_initiate: true
    run_firefly: true"
},

$ReductionMode === "Mixed", {
"    run_initiate: true
    run_triangular: true
    run_firefly: back"
},

$ReductionMode === "NoFactorScan", {
"    run_initiate: true
    run_triangular: true
    run_firefly: back
    factor_scan: false"
},

True, Print["ReductionJob: undefined ReductionMode of Kira" -> $ReductionMode]; Abort[]];

out = Which[
$ReductionMode === "Masters", {},

True, {
" - kira2math:
    target:
     - [`fam`, target]"
}];

template = StringToTemplate[{
"jobs:
 - reduce_sectors:
    reduce:
     - {topologies: [`fam`], sectors: [`top`], r: `r`, s: `s`}",
Sequence@@target,
"    preferred_masters: preferred
    integral_ordering: `order`",
Sequence@@red,
Sequence@@out
}];

top = ToStringInput[Total@Table[If[TopSector[[i]]===_,Power[2,i-1],0],{i,Length@TopSector}]];
rule = <|
"fam" -> ToStringInput[Family],
"top" -> top,
"r" -> ToStringInput[Length@TopSector-Count[TopSector,0]+IBPDot],
"s" -> ToStringInput[IBPRank],
"d" -> ToStringInput[IBPDot],
"order" -> ToStringInput[$IntegralOrder]
|>;

FileTemplateApply[template,rule,FileNameJoin[{dir,"jobs.yaml"}]];
];


Target[target_, dir_]:=Module[{ktar,fp},
ktar = target/.Symbol["j"][fam_,a___]:>fam[a];

fp = OpenWrite[FileNameJoin[{dir,"target"}],BinaryFormat->True];
Table[
	WriteLine[fp,ktar[[i]]]; WriteLine[fp,""]
	,{i,Length@ktar}];
Close[fp];
];


(* ::Subsubsection::Closed:: *)
(*run kira*)


RunKira[]:=Module[{paraopt,ruleopt,cmd},
paraopt = "-p"<>ToStringInput[NThread];
ruleopt = If[#[[1]]=!=ep, "-s"<>ToStringInput[#[[1]]]<>"="<>ToStringInput[#[[2]]],"-sd="<>ToStringInput[4-2*#[[2]]]]&/@FilterRules[IBPRule, Prepend[MassScale, ep]];
cmd = {kiraExecutable, paraopt, "jobs.yaml", Sequence@@ruleopt}
];


(* ::Subsubsection::Closed:: *)
(**compute derivatives (code from LiteIBP.m provided by T.Peraro)*)


Clear[mm];


(* scalar product *)
SetAttributes[mp,Orderless];
mp[p1_Plus, p2_] := mp[#, p2] & /@ p1;
mp[p2_, coeff__ mm[p1__]] := coeff mp[mm[p1], p2];
mp[0,p_]:=0;
mp2[p_] := mp[p, p];

ExpandMP[expr_]:= expr/. {mp[a_,b_]:>mp[Expand[a,mm],Expand[b,mm]]};


(* shorthands *)
mmp[a_,b_]:=mp[mm[a],mm[b]];
mmp2[a_]:=mp[mm[a],mm[a]];


(* metric tensors in D and 4 dimensions *)
SetAttributes[gD, Orderless];
SetAttributes[g4, Orderless];


LContract[expr_,mpD_,mp4_]:= (Expand[expr]/. {gD[mm[k1_],mm[k2_]]:>mp[mm[k1],mm[k2]],
											  gD[mm[k_],mm[k_]]:>mp[mm[k],mm[k]],
											  g4[mm[k1_],mm[k2_]]:>mp[mm[k1],mm[k2]],
	                                           g4[mm[k_],mm[k_]]:>mp[mm[k],mm[k]]}) //. {
	gD[mm[k_],mu_]:>mm[k][mu],
	gD[mu_,mu_] :> MetricD,
	g4[mm[k_],mu_]:>mm[k][mu],
	g4[mu_,mu_] :> 4,
	gD[mu_,nu_]^2 :> MetricD,
    gD[mu_,nu_]gD[nu_,sigma_] :> gD[mu,sigma],
    g4[mu_,nu_]g4[nu_,sigma_] :> g4[mu,sigma],
    gD[mu_,nu_]g4[nu_,sigma_] :> g4[mu,sigma],
	g4[mu_,nu_]^2 :> 4,
	mm[k_][mu_]^2 :> mp[mm[k],mm[k]],
	mm[k1_][mu_] mm[k2_][mu_] :> mp[mm[k1],mm[k2]],
	gD[mu_,nu_] mm[k1_][mu_] mm[k2_][nu_] :> mpD[mm[k1],mm[k2]],
	g4[mu_,nu_] mm[k1_][mu_] mm[k2_][nu_] :> mp4[mm[k1],mm[k2]]}
LContract[expr_]:=LContract[expr,mp,mp] //. {gD[mu_,nu_] mm[k1_][mu_] :> mm[k1][nu],
											 g4[mu_,nu_] mm[k1_][mu_] :> mm[k1][nu]}


(* Derivatives w.r.t. mmenta *)
MomDerivative[expr_,mm[q_][mu_],dim_,gmetric_,finalcontraction_]:=
 ((D[expr /. {mm[q][nu_] :> IndexedMomentum[mm[q], nu]},mm[q]]/.
	{Derivative[1, 0][mp][a_, b_] :> b[mu],
	 Derivative[0, 1][mp][a_, b_] :> a[mu],
     Derivative[1, 0][IndexedMomentum][mm[q], mu] :> dim,  
     Derivative[1, 0][IndexedMomentum][mm[q], nu_] :> gmetric[mu,nu]}) /.
    {IndexedMomentum[mm[q],nu_] :> mm[q][nu]} ) // finalcontraction
MomDerivative[expr_,mm[q_][mu_]]:=MomDerivative[expr,mm[q][mu],MetricD,gD,LContract];


IndexedMomExpr[expr_Plus,mu_]:=IndexedMomExpr[#,mu]&/@expr;
IndexedMomExpr[coeff__ mm[a__],mu_]:=coeff IndexedMomExpr[mm[a],mu];
IndexedMomExpr[mm[a__],mu_]:=mm[a][mu];


LIBPLoopMomenta[fam_]:=Loop;
LIBPIndepExtMomenta[fam_]:=IndepLeg;
SPRule[fam_]:=Join@@Table[If[i===j,Leg[[i]]Leg[[j]]->mmp2[Leg[[i]]],Leg[[i]]Leg[[j]]->mmp[Leg[[i]],Leg[[j]]]],{i,Length@Leg},{j,i,Length@Leg}];
Props[fam_]:=mp2/@(Momentum/.Thread[Join[Loop,Leg]->mm/@Join[Loop,Leg]])+Mass/.LIBPIds[fam]//Expand;
LIBPIds[fam_]:=Thread[(SPToSTU[[All,1]]/.SPRule[fam])->SPToSTU[[All,2]]];
LIBPInvariants[fam_]:=Thread[STUToSP[[All,1]]->(Expand@STUToSP[[All,2]]/.SPRule[fam])];
LIBPDenoms[fam_]:=Table[Symbol["j"][fam, Sequence@@(-UnitVector[Length[Momentum],i])]->Together[Props[fam][[i]]/.LIBPIds[fam]],{i,1,Length[Momentum]}];


LIBPSps[fam_]:= Join[DeleteDuplicates[Flatten[Outer[mmp[#1,#2]&,LIBPLoopMomenta[fam],LIBPLoopMomenta[fam]]]],
                     Flatten[Outer[mmp[#1,#2]&,LIBPLoopMomenta[fam],LIBPIndepExtMomenta[fam]]]];


LIBPSpsToJ[fam_]:=LIBPSpsToJ[fam,Sequence@@ConstantArray[1,Length[LIBPDenoms[fam]]]];
LIBPSpsToJ[fam_,dens___]:=Module[{densidx,thisdens},
  densidx = Flatten[Position[{dens},1]];
  thisdens = LIBPDenoms[fam][[densidx]];
  Solve[(#[[2]]-#[[1]]==0)&/@thisdens,LIBPSps[fam]][[1]]
];


LIBPGetDerivatives[topo_]:=Module[
  {invs, invl, ids, pi,derivs,eqs,vars,pij,inveqs,mysys,learn,mat},
  invs = LIBPInvariants[topo];
  invl = #[[1]]&/@invs;
  ids = LIBPIds[topo];
  pi = LIBPIndepExtMomenta[topo];
  derivs = Table[-mm[LIBPDerivV[pp]][mu]+ mm[pp][mu]LIBPDerivV[mmp2[pp]]+Sum[mm[qq][mu]LIBPDerivV[mmp[pp,qq]],{qq,pi}],{pp,pi}];
  eqs=Collect[Flatten[Table[LContract[mm[pp][mu] ddd],{ddd,derivs},{pp,pi}]],_LIBPDerivV,Expand[#/.ids]&];
  pij = Flatten[Table[mmp[pi[[i]],pi[[j]]],{i,Length[pi]},{j,i,Length[pi]}]];
  inveqs = Table[LIBPDerivV[xk]-Sum[D[ppij/.ids,xk] LIBPDerivV[ppij],{ppij,pij}],{xk,invl}];
  vars=Join[LIBPDerivV/@pij,Flatten[Table[mmp[LIBPDerivV[pp],qq],{pp,pi},{qq,pi}]]];
  
  mat = CoefficientArrays[(#==0)&/@Join[inveqs,eqs], Join[LIBPDerivV/@invl,vars]][[2]]//Normal;
  mat = RowReduce[mat]//Factor;
  mat = Select[mat, FirstPosition[#,1][[1]] <= Length[invl]&];
  Solve[Thread[mat . Join[LIBPDerivV/@invl,vars] == 0], LIBPDerivV/@invl][[1]]
];


LIBPDerivivative[topo_,expr_,inv_,derivs_]:=Module[{mu},
  Together[((LIBPDerivV[inv]/.derivs)/.mp[mm[a_],mm[LIBPDerivV[b_]]]:>LContract[mm[a][mu]MomDerivative[expr/.LIBPInvariants[topo],mm[b][mu]]])/.LIBPIds[topo]]
];


LIBPComputeDerivatives[topo_]:=Module[{},
ClearAll[LIBPDenomsDeriv];
If[LIBPInvariants[topo]=!={},
LIBPDerivatives[topo]=LIBPGetDerivatives[topo];
Table[LIBPDenomsDeriv[topo,sss]=(Collect[LIBPDerivivative[topo,#,sss,LIBPDerivatives[topo]]/.LIBPSpsToJ[topo],_?(Head[#]===Symbol["j"]&),Together]&/@(#[[2]]&/@LIBPDenoms[topo]));,{sss,First/@LIBPInvariants[topo]}];
];
LIBPDenomsDeriv[topo,s_]:=LIBPDenomsDeriv[topo,s]=Collect[D[#,s]/.LIBPSpsToJ[topo],_?(Head[#]===Symbol["j"]&),Together]&/@Values[LIBPDenoms[topo]]; 
];


LIBPDeriv[j_[t_Symbol,a__],s_]:=LIBPDenomsDeriv[t,s] . ((-{a}[[#]] j[t,Sequence@@(UnitVector[Length[{a}],#]+{a})])&/@Range[Length[{a}]]);


LIBPDeriv[a_Plus,s_]:=LIBPDeriv[#,s]&/@a;
LIBPDeriv[a_Times,s_]:=Plus@@(MapAt[LIBPDeriv[#,s]&,a,#]&/@Range[Length[a]]);
LIBPDeriv[expr_,s_]:=D[expr,s];


ComputeDerivative[target_, x_]:=Collect[Expand[LIBPDeriv[#,x]&/@target]//. Symbol["j"][a_, b___] * Symbol["j"][a_, b2___] :> Symbol["j"][a, Sequence@@({b}+{b2})],_?(Head[#]===Symbol["j"]&),Together];


(* ::Subsubsection::Closed:: *)
(*usage*)


CheckDep[]:=Module[{},
Print["CheckDep: dependencies of current reducer:"];
Print["Kira executable" -> FileExistsQ[$KiraExecutable]];
Print["Fermat executable" -> FileExistsQ[$FermatExecutable]];
];


Options[SetReducerOptions] = {"IntegralOrder" -> 5, "ReductionMode" -> "Kira", "PermutationOption" -> None, "MasterRank" -> Infinity, "MasterDot" -> Infinity};
SetReducerOptions[opt___]:=Block[{},
If[MemberQ[Keys[{opt}], "IntegralOrder"], $IntegralOrder = "IntegralOrder"/.{opt}];
If[MemberQ[Keys[{opt}], "ReductionMode"], $ReductionMode = "ReductionMode"/.{opt}];
If[MemberQ[Keys[{opt}], "PermutationOption"], $PermutationOption = "PermutationOption"/.{opt}];
If[MemberQ[Keys[{opt}], "MasterRank"], $MasterRank = "MasterRank"/.{opt}];
If[MemberQ[Keys[{opt}], "MasterDot"], $MasterDot = "MasterDot"/.{opt}];

PrintOptions[SetReducerOptions, $CTX];
];


IBPSystem[top_,rank_,dot_,preferred_,complexmode_,dir_]:=Block[{time, $ReductionMode = "Masters", file, str},
CheckCompleteness[];
$ReductionDirectory = dir;
DeleteDir[dir];
CreateDir[dir];
TopSector = top;
IBPRank = rank;
IBPDot = dot;
ComplexMode = complexmode;
Config[dir];
ReductionJob[dir];
Preferred[preferred, dir];
Print[StringTemplate["IBPSystem: generating ibp system with rank `1` and dot `2`."][ToStringInput[IBPRank], ToStringInput[IBPDot]]];
time = runCommand[RunKira[], ProcessDirectory -> dir, ProcessEnvironment -> Append[GetEnvironment[], "FERMATPATH" -> fermatExecutable]];
Print[StringTemplate["IBPSystem: ibp system generated in `1`s."][ToStringInput[Ceiling[time]]]];
file = FileNameJoin[{dir, "results", ToStringInput[Family], "masters"}];
If[!FileExistsQ[file], str = {}, 
str = ReadList[file, String];
str = StringSplit/@str;
str = str[[All,1]]//ToExpression;
str = str/.Family[a___]:>Symbol["j"][Family, a]];
str = Select[str, JRank[#]<=$MasterRank && JDot[#]<=$MasterDot&];
Put[str, FileNameJoin[{dir, "results", "masters_mma"}]];
];


AnalyticReduction[target_]:=Module[{dir,masters,time,file,str,rule,col,mat},
dir = $ReductionDirectory;
masters = GetFile[FileNameJoin[{dir, "results", "masters_mma"}]];
If[Length[masters] === 0, Return[{{}, {}}]];

Target[target, dir];
ReductionJob[dir];
DeleteDir[FileNameJoin[{dir, "firefly_saves"}]];
DeleteDir[FileNameJoin[{dir, "tmp"}]];
Print[StringTemplate["AnalyticReduction: reducing `1` target integrals."][ToStringInput[Length[target]]]];
time = runCommand[RunKira[], ProcessDirectory->dir, ProcessEnvironment -> Append[GetEnvironment[], "FERMATPATH" -> fermatExecutable]];
file = FileNameJoin[{dir, "results", ToStringInput[Family], "masters"}];
If[!FileExistsQ[file], str = {}, 
str = ReadList[file, String];
str = StringSplit/@str;
str = str[[All,1]]//ToExpression;
str = str/.Family[a___]:>Symbol["j"][Family, a]];
If[!SubsetQ[masters, str], Print["AnalyticReduction: inconsistent masters from Kira."]; Abort[]];
Print[StringTemplate["AnalyticReduction: target integrals reduced to `1` master integrals in `2`s."][ToStringInput[Length[masters]], ToStringInput[Ceiling[time]]]];

file = FileNameJoin[{dir, "results", ToStringInput[Family], "kira_target.m"}];
If[!FileExistsQ[file], Return[{masters, Thread[masters -> IdentityMatrix[Length[masters]]]}]];

rule = GetFile[file]/.Family[a___] :> Symbol["j"][Family, a];
rule = Select[rule, #[[2]] =!= 0&];
If[rule === {}, Return[{masters, Thread[masters -> IdentityMatrix[Length[masters]]]}]];

{col, mat} = CoefficientArrays[Values[rule], masters]//Normal;
If[AnyTrue[col, #=!=0&], Print["AnalyticReduction: wrong reduction results from Kira."]; Abort[]];
mat = Join[mat/.Symbol["d"] -> 4-2ep, IdentityMatrix[Length@masters]];
{masters, Thread[Join[Keys[rule], masters] -> (mat/.CompensateRule)]}
];


DifferentialEquation[vars_]:=Module[{time1,time2,masters,der,integrals,masnew,rules,red,diffeq,sortmasters,pindex,perm},
Print[StringTemplate["DifferentialEquation: constructing differential equations with respect to `1`."][ToStringInput[vars]]];
time1 = AbsoluteTime[];
masters = GetFile[FileNameJoin[{$ReductionDirectory, "results", "masters_mma"}]];

LIBPComputeDerivatives[Family];
der = ComputeDerivative[masters, #]&/@vars/.IBPRule;

integrals = Cases[der,_?(Head[#]===Symbol["j"]&),Infinity]//DeleteDuplicates;
{masnew, rules} = AnalyticReduction[integrals];
If[masnew=!=masters, Print["DifferentialEquation: masters of Kira are not well-defined."]; Abort[]];
red[___]:=ConstantArray[0, Length[masnew]];
Table[red[rules[[i,1]]] = rules[[i,2]], {i,Length[rules]}];
diffeq = Together[der/.Symbol["j"][a___]:>red[Symbol["j"][a]]];
Table[If[diffeq[[i,j]]===0,diffeq[[i,j]] = ConstantArray[0,Length@masters]],{i,Length@diffeq},{j,Length@diffeq[[i]]}];

sortmasters = SortIntegrals[masters];
sortmasters = Join@@Reverse[Reverse/@GatherBy[sortmasters, JSector]];
pindex = PositionIndex[masters];
perm = Flatten[sortmasters/.pindex];
Table[diffeq[[i]] = diffeq[[i,perm,perm]],{i,Length@diffeq}];
Put[{sortmasters, vars, diffeq}, FileNameJoin[{$ReductionDirectory, "results", "diffeq"}]];
time2 = AbsoluteTime[];
Print[StringTemplate["DifferentialEquation: differential equations constructed with `1` master integrals in `2`s."][ToStringInput[Length[sortmasters]], ToStringInput[Ceiling[time2-time1]]]];
{sortmasters, vars, diffeq/.CompensateRule}
];


(* ::Subsubsection::Closed:: *)
(*end*)


GetFile[FileNameJoin[{DirectoryName[$InputFileName], "install.m"}]];
CheckDep[];
SetReducerOptions@@Options[SetReducerOptions];


End[];


EndPackage[];
