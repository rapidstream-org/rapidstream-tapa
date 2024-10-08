# Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
# All rights reserved. The contributor(s) of this file has/have agreed to the
# RapidStream Contributor License Agreement.

# This scipt demonstrates how to run the VecAdd application with RapidStream

# This script demonstrates how to csynth TAPA applications to XO files, optimize the
# design with RapidStream, and cosimulate the design with TAPA.
# To use this script, cd into the directory of this script and run
# with "source run_rs.sh". The run_rs.sh script under the rs_templetes directory
# contains the main logic for running TAPA with the RapidStream tools.

# This test demostrate the usage of detached task in TAPA. This allows the top-level
# to return regardless of the completion of the detached task. It is especially useful
# for implementing pure data driven tasks.

APP=VecAddDetach
CSYNTH_FILE=vadd.cpp
ARGS="1000"

source ../../rs_templetes/run_rs.sh
