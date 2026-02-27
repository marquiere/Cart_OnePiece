#!/bin/bash
set -e
export CARLA_ROOT=/home/eric/CARLA_Latest

echo "--- STARTING SERVER ---"
./tools/run_server.sh --port 2000
echo "Waiting 15 seconds for CARLA to initialize Map..."
sleep 15

echo "--- EVALUATING PIPELINE_STARPU METRICS ---"
# User requested running pipeline_starpu and pulling top stats
./build/pipeline_starpu --engine models/carla_seg_fp16.engine --out_w 512 --out_h 256 --display 1 --frames 100 --host 127.0.0.1 --port 2000 --assume_bgra 1
# Note: pipeline_starpu prints output dims and metrics periodically.

echo "--- RUNNING DATASET DUMP (10 FRAMES FOR PRED COLOR PNGS) ---"
./build/dataset_sanity --host 127.0.0.1 --port 2000 --w 800 --h 600 --fps 10 --frames 10 --engine models/carla_seg_fp16.engine --out_w 512 --out_h 256 --assume_bgra 1

echo "--- FORMATTING DEBUG SAMPLES ---"
python3 tools/verify_seg.py

echo "--- KILLING SERVER ---"
./tools/kill_server.sh

echo "Fix Verification Completed."
