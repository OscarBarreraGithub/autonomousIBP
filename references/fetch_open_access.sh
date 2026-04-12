#!/bin/zsh
set -euo pipefail

root="${0:a:h}"

mkdir -p \
  "$root/theory/auxiliary-mass-flow" \
  "$root/theory/differential-equations" \
  "$root/theory/boundaries-and-regions" \
  "$root/theory/analytic-continuation-and-singularities" \
  "$root/theory/series-solvers" \
  "$root/theory/precision-and-error-control" \
  "$root/ibp/kira" \
  "$root/ibp/fire" \
  "$root/ibp/litered" \
  "$root/ibp/blade" \
  "$root/finite-fields/foundations" \
  "$root/finite-fields/finiteflow" \
  "$root/finite-fields/firefly" \
  "$root/case-studies/papers" \
  "$root/snapshots/amflow" \
  "$root/snapshots/kira" \
  "$root/snapshots/fire" \
  "$root/snapshots/litered" \
  "$root/snapshots/finiteflow" \
  "$root/snapshots/firefly" \
  "$root/snapshots/numeric"

fetch() {
  local url="$1"
  local out="$2"
  curl -L --fail --silent --show-error \
    -A "Mozilla/5.0" \
    --retry 2 \
    --retry-delay 1 \
    "$url" -o "$out"
}

fetch_if_missing() {
  local url="$1"
  local out="$2"
  if [[ ! -f "$out" ]]; then
    fetch "$url" "$out"
  fi
}

# Core AMFlow papers
fetch_if_missing "https://arxiv.org/pdf/1711.09572.pdf" "$root/theory/auxiliary-mass-flow/2017-1711.09572-systematic-efficient-method.pdf"
fetch_if_missing "https://arxiv.org/pdf/2009.07987.pdf" "$root/theory/auxiliary-mass-flow/2020-2009.07987-amf-phase-space.pdf"
fetch_if_missing "https://arxiv.org/pdf/2107.01864.pdf" "$root/theory/auxiliary-mass-flow/2021-2107.01864-collider-processes-amf.pdf"
fetch_if_missing "https://arxiv.org/pdf/2201.11636.pdf" "$root/theory/auxiliary-mass-flow/2022-2201.11636-linear-propagators-amf.pdf"
fetch_if_missing "https://arxiv.org/pdf/2201.11637.pdf" "$root/theory/auxiliary-mass-flow/2022-2201.11637-linear-algebra-only.pdf"
fetch_if_missing "https://arxiv.org/pdf/2201.11669.pdf" "$root/theory/auxiliary-mass-flow/2022-2201.11669-amflow-package.pdf"
fetch_if_missing "https://arxiv.org/pdf/2401.08226.pdf" "$root/theory/auxiliary-mass-flow/2024-2401.08226-singular-kinematics-amf.pdf"

# Differential equations and solver theory
fetch_if_missing "https://arxiv.org/pdf/hep-th/9711188.pdf" "$root/theory/differential-equations/1997-hep-th-9711188-remiddi.pdf"
fetch_if_missing "https://arxiv.org/pdf/hep-ph/9912329.pdf" "$root/theory/differential-equations/1999-hep-ph-9912329-gehrmann-remiddi.pdf"
fetch_if_missing "https://arxiv.org/pdf/1304.1806.pdf" "$root/theory/differential-equations/2013-1304.1806-henn-canonical-basis.pdf"
fetch_if_missing "https://arxiv.org/pdf/1411.0911.pdf" "$root/theory/differential-equations/2015-1411.0911-lee-epsilon-form.pdf"
fetch_if_missing "https://arxiv.org/pdf/1812.03060.pdf" "$root/theory/series-solvers/2019-1812.03060-numerical-des.pdf"
fetch_if_missing "https://arxiv.org/pdf/1709.07525.pdf" "$root/theory/series-solvers/2018-1709.07525-smirnov-lee-singular-points.pdf"
fetch_if_missing "https://arxiv.org/pdf/2006.05510.pdf" "$root/theory/series-solvers/2020-2006.05510-diffexp.pdf"
fetch_if_missing "https://arxiv.org/pdf/2501.01943.pdf" "$root/theory/series-solvers/2025-2501.01943-line.pdf"
fetch_if_missing "https://arxiv.org/pdf/2205.03345.pdf" "$root/theory/analytic-continuation-and-singularities/2022-2205.03345-seasyde.pdf"
fetch_if_missing "https://arxiv.org/pdf/2104.06898.pdf" "$root/theory/analytic-continuation-and-singularities/2021-2104.06898-duals-part1.pdf"
fetch_if_missing "https://arxiv.org/pdf/hep-ph/9711391.pdf" "$root/theory/boundaries-and-regions/1997-hep-ph-9711391-expansion-by-regions.pdf"
fetch_if_missing "https://arxiv.org/pdf/1712.05173.pdf" "$root/theory/precision-and-error-control/2017-1712.05173-dream.pdf"

