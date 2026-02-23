#!/bin/bash
set -e

RUN_DIR=""
DURATION=30

while [[ "$#" -gt 0 ]]; do
    case $1 in
        --run_dir) RUN_DIR="$2"; shift ;;
        --duration|--time) DURATION="$2"; shift ;;
        --help|-h) 
            echo "Usage: $0 [--run_dir PATH] [--duration SECONDS]"
            echo "Monitors CPU/GPU load for the server and client processes."
            exit 0
            ;;
        *) echo "Unknown parameter: $1"; exit 1 ;;
    esac
    shift
done

if [ -z "$RUN_DIR" ]; then
    if [ ! -d "runs" ]; then echo "ERROR: runs/ missing."; exit 1; fi
    LATEST=$(ls -td runs/*/ 2>/dev/null | head -n1)
    if [ -z "$LATEST" ]; then echo "ERROR: No folders in runs/."; exit 1; fi
    RUN_DIR=${LATEST%/}
    echo "No --run_dir provided. Auto-selected latest: $RUN_DIR"
fi

ENV_FILE="${RUN_DIR}/pids.env"
if [ ! -f "$ENV_FILE" ]; then
    echo "ERROR: $ENV_FILE not found. Did you run run_profiled_single_machine?"
    exit 1
fi

source "$ENV_FILE"

if [ -z "$SERVER_PID" ] || [ -z "$CLIENT_PID" ]; then
    echo "ERROR: SERVER_PID or CLIENT_PID missing from $ENV_FILE"
    exit 1
fi

if ! kill -0 "$SERVER_PID" 2>/dev/null; then
    echo "ERROR: Server PID $SERVER_PID is not running."
    exit 1
fi

if ! kill -0 "$CLIENT_PID" 2>/dev/null; then
    echo "ERROR: Client PID $CLIENT_PID is not running. It might have finished."
    exit 1
fi

echo "Monitoring SERVER_PID=$SERVER_PID and CLIENT_PID=$CLIENT_PID for ${DURATION}s..."

META_FILE="${RUN_DIR}/monitor_meta.txt"
echo "--- Monitoring Metadata ---" > "$META_FILE"

# 1. CPU Monitor (sysstat required)
OUT_SERVER="${RUN_DIR}/monitor_cpu_server.log"
OUT_CLIENT="${RUN_DIR}/monitor_cpu_client.log"

if command -v pidstat > /dev/null; then
    echo "Using pidstat for CPU monitoring..."
    echo "CPU_METHOD=pidstat" >> "$META_FILE"
    pidstat -p "$SERVER_PID" 1 "$DURATION" > "$OUT_SERVER" 2>&1 &
    PID_S=$!
    pidstat -p "$CLIENT_PID" 1 "$DURATION" > "$OUT_CLIENT" 2>&1 &
    PID_C=$!
else
    echo "WARNING: pidstat not found (install sysstat). Using top fallback..."
    echo "CPU_METHOD=top" >> "$META_FILE"
    top -b -d 1 -n "$DURATION" -p "$SERVER_PID" > "$OUT_SERVER" 2>&1 &
    PID_S=$!
    top -b -d 1 -n "$DURATION" -p "$CLIENT_PID" > "$OUT_CLIENT" 2>&1 &
    PID_C=$!
fi

# 2. GPU Monitor
OUT_GPU="${RUN_DIR}/monitor_gpu.log"
if command -v nvidia-smi > /dev/null; then
    # Test if pmon has permissions
    if nvidia-smi pmon -s um -c 1 > /dev/null 2>&1; then
        echo "Using nvidia-smi pmon for process-level GPU stats..."
        echo "GPU_METHOD=pmon" >> "$META_FILE"
        # Extract headers and lines matching our PIDs
        nvidia-smi pmon -s um -d 1 -c "$DURATION" | grep -E "PID|$SERVER_PID|$CLIENT_PID|# gpu" > "$OUT_GPU" 2>&1 &
        PID_G=$!
    else
        echo "WARNING: pmon lacks permissions. Falling back to plain smi sampling..."
        echo "GPU_METHOD=compute-apps" >> "$META_FILE"
        (
            for ((i=0; i<DURATION; i++)); do
                nvidia-smi --query-compute-apps=pid,process_name,used_memory --format=csv >> "$OUT_GPU" 2>/dev/null || true
                sleep 1
            done
        ) &
        PID_G=$!
    fi
else
    echo "WARNING: nvidia-smi not found. GPU monitoring skipped."
    echo "GPU_METHOD=none" >> "$META_FILE"
fi

# Wait for monitoring processes to exit
wait $PID_S 2>/dev/null || true
wait $PID_C 2>/dev/null || true
if [ -n "$PID_G" ]; then wait $PID_G 2>/dev/null || true; fi

echo "Monitoring complete!"
echo "- Server CPU: $OUT_SERVER"
echo "- Client CPU: $OUT_CLIENT"
echo "- GPU Stats:  $OUT_GPU"
