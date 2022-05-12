WORK_DIR=run
mkdir -p "${WORK_DIR}"

tapac \
  --work-dir "${WORK_DIR}" \
  --top kernel3 \
  --platform xilinx_u250_xdma_201830_2 \
  --clock-period 3.33 \
  -o "${WORK_DIR}/kernel3.xo" \
  --floorplan-output "${WORK_DIR}/kernel3_floorplan.tcl" \
  --connectivity ./src/link_config.ini \
  --floorplan-strategy QUICK_FLOORPLANNING \
  --enable-synth-util \
  ./src/tapa_kernel.cpp
