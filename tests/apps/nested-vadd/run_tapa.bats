# Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
# All rights reserved. The contributor(s) of this file has/have agreed to the
# RapidStream Contributor License Agreement.

compile_host() {
  cd "${BATS_TEST_DIRNAME}"

  g++ \
    -std=c++17 \
    vadd-host.cpp vadd.cpp \
    -I${RAPIDSTREAM_TAPA_HOME}/usr/include/ \
    -I${XILINX_HLS}/include/ \
    -Wl,-rpath,$(readlink -f ${RAPIDSTREAM_TAPA_HOME}/usr/lib/) \
    -L ${RAPIDSTREAM_TAPA_HOME}/usr/lib/ \
    -ltapa -lcontext -lfrt -lglog -lgflags -l:libOpenCL.so.1 -ltinyxml2 -lstdc++fs \
    -o ${BATS_TMPDIR}/nested-vadd-host

  [ -x "${BATS_TMPDIR}/nested-vadd-host" ]
}

compile_xo() {
  cd "${BATS_TEST_DIRNAME}"

  ${RAPIDSTREAM_TAPA_HOME}/usr/bin/tapa \
    -w ${BATS_TMPDIR}/nested-vadd-workdir \
    compile \
    --platform xilinx_u250_gen3x16_xdma_4_1_202210_1 \
    -f vadd.cpp \
    -t VecAddNested \
    -o ${BATS_TMPDIR}/nested-vadd.xo

  [ -f "${BATS_TMPDIR}/nested-vadd.xo" ]
}

@test "apps/nested-vadd: tapa c simulation passes" {
  compile_host
  ${BATS_TMPDIR}/nested-vadd-host
}

@test "apps/nested-vadd: tapa generates an xo file and its simulation passes" {
  compile_xo
  ${BATS_TMPDIR}/nested-vadd-host --bitstream ${BATS_TMPDIR}/nested-vadd.xo 1000
}
