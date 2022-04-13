
tapac \
  --work-dir run \
  --top Sextans \
  --part-num xcu280-fsvh2892-2L-e \
  --clock-period 3.33 \
  --run-tapacc \
  --run-hls \
  --generate-task-rtl \
  --run-floorplanning \
  --generate-top-rtl \
  --pack-xo \
  -o Sextans.xo \
  --constraint Sextans_floorplan.tcl \
  --connectivity ../src/link_config.ini \
  --reuse-hbm-path-pipelining \
  --floorplan-opt-priority SLR_CROSSING_PRIORITIZED \
  ../src/sextans.cpp \
   2>&1 | tee tapa.log
