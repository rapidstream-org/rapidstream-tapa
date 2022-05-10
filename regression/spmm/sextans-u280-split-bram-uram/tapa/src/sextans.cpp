#include <ap_int.h>
#include <cstdio>
#include <cstring>
#include <cassert>

#include <tapa.h>

#include "sextans.h"
//#include "modules.h"

constexpr int FIFO_DEPTH = 2;
constexpr int PEG_PER_A = 512 / 256;

struct MultBVec {
    ap_uint<18> row;
    float_v8 abvec;
};

template <typename T, typename R>
void async_read(tapa::async_mmap<T> & A,
                tapa::ostream<T> & fifo_A,
                const R A_len,
                R & i_req,
                R & i_resp) {
#pragma HLS inline
    if (i_req < A_len &&
        i_req < i_resp + 64 &&
        A.read_addr.try_write(i_req)) {
        ++i_req;
    }
    if (!fifo_A.full() && !A.read_data.empty()) {
        fifo_A.try_write(A.read_data.read(nullptr));
        ++i_resp;
    }
}

void read_edge_list_ptr(const int num_ite,
                        const int M,
                        const int P_N, // bit 31 - 16: repeat time, bit 15 - 0: N
                        const int K,
                        tapa::async_mmap<int> & edge_list_ptr,
                        tapa::ostream<int> & fifo_edge_list_ptr,
                        tapa::ostream<int> & PE_inst
                        ) {
    PE_inst.write(num_ite);
    PE_inst.write(M);
    PE_inst.write(P_N);
    PE_inst.write(K);

    const int N = P_N & 0xFFFF;
    const int N16 = P_N >> 16;
    const int rp_time = (N16 == 0)? 1 : N16;

    const int num_ite_plus1 = num_ite + 1;
    const int rp_time_N = rp_time * ((N + 7) >> 3);

l_rp:
    for(int rp = 0; rp < rp_time_N; rp++) {
#pragma HLS loop_flatten off
#pragma HLS loop_tripcount min=1 max=16
    rd_ptr:
        for (int i_req = 0, i_resp = 0; i_resp < num_ite_plus1;) {
#pragma HLS loop_tripcount min=1 max=800
#pragma HLS pipeline II=1
            async_read(edge_list_ptr,
                       fifo_edge_list_ptr,
                       num_ite_plus1,
                       i_req, i_resp);
        }
    }
}

void read_A(tapa::async_mmap<ap_uint<512>> & A,
            tapa::ostream<ap_uint<512>> & fifo_A,
            const int A_len,
            const int P_N
            ) {
    const int N16 = P_N >> 16;
    const int rp_time = (N16 == 0)? 1 : N16;
    const int N = P_N & 0xFFFF;

    const int rp_time_N = rp_time * ((N + 7) >> 3);

l_rp:
    for(int rp = 0; rp < rp_time_N; rp++) {
#pragma HLS loop_flatten off
#pragma HLS loop_tripcount min=1 max=16
    rd_A:
        for(int i_req = 0, i_resp = 0; i_resp < A_len;) {
#pragma HLS loop_tripcount min=1 max=10000
#pragma HLS pipeline II=1
            async_read(A,
                       fifo_A,
                       A_len,
                       i_req, i_resp);
        }
    }
}

void read_B(tapa::async_mmap<float_v16> &  B,
            tapa::ostream<float_v16> & fifo_B,
            const int K,
            const int P_N
            ) {
    const int N16 = P_N >> 16;
    const int rp_time = (N16 == 0)? 1 : N16;
    const int N = P_N & 0xFFFF;
    const int num_ite_B = ((K + 7) >> 3) * ((N + 7) >> 3);

l_rp:
    for(int rp = 0; rp < rp_time; rp++) {
#pragma HLS loop_flatten off
#pragma HLS loop_tripcount min=1 max=16
    rd_B:
        for(int i_req = 0, i_resp = 0; i_resp < num_ite_B;) {
#pragma HLS loop_tripcount min=1 max=500000
#pragma HLS pipeline II=1
            async_read(B,
                       fifo_B,
                       num_ite_B,
                       i_req, i_resp);
        }
    }
}

void read_C(tapa::async_mmap<float_v16> &  C,
            tapa::ostream<float_v16> & fifo_C,
            const int M,
            const int P_N,
            tapa::ostream<int> & wrC_inst
            ) {
    wrC_inst.write(M);
    wrC_inst.write(P_N);

    const int N16 = P_N >> 16;
    const int rp_time = (N16 == 0)? 1 : N16;
    const int N = P_N & 0xFFFF;
    const int num_ite_C = ((M + 15) >> 4) * ((N + 7) >> 3);

l_rp:
    for(int rp = 0; rp < rp_time; rp++) {
#pragma HLS loop_flatten off
#pragma HLS loop_tripcount min=1 max=16
    rd_C:
        for(int i_req = 0, i_resp = 0; i_resp < num_ite_C;) {
#pragma HLS loop_tripcount min=1 max=500000
#pragma HLS pipeline II=1
            async_read(C,
                       fifo_C,
                       num_ite_C,
                       i_req, i_resp);
        }
    }
}

