#!/bin/bash
set -e
export CARLA_ROOT=/home/eric/CARLA_Latest

FRAMES=200
ENGINE="models/dummy.engine"
DISPLAY_MODE=0
MEMORY_MODE=0

while [[ "$#" -gt 0 ]]; do
    case $1 in
        --frames) FRAMES="$2"; shift ;;
        --engine) ENGINE="$2"; shift ;;
        --memory) MEMORY_MODE=1 ;;
        --deeplabv3) ENGINE="models/deeplabv3_mobilenet.engine" ;;
        --resnet50) ENGINE="models/fcn_resnet50.engine" ;;
        --display) DISPLAY_MODE=1 ;;
        --no_display) DISPLAY_MODE=0 ;;
        --help) 
            echo "Usage: $0 [--frames N] [--engine PATH] [--memory] [--deeplabv3] [--resnet50] [--display] [--no_display]"
            exit 0
            ;;
        *) echo "Unknown parameter passed: $1"; exit 1 ;;
    esac
    shift
done

ENGINE_NAME=$(basename "$ENGINE" .engine)
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
export RUN_DIR="$(pwd)/runs/${TIMESTAMP}_${ENGINE_NAME}"
mkdir -p "$RUN_DIR"

echo "--- STARTING SERVER ---"
pkill -9 -f CarlaUE4 || true
pkill -9 -f pipeline_starpu || true
./tools/run_server.sh --port 2000 --run_dir "$RUN_DIR"
echo "Waiting 15 seconds for CARLA to initialize Map..."
sleep 15

echo "--- RUNNING DATASET EXTRACTION ---"
# Extract 100% of the simulation length as a dataset 
DATASET_FRAMES=$FRAMES

./build/dataset_sanity --host 127.0.0.1 --port 2000 --w 800 --h 600 --fps 10 --frames $DATASET_FRAMES --engine "$ENGINE" --out_w 512 --out_h 256 --assume_bgra 1 --display "$DISPLAY_MODE" || echo "Extraction Failed!"

echo "--- RUNNING PROFILED PIPELINE ---"
# Compute appropriate print_every based on frames
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

echo "--- SUMMARIZING ---"
python3 ./tools/summarize_run.py --run_dir "$RUN_DIR"

echo "--- KILLING SERVER ---"
./tools/kill_server.sh --run_dir "$RUN_DIR"
