#!/bin/bash
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

# ==============================================================================
echo "=== Diagnostics ==="
echo "APEX Mode: $APEX_MODE"
echo "Checking StarPU Config..."
starpu_config | egrep -i "papi|profil" || true
ldconfig -p | grep -i papi || true

if command -v papi_avail &> /dev/null; then
    echo "PAPI Available. First 30 lines:"
    papi_avail | head -n 30
else
    echo "WARNING: papi_avail not found."
fi

echo "Checking Kernel Perf Permissions..."
PARANOID=$(cat /proc/sys/kernel/perf_event_paranoid 2>/dev/null || echo "UNKNOWN")
echo "perf_event_paranoid: $PARANOID"
if [ "$PARANOID" != "UNKNOWN" ] && [ "$PARANOID" -gt 2 ]; then
    echo "----------------------------------------------------------------"
    echo "WARNING: perf_event_paranoid is set to $PARANOID (Restricted)."
    echo "PAPI/Perf may fail to record events."
    echo "To fix temporarily, run:"
    echo "  sudo sysctl -w kernel.perf_event_paranoid=1"
    echo "----------------------------------------------------------------"
fi

# ==============================================================================
# STEP 0: Create Run Folder with Unique RUN_ID
# ==============================================================================
ENGINE_NAME=$(basename "$ENGINE" .engine)
RUN_ID="$(date +%Y%m%d_%H%M%S)_${ENGINE_NAME}_${SCHED}"
BASE="${TRACE_ROOT}/${RUN_ID}"
echo "=== STEP 0: Initialization ==="
echo "Run ID: $RUN_ID"
echo "Base Dir: $BASE"

# Define folder layout
DIR_RAW="${BASE}/00_raw"
DIR_FXT="${BASE}/01_starpu_fxt_tool"
DIR_DOT="${BASE}/02_graphviz"
DIR_TASKS="${BASE}/03_tasks_complete"
DIR_PROF="${BASE}/04_codelet_profile"
DIR_HISTO="${BASE}/05_histo"
DIR_DATA="${BASE}/06_data_trace"
DIR_STATS="${BASE}/07_state_stats"
DIR_STARVZ="${BASE}/08_starvz"
DIR_PAPI="${BASE}/09_papi"
DIR_APEX="${BASE}/10_apex"
DIR_MEM="${BASE}/11_memory"
DIR_ENERGY="${BASE}/12_energy"
DIR_SUM="${BASE}/summary"

mkdir -p "$DIR_RAW" "$DIR_FXT" "$DIR_DOT" "$DIR_TASKS" "$DIR_PROF" \
         "$DIR_HISTO" "$DIR_DATA" "$DIR_STATS" "$DIR_STARVZ" \
         "$DIR_PAPI" "$DIR_APEX" "$DIR_MEM" "$DIR_ENERGY" "$DIR_SUM"

# ==============================================================================
# DIAGNOSTICS: TaskStubs (If Requested)
# ==============================================================================
if [ "${USE_TASKSTUBS:-0}" -eq 1 ]; then
    echo "Checking TaskStubs Configuration..."
    if pkg-config --exists taskstubs; then
        echo "TaskStubs Found: $(pkg-config --modversion taskstubs)"
    else
        echo "ERROR: TaskStubs not found via pkg-config."
        echo "Please install TaskStubs and set PKG_CONFIG_PATH."
        return 1
    fi
fi

# ==============================================================================
# STEP 1: Run Binary -> 00_raw (Immutable)
# ==============================================================================
echo "=== STEP 1: Run Application ==="

# Set StarPU Environment for Raw Output
export STARPU_FXT_PREFIX="${DIR_RAW}"
export STARPU_FXT_TRACE=1
export STARPU_PROFILING=1

