#include <cassert>
#include <cstring>

#include <algorithm>

#include <tlp.h>

#include "page-rank-kernel.h"

constexpr int kMaxNumPartitions = 2048;
constexpr int kMaxPartitionSize = 1024 * 256;
constexpr int kEstimatedLatency = 50;

void Control(Pid num_partitions, tlp::mmap<uint64_t> metadata,
             // to UpdateHandler
             tlp::ostream<Eid>& update_config_q0,
             tlp::ostream<Eid>& update_config_q1,
             tlp::ostream<bool>& update_phase_q0,
             tlp::ostream<bool>& update_phase_q1,
             // from UpdateHandler
             tlp::istream<NumUpdates>& num_updates_q0,
             tlp::istream<NumUpdates>& num_updates_q1,
             // to ProcElem
             tlp::ostream<TaskReq>& task_req_q0,
             tlp::ostream<TaskReq>& task_req_q1,
             // from ProcElem
             tlp::istream<TaskResp>& task_resp_q0,
             tlp::istream<TaskResp>& task_resp_q1) {
  // HLS crashes without this...
  update_config_q0.close();
  update_config_q1.close();
  update_phase_q0.close();
  update_phase_q1.close();
  task_req_q0.close();
  task_req_q1.close();

  // Keeps track of all partitions.
  // Vid of the 0-th vertex in each partition.
  const Vid base_vid = metadata[0];
  const Vid partition_size = metadata[1];
  const Vid total_num_vertices = partition_size * num_partitions;
  // Number of edges in each partition of each PE.
  Eid num_edges_local[kMaxNumPartitions][kNumPes];
#pragma HLS array_partition complete variable = num_edges_local dim = 2
  // Number of updates in each partition.
  Eid num_updates_local[kMaxNumPartitions];
  // Memory offset of the 0-th edge in each partition.
  Eid eid_offsets[kMaxNumPartitions][kNumPes];
#pragma HLS array_partition complete variable = eid_offsets dim = 2

  Eid eid_offset_acc[kNumPes] = {};
#pragma HLS array_partition complete variable = eid_offset_acc

load_config:
  for (Pid i = 0; i < num_partitions * kNumPes; ++i) {
#pragma HLS pipeline II = 1
    Pid pid = i / kNumPes;
    Pid pe = i % kNumPes;
    Eid num_edges_delta = metadata[i + 2];
    num_edges_local[pid][pe] = num_edges_delta;
    eid_offsets[pid][pe] = eid_offset_acc[pe];
    eid_offset_acc[pe] += num_edges_delta;
  }

  // Initialize UpdateMem, needed only once per execution.
config_update_offsets:
  for (Pid pid = 0; pid < num_partitions; ++pid) {
    auto update_offset = metadata[2 + num_partitions * kNumPes + pid];
    VLOG_F(8, info) << "update offset[" << pid << "]: " << update_offset;
    switch (pid % kNumPes) {
      case 0:
        update_config_q0.write(update_offset);
        break;
      case 1:
        update_config_q1.write(update_offset);
        break;
    }
  }
  // Tells UpdateHandler start to wait for phase requests.
  update_config_q0.close();
  update_config_q1.close();

  bool all_done = false;
  int iter = 0;
bulk_steps:
  while (!all_done) {
    all_done = true;

    // Do the scatter phase for each partition, if active.
    // Wait until all PEs are done with the scatter phase.
    VLOG_F(5, info) << "Phase: " << TaskReq::kScatter;
    update_phase_q0.write(false);
    update_phase_q1.write(false);

  scatter:
    for (Pid pid_send[kNumPes] = {}, pid_recv = 0;
         pid_recv < num_partitions * kNumPes;) {
#pragma HLS pipeline II = 1
      for (int pe = 0; pe < kNumPes; ++pe) {
#pragma HLS unroll
        if (pid_send[pe] < num_partitions) {
          TaskReq req{TaskReq::kScatter,
                      pid_send[pe],
                      base_vid,
                      base_vid + partition_size * pid_send[pe],
                      partition_size,
                      partition_size,
                      num_edges_local[pid_send[pe]][pe],
                      pid_send[pe] * partition_size,
                      eid_offsets[pid_send[pe]][pe],
                      0.f,
                      false};
          switch (pe) {
            case 0:
              if (task_req_q0.try_write(req)) ++pid_send[pe];
              break;
            case 1:
              if (task_req_q1.try_write(req)) ++pid_send[pe];
              break;
          }
        }
      }
      TaskResp resp;
      if (task_resp_q0.try_read(resp)) {
        assert(resp.phase == TaskReq::kScatter);
        ++pid_recv;
      }
      if (task_resp_q1.try_read(resp)) {
        assert(resp.phase == TaskReq::kScatter);
        ++pid_recv;
      }
    }

    // Tell PEs to tell UpdateHandlers that the scatter phase is done.
    TaskReq req = {};
    req.scatter_done = true;
    task_req_q0.write(req);
    task_req_q1.write(req);

    // Get prepared for the gather phase.
    VLOG_F(5, info) << "Phase: " << TaskReq::kGather;
    update_phase_q0.write(true);
    update_phase_q1.write(true);

  reset_num_updates:
    for (Pid pid = 0; pid < num_partitions; ++pid) {
#pragma HLS pipeline II = 1
      num_updates_local[pid] = 0;
    }

  collect_num_updates:
    for (Pid pid_recv = 0; pid_recv < RoundUp<kNumPes>(num_partitions);) {
#pragma HLS dependence false variable = num_updates_local
      NumUpdates num_updates;
      if (num_updates_q0.try_read(num_updates)) {
        Pid pid = num_updates.pid * kNumPes + 0;
        if (pid < num_partitions) {
          VLOG_F(5, recv) << "num_updates: " << num_updates;
          num_updates_local[pid] += num_updates.num_updates;
        }
        ++pid_recv;
      } else if (num_updates_q1.try_read(num_updates)) {
        Pid pid = num_updates.pid * kNumPes + 1;
        if (pid < num_partitions) {
          VLOG_F(5, recv) << "num_updates: " << num_updates;
          num_updates_local[pid] += num_updates.num_updates;
        }
        ++pid_recv;
      }
    }

    // updates.fence()
#ifdef __SYNTHESIS__
    ap_wait_n(80);
#endif

    // Do the gather phase for each partition.
    // Wait until all partitions are done with the gather phase.
  gather:
    for (Pid pid_send[kNumPes] = {}, pid_recv = 0; pid_recv < num_partitions;) {
#pragma HLS pipeline II = 1
      for (int pe = 0; pe < kNumPes; ++pe) {
#pragma HLS unroll
        Pid pid = pid_send[pe] * kNumPes + pe;
        if (pid < num_partitions) {
          TaskReq req{TaskReq::kGather,
                      pid,
                      base_vid,
                      base_vid + partition_size * pid,
                      partition_size,
                      partition_size,
                      num_updates_local[pid],
                      partition_size * pid,
                      0,  // eid_offset, unused
                      (1.f - kDampingFactor) / total_num_vertices,
                      false};
          switch (pe) {
            case 0:
              if (task_req_q0.try_write(req)) ++pid_send[pe];
              break;
            case 1:
              if (task_req_q1.try_write(req)) ++pid_send[pe];
              break;
          }
        }
      }

      TaskResp resp;
      if (task_resp_q0.try_read(resp)) {
        assert(resp.phase == TaskReq::kGather);
        VLOG_F(3, recv) << resp;
        if (resp.active) all_done = false;
        ++pid_recv;
      }
      if (task_resp_q1.try_read(resp)) {
        assert(resp.phase == TaskReq::kGather);
        VLOG_F(3, recv) << resp;
        if (resp.active) all_done = false;
        ++pid_recv;
      }
    }
    VLOG_F(3, info) << (all_done ? "" : "not ") << "all done";
    ++iter;
  }
  // Terminates UpdateHandler.
  update_phase_q0.close();
  update_phase_q1.close();
  task_req_q0.close();
  task_req_q1.close();

  metadata[2 + num_partitions * kNumPes + num_partitions] = iter;
}