# Kira / FIRE / LiteRed / Blade
fetch_if_missing "https://arxiv.org/pdf/1705.05610.pdf" "$root/ibp/kira/2017-1705.05610-kira.pdf"
fetch_if_missing "https://arxiv.org/pdf/2008.06494.pdf" "$root/ibp/kira/2020-2008.06494-kira2.pdf"
fetch_if_missing "https://arxiv.org/pdf/2505.20197.pdf" "$root/ibp/kira/2025-2505.20197-kira3.pdf"
fetch_if_missing "https://arxiv.org/pdf/1901.07808.pdf" "$root/ibp/fire/2019-1901.07808-fire6.pdf"
fetch_if_missing "https://arxiv.org/pdf/2311.02370.pdf" "$root/ibp/fire/2023-2311.02370-fire6.5.pdf"
fetch_if_missing "https://arxiv.org/pdf/2304.13418.pdf" "$root/ibp/fire/2023-2304.13418-fuel.pdf"
fetch_if_missing "https://arxiv.org/pdf/1212.2685.pdf" "$root/ibp/litered/2012-1212.2685-litered.pdf"
fetch_if_missing "https://arxiv.org/pdf/1310.1145.pdf" "$root/ibp/litered/2013-1310.1145-litered-1.4.pdf"
fetch_if_missing "https://arxiv.org/pdf/2405.14621.pdf" "$root/ibp/blade/2024-2405.14621-blade.pdf"

# Finite-field layer
fetch_if_missing "https://link.springer.com/content/pdf/10.1007/JHEP12(2016)030.pdf" "$root/finite-fields/foundations/2016-peraro-scattering-amplitudes-over-finite-fields.pdf"
fetch_if_missing "https://arxiv.org/pdf/1905.08019.pdf" "$root/finite-fields/finiteflow/2019-1905.08019-finiteflow.pdf"
fetch_if_missing "https://arxiv.org/pdf/1904.00009.pdf" "$root/finite-fields/firefly/2019-1904.00009-firefly.pdf"
fetch_if_missing "https://arxiv.org/pdf/2004.01463.pdf" "$root/finite-fields/firefly/2020-2004.01463-firefly-improvements.pdf"

