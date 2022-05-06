#include <tapa.h>
#include <ap_fixed.h>
#include <assert.h>

// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
// line tracing macros

    #ifndef __SYNTHESIS__
    #define SPMV_LINE_TRACING
    // #define SPMV_CLUSTER_LINE_TRACING
    // #define SPMV_RESULT_PACKER_LINE_TRACING
    // #define VAU_LINE_TRACING
    // #define PE_LINE_TRACING
    // #define RESULT_DRAIN_LINE_TRACING
    #endif
// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
// kernel configurations and data types

    #define IDX_MARKER 0xffffffff

    const unsigned PACK_SIZE = 8;

    //-------------------------------------------------------------------------
    // basic data types
    //-------------------------------------------------------------------------
    const unsigned IBITS = 8;
    const unsigned FBITS = 32 - IBITS;
    typedef unsigned IDX_T;
    typedef ap_ufixed<32, IBITS, AP_RND, AP_SAT> VAL_T;
    #define VAL_T_BITCAST(v) (v(31,0))

    //-------------------------------------------------------------------------
    // kernel-memory interface packet types
    //-------------------------------------------------------------------------
    typedef struct {IDX_T data[PACK_SIZE];} PACKED_IDX_T;
    typedef struct {VAL_T data[PACK_SIZE];} PACKED_VAL_T;

    typedef struct {
    PACKED_IDX_T indices;
    PACKED_VAL_T vals;
    } SPMV_MAT_PKT_T;

    typedef SPMV_MAT_PKT_T SPMSPV_MAT_PKT_T;

    typedef struct {IDX_T index; VAL_T val;} IDX_VAL_T;

    //-------------------------------------------------------------------------
    // intra-kernel dataflow payload types
    //-------------------------------------------------------------------------
    // 2-bit instruction
    typedef ap_uint<2> INST_T;
    #define SOD 0x1 // start-of-data
    #define EOD 0x2 // end-of-data
    #define EOS 0x3 // end-of-stream

    // edge payload (COO), used between SpMV matrix loader <-> SpMV shuffle 1
    struct EDGE_PLD_T {
        VAL_T mat_val;
        IDX_T row_idx;
        IDX_T col_idx;
        INST_T inst;
    };
    #define EDGE_PLD_SOD ((EDGE_PLD_T){0,0,0,SOD})
    #define EDGE_PLD_EOD ((EDGE_PLD_T){0,0,0,EOD})
    #define EDGE_PLD_EOS ((EDGE_PLD_T){0,0,0,EOS})

    // update payload, used by all PEs
    struct UPDATE_PLD_T {
        VAL_T mat_val;
        VAL_T vec_val;
        IDX_T row_idx;
        INST_T inst;
    };
    #define UPDATE_PLD_SOD ((UPDATE_PLD_T){0,0,0,SOD})
    #define UPDATE_PLD_EOD ((UPDATE_PLD_T){0,0,0,EOD})
    #define UPDATE_PLD_EOS ((UPDATE_PLD_T){0,0,0,EOS})

    // vector payload, used between SpMV vector unpacker <-> SpMV vector reader
    // and all PE outputs
    struct VEC_PLD_T{
        VAL_T val;
        IDX_T idx;
        INST_T inst;
    };
    #define VEC_PLD_SOD ((VEC_PLD_T){0,0,SOD})
    #define VEC_PLD_EOD ((VEC_PLD_T){0,0,EOD})
    #define VEC_PLD_EOS ((VEC_PLD_T){0,0,EOS})

    #ifndef __SYNTHESIS__
    std::string inst2str(INST_T inst) {
        switch (inst) {
            case SOD: return std::string("SOD");
            case EOD: return std::string("EOD");
            case EOS: return std::string("EOS");
            default:  return std::string(std::to_string((int)inst));
        }
    }

    std::ostream& operator<<(std::ostream& os, const EDGE_PLD_T &p) {
        os << '{'
            << "mat val: " << p.mat_val << '|'
            << "row idx: " << p.row_idx << '|'
            << "col idx: " << p.col_idx << '|'
            << "inst: "  << inst2str(p.inst) << '}';
        return os;
    }

    std::ostream& operator<<(std::ostream& os, const UPDATE_PLD_T &p) {
        os << '{'
            << "mat val: " << p.mat_val << '|'
            << "vec val: " << p.vec_val << '|'
            << "row idx: " << p.row_idx << '|'
            << "inst: "  << inst2str(p.inst) << '}';
        return os;
    }

    std::ostream& operator<<(std::ostream& os, const VEC_PLD_T &p) {
        os << '{'
            << "val: " << p.val << '|'
            << "idx: " << p.idx << '|'
            << "inst: "  << inst2str(p.inst) << '}';
        return os;
    }
    #endif

    //-------------------------------------------------------------------------
    // kernel-to-kernel streaming payload types
    //-------------------------------------------------------------------------

    // only works on Vitis 2020.2
    typedef struct {
        ap_uint<32 * (PACK_SIZE + 1)> data;
        ap_uint<2> user; // same as INST_T
    } VEC_AXIS_T;

    #define VEC_AXIS_PKT_IDX(p) (p.data(31,0))
    #define VEC_AXIS_VAL(p, i) (p.data(63 + 32 * i,32 + 32 * i))

    #ifndef __SYNTHESIS__
    std::ostream& operator<<(std::ostream& os, const VEC_AXIS_T &p) {
        os << '{' << "pktidx: " << VEC_AXIS_PKT_IDX(p) << '|';
        for (unsigned i = 0; i < PACK_SIZE; i++) {
            os << "val: " << float(VEC_AXIS_VAL(p, i)) / (1 << FBITS) << '|';
        }
        os << "user: "  << inst2str(p.user) << '}';
        return os;
    }
    #endif

    //-------------------------------------------------------------------------
    // Kernel configurations
    //-------------------------------------------------------------------------
    const unsigned FIFO_DEPTH = 64;

    const unsigned OB_BANK_SIZE = 1024 * 8;
    const unsigned VB_BANK_SIZE = 1024 * 4;

    const unsigned INTERLEAVE_FACTOR = 1;

    const unsigned OB_PER_CLUSTER = OB_BANK_SIZE * PACK_SIZE;
    const unsigned VB_PER_CLUSTER = VB_BANK_SIZE * PACK_SIZE;
    const unsigned SK0_CLUSTER = 4;
    const unsigned SK1_CLUSTER = 6;
    const unsigned SK2_CLUSTER = 6;
    const unsigned NUM_HBM_CHANNELS = SK0_CLUSTER + SK1_CLUSTER + SK2_CLUSTER;

    const unsigned LOGICAL_OB_SIZE = (SK0_CLUSTER + SK1_CLUSTER + SK2_CLUSTER) * OB_PER_CLUSTER;
    const unsigned LOGICAL_VB_SIZE = VB_PER_CLUSTER;
// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
// matrix loader

    IDX_T array_max(IDX_T array[PACK_SIZE]) {
        #pragma HLS inline
        IDX_T result = 0;
        for (unsigned i = 0; i < PACK_SIZE; i++) {
            #pragma HLS unroll
            result = (array[i] > result)? array[i] : result;
        }
        return result;
    }

    void CPSR_matrix_loader(
        tapa::mmap<SPMV_MAT_PKT_T> matrix_hbm,                      // in
        unsigned row_partition_idx,                            // in
        unsigned num_col_partitions,                           // in
        unsigned num_partitions,                               // in
        tapa::ostreams<EDGE_PLD_T, PACK_SIZE> &ML_to_SF_1_stream  // out
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
            unsigned num_reads = array_max(stream_length);
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
// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
// vector unpacker and packer

    // unpack the vector
    void spmv_vector_unpacker (
        tapa::istream<VEC_AXIS_T> &vec_in,
        tapa::ostreams<VEC_PLD_T, PACK_SIZE> &vec_out
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

    // pack the results
    void spmv_result_packer (
        tapa::istreams<VEC_PLD_T, PACK_SIZE> &res_in,
        tapa::ostream<VEC_AXIS_T> &res_out
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
// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
// 2 kinds of arbiters

    // latency of the arbiter, which is pipeline_depth - 1
    const unsigned ARBITER_LATENCY = 5;

    // arbiter for read requests (EDGE_PLD_T) (depends on col_idx)
    void arbiter_for_read_req(
        const EDGE_PLD_T in_pld[PACK_SIZE],
        EDGE_PLD_T resend_pld[PACK_SIZE],
        const ap_uint<PACK_SIZE> in_valid,
        ap_uint<1> in_resend[PACK_SIZE],
        unsigned xbar_sel[PACK_SIZE],
        ap_uint<PACK_SIZE> &out_valid,
        const unsigned rotate_priority
    ) {
        #pragma HLS pipeline II=1 enable_flush
        #pragma HLS latency min=ARBITER_LATENCY max=ARBITER_LATENCY

        #pragma HLS array_partition variable=in_pld complete
        #pragma HLS array_partition variable=xbar_sel complete

        // prioritized valid and addr
        ap_uint<PACK_SIZE> arb_p_in_valid = in_valid;
        IDX_T arb_p_in_addr[PACK_SIZE];
        IDX_T in_addr[PACK_SIZE];
        #pragma HLS array_partition variable=in_addr complete
        #pragma HLS array_partition variable=arb_p_in_addr complete

        for (unsigned i = 0; i < PACK_SIZE; i++) {
            #pragma HLS unroll
            arb_p_in_addr[i] = in_pld[(i + rotate_priority) % PACK_SIZE].col_idx;
            in_addr[i] = in_pld[i].col_idx;
        }

        arb_p_in_valid.rrotate(rotate_priority);

        loop_A_arbsearch:
        for (unsigned OLid = 0; OLid < PACK_SIZE; OLid++) {
            #pragma HLS unroll
            bool found = false;
            unsigned chosen_port = 0;

            loop_ab_logic_encoder_unroll:
            for (unsigned ILid_plus_1 = PACK_SIZE; ILid_plus_1 > 0; ILid_plus_1--) {
                #pragma HLS unroll
                if (arb_p_in_valid[ILid_plus_1 - 1] && ((arb_p_in_addr[ILid_plus_1 - 1] % PACK_SIZE) == OLid)) {
                    chosen_port = ILid_plus_1 - 1;
                    found = true;
                }
            }
            if (!found) {
                out_valid[OLid] = 0;
                xbar_sel[OLid] = 0;
            } else {
                out_valid[OLid] = 1;
                xbar_sel[OLid] = (chosen_port + rotate_priority) % PACK_SIZE;
            }
        }
        loop_A_grant:
        for (unsigned ILid = 0; ILid < PACK_SIZE; ILid++) {
            #pragma HLS unroll
            unsigned requested_olid = in_addr[ILid] % PACK_SIZE;
            bool in_granted = (in_valid[ILid]
                            && out_valid[requested_olid]
                            && (xbar_sel[requested_olid] == ILid));
            in_resend[ILid] = (in_valid[ILid] && !in_granted) ? 1 : 0;
            resend_pld[ILid] = in_pld[ILid];
        }
    }

    // arbiter for read responses (UPDATE_PLD_T) (depends on row_idx)
    void arbiter_for_read_resp(
        const UPDATE_PLD_T in_pld[PACK_SIZE],
        UPDATE_PLD_T resend_pld[PACK_SIZE],
        const ap_uint<PACK_SIZE> in_valid,
        ap_uint<1> in_resend[PACK_SIZE],
        unsigned xbar_sel[PACK_SIZE],
        ap_uint<PACK_SIZE> &out_valid,
        const unsigned rotate_priority
    ) {
        #pragma HLS pipeline II=1 enable_flush
        #pragma HLS latency min=ARBITER_LATENCY max=ARBITER_LATENCY

        #pragma HLS array_partition variable=in_pld complete
        #pragma HLS array_partition variable=xbar_sel complete

        // prioritized valid and addr
        ap_uint<PACK_SIZE> arb_p_in_valid = in_valid;
        IDX_T arb_p_in_addr[PACK_SIZE];
        IDX_T in_addr[PACK_SIZE];
        #pragma HLS array_partition variable=in_addr complete
        #pragma HLS array_partition variable=arb_p_in_addr complete

        for (unsigned i = 0; i < PACK_SIZE; i++) {
            #pragma HLS unroll
            arb_p_in_addr[i] = in_pld[(i + rotate_priority) % PACK_SIZE].row_idx;
            in_addr[i] = in_pld[i].row_idx;
        }

        arb_p_in_valid.rrotate(rotate_priority);

        loop_A_arbsearch:
        for (unsigned OLid = 0; OLid < PACK_SIZE; OLid++) {
            #pragma HLS unroll
            bool found = false;
            unsigned chosen_port = 0;

            loop_ab_logic_encoder_unroll:
            for (unsigned ILid_plus_1 = PACK_SIZE; ILid_plus_1 > 0; ILid_plus_1--) {
                #pragma HLS unroll
                if (arb_p_in_valid[ILid_plus_1 - 1] && ((arb_p_in_addr[ILid_plus_1 - 1] % PACK_SIZE) == OLid)) {
                    chosen_port = ILid_plus_1 - 1;
                    found = true;
                }
            }
            if (!found) {
                out_valid[OLid] = 0;
                xbar_sel[OLid] = 0;
            } else {
                out_valid[OLid] = 1;
                xbar_sel[OLid] = (chosen_port + rotate_priority) % PACK_SIZE;
            }
        }

        loop_A_grant:
        for (unsigned ILid = 0; ILid < PACK_SIZE; ILid++) {
            #pragma HLS unroll
            unsigned requested_olid = in_addr[ILid] % PACK_SIZE;
            bool in_granted = (in_valid[ILid]
                            && out_valid[requested_olid]
                            && (xbar_sel[requested_olid] == ILid));
            in_resend[ILid] = (in_valid[ILid] && !in_granted) ? 1 : 0;
            resend_pld[ILid] = in_pld[ILid];
        }
    }
// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
// 2 kinds of shuffler_cores

    // shuffler states
    #define SF_WORKING 0 // normal working state
    #define SF_ENDING 1 // flushing the remaining packets in the arbiter

    // shuffler core for read requests: works on 1 partition
    void shuffler_core_for_read_req(
        // fifos
        tapa::istreams<EDGE_PLD_T, PACK_SIZE> &input_lanes,
        tapa::ostreams<EDGE_PLD_T, PACK_SIZE> &output_lanes
    ) {
        const unsigned shuffler_extra_iters = (ARBITER_LATENCY + 1) * PACK_SIZE;
        // pipeline control variables
        ap_uint<PACK_SIZE> fetch_complete = 0;
        unsigned loop_extra_iters = shuffler_extra_iters;
        ap_uint<1> state = SF_WORKING;
        bool loop_exit = false;

        // payloads
        EDGE_PLD_T payload[PACK_SIZE];
        #pragma HLS array_partition variable=payload complete
        ap_uint<PACK_SIZE> valid = 0;

        // resend control
        EDGE_PLD_T payload_resend[PACK_SIZE];
        #pragma HLS array_partition variable=payload_resend complete
        ap_uint<1> resend[PACK_SIZE];
        #pragma HLS array_partition variable=resend complete
        for (unsigned i = 0; i < PACK_SIZE; i++) {
            #pragma HLS unroll
            resend[i] = 0;
        }

        // arbiter outputs
        unsigned xbar_sel[PACK_SIZE];
        ap_uint<PACK_SIZE> xbar_valid = 0;
        #pragma HLS array_partition variable=xbar_sel complete
        // arbiter priority rotation
        unsigned rotate_priority = 0;
        unsigned next_rotate_priority = 0;

        loop_shuffle_pipeline:
        while (!loop_exit) {
            #pragma HLS pipeline II=1
            #pragma HLS dependence variable=resend inter RAW true distance=6
            #pragma HLS dependence variable=payload_resend inter RAW true distance=6

            for (unsigned ILid = 0; ILid < PACK_SIZE; ILid++) {
                #pragma HLS unroll
                if (resend[ILid]) {
                    valid[ILid] = 1;
                    payload[ILid] = payload_resend[ILid];
                } else if (fetch_complete[ILid]) {
                    valid[ILid] = 0;
                    payload[ILid] = (EDGE_PLD_T){0,0,0,0};
                } else {
                    if (input_lanes[ILid].try_read(payload[ILid])) {
                        if (payload[ILid].inst == EOD) {
                            fetch_complete[ILid] = 1;
                            valid[ILid] = 0;
                        } else {
                            valid[ILid] = 1;
                        }
                    } else {
                        valid[ILid] = 0;
                        payload[ILid] = (EDGE_PLD_T){0,0,0,0};
                    }
                }
            }

            switch (state) {
            case SF_WORKING:
                if (fetch_complete.and_reduce()) {
                    state = SF_ENDING;
                }
                break;
            case SF_ENDING:
                loop_extra_iters--;
                loop_exit = (loop_extra_iters == 0);
                break;
            default:
                break;
            }
            // ------- end of F stage

            // Arbiter stage (A) pipeline arbiter, depth = 6
            rotate_priority = next_rotate_priority;
            arbiter_for_read_req(
                payload,
                payload_resend,
                valid,
                resend,
                xbar_sel,
                xbar_valid,
                rotate_priority
            );
            next_rotate_priority = (rotate_priority + 1) % PACK_SIZE;
            // ------- end of A stage

            // crossbar stage (C)
            for (unsigned OLid = 0; OLid < PACK_SIZE; OLid++) {
                #pragma HLS unroll
                if (xbar_valid[OLid]) {
                    if (valid[xbar_sel[OLid]]) {
                        output_lanes[OLid].write(payload[xbar_sel[OLid]]);
                    }
                }

            }
            // ------- end of C stage
        } // main while() loop ends here

        for (unsigned OLid = 0; OLid < PACK_SIZE; OLid++) {
            #pragma HLS unroll
            output_lanes[OLid].write(EDGE_PLD_EOD);
        }
    }

    // shuffler core for read responses: works on 1 partition
    void shuffler_core_for_read_resp(
        // fifos
        tapa::istreams<UPDATE_PLD_T, PACK_SIZE> &input_lanes,
        tapa::ostreams<UPDATE_PLD_T, PACK_SIZE> &output_lanes
    ) {
        const unsigned shuffler_extra_iters = (ARBITER_LATENCY + 1) * PACK_SIZE;
        // pipeline control variables
        ap_uint<PACK_SIZE> fetch_complete = 0;
        unsigned loop_extra_iters = shuffler_extra_iters;
        ap_uint<1> state = SF_WORKING;
        bool loop_exit = false;

        // payloads
        UPDATE_PLD_T payload[PACK_SIZE];
        #pragma HLS array_partition variable=payload complete
        ap_uint<PACK_SIZE> valid = 0;

        // resend control
        UPDATE_PLD_T payload_resend[PACK_SIZE];
        #pragma HLS array_partition variable=payload_resend complete
        ap_uint<1> resend[PACK_SIZE];
        #pragma HLS array_partition variable=resend complete
        for (unsigned i = 0; i < PACK_SIZE; i++) {
            #pragma HLS unroll
            resend[i] = 0;
        }

        // arbiter outputs
        unsigned xbar_sel[PACK_SIZE];
        ap_uint<PACK_SIZE> xbar_valid = 0;
        #pragma HLS array_partition variable=xbar_sel complete
        // arbiter priority rotation
        unsigned rotate_priority = 0;
        unsigned next_rotate_priority = 0;

        loop_shuffle_pipeline:
        while (!loop_exit) {
            #pragma HLS pipeline II=1
            #pragma HLS dependence variable=resend inter RAW true distance=6
            #pragma HLS dependence variable=payload_resend inter RAW true distance=6

            for (unsigned ILid = 0; ILid < PACK_SIZE; ILid++) {
                #pragma HLS unroll
                if (resend[ILid]) {
                    valid[ILid] = 1;
                    payload[ILid] = payload_resend[ILid];
                } else if (fetch_complete[ILid]) {
                    valid[ILid] = 0;
                    payload[ILid] = (UPDATE_PLD_T){0,0,0,0};
                } else {
                    if (input_lanes[ILid].try_read(payload[ILid])) {
                        if (payload[ILid].inst == EOD) {
                            fetch_complete[ILid] = 1;
                            valid[ILid] = 0;
                        } else {
                            valid[ILid] = 1;
                        }
                    } else {
                        valid[ILid] = 0;
                        payload[ILid] = (UPDATE_PLD_T){0,0,0,0};
                    }
                }
            }

            switch (state) {
            case SF_WORKING:
                if (fetch_complete.and_reduce()) {
                    state = SF_ENDING;
                }
                break;
            case SF_ENDING:
                loop_extra_iters--;
                loop_exit = (loop_extra_iters == 0);
                break;
            default:
                break;
            }
            // ------- end of F stage

            // Arbiter stage (A) pipeline arbiter, depth = 6
            rotate_priority = next_rotate_priority;
            arbiter_for_read_resp(
                payload,
                payload_resend,
                valid,
                resend,
                xbar_sel,
                xbar_valid,
                rotate_priority
            );
            next_rotate_priority = (rotate_priority + 1) % PACK_SIZE;
            // ------- end of A stage

            // crossbar stage (C)
            for (unsigned OLid = 0; OLid < PACK_SIZE; OLid++) {
                #pragma HLS unroll
                if (xbar_valid[OLid]) {
                    if (valid[xbar_sel[OLid]]) {
                        output_lanes[OLid].write(payload[xbar_sel[OLid]]);
                    }
                }

            }
            // ------- end of C stage
        } // main while() loop ends here

        for (unsigned OLid = 0; OLid < PACK_SIZE; OLid++) {
            #pragma HLS unroll
            output_lanes[OLid].write(UPDATE_PLD_EOD);
        }
    }
// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
// 2 kinds of shufflers

    void shuffler_read_req(
        tapa::istreams<EDGE_PLD_T, PACK_SIZE> &input_lanes,
        tapa::ostreams<EDGE_PLD_T, PACK_SIZE> &output_lanes
    ) {
        bool first_launch = true;
        ap_uint<PACK_SIZE> got_EOS = 0;
        while (!got_EOS.and_reduce()) {
            #pragma HLS pipeline off
            ap_uint<PACK_SIZE> got_SOD = 0;

            if (first_launch) {
                loop_sync_on_SOD:
                while (!got_SOD.and_reduce()) {
                    #pragma HLS pipeline II=1
                    for (unsigned ILid = 0; ILid < PACK_SIZE; ILid++) {
                        #pragma HLS unroll
                        if (!got_SOD[ILid]) {
                            EDGE_PLD_T p;
                            if (input_lanes[ILid].try_read(p)) {
                                if (p.inst == SOD) {
                                    got_SOD[ILid] = 1;
                                }
                            }
                        }
                    }
                } // while() : sync on first SOD
                first_launch = false;
            } // first launch SOD sync

            for (unsigned OLid = 0; OLid < PACK_SIZE; OLid++) {
                #pragma HLS unroll
                output_lanes[OLid].write(EDGE_PLD_SOD);
            }

            shuffler_core_for_read_req(input_lanes, output_lanes);

            got_SOD = 0;
            loop_sync_on_SOD_EOS:
            while (!(got_SOD.and_reduce() || got_EOS.and_reduce())) {
                #pragma HLS pipeline II=1
                for (unsigned ILid = 0; ILid < PACK_SIZE; ILid++) {
                    #pragma HLS unroll
                    if (!(got_SOD[ILid] || got_EOS[ILid])) {
                        EDGE_PLD_T p;
                        if (input_lanes[ILid].try_read(p)) {
                            if (p.inst == EOS) {
                                got_EOS[ILid] = 1;
                            } else if (p.inst == SOD) {
                                got_SOD[ILid] = 1;
                            }
                        }
                    }
                }
            } // while() : EOS or SOD sync
        } // while() : EOS sync

        for (unsigned OLid = 0; OLid < PACK_SIZE; OLid++) {
            #pragma HLS unroll
            output_lanes[OLid].write(EDGE_PLD_EOS);
        }
    }

    void shuffler_read_resp(
        tapa::istreams<UPDATE_PLD_T, PACK_SIZE> &input_lanes,
        tapa::ostreams<UPDATE_PLD_T, PACK_SIZE> &output_lanes
    ) {
        bool first_launch = true;
        ap_uint<PACK_SIZE> got_EOS = 0;
        while (!got_EOS.and_reduce()) {
            #pragma HLS pipeline off
            ap_uint<PACK_SIZE> got_SOD = 0;

            if (first_launch) {
                loop_sync_on_SOD:
                while (!got_SOD.and_reduce()) {
                    #pragma HLS pipeline II=1
                    for (unsigned ILid = 0; ILid < PACK_SIZE; ILid++) {
                        #pragma HLS unroll
                        if (!got_SOD[ILid]) {
                            UPDATE_PLD_T p;
                            if (input_lanes[ILid].try_read(p)) {
                                if (p.inst == SOD) {
                                    got_SOD[ILid] = 1;
                                }
                            }
                        }
                    }
                } // while() : sync on first SOD
                first_launch = false;
            } // first launch SOD sync

            for (unsigned OLid = 0; OLid < PACK_SIZE; OLid++) {
                #pragma HLS unroll
                output_lanes[OLid].write(UPDATE_PLD_SOD);
            }

            shuffler_core_for_read_resp(input_lanes, output_lanes);

            got_SOD = 0;
            loop_sync_on_SOD_EOS:
            while (!(got_SOD.and_reduce() || got_EOS.and_reduce())) {
                #pragma HLS pipeline II=1
                for (unsigned ILid = 0; ILid < PACK_SIZE; ILid++) {
                    #pragma HLS unroll
                    if (!(got_SOD[ILid] || got_EOS[ILid])) {
                        UPDATE_PLD_T p;
                        if (input_lanes[ILid].try_read(p)) {
                            if (p.inst == EOS) {
                                got_EOS[ILid] = 1;
                            } else if (p.inst == SOD) {
                                got_SOD[ILid] = 1;
                            }
                        }
                    }
                }
            } // while() : EOS or SOD sync
        } // while() : EOS sync

        for (unsigned OLid = 0; OLid < PACK_SIZE; OLid++) {
            #pragma HLS unroll
            output_lanes[OLid].write(UPDATE_PLD_EOS);
        }
    }
// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
// process engine

    #ifdef __SYNTHESIS__
    #include "utils/x_hls_utils.h" // for reg() function
    #else
    #ifndef REG_FOR_SW_EMU
    #define REG_FOR_SW_EMU
    template<typename T>
    T reg(T in) {
        return in;
    }
    #endif
    #endif

    // data type for IFWQ
    struct IN_FLIGHT_WRITE {
        bool valid;
        IDX_T addr;
        VAL_T value;
    };

    // pe process pipeline
    void ufixed_pe_process(
        tapa::istream<UPDATE_PLD_T> &input,
        VAL_T output_buffer[OB_BANK_SIZE]
    ) {
        bool exit = false;

        // in-flight write queue for data-forwarding
        // designed for URAM latnecy=3 (RDL=3, WRL=2)
        IN_FLIGHT_WRITE ifwq[5];
        #pragma HLS array_partition variable=ifwq complete;
        ifwq[0] = (IN_FLIGHT_WRITE){false, 0, 0};
        ifwq[1] = (IN_FLIGHT_WRITE){false, 0, 0};
        ifwq[2] = (IN_FLIGHT_WRITE){false, 0, 0};
        ifwq[3] = (IN_FLIGHT_WRITE){false, 0, 0};
        ifwq[4] = (IN_FLIGHT_WRITE){false, 0, 0};

        pe_process_loop:
        while (!exit) {
            #pragma HLS pipeline II=1
            #pragma HLS dependence variable=output_buffer inter false
            #pragma HLS dependence variable=ifwq intra true
            #pragma HLS dependence variable=ifwq inter false
            bool valid = false;
            UPDATE_PLD_T pld;
            if(input.try_read(pld)) {
    #ifdef PE_LINE_TRACING
            std::cout << "  input payload: " << pld << std::endl;
    #endif
                if (pld.inst == EOD) {
                    exit = true;
                    valid = false;
                } else {
                    exit = false;
                    valid = true;
                }
            } else {
                valid = false;
            }

            IN_FLIGHT_WRITE ifwq_new_entry;
            IDX_T bank_addr = pld.row_idx / PACK_SIZE;
            VAL_T incr = pld.mat_val * pld.vec_val;
            VAL_T q = output_buffer[bank_addr];
            VAL_T q_fwd = ((bank_addr == ifwq[0].addr) && ifwq[0].valid) ? ifwq[0].value :
                        ((bank_addr == ifwq[1].addr) && ifwq[1].valid) ? ifwq[1].value :
                        ((bank_addr == ifwq[2].addr) && ifwq[2].valid) ? ifwq[2].value :
                        ((bank_addr == ifwq[3].addr) && ifwq[3].valid) ? ifwq[3].value :
                        ((bank_addr == ifwq[4].addr) && ifwq[4].valid) ? ifwq[4].value :
                        q;
            VAL_T new_q = q_fwd + incr;
            #pragma HLS bind_op variable=new_q op=add impl=dsp latency=0
            VAL_T new_q_reg = reg(new_q); // force a register after addition
            ifwq_new_entry.addr = bank_addr;
            ifwq_new_entry.value = new_q;
            ifwq_new_entry.valid = valid;

            if (valid) {
                output_buffer[bank_addr] = new_q_reg;
            }

            ifwq[4] = ifwq[3];
            ifwq[3] = ifwq[2];
            ifwq[2] = ifwq[1];
            ifwq[1] = ifwq[0];
            ifwq[0] = ifwq_new_entry;

        }
    }

    // pe output pipeline
    void ufixed_pe_output(
        tapa::ostream<VEC_PLD_T> &output,
        VAL_T output_buffer[OB_BANK_SIZE],
        const unsigned id,
        const unsigned used_buf_len
    ) {
        bool exit = false;
        unsigned dump_count = 0;
        pe_output_loop:
        for (unsigned dump_count = 0; dump_count < used_buf_len; dump_count++) {
            #pragma HLS pipeline II=1
            VAL_T q = output_buffer[dump_count];
            VEC_PLD_T out_pld;
            out_pld.val = q;
            out_pld.idx = dump_count * PACK_SIZE + id;
            out_pld.inst = 0x0;
            output.write(out_pld);
    #ifdef PE_LINE_TRACING
            std::cout << "  write output: " << VEC_PLD_EOD << std::endl;
    #endif
        }
    }

    // unsigned fixed-point pe
    void pe(
        tapa::istream<UPDATE_PLD_T> &input,
        tapa::ostream<VEC_PLD_T> &output,
        unsigned id,
        unsigned used_buf_len
    ) {
        used_buf_len = used_buf_len/PACK_SIZE;
        VAL_T output_buffer[OB_BANK_SIZE];
        #pragma HLS bind_storage variable=output_buffer type=RAM_2P impl=URAM latency=3
        // reset output buffer before doing anything
        loop_reset_ob:
        for (unsigned i = 0; i < used_buf_len; i++) {
            #pragma HLS pipeline II=1
            output_buffer[i] = 0;
        }

        // wait on the first SOD
        bool got_SOD = false;
        pe_sync_SOD:
        while (!got_SOD) {
            #pragma HLS pipeline II=1
            UPDATE_PLD_T p = input.read();
            got_SOD = (p.inst == SOD);
        }

        // start processing
        bool exit = false;
        pe_main_loop:
        while (!exit) {
            #pragma HLS pipeline off
            // this function will exit upon EOD
            ufixed_pe_process(input, output_buffer);

            // read the next payload and decide whether continue processing or exit
            bool got_valid_pld = false;
            pe_sync_SODEOS:
            while (!got_valid_pld) {
                #pragma HLS pipeline II=1
                UPDATE_PLD_T p = input.read();
                if (p.inst == SOD) {
                    got_valid_pld = true;
                    exit = false;
                } else if (p.inst == EOS) {
                    got_valid_pld = true;
                    exit = true;
                } else {
                    got_valid_pld = false;
                    exit = false;
                }
            }
        }

        // dump results
        output.write(VEC_PLD_SOD);
        ufixed_pe_output(output, output_buffer, id,  used_buf_len);
        output.write(VEC_PLD_EOD);
        output.write(VEC_PLD_EOS);
    }
// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^


// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
// vector buffer access unit (VAU)

    //----------------------------------------------------------------
    // vector buffer reader (VR)
    //----------------------------------------------------------------
    #define VR_IDLE 0
    #define VR_WORK 1

    void vecbuf_reader(
        tapa::istream<EDGE_PLD_T> &input,
        tapa::ostream<UPDATE_PLD_T> &output,
        VAL_T vector_buffer[VB_BANK_SIZE]
    ) {
        bool loop_exit = false;
        ap_uint<1> state = VR_IDLE;
    #ifdef VAU_LINE_TRACING
        unsigned vr_sw_emu_iter_cnt = 0;
    #endif
        loop_vr:
        while (!loop_exit) {
            #pragma HLS pipeline II=1
            EDGE_PLD_T pin = input.read();

    #ifdef VAU_LINE_TRACING
        std::cout << "VR: iteration: " << vr_sw_emu_iter_cnt << " VAUid: " << id << std::endl;
        std::cout << "  state: " << state << std::endl;
        std::cout << "  input payload:" << pin << std::endl;
        vr_sw_emu_iter_cnt++;
    #endif
            switch (state) {
            case VR_IDLE:
                if (pin.inst == EOS) {
                    loop_exit = true;
                    output.write(UPDATE_PLD_EOS);
    #ifdef VAU_LINE_TRACING
                    std::cout << "  output payload:" << UPDATE_PLD_EOS << std::endl;
    #endif
                } else if (pin.inst == SOD) {
                    state = VR_WORK;
                    output.write(UPDATE_PLD_SOD);
    #ifdef VAU_LINE_TRACING
                    std::cout << "  output payload:" << UPDATE_PLD_SOD << std::endl;
    #endif
                }
                break;

            case VR_WORK:
                if (pin.inst == EOD) {
                    loop_exit = true;
                    state = VR_IDLE;
                    output.write(UPDATE_PLD_EOD);
    #ifdef VAU_LINE_TRACING
                    std::cout << "  output payload:" << UPDATE_PLD_EOD << std::endl;
    #endif
                } else {
                    IDX_T abs_addr = pin.col_idx;
                    UPDATE_PLD_T pout;
                    pout.inst = 0;
                    pout.mat_val = pin.mat_val;
                    pout.row_idx = pin.row_idx;
                    pout.vec_val = vector_buffer[(abs_addr / PACK_SIZE) % VB_BANK_SIZE];
                    output.write(pout);
    #ifdef VAU_LINE_TRACING
                    std::cout << "  output payload:" << pout << std::endl;
    #endif
                }
                break;

            default:
                break;
            }

        }
    }

    //----------------------------------------------------------------
    // vector buffer writer (VW)
    //----------------------------------------------------------------
    #define VW_IDLE 0
    #define VW_WORK 1

    void vecbuf_writer(
        tapa::istream<VEC_PLD_T> &vec_input,
        VAL_T vector_buffer[VB_BANK_SIZE] // double buffering
    ) {
        bool loop_exit = false;
        ap_uint<1> state = VW_IDLE;
    #ifdef VAU_LINE_TRACING
        unsigned vw_sw_emu_iter_cnt = 0;
    #endif
        loop_vw:
        while (!loop_exit) {
            #pragma HLS pipeline II=1
            VEC_PLD_T pin = vec_input.read();
    // #ifdef VAU_LINE_TRACING
    //         std::cout << "VW: iteration: " << vr_sw_emu_iter_cnt << std::endl;
    //         std::cout << "  state: " << state << std::endl;
    //         std::cout << "  input payload:" << pin << std::endl;
    //         vw_sw_emu_iter_cnt++;
    // #endif
            switch (state) {
            case VW_IDLE:
                if (pin.inst == EOS) {
                    loop_exit = true;
                } else if (pin.inst == SOD) {
                    state = VW_WORK;
                }
                break;

            case VW_WORK:
                if (pin.inst == EOD) {
                    state = VW_IDLE;
                    loop_exit = true;
                } else {
                    IDX_T abs_addr = pin.idx;
                    VAL_T vec_val = pin.val;
                    vector_buffer[(abs_addr / PACK_SIZE) % VB_BANK_SIZE] = vec_val;
                }
                break;

            default:
                break;
            }
        }
    }

    //----------------------------------------------------------------
    // vector buffer access unit (VAU)
    //----------------------------------------------------------------

    // 1 VAU pipeline
    // the col_idx in the input is absolute index
    // bank_addr = (abs_col_idx / pack_size) % bank_size
    void vecbuf_access_unit(
        tapa::istream<EDGE_PLD_T> &input,
        tapa::istream<VEC_PLD_T> &vec_input,
        tapa::ostream<UPDATE_PLD_T> &output,
        unsigned num_partitions
    ) {
        // double-buffered by dataflow in loop
        VAL_T vector_buffer[VB_BANK_SIZE];
        #pragma HLS stream off variable=vector_buffer
        #pragma HLS bind_storage variable=vector_buffer type=RAM_2P impl=URAM

        // +1 due to consuming the last EOS
        for (unsigned i = 0; i < num_partitions + 1; i++) {
            #pragma HLS dataflow
            vecbuf_writer(vec_input, vector_buffer);
            vecbuf_reader(input, output, vector_buffer);
        }
    }
// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^


// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
// one computational cluster
    void spmv_cluster(
        tapa::mmap<SPMV_MAT_PKT_T> matrix_hbm,       // in
        tapa::istream<VEC_AXIS_T> &vec_in,        // in
        tapa::ostream<VEC_AXIS_T> &res_out,       // out
        unsigned num_partitions,                  // in
        unsigned num_col_partitions,               // in
        unsigned row_partition_idx,          // in
        unsigned rows_per_c_in_partition
    ) {
        tapa::streams<EDGE_PLD_T, PACK_SIZE, FIFO_DEPTH> ML2SF;
        tapa::streams<EDGE_PLD_T, PACK_SIZE, FIFO_DEPTH> SF2VAU;
        tapa::streams<UPDATE_PLD_T, PACK_SIZE, FIFO_DEPTH> VAU2SF;
        tapa::streams<UPDATE_PLD_T, PACK_SIZE, FIFO_DEPTH> SF2PE;
        tapa::streams<VEC_PLD_T, PACK_SIZE, FIFO_DEPTH> PE2PK;
        tapa::streams<VEC_PLD_T, PACK_SIZE, FIFO_DEPTH> UPK2VAU;

        // unsigned rows_per_ch_in_last_row_part;
        // if (num_rows % LOGICAL_OB_SIZE == 0) {
        //     rows_per_ch_in_last_row_part = LOGICAL_OB_SIZE / NUM_HBM_CHANNELS;
        // } else {
        //     rows_per_ch_in_last_row_part = num_rows % LOGICAL_OB_SIZE / NUM_HBM_CHANNELS;
        // }

        // unsigned num_row_partitions = (num_rows + LOGICAL_OB_SIZE - 1) / LOGICAL_OB_SIZE;
        // unsigned num_col_partitions = (num_columns + LOGICAL_VB_SIZE - 1) / LOGICAL_VB_SIZE;
        // unsigned num_partitions = num_row_partitions * num_col_partitions;


        // // number of rows per cluster in this row partition
        // unsigned rows_per_c_in_partition = LOGICAL_OB_SIZE / NUM_HBM_CHANNELS;
        // if (row_partition_idx == num_row_partitions - 1) {
        //     rows_per_c_in_partition = rows_per_ch_in_last_row_part;
        // }

        tapa::task()
        .invoke(spmv_vector_unpacker,
            vec_in,
            UPK2VAU
        )
        .invoke(CPSR_matrix_loader,
            matrix_hbm,
            row_partition_idx,
            num_col_partitions,
            num_partitions,
            ML2SF
        )
        .invoke(shuffler_read_req,
            ML2SF,
            SF2VAU
        )
        .invoke(vecbuf_access_unit,
            SF2VAU[0],
            UPK2VAU[0],
            VAU2SF[0],
            num_col_partitions
        )
        .invoke(vecbuf_access_unit,
            SF2VAU[1],
            UPK2VAU[1],
            VAU2SF[1],
            num_col_partitions
        )
        .invoke(vecbuf_access_unit,
            SF2VAU[2],
            UPK2VAU[2],
            VAU2SF[2],
            num_col_partitions
        )
        .invoke(vecbuf_access_unit,
            SF2VAU[3],
            UPK2VAU[3],
            VAU2SF[3],
            num_col_partitions
        )
        .invoke(vecbuf_access_unit,
            SF2VAU[4],
            UPK2VAU[4],
            VAU2SF[4],
            num_col_partitions
        )
        .invoke(vecbuf_access_unit,
            SF2VAU[5],
            UPK2VAU[5],
            VAU2SF[5],
            num_col_partitions
        )
        .invoke(vecbuf_access_unit,
            SF2VAU[6],
            UPK2VAU[6],
            VAU2SF[6],
            num_col_partitions
        )
        .invoke(vecbuf_access_unit,
            SF2VAU[7],
            UPK2VAU[7],
            VAU2SF[7],
            num_col_partitions
        )
        .invoke(shuffler_read_resp,
            VAU2SF,
            SF2PE
        )
        .invoke(pe,
            SF2PE[0],
            PE2PK[0],
            0,
            rows_per_c_in_partition
        )
        .invoke(pe,
            SF2PE[1],
            PE2PK[1],
            1,
            rows_per_c_in_partition
        )
        .invoke(pe,
            SF2PE[2],
            PE2PK[2],
            2,
            rows_per_c_in_partition
        )
        .invoke(pe,
            SF2PE[3],
            PE2PK[3],
            3,
            rows_per_c_in_partition
        )
        .invoke(pe,
            SF2PE[4],
            PE2PK[4],
            4,
            rows_per_c_in_partition
        )
        .invoke(pe,
            SF2PE[5],
            PE2PK[5],
            5,
            rows_per_c_in_partition
        )
        .invoke(pe,
            SF2PE[6],
            PE2PK[6],
            6,
            rows_per_c_in_partition
        )
        .invoke(pe,
            SF2PE[7],
            PE2PK[7],
            7,
            rows_per_c_in_partition
        )
        .invoke(spmv_result_packer,
            PE2PK,
            res_out
        );

    }

// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^





// vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvv
// vector loader and result draining unit

    void vector_loader(
        tapa::mmap<PACKED_VAL_T> packed_dense_vector,              // in
        const unsigned num_cols,                              // in
        tapa::ostreams<VEC_AXIS_T, 16> &duplicate                 // out
    ) {
        unsigned num_col_partitions = (num_cols + LOGICAL_VB_SIZE - 1) / LOGICAL_VB_SIZE;
        unsigned num_col_last_partition;
        if (num_cols % LOGICAL_VB_SIZE == 0) {
            num_col_last_partition = LOGICAL_VB_SIZE;
        } else {
            num_col_last_partition = num_cols % LOGICAL_VB_SIZE;
        }

        vector_loader_over_col_partitions:
        for (unsigned part_id = 0; part_id < num_col_partitions; part_id++)  {
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
                #pragma HLS pipeline II=1
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

    void result_drain(
        tapa::mmap<PACKED_VAL_T> packed_dense_result,      // out
        const unsigned row_part_id,             // in
        tapa::istreams<VEC_AXIS_T, 16> &from_clusters     // in
    ) {
        // write back
        char current_input = 0;
        ap_uint<16> finished = 0;
        unsigned write_counter = 0;
        bool exit = false;
        unsigned pkt_idx_offset = row_part_id * LOGICAL_OB_SIZE / PACK_SIZE;
        result_drain_main_loop:
        while (!exit) {
            #pragma HLS pipeline II=1
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

        } // while
    }
// ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^


// top-level kernel function
void spmv(
    tapa::mmap<SPMV_MAT_PKT_T> matrix_hbm_0,       // in
    tapa::mmap<SPMV_MAT_PKT_T> matrix_hbm_1,       // in
    tapa::mmap<SPMV_MAT_PKT_T> matrix_hbm_2,       // in
    tapa::mmap<SPMV_MAT_PKT_T> matrix_hbm_3,       // in
    tapa::mmap<SPMV_MAT_PKT_T> matrix_hbm_4,       // in
    tapa::mmap<SPMV_MAT_PKT_T> matrix_hbm_5,       // in
    tapa::mmap<SPMV_MAT_PKT_T> matrix_hbm_6,       // in
    tapa::mmap<SPMV_MAT_PKT_T> matrix_hbm_7,       // in
    tapa::mmap<SPMV_MAT_PKT_T> matrix_hbm_8,       // in
    tapa::mmap<SPMV_MAT_PKT_T> matrix_hbm_9,       // in
    tapa::mmap<SPMV_MAT_PKT_T> matrix_hbm_10,      // in
    tapa::mmap<SPMV_MAT_PKT_T> matrix_hbm_11,      // in
    tapa::mmap<SPMV_MAT_PKT_T> matrix_hbm_12,      // in
    tapa::mmap<SPMV_MAT_PKT_T> matrix_hbm_13,      // in
    tapa::mmap<SPMV_MAT_PKT_T> matrix_hbm_14,      // in
    tapa::mmap<SPMV_MAT_PKT_T> matrix_hbm_15,      // in
    tapa::mmap<PACKED_VAL_T> packed_dense_vector,  // in
    tapa::mmap<PACKED_VAL_T> packed_dense_result,        // out
    unsigned num_columns,
    unsigned num_partitions,                  // in
    unsigned num_col_partitions,               // in
    unsigned row_partition_idx,          // in
    unsigned rows_per_c_in_partition
) {
    tapa::streams<VEC_AXIS_T, 16, FIFO_DEPTH> vec_dup;
    tapa::streams<VEC_AXIS_T, 16, FIFO_DEPTH> res;

    tapa::task()
    .invoke(vector_loader, packed_dense_vector, num_columns, vec_dup)
    .invoke(spmv_cluster,
        matrix_hbm_0,
        vec_dup[0],
        res[0],
        num_partitions,                  // in
        num_col_partitions,               // in
        row_partition_idx,          // in
        rows_per_c_in_partition)
    .invoke(spmv_cluster,
        matrix_hbm_1,
        vec_dup[1],
        res[1],
        num_partitions,                  // in
        num_col_partitions,               // in
        row_partition_idx,          // in
        rows_per_c_in_partition)
    .invoke(spmv_cluster,
        matrix_hbm_2,
        vec_dup[2],
        res[2],
        num_partitions,                  // in
        num_col_partitions,               // in
        row_partition_idx,          // in
        rows_per_c_in_partition)
    .invoke(spmv_cluster,
        matrix_hbm_3,
        vec_dup[3],
        res[3],
        num_partitions,                  // in
        num_col_partitions,               // in
        row_partition_idx,          // in
        rows_per_c_in_partition)
    .invoke(spmv_cluster,
        matrix_hbm_4,
        vec_dup[4],
        res[4],
        num_partitions,                  // in
        num_col_partitions,               // in
        row_partition_idx,          // in
        rows_per_c_in_partition)
    .invoke(spmv_cluster,
        matrix_hbm_5,
        vec_dup[5],
        res[5],
        num_partitions,                  // in
        num_col_partitions,               // in
        row_partition_idx,          // in
        rows_per_c_in_partition)
    .invoke(spmv_cluster,
        matrix_hbm_6,
        vec_dup[6],
        res[6],
        num_partitions,                  // in
        num_col_partitions,               // in
        row_partition_idx,          // in
        rows_per_c_in_partition)
    .invoke(spmv_cluster,
        matrix_hbm_7,
        vec_dup[7],
        res[7],
        num_partitions,                  // in
        num_col_partitions,               // in
        row_partition_idx,          // in
        rows_per_c_in_partition)
    .invoke(spmv_cluster,
        matrix_hbm_8,
        vec_dup[8],
        res[8],
        num_partitions,                  // in
        num_col_partitions,               // in
        row_partition_idx,          // in
        rows_per_c_in_partition)
    .invoke(spmv_cluster,
        matrix_hbm_9,
        vec_dup[9],
        res[9],
        num_partitions,                  // in
        num_col_partitions,               // in
        row_partition_idx,          // in
        rows_per_c_in_partition)
    .invoke(spmv_cluster,
        matrix_hbm_10,
        vec_dup[10],
        res[10],
        num_partitions,                  // in
        num_col_partitions,               // in
        row_partition_idx,          // in
        rows_per_c_in_partition)
    .invoke(spmv_cluster,
        matrix_hbm_11,
        vec_dup[11],
        res[11],
        num_partitions,                  // in
        num_col_partitions,               // in
        row_partition_idx,          // in
        rows_per_c_in_partition)
    .invoke(spmv_cluster,
        matrix_hbm_12,
        vec_dup[12],
        res[12],
        num_partitions,                  // in
        num_col_partitions,               // in
        row_partition_idx,          // in
        rows_per_c_in_partition)
    .invoke(spmv_cluster,
        matrix_hbm_13,
        vec_dup[13],
        res[13],
        num_partitions,                  // in
        num_col_partitions,               // in
        row_partition_idx,          // in
        rows_per_c_in_partition)
    .invoke(spmv_cluster,
        matrix_hbm_14,
        vec_dup[14],
        res[14],
        num_partitions,                  // in
        num_col_partitions,               // in
        row_partition_idx,          // in
        rows_per_c_in_partition)
    .invoke(spmv_cluster,
        matrix_hbm_15,
        vec_dup[15],
        res[15],
        num_partitions,                  // in
        num_col_partitions,               // in
        row_partition_idx,          // in
        rows_per_c_in_partition)
    .invoke(result_drain, packed_dense_result, row_partition_idx, res)
    ;

} // kernel
