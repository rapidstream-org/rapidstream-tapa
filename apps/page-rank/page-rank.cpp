#include <cassert>
#include <cstring>

#include <algorithm>

#include <tlp.h>

#include "page-rank-kernel.h"

constexpr int kMaxNumPartitions = 2048;
constexpr int kMaxPartitionSize = 1024 * 256;
constexpr int kEstimatedLatency = 50;

constexpr int kNumPesR0 = 1;
constexpr int kNumPesR1 = (kNumPes - kNumPesR0) / 2;
constexpr int kNumPesR2 = kNumPes - kNumPesR0 - kNumPesR1;

void Control(Pid num_partitions, tlp::mmap<uint64_t> metadata,
             // to VertexMem
             tlp::ostream<VertexReq>& vertex_req_q,
             // to UpdateHandler
             tlp::ostreams<Eid, kNumPes>& update_config_q,
             tlp::ostreams<bool, kNumPes>& update_phase_q,
             // from UpdateHandler
             tlp::istreams<NumUpdates, kNumPes>& num_updates_q,
             // to ProcElem
             tlp::ostreams<TaskReq, kNumPes>& task_req_q,
             // from ProcElem
             tlp::istreams<TaskResp, kNumPes>& task_resp_q) {
  // HLS crashes without this...
  for (int pe = 0; pe < kNumPes; ++pe) {
#pragma HLS unroll
    update_config_q[pe].close();
    update_phase_q[pe].close();
    task_req_q[pe].close();
  }

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
    update_config_q[pid % kNumPes].write(update_offset);
  }
  // Tells UpdateHandler start to wait for phase requests.
  for (int pe = 0; pe < kNumPes; ++pe) {
#pragma HLS unroll
    update_config_q[pe].close();
  }

  bool all_done = false;
  int iter = 0;
bulk_steps:
  while (!all_done) {
    all_done = true;

    // Do the scatter phase for each partition, if active.
    // Wait until all PEs are done with the scatter phase.
    VLOG_F(5, info) << "Phase: " << TaskReq::kScatter;
    for (int pe = 0; pe < kNumPes; ++pe) {
#pragma HLS unroll
      update_phase_q[pe].write(false);
    }

  scatter:
    for (Pid pid = 0; pid < num_partitions; ++pid) {
      // Tell VertexMem to start broadcasting source vertices.
      Vid vid_offset = pid * partition_size;
      vertex_req_q.write({vid_offset, partition_size});
      for (int pe = 0; pe < kNumPes; ++pe) {
#pragma HLS unroll
        TaskReq req{
            /* phase            = */ TaskReq::kScatter,
            /* pid              = */ 0,  // unused
            /* overall_base_vid = */ base_vid,
            /* partition_size   = */ partition_size,
            /* num_edges        = */ num_edges_local[pid][pe],
            /* vid_offset       = */ vid_offset,
            /* eid_offset       = */ eid_offsets[pid][pe],
            /* init             = */ 0.f,   // unused
            /* scatter_done     = */ false  // unused
        };
        task_req_q[pe].write(req);
      }

#ifdef __SYNTHESIS__
      ap_wait();
#endif

      for (int pe = 0; pe < kNumPes; ++pe) {
#pragma HLS unroll
        task_resp_q[pe].read();
      }
    }

    // Tell PEs to tell UpdateHandlers that the scatter phase is done.
    TaskReq req = {};
    req.scatter_done = true;
    for (int pe = 0; pe < kNumPes; ++pe) {
#pragma HLS unroll
      task_req_q[pe].write(req);
    }

    // Get prepared for the gather phase.
    VLOG_F(5, info) << "Phase: " << TaskReq::kGather;
    for (int pe = 0; pe < kNumPes; ++pe) {
#pragma HLS unroll
      update_phase_q[pe].write(true);
    }

  reset_num_updates:
    for (Pid pid = 0; pid < num_partitions; ++pid) {
#pragma HLS pipeline II = 1
      num_updates_local[pid] = 0;
    }

  collect_num_updates:
    for (Pid pid_recv = 0; pid_recv < RoundUp<kNumPes>(num_partitions);) {
#pragma HLS pipeline II = 1
#pragma HLS dependence false variable = num_updates_local
      NumUpdates num_updates;
      bool done = false;
      Pid pid;
      for (int pe = 0; pe < kNumPes; ++pe) {
#pragma HLS unroll
        if (!done && num_updates_q[pe].try_read(num_updates)) {
          done |= true;
          pid = num_updates.addr * kNumPes + pe;
          ++pid_recv;
        }
      }
      if (done && pid < num_partitions) {
        VLOG_F(5, recv) << "num_updates: " << num_updates;
        num_updates_local[pid] += num_updates.payload;
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
                      partition_size,
                      num_updates_local[pid],
                      partition_size * pid,
                      0,  // eid_offset, unused
                      (1.f - kDampingFactor) / total_num_vertices,
                      false};
          if (task_req_q[pe].try_write(req)) ++pid_send[pe];
        }
      }

      TaskResp resp;
      for (int pe = 0; pe < kNumPes; ++pe) {
#pragma HLS unroll
        if (task_resp_q[pe].try_read(resp)) {
          VLOG_F(3, recv) << resp;
          if (resp.active) all_done = false;
          ++pid_recv;
        }
      }
    }
    VLOG_F(3, info) << "iter #" << iter << (all_done ? " " : " not ")
                    << "all done";
    ++iter;
  }
  // Terminates UpdateHandler.
  for (int pe = 0; pe < kNumPes; ++pe) {
#pragma HLS unroll
    update_phase_q[pe].close();
    task_req_q[pe].close();
  }

  metadata[2 + num_partitions * kNumPes + num_partitions] = iter;
}

