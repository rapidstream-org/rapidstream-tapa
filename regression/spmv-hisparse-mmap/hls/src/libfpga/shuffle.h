#ifndef SPMV_SHUFFLE_H_
#define SPMV_SHUFFLE_H_

#include <hls_stream.h>
#include <ap_int.h>
#include "shuffle.h"
#include "common.h"

#ifndef __SYNTHESIS__
#include <iostream>
#include <iomanip>
// bool line_tracing_shuffle = false;
// bool line_tracing_shuffle_core = false;
// bool csim_abort_shuffle_core = false;
// unsigned max_iter_limit_shuffle_core = 100;
unsigned long long iter_cnt = 0;
#endif

const unsigned ARBITER_LATENCY = 5;
//------------------------------------------------------------
// arbiters (2 overloads)
//------------------------------------------------------------
// arbiter for EDGE_PLD_T (depends on col_idx)
template<unsigned num_lanes>
void arbiter_1p(
    const EDGE_PLD_T in_pld[num_lanes],
    EDGE_PLD_T resend_pld[num_lanes],
    const ap_uint<num_lanes> in_valid,
    ap_uint<1> in_resend[num_lanes],
    unsigned xbar_sel[num_lanes],
    ap_uint<num_lanes> &out_valid,
    const unsigned rotate_priority
) {
    #pragma HLS pipeline II=1 enable_flush
    #pragma HLS latency min=ARBITER_LATENCY max=ARBITER_LATENCY

    #pragma HLS array_partition variable=in_addr complete
    #pragma HLS array_partition variable=xbar_sel complete

    // prioritized valid and addr
    ap_uint<num_lanes> arb_p_in_valid = in_valid;
    IDX_T arb_p_in_addr[num_lanes];
    IDX_T in_addr[num_lanes];
    #pragma HLS array_partition variable=in_addr complete
    #pragma HLS array_partition variable=arb_p_in_addr complete

    for (unsigned i = 0; i < num_lanes; i++) {
        #pragma HLS unroll
        arb_p_in_addr[i] = in_pld[(i + rotate_priority) % num_lanes].col_idx;
        in_addr[i] = in_pld[i].col_idx;
    }

    // array_rotate_left<IDX_T, num_lanes>(in_addr, arb_p_in_addr, rotate_priority);
    arb_p_in_valid.rrotate(rotate_priority);

    loop_A_arbsearch:
    for (unsigned OLid = 0; OLid < num_lanes; OLid++) {
        #pragma HLS unroll
        bool found = false;
        unsigned chosen_port = 0;

        loop_ab_logic_encoder_unroll:
        for (unsigned ILid_plus_1 = num_lanes; ILid_plus_1 > 0; ILid_plus_1--) {
            #pragma HLS unroll
            if (arb_p_in_valid[ILid_plus_1 - 1] && ((arb_p_in_addr[ILid_plus_1 - 1] % num_lanes) == OLid)) {
                chosen_port = ILid_plus_1 - 1;
                found = true;
            }
        }
        if (!found) {
            out_valid[OLid] = 0;
            xbar_sel[OLid] = 0;
        } else {
            out_valid[OLid] = 1;
            xbar_sel[OLid] = (chosen_port + rotate_priority) % num_lanes;
        }
    }

    // array_cyclic_add<unsigned, num_lanes, num_lanes>(xbar_sel, out_valid, rotate_priority);

    loop_A_grant:
    for (unsigned ILid = 0; ILid < num_lanes; ILid++) {
        #pragma HLS unroll
        unsigned requested_olid = in_addr[ILid] % num_lanes;
        bool in_granted = (in_valid[ILid]
                           && out_valid[requested_olid]
                           && (xbar_sel[requested_olid] == ILid));
        in_resend[ILid] = (in_valid[ILid] && !in_granted) ? 1 : 0;
        resend_pld[ILid] = in_pld[ILid];
    }
// #ifndef __SYNTHESIS__
//     std::cout << "  Arbiter: "
//               << "RP: " << rotate_priority << ", "
//               << "in_valid: " << std::hex << (int)in_valid << std::dec << ", "
//               << "in_p_valid: " << std::hex << (int)arb_p_in_valid << std::dec << ", "
//               << "out_valid: " << std::hex << (int)out_valid << std::dec << ", "
//               << "in_resend: " << std::hex << (int)in_resend << std::dec << std::endl;
// #endif
}

