// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include <ap_int.h>
#include <hls_stream.h>
#include <cassert>
#include <cstdio>
#include <cstring>

// #define DEBUG_PRINT_KERNEL

#ifdef DEBUG_PRINT_KERNEL
#include <iostream>
using std::cout;
using std::endl;
#endif

const int WINDOW_SIZE = 8192;
const int WINDOW_SIZE_DIV_16 = 512;
const int DEP_DIST_LOAD_STORE = 10;
const int B_PARTITION_FACTOR = 8;
const int URAM_DEPTH = 8192;

template <class T>
T HLS_REG(T in) {
#pragma HLS inline off
#pragma HLS pipeline II = 1
#pragma HLS LATENCY min = 1 max = 1
  return in;
}

float uint32_to_float(ap_uint<32> u) {
#pragma HLS inline
  float* tmpPointer_v = (float*)&u;
  return (*tmpPointer_v);
}

ap_uint<32> float_to_uint32(float u) {
#pragma HLS inline
  ap_uint<32>* tmpPointer_v = (ap_uint<32>*)&u;
  return (*tmpPointer_v);
}

void read_edge_list_ptr(const ap_uint<32> num_ite, const ap_uint<32> M,
                        const ap_uint<16> rp_time, const ap_uint<32> K,
                        const ap_uint<32> alpha_u,
                        const ap_uint<32>* edge_list_ptr,
                        hls::stream<ap_uint<32> >& fifo_edge_list_ptr) {
#pragma HLS inline off
  fifo_edge_list_ptr.write(num_ite);
  fifo_edge_list_ptr.write(M);
  const ap_uint<32> P32 = ((ap_uint<32>)rp_time) << 16;
  fifo_edge_list_ptr.write(P32);
  fifo_edge_list_ptr.write(K);
  fifo_edge_list_ptr.write(alpha_u);

l_rp:
  for (ap_uint<16> rp = 0; rp < rp_time; rp++) {
#pragma HLS loop_flatten off
#pragma HLS loop_tripcount min = 1 max = 16
  rd_ptr:
    for (ap_uint<32> i = 0; i < num_ite + 1; i++) {
#pragma HLS loop_tripcount min = 1 max = 800
#pragma HLS pipeline II = 1
      ap_uint<32> tmp = edge_list_ptr[i];
      fifo_edge_list_ptr.write(tmp);
    }
  }
}

void read_A(const ap_uint<512>* A, hls::stream<ap_uint<512> >& fifo_A,
            const ap_uint<32> A_len, const ap_uint<16> rp_time) {
#pragma HLS inline off
l_rp:
  for (ap_uint<16> rp = 0; rp < rp_time; rp++) {
#pragma HLS loop_flatten off
#pragma HLS loop_tripcount min = 1 max = 16
  rd_A:
    for (ap_uint<32> i = 0; i < A_len; i++) {
#pragma HLS loop_tripcount min = 1 max = 10000
#pragma HLS pipeline II = 1
      ap_uint<512> tmp_A = A[i];
      fifo_A.write(tmp_A);
    }
  }
}

void read_X(const ap_uint<512>* X, hls::stream<ap_uint<512> >& fifo_X,
            const ap_uint<32> K, const ap_uint<16> rp_time) {
#pragma HLS inline off
  const ap_uint<32> num_ite_X = ((K + 15) >> 4);

l_rp:
  for (ap_uint<16> rp = 0; rp < rp_time; rp++) {
#pragma HLS loop_flatten off
#pragma HLS loop_tripcount min = 1 max = 16
  rd_X:
    for (ap_uint<32> i = 0; i < num_ite_X; i++) {
#pragma HLS loop_tripcount min = 1 max = 500000
#pragma HLS pipeline II = 1
      ap_uint<512> tmp_X = X[i];
      fifo_X.write(tmp_X);
    }
  }
}

void PUcore(ap_uint<18>& addr_c, float& a_val_f, float& b_val_f,
            ap_uint<64>* local_C_pe0) {
#pragma HLS inline
  ap_uint<64> c_val_u64 = local_C_pe0[addr_c(17, 1)];

  ap_uint<32> c_val_d0_u = c_val_u64(31, 0);
  ap_uint<32> c_val_d1_u = c_val_u64(63, 32);

  ap_uint<32> c_val_u = (addr_c[0]) ? c_val_d1_u : c_val_d0_u;
  float c_val_f = uint32_to_float(c_val_u);
  c_val_f += HLS_REG(a_val_f) * b_val_f;
  c_val_u = float_to_uint32(c_val_f);

  if (addr_c[0]) {
    c_val_d1_u = c_val_u;
  } else {
    c_val_d0_u = c_val_u;
  }
  c_val_u64(63, 32) = c_val_d1_u;
  c_val_u64(31, 0) = c_val_d0_u;
  local_C_pe0[addr_c(17, 1)] = c_val_u64;
}

void PEcore(ap_uint<14>& addr_b, ap_uint<18>& addr_c, ap_uint<32>& a_val_u,

            ap_uint<64>* local_C_pe0,

            ap_uint<32>* local_B_pe0_pe1) {
#pragma HLS inline
  if (addr_c != ((ap_uint<18>)0x3FFFF)) {
    float a_val_f = uint32_to_float(a_val_u);
    ap_uint<32> b_val_u = local_B_pe0_pe1[addr_b];
    float b_val_f = uint32_to_float(b_val_u);

    PUcore(addr_c, a_val_f, b_val_f, local_C_pe0);
  }
}

void peg2mult(ap_uint<64> opa64, ap_uint<32> alpha_u, ap_uint<64>& mult64) {
#pragma HLS inline
  float alpha_f = uint32_to_float(alpha_u);
  ap_uint<64> c_out;

  float op_a[2];
#pragma HLS array_partition variable = op_a complete
  float op_result[2];
#pragma HLS array_partition variable = op_result complete

  for (ap_uint<2> p = 0; p < 2; ++p) {
    op_a[p] = uint32_to_float(opa64(31 + p * 32, p * 32));
    op_result[p] = HLS_REG(alpha_f) * op_a[p];
    c_out(31 + p * 32, p * 32) = float_to_uint32(op_result[p]);
  }
  mult64 = HLS_REG(c_out);
}