# Custom Scheduler Integration (rr_workers)
if [ "$SCHED" = "rr_workers" ]; then
    echo "=== Custom Scheduler Config: rr_workers ==="
    export STARPU_NCPU=8
    export STARPU_NCUDA=1
    
    PLUGIN_DIR="$(pwd)/custom_sched_rr_workers"
    PLUGIN_BUILD="$PLUGIN_DIR/build"
    PLUGIN_LIB="$PLUGIN_BUILD/libstarpu_sched_rr_workers.so"
    
    if [ ! -f "$PLUGIN_LIB" ]; then
        echo "Building custom rr_workers scheduler..."
        mkdir -p "$PLUGIN_BUILD"
        cmake -S "$PLUGIN_DIR" -B "$PLUGIN_BUILD"
        cmake --build "$PLUGIN_BUILD" -j
    fi
    
    if [ -f "$PLUGIN_LIB" ]; then
        export STARPU_SCHED_LIB="$PLUGIN_LIB"
    else
        echo "ERROR: Failed to build custom scheduler plugin $PLUGIN_LIB"
        return 1
    fi
fi
export STARPU_SCHED="$SCHED"

# Memory/Stats Mode Configuration
if [ "$MEMORY_MODE" -eq 1 ]; then
    echo "Enabling Memory Activity Stats..."
    export STARPU_BUS_STATS=1
    export STARPU_WORKER_STATS=1
    export STARPU_MEMORY_STATS=1
    export STARPU_ENABLE_STATS=1
    
    # Dump Memory Env
    env | grep "STARPU_" > "$DIR_MEM/memory_env.txt"
fi

# PAPI Configuration
export STARPU_PAPI_EVENTS="PAPI_TOT_CYC,PAPI_TOT_INS"
export STARPU_PROF_PAPI_EVENTS="PAPI_TOT_CYC,PAPI_TOT_INS" # Fallback
# export STARPU_PROFILING_NB_TASKS=0 # Optional, unlimited history

# CRITICAL: Disable auto-post processing
unset STARPU_GENERATE_TRACE
unset STARPU_GENERATE_TRACE_OPTIONS

# Determine Binary Path (Check root vs subdir)
if [ -f "$BUILD_DIR/pipeline_starpu" ]; then
    BIN_PATH="$(readlink -f $BUILD_DIR/pipeline_starpu)"
elif [ -f "./build/pipeline_starpu" ]; then
    BIN_PATH="$(readlink -f ./build/pipeline_starpu)"
else
    echo "ERROR: Could not locate pipeline_starpu binary."
    return 1
fi

# Construct Arguments
ARGS="--host 127.0.0.1 --port 2000 --w 800 --h 600 --fps 20 --frames $FRAMES --engine $ENGINE --out_w 512 --out_h 256 --inflight 2 --cpu_workers 8 --print_every 20 --eval_every $FRAMES --display $DISPLAY_MODE"
if [ "$MEMORY_MODE" -eq 1 ]; then
    ARGS="$ARGS --print_stats"
fi

# APEX / TaskStubs Handling
RUN_CMD="$BIN_PATH $ARGS"

# Clear LD_PRELOAD if it's infected (safe check)
if [ -n "$LD_PRELOAD" ] && [[ "$LD_PRELOAD" == *"libapex"* ]]; then
    unset LD_PRELOAD
fi

USE_APEX=${USE_APEX:-0}
USE_TASKSTUBS=${USE_TASKSTUBS:-0}

if [ "$USE_TASKSTUBS" -eq 1 ]; then
    echo "Using TaskStubs Integration (No Wrapper)..."
    export APEX_OTF2=${APEX_OTF2:-1}
    if ! ldd "$BIN_PATH" | grep -q "timer_plugin"; then
         echo "WARNING: Binary does not appear to link libtimer_plugin. Rebuild with -DENABLE_TASKSTUBS_APEX=ON?"
    fi
elif [ "$USE_APEX" -eq 1 ]; then
    if command -v apex_exec &> /dev/null; then
        echo "Using apex_exec wrapper..."
        export APEX_OTF2=${APEX_OTF2:-1} 
        RUN_CMD="apex_exec $RUN_CMD"
    else
        echo "WARNING: USE_APEX=1 but apex_exec not found."
    fi
fi

# ------------------------------------------------------------------------------
# ENERGY CAPTURE: START
# ------------------------------------------------------------------------------
echo "Starting Energy Capture..."
LOG_ENERGY="$DIR_ENERGY/energy.log"