void VertexMem(tlp::istream<VertexReq>& vertex_req_q0,
               tlp::istream<VertexReq>& vertex_req_q1,
               tlp::istream<VertexAttrVec>& vertex_in_q0,
               tlp::istream<VertexAttrVec>& vertex_in_q1,
               tlp::ostream<VertexAttrVec>& vertex_out_q0,
               tlp::ostream<VertexAttrVec>& vertex_out_q1,
               tlp::async_mmap<VertexAttrAlignedVec> vertices) {
  for (;;) {
    VertexReq req;
    if (vertex_req_q0.try_read(req)) {
      VertexAttrAlignedVec resp;
      bool valid = false;
    vertices_0:
      for (Vid i_wr = 0, i_rd_req = 0, i_rd_resp = 0;
           req.phase == TaskReq::kScatter ? i_rd_resp < req.length
                                          : i_wr < req.length;) {
        // Read vertices from DRAM.
        // Send requests.
        if (i_rd_req < req.length &&
            (req.phase == TaskReq::kScatter ||
             i_rd_req < i_wr + kEstimatedLatency * kVertexVecLen) &&
            vertices.read_addr_try_write((req.offset + i_rd_req) /
                                         kVertexVecLen)) {
          i_rd_req += kVertexVecLen;
        }
        // Handle responses.
        if (i_rd_resp < req.length) {
          if (!valid) valid = vertices.read_data_try_read(resp);
          if (valid && vertex_out_q0.try_write(resp)) {
            i_rd_resp += kVertexVecLen;
            valid = false;
          }
        }

        // Write vertices to DRAM.
        if (req.phase == TaskReq::kGather && !vertex_in_q0.empty()) {
          auto v = vertex_in_q0.peek(nullptr);
          if (vertices.write_data_try_write(v)) {
            vertex_in_q0.read(nullptr);
            vertices.write_addr_write((req.offset + i_wr) / kVertexVecLen);
            i_wr += kVertexVecLen;
          }
        }
      }
    } else if (vertex_req_q1.try_read(req)) {
      VertexAttrAlignedVec resp;
      bool valid = false;
    vertices_1:
      for (Vid i_wr = 0, i_rd_req = 0, i_rd_resp = 0;
           req.phase == TaskReq::kScatter ? i_rd_resp < req.length
                                          : i_wr < req.length;) {
        // Read vertices from DRAM.
        // Send requests.
        if (i_rd_req < req.length &&
            (req.phase == TaskReq::kScatter ||
             i_rd_req < i_wr + kEstimatedLatency * kVertexVecLen) &&
            vertices.read_addr_try_write((req.offset + i_rd_req) /
                                         kVertexVecLen)) {
          i_rd_req += kVertexVecLen;
        }
        // Handle responses.
        if (i_rd_resp < req.length) {
          if (!valid) valid = vertices.read_data_try_read(resp);
          if (valid && vertex_out_q1.try_write(resp)) {
            i_rd_resp += kVertexVecLen;
            valid = false;
          }
        }

        // Write vertices to DRAM.
        if (req.phase == TaskReq::kGather && !vertex_in_q1.empty()) {
          auto v = vertex_in_q1.peek(nullptr);
          if (vertices.write_data_try_write(v)) {
            vertex_in_q1.read(nullptr);
            vertices.write_addr_write((req.offset + i_wr) / kVertexVecLen);
            i_wr += kVertexVecLen;
          }
        }
      }
    }
  }
}

