#!/bin/bash
set -e
export CARLA_ROOT=/home/eric/CARLA_Latest

FRAMES=1200
ENGINE="models/dummy.engine"
EXTRA_ARGS=""

while [[ "$#" -gt 0 ]]; do
    case $1 in
        --frames) FRAMES="$2"; shift ;;
        --engine) ENGINE="$2"; shift ;;
        --trace-root) EXTRA_ARGS="$EXTRA_ARGS --trace-root $2"; shift ;;
        --memory) EXTRA_ARGS="$EXTRA_ARGS --memory" ;;
        --apex-mode)
            if [[ -n "$2" && "$2" != --* ]]; then
                EXTRA_ARGS="$EXTRA_ARGS --apex-mode $2"
                shift
            else
                # Default behavior if specified without a target mode
                EXTRA_ARGS="$EXTRA_ARGS --apex-mode all"
            fi
            ;;
        --deeplabv3) ENGINE="models/deeplabv3_mobilenet.engine" ;;
        --resnet50) ENGINE="models/fcn_resnet50.engine" ;;
        --display) EXTRA_ARGS="$EXTRA_ARGS --display" ;;
        --help) 
            echo "Usage: $0 [--frames N] [--engine PATH] [--trace-root PATH] [--memory] [--apex-mode MODE] [--deeplabv3] [--resnet50] [--display]"
            exit 0
            ;;
        *) echo "Unknown parameter passed: $1"; exit 1 ;;
    esac
    shift
done

echo "--- STARTING SERVER ---"
./tools/run_server.sh --port 2000
echo "Waiting 15 seconds for CARLA to initialize Map..."
sleep 15

echo "--- RUNNING PROFILING HARNESS (DEFAULT SCHEDULER: DMDA) ---"
./tools/verify_profiling.sh --frames "$FRAMES" --engine "$ENGINE" --sched dmda $EXTRA_ARGS

echo "--- RUNNING PROFILING HARNESS (CUSTOM SCHEDULER: RR_WORKERS) ---"
./tools/verify_profiling.sh --frames "$FRAMES" --engine "$ENGINE" --sched rr_workers $EXTRA_ARGS

echo "--- RUNNING PROFILING HARNESS (WORK STEALING SCHEDULER: WS) ---"
./tools/verify_profiling.sh --frames "$FRAMES" --engine "$ENGINE" --sched ws $EXTRA_ARGS

echo "--- RUNNING PROFILING HARNESS (EAGER SCHEDULER: EAGER) ---"
./tools/verify_profiling.sh --frames "$FRAMES" --engine "$ENGINE" --sched eager $EXTRA_ARGS

echo "--- KILLING SERVER ---"
./tools/kill_server.sh

echo "Validation Runs Completed."