# Selected benchmark papers
fetch_if_missing "https://link.springer.com/content/pdf/10.1007/JHEP01(2023)156.pdf" "$root/case-studies/papers/2023-ttj-planar-topology.pdf"
fetch_if_missing "https://link.springer.com/content/pdf/10.1007/JHEP05(2023)131.pdf" "$root/case-studies/papers/2023-single-top-planar-nonplanar-topologies.pdf"
fetch_if_missing "https://link.springer.com/content/pdf/10.1007/JHEP12(2023)105.pdf" "$root/case-studies/papers/2023-diphoton-heavy-quark-form-factors.pdf"
fetch_if_missing "https://cds.cern.ch/record/2864941/files/2306.15431.pdf" "$root/case-studies/papers/2024-five-point-one-mass-scattering.pdf"
fetch_if_missing "https://link.springer.com/content/pdf/10.1007/JHEP07(2024)084.pdf" "$root/case-studies/papers/2024-tth-light-quark-loop-mi.pdf"
fetch_if_missing "https://link.springer.com/content/pdf/10.1007/JHEP03(2024)068.pdf" "$root/case-studies/papers/2024-h-to-bb-nnlo.pdf"
fetch_if_missing "https://link.springer.com/content/pdf/10.1007/JHEP02(2024)201.pdf" "$root/case-studies/papers/2024-n4-sym-three-loop-form-factor.pdf"
fetch_if_missing "https://link.springer.com/content/pdf/10.1007/JHEP07(2025)001.pdf" "$root/case-studies/papers/2025-ttw-leading-colour-integrals.pdf"

# Living documentation snapshots
fetch_if_missing "https://gitlab.com/multiloop-pku/amflow/-/raw/master/README.md" "$root/snapshots/amflow/README.md"
fetch_if_missing "https://gitlab.com/multiloop-pku/amflow/-/raw/master/FAQ.md" "$root/snapshots/amflow/FAQ.md"
fetch_if_missing "https://gitlab.com/multiloop-pku/amflow/-/raw/master/CHANGELOG.md" "$root/snapshots/amflow/CHANGELOG.md"
fetch_if_missing "https://gitlab.com/multiloop-pku/amflow/-/raw/master/options_summary" "$root/snapshots/amflow/options_summary.txt"
fetch_if_missing "https://gitlab.com/multiloop-pku/amflow/-/raw/master/ibp_interface/Kira/interface.m" "$root/snapshots/amflow/kira-interface.m"

fetch_if_missing "https://gitlab.com/kira-pyred/kira/-/raw/master/README.rst" "$root/snapshots/kira/README.rst"
fetch_if_missing "https://gitlab.com/kira-pyred/kira/-/raw/master/ChangeLog.md" "$root/snapshots/kira/ChangeLog.md"
fetch_if_missing "https://gitlab.com/kira-pyred/kira/-/raw/master/job_options_summary" "$root/snapshots/kira/job_options_summary.txt"

fetch_if_missing "https://gitlab.com/feynmanIntegrals/fire/-/raw/master/README.md" "$root/snapshots/fire/README.md"
fetch_if_missing "https://gitlab.com/feynmanIntegrals/fire/-/raw/master/CHANGELOG.md" "$root/snapshots/fire/CHANGELOG.md"
fetch_if_missing "https://gitlab.com/feynmanIntegrals/fire/-/raw/master/FIRE6/documentation/README" "$root/snapshots/fire/documentation-README"

fetch_if_missing "https://www.inp.nsk.su/~lee/programs/LiteRed/" "$root/snapshots/litered/litered-home.html"
fetch_if_missing "https://github.com/peraro/finiteflow/raw/master/README.md" "$root/snapshots/finiteflow/README.md"
fetch_if_missing "https://github.com/peraro/finiteflow-mathtools/raw/master/README.md" "$root/snapshots/finiteflow/finiteflow-mathtools-README.md"
fetch_if_missing "https://gitlab.com/firefly-library/firefly/-/raw/kira-2/README.md" "$root/snapshots/firefly/README.md"

fetch_if_missing "https://www.mpfr.org/" "$root/snapshots/numeric/mpfr-home.html"
fetch_if_missing "https://flintlib.org/" "$root/snapshots/numeric/flint-home.html"
fetch_if_missing "https://original.boost.org/doc/libs/1_89_0/libs/multiprecision/doc/html/index.html" "$root/snapshots/numeric/boost-multiprecision-index.html"
fetch_if_missing "https://docs.rs/rug/latest/rug/" "$root/snapshots/numeric/rug-docs.html"

printf 'Fetched open-access papers and documentation snapshots into %s\n' "$root"
