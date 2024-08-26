// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include <ap_int.h>
#include <cassert>
#include <cstdio>
#include <cstring>

#include <tapa.h>

#include "serpens.h"

// #include <iostream>
// using namespace std;

constexpr int FIFO_DEPTH = 2;

const int NUM_CH_SPARSE_div_8 = NUM_CH_SPARSE / 8;
const int NUM_CH_SPARSE_mult_16 = NUM_CH_SPARSE * 16;
const int NUM_CH_SPARSE_mult_2 = NUM_CH_SPARSE * 2;
const int WINDOW_SIZE_div_16 = WINDOW_SIZE >> 4;

using float_v8 = tapa::vec_t<float, 8>;
using float_v2 = tapa::vec_t<float, 2>;

struct MultXVec {
  tapa::vec_t<ap_uint<18>, 8> row;
  float_v8 axv;
};

template <typename T, typename R>
inline void async_read(tapa::async_mmap<T>& A, tapa::ostream<T>& fifo_A,
                       const R A_len, R& i_req, R& i_resp) {
#pragma HLS inline
  if ((i_req < A_len) & !A.read_addr.full()) {
    A.read_addr.try_write(i_req);
    ++i_req;
  }
  if (!fifo_A.full() & !A.read_data.empty()) {
    T tmp;
    A.read_data.try_read(tmp);
    fifo_A.try_write(tmp);
    ++i_resp;
  }
}

void read_edge_list_ptr(const int num_ite, const int M, const int P_N,
                        const int K, tapa::async_mmap<int>& edge_list_ptr,
                        tapa::ostream<int>& PE_inst) {
  const int rp_time = (P_N == 0) ? 1 : P_N;

  PE_inst.write(num_ite);
  PE_inst.write(M);
  PE_inst.write(rp_time);
  PE_inst.write(K);

  const int num_ite_plus1 = num_ite + 1;
l_rp:
  for (int rp = 0; rp < rp_time; rp++) {
#pragma HLS loop_flatten off
#pragma HLS loop_tripcount min = 1 max = 16
  rd_ptr:
    for (int i_req = 0, i_resp = 0; i_resp < num_ite_plus1;) {
#pragma HLS loop_tripcount min = 1 max = 800
#pragma HLS pipeline II = 1
      async_read(edge_list_ptr, PE_inst, num_ite_plus1, i_req, i_resp);
    }
  }
}

void read_X(const int P_N, const int K, tapa::async_mmap<float_v16>& vec_X,
            tapa::ostream<float_v16>& fifo_X) {
  const int rp_time = (P_N == 0) ? 1 : P_N;
  const int num_ite_X = (K + 15) >> 4;

l_rp:
  for (int rp = 0; rp < rp_time; rp++) {
#pragma HLS loop_flatten off
#pragma HLS loop_tripcount min = 1 max = 16
  rd_X:
    for (int i_req = 0, i_resp = 0; i_resp < num_ite_X;) {
#pragma HLS loop_tripcount min = 1 max = 500000
#pragma HLS pipeline II = 1
      async_read(vec_X, fifo_X, num_ite_X, i_req, i_resp);
    }
  }
}

void read_A(const int P_N, const int A_len, tapa::async_mmap<ap_uint<512>>& A,
            tapa::ostream<ap_uint<512>>& fifo_A) {
  const int rp_time = (P_N == 0) ? 1 : P_N;
l_rp:
  for (int rp = 0; rp < rp_time; rp++) {
#pragma HLS loop_flatten off
#pragma HLS loop_tripcount min = 1 max = 16
  rd_A:
    for (int i_req = 0, i_resp = 0; i_resp < A_len;) {
#pragma HLS loop_tripcount min = 1 max = 10000
#pragma HLS pipeline II = 1
      async_read(A, fifo_A, A_len, i_req, i_resp);
    }
  }
}

