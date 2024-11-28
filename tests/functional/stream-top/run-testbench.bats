# Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
# All rights reserved. The contributor(s) of this file has/have agreed to the
# RapidStream Contributor License Agreement.

compile_xo_axis() {
  cd "${BATS_TEST_DIRNAME}"

  tapa \
    -w ${BATS_TMPDIR}/stream-top-axis-workdir \
    compile \
    --jobs 1 \
    --platform xilinx_u250_gen3x16_xdma_4_1_202210_1 \
    -f vadd.cpp \
    -t VecAdd \
    -o ${BATS_TMPDIR}/stream-top.xo

  [ -f "${BATS_TMPDIR}/stream-top.xo" ]
}

compile_hls() {
  cd "${BATS_TEST_DIRNAME}"

  tapa \
    -w ${BATS_TMPDIR}/stream-top-hls-workdir \
    compile \
    --jobs 1 \
    --no-vitis-mode \
    --platform xilinx_u250_gen3x16_xdma_4_1_202210_1 \
    -f vadd.cpp \
    -t VecAdd

  [ -d "${BATS_TMPDIR}/stream-top-hls-workdir" ]
}

@test "functional/stream-top: tapa generates an xo file for axis and passes simulation" {
  compile_xo_axis

  # Test the generated RTL using testbench
  vivado \
    -mode batch \
    -source testbench/sim.tcl \
    -tclargs \
      ${BATS_TMPDIR}/stream-top-axis-workdir \
      testbench/axis-tb.sv
}

@test "functional/stream-top: tapa generates rtl for hls stream and passes simulation" {
  compile_hls
  vivado \
    -mode batch \
    -source testbench/sim.tcl \
    -tclargs \
      ${BATS_TMPDIR}/stream-top-hls-workdir \
      testbench/hls-stream-tb.sv
}
