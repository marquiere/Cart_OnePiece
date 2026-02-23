#!/bin/bash

# starpu_sched_help.sh
# Helper script to print available StarPU schedulers, injecting the custom rr_workers plugin

# Rebuild plugin just in case
make -C custom_sched_rr_workers/build >/dev/null 2>&1 || true

# Compute absolute path to the compiled scheduler plugin
PLUGIN_ABS_PATH="$(readlink -f ./custom_sched_rr_workers/build/libstarpu_sched_rr_workers.so)"

if [ ! -f "$PLUGIN_ABS_PATH" ]; then
    echo "ERROR: Could not find plugin at $PLUGIN_ABS_PATH"
    echo "Please build the plugin first:"
    echo "  cmake -S custom_sched_rr_workers -B custom_sched_rr_workers/build && cmake --build custom_sched_rr_workers/build -j"
    exit 1
fi

# Tell StarPU to load this plugin
export STARPU_SCHED_LIB="$PLUGIN_ABS_PATH"

# Ask StarPU to print the help list of schedulers
export STARPU_SCHED="help"

echo "=== StarPU Schedulers List ==="

# Execute an application that simply calls starpu_init(). 
# Usually starpu_machine_display is installed globally with StarPU, 
# but if not, we fallback to our own pipeline app with minimal args to force an init.
if command -v starpu_machine_display &> /dev/null; then
    starpu_machine_display 2>&1 | awk '/The variable STARPU_SCHED/,/^\[starpu\]/' | head -n -1
else
    # Fallback to the pipeline binary with 1 fast iteration to let starpu_init trigger the help print
    ./tools/starpu_adv_pipeline/build/starpu_adv_pipeline --frames 1 --mb 1 2>&1 | awk '/The variable STARPU_SCHED/,/^\[starpu\]/' | head -n -1 || true
fi