void PEG_Xvec(tapa::istream<int>& fifo_inst_in,
              tapa::istream<ap_uint<512>>& fifo_A,
              tapa::istream<float_v16>& fifo_X_in,
              tapa::ostream<int>& fifo_inst_out,
              tapa::ostream<float_v16>& fifo_X_out,
              // to PEG_Yvec
              tapa::ostream<int>& fifo_inst_out_to_Yvec,
              tapa::ostream<MultXVec>& fifo_aXvec) {
  const int NUM_ITE = fifo_inst_in.read();
  const int M = fifo_inst_in.read();
  const int rp_time = fifo_inst_in.read();
  const int K = fifo_inst_in.read();

  fifo_inst_out.write(NUM_ITE);
  fifo_inst_out.write(M);
  fifo_inst_out.write(rp_time);
  fifo_inst_out.write(K);

  fifo_inst_out_to_Yvec.write(NUM_ITE);
  fifo_inst_out_to_Yvec.write(M);
  fifo_inst_out_to_Yvec.write(rp_time);

l_rp:
  for (int rp = 0; rp < rp_time; rp++) {
#pragma HLS loop_flatten off
#pragma HLS loop_tripcount min = 1 max = 16

    float local_X[4][WINDOW_SIZE];
#pragma HLS bind_storage variable = local_X latency = 2
#pragma HLS array_partition variable = local_X complete dim = 1
#pragma HLS array_partition variable = local_X cyclic factor = \
    X_PARTITION_FACTOR dim = 2

    auto start_32 = fifo_inst_in.read();
    fifo_inst_out.write(start_32);
    fifo_inst_out_to_Yvec.write(start_32);

  main:
    for (int i = 0; i < NUM_ITE; ++i) {
#pragma HLS loop_tripcount min = 1 max = 49

      // fill onchip X
    read_X:
      for (int j = 0; (j < WINDOW_SIZE_div_16) &
                      (j < ((K + 15) >> 4) - i * WINDOW_SIZE_div_16);) {
#pragma HLS loop_tripcount min = 1 max = 512
#pragma HLS pipeline II = 1
        if (!fifo_X_in.empty() & !fifo_X_out.full()) {
          float_v16 x;
          fifo_X_in.try_read(x);
          fifo_X_out.try_write(x);
          for (int kk = 0; kk < 16; ++kk) {
            for (int l = 0; l < 4; ++l) {
              local_X[l][(j << 4) + kk] = x[kk];
            }
          }
          ++j;
        }
      }

      // computation
      const auto end_32 = fifo_inst_in.read();
      fifo_inst_out.write(end_32);
      fifo_inst_out_to_Yvec.write(end_32);

    computation:
      for (int j = start_32; j < end_32;) {
#pragma HLS loop_tripcount min = 1 max = 200
#pragma HLS pipeline II = 1
        if (!fifo_A.empty()) {
          ap_uint<512> a_pes;
          fifo_A.try_read(a_pes);
          MultXVec raxv;

          for (int p = 0; p < 8; ++p) {
            ap_uint<64> a = a_pes(63 + p * 64, p * 64);
            ap_uint<14> a_col = a(63, 50);
            ap_uint<18> a_row = a(49, 32);
            ap_uint<32> a_val = a(31, 0);

            raxv.row[p] = a_row;
            if (a_row[17] == 0) {
              float a_val_f = tapa::bit_cast<float>(a_val);
              raxv.axv[p] = a_val_f * local_X[p / 2][a_col];
            }
          }
          fifo_aXvec.write(raxv);
          ++j;
        }
      }
      start_32 = end_32;
    }
  }
}

inline void PUcore_Ymtx(ap_uint<18> addr_c, float val_d0_f,
                        ap_uint<64> local_C_pe0[URAM_DEPTH]) {
#pragma HLS inline
  ap_uint<64> c_val_u64 = local_C_pe0[addr_c(17, 1)];

  ap_uint<32> c_val_d0_u = c_val_u64(31, 0);
  ap_uint<32> c_val_d1_u = c_val_u64(63, 32);

  ap_uint<32> c_val_u = (addr_c[0]) ? c_val_d1_u : c_val_d0_u;

  float c_val_plus_d0_f = tapa::bit_cast<float>(c_val_u) + val_d0_f;

  c_val_u = tapa::bit_cast<ap_uint<32>>(c_val_plus_d0_f);

  if (addr_c[0]) {
    c_val_d1_u = c_val_u;
  } else {
    c_val_d0_u = c_val_u;
  }

  c_val_u64(63, 32) = c_val_d1_u;
  c_val_u64(31, 0) = c_val_d0_u;
  local_C_pe0[addr_c(17, 1)] = c_val_u64;
}

