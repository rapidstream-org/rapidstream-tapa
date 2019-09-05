add_executable(cannon)
target_sources(cannon
               PRIVATE
               cannon-host.cpp
               cannon.cpp)
target_link_libraries(cannon PRIVATE tlp)
add_test(NAME cannon COMMAND cannon)

find_package(FRT)
if(FRT_FOUND)
  add_tlp_target(cannon-tlp-hls
                 OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/cannon-tlp-hls.cpp
                 INPUT cannon.cpp
                 TLPC $<TARGET_FILE:tlpc>)

  add_xocc_targets(${CMAKE_CURRENT_BINARY_DIR}
                   KERNEL Cannon
                   PLATFORM xilinx_u200_qdma_201910_1
                   INPUT ${CMAKE_CURRENT_BINARY_DIR}/cannon-tlp-hls.cpp
                   HLS_SRC hls_src
                   HW_EMU_XCLBIN hw_emu_xclbin
                   HW_XCLBIN hw_xclbin)
  add_dependencies(${hls_src} cannon-tlp-hls)

  add_executable(cannon-frt)
  target_sources(cannon-frt
                 PRIVATE
                 cannon-host.cpp
                 cannon-frt.cpp)
  target_link_libraries(cannon-frt PRIVATE frt::frt)

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
