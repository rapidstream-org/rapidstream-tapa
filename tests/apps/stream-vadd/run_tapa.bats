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

compile_xo_axis() {
  cd "${BATS_TEST_DIRNAME}"

  tapa \
    -w ${BATS_TMPDIR}/stream-vadd-axis-workdir \
    compile \
    --platform xilinx_u250_gen3x16_xdma_4_1_202210_1 \
    -f vadd.cpp \
    -t VecAdd \
    -o ${BATS_TMPDIR}/stream-vadd.xo

  [ -f "${BATS_TMPDIR}/stream-vadd.xo" ]
}

compile_hls() {
  cd "${BATS_TEST_DIRNAME}"

  tapa \
    -w ${BATS_TMPDIR}/stream-vadd-hls-workdir \
    compile \
    --no-vitis-mode \
    --platform xilinx_u250_gen3x16_xdma_4_1_202210_1 \
    -f vadd.cpp \
    -t VecAdd

  [ -d "${BATS_TMPDIR}/stream-vadd-hls-workdir" ]
}

@test "apps/stream-vadd: tapa c simulation passes" {
  compile_host
  ${BATS_TMPDIR}/stream-vadd-host
}

@test "apps/stream-vadd: tapa generates an xo file for axis and passes simulation" {
  compile_xo_axis
  vivado \
    -mode batch \
    -source testbench/sim.tcl \
    -tclargs \
      ${BATS_TMPDIR}/stream-vadd-axis-workdir \
      testbench/axis-tb.sv
  ${BATS_TMPDIR}/stream-vadd-host --bitstream ${BATS_TMPDIR}/stream-vadd.xo
}

@test "apps/stream-vadd: tapa generates rtl for hls stream and passes simulation" {
  compile_hls
  vivado \
    -mode batch \
    -source testbench/sim.tcl \
    -tclargs \
      ${BATS_TMPDIR}/stream-vadd-hls-workdir \
      testbench/hls-stream-tb.sv
}
