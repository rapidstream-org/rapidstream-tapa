puts "applying slr.tcl"
add_cells_to_pblock [get_pblocks pblock_dynamic_SLR0] [get_cells -hierarchical { Copy_0 chan_0__m_axi Bandwidth_control_s_axi_U }]
add_cells_to_pblock [get_pblocks pblock_dynamic_SLR1] [get_cells -hierarchical { Copy_1 chan_1__m_axi }]
add_cells_to_pblock [get_pblocks pblock_dynamic_SLR2] [get_cells -hierarchical { Copy_2 chan_2__m_axi }]
add_cells_to_pblock [get_pblocks pblock_dynamic_SLR3] [get_cells -hierarchical { Copy_3 chan_3__m_axi }]