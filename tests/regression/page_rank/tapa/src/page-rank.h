// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#ifndef TAPA_PAGE_RANK_H_
#define TAPA_PAGE_RANK_H_

#include <iostream>

#include <tapa.h>

// Some utility functions.

// Round n up to a multiple of N.
template <uint64_t N, typename T>
constexpr inline T RoundUp(T n) {
  return ((n - 1) / N + 1) * N;
}

// Test for a power of 2.
template <uint64_t N>
inline constexpr bool IsPowerOf2() {
  return N > 0 ? N % 2 == 0 && IsPowerOf2<N / 2>() : false;
}

template <>
inline constexpr bool IsPowerOf2<1>() {
  return true;
}

constexpr uint64_t kVecLenBytes = 64;  // 512 bits

// Page Rank.

using Vid = uint32_t;     // can hold all vertices
using Degree = uint32_t;  // can hold the maximum degree
using Eid = uint32_t;     // can hold all edges
using Pid = uint16_t;     // can hold all partitions

constexpr float kDampingFactor = .85f;
constexpr float kConvergenceThreshold = 0.0001f;

constexpr int kNumPes = 8;

struct VertexAttr {
  Degree out_degree;
  float ranking;
  float tmp;
};

constexpr uint64_t kVertexVecLen = kVecLenBytes / sizeof(float);

inline std::ostream& operator<<(std::ostream& os, const VertexAttr& obj) {
  return os << "{out_degree: " << obj.out_degree << ", ranking: " << obj.ranking
            << ", tmp: " << obj.tmp << "}";
}

struct Edge {
  Vid src;
  Vid dst;
};

inline std::ostream& operator<<(std::ostream& os, const Edge& obj) {
  return os << "{" << obj.src << " -> " << obj.dst << "}";
}

constexpr uint64_t kEdgeVecLen = kVecLenBytes / sizeof(Edge);
static_assert(IsPowerOf2<sizeof(Edge)>(),
              "Edge is not aligned to a power of 2");

struct Update {
  Vid dst;
  float delta;
};

inline std::ostream& operator<<(std::ostream& os, const Update& obj) {
  return os << "{dst: " << obj.dst << ", value: " << obj.delta << "}";
}

constexpr uint64_t kUpdateVecLen = kVecLenBytes / sizeof(Update);
static_assert(IsPowerOf2<sizeof(Update)>(),
              "Update is not aligned to a power of 2");

constexpr int kVertexUpdateLatency = 4;
constexpr int kVertexUpdateDepDist = kVertexUpdateLatency + 1;

// Alias for vec types.
using FloatVec = tapa::vec_t<float, kVertexVecLen>;
using EdgeVec = tapa::vec_t<Edge, kEdgeVecLen>;
using UpdateVec = tapa::vec_t<Update, kUpdateVecLen>;

inline std::ostream& operator<<(std::ostream& os, const UpdateVec& obj) {
  os << "{";
  bool first = true;
  for (int i = 0; i < UpdateVec::length; ++i) {
    if (obj[i].dst != 0) {
      if (!first) {
        os << ", ";
      } else {
        first = false;
      }
      os << "[" << i << "]: " << obj[i];
    }
  }
  return os << "}";
}

#endif  // TAPA_APPS_PAGE_RANK_H_
