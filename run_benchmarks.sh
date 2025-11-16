#!/bin/bash
# Usage: ./run_check_refinements.sh /path/to/folder [simulation|language_inclusion|ctl_sat] [serial|parallel]

#nohup ./run_benchmarks.sh ./assets/benchmark/Dataset/Properties language_inclusion parallel > all_logs/lang_parallel_transitive.log 2>&1 &


#nohup ./run_benchmarks.sh ./assets/benchmark/Dataset/Properties language_inclusion serial > all_logs/lang_serial_transitive.log 2>&1 &

#nohup ./ctl_refine_tool ../.././assets/benchmark/Dataset/Properties/2021 --use-full-language-inclusion --verbose -o 2021 --no-parallel > logs.log 2>&1 &

#./check_refinements ./assets/benchmark/Dataset/Properties/2021 --use-full-language-inclusion --verbose -o results/language_inclusion_RCTLC_s/2021 --no-parallel > logs/2021_language_inclusion_s.log 2>&1 &

#CTLAnalysisTool/assets/benchmark/Dataset/Properties

# Default values
EXTRA_ARGS=""
SUFFIX=""
SAT_PATH=""
DEFAULT_SAT_PATH="./assets/extern/ctl-sat"
ANALYSIS_MODE="aut_based_naive"
PROGRAM_NAME="sat"
RUNNING_MODE="serial"
NUM_JOBS=6

# Parse arguments
FOLDER="$1"
shift

if [[ -z "$FOLDER" ]]; then
    echo "Usage: $0 <folder> [aut_based_naive|ctl_sat] [--sat-path <path>] [sat|refinement] [serial|parallel] [-j <num>]"
    exit 1
fi

# Parse remaining optional arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        aut_based_naive|ctl_sat)
            ANALYSIS_MODE="$1"
            shift
            ;;
        --sat-path)
            SAT_PATH="$2"
            shift 2
            ;;
        sat|refinement)
            PROGRAM_NAME="$1"
            shift
            ;;
        serial|parallel)
            RUNNING_MODE="$1"
            shift
            ;;
        -j)
            NUM_JOBS="$2"
            shift 2
            ;;
        *)
            echo "Unknown argument: $1"
            echo "Usage: $0 <folder> [aut_based_naive|ctl_sat] [--sat-path <path>] [sat|refinement] [serial|parallel] [-j <num>]"
            exit 1
            ;;
    esac
done

# Validate analysis mode
if [[ "$ANALYSIS_MODE" != "aut_based_naive" && "$ANALYSIS_MODE" != "ctl_sat" ]]; then
    echo "Invalid analysis mode. Choose 'aut_based_naive' or 'ctl_sat'."
    exit 1
fi

# Set analysis mode specific arguments
if [[ "$ANALYSIS_MODE" == "ctl_sat" ]]; then
    if [[ -z "$SAT_PATH" ]]; then
        echo "WARNING: --sat-path not specified for ctl_sat mode. Using default: $DEFAULT_SAT_PATH"
        SAT_PATH="$DEFAULT_SAT_PATH"
    fi
    EXTRA_ARGS+=" --use-extern-sat CTLSAT --sat-at $SAT_PATH"
fi

# Set program to run
if [[ "$PROGRAM_NAME" == "sat" ]]; then
    PROGRAM_TO_RUN="./sat_checker"
elif [[ "$PROGRAM_NAME" == "refinement" ]]; then
    PROGRAM_TO_RUN="./check_refinements"
else
    echo "Invalid program name. Choose 'sat' or 'refinement'."
    exit 1
fi

# Determine max concurrent jobs and set running mode specific arguments
if [[ "$RUNNING_MODE" == "parallel" ]]; then
    MAX_JOBS=1
    EXTRA_ARGS+=" --parallel -j $NUM_JOBS"
    SUFFIX+="_p"
elif [[ "$RUNNING_MODE" == "serial" ]]; then
    MAX_JOBS=1
    SUFFIX+="_s"
else
    echo "Invalid running mode. Choose 'serial' or 'parallel'."
    exit 1
fi


PARENT_OUTPUT_FOLDER="results/"
mkdir -p "$PARENT_OUTPUT_FOLDER"


OUTPUT_FOLDER="${PARENT_OUTPUT_FOLDER}${ANALYSIS_MODE}"
mkdir -p logs

# Get all subfolders
shopt -s nullglob
SUBFOLDERS=("$FOLDER"/*/)



mkdir -p "$OUTPUT_FOLDER$SUFFIX"
RESULTS_PREFIX="$OUTPUT_FOLDER$SUFFIX"

# Function to run a single job
run_job() {
    local subfolder="$1"
    local sub
    sub=$(basename "$subfolder")
    local log_file="logs/${sub}_${ANALYSIS_MODE}${SUFFIX}.log"

    echo "Starting analysis for $subfolder..."

    #echo "nohup ./ctl_refine_tool \"$subfolder\" -o \"$RESULTS_PREFIX/$year\" --verbose $EXTRA_ARGS > \"$log_file\" 2>&1 &"
    #check_refinements 
    #nohup ./check_refinements "$subfolder" -o "$RESULTS_PREFIX/$sub" --verbose $EXTRA_ARGS > "$log_file" 2>&1 &
    nohup $PROGRAM_TO_RUN "$subfolder" -o "$RESULTS_PREFIX/$sub" --verbose $EXTRA_ARGS > "$log_file" 2>&1 &
}

# Main loop
job_count=0
echo "Starting Refinement in $ANALYSIS_MODE in $RUNNING_MODE mode..."
echo "-------------------------------------"

for subfolder in "${SUBFOLDERS[@]}"; do
    [[ -d "$subfolder" ]] || continue

    run_job "$subfolder"
    ((job_count++))

    if (( job_count >= MAX_JOBS )); then
        wait
        job_count=0
    fi
done

wait
echo "All jobs completed."




