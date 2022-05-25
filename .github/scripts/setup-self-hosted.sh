#!/bin/bash
set -e

wget -O- https://raw.github.com/Blaok/fpga-runtime/master/install.sh | bash

sudo apt-get update
sudo apt-get install -y --no-install-recommends \
  cmake \
  moreutils
sudo apt-get purge -y \
  parallel

# Vivado doesn't like running concurrently for the first time.
source "${XILINX_VIVADO}"/settings64.sh
vivado -mode batch -nojournal -nolog