void write_C(tapa::istream<int> & wrC_inst,
             tapa::istream<float_v16> & fifo_C,
             tapa::async_mmap<float_v16> & C_out
             ) {
    int M = wrC_inst.read();
    int P_N = wrC_inst.read();

    const int N16 = P_N >> 16;
    const int rp_time = (N16 == 0)? 1 : N16;
    const int N = P_N & 0xFFFF;
    const int num_ite_C = ((M + 15) >> 4) * ((N + 7) >> 3);

l_rp:
    for(int rp = 0; rp < rp_time; rp++) {
#pragma HLS loop_flatten off
#pragma HLS loop_tripcount min=1 max=16
    wr_C:
        for(int i_req = 0, i_resp = 0; i_resp < num_ite_C;) {
#pragma HLS loop_tripcount min=1 max=500000
#pragma HLS pipeline II=1
            if (i_req < num_ite_C &&
                i_req < i_resp + 64 &&
                !fifo_C.empty() &&
                !C_out.write_addr.full() &&
                !C_out.write_data.full() ) {
                C_out.write_addr.try_write(i_req);
                C_out.write_data.try_write(fifo_C.read(nullptr));
                ++i_req;
            }
            if (!C_out.write_resp.empty()) {
                i_resp += ap_uint<9>(C_out.write_resp.read(nullptr)) + 1;
            }
        }
    }
}

void FloatvMultConst(const int alpha_u,
            tapa::istream<float_v16> & fifo_in,
            tapa::ostream<float_v16> & fifo_out
            ) {
    const float alpha_f = tapa::bit_cast<float>(alpha_u);
cc:
    for (;;) {
#pragma HLS pipeline II=1
        float_v16 tmp;
        bool read_ready = fifo_in.try_read(tmp);
        if (read_ready) {
            float_v16 c_out = tmp * alpha_f;
            fifo_out.write(c_out);
        }
    }
}

void FloatvAddFloatv(tapa::istream<float_v16> & fifo_in0,
                     tapa::istream<float_v16> & fifo_in1,
                     tapa::ostream<float_v16> & fifo_out
                     ) {
cc:
    for (;;) {
#pragma HLS pipeline II=1
        bool flag_nop = fifo_in0.empty() | fifo_in1.empty();
        if (!flag_nop) {
            float_v16 tmp0 = fifo_in0.read();
            float_v16 tmp1 = fifo_in1.read();
            float_v16 c_out = tmp0 + tmp1;
            fifo_out.write(c_out);
        }
    }
}

/*
void PU2core(ap_uint<18> & addr_c,
             float a_val_f,
             float b_val_d0_f,
             float b_val_d1_f,
             ap_uint<64> local_C_pe0_d0_d1[URAM_DEPTH]
             ) {
#pragma HLS inline
    ap_uint<64> c_val_d0_d1_u64 = local_C_pe0_d0_d1[addr_c];

    ap_uint<32> c_val_d0_u = c_val_d0_d1_u64(31,  0);
    ap_uint<32> c_val_d1_u = c_val_d0_d1_u64(63, 32);

    float c_val_d0_f = tapa::bit_cast<float>(c_val_d0_u);
    float c_val_d1_f = tapa::bit_cast<float>(c_val_d1_u);

    c_val_d0_f += tapa::reg(a_val_f) * b_val_d0_f;
    c_val_d1_f += tapa::reg(a_val_f) * b_val_d1_f;

    c_val_d0_u = tapa::bit_cast<ap_uint<32>>(c_val_d0_f);
    c_val_d1_u = tapa::bit_cast<ap_uint<32>>(c_val_d1_f);

    c_val_d0_d1_u64(31,  0) = c_val_d0_u;
    c_val_d0_d1_u64(63, 32) = c_val_d1_u;

    local_C_pe0_d0_d1[addr_c] = c_val_d0_d1_u64;
}

void PEcore(ap_uint<14> & addr_b,
            ap_uint<18> & addr_c,
            ap_uint<32> & a_val_u,
            ap_uint<64> local_C[4][URAM_DEPTH],
            float local_B[8][WINDOW_SIZE]
            ) {
#pragma HLS inline
    //if (addr_c != ((ap_uint<18>) 0x3FFFF)) {
    if (addr_c[17] == 0) {
        float a_val_f = tapa::bit_cast<float>(a_val_u);
        for (int i = 0; i < 4; ++i) {
            PU2core(addr_c,
                    a_val_f,
                    local_B[i*2+0][addr_b],
                    local_B[i*2+1][addr_b],
                    local_C[i]
                    );
        }
    }
}
*/

