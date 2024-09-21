# Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
# All rights reserved. The contributor(s) of this file has/have agreed to the
# RapidStream Contributor License Agreement.

WORK_DIR=generated

tapa \
  --work-dir ${WORK_DIR} \
  compile \
  --top Serpens \
  --part-num xcu55c-fsvh2892-2L-e \
  --clock-period 3.33 \
  --connectivity src/link_config.ini \
  -o ${WORK_DIR}/serpens24-u55c.xo \
  -f src/serpens-noconst.cpp \
  2>&1 | tee ${WORK_DIR}/tapa.log

# change `src/serpens.cpp` to `src/serpens-noconst.cpp` to generate the design without passing constants through handshake interfaces.
