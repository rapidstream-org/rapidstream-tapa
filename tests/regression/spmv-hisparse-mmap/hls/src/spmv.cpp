// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include <ap_fixed.h>
#include <assert.h>
#include <hls_stream.h>

#include "common.h"
#include "stream_utils.h"

#ifndef SPMV_CLUSTER_H_
#define SPMV_CLUSTER_H_

#include <ap_fixed.h>
#include <hls_stream.h>

#include "common.h"
#include "pe.h"
#include "shuffle.h"
#include "vecbuf_access_unit.h"

#include <ap_fixed.h>
#include <hls_stream.h>

#include "common.h"
#include "spmv_cluster.h"

#ifndef __SYNTHESIS__
// #define SPMV_CLUSTER_H_LINE_TRACING
#endif

template <typename T, unsigned len>
T array_max(T array[len]) {
#pragma HLS inline
#pragma HLS expression_balance
  T result = 0;
  for (unsigned i = 0; i < len; i++) {
#pragma HLS unroll
    result = (array[i] > result) ? array[i] : result;
  }
  return result;
}

void CPSR_matrix_loader(
    const SPMV_MAT_PKT_T* matrix_hbm,                     // in
    unsigned row_partition_idx,                           // in
    unsigned num_col_partitions,                          // in
    unsigned num_partitions,                              // in
    hls::stream<EDGE_PLD_T> ML_to_SF_1_stream[PACK_SIZE]  // out
) {
  IDX_T matrix_pkt_offset = num_partitions * 2;
  for (unsigned col_partition_idx = 0; col_partition_idx < num_col_partitions;
       col_partition_idx++) {
#pragma HLS pipeline off

    // read CPSR matadata: partition start and lengths of each stream
    unsigned part_id =
        row_partition_idx * num_col_partitions + col_partition_idx;
    IDX_T partition_info_idx = 2 * part_id;
    IDX_T partition_start = matrix_hbm[partition_info_idx].indices.data[0];
    PACKED_IDX_T part_len_pkt = matrix_hbm[partition_info_idx + 1].indices;
    IDX_T stream_length[PACK_SIZE];
#pragma HLS array_partition variable = stream_length complete
    for (unsigned k = 0; k < PACK_SIZE; k++) {
#pragma HLS unroll
      stream_length[k] = part_len_pkt.data[k];
    }

    // prepare to read
    unsigned num_reads = array_max<IDX_T, PACK_SIZE>(stream_length);
    IDX_T row_idx[PACK_SIZE];
#pragma HLS ARRAY_PARTITION variable = row_idx complete
    for (unsigned k = 0; k < PACK_SIZE; k++) {
#pragma HLS UNROLL
      row_idx[k] = k;
    }

    // attach start-of-data
    for (unsigned k = 0; k < PACK_SIZE; k++) {
#pragma HLS UNROLL
      ML_to_SF_1_stream[k].write(EDGE_PLD_SOD);
    }

  // TODO: maunally control the burst length will help?
  loop_matrix_loader:
    for (unsigned i = 0; i < num_reads; i++) {
#pragma HLS PIPELINE II = 1
      SPMV_MAT_PKT_T mat_pkt =
          matrix_hbm[i + partition_start + matrix_pkt_offset];
      for (unsigned k = 0; k < PACK_SIZE; k++) {
#pragma HLS UNROLL
        if (i < stream_length[k]) {
          if (mat_pkt.indices.data[k] == IDX_MARKER) {
            // Be careful: mat_pkt.vals.data[k] can not be larger than power(2,
            // 8)
            row_idx[k] += (PACK_SIZE * mat_pkt.vals.data[k](31, 32 - IBITS));
          } else {
            EDGE_PLD_T input_to_SF_1;
            input_to_SF_1.mat_val = mat_pkt.vals.data[k];
            input_to_SF_1.col_idx = mat_pkt.indices.data[k];
            input_to_SF_1.row_idx = row_idx[k];
            ML_to_SF_1_stream[k].write(input_to_SF_1);
          }
        }
      }
    }

    // attach end-of-data
    for (unsigned k = 0; k < PACK_SIZE; k++) {
#pragma HLS UNROLL
      ML_to_SF_1_stream[k].write(EDGE_PLD_EOD);
    }

  }  // loop over column partitions

  // attach end-of-stream
  for (unsigned k = 0; k < PACK_SIZE; k++) {
#pragma HLS UNROLL
    ML_to_SF_1_stream[k].write(EDGE_PLD_EOS);
  }
}