void PEcore_Bmtx(ap_uint<14> addr_b,
                 ap_uint<32> a_val_u,
                 float local_B[8][WINDOW_SIZE],
                 float_v8 & abv
                 ) {
#pragma HLS inline
    float a_val_f = tapa::bit_cast<float>(a_val_u);
    for (int i = 0; i < 8; ++i) {
        abv[i] = a_val_f * local_B[i][addr_b];
    }
}

void PEG_Bmtx(tapa::istream<int> & PE_inst_in,
              tapa::istream<int> & fifo_inst_in,
              //tapa::istream<ap_uint<128>> & fifo_A,
              tapa::istream<ap_uint<256>> & fifo_A,
              tapa::istreams<float_v16, NUM_CH_B> & fifo_B_in, // [256(16)] * 2, 2: dim d
              // [64(32bits * 2.0)] * 8 dim
              tapa::ostream<int> & PE_inst_out,
              tapa::ostream<int> & fifo_inst_out,
              tapa::ostreams<float_v16, NUM_CH_B> & fifo_B_out,
              // to PEG_Cmtx
              tapa::ostream<int> & PE_inst_to_Cmtx,
              tapa::ostream<int> & fifo_inst_out_to_Cmtx,
              tapa::ostreams<MultBVec, 4> & fifo_aBvec
              ) {
    const int NUM_ITE = PE_inst_in.read();
    const int M = PE_inst_in.read();
    const int P_N = PE_inst_in.read();
    const int K = PE_inst_in.read();

    PE_inst_out.write(NUM_ITE);
    PE_inst_out.write(M);
    PE_inst_out.write(P_N);
    PE_inst_out.write(K);

    PE_inst_to_Cmtx.write(NUM_ITE);
    PE_inst_to_Cmtx.write(M);
    PE_inst_to_Cmtx.write(P_N);

    const int N16 = P_N >> 16;
    const int rp_time = (N16 == 0)? 1 : N16;
    const int N = P_N & 0xFFFF;
    const int rp_time_N = rp_time * ((N + 7) >> 3);

l_rp:
    for(int rp = 0; rp < rp_time_N; rp++) {
#pragma HLS loop_flatten off
#pragma HLS loop_tripcount min=1 max=16

        //float local_B[8/2][8][WINDOW_SIZE];
        //float local_B[8][WINDOW_SIZE];
        float local_B[4/2][8][WINDOW_SIZE];
#pragma HLS bind_storage variable=local_B latency=2
#pragma HLS array_partition variable=local_B complete dim=1
#pragma HLS array_partition variable=local_B complete dim=2
#pragma HLS array_partition variable=local_B cyclic factor=B_PARTITION_FACTOR dim=3
//#pragma HLS array_partition variable=local_B cyclic factor=B_PARTITION_FACTOR dim=2

        auto start_32 = fifo_inst_in.read();
        fifo_inst_out.write(start_32);
        fifo_inst_out_to_Cmtx.write(start_32);

    main:
        for (int i = 0; i < NUM_ITE; ++i) {
#pragma HLS loop_tripcount min=1 max=49

            // fill onchip B
        read_B:
            for (int j = 0; (j < (WINDOW_SIZE >> 3)) && (j < ((K + 7) >> 3) - i * (WINDOW_SIZE >> 3)); ) {
#pragma HLS loop_tripcount min=1 max=512
#pragma HLS pipeline II = 1

                bool b_2048_ready = true;
                bool b_2048_out_not_full = true;
                for (int k = 0; k < NUM_CH_B; ++k) {
                    b_2048_ready &= !fifo_B_in[k].empty();
                    b_2048_out_not_full &= !fifo_B_out[k].full();
                }

                if (b_2048_ready & b_2048_out_not_full) {
                    float_v16 b_512_x[NUM_CH_B];
                    for (int k = 0; k < NUM_CH_B; ++k) {
                        b_512_x[k] = fifo_B_in[k].read();
                        fifo_B_out[k].write(b_512_x[k]);
                    }

                    for (int k = 0; k < 8; ++k) {
                        for (int m = 0; m < 8; ++m) {
                            for (int l = 0; l < 2; ++l) {
                                local_B[l][m][j * 8 + k] = b_512_x[m/2][k + m % 2 * 8];
                            }
                        }
                    }
                    ++j;
                }
            }

            // computation
            const auto end_32 = fifo_inst_in.read();
            fifo_inst_out.write(end_32);
            fifo_inst_out_to_Cmtx.write(end_32);

        computation:
            for (int j = start_32; j < end_32; ) {
#pragma HLS loop_tripcount min=1 max=200
#pragma HLS pipeline II=1

                //ap_uint<128> a_pes;
                ap_uint<256> a_pes;
                bool a_pes_ready = fifo_A.try_read(a_pes);

                if (a_pes_ready) {
                    for (int p = 0; p < 4; ++p) {
                        ap_uint<64> a = a_pes(63 + p * 64, p * 64);

                        ap_uint<14> a_col = a(63, 50);
                        ap_uint<18> a_row = a(49, 32);
                        ap_uint<32> a_val = a(31,  0);

                        MultBVec rabv;
                        rabv.row = a_row;

                        if (a_row[17] == 0) {
                            // PE process
                            PEcore_Bmtx(a_col,
                                        a_val,
                                        local_B[p/2],
                                        rabv.abvec
                                        );
                        }
                        fifo_aBvec[p].write(rabv);
                    }
                    ++j;
                }
            }
            start_32 = end_32;
        }
    }
}

