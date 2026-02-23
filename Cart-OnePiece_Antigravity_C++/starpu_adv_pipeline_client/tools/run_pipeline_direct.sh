#!/bin/bash
set -e

START_SERVER=0
FRAMES=200
FPS=20
ENGINE="models/dummy.engine"

while [[ "$#" -gt 0 ]]; do
    case $1 in
        --start-server) START_SERVER=1; shift ;;
        --frames) FRAMES="$2"; shift 2 ;;
        --fps) FPS="$2"; shift 2 ;;
        --engine) ENGINE="$2"; shift 2 ;;
        *) echo "Unknown parameter passed: $1"; exit 1 ;;
    esac
done

if [ "$START_SERVER" -eq 1 ]; then
    echo "Starting CARLA server..."
    ./tools/run_server.sh --port 2000
    sleep 8
fi

# We don't manually create the runs/timestamp dir here for CSV because the C++ app does it natively.
# However, we will capture its stdout to a log file within that same logical structure.
# The app creates the folder dynamically using its own timestamp, so let's just create one here and pass it,
# or simply tee the log in the project root if it's easier.
# Actually, let's keep it simple and tee it beside the app.
echo "--- Compiling ---"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DWITH_TRT=ON
cmake --build build -j

echo "--- Running Pipeline Direct ---"
./build/pipeline_direct \
    --host 127.0.0.1 --port 2000 \
    --w 800 --h 600 --fps $FPS --frames $FRAMES \
    --engine $ENGINE --out_w 512 --out_h 256 \
    --assume_bgra 1 --resize bilinear \
    --print_every 20 --eval_every 50 2>&1 | tee pipeline_direct_latest.log

if [ "$START_SERVER" -eq 1 ]; then
    echo "Killing CARLA server..."
    ./tools/kill_server.sh
fi

echo "Run finished! Check 'runs/' for the output CSVs."