// arbiter for UPDATE_PLD_T (depends on row_idx)
template<unsigned num_lanes>
void arbiter_1p(
    const UPDATE_PLD_T in_pld[num_lanes],
    UPDATE_PLD_T resend_pld[num_lanes],
    const ap_uint<num_lanes> in_valid,
    ap_uint<1> in_resend[num_lanes],
    unsigned xbar_sel[num_lanes],
    ap_uint<num_lanes> &out_valid,
    const unsigned rotate_priority
) {
    #pragma HLS pipeline II=1 enable_flush
    #pragma HLS latency min=ARBITER_LATENCY max=ARBITER_LATENCY

    #pragma HLS array_partition variable=in_addr complete
    #pragma HLS array_partition variable=xbar_sel complete

    // prioritized valid and addr
    ap_uint<num_lanes> arb_p_in_valid = in_valid;
    IDX_T arb_p_in_addr[num_lanes];
    IDX_T in_addr[num_lanes];
    #pragma HLS array_partition variable=in_addr complete
    #pragma HLS array_partition variable=arb_p_in_addr complete

    for (unsigned i = 0; i < num_lanes; i++) {
        #pragma HLS unroll
        arb_p_in_addr[i] = in_pld[(i + rotate_priority) % num_lanes].row_idx;
        in_addr[i] = in_pld[i].row_idx;
    }

    // array_rotate_left<IDX_T, num_lanes>(in_addr, arb_p_in_addr, rotate_priority);
    arb_p_in_valid.rrotate(rotate_priority);

    loop_A_arbsearch:
    for (unsigned OLid = 0; OLid < num_lanes; OLid++) {
        #pragma HLS unroll
        bool found = false;
        unsigned chosen_port = 0;

        loop_ab_logic_encoder_unroll:
        for (unsigned ILid_plus_1 = num_lanes; ILid_plus_1 > 0; ILid_plus_1--) {
            #pragma HLS unroll
            if (arb_p_in_valid[ILid_plus_1 - 1] && ((arb_p_in_addr[ILid_plus_1 - 1] % num_lanes) == OLid)) {
                chosen_port = ILid_plus_1 - 1;
                found = true;
            }
        }
        if (!found) {
            out_valid[OLid] = 0;
            xbar_sel[OLid] = 0;
        } else {
            out_valid[OLid] = 1;
            xbar_sel[OLid] = (chosen_port + rotate_priority) % num_lanes;
        }
    }

    // array_cyclic_add<unsigned, num_lanes, num_lanes>(xbar_sel, out_valid, rotate_priority);

    loop_A_grant:
    for (unsigned ILid = 0; ILid < num_lanes; ILid++) {
        #pragma HLS unroll
        unsigned requested_olid = in_addr[ILid] % num_lanes;
        bool in_granted = (in_valid[ILid]
                           && out_valid[requested_olid]
                           && (xbar_sel[requested_olid] == ILid));
        in_resend[ILid] = (in_valid[ILid] && !in_granted) ? 1 : 0;
        resend_pld[ILid] = in_pld[ILid];
    }
// #ifndef __SYNTHESIS__
//     std::cout << "  Arbiter: "
//               << "RP: " << rotate_priority << ", "
//               << "in_valid: " << std::hex << (int)in_valid << std::dec << ", "
//               << "in_p_valid: " << std::hex << (int)arb_p_in_valid << std::dec << ", "
//               << "out_valid: " << std::hex << (int)out_valid << std::dec << ", "
//               << "in_resend: " << std::hex << (int)in_resend << std::dec << std::endl;
// #endif
}

// shuffler states
#define SF_WORKING 0 // normal working state
#define SF_ENDING 1 // flushing the remaining packets in the arbiter

