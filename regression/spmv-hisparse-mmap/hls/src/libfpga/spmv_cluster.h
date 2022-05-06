#ifndef SPMV_CLUSTER_H_
#define SPMV_CLUSTER_H_

#include <hls_stream.h>
#include <ap_fixed.h>

#include "common.h"
#include "shuffle.h"
#include "vecbuf_access_unit.h"
#include "pe.h"

#include <hls_stream.h>
#include <ap_fixed.h>

#include "common.h"
#include "spmv_cluster.h"

#ifndef __SYNTHESIS__
// #define SPMV_CLUSTER_H_LINE_TRACING
#endif

template<typename T, unsigned len>
T array_max(T array[len]) {
    #pragma HLS inline
    #pragma HLS expression_balance
    T result = 0;
    for (unsigned i = 0; i < len; i++) {
        #pragma HLS unroll
        result = (array[i] > result)? array[i] : result;
    }
    return result;
}

void CPSR_matrix_loader(
    const SPMV_MAT_PKT_T *matrix_hbm,                      // in
    unsigned row_partition_idx,                            // in
    unsigned num_col_partitions,                           // in
    unsigned num_partitions,                               // in
    hls::stream<EDGE_PLD_T> ML_to_SF_1_stream[PACK_SIZE]   // out
) {
    IDX_T matrix_pkt_offset = num_partitions * 2;
    for (unsigned col_partition_idx = 0; col_partition_idx < num_col_partitions; col_partition_idx++) {
        #pragma HLS pipeline off

        // read CPSR matadata: partition start and lengths of each stream
        unsigned part_id = row_partition_idx * num_col_partitions + col_partition_idx;
        IDX_T partition_info_idx = 2 * part_id;
        IDX_T partition_start = matrix_hbm[partition_info_idx].indices.data[0];
        PACKED_IDX_T part_len_pkt = matrix_hbm[partition_info_idx + 1].indices;
        IDX_T stream_length[PACK_SIZE];
        #pragma HLS array_partition variable=stream_length complete
        for (unsigned k = 0; k < PACK_SIZE; k++) {
            #pragma HLS unroll
            stream_length[k] = part_len_pkt.data[k];
        }

        // prepare to read
        unsigned num_reads = array_max<IDX_T, PACK_SIZE>(stream_length);
        IDX_T row_idx[PACK_SIZE];
        #pragma HLS ARRAY_PARTITION variable=row_idx complete
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
            #pragma HLS PIPELINE II=1
            SPMV_MAT_PKT_T mat_pkt = matrix_hbm[i + partition_start + matrix_pkt_offset];
            for (unsigned k = 0; k < PACK_SIZE; k++) {
                #pragma HLS UNROLL
                if (i < stream_length[k]) {
                    if (mat_pkt.indices.data[k] == IDX_MARKER) {
                        // Be careful: mat_pkt.vals.data[k] can not be larger than power(2, 8)
                        row_idx[k] += (PACK_SIZE * mat_pkt.vals.data[k](31, 32-IBITS));
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

    } // loop over column partitions

    // attach end-of-stream
    for (unsigned k = 0; k < PACK_SIZE; k++) {
        #pragma HLS UNROLL
        ML_to_SF_1_stream[k].write(EDGE_PLD_EOS);
    }
}

void spmv_vector_unpacker (
    hls::stream<VEC_AXIS_T> &vec_in,
    hls::stream<VEC_PLD_T> vec_out[PACK_SIZE]
) {
    bool exit = false;
    while (!exit) {
        #pragma HLS pipeline II=1
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

void spmv_result_packer (
    hls::stream<VEC_PLD_T> res_in[PACK_SIZE],
    hls::stream<VEC_AXIS_T> &res_out
) {
    bool exit = false;
    unsigned pkt_idx = 0;
    while (!exit) {
        #pragma HLS pipeline II=1
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
void spmv_cluster(
    const SPMV_MAT_PKT_T *matrix_hbm,       // in
    hls::stream<VEC_AXIS_T> &vec_in,        // in
    hls::stream<VEC_AXIS_T> &res_out,       // out
    const unsigned num_rows,                  // in
    const unsigned num_columns,               // in
    const unsigned row_partition_idx          // in
) {

    hls::stream<EDGE_PLD_T> ML2SF[PACK_SIZE];
    hls::stream<EDGE_PLD_T> SF2VAU[PACK_SIZE];
    hls::stream<UPDATE_PLD_T> VAU2SF[PACK_SIZE];
    hls::stream<UPDATE_PLD_T> SF2PE[PACK_SIZE];
    hls::stream<VEC_PLD_T> PE2PK[PACK_SIZE];
    hls::stream<VEC_PLD_T> UPK2VAU[PACK_SIZE];
    #pragma HLS stream variable=ML2SF   depth=FIFO_DEPTH
    #pragma HLS stream variable=SF2VAU  depth=FIFO_DEPTH
    #pragma HLS stream variable=VAU2SF  depth=FIFO_DEPTH
    #pragma HLS stream variable=FS2PE   depth=FIFO_DEPTH
    #pragma HLS stream variable=PE2PK   depth=FIFO_DEPTH
    #pragma HLS stream variable=UPK2VAU depth=FIFO_DEPTH

    #pragma HLS bind_storage variable=ML2SF   type=FIFO impl=SRL
    #pragma HLS bind_storage variable=SF2VAU  type=FIFO impl=SRL
    #pragma HLS bind_storage variable=VAU2SF  type=FIFO impl=SRL
    #pragma HLS bind_storage variable=FS2PE   type=FIFO impl=SRL
    #pragma HLS bind_storage variable=PE2PK   type=FIFO impl=SRL
    #pragma HLS bind_storage variable=UPK2VAU type=FIFO impl=SRL


    unsigned rows_per_ch_in_last_row_part;
    if (num_rows % LOGICAL_OB_SIZE == 0) {
        rows_per_ch_in_last_row_part = LOGICAL_OB_SIZE / NUM_HBM_CHANNELS;
    } else {
        rows_per_ch_in_last_row_part = num_rows % LOGICAL_OB_SIZE / NUM_HBM_CHANNELS;
    }

    unsigned num_row_partitions = (num_rows + LOGICAL_OB_SIZE - 1) / LOGICAL_OB_SIZE;
    unsigned num_col_partitions = (num_columns + LOGICAL_VB_SIZE - 1) / LOGICAL_VB_SIZE;
    unsigned num_partitions = num_row_partitions * num_col_partitions;


    // number of rows per cluster in this row partition
    unsigned rows_per_c_in_partition = LOGICAL_OB_SIZE / NUM_HBM_CHANNELS;
    if (row_partition_idx == num_row_partitions - 1) {
        rows_per_c_in_partition = rows_per_ch_in_last_row_part;
    }

#ifdef SPMV_LINE_TRACING
        std::cout << "INFO : SpMV Kernel Started: row partition " << row_partition_idx
                  << " with " << rows_per_c_in_partition << " rows per cluster" << std::endl;
#endif


    #pragma HLS dataflow

    spmv_vector_unpacker(
        vec_in,
        UPK2VAU
    );
#ifdef SPMV_CLUSTER_H_LINE_TRACING
    std::cout << "INFO : [SpMV cluster] Vector Unpacker complete" << std::endl;
#endif

    CPSR_matrix_loader(
        matrix_hbm,
        row_partition_idx,
        num_col_partitions,
        num_partitions,
        ML2SF
    );

#ifdef SPMV_CLUSTER_H_LINE_TRACING
    std::cout << "INFO : [SpMV cluster] Matrix Loader complete" << std::endl;
#endif

    shuffler<EDGE_PLD_T, PACK_SIZE>(
        ML2SF,
        SF2VAU
    );

#ifdef SPMV_CLUSTER_H_LINE_TRACING
    std::cout << "INFO : [SpMV cluster] Shuffler 1 complete" << std::endl;
#endif

    vecbuf_access_unit<0, VB_BANK_SIZE, PACK_SIZE>(
        SF2VAU[0],
        UPK2VAU[0],
        VAU2SF[0],
        num_col_partitions
    );
    vecbuf_access_unit<1, VB_BANK_SIZE, PACK_SIZE>(
        SF2VAU[1],
        UPK2VAU[1],
        VAU2SF[1],
        num_col_partitions
    );
    vecbuf_access_unit<2, VB_BANK_SIZE, PACK_SIZE>(
        SF2VAU[2],
        UPK2VAU[2],
        VAU2SF[2],
        num_col_partitions
    );
    vecbuf_access_unit<3, VB_BANK_SIZE, PACK_SIZE>(
        SF2VAU[3],
        UPK2VAU[3],
        VAU2SF[3],
        num_col_partitions
    );
    vecbuf_access_unit<4, VB_BANK_SIZE, PACK_SIZE>(
        SF2VAU[4],
        UPK2VAU[4],
        VAU2SF[4],
        num_col_partitions
    );
    vecbuf_access_unit<5, VB_BANK_SIZE, PACK_SIZE>(
        SF2VAU[5],
        UPK2VAU[5],
        VAU2SF[5],
        num_col_partitions
    );
    vecbuf_access_unit<6, VB_BANK_SIZE, PACK_SIZE>(
        SF2VAU[6],
        UPK2VAU[6],
        VAU2SF[6],
        num_col_partitions
    );
    vecbuf_access_unit<7, VB_BANK_SIZE, PACK_SIZE>(
        SF2VAU[7],
        UPK2VAU[7],
        VAU2SF[7],
        num_col_partitions
    );

#ifdef SPMV_CLUSTER_H_LINE_TRACING
    std::cout << "INFO : [SpMV cluster] Vector Access Unit complete" << std::endl;
#endif

    shuffler<UPDATE_PLD_T, PACK_SIZE>(
        VAU2SF,
        SF2PE
    );

#ifdef SPMV_CLUSTER_H_LINE_TRACING
    std::cout << "INFO : [SpMV cluster] Shuffler 2 complete" << std::endl;
#endif

    pe<0, OB_BANK_SIZE, PACK_SIZE>(
        SF2PE[0],
        PE2PK[0],
        rows_per_c_in_partition / PACK_SIZE
    );
    pe<1, OB_BANK_SIZE, PACK_SIZE>(
        SF2PE[1],
        PE2PK[1],
        rows_per_c_in_partition / PACK_SIZE
    );
    pe<2, OB_BANK_SIZE, PACK_SIZE>(
        SF2PE[2],
        PE2PK[2],
        rows_per_c_in_partition / PACK_SIZE
    );
    pe<3, OB_BANK_SIZE, PACK_SIZE>(
        SF2PE[3],
        PE2PK[3],
        rows_per_c_in_partition / PACK_SIZE
    );
    pe<4, OB_BANK_SIZE, PACK_SIZE>(
        SF2PE[4],
        PE2PK[4],
        rows_per_c_in_partition / PACK_SIZE
    );
    pe<5, OB_BANK_SIZE, PACK_SIZE>(
        SF2PE[5],
        PE2PK[5],
        rows_per_c_in_partition / PACK_SIZE
    );
    pe<6, OB_BANK_SIZE, PACK_SIZE>(
        SF2PE[6],
        PE2PK[6],
        rows_per_c_in_partition / PACK_SIZE
    );
    pe<7, OB_BANK_SIZE, PACK_SIZE>(
        SF2PE[7],
        PE2PK[7],
        rows_per_c_in_partition / PACK_SIZE
    );

#ifdef SPMV_CLUSTER_H_LINE_TRACING
    std::cout << "INFO : [SpMV cluster] Process Elements complete" << std::endl;
#endif

    spmv_result_packer(
        PE2PK,
        res_out
    );

#ifdef SPMV_CLUSTER_H_LINE_TRACING
    std::cout << "INFO : [SpMV cluster] Result Packer complete" << std::endl;
#endif
}

#endif // SPMV_CLUSTER_H_
