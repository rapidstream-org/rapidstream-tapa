cmake_minimum_required(VERSION 3.14)

if(NOT PROJECT_NAME)
  project(tapa-apps-vadd)
endif()

find_package(gflags REQUIRED)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/apps.cmake)

add_executable(vadd)
target_sources(vadd PRIVATE vadd-host.cpp vadd.cpp)
target_link_libraries(vadd PRIVATE ${TAPA} gflags)
add_test(NAME vadd COMMAND vadd)

if(SDx_FOUND)
  add_tapa_target(
    vadd-hw-xo
    INPUT vadd.cpp
    TOP VecAdd
    CONSTRAINT ${CMAKE_CURRENT_BINARY_DIR}/constraint.tcl
    PLATFORM ${PLATFORM})

  add_xocc_hw_link_targets(
    ${CMAKE_CURRENT_BINARY_DIR}
    --vivado.prop=run.impl_1.STEPS.OPT_DESIGN.TCL.PRE=${CMAKE_CURRENT_BINARY_DIR}/constraint.tcl
    INPUT vadd-hw-xo
    HW_EMU_XCLBIN hw_emu_xclbin
    HW_XCLBIN hw_xclbin)

  add_custom_target(
    vadd-cosim
    COMMAND $<TARGET_FILE:vadd>
            --bitstream=$<TARGET_PROPERTY:${hw_emu_xclbin},FILE_NAME> 1000
    DEPENDS vadd ${hw_emu_xclbin}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

  add_custom_target(
    vadd-hw
    COMMAND $<TARGET_FILE:vadd>
            --bitstream=$<TARGET_PROPERTY:${hw_xclbin},FILE_NAME>
    DEPENDS vadd ${hw_xclbin}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

  add_test(NAME vadd-cosim COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
                                   --target vadd-cosim)
endif()
