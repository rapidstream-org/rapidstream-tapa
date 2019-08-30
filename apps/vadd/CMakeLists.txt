add_executable(vadd)
target_sources(vadd
               PRIVATE
               vadd-host.cpp
               vadd.cpp)
target_link_libraries(vadd PRIVATE tlp)
add_test(NAME vadd COMMAND vadd)

find_package(FRT)
if(FRT_FOUND)
  add_tlp_target(vadd-tlp-hls
                 OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/vadd-tlp-hls.cpp
                 INPUT vadd.cpp
                 TLPC $<TARGET_FILE:tlpc>)

  add_xocc_targets(${CMAKE_CURRENT_BINARY_DIR}
                   KERNEL VecAdd
                   PLATFORM xilinx_u200_qdma_201910_1
                   INPUT ${CMAKE_CURRENT_BINARY_DIR}/vadd-tlp-hls.cpp
                   HLS_SRC hls_src
                   SW_EMU_XCLBIN sw_emu_xclbin
                   HW_EMU_XCLBIN hw_emu_xclbin
                   HW_XCLBIN hw_xclbin)
  add_dependencies(${hls_src} vadd-tlp-hls)

  add_executable(vadd-frt)
  target_sources(vadd-frt
                 PRIVATE
                 vadd-host.cpp
                 vadd-frt.cpp)
  target_link_libraries(vadd-frt PRIVATE frt::frt)

  add_custom_target(
    vadd-csim
    COMMAND BITSTREAM=$<TARGET_PROPERTY:${sw_emu_xclbin},FILE_NAME>
            $<TARGET_FILE:vadd-frt>
    DEPENDS vadd-frt ${sw_emu_xclbin}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
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
