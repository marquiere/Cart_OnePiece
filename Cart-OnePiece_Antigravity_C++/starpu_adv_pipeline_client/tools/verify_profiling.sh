#!/bin/bash
set -e

# ==============================================================================
# PROFILING SAFETY CHECK (MUST BE FIRST)
# ==============================================================================
# If user runs "apex_exec ./verify_profiling.sh", LD_PRELOAD infects all sub-commands
# (date, mkdir, ls), corrupting outputs. We must detect and unset it immediately.
if [ -n "$LD_PRELOAD" ]; then
    if [[ "$LD_PRELOAD" == *"libapex"* ]]; then
        echo "NOTE: APEX detected in LD_PRELOAD. Forcing USE_APEX=1 and cleaning environment for script execution."
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
FRAMES=20
MB=2
INFER_ITERS=1
BURST=0
WARMUP=0
SCHED="dmda" # Default to a smart scheduler
MEMORY_MODE=0
TRANSFER_MB=0
APEX_MODE="off"

# Helper to parse args
usage() {
    echo "Usage: $0 [options]"
    echo "  --trace-root PATH   Set trace root directory (default: $TRACE_ROOT_DEFAULT)"
    echo "  --frames N          Number of frames (default: 20)"
    echo "  --mb N              Buffer size in MB (default: 2)"
    echo "  --infer-iters N     Inference iterations (default: 1)"
    echo "  --burst N           Burst count (default: 0)"
    echo "  --warmup N          Warmup frames (default: 0)"
    echo "  --sched NAME        Scheduler policy (default: dmda)"
    echo "  --memory            Enable Memory/Bus Stats mode"
    echo "  --transfer-mb N     Add extra transfer buffer of size N MB"
    echo "  --apex-mode MODE    Set APEX mode (off, gtrace, gtrace-tasks, taskgraph, all)"
    echo "  --help              Show this help"
    exit 1
}

# Parse CLI arguments
while [[ "$#" -gt 0 ]]; do
    case $1 in
        --trace-root) TRACE_ROOT="$2"; shift ;;
        --frames) FRAMES="$2"; shift ;;
        --mb) MB="$2"; shift ;;
        --infer-iters) INFER_ITERS="$2"; shift ;;
        --burst) BURST="$2"; shift ;;
        --warmup) WARMUP="$2"; shift ;;
        --sched) SCHED="$2"; shift ;;
        --memory) MEMORY_MODE=1 ;;
        --transfer-mb) TRANSFER_MB="$2"; shift ;;
        --apex-mode) APEX_MODE="$2"; shift ;;
        --help) usage ;;
        *) echo "Unknown parameter passed: $1"; usage ;;
    esac
    shift
done

# ==============================================================================
# DIAGNOSTICS: PAPI & Perf
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
RUN_ID=$(date +%Y%m%d_%H%M%S)
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
        exit 1
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
        exit 1
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
    exit 1
fi

# Construct Arguments
ARGS="--host 127.0.0.1 --port 2000 --w 800 --h 600 --fps 20 --frames $FRAMES --engine models/dummy.engine --out_w 512 --out_h 256 --inflight 2 --cpu_workers 8 --print_every 20 --eval_every $FRAMES"

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
    # TaskStubs + APEX usually needs APEX_OTF2=1 environment
    export APEX_OTF2=${APEX_OTF2:-1}
    # Ensure binary linked with TaskStubs
    if ! ldd "$BIN_PATH" | grep -q "timer_plugin"; then
         echo "WARNING: Binary does not appear to link libtimer_plugin. Rebuild with -DENABLE_TASKSTUBS_APEX=ON?"
    fi
    # Proceed (Wrapper not needed if linked)
    
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

# A) GPU Energy (Sampling Only for RTX 3060 robustness)
PID_GPU_SAMPLE=""
if command -v nvidia-smi &> /dev/null; then
    echo "Starting GPU Power Sampling (10Hz)..."
    # Format: timestamp, power.draw
    (while true; do nvidia-smi --query-gpu=timestamp,power.draw --format=csv,noheader,nounits; sleep 0.1; done) > "$DIR_ENERGY/gpu_power_samples.csv" 2>>"$LOG_ENERGY" &
    PID_GPU_SAMPLE=$!
else
    echo "WARN: nvidia-smi not found. Skipping GPU energy." >> "$LOG_ENERGY"
fi

