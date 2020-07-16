include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/apps.cmake)

add_executable(cannon)
target_sources(cannon
               PRIVATE
               cannon-host.cpp
               cannon.cpp)
target_link_libraries(cannon PRIVATE ${TLP})
add_test(NAME cannon COMMAND cannon)

find_package(FRT)
if(FRT_FOUND)
  add_tlp_target(cannon-hw-xo
                 INPUT cannon.cpp
                 TOP Cannon
                 PLATFORM xilinx_u250_xdma_201830_2)

  add_xocc_hw_link_targets(${CMAKE_CURRENT_BINARY_DIR}
                           INPUT cannon-hw-xo
                           HW_EMU_XCLBIN hw_emu_xclbin
                           HW_XCLBIN hw_xclbin)

  add_executable(cannon-frt)
  target_sources(cannon-frt
                 PRIVATE
                 cannon-host.cpp
                 cannon-frt.cpp)
  target_link_libraries(cannon-frt PRIVATE ${TLP} frt::frt)

  add_custom_target(
    cannon-cosim
    COMMAND BITSTREAM=$<TARGET_PROPERTY:${hw_emu_xclbin},FILE_NAME>
            $<TARGET_FILE:cannon-frt>
    DEPENDS cannon-frt ${hw_emu_xclbin}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  add_custom_target(cannon-hw
                    COMMAND BITSTREAM=$<TARGET_PROPERTY:${hw_xclbin},FILE_NAME>
                            $<TARGET_FILE:cannon-frt>
                    DEPENDS cannon-frt ${hw_xclbin}
                    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

  add_test(NAME cannon-cosim
           COMMAND ${CMAKE_COMMAND}
                   --build
                   ${CMAKE_BINARY_DIR}
                   --target
                   cannon-cosim)
endif()
