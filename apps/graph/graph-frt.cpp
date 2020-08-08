#include <stdlib.h>

#include <frt.h>
#include <tapa.h>

using Vid = uint32_t;
using Eid = uint32_t;
using Pid = uint16_t;

using VertexAttr = Vid;

struct Edge {
  Vid src;
  Vid dst;
};

struct Update {
  Vid dst;
  float delta;
};

void Graph(Pid num_partitions, tapa::mmap<const Vid> num_vertices,
           tapa::mmap<const Eid> num_edges, tapa::mmap<VertexAttr> vertices,
           tapa::mmap<const Edge> edges, tapa::mmap<Update> updates) {
  fpga::Invoke(getenv("BITSTREAM"), num_partitions,
               fpga::WriteOnly(num_vertices.get(), num_vertices.size()),
               fpga::WriteOnly(num_edges.get(), num_edges.size()),
               fpga::ReadWrite(vertices.get(), vertices.size()),
               fpga::WriteOnly(edges.get(), edges.size()),
               fpga::ReadWrite(updates.get(), updates.size()));
}
