#!/bin/bash
#SBATCH --job-name=lane113-five-point
#SBATCH --partition=sapphire,shared
#SBATCH --time=01:00:00
#SBATCH --mem=16G
#SBATCH --cpus-per-task=4
#SBATCH --output=/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/lane113-five-point-one-mass-scattering/logs/lane113-five-point.%j.out
#SBATCH --error=/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/lane113-five-point-one-mass-scattering/logs/lane113-five-point.%j.err

set -eo pipefail

RUN_DIR=/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/lane113-five-point-one-mass-scattering
AMFLOW_DIR="${RUN_DIR}/amflow"
EXAMPLE_DIR="${AMFLOW_DIR}/examples/five_point_one_mass_complex_proxy"
REPO_DIR=/n/holylabs/schwartz_lab/Lab/obarrera/autoIBP
PACKET_DIR="${REPO_DIR}/tools/reference-harness/specs/phase1/five-point-one-mass-scattering.lane113"
SOL_FILE="${EXAMPLE_DIR}/sol-five-point-one-mass-complex-box-proxy"
MANIFEST_FILE="${RUN_DIR}/run-manifest.${SLURM_JOB_ID:-lane113local}.txt"
AMFLOW_SOURCE=/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/lane93-box-integrals-fermion-bubbles/amflow
AMFLOW_COMMIT=775162498ab18493c45254b861669b4151b841ee

echo "host: $(hostname)"
echo "date: $(date -Is)"
echo "jobid: ${SLURM_JOB_ID:-lane113local}"
echo "run_dir: ${RUN_DIR}"
echo "example_dir: ${EXAMPLE_DIR}"

source /etc/profile
module purge
module load mathematica/13.3.0-fasrc01

set -a
. /n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/work/generated-config/reference.env
set +a

set -u
echo "WOLFRAM_KERNEL=${WOLFRAM_KERNEL}"
echo "KIRA_EXECUTABLE=${KIRA_EXECUTABLE}"
echo "FERMAT_EXECUTABLE=${FERMAT_EXECUTABLE}"
echo "FERMATPATH=${FERMATPATH}"

mkdir -p "${RUN_DIR}/logs"
if [ ! -d "${AMFLOW_DIR}/.git" ]; then
  if [ -d "${AMFLOW_SOURCE}/.git" ]; then
    git clone "${AMFLOW_SOURCE}" "${AMFLOW_DIR}"
  else
    git clone https://gitlab.com/multiloop-pku/amflow.git "${AMFLOW_DIR}"
  fi
fi
git -C "${AMFLOW_DIR}" checkout --detach "${AMFLOW_COMMIT}"
mkdir -p "${EXAMPLE_DIR}"

cp "${PACKET_DIR}/run.wl" "${PACKET_DIR}/verify_output.wl" "${EXAMPLE_DIR}/"

{
  echo "jobid=${SLURM_JOB_ID:-lane113local}"
  echo "start=$(date -Is)"
  echo "run_dir=${RUN_DIR}"
  echo "example_dir=${EXAMPLE_DIR}"
  echo "wolfram_kernel=${WOLFRAM_KERNEL}"
  echo "kira=${KIRA_EXECUTABLE}"
  echo "fermat=${FERMAT_EXECUTABLE}"
  echo "amflow_commit=$(git -C "${AMFLOW_DIR}" rev-parse HEAD)"
  echo "amflow_remote=$(git -C "${AMFLOW_DIR}" remote get-url origin)"
  sha256sum "${EXAMPLE_DIR}/run.wl" "${EXAMPLE_DIR}/verify_output.wl" "${AMFLOW_DIR}/AMFlow.m" "${AMFLOW_DIR}/ibp_interface/Kira/interface.m"
} > "${MANIFEST_FILE}"

cd "${EXAMPLE_DIR}"

rm -f "${SOL_FILE}" "${SOL_FILE}.meta.wl" "${SOL_FILE}.verify.txt"
rm -rf cache

echo "run.wl target lines:"
grep -nE "precisionGoal|epsorder|fivePointOneMassProxyBox|complex_kinematics|GenerateNumericalConfig|SolveIntegrals|Lane95KiraImaginaryRetry" run.wl

echo "kernel-start: $(date -Is)"
"${WOLFRAM_KERNEL}" -script run.wl
RC=$?
echo "kernel-end: $(date -Is)"
echo "kernel-exit: ${RC}"

if [ -f "${SOL_FILE}" ]; then
  echo "outputs ok:"
  ls -la "${SOL_FILE}" "${SOL_FILE}.meta.wl"
  sha256sum "${SOL_FILE}" "${SOL_FILE}.meta.wl" | tee -a "${MANIFEST_FILE}"
  "${WOLFRAM_KERNEL}" -script verify_output.wl | tee "${SOL_FILE}.verify.txt"
  sha256sum "${SOL_FILE}.verify.txt" | tee -a "${MANIFEST_FILE}"
else
  echo "ERROR: expected output not produced" >&2
  ls -la
  exit 2
fi

exit "${RC}"