void spmv_vector_unpacker(hls::stream<VEC_AXIS_T>& vec_in,
                          hls::stream<VEC_PLD_T> vec_out[PACK_SIZE]) {
  bool exit = false;
  while (!exit) {
#pragma HLS pipeline II = 1
    VEC_AXIS_T pkt = vec_in.read();
    for (unsigned k = 0; k < PACK_SIZE; k++) {
#pragma HLS unroll
      VEC_PLD_T p;
      VAL_T_BITCAST(p.val) = VEC_AXIS_VAL(pkt, k);
      p.idx = VEC_AXIS_PKT_IDX(pkt) * PACK_SIZE + k;
      p.inst = pkt.user;
      vec_out[k].write(p);
    }
    exit = (pkt.user == EOS);
  }
}

#ifndef __SYNTHESIS__
// #define SPMV_RESULT_PACKER_LINE_TRACING
#endif

void spmv_result_packer(hls::stream<VEC_PLD_T> res_in[PACK_SIZE],
                        hls::stream<VEC_AXIS_T>& res_out) {
  bool exit = false;
  unsigned pkt_idx = 0;
  while (!exit) {
#pragma HLS pipeline II = 1
    ap_uint<PACK_SIZE> got_SOD = 0;
    ap_uint<PACK_SIZE> got_EOD = 0;
    ap_uint<PACK_SIZE> got_EOS = 0;
    VEC_AXIS_T pkt;
    for (unsigned k = 0; k < PACK_SIZE; k++) {
#pragma HLS unroll
      VEC_PLD_T p = res_in[k].read();
      VEC_AXIS_VAL(pkt, k) = VAL_T_BITCAST(p.val);
      switch (p.inst) {
        case SOD:
          got_SOD[k] = 1;
          got_EOD[k] = 0;
          got_EOS[k] = 0;
          break;
        case EOD:
          got_SOD[k] = 0;
          got_EOD[k] = 1;
          got_EOS[k] = 0;
          break;
        case EOS:
          got_SOD[k] = 0;
          got_EOD[k] = 0;
          got_EOS[k] = 1;
          break;
        default:
          got_SOD[k] = 0;
          got_EOD[k] = 0;
          got_EOS[k] = 0;
          break;
      }
    }

    if (got_SOD.and_reduce()) {
      pkt.user = SOD;
      VEC_AXIS_PKT_IDX(pkt) = 0;
    } else if (got_EOD.and_reduce()) {
      pkt.user = EOD;
      VEC_AXIS_PKT_IDX(pkt) = 0;
    } else if (got_EOS.and_reduce()) {
      pkt.user = EOS;
      VEC_AXIS_PKT_IDX(pkt) = 0;
      exit = true;
    } else {
      pkt.user = 0;
      VEC_AXIS_PKT_IDX(pkt) = pkt_idx;
      pkt_idx++;
    }
    res_out.write(pkt);
#ifdef SPMV_RESULT_PACKER_LINE_TRACING
    std::cout << "SpMV Result Packer write output: " << pkt << std::endl;
#endif
  }
}

