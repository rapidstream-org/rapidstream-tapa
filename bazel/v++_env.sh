#!/bin/bash

# Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
# All rights reserved. The contributor(s) of this file has/have agreed to the
# RapidStream Contributor License Agreement.

# set -e: Exit immediately if a command exits with a non-zero status
set -e

# Setup the Vitis environment
source /opt/tools/xilinx/Vitis/2024.1/settings64.sh
source /opt/xilinx/xrt/setup.sh

# Dump the trash generated by Vitis to /tmp
export HOME=/tmp

# Execute the argument passed to v++
exec "$@"
