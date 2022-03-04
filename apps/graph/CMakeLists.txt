include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/apps.cmake)

add_executable(graph)
target_sources(graph PRIVATE nxgraph.hpp graph-host.cpp graph.cpp)
target_link_libraries(graph PUBLIC ${TAPA})
file(
  DOWNLOAD "https://snap.stanford.edu/data/facebook_combined.txt.gz"
  ${CMAKE_CURRENT_BINARY_DIR}/facebook.txt.gz
  EXPECTED_HASH
    "SHA512=54b65fcdd41cad3e96e52f7ca60f326bea117cc8997ad4ec1831053fafa067fae9e728916b81a1fcf41a24de19e416b78bbfb14a3824c6e2bf67704dd7f89ad3"
)
execute_process(
  INPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/facebook.txt.gz
  COMMAND gzip -cd
  COMMAND sed "s/^0 /4039 /"
  OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/facebook.txt)
add_test(NAME graph COMMAND graph ${CMAKE_CURRENT_BINARY_DIR}/facebook.txt 512)

find_package(SDx)
if(SDx_FOUND)
  add_tapa_target(
    graph-hw-xo
    INPUT graph.cpp
    TOP Graph
    PLATFORM ${PLATFORM})

  add_xocc_hw_link_targets(
    ${CMAKE_CURRENT_BINARY_DIR}
    INPUT graph-hw-xo
    HW_EMU_XCLBIN
    hw_emu_xclbin
    HW_XCLBIN
    hw_xclbin)

  add_custom_target(
    graph-cosim
    COMMAND $<TARGET_FILE:graph> ${CMAKE_CURRENT_SOURCE_DIR}/graph.txt
            --bitstream=$<TARGET_PROPERTY:${hw_emu_xclbin},FILE_NAME>
    DEPENDS graph ${hw_emu_xclbin}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  add_custom_target(
    graph-hw
    COMMAND $<TARGET_FILE:graph> ${CMAKE_CURRENT_BINARY_DIR}/facebook.txt
            --bitstream=$<TARGET_PROPERTY:${hw_xclbin},FILE_NAME>
    DEPENDS graph ${hw_xclbin}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

  add_test(NAME graph-cosim COMMAND ${CMAKE_COMMAND} --build
                                    ${CMAKE_BINARY_DIR} --target graph-cosim)
endif()
