# Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
# All rights reserved. The contributor(s) of this file has/have agreed to the
# RapidStream Contributor License Agreement.

compile_host() {
  cd "${BATS_TEST_DIRNAME}"

  tapa g++ \
    peek-vadd-host.cpp vadd.cpp \
    -o ${BATS_TMPDIR}/vadd-host

  [ -x "${BATS_TMPDIR}/peek-vadd-host" ]
}

compile_xo() {
  cd "${BATS_TEST_DIRNAME}"

  ${RAPIDSTREAM_TAPA_HOME}/usr/bin/tapa \
    -w ${BATS_TMPDIR}/peek-vadd-workdir \
    compile \
    --platform xilinx_u250_gen3x16_xdma_4_1_202210_1 \
    -f vadd.cpp \
    -t VecAdd \
    -o ${BATS_TMPDIR}/peek-vadd.xo

  [ -f "${BATS_TMPDIR}/peek-vadd.xo" ]
}

@test "apps/peek-vadd: tapa c simulation passes" {
  compile_host
  ${BATS_TMPDIR}/peek-vadd-host
}

@test "apps/peek-vadd: tapa generates an xo file and its simulation passes" {
  compile_xo
  ${BATS_TMPDIR}/peek-vadd-host --bitstream ${BATS_TMPDIR}/peek-vadd.xo 1000
}