void PU2core_Cmtx(ap_uint<18> addr_c,
                  float val_d0_f,
                  float val_d1_f,
                  ap_uint<64> local_C_pe0_d0_d1[URAM_DEPTH]
                  ) {
#pragma HLS inline
    ap_uint<64> c_val_d0_d1_u64 = local_C_pe0_d0_d1[addr_c];

    ap_uint<32> c_val_d0_u = c_val_d0_d1_u64(31,  0);
    ap_uint<32> c_val_d1_u = c_val_d0_d1_u64(63, 32);

    float c_val_d0_f = tapa::bit_cast<float>(c_val_d0_u) + val_d0_f;
    float c_val_d1_f = tapa::bit_cast<float>(c_val_d1_u) + val_d1_f;

    c_val_d0_u = tapa::bit_cast<ap_uint<32>>(c_val_d0_f);
    c_val_d1_u = tapa::bit_cast<ap_uint<32>>(c_val_d1_f);

    c_val_d0_d1_u64(31,  0) = c_val_d0_u;
    c_val_d0_d1_u64(63, 32) = c_val_d1_u;

    local_C_pe0_d0_d1[addr_c] = c_val_d0_d1_u64;
}

void PEcore_Cmtx(ap_uint<18> addr_c,
                 float_v8 & abvec,
                 ap_uint<64> local_C[4][URAM_DEPTH]
                 ) {
#pragma HLS inline
    for (int i = 0; i < 4; ++i) {
        PU2core_Cmtx(addr_c,
                     abvec[i*2+0],
                     abvec[i*2+1],
                     local_C[i]
                     );
    }
}

