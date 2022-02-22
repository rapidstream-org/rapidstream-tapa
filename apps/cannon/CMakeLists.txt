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
    FRT_INTERFACE ${CMAKE_CURRENT_BINARY_DIR}/cannon.frt.cpp
    TOP Cannon
    PLATFORM ${PLATFORM})

  add_xocc_hw_link_targets(
    ${CMAKE_CURRENT_BINARY_DIR}
    INPUT cannon-hw-xo
    HW_EMU_XCLBIN
    hw_emu_xclbin
    HW_XCLBIN
    hw_xclbin)

  add_executable(cannon-frt)
  target_include_directories(cannon-frt PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  target_sources(cannon-frt PRIVATE cannon-host.cpp
                                    ${CMAKE_CURRENT_BINARY_DIR}/cannon.frt.cpp)
  target_link_libraries(cannon-frt PRIVATE ${TAPA} frt::frt)

  add_custom_target(
    cannon-cosim
    COMMAND TAPAB=$<TARGET_PROPERTY:${hw_emu_xclbin},FILE_NAME>
            $<TARGET_FILE:cannon-frt>
    DEPENDS cannon-frt ${hw_emu_xclbin}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  add_custom_target(
    cannon-hw
    COMMAND TAPAB=$<TARGET_PROPERTY:${hw_xclbin},FILE_NAME>
            $<TARGET_FILE:cannon-frt>
    DEPENDS cannon-frt ${hw_xclbin}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

  add_test(NAME cannon-cosim COMMAND ${CMAKE_COMMAND} --build
                                     ${CMAKE_BINARY_DIR} --target cannon-cosim)
endif()
