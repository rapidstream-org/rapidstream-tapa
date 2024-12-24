# Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
# All rights reserved. The contributor(s) of this file has/have agreed to the
# RapidStream Contributor License Agreement.

compile_host() {
  cd "${BATS_TEST_DIRNAME}"

  tapa g++ \
    vadd-host.cpp vadd.cpp \
    -o ${BATS_TMPDIR}/custom-rtl-host

  [ -x "${BATS_TMPDIR}/custom-rtl-host" ]
}

compile_xo() {
  cd "${BATS_TEST_DIRNAME}"

  ${RAPIDSTREAM_TAPA_HOME}/usr/bin/tapa \
    -w ${BATS_TMPDIR}/custom-rtl-workdir \
    compile \
    --jobs 2 \
    --platform xilinx_u250_gen3x16_xdma_4_1_202210_1 \
    -f vadd.cpp \
    -t VecAdd \
    -o ${BATS_TMPDIR}/custom-rtl.xo \
    --add-rtl rtl

  [ -f "${BATS_TMPDIR}/custom-rtl.xo" ]
}

@test "apps/custom-rtl-template-lower: tapa generates an xo file and its simulation passes" {
  cd "${BATS_TEST_DIRNAME}"

  ${RAPIDSTREAM_TAPA_HOME}/usr/bin/tapa \
    -w ${BATS_TMPDIR}/custom-rtl-workdir-1 \
    compile \
    --jobs 2 \
    --platform xilinx_u250_gen3x16_xdma_4_1_202210_1 \
    -f vadd.cpp \
    -t VecAdd \
    -o ${BATS_TMPDIR}/custom-rtl-1.xo \
    --gen-template Add
}

@test "apps/custom-rtl-template-upper: tapa generates an xo file and its simulation passes" {
  cd "${BATS_TEST_DIRNAME}"

  ${RAPIDSTREAM_TAPA_HOME}/usr/bin/tapa \
    -w ${BATS_TMPDIR}/custom-rtl-workdir-2 \
    compile \
    --jobs 2 \
    --platform xilinx_u250_gen3x16_xdma_4_1_202210_1 \
    -f vadd.cpp \
    -t VecAdd \
    -o ${BATS_TMPDIR}/custom-rtl-2.xo \
    --gen-template Add_Upper
}

@test "apps/custom-rtl: tapa c simulation passes" {
  compile_host
  ${BATS_TMPDIR}/custom-rtl-host
}

@test "apps/custom-rtl: tapa generates an xo file and its simulation passes" {
  compile_xo
  ${BATS_TMPDIR}/custom-rtl-host --bitstream ${BATS_TMPDIR}/custom-rtl.xo 1000
}
