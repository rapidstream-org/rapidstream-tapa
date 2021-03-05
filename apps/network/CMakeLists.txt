include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/apps.cmake)

add_executable(network)
target_sources(network PRIVATE network-host.cpp network.cpp)
target_link_libraries(network PRIVATE ${TAPA})
add_test(NAME network COMMAND network)

find_package(FRT)
if(FRT_FOUND)
  add_tapa_target(
    network-hw-xo
    INPUT network.cpp
    FRT_INTERFACE ${CMAKE_CURRENT_BINARY_DIR}/network.frt.cpp
    TOP Network
    PLATFORM xilinx_u250_xdma_201830_2)

  add_xocc_hw_link_targets(
    ${CMAKE_CURRENT_BINARY_DIR}
    INPUT network-hw-xo
    HW_EMU_XCLBIN
    hw_emu_xclbin
    HW_XCLBIN
    hw_xclbin)

  add_executable(network-frt)
  target_include_directories(network-frt PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  target_sources(
    network-frt PRIVATE network-host.cpp
                        ${CMAKE_CURRENT_BINARY_DIR}/network.frt.cpp)
  target_link_libraries(network-frt PRIVATE ${TAPA} frt::frt)

  add_custom_target(
    network-cosim
    COMMAND TAPAB=$<TARGET_PROPERTY:${hw_emu_xclbin},FILE_NAME>
            $<TARGET_FILE:network-frt>
    DEPENDS network-frt ${hw_emu_xclbin}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  add_custom_target(
    network-hw
    COMMAND TAPAB=$<TARGET_PROPERTY:${hw_xclbin},FILE_NAME>
            $<TARGET_FILE:network-frt>
    DEPENDS network-frt ${hw_xclbin}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

  add_test(NAME network-cosim
           COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target
                   network-cosim)
endif()
