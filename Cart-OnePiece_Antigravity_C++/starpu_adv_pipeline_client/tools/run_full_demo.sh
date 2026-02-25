#!/bin/bash
set -e
export CARLA_ROOT=/home/eric/CARLA_Latest

echo "--- STARTING SERVER ---"
./tools/run_server.sh --port 2000
echo "Waiting 15 seconds for CARLA to initialize Map..."
sleep 15

echo "--- RUNNING DATASET EXTRACTION ---"
./build/dataset_sanity --host 127.0.0.1 --port 2000 --w 800 --h 600 --fps 10 --frames 30 --engine models/dummy.engine --out_w 512 --out_h 256 --assume_bgra 1 || echo "Extraction Failed!"

echo "--- RUNNING PROFILED PIPELINE ---"
SERVER_CORES="0-9" CLIENT_CORES="10-19" ./tools/run_profiled_single_machine.sh --port 2000 --client ./build/pipeline_starpu --client_args "--host 127.0.0.1 --port 2000 --w 800 --h 600 --fps 20 --frames 120 --engine models/dummy.engine --out_w 512 --out_h 256 --inflight 2 --cpu_workers 8 --print_every 20 --eval_every 60 --display 1" &
CLIENT_PID=$!
sleep 5
./tools/monitor_split.sh --duration 20
wait $CLIENT_PID

echo "--- SUMMARIZING ---"
python3 ./tools/summarize_run.py

echo "--- KILLING SERVER ---"
./tools/kill_server.sh
