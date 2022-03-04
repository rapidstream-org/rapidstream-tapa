include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/apps.cmake)

add_executable(jacobi)
target_sources(jacobi PRIVATE jacobi-host.cpp jacobi.cpp)
target_link_libraries(jacobi PRIVATE ${TAPA})
add_test(NAME jacobi COMMAND jacobi)

find_package(SDx)
if(SDx_FOUND)
  add_tapa_target(
    jacobi-hw-xo
    INPUT jacobi.cpp
    TOP Jacobi
    PLATFORM ${PLATFORM})

  add_xocc_hw_link_targets(
    ${CMAKE_CURRENT_BINARY_DIR}
    INPUT jacobi-hw-xo
    HW_EMU_XCLBIN
    hw_emu_xclbin
    HW_XCLBIN
    hw_xclbin)

  add_custom_target(
    jacobi-cosim
    COMMAND $<TARGET_FILE:jacobi> 20
            --bitstream=$<TARGET_PROPERTY:${hw_emu_xclbin},FILE_NAME>
    DEPENDS jacobi ${hw_emu_xclbin}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  add_custom_target(
    jacobi-hw
    COMMAND $<TARGET_FILE:jacobi>
            --bitstream=$<TARGET_PROPERTY:${hw_xclbin},FILE_NAME>
    DEPENDS jacobi ${hw_xclbin}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

  add_test(NAME jacobi-cosim COMMAND ${CMAKE_COMMAND} --build
                                     ${CMAKE_BINARY_DIR} --target jacobi-cosim)
endif()