// shuffler core: works on 1 partition
template<typename PayloadT, unsigned num_lanes>
void shuffler_core(
    // fifos
    hls::stream<PayloadT> input_lanes[num_lanes],
    hls::stream<PayloadT> output_lanes[num_lanes]
) {
    const unsigned shuffler_extra_iters = (ARBITER_LATENCY + 1) * num_lanes;
    // pipeline control variables
    ap_uint<num_lanes> fetch_complete = 0;
    unsigned loop_extra_iters = shuffler_extra_iters;
    ap_uint<1> state = SF_WORKING;
    bool loop_exit = false;

    // payloads
    PayloadT payload[num_lanes];
    // IDX_T in_addr[num_lanes];
    #pragma HLS array_partition variable=payload complete
    // #pragma HLS array_partition variable=in_addr complete
    ap_uint<num_lanes> valid = 0;

    // resend control
    PayloadT payload_resend[num_lanes];
    #pragma HLS array_partition variable=payload_resend complete
    ap_uint<1> resend[num_lanes];
    #pragma HLS array_partition variable=resend complete
    for (unsigned i = 0; i < num_lanes; i++) {
        #pragma HLS unroll
        resend[i] = 0;
    }

    // arbiter outputs
    unsigned xbar_sel[num_lanes];
    ap_uint<num_lanes> xbar_valid = 0;
    #pragma HLS array_partition variable=xbar_sel complete
    // arbiter priority rotation
    unsigned rotate_priority = 0;
    unsigned next_rotate_priority = 0;

#ifndef __SYNTHESIS__
    int iteration_cnt = 0;
#endif

    loop_shuffle_pipeline:
    while (!loop_exit) {
        #pragma HLS pipeline II=1
        #pragma HLS dependence variable=resend inter RAW true distance=6
        #pragma HLS dependence variable=payload_resend inter RAW true distance=6
        // #pragma HLS dependence variable=in_addr inter RAW true distance=6

        // Fetch stage (F)
// #ifndef __SYNTHESIS__
//         if (line_tracing_shuffle_core) {
//             if (loop_extra_iters == shuffler_extra_iters) {
//                 std::cout << "Iteration: " << iteration_cnt << ", "
//                         << "State: " << (int)state << ", "
//                         << "loop_extra_iters:" << loop_extra_iters << std::endl;
//             } else if (loop_extra_iters == 1) {
//                 std::cout << "Iteration: " << iteration_cnt << ", "
//                         << "State: " << (int)state << ", "
//                         << "loop_extra_iters:" << loop_extra_iters << std::endl;
//             } else {
//                 std::cout << "." << std::flush;
//             }
//         }
// #endif
        for (unsigned ILid = 0; ILid < num_lanes; ILid++) {
            #pragma HLS unroll
            if (resend[ILid]) {
                valid[ILid] = 1;
                payload[ILid] = payload_resend[ILid];
            } else if (fetch_complete[ILid]) {
                valid[ILid] = 0;
                payload[ILid] = (PayloadT){0,0,0,0};
            } else {
                if (input_lanes[ILid].read_nb(payload[ILid])) {
                    if (payload[ILid].inst == EOD) {
                        fetch_complete[ILid] = 1;
                        valid[ILid] = 0;
                    } else {
                        valid[ILid] = 1;
                    }
                } else {
                    valid[ILid] = 0;
                    payload[ILid] = (PayloadT){0,0,0,0};
                }
            }
// #ifndef __SYNTHESIS__
//             if (line_tracing_shuffle_core) {
//                 if (state == SF_WORKING) {
//                     std::cout << "  Shuffle core: ILane " << ILid << ", "
//                             << "resend: " << (resend[ILid] ? "x" : ".") << ", "
//                             << "payload: " << payload[ILid] << ", "
//                             << "valid:" << (valid[ILid] ? "x" : ".") << ", "
//                             << "fetch complete: " << (fetch_complete[ILid] ? "x" : ".") << std::endl;
//                 }
//             }
// #endif
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
        arbiter_1p<num_lanes>(
            payload,
            payload_resend,
            valid,
            resend,
            xbar_sel,
            xbar_valid,
            rotate_priority
        );
        next_rotate_priority = (rotate_priority + 1) % num_lanes;
        // ------- end of A stage

        // crossbar stage (C)
        for (unsigned OLid = 0; OLid < num_lanes; OLid++) {
            #pragma HLS unroll
            if (xbar_valid[OLid]) {
                if (valid[xbar_sel[OLid]]) {
                    output_lanes[OLid].write(payload[xbar_sel[OLid]]);
                }
            }
// #ifndef __SYNTHESIS__
//             if (line_tracing_shuffle_core) {
//                 if (state == SF_WORKING) {
//                     std::cout << "  Shuffle core: OLane " << OLid << ", "
//                             << "sel: " << xbar_sel[OLid] << ", "
//                             << "payload: " << payload[xbar_sel[OLid]] << ", "
//                             << "p-valid:" << (valid[xbar_sel[OLid]] ? "x" : ".") << ", "
//                             << "x-valid: " << (xbar_valid[OLid] ? "x" : ".") << std::endl;
//                 }
//             }
// #endif
        }
        // ------- end of C stage

        // line tracing for debug
// #ifndef __SYNTHESIS__
//         iteration_cnt++;
//         if (csim_abort_shuffle_core && iteration_cnt > max_iter_limit_shuffle_core) {
//             std::cout << "WARNING: shuffle core exceeds the sw_emu iteration limit!" << std::endl;
//             return;
//         }
// #endif
    } // main while() loop ends here

    for (unsigned OLid = 0; OLid < num_lanes; OLid++) {
        #pragma HLS unroll
        output_lanes[OLid].write((PayloadT){0,0,0,EOD});
    }

}

// shuffler: works on EDGE_PLD_T and UPDATE_PLD_T
template<typename PayloadT, unsigned num_lanes>
void shuffler(
    hls::stream<PayloadT> input_lanes[num_lanes],
    hls::stream<PayloadT> output_lanes[num_lanes]
) {
    bool first_launch = true;
    ap_uint<num_lanes> got_EOS = 0;
    while (!got_EOS.and_reduce()) {
        #pragma HLS pipeline off
        ap_uint<num_lanes> got_SOD = 0;

        if (first_launch) {
            loop_sync_on_SOD:
            while (!got_SOD.and_reduce()) {
                #pragma HLS pipeline II=1
                for (unsigned ILid = 0; ILid < num_lanes; ILid++) {
                    #pragma HLS unroll
                    if (!got_SOD[ILid]) {
                        PayloadT p;
                        if (input_lanes[ILid].read_nb(p)) {
// #ifndef __SYNTHESIS__
//                             if (line_tracing_shuffle) {
//                                 std::cout << "Shuffle: Lane " << ILid << " got payload:" << p << std::endl;
//                             }
// #endif
                            if (p.inst == SOD) {
                                got_SOD[ILid] = 1;
                            }
                        }
                    }
                }
            } // while() : sync on first SOD
            first_launch = false;
// #ifndef __SYNTHESIS__
//             if (line_tracing_shuffle) {
//                 std::cout << "Shuffle: successfully synced on first SOD" << std::endl;
//             }
// #endif
        } // first launch SOD sync

        for (unsigned OLid = 0; OLid < num_lanes; OLid++) {
            #pragma HLS unroll
            output_lanes[OLid].write((PayloadT){0,0,0,SOD});
        }

        shuffler_core<PayloadT, num_lanes>(input_lanes, output_lanes);
// #ifndef __SYNTHESIS__
//         if (line_tracing_shuffle) {
//             std::cout << "Shuffle: shuffler_core complete" << std::endl;
//         }
// #endif

        got_SOD = 0;
        loop_sync_on_SOD_EOS:
        while (!(got_SOD.and_reduce() || got_EOS.and_reduce())) {
            #pragma HLS pipeline II=1
            for (unsigned ILid = 0; ILid < num_lanes; ILid++) {
                #pragma HLS unroll
                if (!(got_SOD[ILid] || got_EOS[ILid])) {
                    PayloadT p;
                    if (input_lanes[ILid].read_nb(p)) {
                        if (p.inst == EOS) {
                            got_EOS[ILid] = 1;
                        } else if (p.inst == SOD) {
                            got_SOD[ILid] = 1;
                        }
                    }
                }
            }
// #ifndef __SYNTHESIS__
//             if (line_tracing_shuffle) {
//                 if (got_SOD.and_reduce()) {
//                     std::cout << "Shuffle: successfully synced on SOD" << std::endl;
//                 }
//                 if (got_EOS.and_reduce()) {
//                     std::cout << "Shuffle: successfully synced on EOS" << std::endl;
//                 }
//             }
// #endif
        } // while() : EOS or SOD sync


    } // while() : EOS sync

    for (unsigned OLid = 0; OLid < num_lanes; OLid++) {
        #pragma HLS unroll
        output_lanes[OLid].write((PayloadT){0,0,0,EOS});
    }
}


#endif  // SPMV_SHUFFLE_H_
