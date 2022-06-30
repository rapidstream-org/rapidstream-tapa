cmake_minimum_required(VERSION 3.14)

if(NOT PROJECT_NAME)
  project(tapa-apps-mv)
endif()

find_package(gflags REQUIRED)

if(NOT PLATFORM)
  set(PLATFORM
    xilinx_u250_xdma_201830_2
    CACHE STRING "Target FPGA platform")
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/apps.cmake)

add_executable(mv)
target_sources(mv PRIVATE mv-host.cpp mv-opt.cpp)
target_link_libraries(mv PRIVATE ${TAPA} gflags)
add_test(NAME mv COMMAND mv)



find_package(SDx)
if(SDx_FOUND)
  if(${PLATFORM} EQUAL xilinx_u250_xdma_201830_2
     OR ${PLATFORM} EQUAL xilinx_u280_xdma_201920_3
  )
    list(APPEND TAPA_ARGS --floorplan-output ${CMAKE_CURRENT_BINARY_DIR}/constraint.tcl)
    list(
      APPEND
      VPP_ARGS
      --vivado.prop=run.impl_1.STEPS.OPT_DESIGN.TCL.PRE=${CMAKE_CURRENT_BINARY_DIR}/constraint.tcl
    )
  endif()

  add_tapa_target(
    mv-hw-xo
    INPUT mv-opt.cpp
    TOP mv
    CONNECTIVITY ${CMAKE_CURRENT_SOURCE_DIR}/link_config.ini
    PLATFORM ${PLATFORM})

  add_xocc_hw_link_targets(
    ${CMAKE_CURRENT_BINARY_DIR}
    --config=${CMAKE_CURRENT_SOURCE_DIR}/link_config.ini
    INPUT mv-hw-xo
    HW_EMU_XCLBIN
    hw_emu_xclbin
    HW_XCLBIN
    hw_xclbin)

  add_custom_target(
    mv-cosim
    COMMAND $<TARGET_FILE:mv>
            --bitstream=$<TARGET_PROPERTY:${hw_emu_xclbin},FILE_NAME>
    DEPENDS mv ${hw_emu_xclbin}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  add_custom_target(
    mv-hw
    COMMAND $<TARGET_FILE:mv>
            --bitstream=$<TARGET_PROPERTY:${hw_xclbin},FILE_NAME>
    DEPENDS mv ${hw_xclbin}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

  add_test(NAME mv-cosim COMMAND ${CMAKE_COMMAND} --build
                                     ${CMAKE_BINARY_DIR} --target mv-cosim)
endif()
