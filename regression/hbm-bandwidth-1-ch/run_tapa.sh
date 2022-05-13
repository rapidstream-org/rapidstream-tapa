#! /bin/bash

WORK_DIR=run
mkdir -p "${WORK_DIR}"

tapac \
  --work-dir "${WORK_DIR}" \
  --top Bandwidth \
  --platform xilinx_u280_xdma_201920_3 \
  --clock-period 3.33 \
  -o "${WORK_DIR}/Bandwidth.xo" \
  --floorplan-output "${WORK_DIR}/Bandwidth_floorplan.tcl" \
  --connectivity ./src/link_config.ini \
  ./src/bandwidth.cpp