// Handles edge read requests.
void EdgeMem(tlp::istream<Eid>& edge_req_q, tlp::ostream<EdgeVec>& edge_resp_q,
             tlp::async_mmap<EdgeVec> edges) {
  bool valid = false;
  EdgeVec edge_v;
  for (;;) {
#pragma HLS pipeline II = 1
    // Handle responses.
    if (!valid) valid = edges.read_data_try_read(edge_v);
    if (valid && edge_resp_q.try_write(edge_v)) valid = false;

    // Handle requests.
    if (!edge_req_q.empty() &&
        edges.read_addr_try_write(edge_req_q.peek(nullptr))) {
      edge_req_q.read(nullptr);
    }
  }
}

void UpdateMem(tlp::istream<uint64_t>& read_addr_q,
               tlp::ostream<UpdateVec>& read_data_q,
               tlp::istream<uint64_t>& write_addr_q,
               tlp::istream<UpdateVec>& write_data_q,
               tlp::async_mmap<UpdateVec> updates) {
  bool valid = false;
  UpdateVec update_v;
  for (;;) {
#pragma HLS pipeline II = 1
    // Handle read responses.
    if (!valid) valid = updates.read_data_try_read(update_v);
    if (valid && read_data_q.try_write(update_v)) valid = false;

    // Handle read requests.
    if (!read_addr_q.empty() &&
        updates.read_addr_try_write(read_addr_q.peek(nullptr))) {
      read_addr_q.read(nullptr);
    }

    // Handle write requests.
    if (!write_addr_q.empty() && !write_data_q.empty() &&
        updates.write_addr_try_write(write_addr_q.peek(nullptr))) {
      write_addr_q.read(nullptr);
      updates.write_data_write(write_data_q.read(nullptr));
    }
  }
}

