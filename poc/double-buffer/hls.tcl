#!/usr/bin/env vitis_hls
open_project -reset "build"
add_files kernel.cpp -cflags -std=c++17
set_top Top
open_solution sol
set_part {xcu250-figd2104-2L-e}
create_clock -period 3.333 -name default
config_compile -name_max_length 253
config_interface -m_axi_addr64
csynth_design
exit
