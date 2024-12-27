# Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
# All rights reserved. The contributor(s) of this file has/have agreed to the
# RapidStream Contributor License Agreement.

tapa \
    --work-dir generated.out \
    compile \
    --top kernel0 \
    --part-num xcu250-figd2104-2L-e \
    --clock-period 3.00 \
    -f ./src/kernel_kernel.cpp \
    -o "generated/10x13-u250.xo" \
    2>&1 | tee tapa.log