void VertexMem(tlp::istream<VertexReq>& scatter_req_q,
               tlp::istreams<VertexReq, kNumPesR0 + 1>& vertex_req_q,
               tlp::istreams<VertexAttrVec, kNumPesR0 + 1>& vertex_in_q,
               tlp::ostreams<VertexAttrVec, kNumPesR0 + 1>& vertex_out_q,
               tlp::async_mmap<FloatVec>& degrees,
               tlp::async_mmap<FloatVec>& rankings,
               tlp::async_mmap<FloatVec>& tmps) {
  constexpr int N = kNumPesR0 + 1;
  for (;;) {
    // Prioritize scatter phase broadcast.
    VertexReq scatter_req;
    if (scatter_req_q.try_read(scatter_req)) {
      auto req = tlp::reg(scatter_req);
      // Scatter phase
      //   Send tmp to PEs.
      FloatVec resp;
      VertexAttrVec tmp_out;
      bool valid = false;
      bool ready[N] = {};
    vertices_scatter:
      for (Vid i_req = 0, i_resp = 0; i_resp < req.length;) {
#pragma HLS pipeline II = 1
        // Send requests.
        if (i_req < req.length &&
            i_req < i_resp + kEstimatedLatency * kVertexVecLen &&
            tmps.read_addr_try_write((req.offset + i_req) / kVertexVecLen)) {
          i_req += kVertexVecLen;
        }

        // Handle responses.
        if (!valid) valid = tmps.read_data_try_read(resp);
        for (int i = 0; i < kVertexVecLen; ++i) {
#pragma HLS unroll
          tmp_out.set(i, VertexAttrKernel{0.f, resp[i]});
        }
        if (valid) {
          for (int pe = 0; pe < N; ++pe) {
#pragma HLS unroll
            if (!ready[pe]) ready[pe] = vertex_out_q[pe].try_write(tmp_out);
          }
          if (All(ready)) {
            i_resp += kVertexVecLen;
            valid = false;
            MemSet(ready, false);
          }
        }
      }
    } else {
      VertexReq req;
      bool done = false;
      for (int pe = 0; pe < N; ++pe) {
#pragma HLS unroll
        if (!done && vertex_req_q[pe].try_read(req)) {
          done |= true;
          // Gather phase
          //   Send degree and ranking to PEs.
          //   Recv tmp and ranking from PEs.

          FloatVec resp_degree;
          FloatVec resp_ranking;

          // valid_xx: resp_xx is valid
          bool valid_degree = false;
          bool valid_ranking = false;

          // xx_ready_oo: write_xx has been written.
          bool addr_ready_tmp = false;
          bool data_ready_tmp = false;
          bool addr_ready_ranking = false;
          bool data_ready_ranking = false;

        vertices_gather:
          for (Vid i_rd_req_degree = 0, i_rd_req_ranking = 0, i_rd_resp = 0,
                   i_wr = 0;
               i_wr < req.length;) {
#pragma HLS pipeline II = 1
            // Send read requests.
            if (i_rd_req_degree < req.length &&
                i_rd_req_degree <
                    i_rd_resp + kEstimatedLatency * kVertexVecLen &&
                degrees.read_addr_try_write((req.offset + i_rd_req_degree) /
                                            kVertexVecLen)) {
              i_rd_req_degree += kVertexVecLen;
            }
            if (i_rd_req_ranking < req.length &&
                i_rd_req_ranking <
                    i_rd_resp + kEstimatedLatency * kVertexVecLen &&
                rankings.read_addr_try_write((req.offset + i_rd_req_ranking) /
                                             kVertexVecLen)) {
              i_rd_req_ranking += kVertexVecLen;
            }

            // Handle read responses.
            if (i_rd_resp < req.length) {
              if (!valid_degree)
                valid_degree = degrees.read_data_try_read(resp_degree);
              if (!valid_ranking)
                valid_ranking = rankings.read_data_try_read(resp_ranking);
              VertexAttrVec vertex_attr_out;
              for (int i = 0; i < kVertexVecLen; ++i) {
#pragma HLS unroll
                vertex_attr_out.set(
                    i, VertexAttrKernel{resp_ranking[i], resp_degree[i]});
              }
              if (valid_degree && valid_ranking &&
                  vertex_out_q[pe].try_write(vertex_attr_out)) {
                i_rd_resp += kVertexVecLen;
                valid_degree = false;
                valid_ranking = false;
              }
            }

            // Write to DRAM.
            if (!vertex_in_q[pe].empty()) {
              auto v = vertex_in_q[pe].peek(nullptr);
              FloatVec ranking_out;
              FloatVec tmp_out;
              for (int i = 0; i < kVertexVecLen; ++i) {
#pragma HLS unroll
                ranking_out.set(i, v[i].ranking);
                tmp_out.set(i, v[i].tmp);
              }
              uint64_t addr = (req.offset + i_wr) / kVertexVecLen;
              if (!addr_ready_ranking)
                addr_ready_ranking = rankings.write_addr_try_write(addr);
              if (!data_ready_ranking)
                data_ready_ranking = rankings.write_data_try_write(ranking_out);
              if (!addr_ready_tmp)
                addr_ready_tmp = tmps.write_addr_try_write(addr);
              if (!data_ready_tmp)
                data_ready_tmp = tmps.write_data_try_write(tmp_out);
              if (addr_ready_ranking && data_ready_ranking && addr_ready_tmp &&
                  data_ready_tmp) {
                vertex_in_q[pe].read(nullptr);
                addr_ready_ranking = false;
                data_ready_ranking = false;
                addr_ready_tmp = false;
                data_ready_tmp = false;
                i_wr += kVertexVecLen;
              }
            }
          }
        }
      }
    }
  }
}

