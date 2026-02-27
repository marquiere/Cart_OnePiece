#!/bin/bash
set -e
export CARLA_ROOT=/home/eric/CARLA_Latest

echo "--- STARTING SERVER ---"
./tools/run_server.sh --port 2000
echo "Waiting 15 seconds for CARLA to initialize Map..."
sleep 15

echo "--- RUNNING DATASET EXTRACTION (200 FRAMES) ---"
./build/dataset_sanity --host 127.0.0.1 --port 2000 --w 800 --h 600 --fps 10 --frames 200 --engine models/dummy.engine --out_w 512 --out_h 256 --assume_bgra 1

echo "--- FORMATTING DEBUG SAMPLES ---"
python3 tools/verify_seg.py

echo "--- KILLING SERVER ---"
./tools/kill_server.sh

echo "Fix Verification Completed."