void PEG_Yvec(tapa::istream<int>& fifo_inst_in,
              tapa::istream<MultXVec>& fifo_aXvec,
              tapa::ostream<float_v2>& fifo_Y_out) {
  const int NUM_ITE = fifo_inst_in.read();
  const int M = fifo_inst_in.read();
  const int rp_time = fifo_inst_in.read();

  const int num_v_init =
      (M + NUM_CH_SPARSE_mult_16 - 1) / NUM_CH_SPARSE_mult_16;
  const int num_v_out = (M + NUM_CH_SPARSE_mult_2 - 1) / NUM_CH_SPARSE_mult_2;

  ap_uint<64> local_C[8][URAM_DEPTH];
#pragma HLS bind_storage variable = local_C type = RAM_2P impl = \
    URAM latency = 1
#pragma HLS array_partition complete variable = local_C dim = 1

l_rp:
  for (int rp = 0; rp < rp_time; rp++) {
#pragma HLS loop_flatten off
#pragma HLS loop_tripcount min = 1 max = 16

    // init local C
  init_C:
    for (int i = 0; i < num_v_init; ++i) {
#pragma HLS loop_tripcount min = 1 max = 800
#pragma HLS pipeline II = 1
      for (int p = 0; p < 8; ++p) {
        local_C[p][i] = 0;
      }
    }

    auto start_32 = fifo_inst_in.read();

  main:
    for (int i = 0; i < NUM_ITE; ++i) {
#pragma HLS loop_tripcount min = 1 max = 49

      // computation
      const auto end_32 = fifo_inst_in.read();

    computation:
      for (int j = start_32; j < end_32;) {
#pragma HLS loop_tripcount min = 1 max = 200
#pragma HLS pipeline II = 1
#pragma HLS dependence true variable = local_C distance = DEP_DIST_LOAD_STORE
        if (!fifo_aXvec.empty()) {
          MultXVec raxv;
          fifo_aXvec.try_read(raxv);
          for (int p = 0; p < 8; ++p) {
            auto a_row = raxv.row[p];
            if (a_row[17] == 0) {
              PUcore_Ymtx(a_row, raxv.axv[p], local_C[p]);
            }
          }
          ++j;
        }
      }
      start_32 = end_32;
    }

    // cout << "PE = " << pe_idx << endl;
  write_C_outer:
    for (int i = 0, c_idx = 0; i < num_v_out; ++i) {
#pragma HLS loop_tripcount min = 1 max = 1800
#pragma HLS pipeline II = 1
      float_v2 out_v;
      ap_uint<64> u_64 = local_C[c_idx][i >> 3];
      for (int d = 0; d < 2; ++d) {
        ap_uint<32> u_32_d = u_64(31 + 32 * d, 32 * d);
        out_v[d] = tapa::bit_cast<float>(u_32_d);
      }
      fifo_Y_out.write(out_v);
      ++c_idx;
      if (c_idx == 8) {
        c_idx = 0;
      }
    }
  }
}

void Arbiter_Y(const int P_N, const int M,
               tapa::istreams<float_v2, NUM_CH_SPARSE_div_8>& fifo_in,
               tapa::ostream<float_v2>& fifo_out) {
  const int rp_time = (P_N == 0) ? 1 : P_N;
  const int num_pe_output =
      ((M + NUM_CH_SPARSE_mult_2 - 1) / NUM_CH_SPARSE_mult_2) *
      NUM_CH_SPARSE_div_8;
  const int num_out = (M + 15) >> 4;
  const int num_ite_Y = num_pe_output * rp_time;
aby:
  for (int i = 0, c_idx = 0, o_idx = 0; i < num_ite_Y;) {
#pragma HLS loop_tripcount min = 1 max = 1800
#pragma HLS pipeline II = 1
    if (!fifo_in[c_idx].empty() & !fifo_out.full()) {
      float_v2 tmp;
      fifo_in[c_idx].try_read(tmp);
      if (o_idx < num_out) {
        fifo_out.try_write(tmp);
      }
      ++i;
      c_idx++;
      o_idx++;
      if (c_idx == NUM_CH_SPARSE_div_8) {
        c_idx = 0;
      }
      if (o_idx == num_pe_output) {
        o_idx = 0;
      }
    }
  }
}

