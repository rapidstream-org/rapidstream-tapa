#include <cmath>
#include <cstring>

#include <algorithm>
#include <array>
#include <chrono>
#include <deque>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <vector>

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
using vector = std::vector<T, tlp::aligned_allocator<T>>;
using std::memcpy;
using std::chrono::duration;
using std::chrono::high_resolution_clock;

// metadata layout:
//
// input uint64_t base_vid;
// input uint64_t partition_size;
// input uint64_t num_edges[num_partitions][kNumPes];
// input uint64_t update_offsets[num_partitions];
// output uint64_t num_iterations;

void PageRank(Pid num_partitions, tlp::mmap<uint64_t> metadata,
              tlp::async_mmap<FloatVec> degrees,
              tlp::async_mmap<FloatVec> rankings,
              tlp::async_mmap<FloatVec> tmps,
              tlp::async_mmaps<EdgeVec, kNumPes> edges,
              tlp::async_mmaps<UpdateVec, kNumPes> updates);

// Ground truth implementation of page rank.
//
// Initially the vertices should contain valid out degree information and
// ranking.
void PageRank(Vid base_vid, vector<VertexAttr>& vertices,
              const array<vector<Edge>, kNumPes>& edges) {
  const auto num_vertices = vertices.size();
  vector<Update> updates;

  float max_delta = 0.f;
  uint64_t iter = 0;
  do {
    // scatter
    updates.clear();
    for (auto& edges : edges) {
      for (auto& edge : edges) {
        if (edge.src == 0) continue;
        auto& src = vertices[edge.src - base_vid];
        CHECK_NE(src.out_degree, 0);
        // use pre-computed src.tmp = src.ranking / src.out_degree
        updates.push_back({edge.dst, src.tmp});
      }
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
  const Pid partition_size =
      RoundUp<kVertexPartitionFactor>(argc > 2 ? atoi(argv[2]) : 1024);

  // load and partition the graph using libnxgraph
  const auto partitions =
      nxgraph::LoadEdgeList<Vid, Eid, VertexAttr>(argv[1], partition_size);
  for (auto& partition : partitions) {
    VLOG(6) << "partition";
    CHECK_EQ(partition.num_vertices, partition_size);
    if (VLOG_IS_ON(6)) {
      for (Eid i = 0; i < partition.num_edges; ++i) {
        auto edge = partition.shard.get()[i];
        VLOG(6) << "src: " << edge.src << " dst: " << edge.dst;
      }
    }
  }

  // metadata
  const auto base_vid = partitions[0].base_vid;
  const Pid num_partitions = partitions.size();
  Vid total_num_vertices = num_partitions * partition_size;

  Eid total_num_edges = 0;
  Eid total_num_edges_vec = 0;
  vector<Eid> num_in_edges(num_partitions);
  vector<Eid> num_out_edges(num_partitions);
  array<Eid, kNumPes> num_edges_per_pe = {};
  array<vector<Eid>, kNumPes> num_out_edges_per_pe;

  for (auto& num_edges : num_out_edges_per_pe) num_edges.resize(num_partitions);
  for (auto& partition : partitions) total_num_edges += partition.num_edges;

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
      ++num_edges_per_pe[dst_pid % kNumPes];
    }
  }

  // prepare data for device
  array<vector<Edge>, kNumPes> edges;
  for (int i = 0; i < kNumPes; ++i) {
    edges[i].reserve(num_edges_per_pe[i] * 1.5);
  }
  static_assert(sizeof(Edge) == sizeof(nxgraph::Edge<Vid>),
                "inconsistent Edge type");

  num_edges_per_pe.fill(0);
  Eid total_max_num = 0;
  for (Pid src_pid = 0; src_pid < num_partitions; ++src_pid) {
    vector<size_t> num_edges_i(num_partitions);

    for (Pid dst_pid = 0; dst_pid < num_partitions; ++dst_pid) {
      // there are kEdgeVecLen x kEdgeVecLen edge bins
      // we select edges along diagonal directions until the bins are empty

      for (int src_bank = 0; src_bank < kEdgeVecLen; ++src_bank) {
        for (int dst_bank = 0; dst_bank < kEdgeVecLen; ++dst_bank) {
          auto& q = edge_table[src_pid][dst_pid][src_bank][dst_bank];
        }
      }

      for (bool done = false; !done;) {
        done = true;
        for (int i = 0; i < kEdgeVecLen; ++i) {
          bool active = false;
          Edge edge_v[kEdgeVecLen] = {};
          // empty edge is indicated by src == 0
          // first edge in a vector must have valid dst for routing purpose
          edge_v[0].dst = dst_pid * partition_size + base_vid;
          for (int j = 0; j < kEdgeVecLen; ++j) {
            auto& bin =
                edge_table[src_pid][dst_pid][(base_vid + j) % kEdgeVecLen]
                          [(i + j) % kEdgeVecLen];
            if (!bin.empty()) {
              auto conflict = [&](const Edge& e) -> bool {
                // since the edges are banked by src, we have to search all
                // edges to avoid conflict
                const auto& edges_pe = edges[dst_pid % kNumPes];
                for (auto it = edges_pe.rbegin();
                     it < edges_pe.rbegin() +
                              (kVertexUpdateDepDist - 1) * kEdgeVecLen &&
                     it < edges_pe.rend();
                     ++it) {
                  if (it->dst == e.dst && it->src != 0) return true;
                }
                return false;
              };
              if (!conflict(bin.back())) {
                edge_v[j] = bin.back();
                bin.pop_back();
              }
              active = true;
            }
          }
          if (!active) continue;
          done = false;
          for (int j = 0; j < kEdgeVecLen; ++j) {
            edges[dst_pid % kNumPes].push_back(edge_v[j]);
          }
          num_edges_i[dst_pid] += kEdgeVecLen;
          num_in_edges[dst_pid] += kEdgeVecLen;
          num_out_edges[src_pid] += kEdgeVecLen;
          num_edges_per_pe[dst_pid % kNumPes] += kEdgeVecLen;
          num_out_edges_per_pe[dst_pid % kNumPes][src_pid] += kEdgeVecLen;
        }
      }
    }

    total_num_edges_vec += num_out_edges[src_pid];
    VLOG(5) << "num edges in partition[" << src_pid
            << "]: " << num_out_edges[src_pid];
    Eid max_num = 0;
    for (int pe = 0; pe < kNumPes; ++pe) {
      max_num = std::max(num_out_edges_per_pe[pe][src_pid], max_num);
    }
    total_max_num += max_num * kNumPes;
    VLOG(5) << "num edges in partition[" << src_pid
            << "]: " << max_num * kNumPes << " (+" << std::fixed
            << std::setprecision(2)

            << (100. * (max_num * kNumPes - num_out_edges[src_pid]) /
                num_out_edges[src_pid])
            << "%)";

    for (Pid dst_pid = 0; dst_pid < num_partitions; ++dst_pid) {
      VLOG(5) << "num edges in partition[" << src_pid << "][" << dst_pid
              << "]: " << num_edges_i[dst_pid];
    }
  }

  for (auto& edges : edges) edges.shrink_to_fit();

  LOG(INFO) << "num edges: " << total_num_edges_vec << " (+" << std::fixed
            << std::setprecision(2)
            << 100. * (total_num_edges_vec - total_num_edges) / total_num_edges
            << "% over " << total_num_edges << " due to vectorization)";
  LOG(INFO) << "num edges: " << total_max_num << " (+" << std::fixed
            << std::setprecision(2)
            << 100. * (total_max_num - total_num_edges_vec) /
                   total_num_edges_vec
            << "% over " << total_num_edges_vec
            << " due to partition synchronization)";

  vector<VertexAttr> vertices_baseline(total_num_vertices);
  // initialize vertex attributes
  for (auto& v : vertices_baseline) {
    v.out_degree = 0;
    v.ranking = 1.f / total_num_vertices;
  }
  for (auto& edges : edges) {
    for (auto& e : edges) {
      if (e.src == 0) continue;
      ++vertices_baseline[e.src - base_vid].out_degree;
    }
  }
  // pre-compute the deltas sent through out-going edges
  for (auto& v : vertices_baseline) {
    v.tmp = v.ranking / v.out_degree;
  }

  array<vector<Update>, kNumPes> updates;
  for (int i = 0; i < kNumPes; ++i) {
    updates[i].resize(num_edges_per_pe[i]);
  }

  vector<uint64_t> metadata(num_partitions * (kNumPes + 1) + 3);
  metadata[0] = base_vid;
  metadata[1] = partition_size;
  for (size_t p = 0; p < num_partitions; ++p) {
    for (int pe = 0; pe < kNumPes; ++pe) {
      metadata[2 + kNumPes * p + pe] = num_out_edges_per_pe[pe][p];
    }
    if (p / kNumPes > 0) {
      metadata[2 + kNumPes * num_partitions + p] =
          metadata[2 + kNumPes * num_partitions + p - kNumPes] +
          num_in_edges[p - kNumPes];
    }
  }

  if (VLOG_IS_ON(10)) {
    VLOG(10) << "metadata";
    for (auto n : metadata) {
      VLOG(10) << n;
    }
    VLOG(10) << "vertices: ";
    for (auto v : vertices_baseline) {
      VLOG(10) << v;
    }
    VLOG(10) << "edges: ";
    const int vid_width = int(log10(base_vid + total_num_vertices - 1)) + 1;
    for (auto& edges : edges) {
      for (int i = 0; i < edges.size(); i += kEdgeVecLen) {
        std::ostringstream oss;
        for (int j = 0; j < kEdgeVecLen; ++j) {
          if (j != 0) oss << " | ";
          oss << std::setfill(' ') << std::setw(vid_width) << edges[i + j].src
              << " -> " << std::setfill(' ') << std::setw(vid_width)
              << (edges[i + j].src ? edges[i + j].dst : 0);
        }
        VLOG(10) << oss.str();
      }
    }
    VLOG(10) << "updates: " << updates.size();
  }

  // allocate at least 1 page for each memory bank
  for (int i = 0; i < kNumPes; ++i) {
    if (edges[i].empty()) edges[i].resize(4096 / sizeof(Edge));
    if (updates[i].empty()) updates[i].resize(4096 / sizeof(Update));
  }

  vector<float> degrees(RoundUp<kVertexVecLen>(vertices_baseline.size()));
  vector<float> rankings(RoundUp<kVertexVecLen>(vertices_baseline.size()));
  vector<float> tmps(RoundUp<kVertexVecLen>(vertices_baseline.size()));

  for (size_t i = 0; i < vertices_baseline.size(); ++i) {
    degrees[i] = 1. / vertices_baseline[i].out_degree;
    rankings[i] = vertices_baseline[i].ranking;
    tmps[i] = vertices_baseline[i].tmp;
  }

  PageRank(base_vid, vertices_baseline, edges);
  PageRank(num_partitions, metadata,
           tlp::make_vec_async_mmap<float, kVertexVecLen>(degrees),
           tlp::make_vec_async_mmap<float, kVertexVecLen>(rankings),
           tlp::make_vec_async_mmap<float, kVertexVecLen>(tmps),
           tlp::make_vec_async_mmaps<Edge, kEdgeVecLen, kNumPes>(edges),
           tlp::make_vec_async_mmaps<Update, kUpdateVecLen, kNumPes>(updates));

  LOG(INFO) << "device code finished after " << *metadata.rbegin()
            << " iterations";

  vector<VertexAttr> best_baseline(10);
  vector<decltype(VertexAttr::ranking)> best(best_baseline.size());
  auto comp = [](const VertexAttr& lhs, const VertexAttr& rhs) {
    return lhs.ranking > rhs.ranking;
  };
  std::partial_sort_copy(vertices_baseline.begin(), vertices_baseline.end(),
                         best_baseline.begin(), best_baseline.end(), comp);
  std::partial_sort_copy(rankings.begin(), rankings.end(), best.begin(),
                         best.end(), std::greater<float>{});

  if (VLOG_IS_ON(6)) {
    for (const auto& v : best_baseline) {
      VLOG(6) << "expected best: " << v;
    }
    for (const auto& v : best) {
      VLOG(6) << "actual best: " << v;
    }
  }

  if (VLOG_IS_ON(10)) {
    VLOG(10) << "metadata";
    for (auto n : metadata) {
      VLOG(10) << n;
    }
    VLOG(10) << "vertices: ";
    for (auto v : vertices_baseline) {
      VLOG(10) << v;
    }
    VLOG(10) << "updates: ";
    for (auto& updates : updates) {
      for (auto v : updates) {
        VLOG(10) << "dst: " << v.dst << " delta: " << v.delta;
      }
    }
  }

  uint64_t num_errors = 0;
  const uint64_t threshold = 10;  // only report up to these errors
  for (Vid i = 0; i < best_baseline.size(); ++i) {
    auto expected = best_baseline[i].ranking;
    auto actual = best[i];
    if (actual < expected - kConvergenceThreshold ||
        actual > expected + kConvergenceThreshold) {
      if (num_errors < threshold) {
        LOG(ERROR) << "vertex #" << base_vid + i << ": "
                   << "expected: " << expected << ", actual: " << actual;
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

  return num_errors ? 1 : 0;
}
