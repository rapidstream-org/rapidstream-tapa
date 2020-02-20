puts "applying slr.tcl"
add_cells_to_pblock [get_pblocks pblock_dynamic_SLR0] [get_cells -hierarchical { Copy_0 chan0_m_axi }]
add_cells_to_pblock [get_pblocks pblock_dynamic_SLR1] [get_cells -hierarchical { Copy_1 chan1_m_axi }]
add_cells_to_pblock [get_pblocks pblock_dynamic_SLR2] [get_cells -hierarchical { Copy_2 chan2_m_axi }]
add_cells_to_pblock [get_pblocks pblock_dynamic_SLR3] [get_cells -hierarchical { Copy_3 chan3_m_axi }]