#ifndef SPMV_VECBUF_ACCESS_UNIT_H_
#define SPMV_VECBUF_ACCESS_UNIT_H_

#include "common.h"
#include <hls_stream.h>
#include <ap_int.h>

#ifndef __SYNTHESIS__
// #define VAU_LINE_TRACING
#endif

//----------------------------------------------------------------
// vector buffer reader (VR)
//----------------------------------------------------------------
#define VR_IDLE 0
#define VR_WORK 1

template<int id, unsigned bank_size, unsigned pack_size>
void vecbuf_reader(
    hls::stream<EDGE_PLD_T> &input,
    hls::stream<UPDATE_PLD_T> &output,
    VAL_T vector_buffer[bank_size]
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
                pout.vec_val = vector_buffer[(abs_addr / pack_size) % bank_size];
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

template<int id, unsigned bank_size, unsigned pack_size>
void vecbuf_writer(
    hls::stream<VEC_PLD_T> &vec_input,
    VAL_T vector_buffer[bank_size] // double buffering
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
                vector_buffer[(abs_addr / pack_size) % bank_size] = vec_val;
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
template<int id, unsigned bank_size, unsigned pack_size>
void vecbuf_access_unit(
    hls::stream<EDGE_PLD_T> &input,
    hls::stream<VEC_PLD_T> &vec_input,
    hls::stream<UPDATE_PLD_T> &output,
    unsigned num_partitions
) {
    VAL_T vector_buffer[bank_size];
    #pragma HLS stream off variable=vector_buffer
    #pragma HLS bind_storage variable=vector_buffer type=RAM_2P impl=URAM

    // +1 due to consuming the last EOS
    for (unsigned i = 0; i < num_partitions + 1; i++) {
        #pragma HLS dataflow
        vecbuf_writer<id, bank_size, pack_size>(vec_input, vector_buffer);
        vecbuf_reader<id, bank_size, pack_size>(input, output, vector_buffer);
    }
}

#endif  // SPMV_VECBUF_ACCESS_UNIT_H_
