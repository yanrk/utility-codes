#!/bin/bash
# Start Core service

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${SCRIPT_DIR}/build"

CORE_ADDRESS="${1:-127.0.0.1:9000}"

echo "Starting Core service..."
echo "Listen address: ${CORE_ADDRESS}"
echo ""

exec "${BUILD_DIR}/core" "${CORE_ADDRESS}"
