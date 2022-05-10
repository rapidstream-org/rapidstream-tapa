# 2020.2 results in smaller LUT usage
source /opt/tools/xilinx/Vitis_HLS/2020.2/settings64.sh

tapac \
  --work-dir run \
  --top Knn \
  --part-num xcu280-fsvh2892-2L-e \
  --clock-period 3.33 \
  -o knn.xo \
  --enable-synth-util \
  --floorplan-output knn_floorplan.tcl \
  --connectivity ../src/knn.ini \
  --enable-hbm-binding-adjustment \
  --run-floorplan-dse \
  --max-parallel-synth-jobs 16 \
  ../src/knn.cpp
