include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/apps.cmake)

add_executable(shared-vadd)
target_sources(shared-vadd PRIVATE vadd-host.cpp vadd.cpp)
target_link_libraries(shared-vadd PRIVATE ${TAPA})
add_test(NAME shared-vadd COMMAND shared-vadd)

find_package(FRT)
if(FRT_FOUND)
  add_tapa_target(
    shared-vadd-hw-xo
    INPUT vadd.cpp
    FRT_INTERFACE ${CMAKE_CURRENT_BINARY_DIR}/vadd.frt.cpp
    TOP VecAddShared
    PLATFORM xilinx_u250_xdma_201830_2)

  add_xocc_hw_link_targets(
    ${CMAKE_CURRENT_BINARY_DIR}
    INPUT shared-vadd-hw-xo
    HW_EMU_XCLBIN
    hw_emu_xclbin
    HW_XCLBIN
    hw_xclbin)

  add_executable(shared-vadd-frt)
  target_include_directories(shared-vadd-frt
                             PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  target_sources(shared-vadd-frt
                 PRIVATE vadd-host.cpp ${CMAKE_CURRENT_BINARY_DIR}/vadd.frt.cpp)
  target_link_libraries(shared-vadd-frt PRIVATE ${TAPA} frt::frt)

  add_custom_target(
    shared-vadd-cosim
    COMMAND TAPAB=$<TARGET_PROPERTY:${hw_emu_xclbin},FILE_NAME>
            $<TARGET_FILE:shared-vadd-frt> 1000
    DEPENDS shared-vadd-frt ${hw_emu_xclbin}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  add_custom_target(
    shared-vadd-hw
    COMMAND TAPAB=$<TARGET_PROPERTY:${hw_xclbin},FILE_NAME>
            $<TARGET_FILE:shared-vadd-frt>
    DEPENDS shared-vadd-frt ${hw_xclbin}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

  add_test(NAME shared-vadd-cosim
           COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target
                   shared-vadd-cosim)
endif()
