find_path(vivado_ap_int_INCLUDE_DIR ap_int.h
          PATHS "$ENV{XILINX_VIVADO}/include"
                "${CMAKE_SOURCE_DIR}/3rdparty/vivado_ap_int/include")
add_library(vivado_ap_int INTERFACE)
target_include_directories(vivado_ap_int INTERFACE ${vivado_ap_int_INCLUDE_DIR})