template <bool SEND_META>
void PEG(hls::stream<ap_uint<32> >& fifo_inst,
         hls::stream<ap_uint<512> >& fifo_A,
         hls::stream<ap_uint<512> >& fifo_X,        // 16 FP32
         hls::stream<ap_uint<32> >& fifo_inst_out,  // to next PE
         hls::stream<ap_uint<512> >& fifo_X_out,    // to next PE, 16 FP32
         hls::stream<ap_uint<64> >& fifo_C_out) {
#pragma HLS inline off
  ap_uint<32> NUM_ITE;
  ap_uint<32> M;
  ap_uint<32> P32;
  ap_uint<32> K;
  ap_uint<32> alpha_u;

  ap_uint<32> parameter;
w_ITE:
  for (ap_uint<3> i = 0; i < 5;) {
#pragma HLS loop_tripcount min = 1 max = 10
#pragma HLS pipeline II = 1
    bool parameter_ready = fifo_inst.read_nb(parameter);
    if (parameter_ready) {
      ap_uint<32> parameter_delay = HLS_REG(parameter);
      switch (i) {
        case 0:
          NUM_ITE = parameter_delay;
          break;
        case 1:
          M = parameter_delay;
          break;
        case 2:
          P32 = parameter_delay;
          break;
        case 3:
          K = parameter_delay;
          break;
        case 4:
          alpha_u = parameter_delay;
          break;
      }
      ++i;
      fifo_inst_out.write(parameter_delay);
    }
  }

  const ap_uint<16> rp_time = P32(31, 16);

  if (SEND_META) {
    ap_uint<64> meta64;
    meta64(31, 0) = M;
    meta64(63, 32) = P32;
    fifo_C_out.write(meta64);
  }

  // define local C buffer and bind to URAM
  ap_uint<64> local_C_pe0[URAM_DEPTH];
  ap_uint<64> local_C_pe1[URAM_DEPTH];
  ap_uint<64> local_C_pe2[URAM_DEPTH];
  ap_uint<64> local_C_pe3[URAM_DEPTH];
  ap_uint<64> local_C_pe4[URAM_DEPTH];
  ap_uint<64> local_C_pe5[URAM_DEPTH];
  ap_uint<64> local_C_pe6[URAM_DEPTH];
  ap_uint<64> local_C_pe7[URAM_DEPTH];

#pragma HLS bind_storage variable = local_C_pe0 type = RAM_2P impl = URAM
#pragma HLS bind_storage variable = local_C_pe1 type = RAM_2P impl = URAM
#pragma HLS bind_storage variable = local_C_pe2 type = RAM_2P impl = URAM
#pragma HLS bind_storage variable = local_C_pe3 type = RAM_2P impl = URAM
#pragma HLS bind_storage variable = local_C_pe4 type = RAM_2P impl = URAM
#pragma HLS bind_storage variable = local_C_pe5 type = RAM_2P impl = URAM
#pragma HLS bind_storage variable = local_C_pe6 type = RAM_2P impl = URAM
#pragma HLS bind_storage variable = local_C_pe7 type = RAM_2P impl = URAM

l_rp:
  for (ap_uint<16> rp = 0; rp < rp_time; rp++) {
#pragma HLS loop_flatten off
#pragma HLS loop_tripcount min = 1 max = 16

    // init local C
    const ap_uint<32> M_ITE_INIT = (M + 511) >> 9;
  init_C:
    for (ap_uint<32> i = 0; i < M_ITE_INIT; ++i) {
#pragma HLS loop_tripcount min = 1 max = 800
#pragma HLS pipeline II = 1
      local_C_pe0[i] = 0;
      local_C_pe1[i] = 0;
      local_C_pe2[i] = 0;
      local_C_pe3[i] = 0;
      local_C_pe4[i] = 0;
      local_C_pe5[i] = 0;
      local_C_pe6[i] = 0;
      local_C_pe7[i] = 0;
    }

    // define local B buffer and pragma local B buffer if partition factor > 1
    ap_uint<32> local_B_pe0_pe1[WINDOW_SIZE];
    ap_uint<32> local_B_pe2_pe3[WINDOW_SIZE];
    ap_uint<32> local_B_pe4_pe5[WINDOW_SIZE];
    ap_uint<32> local_B_pe6_pe7[WINDOW_SIZE];

#pragma HLS bind_storage variable = local_B_pe0_pe1 latency = 2
#pragma HLS bind_storage variable = local_B_pe2_pe3 latency = 2
#pragma HLS bind_storage variable = local_B_pe4_pe5 latency = 2
#pragma HLS bind_storage variable = local_B_pe6_pe7 latency = 2

#pragma HLS array_partition variable = local_B_pe0_pe1 cyclic factor = \
    B_PARTITION_FACTOR
#pragma HLS array_partition variable = local_B_pe2_pe3 cyclic factor = \
    B_PARTITION_FACTOR
#pragma HLS array_partition variable = local_B_pe4_pe5 cyclic factor = \
    B_PARTITION_FACTOR
#pragma HLS array_partition variable = local_B_pe6_pe7 cyclic factor = \
    B_PARTITION_FACTOR

    ap_uint<32> start_32_in;
    bool start_32_in_ready = false;
  w1:
    while (!start_32_in_ready) {
#pragma HLS loop_tripcount min = 1 max = 10
#pragma HLS pipeline II = 1
      start_32_in_ready = fifo_inst.read_nb(start_32_in);
    }
    ap_uint<32> start_32 = HLS_REG(start_32_in);

    fifo_inst_out.write(start_32);

  main:
    for (ap_uint<32> i = 0; i < NUM_ITE; ++i) {
#pragma HLS loop_tripcount min = 1 max = 49

      // fill onchip X
    read_X:
      for (ap_uint<14> j = 0; (j < WINDOW_SIZE_DIV_16) &&
                              (j < ((K + 15) >> 4) - i * WINDOW_SIZE_DIV_16);) {
#pragma HLS loop_tripcount min = 1 max = 1024
#pragma HLS pipeline II = 1
        ap_uint<512> x_512;
        bool x_512_ready = fifo_X.read_nb(x_512);
        ;

        if (x_512_ready) {
          ap_uint<512> x_512_delay = HLS_REG(HLS_REG(x_512));
          fifo_X_out.write(x_512_delay);
        fill_X_p:
          for (ap_uint<5> k = 0; k < 16; ++k) {
            local_B_pe0_pe1[HLS_REG(HLS_REG(j)) * 16 + k] =
                x_512_delay(k * 32 + 31, k * 32);
            local_B_pe2_pe3[HLS_REG(HLS_REG(j)) * 16 + k] =
                x_512_delay(k * 32 + 31, k * 32);
            local_B_pe4_pe5[HLS_REG(HLS_REG(j)) * 16 + k] =
                x_512_delay(k * 32 + 31, k * 32);
            local_B_pe6_pe7[HLS_REG(HLS_REG(j)) * 16 + k] =
                x_512_delay(k * 32 + 31, k * 32);
          }
          ++j;
        }
      }

      // computation
      ap_uint<32> end_32_in;
      bool end_32_ready = false;
    w2:
      while (!end_32_ready) {
#pragma HLS loop_tripcount min = 1 max = 10
#pragma HLS pipeline II = 1
        end_32_ready = fifo_inst.read_nb(end_32_in);
      }
      ap_uint<32> end_32 = HLS_REG(end_32_in);

      fifo_inst_out.write(end_32);

    computation:
      for (ap_uint<32> j = start_32; j < end_32;) {
#pragma HLS loop_tripcount min = 1 max = 200
#pragma HLS pipeline II = 1

#pragma HLS dependence true variable = local_C_pe0 distance = \
    DEP_DIST_LOAD_STORE
#pragma HLS dependence true variable = local_C_pe1 distance = \
    DEP_DIST_LOAD_STORE
#pragma HLS dependence true variable = local_C_pe2 distance = \
    DEP_DIST_LOAD_STORE
#pragma HLS dependence true variable = local_C_pe3 distance = \
    DEP_DIST_LOAD_STORE
#pragma HLS dependence true variable = local_C_pe4 distance = \
    DEP_DIST_LOAD_STORE
#pragma HLS dependence true variable = local_C_pe5 distance = \
    DEP_DIST_LOAD_STORE
#pragma HLS dependence true variable = local_C_pe6 distance = \
    DEP_DIST_LOAD_STORE
#pragma HLS dependence true variable = local_C_pe7 distance = \
    DEP_DIST_LOAD_STORE

        ap_uint<512> a_pes;
        bool a_pes_ready = fifo_A.read_nb(a_pes);

        if (a_pes_ready) {
          ap_uint<512> a_pes_delay = HLS_REG(HLS_REG(a_pes));

          ap_uint<64> a[8];
#pragma HLS array_partition variable = a complete

          ap_uint<14> a_col[8];
#pragma HLS array_partition variable = a_col complete

          ap_uint<18> a_row[8];
#pragma HLS array_partition variable = a_row complete

          ap_uint<32> a_val[8];
#pragma HLS array_partition variable = a_val complete

          for (ap_uint<4> p = 0; p < 8; ++p) {
            a[p] = a_pes_delay(63 + p * 64, p * 64);
          }

          for (ap_uint<4> p = 0; p < 8; ++p) {
            a_col[p] = (a[p])(63, 50);
            a_row[p] = (a[p])(49, 32);
            a_val[p] = (a[p])(31, 0);
          }

          // PE process
          PEcore(a_col[0], a_row[0], a_val[0], local_C_pe0, local_B_pe0_pe1);

          PEcore(a_col[1], a_row[1], a_val[1], local_C_pe1, local_B_pe0_pe1);

          PEcore(a_col[2], a_row[2], a_val[2], local_C_pe2, local_B_pe2_pe3);

          PEcore(a_col[3], a_row[3], a_val[3], local_C_pe3, local_B_pe2_pe3);

          PEcore(a_col[4], a_row[4], a_val[4], local_C_pe4, local_B_pe4_pe5);

          PEcore(a_col[5], a_row[5], a_val[5], local_C_pe5, local_B_pe4_pe5);

          PEcore(a_col[6], a_row[6], a_val[6], local_C_pe6, local_B_pe6_pe7);

          PEcore(a_col[7], a_row[7], a_val[7], local_C_pe7, local_B_pe6_pe7);

          ++j;
        }
      }
      start_32 = end_32;
    }

    // write C fifo
  write_C_outer:
    for (ap_uint<32> i = 0; i < (M_ITE_INIT << 3); ++i) {
#pragma HLS loop_tripcount min = 1 max = 1800
#pragma HLS pipeline II = 1
      ap_uint<64> out_c;

      switch (i % 8) {
        case 0:
          out_c = local_C_pe0[i >> 3];
          break;
        case 1:
          out_c = local_C_pe1[i >> 3];
          break;
        case 2:
          out_c = local_C_pe2[i >> 3];
          break;
        case 3:
          out_c = local_C_pe3[i >> 3];
          break;
        case 4:
          out_c = local_C_pe4[i >> 3];
          break;
        case 5:
          out_c = local_C_pe5[i >> 3];
          break;
        case 6:
          out_c = local_C_pe6[i >> 3];
          break;
        case 7:
          out_c = local_C_pe7[i >> 3];
          break;
      }

      ap_uint<64> out_c_mult;
      peg2mult(out_c, alpha_u, out_c_mult);
      fifo_C_out.write(HLS_REG(out_c_mult));
    }
  }
}

