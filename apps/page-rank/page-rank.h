#ifndef TLP_APPS_PAGE_RANK_H_
#define TLP_APPS_PAGE_RANK_H_

#include <iostream>

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

inline std::ostream& operator<<(std::ostream& os, const VertexAttr& obj) {
  return os << "{out_degree: " << obj.out_degree << ", ranking: " << obj.ranking
            << ", tmp: " << obj.tmp << "}";
}

#endif  // TLP_APPS_PAGE_RANK_H_