# Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
# All rights reserved. The contributor(s) of this file has/have agreed to the
# RapidStream Contributor License Agreement.

# Tapa Rapidstream running script template

# This script demonstrates how to csynth TAPA applications to XO files, optimize the
# design with RapidStream, and cosimulate the design with TAPA.
# To use this script, look into the run_rs.sh file in each app.

# Generate config files
python ../../rs_templetes/gen_config.py

# Synthesizing the app to generate the XO file
tapa \
  --work-dir build \
  compile \
  --top $APP \
  --part-num xcu250-figd2104-2L-e \
  --clock-period 3.33 \
  -o build/$APP.xo \
  -f $CSYNTH_FILE

# Optimizing the design with RapidStream
rapidstream-tapaopt \
  --tapa-xo-path build/$APP.xo \
  --work-dir rs_build \
  --device-config config_build/device_config.json \
  --floorplan-config config_build/floorplan_config.json

# Compile the host with TAPA
tapa \
  g++ \
  ./*.cpp \
  -o $APP

# Cosim the design with TAPA
./$APP --bitstream rs_build/dse/solution_0/updated.xo $ARGS
