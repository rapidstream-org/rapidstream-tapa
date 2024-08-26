// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#ifndef TAPA_PAGE_RANK_KERNEL_H_
#define TAPA_PAGE_RANK_KERNEL_H_

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

using VertexAttrVec = tapa::vec_t<VertexAttrKernel, kVertexVecLen>;

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
  bool active;
};

inline std::ostream& operator<<(std::ostream& os, const TaskResp& obj) {
  return os << "{active: " << obj.active << "}";
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
  Vid offset;
  Vid length;
};

using NumUpdates = tapa::packet<Pid, Eid>;
using UpdateVecPacket = tapa::packet<Pid, UpdateVec>;

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

inline bool All(const bool (&array)[1]) {
#pragma HLS inline
  return array[0];
}

template <int N>
inline bool All(const bool (&array)[N]) {
#pragma HLS inline
  return All((const bool(&)[N / 2])(array)) &&
         All((const bool(&)[N - N / 2])(array[N / 2]));
}

template <typename T>
inline void MemSet(T (&array)[1], T value) {
#pragma HLS inline
  array[0] = value;
}

template <typename T, int N>
inline void MemSet(T (&array)[N], T value) {
#pragma HLS inline
  MemSet((T(&)[N / 2])(array), value);
  MemSet((T(&)[N - N / 2])(array[N / 2]), value);
}

#endif  // TAPA_APPS_PAGE_RANK_KERNEL_H_
