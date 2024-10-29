# Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
# All rights reserved. The contributor(s) of this file has/have agreed to the
# RapidStream Contributor License Agreement.

# This scipt demonstrates how to run the VecAdd application with RapidStream

# This script demonstrates how to csynth TAPA applications to XO files, optimize the
# design with RapidStream, and cosimulate the design with TAPA.
# To use this script, cd into the directory of this script and run
# with "source run_rs.sh". The run_rs.sh script under the rs_templetes directory
# contains the main logic for running TAPA with the RapidStream tools.

# This example demostrate end-of-transaction (EoT) tokens to denote the end of a
# data stream. This is particularly useful in dataflow optimizations where proper
# kernel termination is required.

APP=VecAdd
CSYNTH_FILE=vadd.cpp
ARGS="1000"

source ../../rs_templetes/run_rs.sh
