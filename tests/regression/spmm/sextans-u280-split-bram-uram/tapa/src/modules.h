// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#ifndef MODULES_H
#define MODULES_H

#include <ap_int.h>
#include <tapa.h>

// #include <iostream>
// using namespace std;

#include "sextans.h"

template <typename T, typename R>
void async_read(tapa::async_mmap<T>& A, tapa::ostream<T>& fifo_A, const R A_len,
                R& i_req, R& i_resp) {
#pragma HLS inline
  if (i_req < A_len && i_req < i_resp + 64 && A.read_addr.try_write(i_req)) {
    ++i_req;
  }
  if (!fifo_A.full() && !A.read_data.empty()) {
    fifo_A.try_write(A.read_data.read(nullptr));
    ++i_resp;
  }
}

void Relay_int(tapa::istream<int>& fifo_in, tapa::ostream<int>& fifo_out,
               tapa::ostream<int>& fifo_pe) {
  for (;;) {
#pragma HLS pipeline II = 1
    bool flag_nop = fifo_in.empty() | fifo_out.full() | fifo_pe.full();
    if (!flag_nop) {
      int tmp = fifo_in.read();
      fifo_out.write(tmp);
      fifo_pe.write(tmp);
    }
  }
}

void Relay_float16(tapa::istream<float_v16>& fifo_in,
                   tapa::ostream<float_v16>& fifo_out,
                   tapa::ostream<float_v16>& fifo_pe) {
  for (;;) {
#pragma HLS pipeline II = 1
    bool flag_nop = fifo_in.empty() | fifo_out.full() | fifo_pe.full();
    if (!flag_nop) {
      float_v16 tmp;
#pragma HLS aggregate variable = tmp
      tmp = fifo_in.read();
      fifo_out.write(tmp);
      fifo_pe.write(tmp);
    }
  }
}

void read_edge_list_ptr(
    const int num_ite, const int M,
    const int P_N,  // bit 31 - 16: repeat time, bit 15 - 0: N
    const int K, tapa::async_mmap<int> edge_list_ptr,
    tapa::ostream<int>& fifo_edge_list_ptr, tapa::ostream<int>& PE_inst) {
  PE_inst.write(num_ite);
  PE_inst.write(M);
  PE_inst.write(P_N);
  PE_inst.write(K);

  const int N = P_N & 0xFFFF;
  const int N16 = P_N >> 16;
  const int rp_time = (N16 == 0) ? 1 : N16;

  const int num_ite_plus1 = num_ite + 1;
  const int rp_time_N = rp_time * ((N + 7) >> 3);

l_rp:
  for (int rp = 0; rp < rp_time_N; rp++) {
#pragma HLS loop_flatten off
#pragma HLS loop_tripcount min = 1 max = 16
  rd_ptr:
    for (int i_req = 0, i_resp = 0; i_resp < num_ite_plus1;) {
#pragma HLS loop_tripcount min = 1 max = 800
#pragma HLS pipeline II = 1
      async_read(edge_list_ptr, fifo_edge_list_ptr, num_ite_plus1, i_req,
                 i_resp);
    }
  }
}

