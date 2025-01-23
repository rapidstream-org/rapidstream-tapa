// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#pragma once

#include <cassert>
#include <cstring>

#include <iostream>

#include <tapa.h>

// TODO: remove `__has_include` guards when we drop support for 2022.1.
#if __has_include(<hls_fence.h>)
#include <hls_fence.h>
#endif

using Vid = uint32_t;
using Eid = uint32_t;
using Pid = uint16_t;

using VertexAttr = Vid;

using std::ostream;

struct Edge {
  Vid src;
  Vid dst;
};

struct TaskReq {
  enum Phase { kScatter = 0, kGather = 1 };
  Phase phase;
  bool IsScatter() const { return phase == kScatter; }
  bool IsGather() const { return phase == kGather; }
  Pid pid;
  Vid base_vid;
  Vid num_vertices;
  Eid num_edges;
  Vid vid_offset;
  Eid eid_offset;
};

inline ostream& operator<<(ostream& os, const TaskReq::Phase& obj) {
  return os << (obj == TaskReq::kScatter ? "SCATTER" : "GATHER");
}

inline ostream& operator<<(ostream& os, const TaskReq& obj) {
  return os << "{phase: " << obj.phase << ", pid: " << obj.pid
            << ", base_vid: " << obj.base_vid
            << ", num_vertices: " << obj.num_vertices
            << ", num_edges: " << obj.num_edges
            << ", vid_offset: " << obj.vid_offset
            << ", eid_offset: " << obj.eid_offset << "}";
}

struct TaskResp {
  TaskReq::Phase phase;
  Pid pid;
  bool active;
};

inline ostream& operator<<(ostream& os, const TaskResp& obj) {
  return os << "{phase: " << obj.phase << ", pid: " << obj.pid
            << ", active: " << obj.active << "}";
}

struct Update {
  Vid dst;
  Vid value;
};

inline ostream& operator<<(ostream& os, const Update& obj) {
  return os << "{dst: " << obj.dst << ", value: " << obj.value << "}";
}

struct UpdateConfig {
  enum Item { kBaseVid = 0, kPartitionSize = 1, kUpdateOffset = 2 };
  Item item;
  Vid vid;
  Eid eid;
};

inline ostream& operator<<(ostream& os, const UpdateConfig::Item& obj) {
  switch (obj) {
    case UpdateConfig::kBaseVid:
      os << "BASE_VID";
      break;
    case UpdateConfig::kPartitionSize:
      os << "PARTITION_SIZE";
      break;
    case UpdateConfig::kUpdateOffset:
      os << "UPDATE_OFFSET";
      break;
  }
  return os;
}

inline ostream& operator<<(ostream& os, const UpdateConfig& obj) {
  os << "{item: " << obj.item;
  switch (obj.item) {
    case UpdateConfig::kBaseVid:
    case UpdateConfig::kPartitionSize:
      os << ", vid: " << obj.vid;
      break;
    case UpdateConfig::kUpdateOffset:
      os << ", eid: " << obj.eid;
      break;
  }
  return os << "}";
}

struct UpdateReq {
  TaskReq::Phase phase;
  Pid pid;
};

inline ostream& operator<<(ostream& os, const UpdateReq& obj) {
  return os << "{phase: " << obj.phase << ", pid: " << obj.pid << "}";
}

const int kMaxNumPartitions = 1024;
const int kMaxPartitionSize = 1024 * 32;

void Graph(Pid num_partitions, tapa::mmap<const Vid> num_vertices,
           tapa::mmap<const Eid> num_edges, tapa::mmap<VertexAttr> vertices,
           tapa::mmap<const Edge> edges, tapa::mmap<Update> updates);
