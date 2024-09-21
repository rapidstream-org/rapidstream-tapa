# Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
# All rights reserved. The contributor(s) of this file has/have agreed to the
# RapidStream Contributor License Agreement.

WORK_DIR=generated

tapa \
  --work-dir ${WORK_DIR} \
  compile \
  --top Callipepla \
  --part-num xcu55c-fsvh2892-2L-e \
  --clock-period 3.33 \
  --connectivity src_noconst/link_config.ini \
  --read-only-args edge_list_ptr \
  --read-only-args edge_list_ch* \
  --read-only-args vec_digA \
  --write-only-args vec_res \
  -o ${WORK_DIR}/callipepla-u55c.xo \
  -f src_noconst/callipepla.cpp \
  2>&1 | tee ${WORK_DIR}/tapa.log

# change `src/serpens.cpp` to `src/serpens-noconst.cpp` to generate the design without passing constants through handshake interfaces.
