puts "applying slr.tcl"
add_cells_to_pblock [get_pblocks pblock_dynamic_SLR0] [get_cells -hierarchical { \
  EdgeMem_0                 \
  edges_0_m_axi             \
  PageRank_control_s_axi_U  \
}]
add_cells_to_pblock [get_pblocks pblock_dynamic_SLR1] [get_cells -hierarchical { \
  ProcElem_0       \
  UpdateMem_0      \
  updates_0_m_axi  \
  UpdateHandler_0  \
  Control_0        \
}]
add_cells_to_pblock [get_pblocks pblock_dynamic_SLR2] [get_cells -hierarchical { \
  ProcElem_1       \
  UpdateMem_1      \
  updates_1_m_axi  \
  UpdateHandler_1  \
  VertexMem_0      \
  vertices_m_axi   \
}]
add_cells_to_pblock [get_pblocks pblock_dynamic_SLR3] [get_cells -hierarchical { \
  EdgeMem_1        \
  edges_1_m_axi    \
}]