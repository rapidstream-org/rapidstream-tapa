#include <cmath>

#include <algorithm>
#include <array>
#include <chrono>
#include <deque>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <vector>

#include <boost/align.hpp>

#include <tlp.h>

#include "nxgraph.hpp"

#include "page-rank.h"

using std::array;
using std::clog;
using std::deque;
using std::endl;
using std::numeric_limits;
using std::ostream;
using std::runtime_error;
using std::unordered_map;
template <typename T>
using vector = std::vector<T, boost::alignment::aligned_allocator<T, 4096>>;
using std::chrono::duration;
using std::chrono::high_resolution_clock;

void PageRank(Pid num_partitions, tlp::mmap<const Vid> num_vertices,
              tlp::mmap<const Eid> num_edges,
              tlp::async_mmap<VertexAttrAlignedVec> vertices,
              tlp::async_mmap<EdgeVec> edges,
              tlp::async_mmap<UpdateVec> updates);

// Ground truth implementation of page rank.
//
// Initially the vertices should contain valid out degree information and
// ranking.
void PageRank(Vid base_vid, vector<VertexAttrAligned>& vertices,
              const vector<Edge>& edges) {
  const auto num_vertices = vertices.size();
  vector<Update> updates;

  float max_delta = 0.f;
  uint64_t iter = 0;
  do {
    // scatter
    updates.clear();
    for (const auto& edge : edges) {
      if (edge.src == 0) continue;
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
  const size_t partition_size =
      RoundUp<kVertexPartitionFactor>(argc > 2 ? atoi(argv[2]) : 1024);

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
    num_edges[i] = RoundUp<kEdgeVecLen>(partitions[i].num_edges);
    total_num_vertices += partitions[i].num_vertices;
    total_num_edges += num_edges[i];
  }

  // [src_pid][dst_pid][src_bank][dst_bank]
  unordered_map<
      Pid,
      unordered_map<Pid, array<array<deque<Edge>, kEdgeVecLen>, kEdgeVecLen>>>
      edge_table;

  for (size_t i = 0; i < num_partitions; ++i) {
    for (size_t j = 0; j < partitions[i].num_edges; ++j) {
      const auto& edge = partitions[i].shard.get()[j];
      const Pid src_pid = (edge.src - base_vid) / partition_size;
      const Pid dst_pid = (edge.dst - base_vid) / partition_size;
      const auto src_bank = edge.src % kEdgeVecLen;
      const auto dst_bank = edge.dst % kEdgeVecLen;
      edge_table[src_pid][dst_pid][src_bank][dst_bank].push_back(
          reinterpret_cast<const Edge&>(edge));
    }
  }

  vector<Edge> edges;
  edges.reserve(total_num_edges * 1.5);
  static_assert(sizeof(Edge) == sizeof(nxgraph::Edge<Vid>),
                "inconsistent Edge type");

  for (Pid src_pid = 0; src_pid < num_partitions; ++src_pid) {
    num_edges[src_pid] = 0;
    vector<size_t> num_edges_partition_i(num_partitions);

    for (Pid dst_pid = 0; dst_pid < num_partitions; ++dst_pid) {
      // there are kEdgeVecLen x kEdgeVecLen edge bins
      // we select edges along diagonal directions until the bins are empty

      for (int src_bank = 0; src_bank < kEdgeVecLen; ++src_bank) {
        for (int dst_bank = 0; dst_bank < kEdgeVecLen; ++dst_bank) {
          auto& q = edge_table[src_pid][dst_pid][src_bank][dst_bank];
          // std::random_shuffle(q.begin(), q.end());
        }
      }

      for (bool done = false; !done;) {
        done = true;
        for (int i = 0; i < kEdgeVecLen; ++i) {
          bool active = false;
          Edge edge_v[kEdgeVecLen] = {};
          for (int j = 0; j < kEdgeVecLen; ++j) {
            auto& bin =
                edge_table[src_pid][dst_pid][(base_vid + j) % kEdgeVecLen]
                          [(i + j) % kEdgeVecLen];
            if (!bin.empty()) {
              edge_v[j] = bin.back();
              bin.pop_back();
              active = true;
            }
          }
          if (!active) continue;
          done = false;
          for (int j = 0; j < kEdgeVecLen; ++j) {
            edges.push_back(edge_v[j]);
          }
          num_edges[src_pid] += kEdgeVecLen;
          num_edges_partition_i[dst_pid] += kEdgeVecLen;
          num_in_edges[dst_pid] += kEdgeVecLen;
        }
      }
    }

    // align to kEdgeVecLen
    LOG(INFO) << "num edges in partition[" << src_pid
              << "]: " << num_edges[src_pid];
    for (Pid dst_pid = 0; dst_pid < num_partitions; ++dst_pid) {
      VLOG(5) << "num edges in partition[" << src_pid << "][" << dst_pid
              << "]: " << num_edges_partition_i[dst_pid];
    }
  }

  LOG(INFO) << "total num edges: " << edges.size() << " (+" << std::fixed
            << std::setprecision(2)
            << 100. * (edges.size() - total_num_edges) / total_num_edges
            << "% over " << total_num_edges << ")";

  edges.shrink_to_fit();

  Eid total_num_updates = 0;
  for (size_t i = 0; i < num_partitions; ++i) {
    Eid delta = num_in_edges[i];
    total_num_updates += delta;
    if (i < num_partitions - 1) {
      num_edges[num_partitions + i + 1] = num_edges[num_partitions + i] + delta;
    }
    VLOG(5) << "update offset #" << i << ": " << num_edges[num_partitions + i];
  }

  vector<VertexAttrAligned> vertices_baseline(
      RoundUp<kVertexVecLen>(total_num_vertices));
  // initialize vertex attributes
  for (auto& v : vertices_baseline) {
    v.out_degree = 0;
    v.ranking = 1.f / total_num_vertices;
  }
  for (auto& e : edges) {
    if (e.src == 0) continue;
    ++vertices_baseline[e.src - base_vid].out_degree;
  }
  // pre-compute the deltas sent through out-going edges
  for (auto& v : vertices_baseline) {
    v.tmp = v.ranking / v.out_degree;
  }
  vector<VertexAttrAligned> vertices = vertices_baseline;
  vertices_baseline.resize(total_num_vertices);

  vector<Update> updates(total_num_updates);

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
      if (e.src != 0) VLOG(10) << e.src << " -> " << e.dst;
    }
    VLOG(10) << "updates: " << updates.size();
  }
  PageRank(base_vid, vertices_baseline, edges);
  PageRank(num_partitions, num_vertices, num_edges,
           tlp::async_mmap_from_vec<VertexAttrAligned, kVertexVecLen>(vertices),
           tlp::async_mmap_from_vec<Edge, kEdgeVecLen>(edges),
           tlp::async_mmap_from_vec<Update, kUpdateVecLen>(updates));

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
