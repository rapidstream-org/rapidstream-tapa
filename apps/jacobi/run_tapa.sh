#! /bin/bash

WORK_DIR=run
mkdir -p "${WORK_DIR}"

tapac \
  --work-dir "${WORK_DIR}" \
  --top Jacobi \
  --part-num xcu250-figd2104-2L-e \
  --clock-period 3.33 \
  -o "${WORK_DIR}/Jacobi.xo" \
  --floorplan-output "${WORK_DIR}/Jacobi_floorplan.tcl" \
  --connectivity link_config.ini \
  jacobi.cpp