template <uint64_t N>
void VertexRouterTemplated(
    // upstream to VertexMem
    tlp::ostream<VertexReq>& mm_req_q, tlp::istream<VertexAttrVec>& mm_in_q,
    tlp::ostream<VertexAttrVec>& mm_out_q,
    // downstream to ProcElem
    tlp::istreams<VertexReq, N>& pe_req_q,
    tlp::istreams<VertexAttrVec, N>& pe_in_q,
    tlp::ostreams<VertexAttrVec, N>& pe_out_q) {
  TLP_ELEM_TYPE(pe_req_q) pe_req;
  bool pe_req_valid = false;
  bool mm_req_ready = false;

  TLP_ELEM_TYPE(mm_in_q) mm_in;
  bool mm_in_valid = false;
  bool pe_out_ready[N] = {};

  TLP_ELEM_TYPE(pe_in_q) pe_in;
  bool pe_in_valid = false;
  bool mm_out_ready = false;

  uint8_t pe = 0;
  decltype(pe_req.length) mm2pe_count = 0;
  decltype(pe_req.length) pe2mm_count = 0;

vertex_router:
  for (;;) {
#pragma HLS pipeline II = 1
    if (pe2mm_count == 0) {
      // Not processing a gather phase request.

      // Broadcast scatter phase data if any.
      if (!mm_in_valid) mm_in_valid = mm_in_q.try_read(mm_in);
      if (mm_in_valid) {
#pragma HLS latency max = 1
        for (uint64_t i = 0; i < N; ++i) {
#pragma HLS unroll
          if (!pe_out_ready[i]) pe_out_ready[i] = pe_out_q[i].try_write(mm_in);
        }
        if (All(pe_out_ready)) {
#pragma HLS latency max = 1
          VLOG_F(20, fwd) << "scatter phase data";
          mm_in_valid = false;
          MemSet(pe_out_ready, false);
        }
      }

      // Accept gather phase requests.
      for (uint64_t i = 0; i < N; ++i) {
#pragma HLS unroll
        if (!pe_req_valid && pe_req_q[i].try_read(pe_req)) {
          pe_req_valid |= true;
          pe = i;
          mm2pe_count = pe2mm_count = pe_req.length / kVertexVecLen;
        }
      }
    } else {
      // Processing a gather phase request.

      // Forward vertex attribtues from ProcElem to VertexMem.
      if (!pe_in_valid) pe_in_valid = pe_in_q[pe].try_read(pe_in);
      if (pe_in_valid) {
        if (!mm_out_ready) mm_out_ready = mm_out_q.try_write(pe_in);
        if (mm_out_ready) {
          VLOG_F(10, fwd) << "gather phase: memory <- port " << int(pe)
                          << "; remaining: " << pe2mm_count - 1;
          pe_in_valid = false;
          mm_out_ready = false;
          --pe2mm_count;
        }
      }

      // Forward vertex attribtues from VertexMem to ProcElem.
      if (!mm_in_valid) mm_in_valid = mm_in_q.try_read(mm_in);
      if (mm_in_valid && mm2pe_count > 0) {
        if (!pe_out_ready[pe]) pe_out_ready[pe] = pe_out_q[pe].try_write(mm_in);
        if (pe_out_ready[pe]) {
          VLOG_F(10, fwd) << "gather phase: memory -> port " << int(pe)
                          << "; remaining: " << mm2pe_count - 1;
          mm_in_valid = false;
          pe_out_ready[pe] = false;
          --mm2pe_count;
        }
      }

      // Forward request from ProcElem to VertexMem.
      if (pe_req_valid) {
        if (!mm_req_ready) mm_req_ready = mm_req_q.try_write(pe_req);
        if (mm_req_ready) {
          VLOG_F(9, recv) << "fulfilling request from port " << int(pe);
          pe_req_valid = false;
          mm_req_ready = false;
        }
      }
    }
  }
}

