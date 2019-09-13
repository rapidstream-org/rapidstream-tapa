add_executable(graph)
target_sources(graph
               PRIVATE
               nxgraph.hpp
               graph-host.cpp
               graph.cpp)
target_link_libraries(graph PUBLIC tlp)
file(
  DOWNLOAD "https://snap.stanford.edu/data/facebook_combined.txt.gz"
  ${CMAKE_CURRENT_BINARY_DIR}/facebook.txt.gz
  EXPECTED_HASH
    "SHA512=54b65fcdd41cad3e96e52f7ca60f326bea117cc8997ad4ec1831053fafa067fae9e728916b81a1fcf41a24de19e416b78bbfb14a3824c6e2bf67704dd7f89ad3"
  )
execute_process(INPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/facebook.txt.gz
                COMMAND gzip -cd
                COMMAND sed "s/^0 /4039 /"
                OUTPUT_FILE ${CMAKE_CURRENT_BINARY_DIR}/facebook.txt)
add_test(NAME graph COMMAND graph ${CMAKE_CURRENT_BINARY_DIR}/facebook.txt 512)
