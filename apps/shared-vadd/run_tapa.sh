WORK_DIR=run
mkdir -p ${WORK_DIR}

tapac \
  --work-dir ${WORK_DIR} \
  --top VecAddShared \
  --platform xilinx_u250_xdma_201830_2 \
  --clock-period 3.33 \
  -o ${WORK_DIR}/VecAddShared.xo \
  --floorplan-output ${WORK_DIR}/VecAddShared_floorplan.tcl \
  --connectivity link_config.ini \
  vadd.cpp \
   2>&1 | tee ${WORK_DIR}/tapa.log
