#!/bin/bash
# Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
# All rights reserved. The contributor(s) of this file has/have agreed to the
# RapidStream Contributor License Agreement.
set -e

# Starting from 2024.2, Vivado has renamed rdi to xv
source /opt/tools/xilinx/Vivado/2024.2/settings64.sh

# Dump the trash generated by Vivado to /tmp
export HOME=/tmp

# Execute the argument passed to xsc
exec xsc "$@"
