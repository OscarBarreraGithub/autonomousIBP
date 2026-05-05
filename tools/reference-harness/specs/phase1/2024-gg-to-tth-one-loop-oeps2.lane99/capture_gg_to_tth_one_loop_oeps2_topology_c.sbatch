#!/bin/bash
#SBATCH --job-name=lane99-tth1lC
#SBATCH --partition=sapphire,shared
#SBATCH --time=01:00:00
#SBATCH --mem=16G
#SBATCH --cpus-per-task=4
#SBATCH --output=/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/lane99-gg-to-tth-one-loop-oeps2/logs/lane99-tth1lC.%j.out
#SBATCH --error=/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/lane99-gg-to-tth-one-loop-oeps2/logs/lane99-tth1lC.%j.err

set -eo pipefail

RUN_DIR=/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/lane99-gg-to-tth-one-loop-oeps2
TTH_DIR="${RUN_DIR}/TTH"
EXAMPLE_DIR="${TTH_DIR}/one_loop_amflow"
REPO_DIR=/n/holylabs/schwartz_lab/Lab/obarrera/autoIBP
PACKET_DIR="${REPO_DIR}/tools/reference-harness/specs/phase1/2024-gg-to-tth-one-loop-oeps2.lane99"
SOL_FILE="${EXAMPLE_DIR}/sol-2024-gg-to-tth-one-loop-oeps2-topology-c-euclidean"
MANIFEST_FILE="${RUN_DIR}/run-manifest.${SLURM_JOB_ID:-nojob}.txt"

echo "host: $(hostname)"
echo "date: $(date -Is)"
echo "jobid: ${SLURM_JOB_ID:-nojob}"
echo "run_dir: ${RUN_DIR}"
echo "example_dir: ${EXAMPLE_DIR}"

source /etc/profile
module purge
module load mathematica/13.3.0-fasrc01

WOLFRAM_KERNEL="${WOLFRAM_KERNEL:-/n/sw/helmod/apps/centos7/Core/mathematica/Mathematica_13.3.0/Executables/WolframKernel}"
mkdir -p "${RUN_DIR}/logs"

if [ ! -d "${TTH_DIR}/.git" ]; then
  git clone --depth 1 https://github.com/p-a-kreer/TTH.git "${TTH_DIR}"
fi

cp "${PACKET_DIR}/run.wl" "${PACKET_DIR}/verify_output.wl" "${EXAMPLE_DIR}/"

{
  echo "jobid=${SLURM_JOB_ID:-nojob}"
  echo "start=$(date -Is)"
  echo "run_dir=${RUN_DIR}"
  echo "example_dir=${EXAMPLE_DIR}"
  echo "wolfram_kernel=${WOLFRAM_KERNEL}"
  echo "tth_commit=$(git -C "${TTH_DIR}" rev-parse HEAD)"
  sha256sum "${EXAMPLE_DIR}/run.wl" "${EXAMPLE_DIR}/verify_output.wl" "${EXAMPLE_DIR}/package/DESolver.m" "${EXAMPLE_DIR}/package/OneLoopAMFlow.m"
} > "${MANIFEST_FILE}"

cd "${EXAMPLE_DIR}"

rm -f "${SOL_FILE}" "${SOL_FILE}.meta.wl" "${SOL_FILE}.verify.txt"

echo "run.wl target lines:"
grep -nE "precisionGoal|epsorder|PentaboxGGtoTTHTopologyC|arXiv:2312.10015|GenerateNumericalConfig|EvaluateMasters" run.wl

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