void PEG_Cmtx(tapa::istream<int> & PE_inst_in,
              tapa::istream<int> & fifo_inst_in,
              tapa::istreams<MultBVec, 4> & fifo_aBvec,
              tapa::ostream<float_v8> & fifo_C_out
              ) {
    const int NUM_ITE = PE_inst_in.read();
    const int M = PE_inst_in.read();
    const int P_N = PE_inst_in.read();

    const int N16 = P_N >> 16;
    const int rp_time = (N16 == 0)? 1 : N16;
    const int N = P_N & 0xFFFF;
    const int rp_time_N = rp_time * ((N + 7) >> 3);

    const int num_v_init = (M + 63) >> 6;
    //const int num_v_out = (M + 31) >> 5;
    const int num_v_out = (M + 15) >> 4;

    //define local C buffer and pragma to URAM
    //ap_uint<64> local_C[2][8 / 2][URAM_DEPTH];
    ap_uint<64> local_C[4][8 / 2][URAM_DEPTH];
#pragma HLS bind_storage variable=local_C type=RAM_2P impl=URAM latency=1
#pragma HLS array_partition complete variable=local_C dim=1
#pragma HLS array_partition complete variable=local_C dim=2

l_rp:
    for(int rp = 0; rp < rp_time_N; rp++) {
#pragma HLS loop_flatten off
#pragma HLS loop_tripcount min=1 max=16

        //init local C
    init_C:
        for (int i = 0; i < num_v_init; ++i) {
#pragma HLS loop_tripcount min=1 max=800
#pragma HLS pipeline II=1
            //for (int j = 0; j < 2; ++j) {
            for (int j = 0; j < 4; ++j) {
                for (int k = 0; k < 8 / 2; ++k) {
                    local_C[j][k][i] = 0;
                }
            }
        }

        auto start_32 = fifo_inst_in.read();

    main:
        for (int i = 0; i < NUM_ITE; ++i) {
#pragma HLS loop_tripcount min=1 max=49

            // computation
            const auto end_32 = fifo_inst_in.read();

        computation:
            for (int j = start_32; j < end_32; ) {
#pragma HLS loop_tripcount min=1 max=200
#pragma HLS pipeline II=1
#pragma HLS dependence true variable=local_C distance=DEP_DIST_LOAD_STORE
                bool nop_flag = false;
                for (int p = 0; p < 4; ++p) {
                    nop_flag |= fifo_aBvec[p].empty();
                }

                if (!nop_flag) {
                    for (int p = 0; p < 4; ++p) {
                        MultBVec rabv = fifo_aBvec[p].read();
                        ap_uint<18> a_row = rabv.row;

                        if (a_row[17] == 0) {
                            PEcore_Cmtx(a_row,
                                        rabv.abvec,
                                        local_C[p]
                                        );
                        }
                    }
                    ++j;
                }
            }
            start_32 = end_32;
        }

        //cout << "PE = " << pe_idx << endl;
    write_C_outer:
        for (int i = 0, c_idx = 0; i < num_v_out; ++i) {
#pragma HLS loop_tripcount min=1 max=1800
#pragma HLS pipeline II=1
            ap_uint<32> u_32_d[8];

            for (int d = 0; d < 4; ++d) {
                ap_uint<64> u_64 = local_C[c_idx][d][i>>2];
                u_32_d[2 * d    ] = u_64(31,  0);
                u_32_d[2 * d + 1] = u_64(63, 32);
            }

            switch (c_idx) { //0,2,1,3
                case 0: c_idx = 2; break;
                case 1: c_idx = 3; break;
                case 2: c_idx = 1; break;
                case 3: c_idx = 0; break;
            }

            float_v8 out_v;
            for (int d = 0; d < 8; ++d) {
                out_v[d] = tapa::bit_cast<float>(u_32_d[d]);
            }
            fifo_C_out.write(out_v);
            //for (int ii = 0; ii < 8; ++ii) {cout << out_v[ii] << " ";} cout << endl;
        }
    }
}