template <bool SEND_META>
void PEG_last(hls::stream<ap_uint<32> >& fifo_inst,
              hls::stream<ap_uint<512> >& fifo_A,
              hls::stream<ap_uint<512> >& fifo_X,  // 16 FP32
              hls::stream<ap_uint<64> >& fifo_C_out) {
#pragma HLS inline off
  ap_uint<32> NUM_ITE;
  ap_uint<32> M;
  ap_uint<32> P32;
  ap_uint<32> K;
  ap_uint<32> alpha_u;

  ap_uint<32> parameter;
w_ITE:
  for (ap_uint<3> i = 0; i < 5;) {
#pragma HLS loop_tripcount min = 1 max = 10
#pragma HLS pipeline II = 1
    bool parameter_ready = fifo_inst.read_nb(parameter);
    if (parameter_ready) {
      ap_uint<32> parameter_delay = HLS_REG(parameter);
      switch (i) {
        case 0:
          NUM_ITE = parameter_delay;
          break;
        case 1:
          M = parameter_delay;
          break;
        case 2:
          P32 = parameter_delay;
          break;
        case 3:
          K = parameter_delay;
          break;
        case 4:
          alpha_u = parameter_delay;
          break;
      }
      ++i;
    }
  }

  const ap_uint<16> rp_time = P32(31, 16);

  // define local C buffer and bind to URAM
  ap_uint<64> local_C_pe0[URAM_DEPTH];
  ap_uint<64> local_C_pe1[URAM_DEPTH];
  ap_uint<64> local_C_pe2[URAM_DEPTH];
  ap_uint<64> local_C_pe3[URAM_DEPTH];
  ap_uint<64> local_C_pe4[URAM_DEPTH];
  ap_uint<64> local_C_pe5[URAM_DEPTH];
  ap_uint<64> local_C_pe6[URAM_DEPTH];
  ap_uint<64> local_C_pe7[URAM_DEPTH];

#pragma HLS bind_storage variable = local_C_pe0 type = RAM_2P impl = URAM
#pragma HLS bind_storage variable = local_C_pe1 type = RAM_2P impl = URAM
#pragma HLS bind_storage variable = local_C_pe2 type = RAM_2P impl = URAM
#pragma HLS bind_storage variable = local_C_pe3 type = RAM_2P impl = URAM
#pragma HLS bind_storage variable = local_C_pe4 type = RAM_2P impl = URAM
#pragma HLS bind_storage variable = local_C_pe5 type = RAM_2P impl = URAM
#pragma HLS bind_storage variable = local_C_pe6 type = RAM_2P impl = URAM
#pragma HLS bind_storage variable = local_C_pe7 type = RAM_2P impl = URAM

l_rp:
  for (ap_uint<16> rp = 0; rp < rp_time; rp++) {
#pragma HLS loop_flatten off
#pragma HLS loop_tripcount min = 1 max = 16

    // init local C
    const ap_uint<32> M_ITE_INIT = (M + 511) >> 9;
  init_C:
    for (ap_uint<32> i = 0; i < M_ITE_INIT; ++i) {
#pragma HLS loop_tripcount min = 1 max = 800
#pragma HLS pipeline II = 1
      local_C_pe0[i] = 0;
      local_C_pe1[i] = 0;
      local_C_pe2[i] = 0;
      local_C_pe3[i] = 0;
      local_C_pe4[i] = 0;
      local_C_pe5[i] = 0;
      local_C_pe6[i] = 0;
      local_C_pe7[i] = 0;
    }

    // define local B buffer and pragma local B buffer if partition factor > 1
    ap_uint<32> local_B_pe0_pe1[WINDOW_SIZE];
    ap_uint<32> local_B_pe2_pe3[WINDOW_SIZE];
    ap_uint<32> local_B_pe4_pe5[WINDOW_SIZE];
    ap_uint<32> local_B_pe6_pe7[WINDOW_SIZE];

#pragma HLS bind_storage variable = local_B_pe0_pe1 latency = 2
#pragma HLS bind_storage variable = local_B_pe2_pe3 latency = 2
#pragma HLS bind_storage variable = local_B_pe4_pe5 latency = 2
#pragma HLS bind_storage variable = local_B_pe6_pe7 latency = 2

#pragma HLS array_partition variable = local_B_pe0_pe1 cyclic factor = \
    B_PARTITION_FACTOR
#pragma HLS array_partition variable = local_B_pe2_pe3 cyclic factor = \
    B_PARTITION_FACTOR
#pragma HLS array_partition variable = local_B_pe4_pe5 cyclic factor = \
    B_PARTITION_FACTOR
#pragma HLS array_partition variable = local_B_pe6_pe7 cyclic factor = \
    B_PARTITION_FACTOR

    ap_uint<32> start_32_in;
    bool start_32_in_ready = false;
  w1:
    while (!start_32_in_ready) {
#pragma HLS loop_tripcount min = 1 max = 10
#pragma HLS pipeline II = 1
      start_32_in_ready = fifo_inst.read_nb(start_32_in);
    }
    ap_uint<32> start_32 = HLS_REG(start_32_in);

  main:
    for (ap_uint<32> i = 0; i < NUM_ITE; ++i) {
#pragma HLS loop_tripcount min = 1 max = 49

      // fill onchip X
    read_X:
      for (ap_uint<14> j = 0; (j < WINDOW_SIZE_DIV_16) &&
                              (j < ((K + 15) >> 4) - i * WINDOW_SIZE_DIV_16);) {
#pragma HLS loop_tripcount min = 1 max = 1024
#pragma HLS pipeline II = 1
        ap_uint<512> x_512;
        bool x_512_ready = fifo_X.read_nb(x_512);
        ;

        if (x_512_ready) {
          ap_uint<512> x_512_delay = HLS_REG(HLS_REG(x_512));
        fill_X_p:
          for (ap_uint<5> k = 0; k < 16; ++k) {
            local_B_pe0_pe1[HLS_REG(HLS_REG(j)) * 16 + k] =
                x_512_delay(k * 32 + 31, k * 32);
            local_B_pe2_pe3[HLS_REG(HLS_REG(j)) * 16 + k] =
                x_512_delay(k * 32 + 31, k * 32);
            local_B_pe4_pe5[HLS_REG(HLS_REG(j)) * 16 + k] =
                x_512_delay(k * 32 + 31, k * 32);
            local_B_pe6_pe7[HLS_REG(HLS_REG(j)) * 16 + k] =
                x_512_delay(k * 32 + 31, k * 32);
          }
          ++j;
        }
      }

      // computation
      ap_uint<32> end_32_in;
      bool end_32_ready = false;
    w2:
      while (!end_32_ready) {
#pragma HLS loop_tripcount min = 1 max = 10
#pragma HLS pipeline II = 1
        end_32_ready = fifo_inst.read_nb(end_32_in);
      }
      ap_uint<32> end_32 = HLS_REG(end_32_in);

    computation:
      for (ap_uint<32> j = start_32; j < end_32;) {
#pragma HLS loop_tripcount min = 1 max = 200
#pragma HLS pipeline II = 1

#pragma HLS dependence true variable = local_C_pe0 distance = \
    DEP_DIST_LOAD_STORE
#pragma HLS dependence true variable = local_C_pe1 distance = \
    DEP_DIST_LOAD_STORE
#pragma HLS dependence true variable = local_C_pe2 distance = \
    DEP_DIST_LOAD_STORE
#pragma HLS dependence true variable = local_C_pe3 distance = \
    DEP_DIST_LOAD_STORE
#pragma HLS dependence true variable = local_C_pe4 distance = \
    DEP_DIST_LOAD_STORE
#pragma HLS dependence true variable = local_C_pe5 distance = \
    DEP_DIST_LOAD_STORE
#pragma HLS dependence true variable = local_C_pe6 distance = \
    DEP_DIST_LOAD_STORE
#pragma HLS dependence true variable = local_C_pe7 distance = \
    DEP_DIST_LOAD_STORE

        ap_uint<512> a_pes;
        bool a_pes_ready = fifo_A.read_nb(a_pes);

        if (a_pes_ready) {
          ap_uint<512> a_pes_delay = HLS_REG(HLS_REG(a_pes));

          ap_uint<64> a[8];
#pragma HLS array_partition variable = a complete

          ap_uint<14> a_col[8];
#pragma HLS array_partition variable = a_col complete

          ap_uint<18> a_row[8];
#pragma HLS array_partition variable = a_row complete

          ap_uint<32> a_val[8];
#pragma HLS array_partition variable = a_val complete

          for (ap_uint<4> p = 0; p < 8; ++p) {
            a[p] = a_pes_delay(63 + p * 64, p * 64);
          }

          for (ap_uint<4> p = 0; p < 8; ++p) {
            a_col[p] = (a[p])(63, 50);
            a_row[p] = (a[p])(49, 32);
            a_val[p] = (a[p])(31, 0);
          }

          // PE process
          PEcore(a_col[0], a_row[0], a_val[0], local_C_pe0, local_B_pe0_pe1);

          PEcore(a_col[1], a_row[1], a_val[1], local_C_pe1, local_B_pe0_pe1);

          PEcore(a_col[2], a_row[2], a_val[2], local_C_pe2, local_B_pe2_pe3);

          PEcore(a_col[3], a_row[3], a_val[3], local_C_pe3, local_B_pe2_pe3);

          PEcore(a_col[4], a_row[4], a_val[4], local_C_pe4, local_B_pe4_pe5);

          PEcore(a_col[5], a_row[5], a_val[5], local_C_pe5, local_B_pe4_pe5);

          PEcore(a_col[6], a_row[6], a_val[6], local_C_pe6, local_B_pe6_pe7);

          PEcore(a_col[7], a_row[7], a_val[7], local_C_pe7, local_B_pe6_pe7);

          ++j;
        }
      }
      start_32 = end_32;
    }

    // write C fifo
  write_C_outer:
    for (ap_uint<32> i = 0; i < (M_ITE_INIT << 3); ++i) {
#pragma HLS loop_tripcount min = 1 max = 1800
#pragma HLS pipeline II = 1
      ap_uint<64> out_c;

      switch (i % 8) {
        case 0:
          out_c = local_C_pe0[i >> 3];
          break;
        case 1:
          out_c = local_C_pe1[i >> 3];
          break;
        case 2:
          out_c = local_C_pe2[i >> 3];
          break;
        case 3:
          out_c = local_C_pe3[i >> 3];
          break;
        case 4:
          out_c = local_C_pe4[i >> 3];
          break;
        case 5:
          out_c = local_C_pe5[i >> 3];
          break;
        case 6:
          out_c = local_C_pe6[i >> 3];
          break;
        case 7:
          out_c = local_C_pe7[i >> 3];
          break;
      }

      ap_uint<64> out_c_mult;
      peg2mult(out_c, alpha_u, out_c_mult);
      fifo_C_out.write(HLS_REG(out_c_mult));
    }
  }
}

template <bool SEND_META>
void mux_Y(hls::stream<ap_uint<64> >& fifo_in0,
           hls::stream<ap_uint<64> >& fifo_in1,
           hls::stream<ap_uint<64> >& fifo_in2,
           hls::stream<ap_uint<64> >& fifo_in3,
           hls::stream<ap_uint<64> >& fifo_out) {
#pragma HLS inline off
  ap_uint<64> m64;
  bool M_ready = false;
w_M:
  while (!M_ready) {
#pragma HLS loop_tripcount min = 1 max = 10
#pragma HLS pipeline II = 1
    M_ready = fifo_in0.read_nb(m64);
  }
  const ap_uint<64> m64_delay = HLS_REG(m64);
  if (SEND_META) fifo_out.write(m64_delay);
  const ap_uint<32> M = m64_delay(31, 0);
  const ap_uint<16> rp_time = m64_delay(63, 48);

  const ap_uint<32> num_ite = ((M + 511) >> 9) << 5;
  const ap_uint<32> num_out = (M + 15) >> 4;

l_rp:
  for (ap_uint<16> rp = 0; rp < rp_time; rp++) {
#pragma HLS loop_flatten off
#pragma HLS loop_tripcount min = 1 max = 16
    ap_uint<32> n = 0;
  mux_C:
    for (ap_uint<32> i = 0; i < num_ite;) {
#pragma HLS loop_tripcount min = 1 max = 500
#pragma HLS pipeline II = 1
      ap_uint<64> u64;
      bool u64_ready;
      switch (i % 4) {
        case 0:
          u64_ready = fifo_in0.read_nb(u64);
          break;
        case 1:
          u64_ready = fifo_in1.read_nb(u64);
          break;
        case 2:
          u64_ready = fifo_in2.read_nb(u64);
          break;
        case 3:
          u64_ready = fifo_in3.read_nb(u64);
          break;
      }
      if (u64_ready) {
        ap_uint<64> u64_delay = HLS_REG(HLS_REG(u64));
        if (n < num_out) {
          fifo_out.write(u64_delay);
          ++n;
        }
        ++i;
      }
    }
  }
}

void merge_Y(
    hls::stream<ap_uint<64> >& fifo_in0, hls::stream<ap_uint<64> >& fifo_in1,
    hls::stream<ap_uint<64> >& fifo_in2, hls::stream<ap_uint<64> >& fifo_in3,
    hls::stream<ap_uint<64> >& fifo_in4, hls::stream<ap_uint<64> >& fifo_in5,
    hls::stream<ap_uint<64> >& fifo_in6, hls::stream<ap_uint<64> >& fifo_in7,
    hls::stream<ap_uint<512> >& fifo_out) {
#pragma HLS inline off
  ap_uint<64> m64;
  bool M_ready = false;
w_M:
  while (!M_ready) {
#pragma HLS loop_tripcount min = 1 max = 10
#pragma HLS pipeline II = 1
    M_ready = fifo_in0.read_nb(m64);
  }
  const ap_uint<64> m64_delay = HLS_REG(HLS_REG(m64));
  ap_uint<512> m512;
  m512(63, 0) = m64_delay;
  fifo_out.write(m512);
  const ap_uint<32> M = m64_delay(31, 0);
  const ap_uint<16> rp_time = m64_delay(63, 48);

  const ap_uint<32> num_out = (M + 15) >> 4;

l_rp:
  for (ap_uint<16> rp = 0; rp < rp_time; rp++) {
#pragma HLS loop_flatten off
#pragma HLS loop_tripcount min = 1 max = 16
    ap_uint<64> x0;
    ap_uint<64> x1;
    ap_uint<64> x2;
    ap_uint<64> x3;
    ap_uint<64> x4;
    ap_uint<64> x5;
    ap_uint<64> x6;
    ap_uint<64> x7;

    bool x0_ready = false;
    bool x1_ready = false;
    bool x2_ready = false;
    bool x3_ready = false;
    bool x4_ready = false;
    bool x5_ready = false;
    bool x6_ready = false;
    bool x7_ready = false;

  mg_C:
    for (ap_uint<32> i = 0; i < num_out;) {
#pragma HLS loop_tripcount min = 1 max = 500
#pragma HLS pipeline II = 1
      if (!x0_ready) {
        x0_ready = fifo_in0.read_nb(x0);
      }
      if (!x1_ready) {
        x1_ready = fifo_in1.read_nb(x1);
      }
      if (!x2_ready) {
        x2_ready = fifo_in2.read_nb(x2);
      }
      if (!x3_ready) {
        x3_ready = fifo_in3.read_nb(x3);
      }
      if (!x4_ready) {
        x4_ready = fifo_in4.read_nb(x4);
      }
      if (!x5_ready) {
        x5_ready = fifo_in5.read_nb(x5);
      }
      if (!x6_ready) {
        x6_ready = fifo_in6.read_nb(x6);
      }
      if (!x7_ready) {
        x7_ready = fifo_in7.read_nb(x7);
      }
      bool all_ready = x0_ready && x1_ready && x2_ready && x3_ready &&
                       x4_ready && x5_ready && x6_ready && x7_ready;
      if (all_ready) {
        ap_uint<64> x0_delay = HLS_REG(HLS_REG(x0));
        ap_uint<64> x1_delay = HLS_REG(HLS_REG(x1));
        ap_uint<64> x2_delay = HLS_REG(HLS_REG(x2));
        ap_uint<64> x3_delay = HLS_REG(HLS_REG(x3));
        ap_uint<64> x4_delay = HLS_REG(HLS_REG(x4));
        ap_uint<64> x5_delay = HLS_REG(HLS_REG(x5));
        ap_uint<64> x6_delay = HLS_REG(HLS_REG(x6));
        ap_uint<64> x7_delay = HLS_REG(HLS_REG(x7));

        ap_uint<512> x_out;
        x_out(63, 0) = x0_delay;
        x_out(127, 64) = x1_delay;
        x_out(191, 128) = x2_delay;
        x_out(255, 192) = x3_delay;
        x_out(319, 256) = x4_delay;
        x_out(383, 320) = x5_delay;
        x_out(447, 384) = x6_delay;
        x_out(511, 448) = x7_delay;

        fifo_out.write(x_out);

        x0_ready = false;
        x1_ready = false;
        x2_ready = false;
        x3_ready = false;
        x4_ready = false;
        x5_ready = false;
        x6_ready = false;
        x7_ready = false;

        ++i;
      }
    }
  }
}

