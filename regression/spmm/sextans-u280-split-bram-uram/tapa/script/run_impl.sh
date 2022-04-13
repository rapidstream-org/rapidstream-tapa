# run cosim or generate bitstream
TARGET=hw
# TARGET=hw_emu
# DEBUG=-g

TOP="Sextans"
XO="$(pwd)/Sextans.xo"
CONSTRAINT="$(pwd)/Sextans_floorplan.tcl"
CONFIG_FILE="../src/link_config.ini"
TARGET_FREQUENCY=300
STRATEGY="Explore"
PLACEMENT_STRATEGY="EarlyBlockPlacement"
SDA_VER=2021.2
MAX_SYNTH_JOBS=8

# PLATFORM=xilinx_u250_xdma_201830_2
PLATFORM=xilinx_u280_xdma_201920_3

OUTPUT_DIR="$(pwd)/vitis_run_${TARGET}"

if [ ! -f "$CONSTRAINT" ]; then
    echo "no constraint file found"
    exit
fi

SDA_VER=${SDA_VER} v++ ${DEBUG}\
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
  --vivado.prop run.impl_1.STEPS.PHYS_OPT_DESIGN.is_enabled=1 \
  --vivado.prop run.impl_1.STEPS.OPT_DESIGN.ARGS.DIRECTIVE=${STRATEGY} \
  --vivado.prop run.impl_1.STEPS.PLACE_DESIGN.ARGS.DIRECTIVE=EarlyBlockPlacement \
  --vivado.prop run.impl_1.STEPS.PHYS_OPT_DESIGN.ARGS.DIRECTIVE=${STRATEGY} \
  --vivado.prop run.impl_1.STEPS.ROUTE_DESIGN.ARGS.DIRECTIVE=${STRATEGY} \
  --vivado.prop run.impl_1.STEPS.OPT_DESIGN.TCL.PRE=$CONSTRAINT
