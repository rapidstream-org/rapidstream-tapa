add_executable(vadd)
target_sources(vadd
               PRIVATE
               vadd-host.cpp
               vadd.cpp)
target_link_libraries(vadd PRIVATE tlp)
add_test(NAME vadd COMMAND vadd)

find_package(FRT)
if(FRT_FOUND)
  add_tlp_target(vadd-hw-xo
                 INPUT vadd.cpp
                 TOP VecAdd
                 PLATFORM xilinx_u200_qdma_201910_1
                 TLPC ${CMAKE_SOURCE_DIR}/backend/python/tlpc
                 TLPCC $<TARGET_FILE:tlpcc>)

  add_xocc_hw_link_targets(${CMAKE_CURRENT_BINARY_DIR}
                           INPUT vadd-hw-xo
                           HW_EMU_XCLBIN hw_emu_xclbin
                           HW_XCLBIN hw_xclbin)

  add_executable(vadd-frt)
  target_sources(vadd-frt
                 PRIVATE
                 vadd-host.cpp
                 vadd-frt.cpp)
  target_link_libraries(vadd-frt PRIVATE tlp frt::frt)

  add_custom_target(
    vadd-cosim
    COMMAND BITSTREAM=$<TARGET_PROPERTY:${hw_emu_xclbin},FILE_NAME>
            $<TARGET_FILE:vadd-frt> 1000
    DEPENDS vadd-frt ${hw_emu_xclbin}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  add_custom_target(vadd-hw
                    COMMAND BITSTREAM=$<TARGET_PROPERTY:${hw_xclbin},FILE_NAME>
                            $<TARGET_FILE:vadd-frt>
                    DEPENDS vadd-frt ${hw_xclbin}
                    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

  add_test(NAME vadd-csim
           COMMAND ${CMAKE_COMMAND}
                   --build
                   ${CMAKE_BINARY_DIR}
                   --target
                   vadd-csim)
  add_test(NAME vadd-cosim
           COMMAND ${CMAKE_COMMAND}
                   --build
                   ${CMAKE_BINARY_DIR}
                   --target
                   vadd-cosim)
endif()
