#ifndef SPMV_STREAM_UTILS_H_
#define SPMV_STREAM_UTILS_H_

#include <hls_stream.h>
#include "common.h"

// duplicate 1 AXIS stream to N
template<unsigned N>
void axis_duplicate(
    hls::stream<VEC_AXIS_T> &in,
    hls::stream<VEC_AXIS_T> out[N]
) {
    bool exit = false;
    while (!exit) {
        #pragma HLS pipeline II=1
        VEC_AXIS_T pkt = in.read();
        for (unsigned k = 0; k < N; k++) {
            #pragma HLS unroll
            out[k].write(pkt);
        }
        exit = (pkt.user == EOS);
    }
}


#ifndef __SYNTHESIS__
// #define AXIS_MERGE_LINE_TRACING
#endif

// merge N AXIS streams into 1
// in[0] -> in[N-1], cyclic merging
template<unsigned N>
void axis_merge(
    hls::stream<VEC_AXIS_T> in[N],
    hls::stream<VEC_AXIS_T> &out
) {
    unsigned i = 0;
    unsigned c = 0;
    ap_uint<N> got_EOS = 0;
    bool exit = false;
    while (!exit) {
        #pragma HLS pipeline II=1
        if (!got_EOS[i]) {
            VEC_AXIS_T pkt = in[i].read();
            VEC_AXIS_PKT_IDX(pkt) = c;
            if (pkt.user != EOS) {
                out.write(pkt);
#ifdef AXIS_MERGE_LINE_TRACING
            std::cout << "axis merge write output from input " << i << std::endl
                        << "  " << pkt << std::endl;
#endif
            } else {
                got_EOS[i] = 1;
            }
            if (pkt.user != SOD && pkt.user != EOD && pkt.user != EOS) {
                c++;
            }
        }
        i = (i + 1) % N;
        exit = got_EOS.and_reduce();
    }


    VEC_AXIS_T eos;
    for (unsigned k = 0; k < PACK_SIZE; k++) {
        #pragma HLS unroll
        VEC_AXIS_VAL(eos, k) = 0;
    }
    VEC_AXIS_PKT_IDX(eos) = 0;
    eos.user = EOS;
    out.write(eos);
}



#endif  // SPMV_STREAM_UTILS_H_
