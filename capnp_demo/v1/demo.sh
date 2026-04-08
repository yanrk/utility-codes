#!/bin/bash
# Demo script - Start Core and multiple Exts

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"

# Check if built
if [ ! -f "${BUILD_DIR}/core" ] || [ ! -f "${BUILD_DIR}/ext" ]; then
    echo "Please run ./build.sh first to build the project"
    exit 1
fi

cleanup() {
    echo ""
    echo "Stopping all processes..."
    kill $CORE_PID $EXT1_PID $EXT2_PID $EXT3_PID 2>/dev/null
    wait 2>/dev/null
    echo "Stopped"
    exit 0
}

trap cleanup SIGINT SIGTERM

echo "==================================="
echo "  Cap'n Proto RPC Demo"
echo "==================================="
echo ""

# Start Core
echo "Starting Core (127.0.0.1:9000)..."
"${BUILD_DIR}/core" "127.0.0.1:9000" &
CORE_PID=$!
sleep 1

# Start multiple Exts
echo "Starting Ext1 (127.0.0.1:9001)..."
"${BUILD_DIR}/ext" ext1 9001 127.0.0.1:9000 "Extension Node 1" &
EXT1_PID=$!
sleep 0.5

echo "Starting Ext2 (127.0.0.1:9002)..."
"${BUILD_DIR}/ext" ext2 9002 127.0.0.1:9000 "Extension Node 2" &
EXT2_PID=$!
sleep 0.5

echo "Starting Ext3 (127.0.0.1:9003)..."
"${BUILD_DIR}/ext" ext3 9003 127.0.0.1:9000 "Extension Node 3" &
EXT3_PID=$!

echo ""
echo "All services started!"
echo "  Core PID: ${CORE_PID}"
echo "  Ext1 PID: ${EXT1_PID}"
echo "  Ext2 PID: ${EXT2_PID}"
echo "  Ext3 PID: ${EXT3_PID}"
echo ""
echo "Press Ctrl+C to stop all services"
echo ""

# Wait for processes
wait
