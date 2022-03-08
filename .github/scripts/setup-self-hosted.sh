#!/bin/bash
set -e

sudo apt-get update
sudo apt-get install -y --no-install-recommends \
  faketime \
  gcc-multilib \
  iverilog \
  libc6-dev-i386 \
  libncurses5 \
  libtinfo5 \
  libxext6 \
  unzip \
  xrt

# Vivado doesn't like running concurrently for the first time.
source "${XILINX_VIVADO}"/settings64.sh
vivado -mode batch -nojournal -nolog
