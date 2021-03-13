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
    FRT_INTERFACE ${CMAKE_CURRENT_BINARY_DIR}/jacobi.frt.cpp
    TOP Jacobi
    PLATFORM xilinx_u250_xdma_201830_2)

  add_xocc_hw_link_targets(
    ${CMAKE_CURRENT_BINARY_DIR}
    INPUT jacobi-hw-xo
    HW_EMU_XCLBIN
    hw_emu_xclbin
    HW_XCLBIN
    hw_xclbin)

  add_executable(jacobi-frt)
  target_include_directories(jacobi-frt PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  target_sources(jacobi-frt PRIVATE jacobi-host.cpp
                                    ${CMAKE_CURRENT_BINARY_DIR}/jacobi.frt.cpp)
  target_link_libraries(jacobi-frt PRIVATE ${TAPA} frt::frt)

  add_custom_target(
    jacobi-cosim
    COMMAND TAPAB=$<TARGET_PROPERTY:${hw_emu_xclbin},FILE_NAME>
            $<TARGET_FILE:jacobi-frt> 20
    DEPENDS jacobi-frt ${hw_emu_xclbin}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  add_custom_target(
    jacobi-hw
    COMMAND TAPAB=$<TARGET_PROPERTY:${hw_xclbin},FILE_NAME>
            $<TARGET_FILE:jacobi-frt>
    DEPENDS jacobi-frt ${hw_xclbin}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

  add_test(NAME jacobi-cosim COMMAND ${CMAKE_COMMAND} --build
                                     ${CMAKE_BINARY_DIR} --target jacobi-cosim)
endif()
