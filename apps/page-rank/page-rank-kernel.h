#ifndef TLP_APPS_PAGE_RANK_KERNEL_H_
#define TLP_APPS_PAGE_RANK_KERNEL_H_

#include <algorithm>

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

inline bool Valid(const Update& update) {
#pragma HLS inline
  return update.dst != 0;
}

inline bool Valid(const UpdateVec& update_v) {
#pragma HLS inline
  bool valid = false;
  for (int j = 0; j < UpdateVec::length; ++j) {
#pragma HLS unroll
    valid |= Valid(update_v[j]);
  }
  return valid;
}

inline void Invalidate(Update& update) {
#pragma HLS inline
  update.dst = 0;
}

inline void Invalidate(UpdateVec& update_v) {
#pragma HLS inline
  for (int i = 0; i < UpdateVec::length; ++i) {
#pragma HLS unroll
    update_v.set(i, {});
  }
}

inline std::ostream& operator<<(std::ostream& os, const UpdateVecWithPid& obj) {
  return os << "{pid: " << obj.pid << ", updates: " << obj.updates << "}";
}

// return if update conflicts with any of the elements in window[N]
// a and b conflicts if their dsts are the same and != 0
template <int N>
inline bool HasConflict(const Update (&window)[N], Update update) {
#pragma HLS inline
  bool ret = false;
  for (int i = 0; i < N; ++i) {
#pragma HLS unroll
    ret |= window[i].dst == update.dst && Valid(window[i]);
  }
  return ret;
}

template <int N>
inline bool HasConflict(const UpdateVec (&window)[N],
                        const bool (&window_valid)[N], UpdateVec update) {
#pragma HLS inline
  bool ret = false;
  for (int i = 0; i < N; ++i) {
#pragma HLS unroll
    for (int j = 0; j < UpdateVec::length; ++j) {
      ret |= Valid(update[j]) && window[i][j].dst == update[j].dst;
    }
  }
  return ret;
}

template <typename T, int N>
inline int FindFirstEmpty(const T (&buffer)[N]) {
#pragma HLS inline
  for (int i = 0; i < N; ++i) {
#pragma HLS unroll
    if (!Valid(buffer[i])) return i;
  }
  return -1;
}

template <int N>
inline int FindFirstEmpty(const bool (&buffer_valid)[N], bool& output_valid) {
#pragma HLS inline
  for (int i = 0; i < N; ++i) {
#pragma HLS unroll
    if (!buffer_valid[i]) {
      output_valid = true;
      return i;
    }
  }
  output_valid = false;
  return -1;
}

// return the first index in buffer[N1] that do not conflict with window[N2]
template <typename T, int N1, int N2>
inline int FindFirstWithoutConflict(const T (&buffer)[N1],
                                    const T (&window)[N2]) {
#pragma HLS inline
  for (int i = 0; i < N1; ++i) {
#pragma HLS unroll
    if (Valid(buffer[i]) && !HasConflict(window, buffer[i])) {
      return i;
    }
  }
  return -1;
}

// return the first index in buffer[N1] that do not conflict with window[N2]
template <typename T, int N1, int N2>
inline int FindFirstWithoutConflict(const T (&buffer)[N1],
                                    const bool (&buffer_valid)[N1],
                                    const T (&window)[N2],
                                    const bool (&window_valid)[N2],
                                    bool& output_valid) {
#pragma HLS inline
  for (int i = 0; i < N1; ++i) {
#pragma HLS unroll
    if (buffer_valid[i] && !HasConflict(window, window_valid, buffer[i])) {
      output_valid = true;
      return i;
    }
  }
  output_valid = false;
  return -1;
}

// shift a window by 1 elem
template <typename T, int N>
inline void Shift(T (&window)[N], T elem) {
#pragma HLS inline
  for (int i = 0; i < N - 1; ++i) {
#pragma HLS unroll
    window[i] = window[i + 1];
  }
  window[N - 1] = elem;
}

// shift a window by 1 elem
template <typename T, int N>
inline void Shift(T (&window)[N], bool (&window_valid)[N], T elem, bool valid) {
#pragma HLS inline
  for (int i = 0; i < N - 1; ++i) {
#pragma HLS unroll
    window[i] = window[i + 1];
    window_valid[i] = window_valid[i + 1];
  }
  window[N - 1] = elem;
  window_valid[N - 1] = valid;
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
inline T Max(const T (&array)[2]) {
#pragma HLS inline
  return std::max(array[0], array[1]);
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

template <typename T>
inline int Arbitrate(const bool (&valid)[2], T priority) {
#pragma HLS inline
  switch (priority & 1) {
    case 0:
      return valid[0] ? 0 : 1;
    case 1:
      return valid[1] ? 1 : 0;
  }
  return 0;
}

template <typename T>
inline int Arbitrate(const bool (&valid)[4], T priority) {
#pragma HLS inline
  switch (priority & 3) {
    case 0:
      return valid[0] ? 0 : valid[1] ? 1 : valid[2] ? 2 : 3;
    case 1:
      return valid[1] ? 1 : valid[2] ? 2 : valid[3] ? 3 : 0;
    case 2:
      return valid[2] ? 2 : valid[3] ? 3 : valid[0] ? 0 : 1;
    case 3:
      return valid[3] ? 3 : valid[0] ? 0 : valid[1] ? 1 : 2;
  }
  return 0;
}

template <typename T>
T Reg(const T& x) {
#pragma HLS inline self off
#pragma HLS interface ap_none register port = return
  return x;
}

#endif  // TLP_APPS_PAGE_RANK_KERNEL_H_