void VertexRouterR1(
    // upstream to VertexMem
    tlp::ostream<VertexReq>& mm_req_q, tlp::istream<VertexAttrVec>& mm_in_q,
    tlp::ostream<VertexAttrVec>& mm_out_q,
    // downstream to ProcElem
    tlp::istreams<VertexReq, kNumPesR1 + 1>& pe_req_q,
    tlp::istreams<VertexAttrVec, kNumPesR1 + 1>& pe_in_q,
    tlp::ostreams<VertexAttrVec, kNumPesR1 + 1>& pe_out_q) {
#pragma HLS inline region
  VertexRouterTemplated(mm_req_q, mm_in_q, mm_out_q, pe_req_q, pe_in_q,
                        pe_out_q);
}

void VertexRouterR2(
    // upstream to VertexMem
    tlp::ostream<VertexReq>& mm_req_q, tlp::istream<VertexAttrVec>& mm_in_q,
    tlp::ostream<VertexAttrVec>& mm_out_q,
    // downstream to ProcElem
    tlp::istreams<VertexReq, kNumPesR2>& pe_req_q,
    tlp::istreams<VertexAttrVec, kNumPesR2>& pe_in_q,
    tlp::ostreams<VertexAttrVec, kNumPesR2>& pe_out_q) {
#pragma HLS inline region
  VertexRouterTemplated(mm_req_q, mm_in_q, mm_out_q, pe_req_q, pe_in_q,
                        pe_out_q);
}

