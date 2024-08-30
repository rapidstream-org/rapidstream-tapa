# Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
# All rights reserved. The contributor(s) of this file has/have agreed to the
# RapidStream Contributor License Agreement.

compile_host() {
  cd "${BATS_TEST_DIRNAME}"

  g++ \
    -std=c++17 \
    bandwidth-host.cpp bandwidth.cpp \
    -I${RAPIDSTREAM_TAPA_HOME}/usr/include/ \
    -I${XILINX_HLS}/include/ \
    -Wl,-rpath,$(readlink -f ${RAPIDSTREAM_TAPA_HOME}/usr/lib/) \
    -L ${RAPIDSTREAM_TAPA_HOME}/usr/lib/ \
    -ltapa -lfrt -lglog -lgflags -l:libOpenCL.so.1 -ltinyxml2 -lstdc++fs \
    -o ${BATS_TMPDIR}/host

  [ -x "${BATS_TMPDIR}/host" ]
}

compile_xo() {
  cd "${BATS_TEST_DIRNAME}"

  ${RAPIDSTREAM_TAPA_HOME}/usr/bin/tapa \
    -w ${BATS_TMPDIR}/bandwidth-workdir \
    compile \
    --platform xilinx_u250_gen3x16_xdma_4_1_202210_1 \
    -f bandwidth.cpp \
    -t Bandwidth \
    -o ${BATS_TMPDIR}/bandwidth.xo

  [ -f "${BATS_TMPDIR}/bandwidth.xo" ]
}

compile_xclbin() {
  cd "${BATS_TMPDIR}"

  v++ \
    --link \
    -t hw_emu \
    --platform xilinx_u250_gen3x16_xdma_4_1_202210_1 \
    -o bandwidth.xclbin \
    bandwidth.xo

  [ -f "${BATS_TMPDIR}/bandwidth.xclbin" ]
}

@test "apps/bandwidth: tapa c simulation passes" {
  compile_host
  ${BATS_TMPDIR}/host
}

@test "apps/bandwidth: tapa generated xclbin emulation passes" {
  compile_xo
  compile_xclbin
  source /opt/xilinx/xrt/setup.sh
  ${BATS_TMPDIR}/host --bitstream ${BATS_TMPDIR}/bandwidth.xclbin
}
