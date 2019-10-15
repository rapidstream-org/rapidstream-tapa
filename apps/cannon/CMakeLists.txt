add_executable(cannon)
target_sources(cannon
               PRIVATE
               cannon-host.cpp
               cannon.cpp)
target_link_libraries(cannon PRIVATE tlp)
add_test(NAME cannon COMMAND cannon)

find_package(FRT)
if(FRT_FOUND)
  add_tlp_target(cannon-hw-xo
                 INPUT cannon.cpp
                 TOP Cannon
                 PLATFORM xilinx_u200_qdma_201910_1
                 TLPC ${CMAKE_SOURCE_DIR}/backend/python/tlpc
                 TLPCC $<TARGET_FILE:tlpcc>)

  add_xocc_hw_link_targets(${CMAKE_CURRENT_BINARY_DIR}
                           INPUT cannon-hw-xo
                           HW_EMU_XCLBIN hw_emu_xclbin
                           HW_XCLBIN hw_xclbin)

  add_executable(cannon-frt)
  target_sources(cannon-frt
                 PRIVATE
                 cannon-host.cpp
                 cannon-frt.cpp)
  target_link_libraries(cannon-frt PRIVATE tlp frt::frt)

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
