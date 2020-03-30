#ifndef TLP_APPS_PAGE_RANK_KERNEL_H_
#define TLP_APPS_PAGE_RANK_KERNEL_H_

#include <algorithm>

#include "page-rank.h"

#define VLOG_F(level, tag) VLOG(level) << #tag << "@" << __FUNCTION__ << ": "
#define LOG_F(level, tag) LOG(level) << #tag << "@" << __FUNCTION__ << ": "

struct VertexAttrKernel {
  float ranking;
  float tmp;
};

inline std::ostream& operator<<(std::ostream& os, const VertexAttrKernel& obj) {
  return os << "{ranking: " << obj.ranking << ", out_degree|tmp: " << obj.tmp
            << "}";
}

using VertexAttrVec = tlp::vec_t<VertexAttrKernel, kVertexVecLen>;

struct TaskReq {
  enum Phase { kScatter = 0, kGather = 1 };
  Phase phase;
  Pid pid;
  Vid overall_base_vid;
  Vid partition_size;
  Eid num_edges;
  Vid vid_offset;
  Eid eid_offset;
  float init;
  bool scatter_done;
};

inline std::ostream& operator<<(std::ostream& os, const TaskReq::Phase& obj) {
  return os << (obj == TaskReq::kScatter ? "SCATTER" : "GATHER");
}

inline std::ostream& operator<<(std::ostream& os, const TaskReq& obj) {
  return os << "{phase: " << obj.phase << ", pid: " << obj.pid
            << ", partition_size: " << obj.partition_size
            << ", num_edges: " << obj.num_edges
            << ", vid_offset: " << obj.vid_offset
            << ", eid_offset: " << obj.eid_offset << ", init: " << obj.init
            << ", scatter_done: " << obj.scatter_done << "}";
}

struct TaskResp {
  TaskReq::Phase phase;
  Pid pid;
  bool active;
};

inline std::ostream& operator<<(std::ostream& os, const TaskResp& obj) {
  return os << "{phase: " << obj.phase << ", pid: " << obj.pid
            << ", active: " << obj.active << "}";
}

struct UpdateReq {
  TaskReq::Phase phase;
  Pid pid;
  Eid num_updates;
};

inline std::ostream& operator<<(std::ostream& os, const UpdateReq& obj) {
  return os << "{phase: " << obj.phase << ", pid: " << obj.pid << "}";
}

struct VertexReq {
  TaskReq::Phase phase;
  Vid offset;
  Vid length;
};

struct NumUpdates {
  Pid pid;
  Eid num_updates;
};

inline std::ostream& operator<<(std::ostream& os, const NumUpdates& obj) {
  return os << "{pid: " << obj.pid << ", num_updates: " << obj.num_updates
            << "}";
}

struct UpdateVecWithPid {
  Pid pid;
  UpdateVec updates;
};

inline std::ostream& operator<<(std::ostream& os, const UpdateVecWithPid& obj) {
  return os << "{pid: " << obj.pid << ", updates: " << obj.updates << "}";
}

template <typename T>
inline T Max(const T (&array)[1]) {
#pragma HLS inline
  return array[0];
}

template <typename T, int N>
inline T Max(const T (&array)[N]) {
#pragma HLS inline
  return std::max(Max((const T(&)[N / 2])(array)),
                  Max((const T(&)[N - N / 2])(array[N / 2])));
}

#endif  // TLP_APPS_PAGE_RANK_KERNEL_H_