void Merger_Y(tapa::istreams<float_v2, 8>& fifo_in,
              tapa::ostream<float_v16>& fifo_out) {
  for (;;) {
#pragma HLS pipeline II = 1
    bool flag_nop = fifo_out.full();
    for (int i = 0; i < 8; ++i) {
      flag_nop |= fifo_in[i].empty();
    }
    if (!flag_nop) {
      float_v16 tmpv16;
#pragma HLS aggregate variable = tmpv16
      for (int i = 0; i < 8; ++i) {
        float_v2 tmp;
        fifo_in[i].try_read(tmp);
        for (int d = 0; d < 2; ++d) {
          tmpv16[i * 2 + d] = tmp[d];
        }
      }
      fifo_out.try_write(tmpv16);
    }
  }
}

void FloatvMultConst(const int P_N, const int M, const int alpha_u,
                     tapa::istream<float_v16>& fifo_in,
                     tapa::ostream<float_v16>& fifo_out) {
  const float alpha_f = tapa::bit_cast<float>(alpha_u);
  const int rp_time = (P_N == 0) ? 1 : P_N;
  const int num_ite_Y = ((M + 15) >> 4) * rp_time;
cc:
  for (int i = 0; i < num_ite_Y;) {
#pragma HLS pipeline II = 1
    float_v16 tmp;
    bool read_ready = fifo_in.try_read(tmp);
    if (read_ready) {
      float_v16 c_out = tmp * alpha_f;
      fifo_out.write(c_out);
      ++i;
    }
  }
}

void read_Y(const int P_N, const int M, tapa::async_mmap<float_v16>& Y,
            tapa::ostream<float_v16>& fifo_Y) {
  const int rp_time = (P_N == 0) ? 1 : P_N;
  const int num_ite_Y = (M + 15) >> 4;

l_rp:
  for (int rp = 0; rp < rp_time; rp++) {
#pragma HLS loop_flatten off
#pragma HLS loop_tripcount min = 1 max = 16
  rd_Y:
    for (int i_req = 0, i_resp = 0; i_resp < num_ite_Y;) {
#pragma HLS loop_tripcount min = 1 max = 500000
#pragma HLS pipeline II = 1
      async_read(Y, fifo_Y, num_ite_Y, i_req, i_resp);
    }
  }
}

void FloatvAddFloatv(tapa::istream<float_v16>& fifo_in0,
                     tapa::istream<float_v16>& fifo_in1,
                     tapa::ostream<float_v16>& fifo_out) {
cc:
  for (;;) {
#pragma HLS pipeline II = 1
    bool flag_nop = fifo_in0.empty() | fifo_in1.empty();
    if (!flag_nop) {
      float_v16 tmp0;
      fifo_in0.try_read(tmp0);
      float_v16 tmp1;
      fifo_in1.try_read(tmp1);
      float_v16 c_out = tmp0 + tmp1;
      fifo_out.write(c_out);
    }
  }
}

void write_Y(const int P_N, const int M, tapa::istream<float_v16>& fifo_Y,
             tapa::async_mmap<float_v16>& Y_out) {
  const int rp_time = (P_N == 0) ? 1 : P_N;
  const int num_ite_Y = (M + 15) >> 4;

l_rp:
  for (int rp = 0; rp < rp_time; rp++) {
#pragma HLS loop_flatten off
#pragma HLS loop_tripcount min = 1 max = 16
  wr_C:
    for (int i_req = 0, i_resp = 0; i_resp < num_ite_Y;) {
#pragma HLS loop_tripcount min = 1 max = 500000
#pragma HLS pipeline II = 1
      if ((i_req < num_ite_Y) & !fifo_Y.empty() & !Y_out.write_addr.full() &
          !Y_out.write_data.full()) {
        Y_out.write_addr.try_write(i_req);
        float_v16 tmpv16;
        fifo_Y.try_read(tmpv16);
        Y_out.write_data.try_write(tmpv16);
        ++i_req;
      }
      uint8_t n_resp;
      if (Y_out.write_resp.try_read(n_resp)) {
        i_resp += int(n_resp) + 1;
      }
    }
  }
}

void black_hole_int(tapa::istream<int>& fifo_in) {
  for (;;) {
#pragma HLS pipeline II = 1
    int tmp;
    fifo_in.try_read(tmp);
  }
}

