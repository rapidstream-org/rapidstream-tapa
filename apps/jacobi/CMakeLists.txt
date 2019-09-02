add_executable(jacobi)
target_sources(jacobi
               PRIVATE
               jacobi-host.cpp
               jacobi.cpp)
target_link_libraries(jacobi PRIVATE tlp)
add_test(NAME jacobi COMMAND jacobi)

find_package(FRT)
if(FRT_FOUND)
  add_tlp_target(jacobi-tlp-hls
                 OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/jacobi-tlp-hls.cpp
                 INPUT jacobi.cpp
                 TLPC $<TARGET_FILE:tlpc>)

  add_xocc_targets(${CMAKE_CURRENT_BINARY_DIR}
                   KERNEL Jacobi
                   PLATFORM xilinx_u200_qdma_201910_1
                   INPUT ${CMAKE_CURRENT_BINARY_DIR}/jacobi-tlp-hls.cpp
                   HLS_SRC hls_src
                   SW_EMU_XCLBIN sw_emu_xclbin
                   HW_EMU_XCLBIN hw_emu_xclbin
                   HW_XCLBIN hw_xclbin)
  add_dependencies(${hls_src} jacobi-tlp-hls)

  add_executable(jacobi-frt)
  target_sources(jacobi-frt
                 PRIVATE
                 jacobi-host.cpp
                 jacobi-frt.cpp)
  target_link_libraries(jacobi-frt PRIVATE frt::frt)

  add_custom_target(
    jacobi-csim
    COMMAND BITSTREAM=$<TARGET_PROPERTY:${sw_emu_xclbin},FILE_NAME>
            $<TARGET_FILE:jacobi-frt>
    DEPENDS jacobi-frt ${sw_emu_xclbin}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  add_custom_target(
    jacobi-cosim
    COMMAND BITSTREAM=$<TARGET_PROPERTY:${hw_emu_xclbin},FILE_NAME>
            $<TARGET_FILE:jacobi-frt> 20
    DEPENDS jacobi-frt ${hw_emu_xclbin}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  add_custom_target(jacobi-hw
                    COMMAND BITSTREAM=$<TARGET_PROPERTY:${hw_xclbin},FILE_NAME>
                            $<TARGET_FILE:jacobi-frt>
                    DEPENDS jacobi-frt ${hw_xclbin}
                    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

  add_test(NAME jacobi-csim
           COMMAND ${CMAKE_COMMAND}
                   --build
                   ${CMAKE_BINARY_DIR}
                   --target
                   jacobi-csim)
  add_test(NAME jacobi-cosim
           COMMAND ${CMAKE_COMMAND}
                   --build
                   ${CMAKE_BINARY_DIR}
                   --target
                   jacobi-cosim)
endif()
