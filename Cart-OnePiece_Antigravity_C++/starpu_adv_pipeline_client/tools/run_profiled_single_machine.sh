#!/bin/bash
set -e

# Save original command line
ORIG_CMD="$0 $@"

PORT=2000
CLIENT_BIN=""
CLIENT_ARGS=""

while [[ "$#" -gt 0 ]]; do
    case $1 in
        --port) PORT="$2"; shift ;;
        --run_dir) RUN_DIR="$2"; shift ;;
        --client) CLIENT_BIN="$2"; shift ;;
        --client_args) CLIENT_ARGS="$2"; shift ;;
        --help|-h) 
            echo "Usage: $0 [--port PORT] [--run_dir PATH] [--client BIN_PATH] [--client_args ARGS_STRING]"
            exit 0
            ;;
        *) echo "Unknown parameter: $1"; exit 1 ;;
    esac
    shift
done

# Auto-detect cores
TOTAL_CORES=$(nproc)
HALF=$((TOTAL_CORES / 2))
DEFAULT_SERVER="0-$((HALF - 1))"
DEFAULT_CLIENT="${HALF}-$((TOTAL_CORES - 1))"

SERVER_CORES=${SERVER_CORES:-$DEFAULT_SERVER}
CLIENT_CORES=${CLIENT_CORES:-$DEFAULT_CLIENT}

echo "=========================================="
echo "Profiled Launch (Single Machine)"
echo "Total Cores: $TOTAL_CORES"
echo "Server Cores: $SERVER_CORES"
echo "Client Cores: $CLIENT_CORES"
echo "Port: $PORT"
if [ -n "$CLIENT_BIN" ]; then
    echo "Client Bin: $CLIENT_BIN"
    echo "Client Args: $CLIENT_ARGS"
fi
echo "=========================================="

# 1. Skip server start if one is already running
if [ -n "$RUN_DIR" ]; then
    LATEST_RUN="$RUN_DIR"
    PID_FILE="${LATEST_RUN}/server.pid"
    if [ ! -f "$PID_FILE" ]; then
        echo "ERROR: Server PID file not found in $LATEST_RUN. Did run_server.sh succeed?"
        exit 1
    fi
    echo "Using injected RUN_DIR detected at $LATEST_RUN."
else
    # Fallback to auto-discovery
    LATEST_PID_FILE=$(ls -t runs/*/server.pid 2>/dev/null | head -n1 || true)
    if [ -n "$LATEST_PID_FILE" ] && kill -0 $(cat "$LATEST_PID_FILE") 2>/dev/null; then
        LATEST_RUN=$(dirname "$LATEST_PID_FILE")
        PID_FILE="$LATEST_PID_FILE"
        echo "Using existing CARLA server detected in $LATEST_RUN."
    else
        echo "No active CARLA server found. Starting a new one pinned to cores ${SERVER_CORES}..."
        taskset -c "$SERVER_CORES" ./tools/run_server.sh --port "$PORT" || { echo "Failed to start server"; exit 1; }
        sleep 3
        LATEST_RUN=$(ls -td runs/*/ 2>/dev/null | head -n1)
        LATEST_RUN=${LATEST_RUN%/}
        PID_FILE="${LATEST_RUN}/server.pid"
    fi
fi

if [ ! -f "$PID_FILE" ]; then
    echo "ERROR: Server PID file not found in $LATEST_RUN. Did run_server.sh succeed?"
    exit 1
fi

SERVER_PID=$(cat "$PID_FILE")

# 3. Start Client Program pinned
if [ -z "$CLIENT_BIN" ]; then
    CLIENT_CMD="./build/profile_client_stub --host 127.0.0.1 --port $PORT --duration 35"
else
    CLIENT_CMD="$CLIENT_BIN --host 127.0.0.1 --port $PORT $CLIENT_ARGS"
fi
echo "Starting Client pinned to Cores $CLIENT_CORES..."

# We run the client in the background temporarily just to record its PID, 
# then we `wait` on it.
export RUN_DIR="$LATEST_RUN"
if [[ "$CLIENT_ARGS" == *"--display 1"* ]]; then
    taskset -c "$CLIENT_CORES" $CLIENT_CMD &
else
    taskset -c "$CLIENT_CORES" $CLIENT_CMD > "${LATEST_RUN}/client.log" 2>&1 &
fi
CLIENT_PID=$!

echo "CLIENT STARTED with PID: $CLIENT_PID"

# 4. Store metrics
ENV_FILE="${LATEST_RUN}/pids.env"
echo "SERVER_PID=$SERVER_PID" > "$ENV_FILE"
echo "CLIENT_PID=$CLIENT_PID" >> "$ENV_FILE"
echo "RUN_DIR=$LATEST_RUN" >> "$ENV_FILE"
echo "SERVER_CORES=$SERVER_CORES" >> "$ENV_FILE"
echo "CLIENT_CORES=$CLIENT_CORES" >> "$ENV_FILE"

CMD_FILE="${LATEST_RUN}/cmdline.txt"
echo "$ORIG_CMD" > "$CMD_FILE"

ENV_TXT="${LATEST_RUN}/env.txt"
env | grep -E "STARPU|CUDA" > "$ENV_TXT" || true

echo "=========================================="
echo "Profiled launch complete."
echo "Run Dir: $LATEST_RUN"
echo "Server PID: $SERVER_PID (Cores: $SERVER_CORES)"
echo "Client PID: $CLIENT_PID (Cores: $CLIENT_CORES)"
echo "Output saved to $ENV_FILE"
echo "-> You can now run 'python3 ./tools/summarize_run.py --run_dir $LATEST_RUN' after it completes."
echo "=========================================="

# Keep the script blocking until the client finishes or the user Ctrl+C's
wait $CLIENT_PID || true
echo "Client finished execution. Run ./tools/kill_server.sh to stop CARLA."