void UpdateHandler(Pid num_partitions,
                   // from Control
                   tlp::istream<Eid>& update_config_q,
                   tlp::istream<bool>& update_phase_q,
                   // to Control
                   tlp::ostream<NumUpdates>& num_updates_out_q,
                   // from ProcElem via UpdateRouter
                   tlp::istream<UpdateReq>& update_req_q,
                   tlp::istream<UpdateVecWithPid>& update_in_q,
                   // to ProcElem via UpdateReorderer
                   tlp::ostream<UpdateVec>& update_out_q,
                   // to and from UpdateMem
                   tlp::ostream<uint64_t>& updates_read_addr_q,
                   tlp::istream<UpdateVec>& updates_read_data_q,
                   tlp::ostream<uint64_t>& updates_write_addr_q,
                   tlp::ostream<UpdateVec>& updates_write_data_q) {
  // HLS crashes without this...
  update_config_q.open();
  update_phase_q.open();
#ifdef __SYNTHESIS__
  ap_wait();
#endif  // __SYNTEHSIS__
  update_in_q.open();
#ifdef __SYNTHESIS__
  ap_wait();
#endif  // __SYNTEHSIS__
  update_out_q.close();

  // Memory offsets of each update partition.
  Eid update_offsets[RoundUp<kNumPes>(kMaxNumPartitions) / kNumPes];
#pragma HLS resource variable = update_offsets latency = 4
  // Number of updates of each update partition in memory.
  Eid num_updates[RoundUp<kNumPes>(kMaxNumPartitions) / kNumPes];

num_updates_init:
  for (Pid i = 0; i < RoundUp<kNumPes>(num_partitions) / kNumPes; ++i) {
#pragma HLS pipeline II = 1
    num_updates[i] = 0;
  }

  // Initialization; needed only once per execution.
  int update_offset_idx = 0;
update_offset_init:
  TLP_WHILE_NOT_EOS(update_config_q) {
#pragma HLS pipeline II = 1
    update_offsets[update_offset_idx] = update_config_q.read(nullptr);
    ++update_offset_idx;
  }
  update_config_q.open();

update_phases:
  TLP_WHILE_NOT_EOS(update_phase_q) {
    const auto phase = update_phase_q.read();
    VLOG_F(5, recv) << "Phase: " << phase;
    if (phase == TaskReq::kScatter) {
      // kScatter lasts until update_phase_q is non-empty.
      Pid last_last_pid = 0xffffffff;
      Pid last_pid = 0xffffffff;
      Eid last_update_idx = 0xffffffff;
    update_writes:
      TLP_WHILE_NOT_EOS(update_in_q) {
#pragma HLS pipeline II = 1
#pragma HLS dependence variable = num_updates inter true distance = 2
        const auto peek_pid = update_in_q.peek(nullptr).pid;
        if (peek_pid != last_pid && peek_pid == last_last_pid) {
          // insert bubble
          last_last_pid = 0xffffffff;
        } else {
          const auto update_with_pid = update_in_q.read(nullptr);
          VLOG_F(5, recv) << "UpdateWithPid: " << update_with_pid;
          const Pid pid = update_with_pid.pid;
          const UpdateVec& update_v = update_with_pid.updates;

          // number of updates already written to current partition, not
          // including the current update
          Eid update_idx;
          if (last_pid != pid) {
#pragma HLS latency min = 1 max = 1
            update_idx = num_updates[pid / kNumPes];
            if (last_pid != 0xffffffff) {
              num_updates[last_pid / kNumPes] =
                  RoundUp<kUpdateVecLen>(last_update_idx);
            }
          } else {
            update_idx = last_update_idx;
          }

          // set for next iteration
          last_last_pid = last_pid;
          last_pid = pid;
          last_update_idx = update_idx + kUpdateVecLen;

          Eid update_offset = update_offsets[pid / kNumPes] + update_idx;
          updates_write_addr_q.write(update_offset / kUpdateVecLen);
          updates_write_data_q.write(update_v);
        }
      }
      if (last_pid != 0xffffffff) {
        num_updates[last_pid / kNumPes] =
            RoundUp<kUpdateVecLen>(last_update_idx);
      }
      update_in_q.open();
#ifdef __SYNTHESIS__
      ap_wait_n(1);
#endif  // __SYNTHESIS__
    send_num_updates:
      for (Pid i = 0; i < RoundUp<kNumPes>(num_partitions) / kNumPes; ++i) {
        // TODO: store relevant partitions only
        VLOG_F(5, send) << "num_updates[" << i << "]: " << num_updates[i];
        num_updates_out_q.write({i, num_updates[i]});
        num_updates[i] = 0;  // Reset for the next scatter phase.
      }
    } else {
      // Gather phase.
    recv_update_reqs:
      for (UpdateReq update_req; update_phase_q.empty();) {
        if (update_req_q.try_read(update_req)) {
          const auto pid = update_req.pid;
          const auto num_updates_pid = update_req.num_updates;
          VLOG_F(5, recv) << "UpdateReq: " << update_req;

          bool valid = false;
          UpdateVec update_v;
        update_reads:
          for (Eid i_rd = 0, i_wr = 0; i_rd < num_updates_pid;) {
            auto read_addr = update_offsets[pid / kNumPes] + i_wr;
            if (i_wr < num_updates_pid &&
                updates_read_addr_q.try_write(read_addr / kUpdateVecLen)) {
              VLOG_F(5, req)
                  << "UpdateVec[" << read_addr / kUpdateVecLen << "]";
              i_wr += kUpdateVecLen;
            }

            if (valid || (valid = updates_read_data_q.try_read(update_v))) {
              if (update_out_q.try_write(update_v)) {
                VLOG_F(5, send) << "Update: " << update_v;
                i_rd += kUpdateVecLen;
                valid = false;
              }
            }
          }
          update_out_q.close();
        }
      }
    }
  }
  VLOG_F(3, info) << "done";
  update_phase_q.open();
}

