#!/bin/bash
# Start Ext service
# Usage: ./run_ext.sh <ext_id> <ext_port> [core_address] [description]

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"

EXT_ID="${1:-ext1}"
EXT_PORT="${2:-9001}"
CORE_ADDRESS="${3:-127.0.0.1:9000}"
DESCRIPTION="${4:-Extension ${EXT_ID}}"

echo "Starting Ext service..."
echo "  ID: ${EXT_ID}"
echo "  Port: ${EXT_PORT}"
echo "  Core address: ${CORE_ADDRESS}"
echo ""

exec "${BUILD_DIR}/ext" "${EXT_ID}" "${EXT_PORT}" "${CORE_ADDRESS}" "${DESCRIPTION}"
