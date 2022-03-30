# run cosim or generate bitstream
TARGET=hw
# TARGET=hw_emu
# DEBUG=-g

TOP="vadd"
XO="$(pwd)/${TOP}.xo"
CONSTRAINT="$(pwd)/${TOP}_floorplan.tcl"
CONFIG_FILE="./link_config.ini"
TARGET_FREQUENCY=250
MAX_SYNTH_JOBS=16

# in our experience, the EarlyBlockPlacement strategy works better
# for designs with lots of DSPs and BRAMs
STRATEGY="Explore"
PLACEMENT_STRATEGY="EarlyBlockPlacement"

# PLATFORM=xilinx_u250_xdma_201830_2 
PLATFORM=xilinx_u280_xdma_201920_3 

# where to save the intermediate results
OUTPUT_DIR="$(pwd)/vitis_run_${TARGET}"

# check that the floorplan tcl exists
if [ ! -f "$CONSTRAINT" ]; then
    echo "no constraint file found"
    exit
fi

v++ ${DEBUG}\
  --link \
  --output "${OUTPUT_DIR}/${TOP}_${PLATFORM}.xclbin" \
  --kernel ${TOP} \
  --platform ${PLATFORM} \
  --target ${TARGET} \
  --report_level 2 \
  --temp_dir "${OUTPUT_DIR}/${TOP}_${PLATFORM}.temp" \
  --optimize 3 \
  --connectivity.nk ${TOP}:1:${TOP}_1 \
  --save-temps \
  ${XO} \
  --config ${CONFIG_FILE} \
  --kernel_frequency ${TARGET_FREQUENCY} \
  --vivado.synth.jobs ${MAX_SYNTH_JOBS} \
  --vivado.prop=run.impl_1.steps.phys_opt_design.is_enabled=1 \
  --vivado.prop run.impl_1.STEPS.PLACE_DESIGN.ARGS.DIRECTIVE=$PLACEMENT_STRATEGY \
  --vivado.prop=run.impl_1.STEPS.PHYS_OPT_DESIGN.ARGS.DIRECTIVE=$STRATEGY \
  --vivado.prop run.impl_1.STEPS.ROUTE_DESIGN.ARGS.DIRECTIVE=$STRATEGY \
  --vivado.prop run.impl_1.STEPS.OPT_DESIGN.TCL.PRE=$CONSTRAINT
