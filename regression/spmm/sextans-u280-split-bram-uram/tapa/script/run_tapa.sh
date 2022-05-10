
tapac \
  --work-dir run \
  --top Sextans \
  --part-num xcu280-fsvh2892-2L-e \
  --clock-period 3.33 \
  -o Sextans.xo \
  --floorplan-output Sextans_floorplan.tcl \
  --connectivity ../src/link_config.ini \
  --enable-hbm-binding-adjustment \
  --run-floorplan-dse \
  ../src/sextans.cpp \
   2>&1 | tee tapa.log
