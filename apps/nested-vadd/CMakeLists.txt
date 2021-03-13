include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/apps.cmake)

add_executable(nested-vadd)
target_sources(nested-vadd PRIVATE vadd-host.cpp vadd.cpp)
target_link_libraries(nested-vadd PRIVATE ${TAPA})
add_test(NAME nested-vadd COMMAND nested-vadd)

find_package(SDx)
if(SDx_FOUND)
  add_tapa_target(
    nested-vadd-hw-xo
    INPUT vadd.cpp
    FRT_INTERFACE ${CMAKE_CURRENT_BINARY_DIR}/vadd.frt.cpp
    TOP VecAddNested
    PLATFORM xilinx_u250_xdma_201830_2)

  add_xocc_hw_link_targets(
    ${CMAKE_CURRENT_BINARY_DIR}
    INPUT nested-vadd-hw-xo
    HW_EMU_XCLBIN
    hw_emu_xclbin
    HW_XCLBIN
    hw_xclbin)

  add_executable(nested-vadd-frt)
  target_include_directories(nested-vadd-frt PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  target_sources(nested-vadd-frt PRIVATE vadd-host.cpp
      ${CMAKE_CURRENT_BINARY_DIR}/vadd.frt.cpp)
  target_link_libraries(nested-vadd-frt PRIVATE ${TAPA} frt::frt)

  add_custom_target(
    nested-vadd-cosim
    COMMAND TAPAB=$<TARGET_PROPERTY:${hw_emu_xclbin},FILE_NAME>
            $<TARGET_FILE:nested-vadd-frt> 1000
    DEPENDS nested-vadd-frt ${hw_emu_xclbin}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  add_custom_target(
    nested-vadd-hw
    COMMAND TAPAB=$<TARGET_PROPERTY:${hw_xclbin},FILE_NAME>
            $<TARGET_FILE:nested-vadd-frt>
    DEPENDS nested-vadd-frt ${hw_xclbin}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

  add_test(NAME nested-vadd-cosim COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
      --target nested-vadd-cosim)
endif()
