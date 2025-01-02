# Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
# All rights reserved. The contributor(s) of this file has/have agreed to the
# RapidStream Contributor License Agreement.

# Stop on error
set -e

# Tapa Rapidstream running script
WORK_DIR=../tapa/generated.out/
XO_FILE=sextans1613-u55c-333MHz.xo
RS_RUN_DIR=run

# Create the working directory
mkdir -p ${WORK_DIR}

# Generate configuration files
rapidstream ./gen_config.py --max-workers 1 --max-synth-jobs 1

# Synthesizing the app to generate the XO file
tapa \
  --work-dir ${WORK_DIR} \
  compile \
  --top Sextans \
  --part-num xcu55c-fsvh2892-2L-e \
  --clock-period 3.00 \
  -o ${WORK_DIR}/${XO_FILE} \
  -f ../tapa/src/sextans-no-const-para.cpp \
  2>&1 | tee ${WORK_DIR}/tapa.log \

# Optimizing the design with RapidStream
rapidstream-tapaopt \
    --work-dir ${RS_RUN_DIR} \
    --tapa-xo-path ${WORK_DIR}/${XO_FILE} \
    --device-config ./device_config.json \
    --floorplan-config ./floorplan_config.json \
    --connectivity-ini ./link_config.ini \
    --implementation-config ./impl_config.json \
    --pp-scheme single_h_double_v \
    --run-impl \
    # --extract-metrics \
    # --skip-preprocess \
    # --skip-partition \
    # --skip-add-pipeline \
    # --skip-export \
    # --setup-single-slot-eval \
    # --single-reg \

# Report QoRs
rapidstream ../../../../utilities/report_qor.py --run-dir ${RS_RUN_DIR}
