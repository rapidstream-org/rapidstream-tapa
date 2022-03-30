TOP_NAME="vadd"
CLOCK_PERIOD=3.33
SOURCE_FILE=./vadd.cpp

# Currently we support two devices: U250 and U280
# DEVICE_NAME="xcu250-figd2104-2L-e"
DEVICE_NAME="xcu280-fsvh2892-2L-e"

# where to save intermediate results
WORK_DIR=./tapa_run

# add the mapping from each logical port to physical port
# Refer to reference_config_script_flow.ini
# note that the format of the config file in the script flow
# is different from the cmake flow.
# Format for script-based flow:
#
# [connectivity]
# sp=${TOP_NAME}_1.${argument1}:DDR[0]
# sp=${TOP_NAME}_1.${argument2}:HBM[31]
#
CONFIG_FILE=./vadd.ini

# where to save the floorplan results
FLOORPLAN_OUTPUT=${TOP_NAME}_floorplan.tcl

# invoke tapac to compile each task and floorplan the design
# tapac will execute as follows:
# step1: run-tapacc: source-to-source transformation to generate the Vitis HLS C++ for each task
# step2: run-hls: call HLS to synthesize each task in parallel
# step3: generate-task-rtl: extract the output of HLS and construct the rtl for each non-top task
# step4: run-floorplanning: invoke AutoBridge to floorplan the design
# step5: generate-top-rtl: generate the glue logic to connect all non-top tasks
# step6: pack-xo: box up all related files into an xilin xo object
#
# --enable-synth-util will instruct the tool to run logic synthesis
# of each task to get more accurate area information
#
# If you remove --run-tapacc, tapac will start from step2
# Likewise, if you further remove --run-hls, tapac will start from step3
#
# The output of tapac is (1) an xo object, (2) floorplan constraints
# Refer to generate_bitstream.sh about how to start the physical implementation
tapac \
  --work-dir ${WORK_DIR} \
  --top ${TOP_NAME} \
  --part-num ${DEVICE_NAME} \
  --clock-period ${CLOCK_PERIOD} \
  --run-tapacc \
  --run-hls \
  --generate-task-rtl \
  --run-floorplanning \
  --generate-top-rtl \
  --pack-xo \
  --enable-synth-util \
  -o ${TOP_NAME}.xo \
  --constraint ${FLOORPLAN_OUTPUT} \
  --connectivity ${CONFIG_FILE} \
  ${SOURCE_FILE}
