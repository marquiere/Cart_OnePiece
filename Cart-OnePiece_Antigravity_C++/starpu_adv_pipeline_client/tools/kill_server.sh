#!/bin/bash
set -e

RUN_DIR=""

# Parse arguments
while [[ "$#" -gt 0 ]]; do
    case $1 in
        --run_dir) RUN_DIR="$2"; shift ;;
        --help|-h) 
            echo "Usage: $0 [--run_dir PATH]"
            echo "Stops the CARLA Server tracked in the specified run directory."
            echo "If --run_dir is omitted, defaults to the latest folder in runs/"
            exit 0
            ;;
        *) echo "Unknown parameter: $1"; exit 1 ;;
    esac
    shift
done

# If no run_dir specified, find the newest folder in runs/
if [ -z "$RUN_DIR" ]; then
    if [ ! -d "runs" ]; then
        echo "ERROR: 'runs' directory not found. Are you in the project root?"
        exit 1
    fi
    
    # ls -td sorts by time (newest first). head -n1 gets the first one.
    LATEST=$(ls -td runs/*/ 2>/dev/null | head -n1)
    if [ -z "$LATEST" ]; then
         echo "ERROR: No run directories found in 'runs/'"
         exit 1
    fi
    # Remove trailing slash for consistency
    RUN_DIR=${LATEST%/}
    echo "No --run_dir provided. Auto-selected latest: $RUN_DIR"
fi

PID_FILE="${RUN_DIR}/server.pid"

if [ ! -f "$PID_FILE" ]; then
    echo "ERROR: PID file not found at $PID_FILE"
    echo "The server might not have been started correctly, or the directory is wrong."
    # Fallback cleanup check
    if pgrep -f "CarlaUE4" > /dev/null; then
         echo "WARNING: There are still CarlaUE4 processes running on the system!"
         echo "You can kill them manually with: pkill -f CarlaUE4"
    fi
    exit 1
fi

SERVER_PID=$(cat "$PID_FILE")

if ! kill -0 $SERVER_PID 2>/dev/null; then
    echo "Process $SERVER_PID is not running."
    echo "Cleaning up PID file..."
    rm "$PID_FILE"
else
    echo "Stopping current CARLA processes gracefully..."
    kill $SERVER_PID 2>/dev/null || true
    pkill -f "CarlaUE4" 2>/dev/null || true
    
    # Wait for process to exit cleanly
    echo -n "Waiting for processes to exit."
    for i in {1..10}; do
        if pgrep -f "CarlaUE4" > /dev/null || kill -0 $SERVER_PID 2>/dev/null; then
            echo -n "."
            sleep 1
        else
            echo " gracefully stopped."
            break
        fi
    done
    
    # If it's still running after 10 seconds, force kill
    if pgrep -f "CarlaUE4" > /dev/null || kill -0 $SERVER_PID 2>/dev/null; then
        echo -e "\nProcesses taking too long. Force killing..."
        kill -9 $SERVER_PID 2>/dev/null || true
        pkill -9 -f "CarlaUE4" 2>/dev/null || true
    fi
    
    rm "$PID_FILE"
fi

echo "Verifying no leftover CarlaUE4 processes exist..."
sleep 1
if pgrep -f "CarlaUE4" > /dev/null; then
    echo "WARNING: Found leftover CarlaUE4 instances!"
    pgrep -a -f "CarlaUE4"
    echo "You may need to run: pkill -9 -f CarlaUE4"
else
    echo "Clean. No CarlaUE4 processes found."
fi

echo "Done."