// Handles edge read requests.
void EdgeMem(tlp::istream<Eid>& edge_req_q, tlp::ostream<EdgeVec>& edge_resp_q,
             tlp::async_mmap<EdgeVec> edges) {
  bool valid = false;
  EdgeVec edge_v;
  for (;;) {
#pragma HLS pipeline II = 1
    // Handle responses.
    if (valid || (valid = edges.read_data_try_read(edge_v))) {
      if (edge_resp_q.try_write(edge_v)) valid = false;
    }

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
    if (valid || (valid = updates.read_data_try_read(update_v))) {
      if (read_data_q.try_write(update_v)) valid = false;
    }

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
                   tlp::istream<UpdateVecPacket>& update_in_q,
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
      Pid last_last_pid = Pid(-1);
      Pid last_pid = Pid(-1);
      Eid last_update_idx = Eid(-1);
    update_writes:
      TLP_WHILE_NOT_EOS(update_in_q) {
#pragma HLS pipeline II = 1
#pragma HLS dependence variable = num_updates inter true distance = 2
        const auto peek_pid = update_in_q.peek(nullptr).addr;
        if (peek_pid != last_pid && peek_pid == last_last_pid) {
          // insert bubble
          last_last_pid = Pid(-1);
        } else {
          const auto update_with_pid = update_in_q.read(nullptr);
          VLOG_F(5, recv) << "UpdateWithPid: " << update_with_pid;
          const Pid pid = update_with_pid.addr;
          const UpdateVec& update_v = update_with_pid.payload;

          // number of updates already written to current partition, not
          // including the current update
          Eid update_idx;
          if (last_pid != pid) {
#pragma HLS latency min = 1 max = 1
            update_idx = num_updates[pid / kNumPes];
            if (last_pid != Pid(-1)) {
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
      if (last_pid != Pid(-1)) {
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
    tlp::ostream<UpdateVecPacket>& update_out_q) {
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
    auto base_vid = req.overall_base_vid + req.vid_offset;
    VLOG_F(5, recv) << "TaskReq: " << req;
    if (req.scatter_done) {
      update_out_q.close();
    } else {
      bool active = false;
      if (req.phase == TaskReq::kScatter) {
      vertex_reads:
        for (Vid i = 0; i * kVertexVecLen < req.partition_size; ++i) {
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
            UpdateVecPacket update_v = {};
            update_v.addr =
                (edge_v[0].dst - req.overall_base_vid) / req.partition_size;
            for (int i = 0; i < kEdgeVecLen; ++i) {
#pragma HLS unroll
              const auto& edge = edge_v[i];
              if (edge.src != 0) {
                auto addr = edge.src - base_vid;
                LOG_IF(ERROR, addr % kEdgeVecLen != i)
                    << "addr " << addr << " != " << i << " mod " << kEdgeVecLen;
                addr /= kEdgeVecLen;
                // pre-compute pid for routing
                // use pre-computed src.tmp = src.ranking / src.out_degree
                auto tmp = tlp::reg(vertices_local[addr * kEdgeVecLen + i]);
                update_v.payload.set((edge.dst - base_vid) % kEdgeVecLen,
                                     {edge.dst, tmp});
              }
            }
            update_out_q.write(update_v);
            VLOG_F(5, send) << "Update: " << update_v;
            eid_resp += kEdgeVecLen;
          }
        }
      } else {
      vertex_resets:
        for (Vid i = 0; i < req.partition_size; ++i) {
#pragma HLS pipeline II = 1
#pragma HLS unroll factor = kVertexPartitionFactor
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
              auto addr = update.dst - base_vid;
              LOG_IF(ERROR, addr % kEdgeVecLen != i)
                  << "addr " << addr << " != " << i << " mod " << kEdgeVecLen;
#pragma HLS latency max = kVertexUpdateLatency
              addr /= kEdgeVecLen;
              vertices_local[addr * kEdgeVecLen + i] += update.delta;
            }
          }
        }
        update_in_q.open();

        vertex_req_q.write({req.vid_offset, req.partition_size});

        float max_delta = 0.f;

      vertex_writes:
        for (Vid i = 0; i * kVertexVecLen < req.partition_size; ++i) {
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
            vertex.tmp = vertex.ranking * vertex.tmp;
            vertex_vec.set(j, vertex);
            VLOG_F(5, send) << "VertexAttr[" << j << "]: " << vertex;
          }
          max_delta = std::max(max_delta, Max(delta));
          vertex_out_q.write(vertex_vec);
        }
        active = max_delta > kConvergenceThreshold;
      }
      TaskResp resp{active};
      task_resp_q.write(resp);
    }
  }
  task_req_q.open();
}

void PageRank(Pid num_partitions, tlp::mmap<uint64_t> metadata,
              tlp::async_mmap<FloatVec> degrees,
              tlp::async_mmap<FloatVec> rankings,
              tlp::async_mmap<FloatVec> tmps,
              tlp::async_mmaps<EdgeVec, kNumPes> edges,
              tlp::async_mmaps<UpdateVec, kNumPes> updates) {
  // between Control and ProcElem
  tlp::streams<TaskReq, kNumPes, 2> task_req("task_req");
  tlp::streams<TaskResp, kNumPes, 2> task_resp("task_resp");

  // between Control and VertexMem
  tlp::stream<VertexReq, 2> scatter_phase_vertex_req(
      "scatter_phase_vertex_req");

  // between ProcElem and VertexMem in region 0
  tlp::streams<VertexReq, kNumPesR0 + 1, 2> vertex_req_r0("vertex_req_r0");
  tlp::streams<VertexAttrVec, kNumPesR0 + 1, 2> vertex_pe2mm_r0(
      "vertex_pe2mm_r0");
  tlp::streams<VertexAttrVec, kNumPesR0 + 1, 2> vertex_mm2pe_r0(
      "vertex_mm2pe_r0");

  // between ProcElem and VertexMem in region 1
  tlp::streams<VertexReq, kNumPesR1 + 1, 2> vertex_req_r1("vertex_req_r1");
  tlp::streams<VertexAttrVec, kNumPesR1 + 1, 2> vertex_pe2mm_r1(
      "vertex_pe2mm_r1");
  tlp::streams<VertexAttrVec, kNumPesR1 + 1, 2> vertex_mm2pe_r1(
      "vertex_mm2pe_r1");

  // between ProcElem and VertexMem in region 2
  tlp::streams<VertexReq, kNumPesR2, 2> vertex_req_r2("vertex_req_r2");
  tlp::streams<VertexAttrVec, kNumPesR2, 2> vertex_pe2mm_r2("vertex_pe2mm_r2");
  tlp::streams<VertexAttrVec, kNumPesR2, 2> vertex_mm2pe_r2("vertex_mm2pe_r2");

  // between ProcElem and EdgeMem
  tlp::streams<Eid, kNumPes, 2> edge_req("edge_req");
  tlp::streams<EdgeVec, kNumPes, 2> edge_resp("edge_resp");

  // between Control and UpdateHandler
  tlp::streams<Eid, kNumPes, 2> update_config("update_config");
  tlp::streams<bool, kNumPes, 2> update_phase("update_phase");

  // between UpdateHandler and ProcElem
  tlp::streams<UpdateReq, kNumPes, 2> update_req("update_req");

  // between UpdateHandler and UpdateMem
  tlp::streams<uint64_t, kNumPes, 2> update_read_addr("update_read_addr");
  tlp::streams<UpdateVec, kNumPes, 2> update_read_data("update_read_data");
  tlp::streams<uint64_t, kNumPes, 2> update_write_addr("update_write_addr");
  tlp::streams<UpdateVec, kNumPes, 2> update_write_data("update_write_data");

  tlp::streams<UpdateVecPacket, kNumPes, 2> update_pe2mm("update_pe2mm");
  tlp::streams<UpdateVec, kNumPes, 2> update_mm2pe("update_mm2pe");

  tlp::streams<NumUpdates, kNumPes, 2> num_updates("num_updates");

  tlp::task()
      .invoke<-1>(VertexMem, "VertexMem", scatter_phase_vertex_req,
                  vertex_req_r0, vertex_pe2mm_r0, vertex_mm2pe_r0, degrees,
                  rankings, tmps)
      .invoke<-1>(VertexRouterR1, "VertexRouterR1", vertex_req_r0[kNumPesR0],
                  vertex_mm2pe_r0[kNumPesR0], vertex_pe2mm_r0[kNumPesR0],
                  vertex_req_r1, vertex_pe2mm_r1, vertex_mm2pe_r1)
      .invoke<-1>(VertexRouterR2, "VertexRouterR2", vertex_req_r1[kNumPesR1],
                  vertex_mm2pe_r1[kNumPesR1], vertex_pe2mm_r1[kNumPesR1],
                  vertex_req_r2, vertex_pe2mm_r2, vertex_mm2pe_r2)
      .invoke<-1, kNumPes>(EdgeMem, "EdgeMem", edge_req, edge_resp, edges)
      .invoke<-1, kNumPes>(UpdateMem, "UpdateMem", update_read_addr,
                           update_read_data, update_write_addr,
                           update_write_data, updates)
      .invoke<0>(ProcElem, "ProcElem[0]", task_req[0], task_resp[0],
                 vertex_req_r0[0], vertex_mm2pe_r0[0], vertex_pe2mm_r0[0],
                 edge_req[0], edge_resp[0], update_req[0], update_mm2pe[0],
                 update_pe2mm[0])
      .invoke<0>(ProcElem, "ProcElem[1]", task_req[1], task_resp[1],
                 vertex_req_r1[0], vertex_mm2pe_r1[0], vertex_pe2mm_r1[0],
                 edge_req[1], edge_resp[1], update_req[1], update_mm2pe[1],
                 update_pe2mm[1])
      .invoke<0>(ProcElem, "ProcElem[2]", task_req[2], task_resp[2],
                 vertex_req_r1[1], vertex_mm2pe_r1[1], vertex_pe2mm_r1[1],
                 edge_req[2], edge_resp[2], update_req[2], update_mm2pe[2],
                 update_pe2mm[2])
      .invoke<0>(ProcElem, "ProcElem[3]", task_req[3], task_resp[3],
                 vertex_req_r1[2], vertex_mm2pe_r1[2], vertex_pe2mm_r1[2],
                 edge_req[3], edge_resp[3], update_req[3], update_mm2pe[3],
                 update_pe2mm[3])
      .invoke<0>(ProcElem, "ProcElem[4]", task_req[4], task_resp[4],
                 vertex_req_r2[0], vertex_mm2pe_r2[0], vertex_pe2mm_r2[0],
                 edge_req[4], edge_resp[4], update_req[4], update_mm2pe[4],
                 update_pe2mm[4])
      .invoke<0>(ProcElem, "ProcElem[5]", task_req[5], task_resp[5],
                 vertex_req_r2[1], vertex_mm2pe_r2[1], vertex_pe2mm_r2[1],
                 edge_req[5], edge_resp[5], update_req[5], update_mm2pe[5],
                 update_pe2mm[5])
      .invoke<0>(ProcElem, "ProcElem[6]", task_req[6], task_resp[6],
                 vertex_req_r2[2], vertex_mm2pe_r2[2], vertex_pe2mm_r2[2],
                 edge_req[6], edge_resp[6], update_req[6], update_mm2pe[6],
                 update_pe2mm[6])
      .invoke<0>(ProcElem, "ProcElem[7]", task_req[7], task_resp[7],
                 vertex_req_r2[3], vertex_mm2pe_r2[3], vertex_pe2mm_r2[3],
                 edge_req[7], edge_resp[7], update_req[7], update_mm2pe[7],
                 update_pe2mm[7])
      .invoke<0>(Control, "Control", num_partitions, metadata,
                 scatter_phase_vertex_req, update_config, update_phase,
                 num_updates, task_req, task_resp)
      .invoke<0, kNumPes>(UpdateHandler, "UpdateHandler", num_partitions,
                          update_config, update_phase, num_updates, update_req,
                          update_pe2mm, update_mm2pe, update_read_addr,
                          update_read_data, update_write_addr,
                          update_write_data);
}
