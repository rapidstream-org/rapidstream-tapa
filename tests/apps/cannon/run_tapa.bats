# Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
# All rights reserved. The contributor(s) of this file has/have agreed to the
# RapidStream Contributor License Agreement.

compile_host() {
  cd "${BATS_TEST_DIRNAME}"

  g++ \
    -std=c++17 \
    cannon-host.cpp cannon.cpp \
    -I${RAPIDSTREAM_TAPA_HOME}/usr/include/ \
    -I${XILINX_HLS}/include/ \
    -Wl,-rpath,$(readlink -f ${RAPIDSTREAM_TAPA_HOME}/usr/lib/) \
    -L ${RAPIDSTREAM_TAPA_HOME}/usr/lib/ \
    -ltapa -lcontext -lthread -lfrt -lglog -lgflags -l:libOpenCL.so.1 -ltinyxml2 -lstdc++fs \
    -o ${BATS_TMPDIR}/cannon-host

  [ -x "${BATS_TMPDIR}/cannon-host" ]
}

compile_xo() {
  cd "${BATS_TEST_DIRNAME}"

  ${RAPIDSTREAM_TAPA_HOME}/usr/bin/tapa \
    -w ${BATS_TMPDIR}/cannon-workdir \
    compile \
    --platform xilinx_u250_gen3x16_xdma_4_1_202210_1 \
    -f cannon.cpp \
    -t Cannon \
    -o ${BATS_TMPDIR}/cannon.xo

  [ -f "${BATS_TMPDIR}/cannon.xo" ]
}

@test "apps/cannon: tapa c simulation passes" {
  compile_host
  ${BATS_TMPDIR}/cannon-host
}

@test "apps/cannon: tapa generates an xo file and its simulation passes" {
  compile_xo
  ${BATS_TMPDIR}/cannon-host --bitstream ${BATS_TMPDIR}/cannon.xo
}
