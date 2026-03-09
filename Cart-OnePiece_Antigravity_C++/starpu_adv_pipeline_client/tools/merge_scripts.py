import os

with open("tools/verify_profiling.sh", "r") as f:
    lines = f.readlines()

start_idx = 0
for i, l in enumerate(lines):
    if "=== Diagnostics ===" in l:
        start_idx = i - 1
        break

profiling_logic = "".join(lines[start_idx:])
profiling_logic = profiling_logic.replace("exit 1", "return 1")

template = """#!/bin/bash
set -e

export CARLA_ROOT=/home/eric/CARLA_Latest

# ==============================================================================
# PROFILING SAFETY CHECK (MUST BE FIRST)
# ==============================================================================
if [ -n "$LD_PRELOAD" ]; then
    if [[ "$LD_PRELOAD" == *"libapex"* ]]; then
        echo "NOTE: APEX detected in LD_PRELOAD. Forcing USE_APEX=1 and cleaning environment."
        export USE_APEX=1
    fi
    unset LD_PRELOAD
fi

# ==============================================================================
# Configuration & Defaults
# ==============================================================================
TRACE_ROOT_DEFAULT="/tmp/starpu_traces_adv"
BUILD_DIR_DEFAULT="./build"

# Arguments with defaults
TRACE_ROOT="$TRACE_ROOT_DEFAULT"
BUILD_DIR="$BUILD_DIR_DEFAULT"
FRAMES=200
ENGINE="models/dummy.engine"
SCHED="dmda"
MEMORY_MODE=0
APEX_MODE="off"
DISPLAY_MODE=0

RUN_DATASET=0
RUN_SWEEP=0
RUN_SPLIT=0
RUN_PROFILING=0

# Helper to parse args
usage() {
    echo "Usage: $0 [options]"
    echo "  --frames N          Number of frames (default: 200)"
    echo "  --engine PATH       Path to TensorRT engine (default: models/dummy.engine)"
    echo "  --deeplabv3         Use models/deeplabv3_mobilenet.engine"
    echo "  --resnet50          Use models/fcn_resnet50.engine"
    echo "  --trace-root PATH   Set trace root directory (default: $TRACE_ROOT_DEFAULT)"
    echo "  --memory            Enable Memory/Bus Stats mode"
    echo "  --apex-mode MODE    Set APEX mode (off, gtrace, gtrace-tasks, taskgraph, all)"
    echo "  --display           Enable Live SDL2 Semantic GUI window"
    echo "  --sched NAME        Scheduler policy (default: dmda)"
    echo "  --run-dataset       Run dataset generation sanity extraction"
    echo "  --run-sweep         Override scheduler and run sweep (dmda, rr_workers, ws, eager)"
    echo "  --run-split         Run multi-process split monitor (run_profiled_single_machine + monitor)"
    echo "  --run-profiling     Run the single-node starpu profiler using the selected --sched"
    echo "  --help              Show this help"
    exit 1
}

# Parse CLI arguments
while [[ "$#" -gt 0 ]]; do
    case "$1" in
        --frames) FRAMES="$2"; shift ;;
        --engine) ENGINE="$2"; shift ;;
        --deeplabv3) ENGINE="models/deeplabv3_mobilenet.engine" ;;
        --resnet50) ENGINE="models/fcn_resnet50.engine" ;;
        --trace-root) TRACE_ROOT="$2"; shift ;;
        --memory) MEMORY_MODE=1 ;;
        --display) DISPLAY_MODE=1 ;;
        --apex-mode)
            APEX_MODE="$2"
            if [ -z "$APEX_MODE" ] || [[ "$APEX_MODE" == --* ]]; then
                APEX_MODE="all"
            else
                shift
            fi
            ;;
        --sched) SCHED="$2"; shift ;;
        --run-dataset) RUN_DATASET=1 ;;
        --run-sweep) RUN_SWEEP=1; RUN_PROFILING=1 ;;
        --run-split) RUN_SPLIT=1 ;;
        --run-profiling) RUN_PROFILING=1 ;;
        --help) usage ;;
        *) echo "Unknown parameter passed: $1"; usage ;;
    esac
    shift
done

# ==============================================================================
# SERVER LIFECYCLE
# ==============================================================================
ENGINE_NAME=$(basename "$ENGINE" .engine)
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
export RUN_DIR="$(pwd)/runs/${TIMESTAMP}_${ENGINE_NAME}"
mkdir -p "$RUN_DIR"

SERVER_STARTED=0

cleanup() {
    if [ "$SERVER_STARTED" -eq 1 ]; then
        echo "--- KILLING SERVER AND CLEANING UP ---"
        ./tools/kill_server.sh --run_dir "$RUN_DIR" || true
    fi
}
trap cleanup EXIT

echo "--- STARTING SERVER ---"
pkill -9 -f CarlaUE4 || true
pkill -9 -f pipeline_starpu || true
./tools/run_server.sh --port 2000 --run_dir "$RUN_DIR"
SERVER_STARTED=1

echo "Waiting 15 seconds for CARLA to initialize Map..."
sleep 15

# ==============================================================================
# DATASET GENERATION
# ==============================================================================
if [ "$RUN_DATASET" -eq 1 ]; then
    echo "--- RUNNING DATASET EXTRACTION ---"
    DATASET_FRAMES=$FRAMES
    ./build/dataset_sanity --host 127.0.0.1 --port 2000 --w 800 --h 600 --fps 10 --frames $DATASET_FRAMES --engine "$ENGINE" --out_w 512 --out_h 256 --assume_bgra 1 --display "$DISPLAY_MODE" || echo "Extraction Failed!"
fi

# ==============================================================================
# MULTI-PROCESS SPLIT EXECUTION
# ==============================================================================
if [ "$RUN_SPLIT" -eq 1 ]; then
    echo "--- RUNNING PROFILED SPLIT PIPELINE ---"
    PRINT_EVERY=$((FRAMES / 5))
    if [ "$PRINT_EVERY" -lt 1 ]; then PRINT_EVERY=1; fi

    PIPELINE_ARGS="--host 127.0.0.1 --port 2000 --w 800 --h 600 --fps 20 --frames $FRAMES --engine $ENGINE --out_w 512 --out_h 256 --inflight 2 --cpu_workers 8 --print_every $PRINT_EVERY --eval_every $FRAMES --display $DISPLAY_MODE"
    if [ "$MEMORY_MODE" -eq 1 ]; then
        PIPELINE_ARGS="$PIPELINE_ARGS --print_stats"
    fi

    SERVER_CORES="0-9" CLIENT_CORES="10-19" ./tools/run_profiled_single_machine.sh --port 2000 --run_dir "$RUN_DIR" --client ./build/pipeline_starpu --client_args "$PIPELINE_ARGS" &
    CLIENT_PID=$!
    sleep 5
    ./tools/monitor_split.sh --duration 20 --run_dir "$RUN_DIR"
    wait $CLIENT_PID

    echo "--- SUMMARIZING SPLIT RESULTS ---"
    python3 ./tools/summarize_run.py --run_dir "$RUN_DIR"
fi

# ==============================================================================
# STARPU PROFILING (verify_profiling.sh logic wrapped in a function)
# ==============================================================================
run_profiling_internal() {
    local SCHED="$1"
    echo "====================================================================="
    echo "--- RUNNING PROFILING HARNESS (SCHEDULER: $SCHED) ---"
    echo "====================================================================="

"""

template_end = """
}

if [ "$RUN_PROFILING" -eq 1 ]; then
    if [ "$RUN_SWEEP" -eq 1 ]; then
        echo "--- RUNNING PROFILING HARNESS (SWEEP) ---"
        run_profiling_internal "dmda"
        run_profiling_internal "rr_workers"
        run_profiling_internal "ws"
        run_profiling_internal "eager"
    else
        run_profiling_internal "$SCHED"
    fi
fi

echo "Validation Runs Completed."
"""

full_content = template + profiling_logic + template_end

with open("tools/run_pipeline.sh", "w") as f:
    f.write(full_content)

os.chmod("tools/run_pipeline.sh", 0o755)
print("Merge complete: tools/run_pipeline.sh generated successfully.")