/*
void PEG(tapa::istream<int> & PE_inst_in,
         tapa::istream<int> & fifo_inst_in,
         //tapa::istream<ap_uint<128>> & fifo_A,
         tapa::istream<ap_uint<256>> & fifo_A,
         tapa::istreams<float_v16, NUM_CH_B> & fifo_B_in, // [256(16)] * 2, 2: dim d
         // [64(32bits * 2.0)] * 8 dim
         tapa::ostream<int> & PE_inst_out,
         tapa::ostream<int> & fifo_inst_out,
         tapa::ostreams<float_v16, NUM_CH_B> & fifo_B_out,
         tapa::ostream<float_v8> & fifo_C_out
         ) {
    tapa::streams<MultBVec, 4, FIFO_DEPTH> fifo_aBvec("fifo_aBvec");
    tapa::stream<int, FIFO_DEPTH> PE_inst_to_Cmtx("PE_inst_to_Cmtx");
    tapa::stream<int, FIFO_DEPTH> fifo_inst_out_to_Cmtx("fifo_inst_out_to_Cmtx");

    tapa::task()
        .invoke(PEG_Bmtx,
                PE_inst_in,
                fifo_inst_in,
                fifo_A,
                fifo_B_in,
                PE_inst_out,
                fifo_inst_out,
                fifo_B_out,
                // to PEG_Cmtx
                PE_inst_to_Cmtx,
                fifo_inst_out_to_Cmtx,
                fifo_aBvec)

        .invoke(PEG_Cmtx,
                PE_inst_to_Cmtx,
                fifo_inst_out_to_Cmtx,
                fifo_aBvec,
                fifo_C_out)
    ;
}

void PEG_c(tapa::istream<int> & PE_inst_in,
           tapa::istream<int> & fifo_inst_in,
           //tapa::istream<ap_uint<128>> & fifo_A,
           tapa::istream<ap_uint<256>> & fifo_A,
           tapa::istreams<float_v16, NUM_CH_B> & fifo_B_in, // [256(16)] * 2, 2: dim d
           // [64(32bits * 2.0)] * 8 dim
           tapa::ostream<int> & PE_inst_out,
           tapa::ostream<int> & fifo_inst_out,
           tapa::ostreams<float_v16, NUM_CH_B> & fifo_B_out,
           tapa::ostream<float_v8> & fifo_C_out
           ) {
    const int NUM_ITE = PE_inst_in.read();
    const int M = PE_inst_in.read();
    const int P_N = PE_inst_in.read();
    const int K = PE_inst_in.read();

    PE_inst_out.write(NUM_ITE);
    PE_inst_out.write(M);
    PE_inst_out.write(P_N);
    PE_inst_out.write(K);

    const int N16 = P_N >> 16;
    const int rp_time = (N16 == 0)? 1 : N16;
    const int N = P_N & 0xFFFF;
    const int rp_time_N = rp_time * ((N + 7) >> 3);

    const int num_v_init = (M + 63) >> 6;
    //const int num_v_out = (M + 31) >> 5;
    const int num_v_out = (M + 15) >> 4;

    //define local C buffer and pragma to URAM
    //ap_uint<64> local_C[2][8 / 2][URAM_DEPTH];
    ap_uint<64> local_C[4][8 / 2][URAM_DEPTH];
#pragma HLS bind_storage variable=local_C type=RAM_2P impl=URAM latency=1
#pragma HLS array_partition complete variable=local_C dim=1
#pragma HLS array_partition complete variable=local_C dim=2

l_rp:
    for(int rp = 0; rp < rp_time_N; rp++) {
#pragma HLS loop_flatten off
#pragma HLS loop_tripcount min=1 max=16

        //init local C
    init_C:
        for (int i = 0; i < num_v_init; ++i) {
#pragma HLS loop_tripcount min=1 max=800
#pragma HLS pipeline II=1
            //for (int j = 0; j < 2; ++j) {
            for (int j = 0; j < 4; ++j) {
                for (int k = 0; k < 8 / 2; ++k) {
                    local_C[j][k][i] = 0;
                }
            }
        }
        //define local B buffer and pragma local B buffer if partition factor > 1

        //float local_B[8/2][8][WINDOW_SIZE];
        //float local_B[8][WINDOW_SIZE];
        float local_B[4/2][8][WINDOW_SIZE];
#pragma HLS bind_storage variable=local_B latency=2
#pragma HLS array_partition variable=local_B complete dim=1
#pragma HLS array_partition variable=local_B complete dim=2
#pragma HLS array_partition variable=local_B cyclic factor=B_PARTITION_FACTOR dim=3
//#pragma HLS array_partition variable=local_B cyclic factor=B_PARTITION_FACTOR dim=2

        auto start_32 = fifo_inst_in.read();
        fifo_inst_out.write(start_32);

    main:
        for (int i = 0; i < NUM_ITE; ++i) {
#pragma HLS loop_tripcount min=1 max=49

            // fill onchip B
        read_B:
            for (int j = 0; (j < (WINDOW_SIZE >> 3)) && (j < ((K + 7) >> 3) - i * (WINDOW_SIZE >> 3)); ) {
#pragma HLS loop_tripcount min=1 max=512
#pragma HLS pipeline II = 1

                bool b_2048_ready = true;
                bool b_2048_out_not_full = true;
                for (int k = 0; k < NUM_CH_B; ++k) {
                    b_2048_ready &= !fifo_B_in[k].empty();
                    b_2048_out_not_full &= !fifo_B_out[k].full();
                }

                if (b_2048_ready & b_2048_out_not_full) {
                    float_v16 b_512_x[NUM_CH_B];
                    for (int k = 0; k < NUM_CH_B; ++k) {
                        b_512_x[k] = fifo_B_in[k].read();
                        fifo_B_out[k].write(b_512_x[k]);
                    }

                    for (int k = 0; k < 8; ++k) {
                        for (int m = 0; m < 8; ++m) {
                            for (int l = 0; l < 2; ++l) {
                                local_B[l][m][j * 8 + k] = b_512_x[m/2][k + m % 2 * 8];
                            }
                        }
                    }
                    ++j;
                }
            }

            // computation
            const auto end_32 = fifo_inst_in.read();
            fifo_inst_out.write(end_32);

        computation:
            for (int j = start_32; j < end_32; ) {
#pragma HLS loop_tripcount min=1 max=200
#pragma HLS pipeline II=1
#pragma HLS dependence true variable=local_C distance=DEP_DIST_LOAD_STORE

                //ap_uint<128> a_pes;
                ap_uint<256> a_pes;
                bool a_pes_ready = fifo_A.try_read(a_pes);

                if (a_pes_ready) {
                    //for (int p = 0; p < 2; ++p) {
                    for (int p = 0; p < 4; ++p) {
                        ap_uint<14> a_col;
                        ap_uint<18> a_row;
                        ap_uint<32> a_val;

                        ap_uint<64> a = a_pes(63 + p * 64, p * 64);
                        a_col = a(63, 50);
                        a_row = a(49, 32);
                        a_val = a(31,  0);

                        // PE process
                        PEcore(a_col,
                               a_row,
                               a_val,
                               local_C[p],
                               //local_B
                               local_B[p/2]
                               );
                    }
                    ++j;
                }
            }
            start_32 = end_32;
        }

        //cout << "PE = " << pe_idx << endl;
    write_C_outer:
        for (int i = 0, c_idx = 0; i < num_v_out; ++i) {
#pragma HLS loop_tripcount min=1 max=1800
#pragma HLS pipeline II=1
            ap_uint<32> u_32_d[8];

            for (int d = 0; d < 4; ++d) {
                ap_uint<64> u_64 = local_C[c_idx][d][i>>2];
                u_32_d[2 * d    ] = u_64(31,  0);
                u_32_d[2 * d + 1] = u_64(63, 32);
            }

            switch (c_idx) { //0,2,1,3
                case 0: c_idx = 2; break;
                case 1: c_idx = 3; break;
                case 2: c_idx = 1; break;
                case 3: c_idx = 0; break;
            }

            float_v8 out_v;
            for (int d = 0; d < 8; ++d) {
                out_v[d] = tapa::bit_cast<float>(u_32_d[d]);
            }
            fifo_C_out.write(out_v);
            //for (int ii = 0; ii < 8; ++ii) {cout << out_v[ii] << " ";} cout << endl;
        }
    }
}
*/

