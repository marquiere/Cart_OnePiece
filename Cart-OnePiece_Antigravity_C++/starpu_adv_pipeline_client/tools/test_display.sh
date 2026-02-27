#!/bin/bash
set -e
export CARLA_ROOT=/home/eric/CARLA_Latest

echo "--- STARTING SERVER ---"
./tools/run_server.sh --port 2000
echo "Waiting 15 seconds for CARLA to initialize Map..."
sleep 15

echo "--- RUNNING PIPELINE WITH SDL2 VIEWER ---"
./build/pipeline_starpu --host 127.0.0.1 --port 2000 --w 800 --h 600 --fps 20 --frames 30  --out_w 512 --out_h 256 --inflight 2 --cpu_workers 4 --print_every 10 --eval_every 30 --display 1

echo "--- KILLING SERVER ---"
./tools/kill_server.sh

echo "Visualizer Test Completed."
