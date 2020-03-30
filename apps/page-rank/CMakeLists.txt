add_library(page-rank-h INTERFACE)
target_sources(page-rank-h INTERFACE page-rank.h)

add_library(page_rank_host)
target_include_directories(page_rank_host PRIVATE ../graph)
target_sources(page_rank_host PRIVATE ../graph/nxgraph.hpp page-rank-host.cpp)
target_link_libraries(page_rank_host PUBLIC page-rank-h tlp)

add_executable(page-rank)
target_sources(page-rank PRIVATE page-rank.cpp)
target_link_libraries(page-rank PUBLIC page-rank-h page_rank_host)
add_test(NAME page-rank
         COMMAND page-rank ${CMAKE_CURRENT_BINARY_DIR}/../graph/facebook.txt
                 512)

find_package(FRT)
if(FRT_FOUND)

  add_tlp_target(
    page-rank-hw-xo
    INPUT page-rank.cpp
    TOP PageRank
    #PLATFORM xilinx_u50_gen3x16_xdma_201920_3
    PLATFORM xilinx_u280_xdma_201920_3
    TLPC ${CMAKE_SOURCE_DIR}/backend/python/tlpc
    TLPCC $<TARGET_FILE:tlpcc>
    CLOCK_PERIOD 4.000)

  add_xocc_hw_link_targets(
    ${CMAKE_CURRENT_BINARY_DIR}
    --sp=PageRank.edges_0:HBM[0]
    --sp=PageRank.edges_1:HBM[2]
    --sp=PageRank.edges_2:HBM[4]
    --sp=PageRank.edges_3:HBM[6]
    # --sp=PageRank.edges_4:HBM[8]
    # --sp=PageRank.edges_5:HBM[10]
    # --sp=PageRank.edges_6:HBM[12]
    # --sp=PageRank.edges_7:HBM[14]
    --sp=PageRank.updates_0:HBM[1]
    --sp=PageRank.updates_1:HBM[3]
    --sp=PageRank.updates_2:HBM[5]
    --sp=PageRank.updates_3:HBM[7]
    # --sp=PageRank.updates_4:HBM[9]
    # --sp=PageRank.updates_5:HBM[11]
    # --sp=PageRank.updates_6:HBM[13]
    # --sp=PageRank.updates_7:HBM[15]
    --sp=PageRank.metadata:PLRAM[0]
    --sp=PageRank.degrees:HBM[16]
    --sp=PageRank.rankings:HBM[17]
    --sp=PageRank.tmps:HBM[18]
    --remote_ip_cache=$ENV{HOME}/.remote_ip_cache
    --kernel_frequency=250
    --vivado.prop=run.impl_1.STRATEGY=Performance_EarlyBlockPlacement
    --vivado.prop
    run.impl_1.STEPS.OPT_DESIGN.TCL.PRE=${CMAKE_CURRENT_SOURCE_DIR}/slr.tcl
    INPUT page-rank-hw-xo
    HW_EMU_XCLBIN hw_emu_xclbin_target
    HW_XCLBIN hw_xclbin_target)

  add_executable(page-rank-frt)
  target_sources(page-rank-frt PRIVATE page-rank-frt.cpp)
  target_link_libraries(page-rank-frt PRIVATE page-rank-h page_rank_host
                                              frt::frt)

  add_custom_target(
    page-rank-cosim
    COMMAND
      BITSTREAM=$<TARGET_PROPERTY:${hw_emu_xclbin_target},FILE_NAME>
      $<TARGET_FILE:page-rank-frt>
      ${CMAKE_CURRENT_SOURCE_DIR}/../graph/graph.txt
    DEPENDS page-rank-frt ${hw_emu_xclbin_target}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
  add_custom_target(
    page-rank-hw
    COMMAND
      BITSTREAM=$<TARGET_PROPERTY:${hw_xclbin_target},FILE_NAME>
      $<TARGET_FILE:page-rank-frt>
      ${CMAKE_CURRENT_BINARY_DIR}/../graph/facebook.txt
    DEPENDS page-rank-frt ${hw_xclbin_target}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

  add_test(NAME page-rank-cosim
           COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target
                   page-rank-cosim)

endif() # FRT_FOUND