PID_GPU_SAMPLE=""
if command -v nvidia-smi &> /dev/null; then
    echo "Starting GPU Power Sampling (10Hz)..."
    (while true; do nvidia-smi --query-gpu=timestamp,power.draw --format=csv,noheader,nounits; sleep 0.1; done) > "$DIR_ENERGY/gpu_power_samples.csv" 2>>"$LOG_ENERGY" &
    PID_GPU_SAMPLE=$!
else
    echo "WARN: nvidia-smi not found. Skipping GPU energy." >> "$LOG_ENERGY"
fi

get_rapl_snapshot() {
    local USE_SUDO="${USE_SUDO_RAPL:-0}"
    local FILES=$(find /sys/class/powercap/intel-rapl* -name "energy_uj" 2>/dev/null | sort)
    
    if [ -z "$FILES" ]; then
        if [ -e "/sys/class/powercap/intel-rapl:0/energy_uj" ] || [ "$USE_SUDO" -eq 1 ]; then
             FILES="/sys/class/powercap/intel-rapl:0/energy_uj"
        fi
    fi
    if [ -e "/sys/class/powercap/intel-rapl:0:0/energy_uj" ]; then
         FILES="$FILES /sys/class/powercap/intel-rapl:0:0/energy_uj"
    fi

    if [ -z "$FILES" ]; then
        return
    fi
    
    for f in $FILES; do
        local VAL=""
        if [ -r "$f" ]; then
            VAL=$(cat "$f" 2>/dev/null)
        fi
        if [ -z "$VAL" ]; then
             if [ "$USE_SUDO" -eq 1 ] || sudo -n true 2>/dev/null; then
                 VAL=$(sudo -n cat "$f" 2>/dev/null)
             fi
        fi
        
        if [[ "$VAL" =~ ^[0-9]+$ ]]; then
             echo "$f:$VAL"
        else
             echo "$f:PERMISSION_DENIED"
        fi
    done
}

echo "Snapshotting CPU RAPL (Before)..."
get_rapl_snapshot > "$DIR_ENERGY/cpu_rapl_before_uj.txt"

CPU_SAMPLE_RAW="$DIR_ENERGY/cpu_rapl_samples_raw.csv"
echo "timestamp,domain,energy_uj" > "$CPU_SAMPLE_RAW"
(
    while true; do
        TS=$(date +"%Y-%m-%dT%H:%M:%S%z")
        get_rapl_snapshot | while read LINE; do
            P=$(echo "$LINE" | cut -d: -f1)
            V=$(echo "$LINE" | cut -d: -f2)
            if [[ "$P" == *":0:0"* ]]; then DOM="dram"; else DOM="package"; fi
            if [[ "$V" =~ ^[0-9]+$ ]]; then
                echo "$TS,$DOM,$V"
            fi
        done
        sleep 0.1
    done
) >> "$CPU_SAMPLE_RAW" &
PID_CPU_SAMPLE=$!


# ==============================================================================
# EXECUTION LOGIC (Standard or Loop for APEX Modes)
# ==============================================================================

APEX_MODES_TO_RUN=()
case "$APEX_MODE" in
    off) ;;
    gtrace) APEX_MODES_TO_RUN+=("gtrace") ;;
    gtrace-tasks) APEX_MODES_TO_RUN+=("gtrace-tasks") ;;
    taskgraph) APEX_MODES_TO_RUN+=("taskgraph") ;;
    all) APEX_MODES_TO_RUN+=("gtrace" "gtrace-tasks" "taskgraph") ;;
    *) echo "ERROR: Unknown apex-mode: $APEX_MODE"; return 1 ;;
esac

echo "=== STEP 1: Run Application (Primary) ==="
echo "Executing: $RUN_CMD"
echo "Log: $DIR_SUM/run.log"

env > "$DIR_SUM/env.txt"
echo "$RUN_CMD" > "$DIR_SUM/cmd.txt"

TS_START=$(date +%s.%N)
eval $RUN_CMD > "$DIR_SUM/run.log" 2>&1 || true
TS_END=$(date +%s.%N)
DURATION=$(python3 -c "print($TS_END - $TS_START)")