// one computational cluster
void spmv_cluster(const SPMV_MAT_PKT_T* matrix_hbm,  // in
                  hls::stream<VEC_AXIS_T>& vec_in,   // in
                  hls::stream<VEC_AXIS_T>& res_out,  // out
                  const unsigned num_rows,           // in
                  const unsigned num_columns,        // in
                  const unsigned row_partition_idx   // in
) {
  hls::stream<EDGE_PLD_T> ML2SF[PACK_SIZE];
  hls::stream<EDGE_PLD_T> SF2VAU[PACK_SIZE];
  hls::stream<UPDATE_PLD_T> VAU2SF[PACK_SIZE];
  hls::stream<UPDATE_PLD_T> SF2PE[PACK_SIZE];
  hls::stream<VEC_PLD_T> PE2PK[PACK_SIZE];
  hls::stream<VEC_PLD_T> UPK2VAU[PACK_SIZE];
#pragma HLS stream variable = ML2SF depth = FIFO_DEPTH
#pragma HLS stream variable = SF2VAU depth = FIFO_DEPTH
#pragma HLS stream variable = VAU2SF depth = FIFO_DEPTH
#pragma HLS stream variable = FS2PE depth = FIFO_DEPTH
#pragma HLS stream variable = PE2PK depth = FIFO_DEPTH
#pragma HLS stream variable = UPK2VAU depth = FIFO_DEPTH

#pragma HLS bind_storage variable = ML2SF type = FIFO impl = SRL
#pragma HLS bind_storage variable = SF2VAU type = FIFO impl = SRL
#pragma HLS bind_storage variable = VAU2SF type = FIFO impl = SRL
#pragma HLS bind_storage variable = FS2PE type = FIFO impl = SRL
#pragma HLS bind_storage variable = PE2PK type = FIFO impl = SRL
#pragma HLS bind_storage variable = UPK2VAU type = FIFO impl = SRL

  unsigned rows_per_ch_in_last_row_part;
  if (num_rows % LOGICAL_OB_SIZE == 0) {
    rows_per_ch_in_last_row_part = LOGICAL_OB_SIZE / NUM_HBM_CHANNELS;
  } else {
    rows_per_ch_in_last_row_part =
        num_rows % LOGICAL_OB_SIZE / NUM_HBM_CHANNELS;
  }

  unsigned num_row_partitions =
      (num_rows + LOGICAL_OB_SIZE - 1) / LOGICAL_OB_SIZE;
  unsigned num_col_partitions =
      (num_columns + LOGICAL_VB_SIZE - 1) / LOGICAL_VB_SIZE;
  unsigned num_partitions = num_row_partitions * num_col_partitions;

  // number of rows per cluster in this row partition
  unsigned rows_per_c_in_partition = LOGICAL_OB_SIZE / NUM_HBM_CHANNELS;
  if (row_partition_idx == num_row_partitions - 1) {
    rows_per_c_in_partition = rows_per_ch_in_last_row_part;
  }

#ifdef SPMV_LINE_TRACING
  std::cout << "INFO : SpMV Kernel Started: row partition " << row_partition_idx
            << " with " << rows_per_c_in_partition << " rows per cluster"
            << std::endl;
#endif

#pragma HLS dataflow

  spmv_vector_unpacker(vec_in, UPK2VAU);
#ifdef SPMV_CLUSTER_H_LINE_TRACING
  std::cout << "INFO : [SpMV cluster] Vector Unpacker complete" << std::endl;
#endif

  CPSR_matrix_loader(matrix_hbm, row_partition_idx, num_col_partitions,
                     num_partitions, ML2SF);

#ifdef SPMV_CLUSTER_H_LINE_TRACING
  std::cout << "INFO : [SpMV cluster] Matrix Loader complete" << std::endl;
#endif

  shuffler<EDGE_PLD_T, PACK_SIZE>(ML2SF, SF2VAU);

#ifdef SPMV_CLUSTER_H_LINE_TRACING
  std::cout << "INFO : [SpMV cluster] Shuffler 1 complete" << std::endl;
#endif

  vecbuf_access_unit<0, VB_BANK_SIZE, PACK_SIZE>(SF2VAU[0], UPK2VAU[0],
                                                 VAU2SF[0], num_col_partitions);
  vecbuf_access_unit<1, VB_BANK_SIZE, PACK_SIZE>(SF2VAU[1], UPK2VAU[1],
                                                 VAU2SF[1], num_col_partitions);
  vecbuf_access_unit<2, VB_BANK_SIZE, PACK_SIZE>(SF2VAU[2], UPK2VAU[2],
                                                 VAU2SF[2], num_col_partitions);
  vecbuf_access_unit<3, VB_BANK_SIZE, PACK_SIZE>(SF2VAU[3], UPK2VAU[3],
                                                 VAU2SF[3], num_col_partitions);
  vecbuf_access_unit<4, VB_BANK_SIZE, PACK_SIZE>(SF2VAU[4], UPK2VAU[4],
                                                 VAU2SF[4], num_col_partitions);
  vecbuf_access_unit<5, VB_BANK_SIZE, PACK_SIZE>(SF2VAU[5], UPK2VAU[5],
                                                 VAU2SF[5], num_col_partitions);
  vecbuf_access_unit<6, VB_BANK_SIZE, PACK_SIZE>(SF2VAU[6], UPK2VAU[6],
                                                 VAU2SF[6], num_col_partitions);
  vecbuf_access_unit<7, VB_BANK_SIZE, PACK_SIZE>(SF2VAU[7], UPK2VAU[7],
                                                 VAU2SF[7], num_col_partitions);

#ifdef SPMV_CLUSTER_H_LINE_TRACING
  std::cout << "INFO : [SpMV cluster] Vector Access Unit complete" << std::endl;
#endif

  shuffler<UPDATE_PLD_T, PACK_SIZE>(VAU2SF, SF2PE);

#ifdef SPMV_CLUSTER_H_LINE_TRACING
  std::cout << "INFO : [SpMV cluster] Shuffler 2 complete" << std::endl;
#endif

  pe<0, OB_BANK_SIZE, PACK_SIZE>(SF2PE[0], PE2PK[0],
                                 rows_per_c_in_partition / PACK_SIZE);
  pe<1, OB_BANK_SIZE, PACK_SIZE>(SF2PE[1], PE2PK[1],
                                 rows_per_c_in_partition / PACK_SIZE);
  pe<2, OB_BANK_SIZE, PACK_SIZE>(SF2PE[2], PE2PK[2],
                                 rows_per_c_in_partition / PACK_SIZE);
  pe<3, OB_BANK_SIZE, PACK_SIZE>(SF2PE[3], PE2PK[3],
                                 rows_per_c_in_partition / PACK_SIZE);
  pe<4, OB_BANK_SIZE, PACK_SIZE>(SF2PE[4], PE2PK[4],
                                 rows_per_c_in_partition / PACK_SIZE);
  pe<5, OB_BANK_SIZE, PACK_SIZE>(SF2PE[5], PE2PK[5],
                                 rows_per_c_in_partition / PACK_SIZE);
  pe<6, OB_BANK_SIZE, PACK_SIZE>(SF2PE[6], PE2PK[6],
                                 rows_per_c_in_partition / PACK_SIZE);
  pe<7, OB_BANK_SIZE, PACK_SIZE>(SF2PE[7], PE2PK[7],
                                 rows_per_c_in_partition / PACK_SIZE);

#ifdef SPMV_CLUSTER_H_LINE_TRACING
  std::cout << "INFO : [SpMV cluster] Process Elements complete" << std::endl;
#endif

  spmv_result_packer(PE2PK, res_out);

#ifdef SPMV_CLUSTER_H_LINE_TRACING
  std::cout << "INFO : [SpMV cluster] Result Packer complete" << std::endl;
#endif
}

