# Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
# All rights reserved. The contributor(s) of this file has/have agreed to the
# RapidStream Contributor License Agreement.

compile_host() {
  cd "${BATS_TEST_DIRNAME}"

  g++ \
    -std=c++17 \
    graph-host.cpp graph.cpp \
    -I${RAPIDSTREAM_TAPA_HOME}/usr/include/ \
    -I${XILINX_HLS}/include/ \
    -Wl,-rpath,$(readlink -f ${RAPIDSTREAM_TAPA_HOME}/usr/lib/) \
    -L ${RAPIDSTREAM_TAPA_HOME}/usr/lib/ \
    -ltapa -lfrt -lglog -lgflags -l:libOpenCL.so.1 -ltinyxml2 -lstdc++fs \
    -o ${BATS_TMPDIR}/graph-host

  [ -x "${BATS_TMPDIR}/graph-host" ]
}

compile_xo() {
  cd "${BATS_TEST_DIRNAME}"

  ${RAPIDSTREAM_TAPA_HOME}/usr/bin/tapa \
    -w ${BATS_TMPDIR}/graph-workdir \
    compile \
    --platform xilinx_u250_gen3x16_xdma_4_1_202210_1 \
    -f graph.cpp \
    -t Graph \
    -o ${BATS_TMPDIR}/graph.xo

  [ -f "${BATS_TMPDIR}/graph.xo" ]
}

@test "apps/graph: tapa c simulation passes" {
  compile_host
  ${BATS_TMPDIR}/graph-host graph.txt
}

@test "apps/graph: tapa generates an xo file" {
  compile_xo
}
