#!/bin/bash
LATEST_PID_FILE=$(ls -t runs/*/server.pid 2>/dev/null | head -n1 || true)
echo "LATEST_PID_FILE=$LATEST_PID_FILE"
if [ -n "$LATEST_PID_FILE" ] && kill -0 $(cat "$LATEST_PID_FILE") 2>/dev/null; then
    LATEST_RUN=$(dirname "$LATEST_PID_FILE")
    echo "Using active CARLA server detected in $LATEST_RUN."
else
    echo "No active server"
fi