#endif  // SPMV_CLUSTER_H_

#ifndef __SYNTHESIS__
#define SPMV_LINE_TRACING
// #define RESULT_DRAIN_LINE_TRACING
#endif

void vector_loader(const PACKED_VAL_T* packed_dense_vector,  // in
                   const unsigned num_cols,                  // in
                   hls::stream<VEC_AXIS_T> duplicate[16]     // out
) {
  unsigned num_col_partitions =
      (num_cols + LOGICAL_VB_SIZE - 1) / LOGICAL_VB_SIZE;
  unsigned num_col_last_partition;
  if (num_cols % LOGICAL_VB_SIZE == 0) {
    num_col_last_partition = LOGICAL_VB_SIZE;
  } else {
    num_col_last_partition = num_cols % LOGICAL_VB_SIZE;
  }

vector_loader_over_col_partitions:
  for (unsigned part_id = 0; part_id < num_col_partitions; part_id++) {
#pragma HLS pipeline off

    // attach switch column partition token
    VEC_AXIS_T pout_sod;
    pout_sod.user = SOD;
    pout_sod.data = 0;
    for (unsigned k = 0; k < 16; k++) {
#pragma HLS unroll
      duplicate[k].write(pout_sod);
    }

    unsigned part_len = LOGICAL_VB_SIZE;
    if (part_id == num_col_partitions - 1) {
      part_len = num_col_last_partition;
    }

    assert(part_len % PACK_SIZE == 0);

  loop_load_vector_packets:
    for (unsigned i = 0; i < part_len / PACK_SIZE; i++) {
#pragma HLS pipeline II = 1
      IDX_T dv_idx = i + part_id * VB_PER_CLUSTER / PACK_SIZE;
      PACKED_VAL_T dv_pkt = packed_dense_vector[dv_idx];
      VEC_AXIS_T pout[16];
      for (unsigned x = 0; x < 16; x++) {
#pragma HLS unroll
        for (unsigned k = 0; k < PACK_SIZE; k++) {
#pragma HLS unroll
          VEC_AXIS_VAL(pout[x], k) = VAL_T_BITCAST(dv_pkt.data[k]);
        }
        pout[x].user = 0;
        VEC_AXIS_PKT_IDX(pout[x]) = dv_idx;
        duplicate[x].write(pout[x]);
      }
    }

    // attach switch column partition token
    VEC_AXIS_T pout_eod;
    pout_eod.user = EOD;
    pout_eod.data = 0;
    for (unsigned k = 0; k < 16; k++) {
#pragma HLS unroll
      duplicate[k].write(pout_eod);
    }
  }

  // attach last token
  VEC_AXIS_T pout_eos;
  pout_eos.user = EOS;
  pout_eos.data = 0;
  for (unsigned k = 0; k < 16; k++) {
#pragma HLS unroll
    duplicate[k].write(pout_eos);
  }
}

