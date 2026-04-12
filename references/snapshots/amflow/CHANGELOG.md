v1.2
====

Bug fixes
---------
 - Fixed a bug of differential equation solver, which might result in overflows when computing asymptotic series.
 - Fixed a bug of "Mass" mode, which might introduce eta to unexpected propagators.
 - Fixed a bug of Kira interface, which might result in unexpected terminations for the mpi version of Kira.
 - Fixed a bug of defining cache directories, which would cause privilege violation when using interactive command line.
 - Fixed a bug of "FiniteFlow+LiteRed" interface, which might result in failure when constructing differential equations.
 - Fixed a bug of boundary conditions generator, which might give incorrect result when dealing with integrals with unnormalized loop momenta.
 - Fixed a bug of AnalyzeBlock in the DESolver, which might result in incorrect numerical results.

New features
------------
 - Users can now use AMFlow on Windows operating system, with IBP reducers installed in WSL (or the Windows Subsystem for Linux). Users need to set correct paths in install.m for each reducer. 
 - Users can now use Blade as the integration-by-parts reducer, by SetReductionOptions["IBPReducer" -> "Blade"], after a proper installation. See README.md for more details.
 - Users can now perform the calculations with fixed epsilon value, by including a numerical replacement in AMFlowInfo["Numeric"], e.g., AMFlowInfo["Numeric"] = {s -> 114, t-> 514, eps -> 1919/810};
 - Users can now compute target integrals with multiple top-sectors.
 - New option for reducer "Kira": "PermutationOrder". This option allows users to change the order of propagators internally during the reduction in Kira (version >= 2.3), by SetReducerOptions["PermutationOrder" -> X], where X can be 1, 2, 3, 4, or None. By default, no permutation will be used (None). See https://gitlab.com/kira-pyred/kira/-/blob/master/ChangeLog.md?ref_type=heads for more details.
 - New option: "RationalizePre". This allows users to modify the precision goal for rationalizing numbers in the differential equations solver by SetAMFOptions["RationalizePre" -> n], where n should be a positive integer. By defalut, 100 is used as the precison goal.
 - New options added for Kira, which allow users to manually remove unexpected master integrals during the preheating stage. Specifically, the following command SetReducerOptions["MasterRank" -> xx, "MasterDot" -> yy] can help remove any master integral from the list if its rank is higher than xx or if its dot is higher than yy. By default, both options are set to Infinity, meaning no selection is performed. NOTE: Only use these options if you believe that some pseudo master integrals have appeared in the list.
 - New option: "UseCache". Users can use it to let AMFlow read data from cached files, by SetAMFOptions["UseCache" -> True]. It is useful when the computation is unexpectedly interrupted. By default, this option is disabled ( -> False). See examples/automatic_vs_manual for more detailed information.
 - New option: "SkipReduction". Users can use it to skip first-step integrals reduction when running functions like SolveIntegrals and BlackBoxAMFlow, by SetAMFOptions["SkipReduction" -> True]. It is assumed the integrals to be computed already form a basis of or a subset of master integrals.
 - New option: "RunLength". Users can use it to specify the maximally allowed number of steps during the analytic continuation of eta, by SetAMFOptions["RunLength" -> xxx]. The default value is 1000. We set a finite value as default to avoid infinite loop. If it turns out 1000 is not enough (the results can contain expressions like Transpose[$Aborted]), users can set it to be Infinity.

Other changes
-------------
 - Optimized the order of variables (including kinematic variables and epsilon) when reducing integrals with FiniteFlow+LiteRed, which may reduce the number of sample points.
 - Optimized "RunCommand" function to deal with unexpected process failure.
 - Optimized the recognition of boundary regions. Now the trivial regions will be excluded before the IBP reduction.
 - Optimized 'AnalyzeBlock' in DESolver.m. Now the function can determine the blocks correctly even if the differential equations are not cast into block-triangular form.
 - Optimized 'AnalyzeBlock' in DESolver.m. Now the function works much more efficiently for differential equations that contain large expressions.
 - Changed default option for "EndingScheme" from {"Tradition", "Cutkosky", "SingleMass"} to {"Tradition", "Cutkosky", "SingleMass", "Trivial"}, where "Trivial" is a new scheme defined for integrals that cannot be handled by other schemes.


v1.1
====

Bug fixes
---------
 - Fixed a bug of kira interface, which might result in failure of numerical reduction.

New features
------------
 - Users can now use FIRE+LiteRed as the integration-by-parts reducer, by SetReductionOptions["IBPReducer" -> "FIRE+LiteRed"], after a proper installation. See README.md for more details.
 - Users no longer need to define AMFlowInfo["Cut"] when dealing with loop integrals.
 - Users can now define Feynman prescriptions for each loop when dealing with phase-space integrations by AMFlowInfo["Prescription"] = {1, -1, ...}. See examples/feynman_prescription/run.wl for more details.
 - Users can now define their own preference to insert eta by SetAMFOptions["AMFMode" -> {"keyword1", "keyword2", ...}]. See examples/user_defined_amfmode/run.wl for more details.
 - Users can now define their own preference for ending systems and input their own boundary integrals for them by SetAMFOptions["EndingScheme" -> {"keyword1", "keyword2", ...}]. See examples/user_defined_ending/run.wl for more details.
 - Users can now compute epsilon-expansion of integrals with any D = D0-2eps using 'SolveIntegrals' by setting SetAMFOptions["D0" -> D0]. See examples/spacetime_dimension/run.wl for more details.
 - Users can now compute integrals at complex kinematic points. See examples/complex_kinematics/run.wl for more details.
 - Users can now determine whether to turn on or turn off the function of finding symmetries among external momenta when using "FiniteFlow+LiteRed" by SetReducerOptions["EMSymmetry" -> True] or SetReducerOptions["EMSymmetry" -> False]. By default, this function is turned off.
 - The function 'SolveIntegrals' can now output vanishing target integrals.
 - The function 'BlackBoxAMFlow' can now compute any target integrals, while the old version can only deal with master integrals.

Other changes
-------------
 - Optimized the organization of temporary files.
 - Optimized the generation of boundary integrals.
 - Optimized the support for MacOS.
 - Changed the default value of "AMFMode" in SetAMFOptions to {"Prescription", "Mass", "Propagator"}.
 - Changed the default value of "DeleteBlackBoxDirectory" in SetReductionOptions to False.
 - Changed the names of many internal functions.


v1.0
====
 - Released.
