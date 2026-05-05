#!/bin/bash
#SBATCH --job-name=lane111-ttj-planar
#SBATCH --partition=sapphire,shared
#SBATCH --time=02:00:00
#SBATCH --mem=64G
#SBATCH --cpus-per-task=8
#SBATCH --output=/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/lane111-2023-ttj-planar-topology/logs/lane111-ttj-planar.%j.out
#SBATCH --error=/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/lane111-2023-ttj-planar-topology/logs/lane111-ttj-planar.%j.err

set -eo pipefail

RUN_DIR=/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/lane111-2023-ttj-planar-topology
SOURCE_AMFLOW=/n/holylabs/schwartz_lab/Lab/obarrera/amflow-verification/lane46-ttbar-j/amflow
EXAMPLE_DIR="${RUN_DIR}/amflow/examples/ttj_planar_topology_tt_family"
REPO_DIR=/n/holylabs/schwartz_lab/Lab/obarrera/autoIBP
PACKET_DIR="${REPO_DIR}/tools/reference-harness/specs/phase1/2023-ttj-planar-topology.lane111"
SOL_FILE="${EXAMPLE_DIR}/sol-2023-ttj-planar-topology-tt-family"
MANIFEST_FILE="${RUN_DIR}/run-manifest.${SLURM_JOB_ID:-local}.txt"

echo "host: $(hostname)"
echo "date: $(date -Is)"
echo "jobid: ${SLURM_JOB_ID:-local}"
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
if [ ! -d "${RUN_DIR}/amflow" ]; then
  cp -a "${SOURCE_AMFLOW}" "${RUN_DIR}/amflow"
fi
mkdir -p "${EXAMPLE_DIR}"
cp "${PACKET_DIR}/run.wl" "${PACKET_DIR}/verify_output.wl" "${EXAMPLE_DIR}/"

{
  echo "jobid=${SLURM_JOB_ID:-local}"
  echo "start=$(date -Is)"
  echo "run_dir=${RUN_DIR}"
  echo "example_dir=${EXAMPLE_DIR}"
  echo "source_amflow=${SOURCE_AMFLOW}"
  echo "wolfram_kernel=${WOLFRAM_KERNEL}"
  echo "kira=${KIRA_EXECUTABLE}"
  echo "fermat=${FERMAT_EXECUTABLE}"
  sha256sum "${EXAMPLE_DIR}/run.wl" "${EXAMPLE_DIR}/verify_output.wl"
} > "${MANIFEST_FILE}"

cd "${EXAMPLE_DIR}"

rm -f "${SOL_FILE}" "${SOL_FILE}.meta.wl" "${SOL_FILE}.verify.txt"

echo "run.wl target lines:"
grep -nE "precisionGoal|epsorder|2023-ttj-planar-topology|GenerateNumericalConfig|Lane95KiraImaginaryRetry" run.wl

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