void result_drain(PACKED_VAL_T* packed_dense_result,         // out
                  const unsigned row_part_id,                // in
                  hls::stream<VEC_AXIS_T> from_clusters[16]  // in
) {
  // write back
  char current_input = 0;
  ap_uint<16> finished = 0;
  unsigned write_counter = 0;
  bool exit = false;
  unsigned pkt_idx_offset = row_part_id * LOGICAL_OB_SIZE / PACK_SIZE;
result_drain_main_loop:
  while (!exit) {
#pragma HLS pipeline II = 1
    VEC_AXIS_T pkt;
    bool do_write = false;

    if (!finished[current_input]) {
      pkt = from_clusters[current_input].read();
#ifdef RESULT_DRAIN_LINE_TRACING
      std::cout << "RD got pkt from cluster " << current_input << ": " << pkt;
#endif
      if (pkt.user == EOS) {
        finished[current_input] = true;
        do_write = false;
      } else if (pkt.user != SOD && pkt.user != EOD) {
        do_write = true;
      }
    } else {
      do_write = false;
    }
    current_input = (current_input + 1) % 16;

    exit = finished.and_reduce();

    unsigned abs_pkt_idx = write_counter + pkt_idx_offset;
    if (do_write) {
      PACKED_VAL_T rout;
      for (unsigned k = 0; k < PACK_SIZE; k++) {
#pragma HLS unroll
        VAL_T_BITCAST(rout.data[k]) = VEC_AXIS_VAL(pkt, k);
      }
      write_counter++;
      packed_dense_result[abs_pkt_idx] = rout;
    }

#ifdef RESULT_DRAIN_LINE_TRACING
    if (do_write) {
      std::cout << ", written to " << abs_pkt_idx << std::endl;
    } else {
      std::cout << std::endl;
    }
#endif

  }  // while
}

