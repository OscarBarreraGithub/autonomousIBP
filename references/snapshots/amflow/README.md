AMFlow 1.2
==========

#### Descriptions

This is a proof-of-concept implementation of the auxiliary mass flow method, which can evaluate Feynman loop integrals and/or phase-space integrations, with arbitrary number of loops and external legs, in arbitrary spacetime dimension, at arbitrary kinematic point, to arbitrary precision. The only input for this implementation is the integration-by-parts reduction table for relevant integrals. 

The auxiliary mass flow method computes integrals by: 1) introducing an auxiliary mass parameter to some of the propagator denominators to obtain an auxiliary family, 2) setting up closed differential equations for the master integrals in this family with respect to the auxiliary parameter, taking advantage of integration-by-parts reduction, 3) and solving the differential equations numberically with boundary conditions chosen at the infinity, where all integrals become linear combinations of simpler ones. These simpler boundary integrals also have similar structures and can be again evaluated using this method. Therefore, zero-input of boundary conditions can be realized with the iterative applications of this method.

As a by product, this tool also provides a solver [diffeq_solver/DESolver.m] for ordinary differential equations with user-provided differential equations and boundary conditions.

#### References

About this tool:
- Xiao Liu and Yan-Qing Ma. AMFlow: a Mathematica package for Feynman integrals computation via Auxiliary Mass Flow. [Comput. Phys. Commun. 283 (2023) 108565][arXiv: 2201.11669]

About the auxiliary mass flow method towards loop integrals:
- Xiao Liu, Yan-Qing Ma and Chen-Yu Wang. A systematic and efficient method to compute multi-loop master integrals. [Phys. Lett. B 779 (2018) 353-357][arXiv: 1711.09572]
- Xiao Liu and Yan-Qing Ma. Multiloop corrections for collider processes using auxiliary mass flow. [Phys. Rev. D 105 (2022) 5, L051503][arXiv: 2107.01864]
- Zhi-Feng Liu and Yan-Qing Ma. Determining Feynman integrals with only input from linear algebra. [Phys. Rev. Lett. 129 (2022) 22, 222001][arXiv: 2201.11637]

About the auxiliary mass flow method towards phase-space integrals:
- Xiao Liu, Yan-Qing Ma, Wei Tao and Peng Zhang. Calculation of Feynman loop integration and phase-space integration via auxiliary mass flow. [Chin. Phys. C 44 (2020) 1, 013115][arXiv: 2009.07987]

About the extension towards loop integrals with linear propagators:
- Zhi-Feng Liu and Yan-Qing Ma. Automatic computation of Feynman integrals containing linear propagators via auxiliary mass flow. [Phys. Rev. D 105 (2022) 7, 074003][arXiv: 2201.11636]

Dependencies
============

In general, this tool depends on external programs to do integration-by-parts reductions. Currently, interfaces to Blade, FiniteFlow+LiteRed, FIRE+LiteRed and Kira are provided. Interfaces to any other programs can be added freely by users according to their own requirements. (If one just wants to use the differential equations solver in this tool, then no external package is needed.)

#### Blade

If Blade is preferred, the user should first install:
 - Blade (https://gitlab.com/multiloop-pku/blade/).

Then reset the variable listed in ibp_interface/Blade/install.m:
 - $BladePath, as path to [Blade.wl], e.g., "/home/abc/blade".

#### FiniteFlow+LiteRed

If FiniteFlow+LiteRed is preferred, the user should first install:
 - FiniteFlow (https://github.com/peraro/finiteflow/);
 - LiteIBP.m (https://github.com/peraro/finiteflow-mathtools/);
 - LiteRed v1.83 or later (https://www.inp.nsk.su/~lee/programs/LiteRed/).

Then reset the variables listed in ibp_interface/FiniteFlow/install.m:
 - $FFPath, as path to [FiniteFlow.m], e.g., "/home/abc/finiteflow-master/mathlink";
 - $FFLibraryPath, as path to [fflowmlink.so] in Linux or [fflowmlink.dylib] in MacOS, e.g., "/home/abc/finiteflow-master";
 - $LiteIBPPath, as path to [LiteIBP.m], e.g., "/home/abc/finiteflow-mathtools-master/packages";
 - $LiteRedPath, as path to [LiteRed.m], e.g., "/home/abc/LiteRed".

#### FIRE+LiteRed

If FIRE+LiteRed is preferred, the user should first install:
 - FIRE, version 6 or later (https://bitbucket.org/feynmanIntegrals/fire/). Note that LiteRed (https://www.inp.nsk.su/~lee/programs/LiteRed/) has been provided in the FIRE distribution, so the user doesn't have to install LiteRed.

Then reset the variables listed in ibp_interface/FIRE+LiteRed/install.m:
 - $FIREInstallationDirectory, as the installation directory of FIRE, e.g., "/home/abc/fire/FIRE6";
 - $FIREVersion, as the version of FIRE, which should be an integer, e.g., 6.

#### Kira

If Kira is preferred, the user should first install:
 - Kira v2.2 or later (https://kira.hepforge.org/);
 - Fermat (http://home.bway.net/lewis/).

Then reset the variables listed in ibp_interface/Kira/install.m:
 - $KiraExecutable, as path to Kira executable, e.g., "/home/abc/kira-2.2";
 - $FermatExecutable, as path to Fermat executable, e.g., "/home/abc/ferl6/fer64".


Usage
=====

Several examples are provided to explain the basic usage of this tool, in examples/
 - automatic_loop - introduction to automatic evaluation of Feynman loop integrals;
 - automatic_phasespace - introduction to automatic evaluation of phase-space integrations;
 - automatic_vs_manual - introduction to automatic evaluation, and also manual evaluation in case that the automatic evaluation cannot give satisfactory results, including options setting, computing with given spacetime dimension values and fitting;
 - complex_kinematics - introduction to automatic evaluation at complex kinematic points;
 - differential_equation_solver - introduction to usage of differential equations solver, including validation of the results and computation of asymptotic expansions;
 - feynman_prescription - introduction to user-defined Feynman prescription;
 - linear_propagator - introduction to automatic evaluation of integrals with linear propagators;
 - spacetime_dimension - introduction to automatic evaluation with arbitrary spacetime dimension;
 - user_defined_amfmode - introduction to user defined mode to insert eta;
 - user_defined_ending - introduction to user defined ending systems and boundary integrals when performing manual computation.

A folder named "backup" is attached in each example, which contains some outputs during the evaluations serving as benchmarks.

NOTE for prefactor: $1/(i \pi^{D/2})$ for each loop ($-1/(i \pi^{D/2})$ for each loop with -i0 prescription) and $\delta_+(p^2-m^2)/(2 \pi)^{D-1}$ for each integrated final state particle, where $D = D_0-2\epsilon$ with $D_0 = 4$ by default is the spacetime dimension and $p (m)$ is the momentum (mass) of the corresponding particle. Please see also https://gitlab.com/multiloop-pku/amflow/-/issues/1 for clarification.


Contact
=======

If you have any questions or advices, please do not hesitate to contact us: xiao.liu@cern.ch, yqma@pku.edu.cn.
