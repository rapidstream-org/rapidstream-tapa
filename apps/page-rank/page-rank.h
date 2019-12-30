#ifndef TLP_APPS_PAGE_RANK_H_
#define TLP_APPS_PAGE_RANK_H_

#include <iostream>

#include <ap_int.h>

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

constexpr uint64_t kEdgeVecLen = 8;

struct Update {
  Vid dst;
  float delta;
};

constexpr uint64_t kUpdateVecLen = 8;

// Round n up to a multiple of N.
template <uint64_t N, typename T>
inline T RoundUp(T n) {
  return ((n - 1) / N + 1) * N;
}

inline std::ostream& operator<<(std::ostream& os, const VertexAttr& obj) {
  return os << "{out_degree: " << obj.out_degree << ", ranking: " << obj.ranking
            << ", tmp: " << obj.tmp << "}";
}

// return if update conflicts with any of the elements in window[N]
// a and b conflicts if their dsts are the same and != 0
template <int N>
inline bool HasConflict(const Update (&window)[N], Update update) {
#pragma HLS inline
#pragma HLS protocol floating
  bool ret = false;
  for (int i = 0; i < N; ++i) {
#pragma HLS unroll
    ret |= window[i].dst == update.dst && window[i].dst != 0;
  }
  return ret;
}

template <int N>
inline int FindFirstEmpty(const Update (&buffer)[N]) {
#pragma HLS inline
#pragma HLS protocol floating
  for (int i = 0; i < N; ++i) {
#pragma HLS unroll
    if (buffer[i].dst == 0) {
      return i;
    }
  }
  return -1;
}

// return the first index in buffer[N1] that do not conflict with window[N2]
template <int N1, int N2>
inline int FindFirstWithoutConflict(const Update (&buffer)[N1],
                                    const Update (&window)[N2]) {
#pragma HLS inline
#pragma HLS protocol floating
  for (int i = 0; i < N1; ++i) {
#pragma HLS unroll
    if (buffer[0].dst != 0 && !HasConflict(window, buffer[i])) {
      return i;
    }
  }
  return -1;
}

#endif  // TLP_APPS_PAGE_RANK_H_