# 2020.2 results in smaller LUT usage
source /opt/tools/xilinx/Vitis_HLS/2020.2/settings64.sh

tapac \
  --work-dir run \
  --top Knn \
  --part-num xcu280-fsvh2892-2L-e \
  --clock-period 3.33 \
  -o knn.xo \
  --enable-synth-util \
  --run-tapacc \
  --run-hls \
  --generate-task-rtl \
  --run-floorplanning \
  --generate-top-rtl \
  --pack-xo \
  --constraint knn_floorplan.tcl \
  --connectivity ../src/knn.ini \
  --reuse-hbm-path-pipelining \
  --floorplan-strategy SLR_LEVEL_FLOORPLANNING \
  --max-parallel-synth-jobs 16 \
  ../src/knn.cpp
