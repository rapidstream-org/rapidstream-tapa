#! /bin/bash

WORK_DIR=run
mkdir -p "${WORK_DIR}"

tapac \
  --work-dir "${WORK_DIR}" \
  --top spmv \
  --platform xilinx_u280_xdma_201920_3 \
  --clock-period 3.33 \
  --read-only-args "matrix_hbm.*" \
  --write-only-args "packed_dense_.*" \
  --run-floorplan-dse \
  --enable-synth-util \
  --enable-hbm-binding-adjustment \
  -o "${WORK_DIR}/spmv.xo" \
  --floorplan-output "${WORK_DIR}/spmv_floorplan.tcl" \
  --connectivity ./src/spmv.ini \
  ./src/spmv.cpp
