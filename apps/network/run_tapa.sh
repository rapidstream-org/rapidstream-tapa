WORK_DIR=run
mkdir -p ${WORK_DIR}

tapac \
  --work-dir ${WORK_DIR} \
  --top Network \
  --platform xilinx_u250_xdma_201830_2 \
  --clock-period 3.33 \
  -o ${WORK_DIR}/Network.xo \
  --floorplan-output ${WORK_DIR}/Network_floorplan.tcl \
  --connectivity link_config.ini \
  network.cpp \
   2>&1 | tee ${WORK_DIR}/tapa.log
