#ifndef TLP_APPS_PAGE_RANK_KERNEL_H_
#define TLP_APPS_PAGE_RANK_KERNEL_H_

#include "page-rank.h"

#define VLOG_F(level, tag) VLOG(level) << #tag << "@" << __FUNCTION__ << ": "
#define LOG_F(level, tag) LOG(level) << #tag << "@" << __FUNCTION__ << ": "

struct TaskReq {
  enum Phase { kScatter = 0, kGather = 1 };
  Phase phase;
  Pid pid;
  Vid overall_base_vid;
  Vid base_vid;
  Vid partition_size;
  Vid num_vertices;
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
            << ", base_vid: " << obj.base_vid
            << ", partition_size: " << obj.partition_size
            << ", num_vertices: " << obj.num_vertices
            << ", num_edges: " << obj.num_edges
            << ", vid_offset: " << obj.vid_offset
            << ", eid_offset: " << obj.eid_offset << ", init:" << obj.init
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

struct UpdateWithPid {
  Vid dst;
  float delta;
  Pid pid;
};

inline std::ostream& operator<<(std::ostream& os, const UpdateWithPid& obj) {
  return os << "{dst: " << obj.dst << ", value: " << obj.delta
            << ", pid: " << obj.pid << "}";
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

template <typename T, int N>
inline int FindFirstEmpty(const T (&buffer)[N]) {
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
template <typename T, int N1, int N2>
inline int FindFirstWithoutConflict(const T (&buffer)[N1],
                                    const T (&window)[N2]) {
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

// shift a window by 1 elem
template <typename T, int N>
inline void Shift(T (&window)[N], T elem) {
#pragma HLS inline
#pragma HLS protocol floating
  for (int i = 0; i < N - 1; ++i) {
#pragma HLS unroll
    window[i] = window[i + 1];
  }
  window[N - 1] = elem;
}
template <int N>
inline bool Any(const bool (&valid)[N]) {
  bool ret = false;
  for (int i = 0; i < N; ++i) {
#pragma HLS unroll
    ret |= valid[i];
  }
  return ret;
}

template <typename T>
inline int Arbitrate(const bool (&valid)[2], T priority) {
#pragma HLS inline
#pragma HLS protocol floating
  switch (priority & 1) {
    case 0:
      return valid[0] ? 0 : 1;
    case 1:
      return valid[1] ? 1 : 0;
  }
  return 0;
}

#endif  // TLP_APPS_PAGE_RANK_KERNEL_H_