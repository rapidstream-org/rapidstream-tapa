#!/bin/bash
set -e

sudo apt-get update
sudo apt-get install -y --no-install-recommends \
  iverilog \
  python3-venv \

# Vivado doesn't like running concurrently for the first time.
source "${XILINX_VIVADO}"/settings64.sh
vivado -mode batch -nojournal -nolog
