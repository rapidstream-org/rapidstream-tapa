#include <stdlib.h>

#include <frt.h>
#include <tlp.h>

// typedefs that should have been placed in header

using Vid = uint32_t;     // can hold all vertices
using Degree = uint32_t;  // can hold the maximum degree
using Eid = uint32_t;     // can hold all edges
using Pid = uint32_t;     // can hold all partitions

constexpr float kDampingFactor = .85f;
constexpr float kConvergenceThreshold = 0.0001f;

struct VertexAttr {
  Degree out_degree;
  float ranking;
  float tmp;
  uint32_t padding;
};

struct Edge {
  Vid src;
  Vid dst;
};

struct Update {
  Vid dst;
  float delta;
};

void PageRank(Pid num_partitions, tlp::mmap<const Vid> num_vertices,
              tlp::mmap<const Eid> num_edges, tlp::mmap<VertexAttr> vertices,
              tlp::mmap<const Edge> edges, tlp::mmap<Update> updates) {
  auto instance =
      fpga::Invoke(getenv("BITSTREAM"), num_partitions,
                   fpga::WriteOnly(num_vertices.get(), num_vertices.size()),
                   fpga::WriteOnly(num_edges.get(), num_edges.size()),
                   fpga::ReadWrite(vertices.get(), vertices.size()),
                   fpga::WriteOnly(edges.get(), edges.size()),
                   fpga::ReadWrite(updates.get(), updates.size()));
  LOG(INFO) << "kernel time: " << instance.ComputeTimeSeconds() << "s";
}