void ProcElem(
    // from Control
    tlp::istream<TaskReq>& task_req_q,
    // to Control
    tlp::ostream<TaskResp>& task_resp_q,
    // to and from VertexMem
    tlp::ostream<VertexReq>& vertex_req_q,
    tlp::istream<VertexAttrVec>& vertex_in_q,
    tlp::ostream<VertexAttrVec>& vertex_out_q,
    // to and from EdgeMem
    tlp::ostream<Eid>& edge_req_q, tlp::istream<EdgeVec>& edge_resp_q,
    // to UpdateHandler
    tlp::ostream<UpdateReq>& update_req_q,
    // from UpdateHandler via UpdateReorderer
    tlp::istream<UpdateVec>& update_in_q,
    // to UpdateHandler via UpdateRouter
    tlp::ostream<UpdateVecWithPid>& update_out_q) {
  // HLS crashes without this...
  task_req_q.open();
#ifdef __SYNTHESIS__
  ap_wait();
#endif  // __SYNTEHSIS__
  update_out_q.close();
#ifdef __SYNTHESIS__
  ap_wait();
#endif  // __SYNTEHSIS__
  update_in_q.open();

  decltype(VertexAttr::tmp) vertices_local[kMaxPartitionSize];
#pragma HLS array_partition variable = vertices_local cyclic factor = \
    kVertexPartitionFactor
#pragma HLS resource variable = vertices_local core = RAM_S2P_URAM

task_requests:
  TLP_WHILE_NOT_EOS(task_req_q) {
    const auto req = task_req_q.read();
    VLOG_F(5, recv) << "TaskReq: " << req;
    if (req.scatter_done) {
      update_out_q.close();
    } else {
      bool active = false;
      if (req.phase == TaskReq::kScatter) {
        vertex_req_q.write(
            {TaskReq::kScatter, req.vid_offset, req.num_vertices});
      vertex_reads:
        for (Vid i = 0; i * kVertexVecLen < req.num_vertices; ++i) {
#pragma HLS pipeline II = 1
          auto vertex_vec = vertex_in_q.read();
          for (uint64_t j = 0; j < kVertexVecLen; ++j) {
#pragma HLS unroll
            vertices_local[i * kVertexVecLen + j] = vertex_vec[j].tmp;
          }
        }

      edge_reads:
        for (Eid eid_resp = 0, eid_req = 0; eid_resp < req.num_edges;) {
#pragma HLS pipeline II = 1
          if (eid_req < req.num_edges &&
              eid_resp < eid_req + kEstimatedLatency * kEdgeVecLen &&
              edge_req_q.try_write(req.eid_offset / kEdgeVecLen +
                                   eid_req / kEdgeVecLen)) {
            eid_req += kEdgeVecLen;
          }
          EdgeVec edge_v;
          // empty edge is indicated by src == 0
          // first edge in a vector must have valid dst for routing purpose
          if (edge_resp_q.try_read(edge_v)) {
            VLOG_F(10, recv) << "Edge: " << edge_v;
            UpdateVecWithPid update_v = {};
            update_v.pid =
                (edge_v[0].dst - req.overall_base_vid) / req.partition_size;
            for (int i = 0; i < kEdgeVecLen; ++i) {
#pragma HLS unroll
              const auto& edge = edge_v[i];
              if (edge.src != 0) {
                auto addr = edge.src - req.base_vid;
                LOG_IF(ERROR, addr % kEdgeVecLen != i)
                    << "addr " << addr << " != " << i << " mod " << kEdgeVecLen;
                addr /= kEdgeVecLen;
                // pre-compute pid for routing
                // use pre-computed src.tmp = src.ranking / src.out_degree
                update_v.updates.set(
                    (edge.dst - req.base_vid) % kEdgeVecLen,
                    {edge.dst, vertices_local[addr * kEdgeVecLen + i]});
              }
            }
            update_out_q.write(update_v);
            VLOG_F(5, send) << "Update: " << update_v;
            eid_resp += kEdgeVecLen;
          }
        }
      } else {
      vertex_resets:
        for (Vid i = 0; i < req.num_vertices; ++i) {
#pragma HLS pipeline II = 1
#pragma HLS unroll factor = kEdgeVecLen
          vertices_local[i] = 0.f;
        }

        update_req_q.write({req.phase, req.pid, req.num_edges});
      update_reads:
        TLP_WHILE_NOT_EOS(update_in_q) {
#pragma HLS pipeline II = 1
#pragma HLS dependence variable = vertices_local inter true distance = \
    kVertexUpdateDepDist
          auto update_v = update_in_q.read(nullptr);
          VLOG_F(5, recv) << "Update: " << update_v;
          for (int i = 0; i < kUpdateVecLen; ++i) {
#pragma HLS unroll
            auto update = update_v[i];
            if (update.dst != 0) {
              auto addr = update.dst - req.base_vid;
              LOG_IF(ERROR, addr % kEdgeVecLen != i)
                  << "addr " << addr << " != " << i << " mod " << kEdgeVecLen;
#pragma HLS latency max = kVertexUpdateLatency
              addr /= kEdgeVecLen;
              vertices_local[addr * kEdgeVecLen + i] += update.delta;
            }
          }
        }
        update_in_q.open();

        vertex_req_q.write(
            {TaskReq::kGather, req.vid_offset, req.num_vertices});

        float max_delta = 0.f;

      vertex_writes:
        for (Vid i = 0; i * kVertexVecLen < req.num_vertices; ++i) {
#pragma HLS pipeline II = 1
          VertexAttrVec vertex_vec = vertex_in_q.read();
          float delta[kVertexVecLen];
#pragma HLS array_partition variable = delta complete
          for (uint64_t j = 0; j < kVertexVecLen; ++j) {
#pragma HLS unroll
            auto vertex = vertex_vec[j];
            auto tmp = vertices_local[i * kVertexVecLen + j];
            const float new_ranking = req.init + tmp * kDampingFactor;
            delta[j] = std::abs(new_ranking - vertex.ranking);
            vertex.ranking = new_ranking;
            // pre-compute vertex.tmp = vertex.ranking / vertex.out_degree
            vertex.tmp = vertex.ranking / vertex.out_degree;
            vertex_vec.set(j, vertex);
            VLOG_F(5, send) << "VertexAttr[" << j << "]: " << vertex;
          }
          max_delta = std::max(max_delta, Max(delta));
          vertex_out_q.write(vertex_vec);
        }
        active = max_delta > kConvergenceThreshold;
      }
      TaskResp resp{req.phase, req.pid, active};
      task_resp_q.write(resp);
    }
  }
  task_req_q.open();
}

