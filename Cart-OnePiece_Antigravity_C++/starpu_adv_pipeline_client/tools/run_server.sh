#!/bin/bash
set -e

# Defaults
PORT=2000
CARLA_ROOT_DIR="${CARLA_ROOT:-}"

# Parse arguments
while [[ "$#" -gt 0 ]]; do
    case $1 in
        --port) PORT="$2"; shift ;;
        --carla_root) CARLA_ROOT_DIR="$2"; shift ;;
        --suffix) SUFFIX="_$2"; shift ;;
        --run_dir) OVERRIDE_RUN_DIR="$2"; shift ;;
        --help|-h) 
            echo "Usage: $0 [--port PORT] [--carla_root PATH] [--suffix STRING]"
            echo "Starts CARLA Server in offscreen mode and tracks its PID."
            exit 0
            ;;
        *) echo "Unknown parameter: $1"; exit 1 ;;
    esac
    shift
done

if [ -z "$CARLA_ROOT_DIR" ]; then
    echo "ERROR: --carla_root not provided and CARLA_ROOT env var not set."
    echo "Please specify the root directory where CarlaUE4.sh is located."
    exit 1
fi

CARLA_SH="${CARLA_ROOT_DIR}/CarlaUE4.sh"
if [ ! -f "$CARLA_SH" ]; then
    echo "ERROR: Could not find CarlaUE4.sh at $CARLA_SH"
    exit 1
fi
if [ ! -x "$CARLA_SH" ]; then
    echo "ERROR: $CARLA_SH is not executable. Run: chmod +x $CARLA_SH"
    exit 1
fi

# Setup Run Directory
if [ -n "$OVERRIDE_RUN_DIR" ]; then
    RUN_DIR="$OVERRIDE_RUN_DIR"
else
    TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
    RUN_DIR="$(pwd)/runs/${TIMESTAMP}${SUFFIX}"
fi
mkdir -p "$RUN_DIR"

LOG_FILE="${RUN_DIR}/server.log"
PID_FILE="${RUN_DIR}/server.pid"

echo "=========================================="
echo "Starting CARLA Server..."
echo "Port: $PORT"
echo "Run Directory: $RUN_DIR"
echo "Log File: $LOG_FILE"
echo "PID File: $PID_FILE"
echo "=========================================="

# Launch CARLA in the background
# We use nohup to prevent it from dying if the terminal closes, 
# and redirect stdout and stderr to the log file.
nohup "$CARLA_SH" -carla-rpc-port="$PORT" -RenderOffScreen > "$LOG_FILE" 2>&1 &
SERVER_PID=$!

# Save the PID
echo $SERVER_PID > "$PID_FILE"

echo "SERVER STARTED with PID: $SERVER_PID"
echo "You can monitor logs with: tail -f $LOG_FILE"
echo "To stop the server, run: ./tools/kill_server.sh --run_dir $RUN_DIR"
