# Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
# All rights reserved. The contributor(s) of this file has/have agreed to the
# RapidStream Contributor License Agreement.

WORK_DIR=generated

tapa \
  --work-dir ${WORK_DIR} \
  compile \
  --top serpens \
  --part-num xcu280-fsvh2892-2L-e \
  --connectivity src/link_config.ini \
  --clock-period 3.33 \
  -o ${WORK_DIR}/serpens32.xo \
  -f src/serpens_tapa.cpp \
  2>&1 | tee ${WORK_DIR}/tapa.log