void Scatter_1_2(tapa::istream<ap_uint<512>> & fifo_in,
                 tapa::ostreams<ap_uint<256>, 2> & fifo_out) {
    for (;;) {
#pragma HLS pipeline II=1
        bool flag_nop = fifo_in.empty();
        for (int i = 0; i < 2; ++i) {
            flag_nop |= fifo_out[i].full();
        }
        if (!flag_nop) {
            ap_uint<512> tmp = fifo_in.read();
            for (int i = 0; i < 2; ++i) {
                fifo_out[i].write(tmp(255 + i * 256, i * 256));
            }
        }
    }
}

void Merger(tapa::istreams<float_v8, 2> & fifo_in,
            tapa::ostream<float_v16> & fifo_out) {
    for (;;) {
#pragma HLS pipeline II=1
        bool flag_nop = fifo_out.full() | fifo_in[0].empty() | fifo_in[1].empty();
        if (!flag_nop) {
            float_v16 tmpv16;
            float_v8 tmpv8[2];
#pragma HLS aggregate variable=tmpv16
            tmpv8[0] = fifo_in[0].read();
            tmpv8[1] = fifo_in[1].read();
            for (int i = 0; i < 8; ++i) {
                tmpv16[i] = tmpv8[0][i];
                tmpv16[i + 8] = tmpv8[1][i];
            }
            fifo_out.write(tmpv16);
        }
    }
}

void black_hole_int(tapa::istream<int> & fifo_in) {
    for (;;) {
#pragma HLS pipeline II=1
        fifo_in.read(nullptr);
    }
}

void black_hole_float_v16(tapa::istream<float_v16> & fifo_in) {
    for (;;) {
#pragma HLS pipeline II=1
        fifo_in.read(nullptr);
    }
}