void black_hole_float_v16(tapa::istream<float_v16>& fifo_in) {
  for (;;) {
#pragma HLS pipeline II = 1
    float_v16 tmp;
    fifo_in.try_read(tmp);
  }
}

void Serpens(tapa::mmap<int> edge_list_ptr,

             tapa::mmaps<ap_uint<512>, NUM_CH_SPARSE> edge_list_ch,

             tapa::mmap<float_v16> vec_X,

             tapa::mmap<float_v16> vec_Y,

             tapa::mmap<float_v16> vec_Y_out,

             const int NUM_ITE, const int NUM_A_LEN, const int M, const int K,
             const int P_N, const int alpha_u, const int beta_u) {
  tapa::streams<int, NUM_CH_SPARSE + 1, FIFO_DEPTH> PE_inst("PE_inst");

  tapa::streams<float_v16, NUM_CH_SPARSE + 1, FIFO_DEPTH> fifo_X_pe(
      "fifo_X_pe");

  tapa::streams<ap_uint<512>, NUM_CH_SPARSE, FIFO_DEPTH> fifo_A("fifo_A");

  tapa::streams<int, NUM_CH_SPARSE, FIFO_DEPTH> Yvec_inst("Yvec_inst");

  tapa::streams<MultXVec, NUM_CH_SPARSE, FIFO_DEPTH> fifo_aXvec("fifo_aXvec");

  tapa::streams<float_v2, NUM_CH_SPARSE, FIFO_DEPTH> fifo_Y_pe("fifo_Y_pe");

  tapa::streams<float_v2, 8, FIFO_DEPTH> fifo_Y_pe_abd("fifo_Y_pe_abd");

  tapa::stream<float_v16, FIFO_DEPTH> fifo_Y_AX("fifo_Y_AX");

  tapa::stream<float_v16, FIFO_DEPTH> fifo_Y_alpha_AX("fifo_Y_alpha_AX");

  tapa::stream<float_v16, FIFO_DEPTH> fifo_Y_in("fifo_Y_in");

  tapa::stream<float_v16, FIFO_DEPTH> fifo_Y_in_beta("fifo_Y_in_beta");

  tapa::stream<float_v16, FIFO_DEPTH> fifo_Y_out("fifo_Y_out");

  /* =========deploy modules======= */

  tapa::task()
      .invoke(read_edge_list_ptr, NUM_ITE, M, P_N, K, edge_list_ptr, PE_inst)

      .invoke<tapa::join>(read_X, P_N, K, vec_X, fifo_X_pe)

      .invoke<tapa::join, NUM_CH_SPARSE>(read_A, P_N, NUM_A_LEN, edge_list_ch,
                                         fifo_A)

      .invoke<tapa::join, NUM_CH_SPARSE>(PEG_Xvec, PE_inst, fifo_A, fifo_X_pe,
                                         PE_inst, fifo_X_pe, Yvec_inst,
                                         fifo_aXvec)
      .invoke<tapa::detach>(black_hole_int, PE_inst)
      .invoke<tapa::detach>(black_hole_float_v16, fifo_X_pe)

      .invoke<tapa::join, NUM_CH_SPARSE>(PEG_Yvec, Yvec_inst, fifo_aXvec,
                                         fifo_Y_pe)

      .invoke<tapa::join, 8>(Arbiter_Y, P_N, M, fifo_Y_pe, fifo_Y_pe_abd)

      .invoke<tapa::detach>(Merger_Y, fifo_Y_pe_abd, fifo_Y_AX)

      .invoke<tapa::join>(FloatvMultConst, P_N, M, alpha_u, fifo_Y_AX,
                          fifo_Y_alpha_AX)

      .invoke<tapa::join>(read_Y, P_N, M, vec_Y, fifo_Y_in)

      .invoke<tapa::join>(FloatvMultConst, P_N, M, beta_u, fifo_Y_in,
                          fifo_Y_in_beta)

      .invoke<tapa::detach>(FloatvAddFloatv, fifo_Y_alpha_AX, fifo_Y_in_beta,
                            fifo_Y_out)

      .invoke<tapa::join>(write_Y, P_N, M, fifo_Y_out, vec_Y_out);
}
