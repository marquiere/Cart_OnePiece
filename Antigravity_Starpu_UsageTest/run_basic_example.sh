#!/bin/bash

# run_basic_example.sh
# Helper script to run StarPU basic examples with rr_workers and generate StarVZ visualizations.

if [ -z "$1" ]; then
    echo "Usage: ./run_basic_example.sh <path_to_example_executable> [scheduler_policy] [ncpu] [ncuda]"
    echo "Example: ./run_basic_example.sh examples/basic_examples/mult eager 3 1"
    echo "Default scheduler: rr_workers"
    echo "Default ncpu: (StarPU default)"
    echo "Default ncuda: (StarPU default)"
    exit 1
fi

EXE="$1"
SCHED="${2:-rr_workers}"
NCPU="$3"
NCUDA="$4"
# Additional arguments for the executable can be passed after NCUDA
shift 4 2>/dev/null || shift $(($# > 0 ? $# : 0))
EXTRA_ARGS="$@"

if [ ! -z "$NCPU" ]; then
    export STARPU_NCPU="$NCPU"
fi
if [ ! -z "$NCUDA" ]; then
    export STARPU_NCUDA="$NCUDA"
fi
if [ ! -f "$EXE" ]; then
    echo "ERROR: Executable '$EXE' not found!"
    exit 1
fi

# Create a unique output directory
RUN_ID=$(date +%Y%m%d_%H%M%S)_$(basename "$EXE")
BASE="/tmp/starpu_basic_examples/$RUN_ID"
DIR_RAW="$BASE/00_raw"
DIR_FXT="$BASE/01_fxt"

mkdir -p "$DIR_RAW" "$DIR_FXT"

# Set up StarPU Environment
export STARPU_FXT_PREFIX="$DIR_RAW"
export STARPU_FXT_TRACE=1
export STARPU_PROFILING=1
export STARPU_WORKER_STATS=1
export STARPU_BUS_STATS=1
export STARPU_MEMORY_STATS=1
export STARPU_ENABLE_STATS=1
export STARPU_TRACE_BUFFER_SIZE=5000
export STARPU_FXT_EVENTS=1

# Inject custom scheduler globally
export STARPU_SCHED_LIB="$(readlink -f ./custom_sched_rr_workers/build/libstarpu_sched_rr_workers.so)"
export STARPU_SCHED="$SCHED"
export RR_WORKERS_DEBUG=1

echo "=============================================="
echo "Running: $EXE"
echo "Scheduler: $SCHED"
echo "CPUs configured: ${STARPU_NCPU:-default}"
echo "CUDAs configured: ${STARPU_NCUDA:-default}"
echo "Output Directory: $BASE"
echo "=============================================="

# Execute the example
$EXE $EXTRA_ARGS

echo "=============================================="
echo "Execution finished. Converting raw FxT trace..."
starpu_fxt_tool -i "$DIR_RAW"/prof_file_* -d "$DIR_FXT" > "$BASE/fxt.log" 2>&1

# Move all local .rec files to the DIR_FXT to avoid pollution and ensure StarVZ sees them
# This is a key step because StarPU examples often generate .rec files in CWD
# IMPORTANT: Use the output directory BASE if they are there, or CWD
find . -maxdepth 2 -name "*.rec" -exec mv {} "$DIR_FXT/" \; 2>/dev/null || true
find "$BASE" -name "*.rec" -exec mv {} "$DIR_FXT/" \; 2>/dev/null || true
find /tmp -maxdepth 1 -name "*.rec" -user $(whoami) -exec mv {} "$DIR_FXT/" \; 2>/dev/null || true

echo "Generating StarVZ visualization..."
DIR_STARVZ="$BASE/08_starvz"
mkdir -p "$DIR_STARVZ"

# --- DAG Generation ---
DAG_DOT="$DIR_FXT/dag.dot"
if [ -f "$DAG_DOT" ]; then
    echo "Generating DAG visualization..."
    if command -v dot &> /dev/null; then
        dot -Tpdf "$DAG_DOT" -o "$BASE/dag.pdf" 2>/dev/null
        if [ -f "$BASE/dag.pdf" ]; then
            echo "-> SUCCESS: DAG PDF generated at $BASE/dag.pdf"
        else
            # Try PNG if PDF fails for some reason
            dot -Tpng "$DAG_DOT" -o "$BASE/dag.png" 2>/dev/null
            if [ -f "$BASE/dag.png" ]; then
                echo "-> SUCCESS: DAG PNG generated at $BASE/dag.png"
            fi
        fi
    else
        echo "-> WARNING: 'dot' command (Graphviz) not found. Skipping DAG conversion."
    fi
fi
# ----------------------

if command -v starvz &> /dev/null; then
    # Workaround: Ensure tasks.rec has the SubmitOrder and SubmitTime field to avoid Phase 1 crash
    # If tasks.rec is missing, we try to use sched_tasks.rec as a base
    TASKS_REC="$DIR_FXT/tasks.rec"
    if [ ! -f "$TASKS_REC" ]; then
        if [ -f "$DIR_FXT/sched_tasks.rec" ]; then
            echo "-> INFO: Using sched_tasks.rec as tasks.rec"
            cp "$DIR_FXT/sched_tasks.rec" "$TASKS_REC"
        elif [ -f "$DIR_FXT/rec.tasks.csv.gz" ]; then
            # If we already have the CSV but it's broken, StarVZ Phase 1 will fail.
            # We can try to hide it so StarVZ regenerates it from .rec files if possible,
            # but here it seems we don't have the right .rec files.
            :
        fi
    fi

    if [ -f "$TASKS_REC" ]; then
        echo "-> INFO: Patching tasks.rec to satisfy StarVZ."
        # Inject SubmitOrder if missing
        if ! grep -q "SubmitOrder:" "$TASKS_REC"; then
            sed -i '/^\(JobId\|TaskPointer\): /a SubmitOrder: 0' "$TASKS_REC"
        fi
        # Inject SubmitTime if missing (use Time as fallback)
        if ! grep -q "SubmitTime:" "$TASKS_REC"; then
            # We use a trick: duplicate the Time: field as SubmitTime:
            # In some .rec files it might be 'Time', in others 'ReadyTime'
            if grep -q "^Time: " "$TASKS_REC"; then
                sed -i 's/^Time: \(.*\)/Time: \1\nSubmitTime: \1/' "$TASKS_REC"
            elif grep -q "^ReadyTime: " "$TASKS_REC"; then
                sed -i 's/^ReadyTime: \(.*\)/ReadyTime: \1\nSubmitTime: \1/' "$TASKS_REC"
            else
                # Last resort: add a dummy SubmitTime
                sed -i '/^JobId: /a SubmitTime: 0' "$TASKS_REC"
            fi
        fi
    fi

    (
        cd "$DIR_STARVZ" && \
        starvz --use-paje-trace "$DIR_FXT" > "$BASE/starvz.log" 2>&1
    )
    STARVZ_RC=$?

    # Defensive check: if it failed because of empty worker states, it's a known limitation
    if [ $STARVZ_RC -ne 0 ] && grep -q "After reading worker states, number of rows is zero" "$BASE/starvz.log"; then
        echo "-> WARNING: StarVZ failed because the trace contains no worker state events (common for some examples)."
        echo "   Consider using a larger workload or checking if the example supports profiling."
    fi

    # StarVZ sometimes puts the PDF in the input directory instead of CWD
    if [ ! -f "$DIR_STARVZ/starvz.pdf" ] && [ -f "$DIR_FXT/starvz.pdf" ]; then
        mv "$DIR_FXT/starvz.pdf" "$DIR_STARVZ/starvz.pdf"
    fi

    if [ $STARVZ_RC -eq 0 ] && [ -f "$DIR_STARVZ/starvz.pdf" ]; then
        echo "-> SUCCESS: StarVZ PDF generated at $DIR_STARVZ/starvz.pdf"
    else
        echo "-> WARNING: StarVZ failed to generate PDF. See log: $BASE/starvz.log"
    fi
else
    echo "-> WARNING: starvz CLI not found on system path!"
fi

echo "All artifacts saved securely in: $BASE"
echo "=============================================="