void read_A(tapa::async_mmap<ap_uint<512>> A,
            tapa::ostream<ap_uint<512>>& fifo_A, const int A_len,
            const int P_N) {
  const int N16 = P_N >> 16;
  const int rp_time = (N16 == 0) ? 1 : N16;
  const int N = P_N & 0xFFFF;

  const int rp_time_N = rp_time * ((N + 7) >> 3);

l_rp:
  for (int rp = 0; rp < rp_time_N; rp++) {
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

void read_B(tapa::async_mmap<float_v16> B, tapa::ostream<float_v16>& fifo_B,
            const int K, const int P_N) {
  const int N16 = P_N >> 16;
  const int rp_time = (N16 == 0) ? 1 : N16;
  const int N = P_N & 0xFFFF;
  const int num_ite_B = ((K + 7) >> 3) * ((N + 7) >> 3);

l_rp:
  for (int rp = 0; rp < rp_time; rp++) {
#pragma HLS loop_flatten off
#pragma HLS loop_tripcount min = 1 max = 16
  rd_B:
    for (int i_req = 0, i_resp = 0; i_resp < num_ite_B;) {
#pragma HLS loop_tripcount min = 1 max = 500000
#pragma HLS pipeline II = 1
      async_read(B, fifo_B, num_ite_B, i_req, i_resp);
    }
  }
}

void read_C(tapa::async_mmap<float_v16> C, tapa::ostream<float_v16>& fifo_C,
            const int M, const int P_N, tapa::ostream<int>& wrC_inst) {
  wrC_inst.write(M);
  wrC_inst.write(P_N);

  const int N16 = P_N >> 16;
  const int rp_time = (N16 == 0) ? 1 : N16;
  const int N = P_N & 0xFFFF;
  const int num_ite_C = ((M + 15) >> 4) * ((N + 7) >> 3);

l_rp:
  for (int rp = 0; rp < rp_time; rp++) {
#pragma HLS loop_flatten off
#pragma HLS loop_tripcount min = 1 max = 16
  rd_C:
    for (int i_req = 0, i_resp = 0; i_resp < num_ite_C;) {
#pragma HLS loop_tripcount min = 1 max = 500000
#pragma HLS pipeline II = 1
      async_read(C, fifo_C, num_ite_C, i_req, i_resp);
    }
  }
}

void write_C(tapa::istream<int>& wrC_inst, tapa::istream<float_v16>& fifo_C,
             tapa::async_mmap<float_v16> C_out) {
  int M = wrC_inst.read();
  int P_N = wrC_inst.read();

  const int N16 = P_N >> 16;
  const int rp_time = (N16 == 0) ? 1 : N16;
  const int N = P_N & 0xFFFF;
  const int num_ite_C = ((M + 15) >> 4) * ((N + 7) >> 3);

l_rp:
  for (int rp = 0; rp < rp_time; rp++) {
#pragma HLS loop_flatten off
#pragma HLS loop_tripcount min = 1 max = 16
  wr_C:
    for (int i_req = 0, i_resp = 0; i_resp < num_ite_C;) {
#pragma HLS loop_tripcount min = 1 max = 500000
#pragma HLS pipeline II = 1
      if (i_req < num_ite_C && i_req < i_resp + 64 && !fifo_C.empty() &&
          !C_out.write_addr.full() && !C_out.write_data.full()) {
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

/*
void C_collect(const int M,
               tapa::istreams<float_v16, NUM_CH_SPARSE> & fifo_C_in,
               tapa::ostreams<float_v16, NUM_CH_C> & fifo_C_out
               ) {
    const int T_consume = ((M + 63) >> 6) << 2;
    const int T_send = (M + 15) >> 4;
    int i = 0;
cvt_c:
    for(;;) {
#pragma HLS loop_tripcount min=1 max=800
#pragma HLS pipeline II=1
        bool all_c_ready = true;
        for (int j = 0; j < NUM_CH_SPARSE; ++j) {
            all_c_ready &= !fifo_C_in[j].empty();
        }
        if (all_c_ready) {
            //cout << "collect C" << endl;
            float_v16 out_c[NUM_CH_C];
#pragma HLS aggregate variable=out_c
            for (int j = 0; j < NUM_CH_SPARSE; ++j) {
                float_v16 tmp_c = fifo_C_in[j].read(nullptr);
                for (int k = 0; k < NUM_CH_C; ++k) {
                    //out_c[k](64*j+63, 64*j) = tmp_c(64*k+63, 64*k);
                    out_c[k][2 * j] = tmp_c[2 * k];
                    out_c[k][2 * j + 1] = tmp_c[2 * k + 1];
                }
            }

            if (i < T_send) {
                for (int j = 0; j < NUM_CH_C; ++j) {
                    fifo_C_out[j].write(out_c[j]);
                }
            }

            ++i;
            if (i == T_consume) {
                i = 0;
            }
        }
    }
}
*/

void comp_C(const int alpha_u, const int beta_u,
            tapa::istream<float_v16>& fifo_C_read_in,
            tapa::istream<float_v16>& fifo_C_pe_in,
            tapa::ostream<float_v16>& fifo_C_out) {
  float beta_f = tapa::bit_cast<float>(beta_u);
  float alpha_f = tapa::bit_cast<float>(alpha_u);

  float_v16 c_read;
  float_v16 c_pe;

  bool c_read_ready = false;
  bool c_pe_ready = false;

cc:
  for (;;) {
#pragma HLS loop_tripcount min = 1 max = 5000
#pragma HLS pipeline II = 1
    if (!c_read_ready) {
      c_read_ready = fifo_C_read_in.try_read(c_read);
    }
    if (!c_pe_ready) {
      c_pe_ready = fifo_C_pe_in.try_read(c_pe);
    }
    if (c_read_ready && c_pe_ready) {
      float_v16 c_out = c_pe * alpha_f + c_read * beta_f;
      fifo_C_out.write(c_out);
      c_read_ready = false;
      c_pe_ready = false;
    }
  }
}

void PU2core(ap_uint<18>& addr_c, float a_val_f, float b_val_d0_f,
             float b_val_d1_f, ap_uint<64> local_C_pe0_d0_d1[URAM_DEPTH]) {
#pragma HLS inline
  ap_uint<64> c_val_d0_d1_u64 = local_C_pe0_d0_d1[addr_c];

  ap_uint<32> c_val_d0_u = c_val_d0_d1_u64(31, 0);
  ap_uint<32> c_val_d1_u = c_val_d0_d1_u64(63, 32);

  float c_val_d0_f = tapa::bit_cast<float>(c_val_d0_u);
  float c_val_d1_f = tapa::bit_cast<float>(c_val_d1_u);

  c_val_d0_f += tapa::reg(a_val_f) * b_val_d0_f;
  c_val_d1_f += tapa::reg(a_val_f) * b_val_d1_f;

  c_val_d0_u = tapa::bit_cast<ap_uint<32>>(c_val_d0_f);
  c_val_d1_u = tapa::bit_cast<ap_uint<32>>(c_val_d1_f);

  c_val_d0_d1_u64(31, 0) = c_val_d0_u;
  c_val_d0_d1_u64(63, 32) = c_val_d1_u;

  local_C_pe0_d0_d1[addr_c] = c_val_d0_d1_u64;
}

void PEcore(ap_uint<14>& addr_b, ap_uint<18>& addr_c, ap_uint<32>& a_val_u,
            ap_uint<64> local_C[4][URAM_DEPTH], float local_B[8][WINDOW_SIZE]) {
#pragma HLS inline
  // if (addr_c != ((ap_uint<18>) 0x3FFFF)) {
  if (addr_c[17] == 0) {
    float a_val_f = tapa::bit_cast<float>(a_val_u);
    for (int i = 0; i < 4; ++i) {
      PU2core(addr_c, a_val_f, local_B[i * 2 + 0][addr_b],
              local_B[i * 2 + 1][addr_b], local_C[i]);
    }
  }
}

void PEG(
    tapa::istream<int>& PE_inst, tapa::istream<int>& fifo_inst,
    tapa::istream<ap_uint<128>>& fifo_A,
    tapa::istreams<float_v16, NUM_CH_B>& fifo_B,  // [256(16)] * 2, 2: dim d
    // [64(32bits * 2.0)] * 8 dim
    tapa::ostream<float_v8>& fifo_C_out) {
  const int NUM_ITE = PE_inst.read();
  const int M = PE_inst.read();
  const int P_N = PE_inst.read();
  const int K = PE_inst.read();

  const int N16 = P_N >> 16;
  const int rp_time = (N16 == 0) ? 1 : N16;
  const int N = P_N & 0xFFFF;
  const int rp_time_N = rp_time * ((N + 7) >> 3);

  const int num_v_init = (M + 63) >> 6;
  const int num_v_out = (M + 31) >> 5;

  // define local C buffer and pragma to URAM
  // ap_uint<64> local_C[8][8 / 2][URAM_DEPTH];
  ap_uint<64> local_C[2][8 / 2][URAM_DEPTH];
#pragma HLS bind_storage variable = local_C type = RAM_2P impl = \
    URAM latency = 1
#pragma HLS array_partition complete variable = local_C dim = 1
#pragma HLS array_partition complete variable = local_C dim = 2

l_rp:
  for (int rp = 0; rp < rp_time_N; rp++) {
#pragma HLS loop_flatten off
#pragma HLS loop_tripcount min = 1 max = 16

    // init local C
  init_C:
    for (int i = 0; i < num_v_init; ++i) {
#pragma HLS loop_tripcount min = 1 max = 800
#pragma HLS pipeline II = 1
      for (int j = 0; j < 2; ++j) {
        for (int k = 0; k < 8 / 2; ++k) {
          local_C[j][k][i] = 0;
        }
      }
    }
    // define local B buffer and pragma local B buffer if partition factor > 1

    // float local_B[8/2][8][WINDOW_SIZE];
    float local_B[8][WINDOW_SIZE];
#pragma HLS bind_storage variable = local_B latency = 2
#pragma HLS array_partition variable = local_B complete dim = 1
    // #pragma HLS array_partition variable=local_B complete dim=2
    // #pragma HLS array_partition variable=local_B cyclic
    // factor=B_PARTITION_FACTOR dim=3
#pragma HLS array_partition variable = local_B cyclic factor = \
    B_PARTITION_FACTOR dim = 2

    auto start_32 = fifo_inst.read();

  main:
    for (int i = 0; i < NUM_ITE; ++i) {
#pragma HLS loop_tripcount min = 1 max = 49

      // fill onchip B
    read_B:
      for (int j = 0; (j < (WINDOW_SIZE >> 3)) &&
                      (j < ((K + 7) >> 3) - i * (WINDOW_SIZE >> 3));) {
#pragma HLS loop_tripcount min = 1 max = 512
#pragma HLS pipeline II = 1

        bool b_2048_ready = true;
        for (int k = 0; k < NUM_CH_B; ++k) {
          b_2048_ready &= !fifo_B[k].empty();
        }

        if (b_2048_ready) {
          float_v16 b_512_x[NUM_CH_B];
          for (int k = 0; k < NUM_CH_B; ++k) {
            b_512_x[k] = fifo_B[k].read(nullptr);
          }

          for (int k = 0; k < 8; ++k) {
            for (int m = 0; m < 8; ++m) {
              local_B[m][j * 8 + k] = b_512_x[m / 2][k + m % 2 * 8];
            }
          }
          ++j;
        }
      }

      // computation
      const auto end_32 = fifo_inst.read();

    computation:
      for (int j = start_32; j < end_32;) {
#pragma HLS loop_tripcount min = 1 max = 200
#pragma HLS pipeline II = 1
#pragma HLS dependence true variable = local_C distance = DEP_DIST_LOAD_STORE

        ap_uint<128> a_pes;
        bool a_pes_ready = fifo_A.try_read(a_pes);

        if (a_pes_ready) {
          for (int p = 0; p < 2; ++p) {
            ap_uint<14> a_col;
            ap_uint<18> a_row;
            ap_uint<32> a_val;

            ap_uint<64> a = a_pes(63 + p * 64, p * 64);
            a_col = a(63, 50);
            a_row = a(49, 32);
            a_val = a(31, 0);

            // PE process
            PEcore(a_col, a_row, a_val, local_C[p], local_B);
          }
          ++j;
        }
      }
      start_32 = end_32;
    }

    // cout << "PE = " << pe_idx << endl;
  write_C_outer:
    for (int i = 0; i < num_v_out; ++i) {
#pragma HLS loop_tripcount min = 1 max = 1800
#pragma HLS pipeline II = 1
      ap_uint<32> u_32_d[8];

      for (int d = 0; d < 4; ++d) {
        ap_uint<64> u_64 = local_C[i % 2][d][i >> 1];
        u_32_d[2 * d] = u_64(31, 0);
        u_32_d[2 * d + 1] = u_64(63, 32);
      }

      float_v8 out_v;
      for (int d = 0; d < 8; ++d) {
        out_v[d] = tapa::bit_cast<float>(u_32_d[d]);
      }
      fifo_C_out.write(out_v);
      // for (int ii = 0; ii < 8; ++ii) {cout << out_v[ii] << " ";} cout <<
      // endl;
    }
  }
}

void Scatter_1_4(tapa::istream<ap_uint<512>>& fifo_in,
                 tapa::ostreams<ap_uint<128>, 4>& fifo_out) {
  for (;;) {
#pragma HLS pipeline II = 1
    bool flag_nop = fifo_in.empty();
    for (int i = 0; i < 4; ++i) {
      flag_nop |= fifo_out[i].full();
    }
    if (!flag_nop) {
      ap_uint<512> tmp = fifo_in.read();
      for (int i = 0; i < 4; ++i) {
        fifo_out[i].write(tmp(127 + i * 128, i * 128));
      }
    }
  }
}

void Merger(tapa::istreams<float_v8, 2>& fifo_in,
            tapa::ostream<float_v16>& fifo_out) {
  for (;;) {
#pragma HLS pipeline II = 1
    bool flag_nop = fifo_out.full() | fifo_in[0].empty() | fifo_in[1].empty();
    if (!flag_nop) {
      float_v16 tmpv16;
      float_v8 tmpv8[2];
#pragma HLS aggregate variable = tmpv16
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

void Selector(const int M, tapa::istreams<float_v8, 2>& fifo_in,
              tapa::ostream<float_v8>& fifo_out) {
  const int T_consume = ((M + 31) >> 5) << 1;
  const int T_send = (M + 15) >> 4;
  int i = 0;
  int ch = 0;

  for (;;) {
#pragma HLS pipeline II = 1
    bool flag_nop = fifo_out.full() | fifo_in[ch].empty();
    if (!flag_nop) {
      float_v8 tmp;
#pragma HLS aggregate variable = tmp
      tmp = fifo_in[ch].read();

      if (i < T_send) {
        fifo_out.write(tmp);
      }

      ++i;
      ch = 1 - ch;

      if (i == T_consume) {
        i = 0;
      }
    }
  }
}

#endif
