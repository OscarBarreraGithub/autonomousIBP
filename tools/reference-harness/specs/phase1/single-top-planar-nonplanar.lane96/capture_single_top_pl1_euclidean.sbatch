#!/bin/bash
#SBATCH --job-name=lane96-st-pl1
#SBATCH --partition=sapphire,shared
#SBATCH --time=04:00:00
#SBATCH --mem=32G
#SBATCH --cpus-per-task=8
#SBATCH --output=/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/lane96-single-top-planar-nonplanar/logs/lane96-st-pl1.%j.out
#SBATCH --error=/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/lane96-single-top-planar-nonplanar/logs/lane96-st-pl1.%j.err

set -eo pipefail

RUN_DIR=/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/lane96-single-top-planar-nonplanar
EXAMPLE_DIR="${RUN_DIR}/amflow/examples/single_top_planar_nonplanar_pl1_euclidean"
SOL_FILE="${EXAMPLE_DIR}/sol-single-top-pl1-euclidean"
MANIFEST_FILE="${RUN_DIR}/run-manifest.${SLURM_JOB_ID:-nojob}.txt"

echo "host: $(hostname)"
echo "date: $(date -Is)"
echo "jobid: ${SLURM_JOB_ID}"
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

{
  echo "jobid=${SLURM_JOB_ID}"
  echo "start=$(date -Is)"
  echo "run_dir=${RUN_DIR}"
  echo "example_dir=${EXAMPLE_DIR}"
  echo "wolfram_kernel=${WOLFRAM_KERNEL}"
  echo "kira=${KIRA_EXECUTABLE}"
  echo "fermat=${FERMAT_EXECUTABLE}"
  sha256sum "${EXAMPLE_DIR}/run.wl" "${EXAMPLE_DIR}/verify_output.wl"
} > "${MANIFEST_FILE}"

cd "${EXAMPLE_DIR}"

rm -f "${SOL_FILE}" "${SOL_FILE}.meta.wl" "${SOL_FILE}.verify.txt"

echo "run.wl target lines:"
grep -nE "precisionGoal|epsorder|singleTopPL1|JHEP05\\(2023\\)131|GenerateNumericalConfig|Euclidean" run.wl

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
else
  echo "ERROR: expected output not produced" >&2
  ls -la
  exit 2
fi

exit "${RC}"
