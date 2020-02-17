add_executable(bandwidth)
target_sources(bandwidth PRIVATE bandwidth-host.cpp bandwidth.cpp)
target_link_libraries(bandwidth PUBLIC tlp)
add_test(NAME bandwidth COMMAND bandwidth)

find_package(FRT)
if(FRT_FOUND)
  add_tlp_target(
    bandwidth-hw-xo
    INPUT bandwidth.cpp
    TOP Bandwidth
    PLATFORM xilinx_u250_xdma_201830_2
    TLPC ${CMAKE_SOURCE_DIR}/backend/python/tlpc
    TLPCC $<TARGET_FILE:tlpcc>)

  add_xocc_hw_link_targets(
    ${CMAKE_CURRENT_BINARY_DIR}
    INPUT bandwidth-hw-xo
    HW_EMU_XCLBIN
    hw_emu_xclbin
    HW_XCLBIN
    hw_xclbin
      --sp
      Bandwidth.m_axi_chan0:DDR[0]
      --sp
      Bandwidth.m_axi_chan1:DDR[1]
      --sp
      Bandwidth.m_axi_chan2:DDR[2]
      --sp
      Bandwidth.m_axi_chan3:DDR[3]
      --vivado.prop
      run.impl_1.STEPS.OPT_DESIGN.TCL.PRE=${CMAKE_CURRENT_SOURCE_DIR}/slr.tcl)

  add_executable(bandwidth-frt)
  target_sources(bandwidth-frt PRIVATE bandwidth-host.cpp bandwidth-frt.cpp)
  target_link_libraries(bandwidth-frt PRIVATE tlp frt::frt)

  add_custom_target(
    bandwidth-cosim
    COMMAND BITSTREAM=$<TARGET_PROPERTY:${hw_emu_xclbin},FILE_NAME>
            $<TARGET_FILE:bandwidth-frt> 1000
    DEPENDS bandwidth-frt ${hw_emu_xclbin}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  add_custom_target(
    bandwidth-hw
    COMMAND BITSTREAM=$<TARGET_PROPERTY:${hw_xclbin},FILE_NAME>
            $<TARGET_FILE:bandwidth-frt>
    DEPENDS bandwidth-frt ${hw_xclbin}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

  add_test(NAME bandwidth-cosim
           COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target
                   bandwidth-cosim)
endif()
