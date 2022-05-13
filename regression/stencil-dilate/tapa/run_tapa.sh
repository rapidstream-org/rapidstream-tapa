#! /bin/bash

# originall tested on 2020.2
# source /opt/tools/xilinx/Vitis_HLS/2020.2/settings64.sh

WORK_DIR=run
mkdir -p ${WORK_DIR}

tapac \
  --work-dir ${WORK_DIR} \
  --platform xilinx_u280_xdma_201920_3 \
  --clock-period 3.33 \
  --other-hls-configs "config_compile -unsafe_math_optimizations" \
  \
  --top unikernel \
  -o "${WORK_DIR}/unikernel.xo" \
  --floorplan-output "${WORK_DIR}/unikernel_floorplan.tcl" \
  \
  --run-floorplan-dse \
  --enable-synth-util \
  --enable-hbm-binding-adjustment \
  \
  --connectivity ./src/unikernel.ini \
  ./src/unikernel.cpp
