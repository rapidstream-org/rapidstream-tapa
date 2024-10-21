# Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
# All rights reserved. The contributor(s) of this file has/have agreed to the
# RapidStream Contributor License Agreement.

compile_host() {
  cd "${BATS_TEST_DIRNAME}"

  tapa g++ \
    vadd-host.cpp vadd.cpp \
    -o ${BATS_TMPDIR}/stream-vadd-host

  [ -x "${BATS_TMPDIR}/stream-vadd-host" ]
}

compile_xo() {
  cd "${BATS_TEST_DIRNAME}"

  ${RAPIDSTREAM_TAPA_HOME}/usr/bin/tapa \
    -w ${BATS_TMPDIR}/stream-vadd-workdir \
    compile \
    --platform xilinx_u250_gen3x16_xdma_4_1_202210_1 \
    -f vadd.cpp \
    -t VecAdd \
    -o ${BATS_TMPDIR}/stream-vadd.xo

  [ -f "${BATS_TMPDIR}/stream-vadd.xo" ]
}

@test "apps/stream-vadd: tapa c simulation passes" {
  compile_host
  ${BATS_TMPDIR}/stream-vadd-host
}

@test "apps/stream-vadd: tapa generates an xo file and passes simulation" {
  compile_xo
  vivado -mode batch -source VecAdd_sim.tcl -tclargs ${BATS_TMPDIR}/stream-vadd-workdir
}
