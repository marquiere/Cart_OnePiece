#!/bin/bash
set -e
export CARLA_ROOT=/home/eric/CARLA_Latest

echo "--- STARTING SERVER ---"
./tools/run_server.sh --port 2000
echo "Waiting 15 seconds for CARLA to initialize Map..."
sleep 15

echo "--- RUNNING PROFILING HARNESS (DEFAULT SCHEDULER: DMDA) ---"
./tools/verify_profiling.sh --frames 120 --sched dmda

echo "--- RUNNING PROFILING HARNESS (CUSTOM SCHEDULER: RR_WORKERS) ---"
./tools/verify_profiling.sh --frames 120 --sched rr_workers

echo "--- KILLING SERVER ---"
./tools/kill_server.sh

echo "Validation Runs Completed."
