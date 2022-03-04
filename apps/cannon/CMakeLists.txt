include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/apps.cmake)

add_executable(cannon)
target_sources(cannon PRIVATE cannon-host.cpp cannon.cpp)
target_link_libraries(cannon PRIVATE ${TAPA})
add_test(NAME cannon COMMAND cannon)

find_package(SDx)
if(SDx_FOUND)
  add_tapa_target(
    cannon-hw-xo
    INPUT cannon.cpp
    TOP Cannon
    PLATFORM ${PLATFORM})

  add_xocc_hw_link_targets(
    ${CMAKE_CURRENT_BINARY_DIR}
    INPUT cannon-hw-xo
    HW_EMU_XCLBIN
    hw_emu_xclbin
    HW_XCLBIN
    hw_xclbin)

  add_custom_target(
    cannon-cosim
    COMMAND $<TARGET_FILE:cannon>
            --bitstream=$<TARGET_PROPERTY:${hw_emu_xclbin},FILE_NAME>
    DEPENDS cannon ${hw_emu_xclbin}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  add_custom_target(
    cannon-hw
    COMMAND $<TARGET_FILE:cannon>
            --bitstream=$<TARGET_PROPERTY:${hw_xclbin},FILE_NAME>
    DEPENDS cannon ${hw_xclbin}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

  add_test(NAME cannon-cosim COMMAND ${CMAKE_COMMAND} --build
                                     ${CMAKE_BINARY_DIR} --target cannon-cosim)
endif()