extern "C" {
void spmv(const SPMV_MAT_PKT_T* matrix_hbm_0,       // in
          const SPMV_MAT_PKT_T* matrix_hbm_1,       // in
          const SPMV_MAT_PKT_T* matrix_hbm_2,       // in
          const SPMV_MAT_PKT_T* matrix_hbm_3,       // in
          const SPMV_MAT_PKT_T* matrix_hbm_4,       // in
          const SPMV_MAT_PKT_T* matrix_hbm_5,       // in
          const SPMV_MAT_PKT_T* matrix_hbm_6,       // in
          const SPMV_MAT_PKT_T* matrix_hbm_7,       // in
          const SPMV_MAT_PKT_T* matrix_hbm_8,       // in
          const SPMV_MAT_PKT_T* matrix_hbm_9,       // in
          const SPMV_MAT_PKT_T* matrix_hbm_10,      // in
          const SPMV_MAT_PKT_T* matrix_hbm_11,      // in
          const SPMV_MAT_PKT_T* matrix_hbm_12,      // in
          const SPMV_MAT_PKT_T* matrix_hbm_13,      // in
          const SPMV_MAT_PKT_T* matrix_hbm_14,      // in
          const SPMV_MAT_PKT_T* matrix_hbm_15,      // in
          const PACKED_VAL_T* packed_dense_vector,  // in
          PACKED_VAL_T* packed_dense_result,        // out
          const unsigned num_rows,                  // in
          const unsigned num_columns,               // in
          const unsigned row_partition_idx          // in
) {
#pragma HLS interface m_axi port = matrix_hbm_0 offset = slave bundle = \
    spmv_mat0
#pragma HLS interface m_axi port = matrix_hbm_1 offset = slave bundle = \
    spmv_mat1
#pragma HLS interface m_axi port = matrix_hbm_2 offset = slave bundle = \
    spmv_mat2
#pragma HLS interface m_axi port = matrix_hbm_3 offset = slave bundle = \
    spmv_mat3
#pragma HLS interface m_axi port = matrix_hbm_4 offset = slave bundle = \
    spmv_mat4
#pragma HLS interface m_axi port = matrix_hbm_5 offset = slave bundle = \
    spmv_mat5
#pragma HLS interface m_axi port = matrix_hbm_6 offset = slave bundle = \
    spmv_mat6
#pragma HLS interface m_axi port = matrix_hbm_7 offset = slave bundle = \
    spmv_mat7
#pragma HLS interface m_axi port = matrix_hbm_8 offset = slave bundle = \
    spmv_mat8
#pragma HLS interface m_axi port = matrix_hbm_9 offset = slave bundle = \
    spmv_mat9
#pragma HLS interface m_axi port = matrix_hbm_10 offset = slave bundle = \
    spmv_mat10
#pragma HLS interface m_axi port = matrix_hbm_11 offset = slave bundle = \
    spmv_mat11
#pragma HLS interface m_axi port = matrix_hbm_12 offset = slave bundle = \
    spmv_mat12
#pragma HLS interface m_axi port = matrix_hbm_13 offset = slave bundle = \
    spmv_mat13
#pragma HLS interface m_axi port = matrix_hbm_14 offset = slave bundle = \
    spmv_mat14
#pragma HLS interface m_axi port = matrix_hbm_15 offset = slave bundle = \
    spmv_mat15

#pragma HLS interface s_axilite port = matrix_hbm_0 bundle = control
#pragma HLS interface s_axilite port = matrix_hbm_1 bundle = control
#pragma HLS interface s_axilite port = matrix_hbm_2 bundle = control
#pragma HLS interface s_axilite port = matrix_hbm_3 bundle = control
#pragma HLS interface s_axilite port = matrix_hbm_4 bundle = control
#pragma HLS interface s_axilite port = matrix_hbm_5 bundle = control
#pragma HLS interface s_axilite port = matrix_hbm_6 bundle = control
#pragma HLS interface s_axilite port = matrix_hbm_7 bundle = control
#pragma HLS interface s_axilite port = matrix_hbm_8 bundle = control
#pragma HLS interface s_axilite port = matrix_hbm_9 bundle = control
#pragma HLS interface s_axilite port = matrix_hbm_10 bundle = control
#pragma HLS interface s_axilite port = matrix_hbm_11 bundle = control
#pragma HLS interface s_axilite port = matrix_hbm_12 bundle = control
#pragma HLS interface s_axilite port = matrix_hbm_13 bundle = control
#pragma HLS interface s_axilite port = matrix_hbm_14 bundle = control
#pragma HLS interface s_axilite port = matrix_hbm_15 bundle = control

#pragma HLS interface s_axilite port = num_rows bundle = control
#pragma HLS interface s_axilite port = num_columns bundle = control
#pragma HLS interface s_axilite port = row_partition_idx bundle = control
#pragma HLS interface s_axilite port = return bundle = control

  hls::stream<VEC_AXIS_T> vec_dup_0;
  hls::stream<VEC_AXIS_T> vec_dup_1;
  hls::stream<VEC_AXIS_T> vec_dup_2;
  hls::stream<VEC_AXIS_T> vec_dup_3;
  hls::stream<VEC_AXIS_T> vec_dup_4;
  hls::stream<VEC_AXIS_T> vec_dup_5;
  hls::stream<VEC_AXIS_T> vec_dup_6;
  hls::stream<VEC_AXIS_T> vec_dup_7;
  hls::stream<VEC_AXIS_T> vec_dup_8;
  hls::stream<VEC_AXIS_T> vec_dup_9;
  hls::stream<VEC_AXIS_T> vec_dup_10;
  hls::stream<VEC_AXIS_T> vec_dup_11;
  hls::stream<VEC_AXIS_T> vec_dup_12;
  hls::stream<VEC_AXIS_T> vec_dup_13;
  hls::stream<VEC_AXIS_T> vec_dup_14;
  hls::stream<VEC_AXIS_T> vec_dup_15;

  hls::stream<VEC_AXIS_T> res_0;
  hls::stream<VEC_AXIS_T> res_1;
  hls::stream<VEC_AXIS_T> res_2;
  hls::stream<VEC_AXIS_T> res_3;
  hls::stream<VEC_AXIS_T> res_4;
  hls::stream<VEC_AXIS_T> res_5;
  hls::stream<VEC_AXIS_T> res_6;
  hls::stream<VEC_AXIS_T> res_7;
  hls::stream<VEC_AXIS_T> res_8;
  hls::stream<VEC_AXIS_T> res_9;
  hls::stream<VEC_AXIS_T> res_10;
  hls::stream<VEC_AXIS_T> res_11;
  hls::stream<VEC_AXIS_T> res_12;
  hls::stream<VEC_AXIS_T> res_13;
  hls::stream<VEC_AXIS_T> res_14;
  hls::stream<VEC_AXIS_T> res_15;

#pragma HLS stream variable = vec_dup depth = FIFO_DEPTH
#pragma HLS stream variable = res depth = FIFO_DEPTH
#pragma HLS bind_storage variable = vec_dup type = FIFO impl = SRL
#pragma HLS bind_storage variable = res type = FIFO impl = SRL

#pragma HLS dataflow

  vector_loader(packed_dense_vector, num_columns, vec_dup);

#ifdef SPMV_LINE_TRACING
  std::cout << "INFO : Vector Loader Complete" << std::endl;
#endif
  spmv_cluster(matrix_hbm_0, vec_dup_0, res_0, num_rows, num_columns,
               row_partition_idx);

#ifdef SPMV_LINE_TRACING
  std::cout << "INFO : Cluster 0 Complete" << std::endl;
#endif

  spmv_cluster(matrix_hbm_1, vec_dup_1, res_1, num_rows, num_columns,
               row_partition_idx);
#ifdef SPMV_LINE_TRACING
  std::cout << "INFO : Cluster 1 Complete" << std::endl;
#endif
  spmv_cluster(matrix_hbm_2, vec_dup_2, res_2, num_rows, num_columns,
               row_partition_idx);
#ifdef SPMV_LINE_TRACING
  std::cout << "INFO : Cluster 2 Complete" << std::endl;
#endif
  spmv_cluster(matrix_hbm_3, vec_dup_3, res_3, num_rows, num_columns,
               row_partition_idx);
#ifdef SPMV_LINE_TRACING
  std::cout << "INFO : Cluster 3 Complete" << std::endl;
#endif
  spmv_cluster(matrix_hbm_4, vec_dup_4, res_4, num_rows, num_columns,
               row_partition_idx);
#ifdef SPMV_LINE_TRACING
  std::cout << "INFO : Cluster 4 Complete" << std::endl;
#endif
  spmv_cluster(matrix_hbm_5, vec_dup_5, res_5, num_rows, num_columns,
               row_partition_idx);
#ifdef SPMV_LINE_TRACING
  std::cout << "INFO : Cluster 5 Complete" << std::endl;
#endif
  spmv_cluster(matrix_hbm_6, vec_dup_6, res_6, num_rows, num_columns,
               row_partition_idx);
#ifdef SPMV_LINE_TRACING
  std::cout << "INFO : Cluster 6 Complete" << std::endl;
#endif
  spmv_cluster(matrix_hbm_7, vec_dup_7, res_7, num_rows, num_columns,
               row_partition_idx);
#ifdef SPMV_LINE_TRACING
  std::cout << "INFO : Cluster 7 Complete" << std::endl;
#endif
  spmv_cluster(matrix_hbm_8, vec_dup_8, res_8, num_rows, num_columns,
               row_partition_idx);
#ifdef SPMV_LINE_TRACING
  std::cout << "INFO : Cluster 8 Complete" << std::endl;
#endif
  spmv_cluster(matrix_hbm_9, vec_dup_9, res_9, num_rows, num_columns,
               row_partition_idx);
#ifdef SPMV_LINE_TRACING
  std::cout << "INFO : Cluster 9 Complete" << std::endl;
#endif
  spmv_cluster(matrix_hbm_10, vec_dup_10, res_10, num_rows, num_columns,
               row_partition_idx);
#ifdef SPMV_LINE_TRACING
  std::cout << "INFO : Cluster 10 Complete" << std::endl;
#endif
  spmv_cluster(matrix_hbm_11, vec_dup_11, res_11, num_rows, num_columns,
               row_partition_idx);
#ifdef SPMV_LINE_TRACING
  std::cout << "INFO : Cluster 11 Complete" << std::endl;
#endif
  spmv_cluster(matrix_hbm_12, vec_dup_12, res_12, num_rows, num_columns,
               row_partition_idx);
#ifdef SPMV_LINE_TRACING
  std::cout << "INFO : Cluster 12 Complete" << std::endl;
#endif
  spmv_cluster(matrix_hbm_13, vec_dup_13, res_13, num_rows, num_columns,
               row_partition_idx);
#ifdef SPMV_LINE_TRACING
  std::cout << "INFO : Cluster 13 Complete" << std::endl;
#endif
  spmv_cluster(matrix_hbm_14, vec_dup_14, res_14, num_rows, num_columns,
               row_partition_idx);
#ifdef SPMV_LINE_TRACING
  std::cout << "INFO : Cluster 14 Complete" << std::endl;
#endif
  spmv_cluster(matrix_hbm_15, vec_dup_15, res_15, num_rows, num_columns,
               row_partition_idx);
#ifdef SPMV_LINE_TRACING
  std::cout << "INFO : Cluster 15 Complete" << std::endl;
#endif
  result_drain(packed_dense_result, row_partition_idx, res);

#ifdef SPMV_LINE_TRACING
  std::cout << "INFO : Result Drain Complete" << std::endl;
#endif

}  // kernel
}  // extern "C"