void Sextans(tapa::mmap<int> edge_list_ptr,

             tapa::mmaps<ap_uint<512>, NUM_CH_SPARSE> edge_list_ch,

             tapa::mmaps<float_v16, NUM_CH_B> mat_B_ch,

             tapa::mmaps<float_v16, NUM_CH_C> mat_C_ch_in,

             tapa::mmaps<float_v16, NUM_CH_C> mat_C_ch,

             const int NUM_ITE,
             const int NUM_A_LEN,
             const int M,
             const int K,
             const int P_N,
             const int alpha_u,
             const int beta_u
             ) {
    tapa::streams<int, NUM_CH_SPARSE * PEG_PER_A + 1, FIFO_DEPTH> PE_inst("PE_inst");

    tapa::streams<int, NUM_CH_C, FIFO_DEPTH> wrC_inst("wrC_inst");

    tapa::streams<int, NUM_CH_SPARSE * PEG_PER_A + 1, FIFO_DEPTH> fifo_edge_list_ptr("fifo_edge_list_ptr");

    tapa::streams<int, NUM_CH_SPARSE * PEG_PER_A, FIFO_DEPTH> PE_inst_to_Cmtx("PE_inst_to_Cmtx");

    tapa::streams<int, NUM_CH_SPARSE * PEG_PER_A, FIFO_DEPTH> fifo_edge_list_ptr_to_Cmtx("fifo_edge_list_ptr_to_Cmtx");

    /* ============================== */

    tapa::streams<ap_uint<512>, NUM_CH_SPARSE, FIFO_DEPTH> fifo_A("fifo_A");

    tapa::streams<ap_uint<256>, NUM_CH_SPARSE * PEG_PER_A, FIFO_DEPTH> fifo_A_pe("fifo_A_pe");

    tapa::streams<float_v16, (NUM_CH_SPARSE * PEG_PER_A + 1) * NUM_CH_B, FIFO_DEPTH> fifo_B_pe("fifo_B_pe");

    tapa::streams<float_v8, NUM_CH_SPARSE * PEG_PER_A, FIFO_DEPTH> fifo_C_pe("fifo_C_pe");

    tapa::streams<MultBVec, NUM_CH_SPARSE * PEG_PER_A * 4, FIFO_DEPTH> fifo_aBvec("fifo_aBvec");

    tapa::streams<float_v16, NUM_CH_C, FIFO_DEPTH> fifo_C_read_in("fifo_C_read_in");

    tapa::streams<float_v16, NUM_CH_C, FIFO_DEPTH> fifo_C_read_in_beta("fifo_C_read_in_beta");

    tapa::streams<float_v16, NUM_CH_C, FIFO_DEPTH> fifo_C_ch_result("fifo_C_ch_result");

    tapa::streams<float_v16, NUM_CH_C, FIFO_DEPTH> fifo_C_ch_result_alpha("fifo_C_ch_result_alpha");

    tapa::streams<float_v16, NUM_CH_C, FIFO_DEPTH> fifo_C_ch("fifo_C_ch");

    tapa::task()
        .invoke(read_edge_list_ptr,
                NUM_ITE,
                M,
                P_N,
                K,
                edge_list_ptr,
                fifo_edge_list_ptr,
                PE_inst
                )

        .invoke<tapa::join, NUM_CH_SPARSE>(read_A,
                                           edge_list_ch,
                                           fifo_A,
                                           NUM_A_LEN,
                                           P_N
                                           )

        .invoke<tapa::detach, NUM_CH_SPARSE>(Scatter_1_2,
                                             fifo_A,
                                             fifo_A_pe
                                             )

        .invoke<tapa::join, NUM_CH_B>(read_B,
                                      mat_B_ch,
                                      fifo_B_pe,
                                      K,
                                      P_N
                                      )

        .invoke<tapa::join, NUM_CH_SPARSE * PEG_PER_A>(PEG_Bmtx,
                                                       PE_inst,
                                                       fifo_edge_list_ptr,
                                                       fifo_A_pe,
                                                       fifo_B_pe,
                                                       PE_inst,
                                                       fifo_edge_list_ptr,
                                                       fifo_B_pe,
                                                       PE_inst_to_Cmtx,
                                                       fifo_edge_list_ptr_to_Cmtx,
                                                       fifo_aBvec
                                                       )

        .invoke<tapa::join, NUM_CH_SPARSE * PEG_PER_A>(PEG_Cmtx,
                                                       PE_inst_to_Cmtx,
                                                       fifo_edge_list_ptr_to_Cmtx,
                                                       fifo_aBvec,
                                                       fifo_C_pe
                                                       )

        .invoke<tapa::detach>(black_hole_int,
                              PE_inst)
        .invoke<tapa::detach>(black_hole_int,
                              fifo_edge_list_ptr)
        .invoke<tapa::detach, NUM_CH_B>(black_hole_float_v16,
                                        fifo_B_pe)

        .invoke<tapa::detach, NUM_CH_SPARSE>(Merger,
                                             fifo_C_pe,
                                             fifo_C_ch_result
                                             )

        .invoke<tapa::join, NUM_CH_C>(read_C,
                                      mat_C_ch_in,
                                      fifo_C_read_in,
                                      M,
                                      P_N,
                                      wrC_inst
                                      )

        .invoke<tapa::detach, NUM_CH_C>(FloatvMultConst,
                                        beta_u,
                                        fifo_C_read_in,
                                        fifo_C_read_in_beta
                                        )

        .invoke<tapa::detach, NUM_CH_C>(FloatvMultConst,
                                        alpha_u,
                                        fifo_C_ch_result,
                                        fifo_C_ch_result_alpha
                                        )

        .invoke<tapa::detach, NUM_CH_C>(FloatvAddFloatv,
                                        fifo_C_ch_result_alpha,
                                        fifo_C_read_in_beta,
                                        fifo_C_ch
                                        )

        .invoke<tapa::join, NUM_CH_C>(write_C,
                                      wrC_inst,
                                      fifo_C_ch,
                                      mat_C_ch
                                      )
    ;
}
