#! /bin/bash

WORK_DIR=run
mkdir -p "${WORK_DIR}"

tapac \
  --work-dir "${WORK_DIR}" \
  --top Bandwidth \
  --part-num xcu250-figd2104-2L-e \
  --clock-period 3.33 \
  -o "${WORK_DIR}/Bandwidth.xo" \
  --floorplan-output "${WORK_DIR}/Bandwidth_floorplan.tcl" \
  --connectivity link_config.ini \
  bandwidth.cpp
