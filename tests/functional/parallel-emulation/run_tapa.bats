# Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
# All rights reserved. The contributor(s) of this file has/have agreed to the
# RapidStream Contributor License Agreement.

compile_host() {
  cd "${BATS_TEST_DIRNAME}"

  tapa g++ \
    ../../apps/cannon/cannon-host.cpp cannon.cpp \
    -o ${BATS_TMPDIR}/parallel-emulation-host

  [ -x "${BATS_TMPDIR}/parallel-emulation-host" ]
}

compile_zip() {
  cd "${BATS_TEST_DIRNAME}"

  tapa \
    -w ${BATS_TMPDIR}/parallel-emulation-workdir \
    compile \
    --jobs 2 \
    --platform xilinx_u250_gen3x16_xdma_4_1_202210_1 \
    -f cannon.cpp \
    -t ProcElem \
    --target xilinx-hls \
    -o ${BATS_TMPDIR}/parallel-emulation.zip

  [ -f "${BATS_TMPDIR}/parallel-emulation.zip" ]
}

@test "apps/parallel-emulation: tapa c simulation passes" {
  compile_host
  ${BATS_TMPDIR}/parallel-emulation-host
}

@test "apps/parallel-emulation: tapa generates a zip file and its simulation passes" {
  compile_zip
  ${BATS_TMPDIR}/parallel-emulation-host \
    --proc_elem_bitstream ${BATS_TMPDIR}/parallel-emulation.zip
}