void PageRank(Pid num_partitions, tlp::mmap<uint64_t> metadata,
              tlp::async_mmap<VertexAttrAlignedVec> vertices,
              tlp::async_mmap<EdgeVec> edges_0,
              tlp::async_mmap<EdgeVec> edges_1,
              tlp::async_mmap<UpdateVec> updates_0,
              tlp::async_mmap<UpdateVec> updates_1) {
  // between Control and ProcElem
  tlp::stream<TaskReq, 2> task_req_0("task_req_0");
  tlp::stream<TaskReq, 2> task_req_1("task_req_1");
  tlp::stream<TaskResp, 2> task_resp_0("task_resp_0");
  tlp::stream<TaskResp, 2> task_resp_1("task_resp_1");

  // between ProcElem and VertexMem
  tlp::stream<VertexReq, 2> vertex_req_0("vertex_req_0");
  tlp::stream<VertexReq, 2> vertex_req_1("vertex_req_1");
  tlp::stream<VertexAttrVec, 2> vertex_pe2mm_0("vertex_pe2mm_0");
  tlp::stream<VertexAttrVec, 2> vertex_pe2mm_1("vertex_pe2mm_1");
  tlp::stream<VertexAttrVec, 2> vertex_mm2pe_0("vertex_mm2pe_0");
  tlp::stream<VertexAttrVec, 2> vertex_mm2pe_1("vertex_mm2pe_1");

  // between ProcElem and EdgeMem
  tlp::stream<Eid, 2> edge_req_0("edge_req_0");
  tlp::stream<Eid, 2> edge_req_1("edge_req_1");
  tlp::stream<EdgeVec, 2> edge_resp_0("edge_resp_0");
  tlp::stream<EdgeVec, 2> edge_resp_1("edge_resp_1");

  // between Control and UpdateHandler
  tlp::stream<Eid, 2> update_config_0("update_config_0");
  tlp::stream<Eid, 2> update_config_1("update_config_1");
  tlp::stream<bool, 2> update_phase_0("update_phase_0");
  tlp::stream<bool, 2> update_phase_1("update_phase_1");

  // between UpdateHandler and ProcElem
  tlp::stream<UpdateReq, 2> update_req_0("update_req_0");
  tlp::stream<UpdateReq, 2> update_req_1("update_req_1");

  // between UpdateHandler and UpdateMem
  tlp::stream<uint64_t, 2> update_read_addr_0("update_read_addr_0");
  tlp::stream<uint64_t, 2> update_read_addr_1("update_read_addr_1");
  tlp::stream<UpdateVec, 2> update_read_data_0("update_read_data_0");
  tlp::stream<UpdateVec, 2> update_read_data_1("update_read_data_1");
  tlp::stream<uint64_t, 2> update_write_addr_0("update_write_addr_0");
  tlp::stream<uint64_t, 2> update_write_addr_1("update_write_addr_1");
  tlp::stream<UpdateVec, 2> update_write_data_0("update_write_data_0");
  tlp::stream<UpdateVec, 2> update_write_data_1("update_write_data_1");

  tlp::stream<UpdateVecWithPid, 2> update_pe2mm_0("update_pe2mm_0");
  tlp::stream<UpdateVecWithPid, 2> update_pe2mm_1("update_pe2mm_1");
  tlp::stream<UpdateVec, 2> update_mm2pe_0("update_mm2pe_0");
  tlp::stream<UpdateVec, 2> update_mm2pe_1("update_mm2pe_1");

  tlp::stream<NumUpdates, 2> num_updates_0("num_updates_0");
  tlp::stream<NumUpdates, 2> num_updates_1("num_updates_1");

  tlp::task()
      .invoke<-1>(VertexMem, "VertexMem", vertex_req_0, vertex_req_1,
                  vertex_pe2mm_0, vertex_pe2mm_1, vertex_mm2pe_0,
                  vertex_mm2pe_1, vertices)
      .invoke<-1>(EdgeMem, "EdgeMem_0", edge_req_0, edge_resp_0, edges_0)
      .invoke<-1>(EdgeMem, "EdgeMem_1", edge_req_1, edge_resp_1, edges_1)
      .invoke<-1>(UpdateMem, "UpdateMem_0", update_read_addr_0,
                  update_read_data_0, update_write_addr_0, update_write_data_0,
                  updates_0)
      .invoke<-1>(UpdateMem, "UpdateMem_1", update_read_addr_1,
                  update_read_data_1, update_write_addr_1, update_write_data_1,
                  updates_1)
      .invoke<0>(ProcElem, "ProcElem_0", task_req_0, task_resp_0, vertex_req_0,
                 vertex_mm2pe_0, vertex_pe2mm_0, edge_req_0, edge_resp_0,
                 update_req_0, update_mm2pe_0, update_pe2mm_0)
      .invoke<0>(ProcElem, "ProcElem_1", task_req_1, task_resp_1, vertex_req_1,
                 vertex_mm2pe_1, vertex_pe2mm_1, edge_req_1, edge_resp_1,
                 update_req_1, update_mm2pe_1, update_pe2mm_1)
      .invoke<0>(Control, "Control", num_partitions, metadata, update_config_0,
                 update_config_1, update_phase_0, update_phase_1, num_updates_0,
                 num_updates_1, task_req_0, task_req_1, task_resp_0,
                 task_resp_1)
      .invoke<0>(UpdateHandler, "UpdateHandler_0", num_partitions,
                 update_config_0, update_phase_0, num_updates_0, update_req_0,
                 update_pe2mm_0, update_mm2pe_0, update_read_addr_0,
                 update_read_data_0, update_write_addr_0, update_write_data_0)
      .invoke<0>(UpdateHandler, "UpdateHandler_1", num_partitions,
                 update_config_1, update_phase_1, num_updates_1, update_req_1,
                 update_pe2mm_1, update_mm2pe_1, update_read_addr_1,
                 update_read_data_1, update_write_addr_1, update_write_data_1);
}
