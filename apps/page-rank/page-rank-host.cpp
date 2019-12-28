#include <cmath>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

#include <boost/align.hpp>

#include <tlp.h>

#include "nxgraph.hpp"

#include "page-rank.h"

using std::clog;
using std::endl;
using std::numeric_limits;
using std::ostream;
using std::runtime_error;
template <typename T>
using vector = std::vector<T, boost::alignment::aligned_allocator<T, 4096>>;
using std::chrono::duration;
using std::chrono::high_resolution_clock;

void PageRank(Pid num_partitions, tlp::mmap<const Vid> num_vertices,
              tlp::mmap<const Eid> num_edges, tlp::mmap<VertexAttr> vertices,
              tlp::mmap<const Edge> edges, tlp::async_mmap<Update> updates);

// Ground truth implementation of page rank.
//
// Initially the vertices should contain valid out degree information and
// ranking.
void PageRank(Vid base_vid, vector<VertexAttr>& vertices,
              const vector<Edge>& edges) {
  const auto num_vertices = vertices.size();
  vector<Update> updates;

  float max_delta = 0.f;
  uint64_t iter = 0;
  do {
    // scatter
    updates.clear();
    for (const auto& edge : edges) {
      auto& src = vertices[edge.src - base_vid];
      CHECK_NE(src.out_degree, 0);
      // use pre-computed src.tmp = src.ranking / src.out_degree
      updates.push_back({edge.dst, src.tmp});
    }

    // gather
    for (auto& vertex : vertices) {
      vertex.tmp = 0.f;
    }
    for (const auto& update : updates) {
      vertices[update.dst - base_vid].tmp += update.delta;
    }

    max_delta = 0.f;
    const float init = (1.f - kDampingFactor) / num_vertices;
    for (auto& vertex : vertices) {
      const float new_ranking = init + vertex.tmp * kDampingFactor;
      const float abs_delta = std::abs(new_ranking - vertex.ranking);
      if (abs_delta > max_delta) max_delta = abs_delta;
      vertex.ranking = new_ranking;
      // pre-compute vertex.tmp = vertex.ranking / vertex.out_degree
      vertex.tmp = vertex.ranking / vertex.out_degree;
    }
    LOG(INFO) << "iteration #" << iter << " max delta: " << max_delta;

    ++iter;
  } while (max_delta > kConvergenceThreshold);
}

int main(int argc, char* argv[]) {
  const size_t partition_size = argc > 2 ? atoi(argv[2]) : 1024;

  // load and partition the graph using libnxgraph
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
  vector<Eid> num_edges(num_partitions * 2);
  vector<Eid> num_in_edges(num_partitions);
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

  // calculate the update offsets
  for (size_t i = 0; i < num_partitions; ++i) {
    for (auto* ptr = partitions[i].shard.get();
         ptr < partitions[i].shard.get() + partitions[i].num_edges; ++ptr) {
      ++num_in_edges[(ptr->dst - base_vid) / partition_size];
    }
  }
  for (size_t i = 1; i < num_partitions; ++i) {
    num_edges[num_partitions + i] =
        num_edges[num_partitions + i - 1] + num_in_edges[i - 1];
    VLOG(5) << "update offset #" << i << ": " << num_edges[num_partitions + i];
  }

  vector<Edge> edges;
  edges.reserve(total_num_edges);
  if (sizeof(Edge) != sizeof(nxgraph::Edge<Vid>)) {
    throw runtime_error("inconsistent Edge type");
  }

  for (size_t i = 0; i < num_partitions; ++i) {
    auto partition_begin = edges.end();
    auto edge_ptr = reinterpret_cast<Edge*>(partitions[i].shard.get());
    edges.insert(edges.end(), edge_ptr, edge_ptr + num_edges[i]);

    // increase burst length of update writes
    std::sort(partition_begin, edges.end(),
              [&](const Edge& lhs, const Edge& rhs) -> bool {
                return (lhs.dst - base_vid) / partition_size <
                       (rhs.dst - base_vid) / partition_size;
              });
  }

  vector<VertexAttr> vertices_baseline(total_num_vertices);
  // initialize vertex attributes
  for (auto& v : vertices_baseline) {
    v.out_degree = 0;
    v.ranking = 1.f / total_num_vertices;
  }
  for (auto& e : edges) {
    ++vertices_baseline[e.src - base_vid].out_degree;
  }
  // pre-compute the deltas sent through out-going edges
  for (auto& v : vertices_baseline) {
    v.tmp = v.ranking / v.out_degree;
  }
  vector<VertexAttr> vertices{vertices_baseline};

  vector<Update> updates(total_num_edges);

  if (VLOG_IS_ON(10)) {
    VLOG(10) << "num_vertices";
    for (auto n : num_vertices) {
      VLOG(10) << n;
    }
    VLOG(10) << "num_edges";
    for (auto n : num_edges) {
      VLOG(10) << n;
    }
    VLOG(10) << "vertices: ";
    for (auto v : vertices) {
      VLOG(10) << v;
    }
    VLOG(10) << "edges: ";
    for (auto e : edges) {
      VLOG(10) << e.src << " -> " << e.dst;
    }
    VLOG(10) << "updates: " << updates.size();
  }
  PageRank(base_vid, vertices_baseline, edges);
  PageRank(num_partitions, num_vertices, num_edges, vertices, edges, updates);

  vector<VertexAttr> best_baseline(10);
  vector<VertexAttr> best{best_baseline};
  auto comp = [](const VertexAttr& lhs, const VertexAttr& rhs) {
    return lhs.ranking > rhs.ranking;
  };
  std::partial_sort_copy(vertices_baseline.begin(), vertices_baseline.end(),
                         best_baseline.begin(), best_baseline.end(), comp);
  std::partial_sort_copy(vertices.begin(), vertices.end(), best.begin(),
                         best.end(), comp);

  if (VLOG_IS_ON(6)) {
    for (const auto& v : best_baseline) {
      VLOG(6) << "expected best: " << v;
    }
    for (const auto& v : best) {
      VLOG(6) << "actual best: " << v;
    }
  }

  if (VLOG_IS_ON(10)) {
    VLOG(10) << "vertices: ";
    for (auto v : vertices) {
      VLOG(10) << v;
    }
    VLOG(10) << "updates: ";
    for (auto v : updates) {
      VLOG(10) << "dst: " << v.dst << " delta: " << v.delta;
    }
  }

  uint64_t num_errors = 0;
  const uint64_t threshold = 10;  // only report up to these errors
  for (Vid i = 0; i < best_baseline.size(); ++i) {
    auto expected = best_baseline[i].ranking;
    auto actual = best[i].ranking;
    if (actual < expected - kConvergenceThreshold ||
        actual > expected + kConvergenceThreshold) {
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