void read_Y(const ap_uint<512>* Y_in, hls::stream<ap_uint<512> >& fifo_Y,
            const ap_uint<32> M, const ap_uint<16> rp_time) {
#pragma HLS inline off
  const ap_uint<32> num_ite_Y = (M + 15) >> 4;
l_rp:
  for (ap_uint<16> rp = 0; rp < rp_time; rp++) {
#pragma HLS loop_flatten off
#pragma HLS loop_tripcount min = 1 max = 16
  rd_Y:
    for (ap_uint<32> i = 0; i < num_ite_Y; i++) {
#pragma HLS loop_tripcount min = 1 max = 500000
#pragma HLS pipeline II = 1
      ap_uint<512> tmp_y = Y_in[i];
      fifo_Y.write(tmp_y);
    }
  }
}

void comp_Y(hls::stream<ap_uint<512> >& fifo_Y_read_in,
            hls::stream<ap_uint<512> >& fifo_Y_pe_in,
            hls::stream<ap_uint<512> >& fifo_Y_out, const ap_uint<32> beta_u) {
#pragma HLS inline off
  bool M_ready = false;
  ap_uint<512> M512;
w_Mxx:
  while (!M_ready) {
#pragma HLS loop_tripcount min = 1 max = 10
#pragma HLS pipeline II = 1
    M_ready = fifo_Y_pe_in.read_nb(M512);
  }
  ap_uint<512> M512_delay = HLS_REG(M512);
  fifo_Y_out.write(M512_delay);
  const ap_uint<32> M = M512_delay(31, 0);
  const ap_uint<16> rp_time = M512_delay(63, 48);

  const float beta_f = uint32_to_float(beta_u);
  const ap_uint<32> num_ite_Y = (M + 15) >> 4;

l_rp:
  for (ap_uint<16> rp = 0; rp < rp_time; rp++) {
#pragma HLS loop_flatten off
#pragma HLS loop_tripcount min = 1 max = 16

    ap_uint<512> y_read;
    bool y_read_ready = false;
    ap_uint<512> y_pe;
    bool y_pe_ready = false;

  cc:
    for (ap_uint<32> i = 0; i < num_ite_Y;) {
#pragma HLS loop_tripcount min = 1 max = 5000
#pragma HLS pipeline II = 1
      if (!y_read_ready) {
        y_read_ready = fifo_Y_read_in.read_nb(y_read);
      }
      if (!y_pe_ready) {
        y_pe_ready = fifo_Y_pe_in.read_nb(y_pe);
      }
      if (y_read_ready && y_pe_ready) {
        ap_uint<512> y_pe_delay = HLS_REG(HLS_REG(y_pe));
        ap_uint<512> y_read_delay = HLS_REG(HLS_REG(y_read));

        ap_uint<512> y_out;
        for (ap_uint<5> p = 0; p < 16; ++p) {
          float op_ab = uint32_to_float(y_pe_delay(31 + p * 32, p * 32));
          float op_y = uint32_to_float(y_read_delay(31 + p * 32, p * 32));
          float op_result = op_ab + HLS_REG(beta_f) * op_y;
          y_out(31 + p * 32, p * 32) = float_to_uint32(op_result);
        }

        fifo_Y_out.write(HLS_REG(y_out));
        y_read_ready = false;
        y_pe_ready = false;
        ++i;
      }
    }
  }
}

void write_Y(hls::stream<ap_uint<512> >& fifo_Y, ap_uint<512>* Y_out) {
#pragma HLS inline off
  bool M_ready = false;
  ap_uint<512> M512;

w_Mxx:
  while (!M_ready) {
#pragma HLS loop_tripcount min = 1 max = 10
#pragma HLS pipeline II = 1
    M_ready = fifo_Y.read_nb(M512);
  }

  const ap_uint<32> M = M512(31, 0);
  const ap_uint<16> rp_time = M512(63, 48);
  const ap_uint<32> num_ite_Y = (M + 15) >> 4;

l_rp:
  for (ap_uint<16> rp = 0; rp < rp_time; rp++) {
#pragma HLS loop_flatten off
#pragma HLS loop_tripcount min = 1 max = 16
  wr_y:
    for (ap_uint<32> i = 0; i < num_ite_Y; ++i) {
#pragma HLS loop_tripcount min = 1 max = 5000
#pragma HLS pipeline II = 1
      ap_uint<512> tmp_y = fifo_Y.read();
      Y_out[i] = tmp_y;
    }
  }
}

