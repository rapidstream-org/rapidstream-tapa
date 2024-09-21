# Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
# All rights reserved. The contributor(s) of this file has/have agreed to the
# RapidStream Contributor License Agreement.

WORK_DIR=generated

tapa \
  --work-dir ${WORK_DIR} \
  compile \
  --top Sextans \
  --part-num xcu55c-fsvh2892-2L-e \
  --clock-period 3.33 \
  --connectivity src/link_config.ini \
  -o ${WORK_DIR}/sextans1613-u55c.xo \
  -f src/sextans-no-const-para.cpp \
  2>&1 | tee ${WORK_DIR}/tapa.log
