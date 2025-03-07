# Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
# All rights reserved. The contributor(s) of this file has/have agreed to the
# RapidStream Contributor License Agreement.

# This scipt demonstrates how to generate ab_graph for the VecAdd application

APP=VecAdd
CSYNTH_FILE=vadd.cpp
ARGS="1000"

# Synthesizing the app to generate the XO file
/home/Ed-5100/rapidstream-tapa/bazel-bin/tapa/tapa \
    --work-dir build \
    compile \
    --top $APP \
    --part-num xcu250-figd2104-2L-e \
    --clock-period 3.33 \
    -o build/$APP.xo \
    -f $CSYNTH_FILE \
    --enable-synth-util \
    --gen-ab-graph \
    --flatten-hierarchy