if [ -n "$PID_GPU_SAMPLE" ]; then
    kill "$PID_GPU_SAMPLE" 2>/dev/null || true
    wait "$PID_GPU_SAMPLE" 2>/dev/null || true
fi
if [ -n "$PID_CPU_SAMPLE" ]; then
    kill "$PID_CPU_SAMPLE" 2>/dev/null || true
    wait "$PID_CPU_SAMPLE" 2>/dev/null || true
fi

echo "Snapshotting CPU RAPL (After)..."
get_rapl_snapshot > "$DIR_ENERGY/cpu_rapl_after_uj.txt"

if [ "$USE_TASKSTUBS" -eq 1 ] || [ "$USE_APEX" -eq 1 ]; then
    echo "Capturing Primary Run APEX output to $DIR_APEX"
    mv apex.* *.otf2 *.json "$DIR_APEX/" 2>/dev/null || true
    if [ "$USE_APEX" -eq 1 ]; then
         sed -n '/APEX Version:/,$p' "$DIR_SUM/run.log" > "$DIR_APEX/apex.log"
    fi
fi

if [ ${#APEX_MODES_TO_RUN[@]} -gt 0 ]; then
    echo "=== APEX Verification Modes: ${APEX_MODES_TO_RUN[*]} ==="
    unset STARPU_FXT_TRACE
    unset STARPU_PROFILING
    
    for MODE in "${APEX_MODES_TO_RUN[@]}"; do
        SUB_DIR="$DIR_APEX/run_$MODE"
        mkdir -p "$SUB_DIR"
        echo ">> Running APEX Mode: $MODE (in $SUB_DIR)"
        
        # Replace --display 1 with --display 0 so APEX executions are completely silent to the user
        SILENT_ARGS="${ARGS/--display 1/--display 0}"
        APEX_CMD="apex_exec --apex:$MODE $BIN_PATH $SILENT_ARGS"
        
        (
            set +e
            export APEX_OUTPUT_FILE_PATH="$SUB_DIR"
            echo "Command: $APEX_CMD" > "$SUB_DIR/apex_runinfo.txt"
            echo "Start: $(date)" >> "$SUB_DIR/apex_runinfo.txt"
            eval "$APEX_CMD" > "$SUB_DIR/apex.log" 2>&1
            RET=$?
            echo "End: $(date)" >> "$SUB_DIR/apex_runinfo.txt"
            echo "Return Code: $RET" >> "$SUB_DIR/apex_runinfo.txt"
            echo "Files:" >> "$SUB_DIR/apex_runinfo.txt"
            ls -1 "$SUB_DIR" >> "$SUB_DIR/apex_runinfo.txt"
        )
    done
    export STARPU_FXT_TRACE=1
    export STARPU_PROFILING=1
fi

echo "Post-Processing APEX Artifacts in $DIR_APEX..."
rm -f "$DIR_APEX/apex_index.txt"
touch "$DIR_APEX/apex_index.txt"

process_apex_dir() {
    local D="$1"
    local NAME=$(basename "$D")
    [ ! -d "$D" ] && return
    echo "Processing $NAME..."
    find "$D" -maxdepth 1 -name "*.dot" | while read DOTFILE; do
        if command -v dot &> /dev/null; then
            dot -Tpdf "$DOTFILE" -o "$DOTFILE.pdf"
            echo "  Rendered $DOTFILE.pdf"
        fi
    done
    find "$D" -maxdepth 1 -name "trace_events*.json.gz" | while read TGZ; do
        BASE_JSON="${TGZ%.gz}"
        gunzip -c "$TGZ" > "$BASE_JSON"
        echo "  Unzipped $BASE_JSON"
        SIZE=$(du -h "$BASE_JSON" | cut -f1)
        LINES=$(wc -l < "$BASE_JSON")
        echo "Trace: $(basename $TGZ)" >> "$D/apex_trace_summary.txt"
        echo "  Size: $SIZE" >> "$D/apex_trace_summary.txt"
        echo "  Lines: $LINES" >> "$D/apex_trace_summary.txt"
    done
    echo "[$NAME]" >> "$DIR_APEX/apex_index.txt"
    ls -1 "$D" | sed 's/^/  - /' >> "$DIR_APEX/apex_index.txt"
    echo "" >> "$DIR_APEX/apex_index.txt"
}

process_apex_dir "$DIR_APEX"
for D in "$DIR_APEX"/run_*; do
    process_apex_dir "$D"
done

if [ "$MEMORY_MODE" -eq 1 ]; then
    echo "Processing Memory Stats..."
    STATS_FILE="$DIR_MEM/starpu_stats.txt"
    
    awk '/MSI cache stats :|Allocation cache stats:|Data transfer stats:|Worker stats:/{f=1} f{print}' "$DIR_SUM/run.log" > "$STATS_FILE"
    
    if [ ! -s "$STATS_FILE" ]; then
        echo "FAIL: starpu_stats.txt is empty. Stats extraction failed."
        return 1
    fi
    
    CHECK_LOG="$DIR_MEM/memory_check.log"
    echo "Verifying Memory Stats..." > "$CHECK_LOG"

    awk '/Data transfer stats:/{f=1} f{print; if ($0 ~ /^#---------------------$/ && f) {f=0; exit}}' "$STATS_FILE" > "$DIR_MEM/transfers.txt"
    if [ ! -s "$DIR_MEM/transfers.txt" ]; then
        echo "No transfer stats collected." > "$DIR_MEM/transfers.txt"
        echo "WARN: Transfer stats missing in split." >> "$CHECK_LOG"
    fi

    awk '/Worker stats:/{f=1} f{print; if ($0 ~ /^#---------------------$/ && f) {f=0; exit}}' "$STATS_FILE" > "$DIR_MEM/workers.txt"
    if [ ! -s "$DIR_MEM/workers.txt" ]; then
        echo "No worker stats collected." > "$DIR_MEM/workers.txt"
        echo "WARN: Worker stats missing in split." >> "$CHECK_LOG"
    fi

    awk 'BEGIN{f=0} /MSI cache stats :|Allocation cache stats:/{f=1} f{print} /Data transfer stats:/{exit}' "$STATS_FILE" | sed '/Data transfer stats:/d' > "$DIR_MEM/cache.txt"
    if [ ! -s "$DIR_MEM/cache.txt" ]; then
        echo "No cache stats collected." > "$DIR_MEM/cache.txt"
        echo "WARN: Cache stats missing (optional)." >> "$CHECK_LOG"
    fi
    
    PASS=1
    if [ -s "$STATS_FILE" ]; then
        echo "[PASS] starpu_stats.txt exists and non-empty." >> "$CHECK_LOG"
    else
        echo "[FAIL] starpu_stats.txt missing or empty." >> "$CHECK_LOG"
        PASS=0
    fi
    if grep -q "Worker stats:" "$STATS_FILE"; then
         echo "[PASS] 'Worker stats:' found." >> "$CHECK_LOG"
    else
         echo "[FAIL] 'Worker stats:' NOT found." >> "$CHECK_LOG"
         PASS=0
    fi
    if grep -E -q "Data transfer stats:|Transfers from CUDA" "$STATS_FILE"; then
         echo "[PASS] Transfer stats found." >> "$CHECK_LOG"
    else
         echo "[WARN] Transfer stats NOT found (might be CPU-only run)." >> "$CHECK_LOG"
         PASS=0
    fi
    echo "Stats File Sizes:" >> "$CHECK_LOG"
    du -h "$DIR_MEM"/* >> "$CHECK_LOG"
    if [ "$PASS" -eq 0 ]; then
        echo "MEMORY VERIFICATION FAILED. See $CHECK_LOG"
        return 1
    fi
fi

echo "=== STEP 2: Verify RAW ==="
RAW_FILES=$(ls "$DIR_RAW"/prof_file_* 2>/dev/null)
if [ -z "$RAW_FILES" ]; then
    echo "ERROR: No prof_file_* found in $DIR_RAW. Execution failed/No tracing."
    return 1
fi
echo "PASS: Raw traces exist."

echo "=== STEP 3: FxT Conversion ==="
cd "$DIR_FXT"
if command -v starpu_fxt_tool &> /dev/null; then
    starpu_fxt_tool -i "$DIR_RAW"/prof_file_* > fxt_tool.log 2>&1
else
    echo "ERROR: starpu_fxt_tool not found."
    return 1
fi
for f in paje.trace tasks.rec dag.dot; do
    if [ ! -s "$f" ]; then
        echo "ERROR: $f missing or empty in $DIR_FXT."
        return 1
    fi
done

echo "=== STEP 4: Distribution ==="
cp "$DIR_FXT/dag.dot" "$DIR_DOT/"
cp "$DIR_FXT/tasks.rec" "$DIR_TASKS/"
if [ -s "$DIR_FXT/papi.rec" ]; then
    cp "$DIR_FXT/papi.rec" "$DIR_PAPI/"
else
    touch "$DIR_PAPI/papi.rec"
fi

echo "=== STEP 5: Graphviz ==="
if command -v dot &> /dev/null; then
    dot -Tpdf "$DIR_DOT/dag.dot" -o "$DIR_DOT/dag.pdf"
fi

echo "=== STEP 6: Tasks Completion ==="
if command -v starpu_tasks_rec_complete &> /dev/null; then
    starpu_tasks_rec_complete "$DIR_TASKS/tasks.rec" "$DIR_TASKS/tasks2.rec" > "$DIR_TASKS/tasks_complete.log" 2>&1 || true
fi

echo "=== STEP 7: Codelet Profile ==="
cd "$DIR_PROF"
if [ -s "$DIR_FXT/distrib.data" ]; then
    cp "$DIR_FXT/distrib.data" .
elif [ -s "$DIR_FXT/trace.rec" ]; then
      starpu_fxt_tool -d "$DIR_FXT/trace.rec" > /dev/null 2>&1 || true
fi
if [ -s "distrib.data" ] && command -v starpu_codelet_profile &> /dev/null; then
    for symbol in cl_preproc cl_infer_trt cl_post; do
        starpu_codelet_profile distrib.data "$symbol" > "$symbol.profile.log" 2>&1 || true
    done
fi

echo "=== STEP 8: Histo Profile ==="
cd "$DIR_HISTO"
if [ -s "$DIR_PROF/distrib.data" ] && command -v starpu_codelet_histo_profile &> /dev/null; then
    starpu_codelet_histo_profile "$DIR_PROF/distrib.data" > histo.log 2>&1 || true
fi

echo "=== STEP 9: Data Trace ==="
cd "$DIR_DATA"
if command -v starpu_fxt_data_trace &> /dev/null; then
    starpu_fxt_data_trace "$DIR_RAW"/prof_file_* cl_preproc cl_infer_trt cl_post > data_trace.txt 2>&1 || true
fi

echo "=== STEP 10: State Stats ==="
cd "$DIR_STATS"
if command -v starpu_trace_state_stats.py &> /dev/null && [ -f "$DIR_FXT/trace.rec" ]; then
    starpu_trace_state_stats.py "$DIR_FXT/trace.rec" > state_stats.txt 2>&1 || true
fi

echo "=== STEP 11: StarVZ ==="
cd "$DIR_STARVZ"

# WORKAROUND: StarVZ Rcpp stoi parsing crashes natively on the Link traces
# generated by the `ws` (work stealing) scheduler since they are not standard MPI vectors.
# We set STARVZ_IGNORE_LINKS=1 which our patched phase1-workflow.sh understands to bypass it!
if [ "$SCHED" = "ws" ]; then
    echo "Applying WS-Link stoi parsing workaround..."
    export STARVZ_IGNORE_LINKS=1
fi

if command -v starvz &> /dev/null; then
    starvz --use-paje-trace "$DIR_FXT" > starvz.log 2>&1 || true
    cp "$DIR_FXT"/starvz* . 2>/dev/null || true
    cp "$DIR_FXT"/*.parquet . 2>/dev/null || true
fi

echo "=== STEP 12: Summary ==="
echo "RUN COMPLETE: $BASE"

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
