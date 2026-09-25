#!/bin/bash
set -e
echo "======================================================="
echo "Building Predictive LTE Handover Simulation (Linux/WSL)"
echo "======================================================="

mkdir -p build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

echo ""
echo "Running Simulation..."
./lte_handover_sim