#ifndef HLS
extern "C" {
#endif

void serpens(
    const ap_uint<32>* edge_list_ptr,

    const ap_uint<512>* edge_list_ch0, const ap_uint<512>* edge_list_ch1,
    const ap_uint<512>* edge_list_ch2, const ap_uint<512>* edge_list_ch3,
    const ap_uint<512>* edge_list_ch4, const ap_uint<512>* edge_list_ch5,
    const ap_uint<512>* edge_list_ch6, const ap_uint<512>* edge_list_ch7,
    const ap_uint<512>* edge_list_ch8, const ap_uint<512>* edge_list_ch9,
    const ap_uint<512>* edge_list_ch10, const ap_uint<512>* edge_list_ch11,
    const ap_uint<512>* edge_list_ch12, const ap_uint<512>* edge_list_ch13,
    const ap_uint<512>* edge_list_ch14, const ap_uint<512>* edge_list_ch15,
    const ap_uint<512>* edge_list_ch16, const ap_uint<512>* edge_list_ch17,
    const ap_uint<512>* edge_list_ch18, const ap_uint<512>* edge_list_ch19,
    const ap_uint<512>* edge_list_ch20, const ap_uint<512>* edge_list_ch21,
    const ap_uint<512>* edge_list_ch22, const ap_uint<512>* edge_list_ch23,
    const ap_uint<512>* edge_list_ch24, const ap_uint<512>* edge_list_ch25,
    const ap_uint<512>* edge_list_ch26, const ap_uint<512>* edge_list_ch27,
    const ap_uint<512>* edge_list_ch28, const ap_uint<512>* edge_list_ch29,
    const ap_uint<512>* edge_list_ch30, const ap_uint<512>* edge_list_ch31,

    const ap_uint<512>* vec_X, const ap_uint<512>* vec_Y,
    ap_uint<512>* vec_Y_out,

    const int NUM_ITE, const int NUM_A_LEN, const int M, const int K,
    const short P, const unsigned int alpha_u, const unsigned int beta_u) {
#pragma HLS INTERFACE m_axi port = edge_list_ptr offset = slave bundle = ddr_ptr
#pragma HLS INTERFACE m_axi port = vec_X offset = slave bundle = ddr_vx

#pragma HLS INTERFACE m_axi port = edge_list_ch0 offset = slave bundle = hbm0
#pragma HLS INTERFACE m_axi port = edge_list_ch1 offset = slave bundle = hbm1
#pragma HLS INTERFACE m_axi port = edge_list_ch2 offset = slave bundle = hbm2
#pragma HLS INTERFACE m_axi port = edge_list_ch3 offset = slave bundle = hbm3
#pragma HLS INTERFACE m_axi port = edge_list_ch4 offset = slave bundle = hbm4
#pragma HLS INTERFACE m_axi port = edge_list_ch5 offset = slave bundle = hbm5
#pragma HLS INTERFACE m_axi port = edge_list_ch6 offset = slave bundle = hbm6
#pragma HLS INTERFACE m_axi port = edge_list_ch7 offset = slave bundle = hbm7
#pragma HLS INTERFACE m_axi port = edge_list_ch8 offset = slave bundle = hbm8
#pragma HLS INTERFACE m_axi port = edge_list_ch9 offset = slave bundle = hbm9
#pragma HLS INTERFACE m_axi port = edge_list_ch10 offset = slave bundle = hbm10
#pragma HLS INTERFACE m_axi port = edge_list_ch11 offset = slave bundle = hbm11
#pragma HLS INTERFACE m_axi port = edge_list_ch12 offset = slave bundle = hbm12
#pragma HLS INTERFACE m_axi port = edge_list_ch13 offset = slave bundle = hbm13
#pragma HLS INTERFACE m_axi port = edge_list_ch14 offset = slave bundle = hbm14
#pragma HLS INTERFACE m_axi port = edge_list_ch15 offset = slave bundle = hbm15
#pragma HLS INTERFACE m_axi port = edge_list_ch16 offset = slave bundle = hbm16
#pragma HLS INTERFACE m_axi port = edge_list_ch17 offset = slave bundle = hbm17
#pragma HLS INTERFACE m_axi port = edge_list_ch18 offset = slave bundle = hbm18
#pragma HLS INTERFACE m_axi port = edge_list_ch19 offset = slave bundle = hbm19
#pragma HLS INTERFACE m_axi port = edge_list_ch20 offset = slave bundle = hbm20
#pragma HLS INTERFACE m_axi port = edge_list_ch21 offset = slave bundle = hbm21
#pragma HLS INTERFACE m_axi port = edge_list_ch22 offset = slave bundle = hbm22
#pragma HLS INTERFACE m_axi port = edge_list_ch23 offset = slave bundle = hbm23
#pragma HLS INTERFACE m_axi port = edge_list_ch24 offset = slave bundle = hbm24
#pragma HLS INTERFACE m_axi port = edge_list_ch25 offset = slave bundle = hbm25
#pragma HLS INTERFACE m_axi port = edge_list_ch26 offset = slave bundle = hbm26
#pragma HLS INTERFACE m_axi port = edge_list_ch27 offset = slave bundle = hbm27
#pragma HLS INTERFACE m_axi port = edge_list_ch28 offset = slave bundle = hbm28
#pragma HLS INTERFACE m_axi port = edge_list_ch29 offset = slave bundle = hbm29
#pragma HLS INTERFACE m_axi port = edge_list_ch30 offset = slave bundle = hbm30
#pragma HLS INTERFACE m_axi port = edge_list_ch31 offset = slave bundle = hbm31

#pragma HLS INTERFACE m_axi port = vec_Y offset = slave bundle = ddr_vy
#pragma HLS INTERFACE m_axi port = vec_Y_out offset = slave bundle = ddr_vyo

  hls::stream<ap_uint<32> > fifo_edge_list_ptr_pe0("fifo_edge_list_ptr_pe0");
  hls::stream<ap_uint<32> > fifo_edge_list_ptr_pe1("fifo_edge_list_ptr_pe1");
  hls::stream<ap_uint<32> > fifo_edge_list_ptr_pe2("fifo_edge_list_ptr_pe2");
  hls::stream<ap_uint<32> > fifo_edge_list_ptr_pe3("fifo_edge_list_ptr_pe3");
  hls::stream<ap_uint<32> > fifo_edge_list_ptr_pe4("fifo_edge_list_ptr_pe4");
  hls::stream<ap_uint<32> > fifo_edge_list_ptr_pe5("fifo_edge_list_ptr_pe5");
  hls::stream<ap_uint<32> > fifo_edge_list_ptr_pe6("fifo_edge_list_ptr_pe6");
  hls::stream<ap_uint<32> > fifo_edge_list_ptr_pe7("fifo_edge_list_ptr_pe7");
  hls::stream<ap_uint<32> > fifo_edge_list_ptr_pe8("fifo_edge_list_ptr_pe8");
  hls::stream<ap_uint<32> > fifo_edge_list_ptr_pe9("fifo_edge_list_ptr_pe9");
  hls::stream<ap_uint<32> > fifo_edge_list_ptr_pe10("fifo_edge_list_ptr_pe10");
  hls::stream<ap_uint<32> > fifo_edge_list_ptr_pe11("fifo_edge_list_ptr_pe11");
  hls::stream<ap_uint<32> > fifo_edge_list_ptr_pe12("fifo_edge_list_ptr_pe12");
  hls::stream<ap_uint<32> > fifo_edge_list_ptr_pe13("fifo_edge_list_ptr_pe13");
  hls::stream<ap_uint<32> > fifo_edge_list_ptr_pe14("fifo_edge_list_ptr_pe14");
  hls::stream<ap_uint<32> > fifo_edge_list_ptr_pe15("fifo_edge_list_ptr_pe15");
  hls::stream<ap_uint<32> > fifo_edge_list_ptr_pe16("fifo_edge_list_ptr_pe16");
  hls::stream<ap_uint<32> > fifo_edge_list_ptr_pe17("fifo_edge_list_ptr_pe17");
  hls::stream<ap_uint<32> > fifo_edge_list_ptr_pe18("fifo_edge_list_ptr_pe18");
  hls::stream<ap_uint<32> > fifo_edge_list_ptr_pe19("fifo_edge_list_ptr_pe19");
  hls::stream<ap_uint<32> > fifo_edge_list_ptr_pe20("fifo_edge_list_ptr_pe20");
  hls::stream<ap_uint<32> > fifo_edge_list_ptr_pe21("fifo_edge_list_ptr_pe21");
  hls::stream<ap_uint<32> > fifo_edge_list_ptr_pe22("fifo_edge_list_ptr_pe22");
  hls::stream<ap_uint<32> > fifo_edge_list_ptr_pe23("fifo_edge_list_ptr_pe23");
  hls::stream<ap_uint<32> > fifo_edge_list_ptr_pe24("fifo_edge_list_ptr_pe24");
  hls::stream<ap_uint<32> > fifo_edge_list_ptr_pe25("fifo_edge_list_ptr_pe25");
  hls::stream<ap_uint<32> > fifo_edge_list_ptr_pe26("fifo_edge_list_ptr_pe26");
  hls::stream<ap_uint<32> > fifo_edge_list_ptr_pe27("fifo_edge_list_ptr_pe27");
  hls::stream<ap_uint<32> > fifo_edge_list_ptr_pe28("fifo_edge_list_ptr_pe28");
  hls::stream<ap_uint<32> > fifo_edge_list_ptr_pe29("fifo_edge_list_ptr_pe29");
  hls::stream<ap_uint<32> > fifo_edge_list_ptr_pe30("fifo_edge_list_ptr_pe30");
  hls::stream<ap_uint<32> > fifo_edge_list_ptr_pe31("fifo_edge_list_ptr_pe31");

#pragma HLS STREAM variable = fifo_edge_list_ptr_pe0 depth = 2
#pragma HLS STREAM variable = fifo_edge_list_ptr_pe1 depth = 2
#pragma HLS STREAM variable = fifo_edge_list_ptr_pe2 depth = 2
#pragma HLS STREAM variable = fifo_edge_list_ptr_pe3 depth = 2
#pragma HLS STREAM variable = fifo_edge_list_ptr_pe4 depth = 2
#pragma HLS STREAM variable = fifo_edge_list_ptr_pe5 depth = 2
#pragma HLS STREAM variable = fifo_edge_list_ptr_pe6 depth = 2
#pragma HLS STREAM variable = fifo_edge_list_ptr_pe7 depth = 2
#pragma HLS STREAM variable = fifo_edge_list_ptr_pe8 depth = 2
#pragma HLS STREAM variable = fifo_edge_list_ptr_pe9 depth = 2
#pragma HLS STREAM variable = fifo_edge_list_ptr_pe10 depth = 2
#pragma HLS STREAM variable = fifo_edge_list_ptr_pe11 depth = 2
#pragma HLS STREAM variable = fifo_edge_list_ptr_pe12 depth = 2
#pragma HLS STREAM variable = fifo_edge_list_ptr_pe13 depth = 2
#pragma HLS STREAM variable = fifo_edge_list_ptr_pe14 depth = 2
#pragma HLS STREAM variable = fifo_edge_list_ptr_pe15 depth = 2
#pragma HLS STREAM variable = fifo_edge_list_ptr_pe16 depth = 2
#pragma HLS STREAM variable = fifo_edge_list_ptr_pe17 depth = 2
#pragma HLS STREAM variable = fifo_edge_list_ptr_pe18 depth = 2
#pragma HLS STREAM variable = fifo_edge_list_ptr_pe19 depth = 2
#pragma HLS STREAM variable = fifo_edge_list_ptr_pe20 depth = 2
#pragma HLS STREAM variable = fifo_edge_list_ptr_pe21 depth = 2
#pragma HLS STREAM variable = fifo_edge_list_ptr_pe22 depth = 2
#pragma HLS STREAM variable = fifo_edge_list_ptr_pe23 depth = 2
#pragma HLS STREAM variable = fifo_edge_list_ptr_pe24 depth = 2
#pragma HLS STREAM variable = fifo_edge_list_ptr_pe25 depth = 2
#pragma HLS STREAM variable = fifo_edge_list_ptr_pe26 depth = 2
#pragma HLS STREAM variable = fifo_edge_list_ptr_pe27 depth = 2
#pragma HLS STREAM variable = fifo_edge_list_ptr_pe28 depth = 2
#pragma HLS STREAM variable = fifo_edge_list_ptr_pe29 depth = 2
#pragma HLS STREAM variable = fifo_edge_list_ptr_pe30 depth = 2
#pragma HLS STREAM variable = fifo_edge_list_ptr_pe31 depth = 2

#pragma HLS bind_storage variable = fifo_edge_list_ptr_pe0 type = FIFO impl = \
    SRL
#pragma HLS bind_storage variable = fifo_edge_list_ptr_pe1 type = FIFO impl = \
    SRL
#pragma HLS bind_storage variable = fifo_edge_list_ptr_pe2 type = FIFO impl = \
    SRL
#pragma HLS bind_storage variable = fifo_edge_list_ptr_pe3 type = FIFO impl = \
    SRL
#pragma HLS bind_storage variable = fifo_edge_list_ptr_pe4 type = FIFO impl = \
    SRL
#pragma HLS bind_storage variable = fifo_edge_list_ptr_pe5 type = FIFO impl = \
    SRL
#pragma HLS bind_storage variable = fifo_edge_list_ptr_pe6 type = FIFO impl = \
    SRL
#pragma HLS bind_storage variable = fifo_edge_list_ptr_pe7 type = FIFO impl = \
    SRL
#pragma HLS bind_storage variable = fifo_edge_list_ptr_pe8 type = FIFO impl = \
    SRL
#pragma HLS bind_storage variable = fifo_edge_list_ptr_pe9 type = FIFO impl = \
    SRL
#pragma HLS bind_storage variable = fifo_edge_list_ptr_pe10 type = FIFO impl = \
    SRL
#pragma HLS bind_storage variable = fifo_edge_list_ptr_pe11 type = FIFO impl = \
    SRL
#pragma HLS bind_storage variable = fifo_edge_list_ptr_pe12 type = FIFO impl = \
    SRL
#pragma HLS bind_storage variable = fifo_edge_list_ptr_pe13 type = FIFO impl = \
    SRL
#pragma HLS bind_storage variable = fifo_edge_list_ptr_pe14 type = FIFO impl = \
    SRL
#pragma HLS bind_storage variable = fifo_edge_list_ptr_pe15 type = FIFO impl = \
    SRL
#pragma HLS bind_storage variable = fifo_edge_list_ptr_pe16 type = FIFO impl = \
    SRL
#pragma HLS bind_storage variable = fifo_edge_list_ptr_pe17 type = FIFO impl = \
    SRL
#pragma HLS bind_storage variable = fifo_edge_list_ptr_pe18 type = FIFO impl = \
    SRL
#pragma HLS bind_storage variable = fifo_edge_list_ptr_pe19 type = FIFO impl = \
    SRL
#pragma HLS bind_storage variable = fifo_edge_list_ptr_pe20 type = FIFO impl = \
    SRL
#pragma HLS bind_storage variable = fifo_edge_list_ptr_pe21 type = FIFO impl = \
    SRL
#pragma HLS bind_storage variable = fifo_edge_list_ptr_pe22 type = FIFO impl = \
    SRL
#pragma HLS bind_storage variable = fifo_edge_list_ptr_pe23 type = FIFO impl = \
    SRL
#pragma HLS bind_storage variable = fifo_edge_list_ptr_pe24 type = FIFO impl = \
    SRL
#pragma HLS bind_storage variable = fifo_edge_list_ptr_pe25 type = FIFO impl = \
    SRL
#pragma HLS bind_storage variable = fifo_edge_list_ptr_pe26 type = FIFO impl = \
    SRL
#pragma HLS bind_storage variable = fifo_edge_list_ptr_pe27 type = FIFO impl = \
    SRL
#pragma HLS bind_storage variable = fifo_edge_list_ptr_pe28 type = FIFO impl = \
    SRL
#pragma HLS bind_storage variable = fifo_edge_list_ptr_pe29 type = FIFO impl = \
    SRL
#pragma HLS bind_storage variable = fifo_edge_list_ptr_pe30 type = FIFO impl = \
    SRL
#pragma HLS bind_storage variable = fifo_edge_list_ptr_pe31 type = FIFO impl = \
    SRL

  hls::stream<ap_uint<512> > fifo_A_pe0("fifo_A_pe0");
  hls::stream<ap_uint<512> > fifo_A_pe1("fifo_A_pe1");
  hls::stream<ap_uint<512> > fifo_A_pe2("fifo_A_pe2");
  hls::stream<ap_uint<512> > fifo_A_pe3("fifo_A_pe3");
  hls::stream<ap_uint<512> > fifo_A_pe4("fifo_A_pe4");
  hls::stream<ap_uint<512> > fifo_A_pe5("fifo_A_pe5");
  hls::stream<ap_uint<512> > fifo_A_pe6("fifo_A_pe6");
  hls::stream<ap_uint<512> > fifo_A_pe7("fifo_A_pe7");
  hls::stream<ap_uint<512> > fifo_A_pe8("fifo_A_pe8");
  hls::stream<ap_uint<512> > fifo_A_pe9("fifo_A_pe9");
  hls::stream<ap_uint<512> > fifo_A_pe10("fifo_A_pe10");
  hls::stream<ap_uint<512> > fifo_A_pe11("fifo_A_pe11");
  hls::stream<ap_uint<512> > fifo_A_pe12("fifo_A_pe12");
  hls::stream<ap_uint<512> > fifo_A_pe13("fifo_A_pe13");
  hls::stream<ap_uint<512> > fifo_A_pe14("fifo_A_pe14");
  hls::stream<ap_uint<512> > fifo_A_pe15("fifo_A_pe15");
  hls::stream<ap_uint<512> > fifo_A_pe16("fifo_A_pe16");
  hls::stream<ap_uint<512> > fifo_A_pe17("fifo_A_pe17");
  hls::stream<ap_uint<512> > fifo_A_pe18("fifo_A_pe18");
  hls::stream<ap_uint<512> > fifo_A_pe19("fifo_A_pe19");
  hls::stream<ap_uint<512> > fifo_A_pe20("fifo_A_pe20");
  hls::stream<ap_uint<512> > fifo_A_pe21("fifo_A_pe21");
  hls::stream<ap_uint<512> > fifo_A_pe22("fifo_A_pe22");
  hls::stream<ap_uint<512> > fifo_A_pe23("fifo_A_pe23");
  hls::stream<ap_uint<512> > fifo_A_pe24("fifo_A_pe24");
  hls::stream<ap_uint<512> > fifo_A_pe25("fifo_A_pe25");
  hls::stream<ap_uint<512> > fifo_A_pe26("fifo_A_pe26");
  hls::stream<ap_uint<512> > fifo_A_pe27("fifo_A_pe27");
  hls::stream<ap_uint<512> > fifo_A_pe28("fifo_A_pe28");
  hls::stream<ap_uint<512> > fifo_A_pe29("fifo_A_pe29");
  hls::stream<ap_uint<512> > fifo_A_pe30("fifo_A_pe30");
  hls::stream<ap_uint<512> > fifo_A_pe31("fifo_A_pe31");

#pragma HLS STREAM variable = fifo_A_pe0 depth = 2
#pragma HLS STREAM variable = fifo_A_pe1 depth = 2
#pragma HLS STREAM variable = fifo_A_pe2 depth = 2
#pragma HLS STREAM variable = fifo_A_pe3 depth = 2
#pragma HLS STREAM variable = fifo_A_pe4 depth = 2
#pragma HLS STREAM variable = fifo_A_pe5 depth = 2
#pragma HLS STREAM variable = fifo_A_pe6 depth = 2
#pragma HLS STREAM variable = fifo_A_pe7 depth = 2
#pragma HLS STREAM variable = fifo_A_pe8 depth = 2
#pragma HLS STREAM variable = fifo_A_pe9 depth = 2
#pragma HLS STREAM variable = fifo_A_pe10 depth = 2
#pragma HLS STREAM variable = fifo_A_pe11 depth = 2
#pragma HLS STREAM variable = fifo_A_pe12 depth = 2
#pragma HLS STREAM variable = fifo_A_pe13 depth = 2
#pragma HLS STREAM variable = fifo_A_pe14 depth = 2
#pragma HLS STREAM variable = fifo_A_pe15 depth = 2
#pragma HLS STREAM variable = fifo_A_pe16 depth = 2
#pragma HLS STREAM variable = fifo_A_pe17 depth = 2
#pragma HLS STREAM variable = fifo_A_pe18 depth = 2
#pragma HLS STREAM variable = fifo_A_pe19 depth = 2
#pragma HLS STREAM variable = fifo_A_pe20 depth = 2
#pragma HLS STREAM variable = fifo_A_pe21 depth = 2
#pragma HLS STREAM variable = fifo_A_pe22 depth = 2
#pragma HLS STREAM variable = fifo_A_pe23 depth = 2
#pragma HLS STREAM variable = fifo_A_pe24 depth = 2
#pragma HLS STREAM variable = fifo_A_pe25 depth = 2
#pragma HLS STREAM variable = fifo_A_pe26 depth = 2
#pragma HLS STREAM variable = fifo_A_pe27 depth = 2
#pragma HLS STREAM variable = fifo_A_pe28 depth = 2
#pragma HLS STREAM variable = fifo_A_pe29 depth = 2
#pragma HLS STREAM variable = fifo_A_pe30 depth = 2
#pragma HLS STREAM variable = fifo_A_pe31 depth = 2

#pragma HLS bind_storage variable = fifo_A_pe0 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_A_pe1 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_A_pe2 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_A_pe3 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_A_pe4 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_A_pe5 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_A_pe6 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_A_pe7 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_A_pe8 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_A_pe9 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_A_pe10 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_A_pe11 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_A_pe12 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_A_pe13 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_A_pe14 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_A_pe15 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_A_pe16 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_A_pe17 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_A_pe18 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_A_pe19 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_A_pe20 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_A_pe21 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_A_pe22 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_A_pe23 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_A_pe24 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_A_pe25 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_A_pe26 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_A_pe27 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_A_pe28 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_A_pe29 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_A_pe30 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_A_pe31 type = FIFO impl = SRL

  hls::stream<ap_uint<512> > fifo_X_pe0("fifo_X_pe0");
  hls::stream<ap_uint<512> > fifo_X_pe1("fifo_X_pe1");
  hls::stream<ap_uint<512> > fifo_X_pe2("fifo_X_pe2");
  hls::stream<ap_uint<512> > fifo_X_pe3("fifo_X_pe3");
  hls::stream<ap_uint<512> > fifo_X_pe4("fifo_X_pe4");
  hls::stream<ap_uint<512> > fifo_X_pe5("fifo_X_pe5");
  hls::stream<ap_uint<512> > fifo_X_pe6("fifo_X_pe6");
  hls::stream<ap_uint<512> > fifo_X_pe7("fifo_X_pe7");
  hls::stream<ap_uint<512> > fifo_X_pe8("fifo_X_pe8");
  hls::stream<ap_uint<512> > fifo_X_pe9("fifo_X_pe9");
  hls::stream<ap_uint<512> > fifo_X_pe10("fifo_X_pe10");
  hls::stream<ap_uint<512> > fifo_X_pe11("fifo_X_pe11");
  hls::stream<ap_uint<512> > fifo_X_pe12("fifo_X_pe12");
  hls::stream<ap_uint<512> > fifo_X_pe13("fifo_X_pe13");
  hls::stream<ap_uint<512> > fifo_X_pe14("fifo_X_pe14");
  hls::stream<ap_uint<512> > fifo_X_pe15("fifo_X_pe15");
  hls::stream<ap_uint<512> > fifo_X_pe16("fifo_X_pe16");
  hls::stream<ap_uint<512> > fifo_X_pe17("fifo_X_pe17");
  hls::stream<ap_uint<512> > fifo_X_pe18("fifo_X_pe18");
  hls::stream<ap_uint<512> > fifo_X_pe19("fifo_X_pe19");
  hls::stream<ap_uint<512> > fifo_X_pe20("fifo_X_pe20");
  hls::stream<ap_uint<512> > fifo_X_pe21("fifo_X_pe21");
  hls::stream<ap_uint<512> > fifo_X_pe22("fifo_X_pe22");
  hls::stream<ap_uint<512> > fifo_X_pe23("fifo_X_pe23");
  hls::stream<ap_uint<512> > fifo_X_pe24("fifo_X_pe24");
  hls::stream<ap_uint<512> > fifo_X_pe25("fifo_X_pe25");
  hls::stream<ap_uint<512> > fifo_X_pe26("fifo_X_pe26");
  hls::stream<ap_uint<512> > fifo_X_pe27("fifo_X_pe27");
  hls::stream<ap_uint<512> > fifo_X_pe28("fifo_X_pe28");
  hls::stream<ap_uint<512> > fifo_X_pe29("fifo_X_pe29");
  hls::stream<ap_uint<512> > fifo_X_pe30("fifo_X_pe30");
  hls::stream<ap_uint<512> > fifo_X_pe31("fifo_X_pe31");

#pragma HLS STREAM variable = fifo_X_pe0 depth = 2
#pragma HLS STREAM variable = fifo_X_pe1 depth = 2
#pragma HLS STREAM variable = fifo_X_pe2 depth = 2
#pragma HLS STREAM variable = fifo_X_pe3 depth = 2
#pragma HLS STREAM variable = fifo_X_pe4 depth = 2
#pragma HLS STREAM variable = fifo_X_pe5 depth = 2
#pragma HLS STREAM variable = fifo_X_pe6 depth = 2
#pragma HLS STREAM variable = fifo_X_pe7 depth = 2
#pragma HLS STREAM variable = fifo_X_pe8 depth = 2
#pragma HLS STREAM variable = fifo_X_pe9 depth = 2
#pragma HLS STREAM variable = fifo_X_pe10 depth = 2
#pragma HLS STREAM variable = fifo_X_pe11 depth = 2
#pragma HLS STREAM variable = fifo_X_pe12 depth = 2
#pragma HLS STREAM variable = fifo_X_pe13 depth = 2
#pragma HLS STREAM variable = fifo_X_pe14 depth = 2
#pragma HLS STREAM variable = fifo_X_pe15 depth = 2
#pragma HLS STREAM variable = fifo_X_pe16 depth = 2
#pragma HLS STREAM variable = fifo_X_pe17 depth = 2
#pragma HLS STREAM variable = fifo_X_pe18 depth = 2
#pragma HLS STREAM variable = fifo_X_pe19 depth = 2
#pragma HLS STREAM variable = fifo_X_pe20 depth = 2
#pragma HLS STREAM variable = fifo_X_pe21 depth = 2
#pragma HLS STREAM variable = fifo_X_pe22 depth = 2
#pragma HLS STREAM variable = fifo_X_pe23 depth = 2
#pragma HLS STREAM variable = fifo_X_pe24 depth = 2
#pragma HLS STREAM variable = fifo_X_pe25 depth = 2
#pragma HLS STREAM variable = fifo_X_pe26 depth = 2
#pragma HLS STREAM variable = fifo_X_pe27 depth = 2
#pragma HLS STREAM variable = fifo_X_pe28 depth = 2
#pragma HLS STREAM variable = fifo_X_pe29 depth = 2
#pragma HLS STREAM variable = fifo_X_pe30 depth = 2
#pragma HLS STREAM variable = fifo_X_pe31 depth = 2

#pragma HLS bind_storage variable = fifo_X_pe0 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_X_pe1 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_X_pe2 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_X_pe3 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_X_pe4 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_X_pe5 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_X_pe6 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_X_pe7 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_X_pe8 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_X_pe9 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_X_pe10 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_X_pe11 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_X_pe12 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_X_pe13 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_X_pe14 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_X_pe15 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_X_pe16 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_X_pe17 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_X_pe18 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_X_pe19 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_X_pe20 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_X_pe21 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_X_pe22 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_X_pe23 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_X_pe24 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_X_pe25 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_X_pe26 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_X_pe27 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_X_pe28 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_X_pe29 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_X_pe30 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_X_pe31 type = FIFO impl = SRL

  hls::stream<ap_uint<64> > fifo_Y_pe0("fifo_Y_pe0");
  hls::stream<ap_uint<64> > fifo_Y_pe1("fifo_Y_pe1");
  hls::stream<ap_uint<64> > fifo_Y_pe2("fifo_Y_pe2");
  hls::stream<ap_uint<64> > fifo_Y_pe3("fifo_Y_pe3");
  hls::stream<ap_uint<64> > fifo_Y_pe4("fifo_Y_pe4");
  hls::stream<ap_uint<64> > fifo_Y_pe5("fifo_Y_pe5");
  hls::stream<ap_uint<64> > fifo_Y_pe6("fifo_Y_pe6");
  hls::stream<ap_uint<64> > fifo_Y_pe7("fifo_Y_pe7");
  hls::stream<ap_uint<64> > fifo_Y_pe8("fifo_Y_pe8");
  hls::stream<ap_uint<64> > fifo_Y_pe9("fifo_Y_pe9");
  hls::stream<ap_uint<64> > fifo_Y_pe10("fifo_Y_pe10");
  hls::stream<ap_uint<64> > fifo_Y_pe11("fifo_Y_pe11");
  hls::stream<ap_uint<64> > fifo_Y_pe12("fifo_Y_pe12");
  hls::stream<ap_uint<64> > fifo_Y_pe13("fifo_Y_pe13");
  hls::stream<ap_uint<64> > fifo_Y_pe14("fifo_Y_pe14");
  hls::stream<ap_uint<64> > fifo_Y_pe15("fifo_Y_pe15");
  hls::stream<ap_uint<64> > fifo_Y_pe16("fifo_Y_pe16");
  hls::stream<ap_uint<64> > fifo_Y_pe17("fifo_Y_pe17");
  hls::stream<ap_uint<64> > fifo_Y_pe18("fifo_Y_pe18");
  hls::stream<ap_uint<64> > fifo_Y_pe19("fifo_Y_pe19");
  hls::stream<ap_uint<64> > fifo_Y_pe20("fifo_Y_pe20");
  hls::stream<ap_uint<64> > fifo_Y_pe21("fifo_Y_pe21");
  hls::stream<ap_uint<64> > fifo_Y_pe22("fifo_Y_pe22");
  hls::stream<ap_uint<64> > fifo_Y_pe23("fifo_Y_pe23");
  hls::stream<ap_uint<64> > fifo_Y_pe24("fifo_Y_pe24");
  hls::stream<ap_uint<64> > fifo_Y_pe25("fifo_Y_pe25");
  hls::stream<ap_uint<64> > fifo_Y_pe26("fifo_Y_pe26");
  hls::stream<ap_uint<64> > fifo_Y_pe27("fifo_Y_pe27");
  hls::stream<ap_uint<64> > fifo_Y_pe28("fifo_Y_pe28");
  hls::stream<ap_uint<64> > fifo_Y_pe29("fifo_Y_pe29");
  hls::stream<ap_uint<64> > fifo_Y_pe30("fifo_Y_pe30");
  hls::stream<ap_uint<64> > fifo_Y_pe31("fifo_Y_pe31");

#pragma HLS STREAM variable = fifo_Y_pe0 depth = 2
#pragma HLS STREAM variable = fifo_Y_pe1 depth = 2
#pragma HLS STREAM variable = fifo_Y_pe2 depth = 2
#pragma HLS STREAM variable = fifo_Y_pe3 depth = 2
#pragma HLS STREAM variable = fifo_Y_pe4 depth = 2
#pragma HLS STREAM variable = fifo_Y_pe5 depth = 2
#pragma HLS STREAM variable = fifo_Y_pe6 depth = 2
#pragma HLS STREAM variable = fifo_Y_pe7 depth = 2
#pragma HLS STREAM variable = fifo_Y_pe8 depth = 2
#pragma HLS STREAM variable = fifo_Y_pe9 depth = 2
#pragma HLS STREAM variable = fifo_Y_pe10 depth = 2
#pragma HLS STREAM variable = fifo_Y_pe11 depth = 2
#pragma HLS STREAM variable = fifo_Y_pe12 depth = 2
#pragma HLS STREAM variable = fifo_Y_pe13 depth = 2
#pragma HLS STREAM variable = fifo_Y_pe14 depth = 2
#pragma HLS STREAM variable = fifo_Y_pe15 depth = 2
#pragma HLS STREAM variable = fifo_Y_pe16 depth = 2
#pragma HLS STREAM variable = fifo_Y_pe17 depth = 2
#pragma HLS STREAM variable = fifo_Y_pe18 depth = 2
#pragma HLS STREAM variable = fifo_Y_pe19 depth = 2
#pragma HLS STREAM variable = fifo_Y_pe20 depth = 2
#pragma HLS STREAM variable = fifo_Y_pe21 depth = 2
#pragma HLS STREAM variable = fifo_Y_pe22 depth = 2
#pragma HLS STREAM variable = fifo_Y_pe23 depth = 2
#pragma HLS STREAM variable = fifo_Y_pe24 depth = 2
#pragma HLS STREAM variable = fifo_Y_pe25 depth = 2
#pragma HLS STREAM variable = fifo_Y_pe26 depth = 2
#pragma HLS STREAM variable = fifo_Y_pe27 depth = 2
#pragma HLS STREAM variable = fifo_Y_pe28 depth = 2
#pragma HLS STREAM variable = fifo_Y_pe29 depth = 2
#pragma HLS STREAM variable = fifo_Y_pe30 depth = 2
#pragma HLS STREAM variable = fifo_Y_pe31 depth = 2

#pragma HLS bind_storage variable = fifo_Y_pe0 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_Y_pe1 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_Y_pe2 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_Y_pe3 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_Y_pe4 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_Y_pe5 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_Y_pe6 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_Y_pe7 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_Y_pe8 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_Y_pe9 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_Y_pe10 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_Y_pe11 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_Y_pe12 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_Y_pe13 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_Y_pe14 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_Y_pe15 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_Y_pe16 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_Y_pe17 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_Y_pe18 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_Y_pe19 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_Y_pe20 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_Y_pe21 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_Y_pe22 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_Y_pe23 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_Y_pe24 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_Y_pe25 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_Y_pe26 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_Y_pe27 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_Y_pe28 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_Y_pe29 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_Y_pe30 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_Y_pe31 type = FIFO impl = SRL

  hls::stream<ap_uint<64> > fifo_Y_m0("fifo_Y_m0");
  hls::stream<ap_uint<64> > fifo_Y_m1("fifo_Y_m1");
  hls::stream<ap_uint<64> > fifo_Y_m2("fifo_Y_m2");
  hls::stream<ap_uint<64> > fifo_Y_m3("fifo_Y_m3");
  hls::stream<ap_uint<64> > fifo_Y_m4("fifo_Y_m4");
  hls::stream<ap_uint<64> > fifo_Y_m5("fifo_Y_m5");
  hls::stream<ap_uint<64> > fifo_Y_m6("fifo_Y_m6");
  hls::stream<ap_uint<64> > fifo_Y_m7("fifo_Y_m7");

#pragma HLS STREAM variable = fifo_Y_m0 depth = 2
#pragma HLS STREAM variable = fifo_Y_m1 depth = 2
#pragma HLS STREAM variable = fifo_Y_m2 depth = 2
#pragma HLS STREAM variable = fifo_Y_m3 depth = 2
#pragma HLS STREAM variable = fifo_Y_m4 depth = 2
#pragma HLS STREAM variable = fifo_Y_m5 depth = 2
#pragma HLS STREAM variable = fifo_Y_m6 depth = 2
#pragma HLS STREAM variable = fifo_Y_m7 depth = 2

#pragma HLS bind_storage variable = fifo_Y_m0 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_Y_m1 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_Y_m2 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_Y_m3 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_Y_m4 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_Y_m5 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_Y_m6 type = FIFO impl = SRL
#pragma HLS bind_storage variable = fifo_Y_m7 type = FIFO impl = SRL

  hls::stream<ap_uint<512> > fifo_Y_pe_u512("fifo_Y_pe_u512");
#pragma HLS STREAM variable = fifo_Y_pe_u512 depth = 2
#pragma HLS bind_storage variable = fifo_Y_pe_u512 type = FIFO impl = SRL

  hls::stream<ap_uint<512> > fifo_Y_read_in0("fifo_Y_read_in0");
#pragma HLS STREAM variable = fifo_Y_read_in0 depth = 2
#pragma HLS bind_storage variable = fifo_Y_read_in0 type = FIFO impl = SRL

  hls::stream<ap_uint<512> > fifo_Y_ch0("fifo_Y_ch0");
#pragma HLS STREAM variable = fifo_Y_ch0 depth = 2
#pragma HLS bind_storage variable = fifo_Y_ch0 type = FIFO impl = SRL

#pragma HLS dataflow

  read_edge_list_ptr(NUM_ITE, M, P, K, alpha_u, edge_list_ptr,
                     fifo_edge_list_ptr_pe0);

  read_A(edge_list_ch0, fifo_A_pe0, NUM_A_LEN, P);

  read_A(edge_list_ch1, fifo_A_pe1, NUM_A_LEN, P);

  read_A(edge_list_ch2, fifo_A_pe2, NUM_A_LEN, P);

  read_A(edge_list_ch3, fifo_A_pe3, NUM_A_LEN, P);

  read_A(edge_list_ch4, fifo_A_pe4, NUM_A_LEN, P);

  read_A(edge_list_ch5, fifo_A_pe5, NUM_A_LEN, P);

  read_A(edge_list_ch6, fifo_A_pe6, NUM_A_LEN, P);

  read_A(edge_list_ch7, fifo_A_pe7, NUM_A_LEN, P);

  read_A(edge_list_ch8, fifo_A_pe8, NUM_A_LEN, P);

  read_A(edge_list_ch9, fifo_A_pe9, NUM_A_LEN, P);

  read_A(edge_list_ch10, fifo_A_pe10, NUM_A_LEN, P);

  read_A(edge_list_ch11, fifo_A_pe11, NUM_A_LEN, P);

  read_A(edge_list_ch12, fifo_A_pe12, NUM_A_LEN, P);

  read_A(edge_list_ch13, fifo_A_pe13, NUM_A_LEN, P);

  read_A(edge_list_ch14, fifo_A_pe14, NUM_A_LEN, P);

  read_A(edge_list_ch15, fifo_A_pe15, NUM_A_LEN, P);

  read_A(edge_list_ch16, fifo_A_pe16, NUM_A_LEN, P);

  read_A(edge_list_ch17, fifo_A_pe17, NUM_A_LEN, P);

  read_A(edge_list_ch18, fifo_A_pe18, NUM_A_LEN, P);

  read_A(edge_list_ch19, fifo_A_pe19, NUM_A_LEN, P);

  read_A(edge_list_ch20, fifo_A_pe20, NUM_A_LEN, P);

  read_A(edge_list_ch21, fifo_A_pe21, NUM_A_LEN, P);

  read_A(edge_list_ch22, fifo_A_pe22, NUM_A_LEN, P);

  read_A(edge_list_ch23, fifo_A_pe23, NUM_A_LEN, P);

  read_A(edge_list_ch24, fifo_A_pe24, NUM_A_LEN, P);

  read_A(edge_list_ch25, fifo_A_pe25, NUM_A_LEN, P);

  read_A(edge_list_ch26, fifo_A_pe26, NUM_A_LEN, P);

  read_A(edge_list_ch27, fifo_A_pe27, NUM_A_LEN, P);

  read_A(edge_list_ch28, fifo_A_pe28, NUM_A_LEN, P);

  read_A(edge_list_ch29, fifo_A_pe29, NUM_A_LEN, P);

  read_A(edge_list_ch30, fifo_A_pe30, NUM_A_LEN, P);

  read_A(edge_list_ch31, fifo_A_pe31, NUM_A_LEN, P);

  read_X(vec_X, fifo_X_pe0, K, P);

  PEG<true>(fifo_edge_list_ptr_pe0, fifo_A_pe0, fifo_X_pe0,
            fifo_edge_list_ptr_pe1, fifo_X_pe1, fifo_Y_pe0);

  PEG<false>(fifo_edge_list_ptr_pe1, fifo_A_pe1, fifo_X_pe1,
             fifo_edge_list_ptr_pe2, fifo_X_pe2, fifo_Y_pe1);

  PEG<false>(fifo_edge_list_ptr_pe2, fifo_A_pe2, fifo_X_pe2,
             fifo_edge_list_ptr_pe3, fifo_X_pe3, fifo_Y_pe2);

  PEG<false>(fifo_edge_list_ptr_pe3, fifo_A_pe3, fifo_X_pe3,
             fifo_edge_list_ptr_pe4, fifo_X_pe4, fifo_Y_pe3);

  PEG<true>(fifo_edge_list_ptr_pe4, fifo_A_pe4, fifo_X_pe4,
            fifo_edge_list_ptr_pe5, fifo_X_pe5, fifo_Y_pe4);

  PEG<false>(fifo_edge_list_ptr_pe5, fifo_A_pe5, fifo_X_pe5,
             fifo_edge_list_ptr_pe6, fifo_X_pe6, fifo_Y_pe5);

  PEG<false>(fifo_edge_list_ptr_pe6, fifo_A_pe6, fifo_X_pe6,
             fifo_edge_list_ptr_pe7, fifo_X_pe7, fifo_Y_pe6);

  PEG<false>(fifo_edge_list_ptr_pe7, fifo_A_pe7, fifo_X_pe7,
             fifo_edge_list_ptr_pe8, fifo_X_pe8, fifo_Y_pe7);

  PEG<true>(fifo_edge_list_ptr_pe8, fifo_A_pe8, fifo_X_pe8,
            fifo_edge_list_ptr_pe9, fifo_X_pe9, fifo_Y_pe8);

  PEG<false>(fifo_edge_list_ptr_pe9, fifo_A_pe9, fifo_X_pe9,
             fifo_edge_list_ptr_pe10, fifo_X_pe10, fifo_Y_pe9);

  PEG<false>(fifo_edge_list_ptr_pe10, fifo_A_pe10, fifo_X_pe10,
             fifo_edge_list_ptr_pe11, fifo_X_pe11, fifo_Y_pe10);

  PEG<false>(fifo_edge_list_ptr_pe11, fifo_A_pe11, fifo_X_pe11,
             fifo_edge_list_ptr_pe12, fifo_X_pe12, fifo_Y_pe11);

  PEG<true>(fifo_edge_list_ptr_pe12, fifo_A_pe12, fifo_X_pe12,
            fifo_edge_list_ptr_pe13, fifo_X_pe13, fifo_Y_pe12);

  PEG<false>(fifo_edge_list_ptr_pe13, fifo_A_pe13, fifo_X_pe13,
             fifo_edge_list_ptr_pe14, fifo_X_pe14, fifo_Y_pe13);

  PEG<false>(fifo_edge_list_ptr_pe14, fifo_A_pe14, fifo_X_pe14,
             fifo_edge_list_ptr_pe15, fifo_X_pe15, fifo_Y_pe14);

  PEG<false>(fifo_edge_list_ptr_pe15, fifo_A_pe15, fifo_X_pe15,
             fifo_edge_list_ptr_pe16, fifo_X_pe16, fifo_Y_pe15);

  PEG<true>(fifo_edge_list_ptr_pe16, fifo_A_pe16, fifo_X_pe16,
            fifo_edge_list_ptr_pe17, fifo_X_pe17, fifo_Y_pe16);

  PEG<false>(fifo_edge_list_ptr_pe17, fifo_A_pe17, fifo_X_pe17,
             fifo_edge_list_ptr_pe18, fifo_X_pe18, fifo_Y_pe17);

  PEG<false>(fifo_edge_list_ptr_pe18, fifo_A_pe18, fifo_X_pe18,
             fifo_edge_list_ptr_pe19, fifo_X_pe19, fifo_Y_pe18);

  PEG<false>(fifo_edge_list_ptr_pe19, fifo_A_pe19, fifo_X_pe19,
             fifo_edge_list_ptr_pe20, fifo_X_pe20, fifo_Y_pe19);

  PEG<true>(fifo_edge_list_ptr_pe20, fifo_A_pe20, fifo_X_pe20,
            fifo_edge_list_ptr_pe21, fifo_X_pe21, fifo_Y_pe20);

  PEG<false>(fifo_edge_list_ptr_pe21, fifo_A_pe21, fifo_X_pe21,
             fifo_edge_list_ptr_pe22, fifo_X_pe22, fifo_Y_pe21);

  PEG<false>(fifo_edge_list_ptr_pe22, fifo_A_pe22, fifo_X_pe22,
             fifo_edge_list_ptr_pe23, fifo_X_pe23, fifo_Y_pe22);

  PEG<false>(fifo_edge_list_ptr_pe23, fifo_A_pe23, fifo_X_pe23,
             fifo_edge_list_ptr_pe24, fifo_X_pe24, fifo_Y_pe23);

  PEG<true>(fifo_edge_list_ptr_pe24, fifo_A_pe24, fifo_X_pe24,
            fifo_edge_list_ptr_pe25, fifo_X_pe25, fifo_Y_pe24);

  PEG<false>(fifo_edge_list_ptr_pe25, fifo_A_pe25, fifo_X_pe25,
             fifo_edge_list_ptr_pe26, fifo_X_pe26, fifo_Y_pe25);

  PEG<false>(fifo_edge_list_ptr_pe26, fifo_A_pe26, fifo_X_pe26,
             fifo_edge_list_ptr_pe27, fifo_X_pe27, fifo_Y_pe26);

  PEG<false>(fifo_edge_list_ptr_pe27, fifo_A_pe27, fifo_X_pe27,
             fifo_edge_list_ptr_pe28, fifo_X_pe28, fifo_Y_pe27);

  PEG<true>(fifo_edge_list_ptr_pe28, fifo_A_pe28, fifo_X_pe28,
            fifo_edge_list_ptr_pe29, fifo_X_pe29, fifo_Y_pe28);

  PEG<false>(fifo_edge_list_ptr_pe29, fifo_A_pe29, fifo_X_pe29,
             fifo_edge_list_ptr_pe30, fifo_X_pe30, fifo_Y_pe29);

  PEG<false>(fifo_edge_list_ptr_pe30, fifo_A_pe30, fifo_X_pe30,
             fifo_edge_list_ptr_pe31, fifo_X_pe31, fifo_Y_pe30);

  PEG_last<false>(fifo_edge_list_ptr_pe31, fifo_A_pe31, fifo_X_pe31,
                  fifo_Y_pe31);

  mux_Y<true>(fifo_Y_pe0, fifo_Y_pe1, fifo_Y_pe2, fifo_Y_pe3, fifo_Y_m0);

  mux_Y<false>(fifo_Y_pe4, fifo_Y_pe5, fifo_Y_pe6, fifo_Y_pe7, fifo_Y_m1);

  mux_Y<false>(fifo_Y_pe8, fifo_Y_pe9, fifo_Y_pe10, fifo_Y_pe11, fifo_Y_m2);

  mux_Y<false>(fifo_Y_pe12, fifo_Y_pe13, fifo_Y_pe14, fifo_Y_pe15, fifo_Y_m3);

  mux_Y<false>(fifo_Y_pe16, fifo_Y_pe17, fifo_Y_pe18, fifo_Y_pe19, fifo_Y_m4);

  mux_Y<false>(fifo_Y_pe20, fifo_Y_pe21, fifo_Y_pe22, fifo_Y_pe23, fifo_Y_m5);

  mux_Y<false>(fifo_Y_pe24, fifo_Y_pe25, fifo_Y_pe26, fifo_Y_pe27, fifo_Y_m6);

  mux_Y<false>(fifo_Y_pe28, fifo_Y_pe29, fifo_Y_pe30, fifo_Y_pe31, fifo_Y_m7);

  merge_Y(fifo_Y_m0, fifo_Y_m1, fifo_Y_m2, fifo_Y_m3, fifo_Y_m4, fifo_Y_m5,
          fifo_Y_m6, fifo_Y_m7, fifo_Y_pe_u512);

  read_Y(vec_Y, fifo_Y_read_in0, M, P);

  comp_Y(fifo_Y_read_in0, fifo_Y_pe_u512, fifo_Y_ch0, beta_u);

  write_Y(fifo_Y_ch0, vec_Y_out);
}

#ifndef HLS
}  // end of extern C
#endif
