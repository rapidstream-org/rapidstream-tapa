#ifndef TLP_APPS_PAGE_RANK_H_
#define TLP_APPS_PAGE_RANK_H_

#include <iostream>

#include <tlp.h>

// Some utility functions.

// Round n up to a multiple of N.
template <uint64_t N, typename T>
inline T RoundUp(T n) {
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
using Pid = uint32_t;     // can hold all partitions

constexpr float kDampingFactor = .85f;
constexpr float kConvergenceThreshold = 0.0001f;

struct VertexAttr {
  Degree out_degree;
  float ranking;
  float tmp;
};

inline std::ostream& operator<<(std::ostream& os, const VertexAttr& obj) {
  return os << "{out_degree: " << obj.out_degree << ", ranking: " << obj.ranking
            << ", tmp: " << obj.tmp << "}";
}

struct VertexAttrAligned : public VertexAttr {
  uint32_t padding;

  VertexAttrAligned() = default;
  VertexAttrAligned(const VertexAttr& v) : VertexAttr(v) {}
};

static_assert(IsPowerOf2<sizeof(VertexAttrAligned)>(),
              "VertexAttrAligned is not aligned to a power of 2");

constexpr uint64_t kVertexVecLen = kVecLenBytes / sizeof(VertexAttrAligned);

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

constexpr uint64_t kVertexPartitionFactor =
    kEdgeVecLen > kVertexVecLen
        ? kEdgeVecLen > kUpdateVecLen ? kEdgeVecLen : kUpdateVecLen
        : kVertexVecLen > kUpdateVecLen ? kVertexVecLen : kUpdateVecLen;

// Alias for vec types.
using VertexAttrVec = tlp::vec_t<VertexAttr, kVertexVecLen>;
using VertexAttrAlignedVec = tlp::vec_t<VertexAttrAligned, kVertexVecLen>;
using EdgeVec = tlp::vec_t<Edge, kEdgeVecLen>;
using UpdateVec = tlp::vec_t<Update, kUpdateVecLen>;

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

#endif  // TLP_APPS_PAGE_RANK_H_