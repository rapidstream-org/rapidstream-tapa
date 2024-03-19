include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/apps.cmake)

add_executable(nested-vadd)
target_sources(nested-vadd PRIVATE vadd-host.cpp vadd.cpp)
target_link_libraries(nested-vadd PRIVATE ${TAPA} gflags)
add_test(NAME nested-vadd COMMAND nested-vadd)

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
    nested-vadd-hw-xo
    INPUT vadd.cpp
    TOP VecAddNested
    CONNECTIVITY ${CMAKE_CURRENT_SOURCE_DIR}/link_config.ini
    PLATFORM ${PLATFORM})

  add_xocc_hw_link_targets(
    ${CMAKE_CURRENT_BINARY_DIR}
    --config=${CMAKE_CURRENT_SOURCE_DIR}/link_config.ini
    INPUT nested-vadd-hw-xo
    HW_EMU_XCLBIN
    hw_emu_xclbin
    HW_XCLBIN
    hw_xclbin)

  add_custom_target(
    nested-vadd-cosim
    COMMAND $<TARGET_FILE:nested-vadd> 1000
            --bitstream=$<TARGET_PROPERTY:${hw_emu_xclbin},FILE_NAME>
    DEPENDS nested-vadd ${hw_emu_xclbin}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  add_custom_target(
    nested-vadd-hw
    COMMAND $<TARGET_FILE:nested-vadd>
            --bitstream=$<TARGET_PROPERTY:${hw_xclbin},FILE_NAME>
    DEPENDS nested-vadd ${hw_xclbin}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

  add_test(NAME nested-vadd-cosim COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
      --target nested-vadd-cosim)
endif()
