// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include "graph.h"

void Control(Pid num_partitions, tapa::mmap<const Vid> num_vertices,
             tapa::mmap<const Eid> num_edges,
             tapa::ostream<UpdateConfig>& update_config_q,
             tapa::ostream<TaskReq>& req_q, tapa::istream<TaskResp>& resp_q) {
  // Keeps track of all partitions.

  // Vid of the 0-th vertex in each partition.
  Vid base_vids[kMaxNumPartitions];
  // Number of vertices in each partition.
  Vid num_vertices_local[kMaxNumPartitions];
  // Number of edges in each partition.
  Eid num_edges_local[kMaxNumPartitions];
  // Memory offset of the 0-th vertex in each partition.
  Vid vid_offsets[kMaxNumPartitions];
  // Memory offset of the 0-th edge in each partition.
  Eid eid_offsets[kMaxNumPartitions];

  Vid base_vid_acc = num_vertices[0];
  Vid vid_offset_acc = 0;
  Eid eid_offset_acc = 0;
  bool done[kMaxNumPartitions] = {};
init_arrays:
  [[tapa::pipeline(1)]] for (Pid pid = 0; pid < num_partitions; ++pid) {
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

  // Initialize UpdateHandler, needed only once per execution.
  update_config_q.write({UpdateConfig::kBaseVid, base_vids[0], 0});
  update_config_q.write(
      {UpdateConfig::kPartitionSize, num_vertices_local[0], 0});
init_update_handler:
  for (Pid pid = 0; pid < num_partitions; ++pid) {
    VLOG(8) << "info@Control: eid offset[" << pid
            << "]: " << eid_offset_acc * pid;
    UpdateConfig info{UpdateConfig::kUpdateOffset, 0, eid_offset_acc * pid};
    update_config_q.write(info);
  }
  update_config_q.close();

  bool all_done = false;
  while (!all_done) {
    all_done = true;

    // Do the scatter phase for each partition, if active.
  scatter_req:
    for (Pid pid = 0; pid < num_partitions; ++pid) {
      if (!done[pid]) {
        TaskReq req{TaskReq::kScatter,    pid,
                    base_vids[pid],       num_vertices_local[pid],
                    num_edges_local[pid], vid_offsets[pid],
                    eid_offsets[pid]};
        req_q.write(req);
      }
    }

    // Make sure both ostreams has been written before reading responses.
#if __has_include(<hls_fence.h>)
    // TODO: use half-fence once we drop support for 2023.2.
    hls::fence(update_config_q, req_q, resp_q);
#endif

    // Wait until all partitions are done with the scatter phase.
  scatter_resp:
    for (Pid pid = 0; pid < num_partitions;) {
      if (!done[pid]) {
        bool succeeded;
        TaskResp resp = resp_q.read(succeeded);
        if (succeeded) {
          assert(resp.phase == TaskReq::kScatter);
          ++pid;
        }
      } else {
        ++pid;
      }
    }

    // Do the gather phase for each partition.
  gather_req:
    for (Pid pid = 0; pid < num_partitions; ++pid) {
      TaskReq req{TaskReq::kGather,     pid,
                  base_vids[pid],       num_vertices_local[pid],
                  num_edges_local[pid], vid_offsets[pid],
                  eid_offsets[pid]};
      req_q.write(req);
    }

    // Make sure requests has been written before reading responses.
#if __has_include(<hls_fence.h>)
    // TODO: use half-fence once we drop support for 2023.2.
    hls::fence(req_q, resp_q);
#endif

    // Wait until all partitions are done with the gather phase.
  gather_resp:
    for (Pid pid = 0; pid < num_partitions;) {
      bool succeeded;
      TaskResp resp = resp_q.read(succeeded);
      if (succeeded) {
        assert(resp.phase == TaskReq::kGather);
        VLOG(3) << "recv@Control: " << resp;
        if (resp.active) {
          all_done = false;
        } else {
          done[pid] = true;
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
                   tapa::istream<UpdateConfig>& update_config_q,
                   tapa::istream<UpdateReq>& update_req_q,
                   tapa::istream<Update>& update_in_q,
                   tapa::ostream<Update>& update_out_q,
                   tapa::mmap<Update> updates) {
  // Base vid of all vertices; used to determine dst partition id.
  Vid base_vid = 0;
  // Used to determine dst partition id.
  Vid partition_size = 1;
  // Memory offsets of each update partition.
  Eid update_offsets[kMaxNumPartitions] = {};
  // Number of updates of each update partition in memory.
  Eid num_updates[kMaxNumPartitions] = {};

  // Initialization; needed only once per execution.
  int update_offset_idx = 0;
init:
  [[tapa::pipeline(1)]] TAPA_WHILE_NOT_EOT(update_config_q) {
    auto config = update_config_q.read(nullptr);
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
  update_config_q.open();

  TAPA_WHILE_NOT_EOT(update_req_q) {
    // Each UpdateReq either requests forwarding all Updates from ProcElem to
    // the memory (scatter phase), or requests forwarding all Updates from the
    // memory to ProcElem (gather phase).
    const auto update_req = update_req_q.read();
    VLOG(5) << "recv@UpdateHandler: UpdateReq: " << update_req;
    if (update_req.phase == TaskReq::kScatter) {
    scatter:
      [[tapa::pipeline(1)]] TAPA_WHILE_NOT_EOT(update_in_q) {
        Update update = update_in_q.read(nullptr);
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
      auto num_updates_pid = num_updates[pid];
      VLOG(6) << "info@UpdateHandler: num_updates[" << pid
              << "]: " << num_updates_pid;
    gather:
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
  update_req_q.open();
  VLOG(3) << "info@UpdateHandler: done";
}

void ProcElem(tapa::istream<TaskReq>& req_q, tapa::ostream<TaskResp>& resp_q,
              tapa::ostream<UpdateReq>& update_req_q,
              tapa::istream<Update>& update_in_q,
              tapa::ostream<Update>& update_out_q,
              tapa::mmap<VertexAttr> vertices, tapa::mmap<const Edge> edges) {
  VertexAttr vertices_local[kMaxPartitionSize];
#pragma HLS bind_storage variable = vertices_local type = RAM_T2P impl = URAM

  TAPA_WHILE_NOT_EOT(req_q) {
    const TaskReq req = req_q.read();
    VLOG(5) << "recv@ProcElem: TaskReq: " << req;
    update_req_q.write({req.phase, req.pid});
    memcpy(vertices_local, vertices + req.vid_offset,
           req.num_vertices * sizeof(VertexAttr));
    bool active = false;
    if (req.IsScatter()) {
    scatter:
      for (Eid eid = 0; eid < req.num_edges; ++eid) {
        auto edge = edges[req.eid_offset + eid];
        auto vertex_attr = vertices_local[edge.src - req.base_vid];
        Update update;
        update.dst = edge.dst;
        update.value = vertex_attr;
        VLOG(5) << "send@ProcElem: Update: " << update;
        update_out_q.write(update);
      }
      update_out_q.close();
    } else {
    gather:
      [[tapa::pipeline(1)]] TAPA_WHILE_NOT_EOT(update_in_q) {
#pragma HLS dependence false variable = vertices_local
        auto update = update_in_q.read(nullptr);
        VLOG(5) << "recv@ProcElem: Update: " << update;
        auto idx = update.dst - req.base_vid;
        auto old_vertex_value = vertices_local[idx];
        if (update.value < old_vertex_value) {
          vertices_local[idx] = update.value;
          active = true;
        }
      }
      update_in_q.open();
      memcpy(vertices + req.vid_offset, vertices_local,
             req.num_vertices * sizeof(VertexAttr));
    }
    TaskResp resp{req.phase, req.pid, active};
    resp_q.write(resp);
  }
  req_q.open();

  // Terminates the UpdateHandler.
  update_req_q.close();
}

void Graph(Pid num_partitions, tapa::mmap<const Vid> num_vertices,
           tapa::mmap<const Eid> num_edges, tapa::mmap<VertexAttr> vertices,
           tapa::mmap<const Edge> edges, tapa::mmap<Update> updates) {
  tapa::stream<TaskReq, kMaxNumPartitions> task_req("task_req");
  tapa::stream<TaskResp, 32> task_resp("task_resp");
  tapa::stream<Update, 32> update_pe2handler("update_pe2handler");
  tapa::stream<Update, 32> update_handler2pe("update_handler2pe");
  tapa::stream<UpdateConfig, 32> update_config("update_config");
  tapa::stream<UpdateReq, 32> update_req("update_req");

  tapa::task()
      .invoke(Control, num_partitions, num_vertices, num_edges, update_config,
              task_req, task_resp)
      .invoke(UpdateHandler, num_partitions, update_config, update_req,
              update_pe2handler, update_handler2pe, updates)
      .invoke(ProcElem, task_req, task_resp, update_req, update_handler2pe,
              update_pe2handler, vertices, edges);
}
