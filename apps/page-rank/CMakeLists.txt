add_library(page_rank_host)
target_include_directories(page_rank_host PRIVATE ../graph)
target_sources(page_rank_host PRIVATE ../graph/nxgraph.hpp page-rank-host.cpp)
target_link_libraries(page_rank_host PUBLIC tlp)

add_executable(page-rank)
target_sources(page-rank PRIVATE page-rank.cpp)
target_link_libraries(page-rank PUBLIC page_rank_host)
add_test(NAME page-rank
         COMMAND page-rank ${CMAKE_CURRENT_BINARY_DIR}/../graph/facebook.txt
                 512)

find_package(FRT)
if(FRT_FOUND)
  add_tlp_target(
    page-rank-hw-xo
    INPUT page-rank.cpp
    TOP PageRank
    PLATFORM xilinx_u200_qdma_201910_1
    TLPC ${CMAKE_SOURCE_DIR}/backend/python/tlpc
    TLPCC $<TARGET_FILE:tlpcc>)

  add_xocc_hw_link_targets(
    ${CMAKE_CURRENT_BINARY_DIR}
    INPUT page-rank-hw-xo
    HW_EMU_XCLBIN
    hw_emu_xclbin
    HW_XCLBIN
    hw_xclbin)

  add_executable(page-rank-frt)
  target_sources(page-rank-frt PRIVATE page-rank-frt.cpp)
  target_link_libraries(page-rank-frt PRIVATE page_rank_host frt::frt)

  add_custom_target(
    page-rank-cosim
    COMMAND BITSTREAM=$<TARGET_PROPERTY:${hw_emu_xclbin},FILE_NAME>
            $<TARGET_FILE:page-rank-frt>
            ${CMAKE_CURRENT_SOURCE_DIR}/../graph/graph.txt
    DEPENDS page-rank-frt ${hw_emu_xclbin}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  add_custom_target(
    page-rank-hw
    COMMAND BITSTREAM=$<TARGET_PROPERTY:${hw_xclbin},FILE_NAME>
            $<TARGET_FILE:page-rank-frt>
            ${CMAKE_CURRENT_BINARY_DIR}/../graph/facebook.txt
    DEPENDS page-rank-frt ${hw_xclbin}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

  add_test(NAME page-rank-cosim
           COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target
                   page-rank-cosim)
endif()
