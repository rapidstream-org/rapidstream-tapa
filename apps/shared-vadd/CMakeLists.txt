include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/apps.cmake)

add_executable(shared-vadd)
target_sources(shared-vadd PRIVATE vadd-host.cpp vadd.cpp)
target_link_libraries(shared-vadd PRIVATE ${TAPA})
add_test(NAME shared-vadd COMMAND shared-vadd)

find_package(SDx)
if(SDx_FOUND)
  add_tapa_target(
    shared-vadd-hw-xo
    INPUT vadd.cpp
    TOP VecAddShared
    PLATFORM ${PLATFORM})

  add_xocc_hw_link_targets(
    ${CMAKE_CURRENT_BINARY_DIR}
    INPUT shared-vadd-hw-xo
    HW_EMU_XCLBIN
    hw_emu_xclbin
    HW_XCLBIN
    hw_xclbin)

  add_custom_target(
    shared-vadd-cosim
    COMMAND $<TARGET_FILE:shared-vadd>
            --bitstream=$<TARGET_PROPERTY:${hw_emu_xclbin},FILE_NAME> 1000
    DEPENDS shared-vadd ${hw_emu_xclbin}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  )
  add_custom_target(
    shared-vadd-hw
    COMMAND $<TARGET_FILE:shared-vadd>
            --bitstream=$<TARGET_PROPERTY:${hw_xclbin},FILE_NAME>
    DEPENDS shared-vadd ${hw_xclbin}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  )

  add_test(NAME shared-vadd-cosim
           COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target
                   shared-vadd-cosim)
endif()