# B) CPU RAPL (Robust with optional Sudo)
# ------------------------------------------------------------------------------
# Function to read RAPL values (single snapshot)
# Returns: "path:value" lines
get_rapl_snapshot() {
    local USE_SUDO="${USE_SUDO_RAPL:-0}"
    local FILES=$(find /sys/class/powercap/intel-rapl* -name "energy_uj" 2>/dev/null | sort)
    
    # Fallback
    if [ -z "$FILES" ]; then
        if [ -e "/sys/class/powercap/intel-rapl:0/energy_uj" ] || [ "$USE_SUDO" -eq 1 ]; then
             FILES="/sys/class/powercap/intel-rapl:0/energy_uj"
        fi
    fi
     # Attempt DRAM
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

# Capture Baseline
echo "Snapshotting CPU RAPL (Before)..."
get_rapl_snapshot > "$DIR_ENERGY/cpu_rapl_before_uj.txt"

# Start Background CPU Sampler (10Hz)
CPU_SAMPLE_RAW="$DIR_ENERGY/cpu_rapl_samples_raw.csv"
echo "timestamp,domain,energy_uj" > "$CPU_SAMPLE_RAW"
(
    while true; do
        TS=$(date +"%Y-%m-%dT%H:%M:%S%z")
        get_rapl_snapshot | while read LINE; do
            # LINE format: path:value
            # Parse domain name from path
            P=$(echo "$LINE" | cut -d: -f1)
            V=$(echo "$LINE" | cut -d: -f2)
            
            # Map path to domain name
            # intel-rapl:0 -> package
            # intel-rapl:0:0 -> dram
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

# Determine APEX Modes to Run
APEX_MODES_TO_RUN=()
case "$APEX_MODE" in
    off) ;; # Normal run, no extra APEX specific modes (but USE_TASKSTUBS/USE_APEX might still be active)
    gtrace) APEX_MODES_TO_RUN+=("gtrace") ;;
    gtrace-tasks) APEX_MODES_TO_RUN+=("gtrace-tasks") ;;
    taskgraph) APEX_MODES_TO_RUN+=("taskgraph") ;;
    all) APEX_MODES_TO_RUN+=("gtrace" "gtrace-tasks" "taskgraph") ;;
    *) echo "ERROR: Unknown apex-mode: $APEX_MODE"; exit 1 ;;
esac

# ------------------------------------------------------------------------------
# 1. Standard Run (Primary for StarPU 00_raw)
# ------------------------------------------------------------------------------
# We always run the "standard" configured command first.
# If APEX_MODE is OFF, this is the only run.
# If MODES are active, this run might be redundant if the user only wanted APEX data,
# BUT 'verify_profiling.sh' serves StarPU verification primarily.
# To satsify "Do not break current StarPU tool workflow", we must ensure 00_raw is populated.
# We will treat the first run as the "Primary StarPU Run" and then do APEX extra runs if requested.

echo "=== STEP 1: Run Application (Primary) ==="
echo "Executing: $RUN_CMD"
echo "Log: $DIR_SUM/run.log"

env > "$DIR_SUM/env.txt"
echo "$RUN_CMD" > "$DIR_SUM/cmd.txt"

# Precise Timing
TS_START=$(date +%s.%N)

# Run!
eval $RUN_CMD > "$DIR_SUM/run.log" 2>&1

TS_END=$(date +%s.%N)
DURATION=$(python3 -c "print($TS_END - $TS_START)")

# ------------------------------------------------------------------------------
# ENERGY CAPTURE: END (For Primary Run)
# ------------------------------------------------------------------------------
# Stop Sampling
if [ -n "$PID_GPU_SAMPLE" ]; then
    kill "$PID_GPU_SAMPLE" 2>/dev/null || true
    wait "$PID_GPU_SAMPLE" 2>/dev/null || true
fi
if [ -n "$PID_CPU_SAMPLE" ]; then
    kill "$PID_CPU_SAMPLE" 2>/dev/null || true
    wait "$PID_CPU_SAMPLE" 2>/dev/null || true
fi

# Snapshot CPU RAPL (After)
echo "Snapshotting CPU RAPL (After)..."
get_rapl_snapshot > "$DIR_ENERGY/cpu_rapl_after_uj.txt"

