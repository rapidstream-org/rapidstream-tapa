WORK_DIR=run
mkdir -p ${WORK_DIR}

tapac \
  --work-dir ${WORK_DIR} \
  --top VecAddNested \
  --platform xilinx_u250_xdma_201830_2 \
  --clock-period 3.33 \
  -o ${WORK_DIR}/VecAddNested.xo \
  --floorplan-output ${WORK_DIR}/VecAddNested_floorplan.tcl \
  --connectivity link_config.ini \
  vadd.cpp \
   2>&1 | tee ${WORK_DIR}/tapa.log
