#include <cassert>
#include <cstring>

#include <algorithm>

#include <tlp.h>

#include "page-rank.h"

using std::ostream;

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
  float init;
};

ostream& operator<<(ostream& os, const TaskReq::Phase& obj) {
  return os << (obj == TaskReq::kScatter ? "SCATTER" : "GATHER");
}

ostream& operator<<(ostream& os, const TaskReq& obj) {
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

ostream& operator<<(ostream& os, const TaskResp& obj) {
  return os << "{phase: " << obj.phase << ", pid: " << obj.pid
            << ", active: " << obj.active << "}";
}

ostream& operator<<(ostream& os, const Update& obj) {
  return os << "{dst: " << obj.dst << ", value: " << obj.delta << "}";
}

struct UpdateConfig {
  enum Item { kBaseVid = 0, kPartitionSize = 1, kUpdateOffset = 2 };
  Item item;
  Vid vid;
  Eid eid;
};

ostream& operator<<(ostream& os, const UpdateConfig::Item& obj) {
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

ostream& operator<<(ostream& os, const UpdateConfig& obj) {
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

ostream& operator<<(ostream& os, const UpdateReq& obj) {
  return os << "{phase: " << obj.phase << ", pid: " << obj.pid << "}";
}

const int kMaxNumPartitions = 2048;
const int kMaxPartitionSize = 1024 * 32;

void Control(Pid num_partitions, tlp::mmap<const Vid> num_vertices,
             tlp::mmap<const Eid> num_edges,
             tlp::ostream<UpdateConfig>& update_config_q,
             tlp::ostream<TaskReq>& req_q, tlp::istream<TaskResp>& resp_q) {
  // Keeps track of all partitions.

  // Vid of the 0-th vertex in each partition.
  Vid base_vids[kMaxNumPartitions];
#pragma HLS resource variable = base_vids latency = 4
  // Number of vertices in each partition.
  Vid num_vertices_local[kMaxNumPartitions];
#pragma HLS resource variable = num_vertices_local latency = 4
  // Number of edges in each partition.
  Eid num_edges_local[kMaxNumPartitions];
#pragma HLS resource variable = num_edges_local latency = 4
  // Memory offset of the 0-th vertex in each partition.
  Vid vid_offsets[kMaxNumPartitions];
#pragma HLS resource variable = vid_offsets latency = 4
  // Memory offset of the 0-th edge in each partition.
  Eid eid_offsets[kMaxNumPartitions];
#pragma HLS resource variable = eid_offsets latency = 4

  Vid base_vid_acc = num_vertices[0];
  Vid vid_offset_acc = 0;
  Eid eid_offset_acc = 0;
  for (Pid pid = 0; pid < num_partitions; ++pid) {
#pragma HLS pipeline II = 1
    Vid num_vertices_delta = num_vertices[pid + 1];
    Eid num_edges_delta = num_edges[pid];

    base_vids[pid] = base_vid_acc;
    num_vertices_local[pid] = num_vertices_delta;
    num_edges_local[pid] = num_edges_delta;
    vid_offsets[pid] = vid_offset_acc;
    eid_offsets[pid] = eid_offset_acc;

    base_vid_acc += num_vertices_delta;
    vid_offset_acc += num_vertices_delta;
    eid_offset_acc += num_edges_delta;
  }
  const Vid total_num_vertices = base_vid_acc - base_vids[0];

  // Initialize UpdateHandler, needed only once per execution.
  update_config_q.write({UpdateConfig::kBaseVid, base_vids[0], 0});
  update_config_q.write(
      {UpdateConfig::kPartitionSize, num_vertices_local[0], 0});
  for (Pid pid = 0; pid < num_partitions; ++pid) {
    VLOG(8) << "info@Control: eid offset[" << pid
            << "]: " << num_edges[num_partitions + pid];
    UpdateConfig info{UpdateConfig::kUpdateOffset, 0,
                      num_edges[num_partitions + pid]};
    update_config_q.write(info);
  }
  update_config_q.close();

  bool all_done = false;
  while (!all_done) {
    all_done = true;

    // Do the scatter phase for each partition, if active.
    for (Pid pid = 0; pid < num_partitions; ++pid) {
      TaskReq req{TaskReq::kScatter,    pid,
                  base_vids[pid],       num_vertices_local[pid],
                  num_edges_local[pid], vid_offsets[pid],
                  eid_offsets[pid],     0.f};
      req_q.write(req);
    }

    // Wait until all partitions are done with the scatter phase.
    for (Pid pid = 0; pid < num_partitions;) {
      bool succeeded;
      TaskResp resp = resp_q.read(succeeded);
      if (succeeded) {
        assert(resp.phase == TaskReq::kScatter);
        ++pid;
      }
    }

    // Do the gather phase for each partition.
    for (Pid pid = 0; pid < num_partitions; ++pid) {
      TaskReq req{
          TaskReq::kGather,     pid,
          base_vids[pid],       num_vertices_local[pid],
          num_edges_local[pid], vid_offsets[pid],
          eid_offsets[pid],     (1.f - kDampingFactor) / total_num_vertices};
      req_q.write(req);
    }

    // Wait until all partitions are done with the gather phase.
    for (Pid pid = 0; pid < num_partitions;) {
      bool succeeded;
      TaskResp resp = resp_q.read(succeeded);
      if (succeeded) {
        assert(resp.phase == TaskReq::kGather);
        VLOG(3) << "recv@Control: " << resp;
        if (resp.active) {
          all_done = false;
        }
        ++pid;
      }
    }
    VLOG(3) << "info@Control: " << (all_done ? "" : "not ") << "all done";
  }

  // Terminates the ProcElem.
  req_q.close();
}

void UpdateHandler(Pid num_partitions,
                   tlp::istream<UpdateConfig>& update_config_q,
                   tlp::istream<UpdateReq>& update_req_q,
                   tlp::istream<Update>& update_in_q,
                   tlp::ostream<Update>& update_out_q,
                   tlp::mmap<Update> updates) {
  // Base vid of all vertices; used to determine dst partition id.
  Vid base_vid = 0;
  // Used to determine dst partition id.
  Vid partition_size = 1;
  // Memory offsets of each update partition.
  Eid update_offsets[kMaxNumPartitions];
#pragma HLS resource variable = update_offsets latency = 4
  // Number of updates of each update partition in memory.
  Eid num_updates[kMaxNumPartitions] = {};

  // Initialization; needed only once per execution.
  int update_offset_idx = 0;
  TLP_WHILE_NOT_EOS(update_config_q) {
#pragma HLS pipeline II = 1
    bool succeeded;
    auto config = update_config_q.read(succeeded);
    VLOG(5) << "recv@UpdateHandler: UpdateConfig: " << config;
    switch (config.item) {
      case UpdateConfig::kBaseVid:
        base_vid = config.vid;
        break;
      case UpdateConfig::kPartitionSize:
        partition_size = config.vid;
        break;
      case UpdateConfig::kUpdateOffset:
        update_offsets[update_offset_idx] = config.eid;
        ++update_offset_idx;
        break;
    }
  }

  TLP_WHILE_NOT_EOS(update_req_q) {
    // Each UpdateReq either requests forwarding all Updates from ProcElem to
    // the memory (scatter phase), or requests forwarding all Updates from the
    // memory to ProcElem (gather phase).
    const auto update_req = update_req_q.read();
    VLOG(5) << "recv@UpdateHandler: UpdateReq: " << update_req;
    if (update_req.phase == TaskReq::kScatter) {
      TLP_WHILE_NOT_EOS(update_in_q) {
#pragma HLS pipeline II = 1
        bool succeeded;
        Update update = update_in_q.read(succeeded);
        VLOG(5) << "recv@UpdateHandler: Update: " << update;
        Pid pid = (update.dst - base_vid) / partition_size;
        VLOG(5) << "info@UpdateHandler: dst partition id: " << pid;
        Eid update_idx = num_updates[pid];
        Eid update_offset = update_offsets[pid] + update_idx;
        updates[update_offset] = update;
        num_updates[pid] = update_idx + 1;
      }
      update_in_q.open();
    } else {
      const auto pid = update_req.pid;
      const auto num_updates_pid = num_updates[pid];
      VLOG(6) << "info@UpdateHandler: num_updates[" << pid
              << "]: " << num_updates_pid;
      for (Eid update_idx = 0; update_idx < num_updates_pid; ++update_idx) {
        Eid update_offset = update_offsets[pid] + update_idx;
        VLOG(5) << "send@UpdateHandler: update_offset: " << update_offset
                << " Update: " << updates[update_offset];
        update_out_q.write(updates[update_offset]);
      }
      num_updates[pid] = 0;  // Reset for the next scatter phase.
      update_out_q.close();
    }
  }
  VLOG(3) << "info@UpdateHandler: done";
}

void ProcElem(tlp::istream<TaskReq>& req_q, tlp::ostream<TaskResp>& resp_q,
              tlp::ostream<UpdateReq>& update_req_q,
              tlp::istream<Update>& update_in_q,
              tlp::ostream<Update>& update_out_q,
              tlp::mmap<VertexAttr> vertices, tlp::mmap<const Edge> edges) {
  VertexAttr vertices_local[kMaxPartitionSize];
  TLP_WHILE_NOT_EOS(req_q) {
    const TaskReq req = req_q.read();
    VLOG(5) << "recv@ProcElem: TaskReq: " << req;
    update_req_q.write({req.phase, req.pid});
    bool active = false;
    if (req.IsScatter()) {
      memcpy(vertices_local, vertices + req.vid_offset,
             req.num_vertices * sizeof(VertexAttr));
      for (Eid eid = 0; eid < req.num_edges; ++eid) {
        auto edge = edges[req.eid_offset + eid];
        auto src = vertices_local[edge.src - req.base_vid];
        // use pre-computed src.tmp = src.ranking / src.out_degree
        Update update{edge.dst, src.tmp};
        VLOG(5) << "send@ProcElem: Update: " << update;
        update_out_q.write(update);
      }
      update_out_q.close();
    } else {
      for (Vid i = 0; i < req.num_vertices; ++i) {
#pragma HLS pipeline II = 1
        vertices_local[i] = vertices[req.vid_offset + i];
        vertices_local[i].tmp = 0.f;
      }
      TLP_WHILE_NOT_EOS(update_in_q) {
#pragma HLS pipeline II = 1
        bool succeeded;
        auto update = update_in_q.read(succeeded);
        VLOG(5) << "recv@ProcElem: Update: " << update;
        vertices_local[update.dst - req.base_vid].tmp += update.delta;
      }
      update_in_q.open();
      float max_delta = 0.f;
      for (Vid i = 0; i < req.num_vertices; ++i) {
#pragma HLS pipeline II = 1
        auto vertex = vertices_local[i];
        const float new_ranking = req.init + vertex.tmp * kDampingFactor;
        const float abs_delta = std::abs(new_ranking - vertex.ranking);
        max_delta = std::max(max_delta, abs_delta);
        vertex.ranking = new_ranking;
        // pre-compute vertex.tmp = vertex.ranking / vertex.out_degree
        vertex.tmp = vertex.ranking / vertex.out_degree;
        vertices[req.vid_offset + i] = vertex;
      }
      active = max_delta > kConvergenceThreshold;
    }
    TaskResp resp{req.phase, req.pid, active};
    resp_q.write(resp);
  }

  // Terminates the UpdateHandler.
  update_req_q.close();
}

void PageRank(Pid num_partitions, tlp::mmap<const Vid> num_vertices,
              tlp::mmap<const Eid> num_edges, tlp::mmap<VertexAttr> vertices,
              tlp::mmap<const Edge> edges, tlp::mmap<Update> updates) {
  tlp::stream<TaskReq, kMaxNumPartitions> task_req("task_req");
  tlp::stream<TaskResp, 32> task_resp("task_resp");
  tlp::stream<Update, 32> update_pe2handler("update_pe2handler");
  tlp::stream<Update, 32> update_handler2pe("update_handler2pe");
  tlp::stream<UpdateConfig, 32> update_config("update_config");
  tlp::stream<UpdateReq, 32> update_req("update_req");

  tlp::task()
      .invoke<0>(Control, num_partitions, num_vertices, num_edges,
                 update_config, task_req, task_resp)
      .invoke<0>(UpdateHandler, num_partitions, update_config, update_req,
                 update_pe2handler, update_handler2pe, updates)
      .invoke<0>(ProcElem, task_req, task_resp, update_req, update_handler2pe,
                 update_pe2handler, vertices, edges);
}
