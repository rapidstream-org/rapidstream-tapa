#include <cmath>

#include <chrono>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

#include <tlp.h>

#include "nxgraph.hpp"

using std::clog;
using std::endl;
using std::numeric_limits;
using std::runtime_error;
using std::vector;
using std::chrono::duration;
using std::chrono::high_resolution_clock;

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

void Graph(Pid num_partitions, tlp::mmap<const Vid> num_vertices,
           tlp::mmap<const Eid> num_edges, tlp::mmap<VertexAttr> vertices,
           tlp::mmap<const Edge> edges, tlp::mmap<Update> updates);

void Graph(Vid base_vid, vector<VertexAttr>& vertices,
           const vector<Edge>& edges) {
  bool has_update = true;
  while (has_update) {
    has_update = false;
    for (const auto& edge : edges) {
      if (vertices[edge.src - base_vid] < vertices[edge.dst - base_vid]) {
        vertices[edge.dst - base_vid] = vertices[edge.src - base_vid];
        has_update = true;
      }
    }
  }
}

int main(int argc, char* argv[]) {
  const size_t partition_size = argc > 2 ? atoi(argv[2]) : 1024;
  auto partitions =
      nxgraph::LoadEdgeList<Vid, Eid, VertexAttr>(argv[1], partition_size);
  for (const auto& partition : partitions) {
    VLOG(6) << "partition";
    VLOG(6) << "num vertices: " << partition.num_vertices;
    for (Eid i = 0; i < partition.num_edges; ++i) {
      auto edge = partition.shard.get()[i];
      VLOG(6) << "src: " << edge.src << " dst: " << edge.dst;
    }
  }
  const size_t num_partitions = partitions.size();

  vector<Vid> num_vertices(num_partitions + 1);
  vector<Eid> num_edges(num_partitions);
  Vid total_num_vertices = 0;
  Eid total_num_edges = 0;
  num_vertices[0] = partitions[0].base_vid;
  const auto& base_vid = num_vertices[0];
  for (size_t i = 0; i < num_partitions; ++i) {
    num_vertices[i + 1] = partitions[i].num_vertices;
    num_edges[i] = partitions[i].num_edges;
    total_num_vertices += partitions[i].num_vertices;
    total_num_edges += partitions[i].num_edges;
  }

  vector<VertexAttr> vertices(total_num_vertices);
  vector<VertexAttr> vertices_baseline(total_num_vertices);
  for (Vid i = 0; i < total_num_vertices; ++i) {
    vertices[i] = base_vid + i;
    vertices_baseline[i] = base_vid + i;
  }

  vector<Edge> edges(total_num_edges);
  if (sizeof(Edge) != sizeof(nxgraph::Edge<Vid>)) {
    throw runtime_error("inconsistent Edge type");
  }
  auto edge_ptr = edges.data();
  for (size_t i = 0; i < num_partitions; ++i) {
    memcpy(edge_ptr, partitions[i].shard.get(), num_edges[i] * sizeof(Edge));
    edge_ptr += num_edges[i];
  }
  vector<Update> updates(total_num_edges * num_partitions);
  Graph(num_partitions, num_vertices.data(), num_edges.data(), vertices.data(),
        edges.data(), updates.data());
  Graph(base_vid, vertices_baseline, edges);

  uint64_t num_errors = 0;
  const uint64_t threshold = 10;  // only report up to these errors
  for (Vid i = 0; i < total_num_vertices; ++i) {
    auto expected = vertices_baseline[i];
    auto actual = vertices[i];
    if (actual != expected) {
      if (num_errors < threshold) {
        LOG(ERROR) << "vertex #" << base_vid + i << ": "
                   << "expected: " << expected << ", actual: " << actual
                   << endl;
      } else if (num_errors == threshold) {
        LOG(ERROR) << "...";
      }
      ++num_errors;
    }
  }
  if (num_errors == 0) {
    clog << "PASS!" << endl;
  } else {
    if (num_errors > threshold) {
      clog << " (+" << (num_errors - threshold) << " more errors)" << endl;
    }
    clog << "FAIL!" << endl;
  }

  return 0;
}