# ------------------------------------------------------------------------------
# APEX Output Capture (Primary Run)
# ------------------------------------------------------------------------------
# If USE_TASKSTUBS or USE_APEX was set for the primary run, artifacts land in PWD.
# Move them to 10_apex root (default location).
if [ "$USE_TASKSTUBS" -eq 1 ] || [ "$USE_APEX" -eq 1 ]; then
    echo "Capturing Primary Run APEX output to $DIR_APEX"
    mv apex.* *.otf2 *.json "$DIR_APEX/" 2>/dev/null || true
    # For screen output APEX (wrapper), extract from log
    if [ "$USE_APEX" -eq 1 ]; then
         sed -n '/APEX Version:/,$p' "$DIR_SUM/run.log" > "$DIR_APEX/apex.log"
    fi
fi

# ------------------------------------------------------------------------------
# 2. Extra APEX Mode Runs (Sequential)
# ------------------------------------------------------------------------------
if [ ${#APEX_MODES_TO_RUN[@]} -gt 0 ]; then
    echo "=== APEX Verification Modes: ${APEX_MODES_TO_RUN[*]} ==="
    
    # We must unset StarPU FxT tracing for these runs to avoid overwriting 00_raw 
    # or creating confusion? 
    # User Requirement: "Keep raw-first discipline... Do NOT reduce existing StarPU outputs".
    # User Requirement: "For each selected APEX mode... run apex_exec... output to run_<mode>".
    # I will DISABLE FxT for these runs to keep them pure APEX focused and faster.
    unset STARPU_FXT_TRACE
    unset STARPU_PROFILING
    
    for MODE in "${APEX_MODES_TO_RUN[@]}"; do
        SUB_DIR="$DIR_APEX/run_$MODE"
        mkdir -p "$SUB_DIR"
        echo ">> Running APEX Mode: $MODE (in $SUB_DIR)"
        
        # Construct specific command
        # Note: If USE_TASKSTUBS=1 is on, binary has plugin. APEX env vars control it.
        # But user requested `apex_exec --apex:<mode>` syntax which is wrapper-based.
        # wrapper `apex_exec` handles setting the mode env vars even for linked binaries?
        # Yes, usually. Or we can set env vars manually. 
        # Requirement: "apex_exec --apex:<modeflag>"
        
        # We need to strip existing `apex_exec` from RUN_CMD if it was there to avoid double wrap?
        # Actually, let's just build the command freshly using BIN_PATH and ARGS.
        
        APEX_CMD="apex_exec --apex:$MODE $BIN_PATH $ARGS"
        
        # Execution
        # We cd into subdir so files land there
        (
            cd "$SUB_DIR"
            echo "Command: $APEX_CMD" > apex_runinfo.txt
            echo "Start: $(date)" >> apex_runinfo.txt
            
            # Execute
            eval "$APEX_CMD" > apex.log 2>&1
            RET=$?
            
            echo "End: $(date)" >> apex_runinfo.txt
            echo "Return Code: $RET" >> apex_runinfo.txt
            echo "Files:" >> apex_runinfo.txt
            ls -1 >> apex_runinfo.txt
        )
    done
    
    # Restore StarPU Env just in case (though script continues to processing now)
    export STARPU_FXT_TRACE=1
    export STARPU_PROFILING=1
fi

# ------------------------------------------------------------------------------
# APEX Post-Processing (All Modes)
# ------------------------------------------------------------------------------
echo "Post-Processing APEX Artifacts in $DIR_APEX..."
rm -f "$DIR_APEX/apex_index.txt"
touch "$DIR_APEX/apex_index.txt"

# Helper to process a directory
process_apex_dir() {
    local D="$1"
    local NAME=$(basename "$D")
    [ ! -d "$D" ] && return
    
    echo "Processing $NAME..."
    
    # 1. Graphviz Dot -> PDF
    # Find any .dot files
    find "$D" -maxdepth 1 -name "*.dot" | while read DOTFILE; do
        if command -v dot &> /dev/null; then
            dot -Tpdf "$DOTFILE" -o "$DOTFILE.pdf"
            echo "  Rendered $DOTFILE.pdf"
        fi
    done
    
    # 2. JSON Traces (trace_events*.json.gz)
    find "$D" -maxdepth 1 -name "trace_events*.json.gz" | while read TGZ; do
        BASE_JSON="${TGZ%.gz}"
        # Unzip
        gunzip -c "$TGZ" > "$BASE_JSON"
        echo "  Unzipped $BASE_JSON"
        
        # Summary Stats
        SIZE=$(du -h "$BASE_JSON" | cut -f1)
        LINES=$(wc -l < "$BASE_JSON")
        echo "Trace: $(basename $TGZ)" >> "$D/apex_trace_summary.txt"
        echo "  Size: $SIZE" >> "$D/apex_trace_summary.txt"
        echo "  Lines: $LINES" >> "$D/apex_trace_summary.txt"
        
        # Perfetto Note
        if command -v perfetto &> /dev/null; then
             echo "  Perfetto CLI found (user can check documentation)."
        fi
    done
    
    # Update Index
    echo "[$NAME]" >> "$DIR_APEX/apex_index.txt"
    ls -1 "$D" | sed 's/^/  - /' >> "$DIR_APEX/apex_index.txt"
    echo "" >> "$DIR_APEX/apex_index.txt"
}

# Process Root (Standard Run)
process_apex_dir "$DIR_APEX"

# Process Subdirs (Modes)
for D in "$DIR_APEX"/run_*; do
    process_apex_dir "$D"
done

# ------------------------------------------------------------------------------
# MEMORY STATS EXTRACTION & VERIFICATION
# ------------------------------------------------------------------------------
if [ "$MEMORY_MODE" -eq 1 ]; then
    echo "Processing Memory Stats..."
    STATS_FILE="$DIR_MEM/starpu_stats.txt"
    
    # Extract block between markers
    sed -n '/===STARPU_STATS_BEGIN===/,/===STARPU_STATS_END===/p' "$DIR_SUM/run.log" > "$STATS_FILE"
    
    # Remove markers for clean file
    sed -i '/===STARPU_STATS_BEGIN===/d;/===STARPU_STATS_END===/d' "$STATS_FILE"
    
    # Verify extraction
    if [ ! -s "$STATS_FILE" ]; then
        echo "FAIL: starpu_stats.txt is empty. Stats extraction failed."
        exit 1
    fi
    
    # Hard Verification
    CHECK_LOG="$DIR_MEM/memory_check.log"
    echo "Verifying Memory Stats..." > "$CHECK_LOG"

    # Section Splitting
    # transfers.txt
    awk '/Data transfer stats:/{f=1} f{print} /Worker stats:/{f=0; exit} /^$/{if(f) exit}' "$STATS_FILE" > "$DIR_MEM/transfers.txt"
    if [ ! -s "$DIR_MEM/transfers.txt" ]; then
        echo "No transfer stats collected." > "$DIR_MEM/transfers.txt"
        echo "WARN: Transfer stats missing in split." >> "$CHECK_LOG"
    fi

    # workers.txt 
    awk '/Worker stats:/{f=1} f{print} /Allocation cache stats:|MSI cache stats:/{if(f) exit}' "$STATS_FILE" > "$DIR_MEM/workers.txt"
    if [ ! -s "$DIR_MEM/workers.txt" ]; then
        echo "No worker stats collected." > "$DIR_MEM/workers.txt"
        echo "WARN: Worker stats missing in split." >> "$CHECK_LOG"
    fi

    # cache.txt
    awk '/Allocation cache stats:|MSI cache stats:/{f=1} f{print}' "$STATS_FILE" > "$DIR_MEM/cache.txt"
    if [ ! -s "$DIR_MEM/cache.txt" ]; then
        echo "No cache stats collected." > "$DIR_MEM/cache.txt"
        echo "WARN: Cache stats missing (optional)." >> "$CHECK_LOG"
    fi
    
    PASS=1
    
    # Check 1: Stats File Exists
    if [ -s "$STATS_FILE" ]; then
        echo "[PASS] starpu_stats.txt exists and non-empty." >> "$CHECK_LOG"
    else
        echo "[FAIL] starpu_stats.txt missing or empty." >> "$CHECK_LOG"
        PASS=0
    fi
    
    # Check 2: Worker Stats present
    if grep -q "Worker stats:" "$STATS_FILE"; then
         echo "[PASS] 'Worker stats:' found." >> "$CHECK_LOG"
    else
         echo "[FAIL] 'Worker stats:' NOT found." >> "$CHECK_LOG"
         PASS=0
    fi
    
    # Check 3: Transfers (Either title or content)
    if grep -E -q "Data transfer stats:|Transfers from CUDA" "$STATS_FILE"; then
         echo "[PASS] Transfer stats found." >> "$CHECK_LOG"
    else
         echo "[WARN] Transfer stats NOT found (might be CPU-only run)." >> "$CHECK_LOG"
         # Requirement said: "FAIL only if hard checks like file existence fail... and contains either... string match"
         # "If any hard check fails... exit non-zero"
         # But transfers might be empty if 0 transfers happened?
         # User requirement: "and contains either 'Data transfer stats:' OR 'Transfers from CUDA'".
         # I will stick to the requirement. If StarPU prints "Data transfer stats:" even if empty, it's fine.
         # Checked previous log: "Data transfer stats:" matches.
         PASS=0
    fi

    echo "Stats File Sizes:" >> "$CHECK_LOG"
    du -h "$DIR_MEM"/* >> "$CHECK_LOG"
    
    cat "$CHECK_LOG"
    
    if [ "$PASS" -eq 0 ]; then
        echo "MEMORY VERIFICATION FAILED. See $CHECK_LOG"
        exit 1
    fi
fi

# ==============================================================================
# STEP 2: Verify RAW Contents
# ==============================================================================
echo "=== STEP 2: Verify RAW ==="
RAW_FILES=$(ls "$DIR_RAW"/prof_file_* 2>/dev/null)
if [ -z "$RAW_FILES" ]; then
    echo "ERROR: No prof_file_* found in $DIR_RAW. Execution failed/No tracing."
    cat "$DIR_SUM/run.log"
    exit 1
fi
echo "Found Raw Trace Files: " >> "$DIR_SUM/summary.md"
ls -l "$DIR_RAW" >> "$DIR_SUM/summary.md"
echo "PASS: Raw traces exist."

# ==============================================================================
# STEP 3: Convert to FxT (01_starpu_fxt_tool)
# ==============================================================================
echo "=== STEP 3: FxT Conversion ==="
cd "$DIR_FXT"

if command -v starpu_fxt_tool &> /dev/null; then
    starpu_fxt_tool -i "$DIR_RAW"/prof_file_* > fxt_tool.log 2>&1
else
    echo "ERROR: starpu_fxt_tool not found (MANDATORY)."
    exit 1
fi

# Verify Mandatory Artifacts
for f in paje.trace tasks.rec dag.dot; do
    if [ ! -s "$f" ]; then
        echo "ERROR: $f missing or empty in $DIR_FXT."
        exit 1
    fi
done
echo "PASS: FxT conversion successful."

# ==============================================================================
# STEP 4: Distribute Outputs (COPY)
# ==============================================================================
echo "=== STEP 4: Distribution ==="
cp "$DIR_FXT/dag.dot" "$DIR_DOT/"
cp "$DIR_FXT/tasks.rec" "$DIR_TASKS/"

# PAPI Handling
# Usually generated by starpu_fxt_tool if PAPI events present
if [ -s "$DIR_FXT/papi.rec" ]; then
    cp "$DIR_FXT/papi.rec" "$DIR_PAPI/"
else
    # Try searching in raw just in case (unlikely for FxT)
    # If empty, create empty placeholder
    touch "$DIR_PAPI/papi.rec"
    echo "WARN: papi.rec empty or missing in FxT output." >> "$DIR_SUM/run.log"
fi

# ==============================================================================
# STEP 5: Graphviz
# ==============================================================================
echo "=== STEP 5: Graphviz ==="
if command -v dot &> /dev/null; then
    dot -Tpdf "$DIR_DOT/dag.dot" -o "$DIR_DOT/dag.pdf"
    if [ ! -s "$DIR_DOT/dag.pdf" ]; then
        echo "ERROR: dag.pdf empty."
        exit 1
    fi
else
    echo "ERROR: 'dot' command not found (MANDATORY)."
    exit 1
fi
echo "PASS: DAG generated."

# ==============================================================================
# STEP 6: Tasks Completion
# ==============================================================================
echo "=== STEP 6: Tasks Completion ==="
if command -v starpu_tasks_rec_complete &> /dev/null; then
    starpu_tasks_rec_complete "$DIR_TASKS/tasks.rec" "$DIR_TASKS/tasks2.rec" > "$DIR_TASKS/tasks_complete.log" 2>&1 || true
    if [ ! -s "$DIR_TASKS/tasks2.rec" ]; then
        echo "WARN: tasks2.rec empty/failed."
    fi
else
    echo "WARN: starpu_tasks_rec_complete not found."
fi

# ==============================================================================
# STEP 7: Codelet Profile
# ==============================================================================
echo "=== STEP 7: Codelet Profile ==="
cd "$DIR_PROF"
if [ -s "$DIR_FXT/distrib.data" ]; then
    cp "$DIR_FXT/distrib.data" .
elif [ -s "$DIR_FXT/trace.rec" ]; then
      starpu_fxt_tool -d "$DIR_FXT/trace.rec" > /dev/null 2>&1 || true
fi

if [ -s "distrib.data" ]; then
    if command -v starpu_codelet_profile &> /dev/null; then
        for symbol in cl_preproc cl_infer_trt cl_post; do
            starpu_codelet_profile distrib.data "$symbol" > "$symbol.profile.log" 2>&1 || true
        done
    else
        echo "WARN: starpu_codelet_profile not found."
    fi
else
    echo "WARN: distrib.data missing. Skipping codelet profiles."
fi

# ==============================================================================
# STEP 8: Histo Profile
# ==============================================================================
echo "=== STEP 8: Histo Profile ==="
cd "$DIR_HISTO"
if [ -s "$DIR_PROF/distrib.data" ]; then
    if command -v starpu_codelet_histo_profile &> /dev/null; then
        starpu_codelet_histo_profile "$DIR_PROF/distrib.data" > histo.log 2>&1 || true
    fi
fi

# ==============================================================================
# STEP 9: Data Trace
# ==============================================================================
echo "=== STEP 9: Data Trace ==="
cd "$DIR_DATA"
if command -v starpu_fxt_data_trace &> /dev/null; then
    starpu_fxt_data_trace "$DIR_RAW"/prof_file_* cl_preproc cl_infer_trt cl_post > data_trace.txt 2>&1 || true
fi

# ==============================================================================
# STEP 10: State Stats
# ==============================================================================
echo "=== STEP 10: State Stats ==="
cd "$DIR_STATS"
if command -v starpu_trace_state_stats.py &> /dev/null && [ -f "$DIR_FXT/trace.rec" ]; then
    starpu_trace_state_stats.py "$DIR_FXT/trace.rec" > state_stats.txt 2>&1 || true
fi

# ==============================================================================
# STEP 11: StarVZ
# ==============================================================================
echo "=== STEP 11: StarVZ ==="
cd "$DIR_STARVZ"
if command -v starvz &> /dev/null; then
    starvz --use-paje-trace "$DIR_FXT" > starvz.log 2>&1 || echo "WARN: StarVZ failed"
    cp "$DIR_FXT"/starvz* . 2>/dev/null || true
    cp "$DIR_FXT"/*.parquet . 2>/dev/null || true
elif R -q -e "library(starvz)" &> /dev/null; then
    # Basic check since full starvz CLI missing
    echo "StarVZ CLI missing, verify R pkg manually." > starvz.log
fi

# ==============================================================================
# STEP 12: Summary
# ==============================================================================
echo "=== STEP 12: Summary ==="
echo "# Summary for Run $RUN_ID" > "$DIR_SUM/summary.md"
echo "Command: $RUN_CMD" >> "$DIR_SUM/summary.md"
echo "Result: SUCCESS" >> "$DIR_SUM/summary.md"
echo "" >> "$DIR_SUM/summary.md"
echo "## Folder Structure" >> "$DIR_SUM/summary.md"
if command -v tree &> /dev/null; then
    tree -L 2 "$BASE" >> "$DIR_SUM/summary.md"
    tree -L 2 "$BASE"
else
    find "$BASE" -maxdepth 2
fi

# PAPI Verification
echo "## PAPI Check"
PAPI_FILE="$DIR_PAPI/papi.rec"
if [ -s "$PAPI_FILE" ]; then
    # Check if file has meaningful content (more than header)
    # starpu_fxt_tool output often headers.
    LINE_COUNT=$(wc -l < "$PAPI_FILE")
    if [ "$LINE_COUNT" -gt 1 ]; then
        echo "PAPI PASS: $PAPI_FILE has $LINE_COUNT lines."
        head -n 5 "$PAPI_FILE"
    else
         echo "PAPI FAIL: $PAPI_FILE is almost empty."
    fi
else
    echo "PAPI FAIL: $PAPI_FILE is empty."
fi

echo "========================================"
echo "RUN COMPLETE: $BASE"
echo "========================================"
