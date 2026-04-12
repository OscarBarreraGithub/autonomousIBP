FAQ
===


Q: Output of SolveIntegrals or BlackBoxAMFlow is something like {j[fam,...] -> {DESolver`Private`variables[1,1],...},...}
-------------------------------------------------------------------------------------------------------------------------

A: Make sure your Mathematica version is 11.3 or higher and IBP reducer has been installed properly.


Q: Error reported: "BlackBoxAMFlow: some master integrals have not been solved yet." or "AMFSystemCombineSolution: some boundary integrals have not been solved yet in this system -> xx."
------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

A: This may be solved by increasing "BlackBoxRank" (3 by default) and "BlackBoxDot" (0 by default) through e.g.
		SetReductionOptions["BlackBoxRank" -> 4, "BlackBoxDot" -> 1];
before running SolveIntegrals or BlackBoxAMFlow. Note they should not be increased immoderately becasue the computational cost may increase heavily with these parameters.


Q: Error reported: "GetFile: file not found: /../../border"
-----------------------------------------------------------

A: This may be solved by increasing "ExtraXOrder" (20 by default) through e.g.
		SetAMFOptions["ExtraXOrder" -> 30];
before running SolveIntegrals or BlackBoxAMFlow.


Q: SolveIntegrals or BlackBoxAMFlow gets stuck at "AMFSystemSolution: all subsystems of system xx done, solving differential equations."
----------------------------------------------------------------------------------------------------------------------------------------

A: This may be solved by resetting "LearnXOrder" (if it has been modified) as a negative integer through e.g.
		SetAMFOptions["LearnXOrder" -> -1];
before running SolveIntegrals or BlackBoxAMFlow.