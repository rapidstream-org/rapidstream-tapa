// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include "kernel_kernel.h"
template <typename T1, typename T2>
inline T1 min(T1 x, T2 y) {
  return (x < T1(y)) ? x : T1(y);
}
template <typename T1, typename T2>
inline T1 max(T1 x, T2 y) {
  return (x > T1(y)) ? x : T1(y);
}

/* Module Definition */
void A_IO_L3_in(tapa::istream<A_t16>& fifo_A_in,
                tapa::ostream<A_t16>& fifo_A_local_out) {
#pragma HLS INLINE OFF
  /* Variable Declaration */
  /* Variable Declaration */

  // array
  // io_L3
  for (ap_uint<5> c3 = 0; c3 <= 9; c3 += 1) {
    // io_L2
    for (ap_uint<4> c4 = 0; c4 <= 7; c4 += 1) {
      // access_coalesce
      // access_serialize
      for (ap_uint<3> c5 = 0; c5 <= 3; c5 += 1) {
#pragma HLS PIPELINE style = stp II = 1
        {
          A_t16 in_data;
          A_t16 out_data;
          in_data = fifo_A_in.read();
          out_data = in_data;
          fifo_A_local_out.write(out_data);
        }
      }
    }
  }
}
/* Module Definition */

/* Module Definition */
void A_IO_L3_in_serialize(tapa::mmap<A_t16> A,
                          tapa::ostream<A_t16>& fifo_A_local_out) {
#pragma HLS INLINE OFF
  /* Variable Declaration */
  /* Variable Declaration */

  for (ap_uint<10> i = 0; i < 320; i++) {
#pragma HLS PIPELINE style = stp II = 1
    A_t16 fifo_data;
    fifo_data = A[i];
    fifo_A_local_out.write(fifo_data);
  }
}
/* Module Definition */

/* Module Definition */
void A_IO_L2_in_intra_trans(int idx, A_t16 local_A[8][4],
                            tapa::ostream<A_t16>& fifo_A_local_out,
                            bool intra_trans_en) {
#pragma HLS INLINE OFF
  /* Variable Declaration */
  int p0 = idx;  // module id
  /* Variable Declaration */

  if (!intra_trans_en) return;

  // array
  // io_L3
  // io_L2
  // io_L1
  // pe
  for (ap_uint<3> c5 = 0; c5 <= 3; c5 += 1) {
    // latency
    for (ap_uint<4> c6 = 0; c6 <= 7; c6 += 1) {
      // latency
      for (ap_uint<4> c7 = 0; c7 <= 7; c7 += 1) {
#pragma HLS PIPELINE style = stp II = 1
        // simd
        {
          A_t16 in_data;
          A_t16 out_data;
          in_data = local_A[c7][c5];
          out_data = in_data;
          fifo_A_local_out.write(out_data);
        }
      }
    }
  }
}
/* Module Definition */

/* Module Definition */
void A_IO_L2_in_inter_trans(int idx, A_t16 local_A[8][4],
                            tapa::istream<A_t16>& fifo_A_in,
                            tapa::ostream<A_t16>& fifo_A_out,
                            bool inter_trans_en) {
#pragma HLS INLINE OFF
  /* Variable Declaration */
  int p0 = idx;  // module id
  /* Variable Declaration */

  if (!inter_trans_en) return;

  for (ap_uint<5> c3 = p0; c3 <= 9; c3 += 1) {
    // io_L2
    if (c3 == p0) {
      for (ap_uint<4> c4 = 0; c4 <= 7; c4 += 1) {
        // access_coalesce
        for (ap_uint<3> c5 = 0; c5 <= 3; c5 += 1) {
#pragma HLS PIPELINE style = stp II = 1
          {
            A_t16 in_data;
            A_t16 out_data;
            in_data = fifo_A_in.read();
            out_data = in_data;
            local_A[c4][c5] = out_data;
          }
        }
      }
    } else {
      for (ap_uint<4> c4 = 0; c4 <= 7; c4 += 1) {
        // access_coalesce
        for (ap_uint<3> c5 = 0; c5 <= 3; c5 += 1) {
#pragma HLS PIPELINE style = stp II = 1
          {
            A_t16 in_data;
            A_t16 out_data;
            in_data = fifo_A_in.read();
            out_data = in_data;
            fifo_A_out.write(out_data);
          }
        }
      }
    }
  }
}
/* Module Definition */

/* Module Definition */
void A_IO_L2_in_inter_trans_boundary(int idx, A_t16 local_A[8][4],
                                     tapa::istream<A_t16>& fifo_A_in,
                                     bool inter_trans_en) {
#pragma HLS INLINE OFF
  /* Variable Declaration */
  int p0 = idx;  // module id
  /* Variable Declaration */

  if (!inter_trans_en) return;

  for (ap_uint<5> c3 = p0; c3 <= 9; c3 += 1)
    if (c3 == p0) {
      // io_L2
      for (ap_uint<4> c4 = 0; c4 <= 7; c4 += 1) {
        // access_coalesce
        for (ap_uint<3> c5 = 0; c5 <= 3; c5 += 1) {
#pragma HLS PIPELINE style = stp II = 1
          {
            A_t16 in_data;
            A_t16 out_data;
            in_data = fifo_A_in.read();
            out_data = in_data;
            local_A[c4][c5] = out_data;
          }
        }
      }
    }
}
/* Module Definition */

/* Module Definition */
void A_IO_L2_in(int idx, tapa::istream<A_t16>& fifo_A_in,
                tapa::ostream<A_t16>& fifo_A_out,
                tapa::ostream<A_t16>& fifo_A_local_out) {
#pragma HLS INLINE OFF
  /* Variable Declaration */
  int p0 = idx;  // module id
  A_t16 local_A_ping[8][4];
#pragma HLS RESOURCE variable = local_A_ping core = RAM_1P_BRAM
  A_t16 local_A_pong[8][4];
#pragma HLS RESOURCE variable = local_A_pong core = RAM_1P_BRAM
  bool arb = 0;
  bool inter_trans_en = 1;
  bool intra_trans_en = 0;
  /* Variable Declaration */

  {
    // array
    // io_L3
    {
      if (arb == 0) {
        A_IO_L2_in_inter_trans(
            /* module id */ idx,
            /* array */ local_A_pong,
            /* fifo */ fifo_A_in,
            /* fifo */ fifo_A_out,
            /* enable */ inter_trans_en);
        A_IO_L2_in_intra_trans(
            /* module id */ idx,
            /* array */ local_A_ping,
            /* fifo */ fifo_A_local_out,
            /* enable */ intra_trans_en);
      } else {
        A_IO_L2_in_inter_trans(
            /* module id */ idx,
            /* array */ local_A_ping,
            /* fifo */ fifo_A_in,
            /* fifo */ fifo_A_out,
            /* enable */ inter_trans_en);
        A_IO_L2_in_intra_trans(
            /* module id */ idx,
            /* array */ local_A_pong,
            /* fifo */ fifo_A_local_out,
            /* enable */ intra_trans_en);
      }
      intra_trans_en = 1;
      arb = !arb;
    }
    if (arb == 0) {
      A_IO_L2_in_intra_trans(
          /* module id */ idx,
          /* array */ local_A_ping,
          /* fifo */ fifo_A_local_out,
          /* enable */ intra_trans_en);
    } else {
      A_IO_L2_in_intra_trans(
          /* module id */ idx,
          /* array */ local_A_pong,
          /* fifo */ fifo_A_local_out,
          /* enable */ intra_trans_en);
    }
  }
}
/* Module Definition */

/* Module Definition */
void A_IO_L2_in_boundary(int idx, tapa::istream<A_t16>& fifo_A_in,
                         tapa::ostream<A_t16>& fifo_A_local_out) {
#pragma HLS INLINE OFF
  /* Variable Declaration */
  int p0 = idx;  // module id
  A_t16 local_A_ping[8][4];
#pragma HLS RESOURCE variable = local_A_ping core = RAM_1P_BRAM
  A_t16 local_A_pong[8][4];
#pragma HLS RESOURCE variable = local_A_pong core = RAM_1P_BRAM
  bool arb = 0;
  bool inter_trans_en = 1;
  bool intra_trans_en = 0;
  /* Variable Declaration */

  {
    // array
    // io_L3
    {
      if (arb == 0) {
        A_IO_L2_in_inter_trans_boundary(
            /* module id */ idx,
            /* array */ local_A_pong,
            /* fifo */ fifo_A_in,
            /* enable */ inter_trans_en);
        A_IO_L2_in_intra_trans(
            /* module id */ idx,
            /* array */ local_A_ping,
            /* fifo */ fifo_A_local_out,
            /* enable */ intra_trans_en);
      } else {
        A_IO_L2_in_inter_trans_boundary(
            /* module id */ idx,
            /* array */ local_A_ping,
            /* fifo */ fifo_A_in,
            /* enable */ inter_trans_en);
        A_IO_L2_in_intra_trans(
            /* module id */ idx,
            /* array */ local_A_pong,
            /* fifo */ fifo_A_local_out,
            /* enable */ intra_trans_en);
      }
      intra_trans_en = 1;
      arb = !arb;
    }
    if (arb == 0) {
      A_IO_L2_in_intra_trans(
          /* module id */ idx,
          /* array */ local_A_ping,
          /* fifo */ fifo_A_local_out,
          /* enable */ intra_trans_en);
    } else {
      A_IO_L2_in_intra_trans(
          /* module id */ idx,
          /* array */ local_A_pong,
          /* fifo */ fifo_A_local_out,
          /* enable */ intra_trans_en);
    }
  }
}
/* Module Definition */

/* Module Definition */
void B_IO_L3_in(tapa::istream<B_t16>& fifo_B_in,
                tapa::ostream<B_t16>& fifo_B_local_out) {
#pragma HLS INLINE OFF
  /* Variable Declaration */
  /* Variable Declaration */

  // array
  // io_L3
  for (ap_uint<5> c3 = 0; c3 <= 12; c3 += 1) {
    // io_L2
    for (ap_uint<4> c4 = 0; c4 <= 7; c4 += 1) {
      // access_coalesce
      // access_serialize
      for (ap_uint<3> c5 = 0; c5 <= 3; c5 += 1) {
#pragma HLS PIPELINE style = stp II = 1
        {
          B_t16 in_data;
          B_t16 out_data;
          in_data = fifo_B_in.read();
          out_data = in_data;
          fifo_B_local_out.write(out_data);
        }
      }
    }
  }
}
/* Module Definition */

/* Module Definition */
void B_IO_L3_in_serialize(tapa::mmap<B_t16> B,
                          tapa::ostream<B_t16>& fifo_B_local_out) {
#pragma HLS INLINE OFF
  /* Variable Declaration */
  /* Variable Declaration */

  for (ap_uint<10> i = 0; i < 416; i++) {
#pragma HLS PIPELINE style = stp II = 1
    B_t16 fifo_data;
    fifo_data = B[i];
    fifo_B_local_out.write(fifo_data);
  }
}
/* Module Definition */

/* Module Definition */
void B_IO_L2_in_intra_trans(int idx, B_t16 local_B[8][4],
                            tapa::ostream<B_t16>& fifo_B_local_out,
                            bool intra_trans_en) {
#pragma HLS INLINE OFF
  /* Variable Declaration */
  int p0 = idx;  // module id
  /* Variable Declaration */

  if (!intra_trans_en) return;

  // array
  // io_L3
  // io_L2
  // io_L1
  // pe
  for (ap_uint<3> c5 = 0; c5 <= 3; c5 += 1) {
    // latency
    for (ap_uint<4> c6 = 0; c6 <= 7; c6 += 1) {
      // latency
      for (ap_uint<4> c7 = 0; c7 <= 7; c7 += 1) {
#pragma HLS PIPELINE style = stp II = 1
        // simd
        {
          B_t16 in_data;
          B_t16 out_data;
          in_data = local_B[c6][c5];
          out_data = in_data;
          fifo_B_local_out.write(out_data);
        }
      }
    }
  }
}
/* Module Definition */

/* Module Definition */
void B_IO_L2_in_inter_trans(int idx, B_t16 local_B[8][4],
                            tapa::istream<B_t16>& fifo_B_in,
                            tapa::ostream<B_t16>& fifo_B_out,
                            bool inter_trans_en) {
#pragma HLS INLINE OFF
  /* Variable Declaration */
  int p0 = idx;  // module id
  /* Variable Declaration */

  if (!inter_trans_en) return;

  for (ap_uint<5> c3 = p0; c3 <= 12; c3 += 1) {
    // io_L2
    if (c3 == p0) {
      for (ap_uint<4> c4 = 0; c4 <= 7; c4 += 1) {
        // access_coalesce
        for (ap_uint<3> c5 = 0; c5 <= 3; c5 += 1) {
#pragma HLS PIPELINE style = stp II = 1
          {
            B_t16 in_data;
            B_t16 out_data;
            in_data = fifo_B_in.read();
            out_data = in_data;
            local_B[c4][c5] = out_data;
          }
        }
      }
    } else {
      for (ap_uint<4> c4 = 0; c4 <= 7; c4 += 1) {
        // access_coalesce
        for (ap_uint<3> c5 = 0; c5 <= 3; c5 += 1) {
#pragma HLS PIPELINE style = stp II = 1
          {
            B_t16 in_data;
            B_t16 out_data;
            in_data = fifo_B_in.read();
            out_data = in_data;
            fifo_B_out.write(out_data);
          }
        }
      }
    }
  }
}
/* Module Definition */

/* Module Definition */
void B_IO_L2_in_inter_trans_boundary(int idx, B_t16 local_B[8][4],
                                     tapa::istream<B_t16>& fifo_B_in,
                                     bool inter_trans_en) {
#pragma HLS INLINE OFF
  /* Variable Declaration */
  int p0 = idx;  // module id
  /* Variable Declaration */

  if (!inter_trans_en) return;

  for (ap_uint<5> c3 = p0; c3 <= 12; c3 += 1)
    if (c3 == p0) {
      // io_L2
      for (ap_uint<4> c4 = 0; c4 <= 7; c4 += 1) {
        // access_coalesce
        for (ap_uint<3> c5 = 0; c5 <= 3; c5 += 1) {
#pragma HLS PIPELINE style = stp II = 1
          {
            B_t16 in_data;
            B_t16 out_data;
            in_data = fifo_B_in.read();
            out_data = in_data;
            local_B[c4][c5] = out_data;
          }
        }
      }
    }
}
/* Module Definition */

/* Module Definition */
void B_IO_L2_in(int idx, tapa::istream<B_t16>& fifo_B_in,
                tapa::ostream<B_t16>& fifo_B_out,
                tapa::ostream<B_t16>& fifo_B_local_out) {
#pragma HLS INLINE OFF
  /* Variable Declaration */
  int p0 = idx;  // module id
  B_t16 local_B_ping[8][4];
#pragma HLS RESOURCE variable = local_B_ping core = RAM_1P_BRAM
  B_t16 local_B_pong[8][4];
#pragma HLS RESOURCE variable = local_B_pong core = RAM_1P_BRAM
  bool arb = 0;
  bool inter_trans_en = 1;
  bool intra_trans_en = 0;
  /* Variable Declaration */

  {
    // array
    // io_L3
    {
      if (arb == 0) {
        B_IO_L2_in_inter_trans(
            /* module id */ idx,
            /* array */ local_B_pong,
            /* fifo */ fifo_B_in,
            /* fifo */ fifo_B_out,
            /* enable */ inter_trans_en);
        B_IO_L2_in_intra_trans(
            /* module id */ idx,
            /* array */ local_B_ping,
            /* fifo */ fifo_B_local_out,
            /* enable */ intra_trans_en);
      } else {
        B_IO_L2_in_inter_trans(
            /* module id */ idx,
            /* array */ local_B_ping,
            /* fifo */ fifo_B_in,
            /* fifo */ fifo_B_out,
            /* enable */ inter_trans_en);
        B_IO_L2_in_intra_trans(
            /* module id */ idx,
            /* array */ local_B_pong,
            /* fifo */ fifo_B_local_out,
            /* enable */ intra_trans_en);
      }
      intra_trans_en = 1;
      arb = !arb;
    }
    if (arb == 0) {
      B_IO_L2_in_intra_trans(
          /* module id */ idx,
          /* array */ local_B_ping,
          /* fifo */ fifo_B_local_out,
          /* enable */ intra_trans_en);
    } else {
      B_IO_L2_in_intra_trans(
          /* module id */ idx,
          /* array */ local_B_pong,
          /* fifo */ fifo_B_local_out,
          /* enable */ intra_trans_en);
    }
  }
}
/* Module Definition */

/* Module Definition */
void B_IO_L2_in_boundary(int idx, tapa::istream<B_t16>& fifo_B_in,
                         tapa::ostream<B_t16>& fifo_B_local_out) {
#pragma HLS INLINE OFF
  /* Variable Declaration */
  int p0 = idx;  // module id
  B_t16 local_B_ping[8][4];
#pragma HLS RESOURCE variable = local_B_ping core = RAM_1P_BRAM
  B_t16 local_B_pong[8][4];
#pragma HLS RESOURCE variable = local_B_pong core = RAM_1P_BRAM
  bool arb = 0;
  bool inter_trans_en = 1;
  bool intra_trans_en = 0;
  /* Variable Declaration */

  {
    // array
    // io_L3
    {
      if (arb == 0) {
        B_IO_L2_in_inter_trans_boundary(
            /* module id */ idx,
            /* array */ local_B_pong,
            /* fifo */ fifo_B_in,
            /* enable */ inter_trans_en);
        B_IO_L2_in_intra_trans(
            /* module id */ idx,
            /* array */ local_B_ping,
            /* fifo */ fifo_B_local_out,
            /* enable */ intra_trans_en);
      } else {
        B_IO_L2_in_inter_trans_boundary(
            /* module id */ idx,
            /* array */ local_B_ping,
            /* fifo */ fifo_B_in,
            /* enable */ inter_trans_en);
        B_IO_L2_in_intra_trans(
            /* module id */ idx,
            /* array */ local_B_pong,
            /* fifo */ fifo_B_local_out,
            /* enable */ intra_trans_en);
      }
      intra_trans_en = 1;
      arb = !arb;
    }
    if (arb == 0) {
      B_IO_L2_in_intra_trans(
          /* module id */ idx,
          /* array */ local_B_ping,
          /* fifo */ fifo_B_local_out,
          /* enable */ intra_trans_en);
    } else {
      B_IO_L2_in_intra_trans(
          /* module id */ idx,
          /* array */ local_B_pong,
          /* fifo */ fifo_B_local_out,
          /* enable */ intra_trans_en);
    }
  }
}
/* Module Definition */

/* Module Definition */
void PE(int idx, int idy, tapa::istream<A_t16>& fifo_A_in,
        tapa::ostream<A_t16>& fifo_A_out, tapa::istream<B_t16>& fifo_B_in,
        tapa::ostream<B_t16>& fifo_B_out,
        tapa::ostream<float>& fifo_C_drain_out) {
#pragma HLS INLINE OFF
  /* Variable Declaration */
  int p0 = idx, p1 = idy;  // module id
  A_t1 local_A[1][16];
#pragma HLS ARRAY_PARTITION variable = local_A dim = 2 factor = 16 cyclic
#pragma HLS RESOURCE variable = local_A core = RAM_2P_BRAM
  B_t1 local_B[1][16];
#pragma HLS ARRAY_PARTITION variable = local_B dim = 2 factor = 16 cyclic
#pragma HLS RESOURCE variable = local_B core = RAM_2P_BRAM
  C_t1 local_C[8][8];
#pragma HLS RESOURCE variable = local_C core = RAM_2P_BRAM
  /* Variable Declaration */

  // array
  // pe
  for (ap_uint<3> c5 = 0; c5 <= 3; c5 += 1) {
    // latency
    for (ap_uint<4> c6 = 0; c6 <= 7; c6 += 1) {
      // latency
      for (ap_uint<4> c7 = 0; c7 <= 7; c7 += 1) {
#pragma HLS PIPELINE style = stp II = 1
        {
          {
            A_t16 fifo_data;
            fifo_data = fifo_A_in.read();
            for (ap_uint<5> n = 0; n < 16; n++) {
#pragma HLS UNROLL
              local_A[0][n] = fifo_data[n];
            }
          }
          {
            B_t16 fifo_data;
            fifo_data = fifo_B_in.read();
            for (ap_uint<5> n = 0; n < 16; n++) {
#pragma HLS UNROLL
              local_B[0][n] = fifo_data[n];
            }
          }
          // simd
          {
            if (c5 == 0) {
              // hls_unroll
              local_C[c7][c6] = 0;
            }
            for (ap_uint<5> c8 = 0; c8 <= 15; c8 += 1) {
#pragma HLS UNROLL
              local_C[c7][c6] =
                  (local_C[c7][c6] + (local_A[0][c8] * local_B[0][c8]));
            }
          }
          if (c5 == 3) fifo_C_drain_out.write(local_C[c7][c6]);
          {
            B_t16 fifo_data;
            float f15, f14, f13, f12, f11, f10, f9, f8, f7, f6, f5, f4, f3, f2,
                f1, f0;
            f15 = local_B[0][15];
            f14 = local_B[0][14];
            f13 = local_B[0][13];
            f12 = local_B[0][12];
            f11 = local_B[0][11];
            f10 = local_B[0][10];
            f9 = local_B[0][9];
            f8 = local_B[0][8];
            f7 = local_B[0][7];
            f6 = local_B[0][6];
            f5 = local_B[0][5];
            f4 = local_B[0][4];
            f3 = local_B[0][3];
            f2 = local_B[0][2];
            f1 = local_B[0][1];
            f0 = local_B[0][0];
            fifo_data.set(15, f15);
            fifo_data.set(14, f14);
            fifo_data.set(13, f13);
            fifo_data.set(12, f12);
            fifo_data.set(11, f11);
            fifo_data.set(10, f10);
            fifo_data.set(9, f9);
            fifo_data.set(8, f8);
            fifo_data.set(7, f7);
            fifo_data.set(6, f6);
            fifo_data.set(5, f5);
            fifo_data.set(4, f4);
            fifo_data.set(3, f3);
            fifo_data.set(2, f2);
            fifo_data.set(1, f1);
            fifo_data.set(0, f0);
            fifo_B_out.write(fifo_data);
          }
          {
            A_t16 fifo_data;
            float f15, f14, f13, f12, f11, f10, f9, f8, f7, f6, f5, f4, f3, f2,
                f1, f0;
            f15 = local_A[0][15];
            f14 = local_A[0][14];
            f13 = local_A[0][13];
            f12 = local_A[0][12];
            f11 = local_A[0][11];
            f10 = local_A[0][10];
            f9 = local_A[0][9];
            f8 = local_A[0][8];
            f7 = local_A[0][7];
            f6 = local_A[0][6];
            f5 = local_A[0][5];
            f4 = local_A[0][4];
            f3 = local_A[0][3];
            f2 = local_A[0][2];
            f1 = local_A[0][1];
            f0 = local_A[0][0];
            fifo_data.set(15, f15);
            fifo_data.set(14, f14);
            fifo_data.set(13, f13);
            fifo_data.set(12, f12);
            fifo_data.set(11, f11);
            fifo_data.set(10, f10);
            fifo_data.set(9, f9);
            fifo_data.set(8, f8);
            fifo_data.set(7, f7);
            fifo_data.set(6, f6);
            fifo_data.set(5, f5);
            fifo_data.set(4, f4);
            fifo_data.set(3, f3);
            fifo_data.set(2, f2);
            fifo_data.set(1, f1);
            fifo_data.set(0, f0);
            fifo_A_out.write(fifo_data);
          }
        }
      }
    }
  }
}
/* Module Definition */

/* Module Definition */
void PE_wrapper(int idx, int idy, tapa::istream<A_t16>& fifo_A_in,
                tapa::ostream<A_t16>& fifo_A_out,
                tapa::istream<B_t16>& fifo_B_in,
                tapa::ostream<B_t16>& fifo_B_out,
                tapa::ostream<float>& fifo_C_drain_out) {
  PE(
      /* module id */ idx,
      /* module id */ idy,
      /* fifo */ fifo_A_in,
      /* fifo */ fifo_A_out,
      /* fifo */ fifo_B_in,
      /* fifo */ fifo_B_out,
      /* fifo */ fifo_C_drain_out);
}
/* Module Definition */

/* Module Definition */
void A_PE_dummy_in(int idx, int idy, tapa::istream<A_t16>& fifo_A_in) {
  /* Variable Declaration */
  int p0 = idx, p1 = idy;  // module id
  /* Variable Declaration */

  // array
  // pe
  for (ap_uint<3> c5 = 0; c5 <= 3; c5 += 1) {
    // latency
    for (ap_uint<4> c6 = 0; c6 <= 7; c6 += 1) {
      // latency
      for (ap_uint<4> c7 = 0; c7 <= 7; c7 += 1) {
#pragma HLS PIPELINE style = stp II = 1
        A_t16 fifo_data;
        fifo_data = fifo_A_in.read();
      }
    }
  }
}
/* Module Definition */

/* Module Definition */
void B_PE_dummy_in(int idx, int idy, tapa::istream<B_t16>& fifo_B_in) {
  /* Variable Declaration */
  int p0 = idx, p1 = idy;  // module id
  /* Variable Declaration */

  // array
  // pe
  for (ap_uint<3> c5 = 0; c5 <= 3; c5 += 1) {
    // latency
    for (ap_uint<4> c6 = 0; c6 <= 7; c6 += 1) {
      // latency
      for (ap_uint<4> c7 = 0; c7 <= 7; c7 += 1) {
#pragma HLS PIPELINE style = stp II = 1
        B_t16 fifo_data;
        fifo_data = fifo_B_in.read();
      }
    }
  }
}
/* Module Definition */

/* Module Definition */
void C_drain_IO_L1_out_intra_trans(
    int idx, int idy, C_t4 local_C[8][2],
    tapa::istream<float>& fifo_C_drain_local_in) {
#pragma HLS INLINE
  /* Variable Declaration */
  int p0 = idx, p1 = idy;  // module id
  float data_split[4];
#pragma HLS ARRAY_PARTITION variable = data_split complete
  /* Variable Declaration */

  // io_L1
  // pe
  // latency
  for (ap_uint<4> c6 = 0; c6 <= 7; c6 += 1) {
    // latency
    for (ap_uint<4> c7 = 0; c7 <= 7; c7 += 1) {
#pragma HLS PIPELINE style = stp II = 1
      // simd
      {
        C_t1 in_data;
        C_t4 out_data;
        in_data = fifo_C_drain_local_in.read();
        int split_idx = (c6) % 4;
        out_data = local_C[c7][c6 / 4];
        for (ap_uint<3> n = 0; n < 4; n++) {
#pragma HLS UNROLL
          data_split[n] = out_data[n];
        }
        data_split[split_idx] = in_data;
        out_data.set(0, data_split[0]);
        out_data.set(1, data_split[1]);
        out_data.set(2, data_split[2]);
        out_data.set(3, data_split[3]);
        local_C[c7][c6 / 4] = out_data;
      }
    }
  }
}
/* Module Definition */

/* Module Definition */
void C_drain_IO_L1_out_inter_trans(int idx, int idy, C_t4 local_C[8][2],
                                   tapa::istream<C_t4>& fifo_C_drain_in,
                                   tapa::ostream<C_t4>& fifo_C_drain_out) {
#pragma HLS INLINE
  /* Variable Declaration */
  int p0 = idx, p1 = idy;  // module id
  /* Variable Declaration */

  for (ap_uint<5> c4 = p1; c4 <= 9; c4 += 1) {
    // io_L1
    if (c4 == p1) {
      for (ap_uint<4> c5 = 0; c5 <= 7; c5 += 1) {
        // access_coalesce
        for (ap_uint<2> c6 = 0; c6 <= 1; c6 += 1) {
#pragma HLS PIPELINE style = stp II = 1
          {
            C_t4 in_data;
            C_t4 out_data;
            in_data = local_C[c5][c6];
            out_data = in_data;
            fifo_C_drain_out.write(out_data);
          }
        }
      }
    } else {
      for (ap_uint<4> c5 = 0; c5 <= 7; c5 += 1) {
        // access_coalesce
        for (ap_uint<2> c6 = 0; c6 <= 1; c6 += 1) {
#pragma HLS PIPELINE style = stp II = 1
          {
            C_t4 in_data;
            C_t4 out_data;
            in_data = fifo_C_drain_in.read();
            out_data = in_data;
            fifo_C_drain_out.write(out_data);
          }
        }
      }
    }
  }
}
/* Module Definition */

/* Module Definition */
void C_drain_IO_L1_out_inter_trans_boundary(
    int idx, int idy, C_t4 local_C[8][2],
    tapa::ostream<C_t4>& fifo_C_drain_out) {
#pragma HLS INLINE
  /* Variable Declaration */
  int p0 = idx, p1 = idy;  // module id
  /* Variable Declaration */

  for (ap_uint<5> c4 = p1; c4 <= 9; c4 += 1)
    if (c4 == p1) {
      // io_L1
      for (ap_uint<4> c5 = 0; c5 <= 7; c5 += 1) {
        // access_coalesce
        for (ap_uint<2> c6 = 0; c6 <= 1; c6 += 1) {
#pragma HLS PIPELINE style = stp II = 1
          {
            C_t4 in_data;
            C_t4 out_data;
            in_data = local_C[c5][c6];
            out_data = in_data;
            fifo_C_drain_out.write(out_data);
          }
        }
      }
    }
}
/* Module Definition */

/* Module Definition */
void C_drain_IO_L1_out(int idx, int idy, tapa::istream<C_t4>& fifo_C_drain_in,
                       tapa::ostream<C_t4>& fifo_C_drain_out,
                       tapa::istream<float>& fifo_C_drain_local_in) {
#pragma HLS INLINE OFF
  /* Variable Declaration */
  int p0 = idx, p1 = idy;  // module id
  C_t4 local_C[8][2];
#pragma HLS RESOURCE variable = local_C core = RAM_2P_BRAM
  /* Variable Declaration */

  // array
  // io_L3
  // io_L2
  C_drain_IO_L1_out_intra_trans(
      /* module id */ idx,
      /* module id */ idy,
      /* array */ local_C,
      /* fifo */ fifo_C_drain_local_in);
  C_drain_IO_L1_out_inter_trans(
      /* module id */ idx,
      /* module id */ idy,
      /* array */ local_C,
      /* fifo */ fifo_C_drain_in,
      /* fifo */ fifo_C_drain_out);
}
/* Module Definition */

/* Module Definition */
void C_drain_IO_L1_out_wrapper(int idx, int idy,
                               tapa::istream<C_t4>& fifo_C_drain_in,
                               tapa::ostream<C_t4>& fifo_C_drain_out,
                               tapa::istream<float>& fifo_C_drain_local_in) {
  C_drain_IO_L1_out(
      /* module id */ idx,
      /* module id */ idy,
      /* fifo */ fifo_C_drain_in,
      /* fifo */ fifo_C_drain_out,
      /* fifo */ fifo_C_drain_local_in);
}
/* Module Definition */

/* Module Definition */
void C_drain_IO_L1_out_boundary(int idx, int idy,
                                tapa::ostream<C_t4>& fifo_C_drain_out,
                                tapa::istream<float>& fifo_C_drain_local_in) {
#pragma HLS INLINE
  /* Variable Declaration */
  int p0 = idx, p1 = idy;  // module id
  C_t4 local_C[8][2];
#pragma HLS RESOURCE variable = local_C core = RAM_2P_BRAM
  /* Variable Declaration */

  // array
  // io_L3
  // io_L2
  C_drain_IO_L1_out_intra_trans(
      /* module id */ idx,
      /* module id */ idy,
      /* array */ local_C,
      /* fifo */ fifo_C_drain_local_in);
  C_drain_IO_L1_out_inter_trans_boundary(
      /* module id */ idx,
      /* module id */ idy,
      /* array */ local_C,
      /* fifo */ fifo_C_drain_out);
}
/* Module Definition */

/* Module Definition */
void C_drain_IO_L1_out_boundary_wrapper(
    int idx, int idy, tapa::ostream<C_t4>& fifo_C_drain_out,
    tapa::istream<float>& fifo_C_drain_local_in) {
  C_drain_IO_L1_out_boundary(
      /* module id */ idx,
      /* module id */ idy,
      /* fifo */ fifo_C_drain_out,
      /* fifo */ fifo_C_drain_local_in);
}
/* Module Definition */

/* Module Definition */
void C_drain_IO_L2_out(int idx, tapa::istream<C_t4>& fifo_C_drain_in,
                       tapa::ostream<C_t4>& fifo_C_drain_out,
                       tapa::istream<C_t4>& fifo_C_drain_local_in) {
#pragma HLS INLINE OFF
  /* Variable Declaration */
  int p0 = idx;  // module id
  /* Variable Declaration */

  // array
  // io_L3
  for (ap_uint<5> c3 = p0; c3 <= 12; c3 += 1) {
    // io_L2
    if (c3 == p0) {
      for (ap_uint<5> c4 = 0; c4 <= 9; c4 += 1) {
        // io_L1
        for (ap_uint<4> c5 = 0; c5 <= 7; c5 += 1) {
          // access_coalesce
          for (ap_uint<2> c6 = 0; c6 <= 1; c6 += 1) {
#pragma HLS PIPELINE style = stp II = 1
            {
              C_t4 in_data;
              C_t4 out_data;
              in_data = fifo_C_drain_local_in.read();
              out_data = in_data;
              fifo_C_drain_out.write(out_data);
            }
          }
        }
      }
    } else {
      for (ap_uint<5> c4 = 0; c4 <= 9; c4 += 1) {
        // io_L1
        for (ap_uint<4> c5 = 0; c5 <= 7; c5 += 1) {
          // access_coalesce
          for (ap_uint<2> c6 = 0; c6 <= 1; c6 += 1) {
#pragma HLS PIPELINE style = stp II = 1
            {
              C_t4 in_data;
              C_t4 out_data;
              in_data = fifo_C_drain_in.read();
              out_data = in_data;
              fifo_C_drain_out.write(out_data);
            }
          }
        }
      }
    }
  }
}
/* Module Definition */

/* Module Definition */
void C_drain_IO_L2_out_boundary(int idx, tapa::ostream<C_t4>& fifo_C_drain_out,
                                tapa::istream<C_t4>& fifo_C_drain_local_in) {
#pragma HLS INLINE OFF
  /* Variable Declaration */
  int p0 = idx;  // module id
  /* Variable Declaration */

  // array
  // io_L3
  for (ap_uint<5> c3 = p0; c3 <= 12; c3 += 1)
    if (c3 == p0) {
      // io_L2
      for (ap_uint<5> c4 = 0; c4 <= 9; c4 += 1) {
        // io_L1
        for (ap_uint<4> c5 = 0; c5 <= 7; c5 += 1) {
          // access_coalesce
          for (ap_uint<2> c6 = 0; c6 <= 1; c6 += 1) {
#pragma HLS PIPELINE style = stp II = 1
            {
              C_t4 in_data;
              C_t4 out_data;
              in_data = fifo_C_drain_local_in.read();
              out_data = in_data;
              fifo_C_drain_out.write(out_data);
            }
          }
        }
      }
    }
}
/* Module Definition */

/* Module Definition */
void C_drain_IO_L3_out(tapa::ostream<C_t4>& fifo_C_drain_out,
                       tapa::istream<C_t4>& fifo_C_drain_local_in) {
#pragma HLS INLINE OFF
  /* Variable Declaration */
  /* Variable Declaration */

  // array
  // io_L3
  for (ap_uint<5> c3 = 0; c3 <= 12; c3 += 1) {
    // io_L2
    for (ap_uint<5> c4 = 0; c4 <= 9; c4 += 1) {
      // io_L1
      for (ap_uint<4> c5 = 0; c5 <= 7; c5 += 1) {
        // access_coalesce
        // access_serialize
        for (ap_uint<2> c6 = 0; c6 <= 1; c6 += 1) {
#pragma HLS PIPELINE style = stp II = 1
          {
            C_t4 in_data;
            C_t4 out_data;
            in_data = fifo_C_drain_local_in.read();
            out_data = in_data;
            fifo_C_drain_out.write(out_data);
          }
        }
      }
    }
  }
}
/* Module Definition */

/* Module Definition */
void C_drain_IO_L3_out_serialize(tapa::mmap<C_t16> C,
                                 tapa::istream<C_t4>& fifo_C_drain_local_in) {
#pragma HLS INLINE OFF
  /* Variable Declaration */
  /* Variable Declaration */

  for (ap_uint<11> i = 0; i < 520; i++) {
#pragma HLS PIPELINE style = stp II = 1
    C_t4 fifo_data;
    C_t16 mem_data;
    C_t4 mem_data_split[4];
#pragma HLS ARRAY_PARTITION variable = mem_data_split complete
    for (ap_uint<3> p = 0; p < 4; p++) {
      fifo_data = fifo_C_drain_local_in.read();
      mem_data_split[p] = fifo_data;
    }
    mem_data.set(0, mem_data_split[0][0]);
    mem_data.set(1, mem_data_split[0][1]);
    mem_data.set(2, mem_data_split[0][2]);
    mem_data.set(3, mem_data_split[0][3]);
    mem_data.set(4, mem_data_split[1][0]);
    mem_data.set(5, mem_data_split[1][1]);
    mem_data.set(6, mem_data_split[1][2]);
    mem_data.set(7, mem_data_split[1][3]);
    mem_data.set(8, mem_data_split[2][0]);
    mem_data.set(9, mem_data_split[2][1]);
    mem_data.set(10, mem_data_split[2][2]);
    mem_data.set(11, mem_data_split[2][3]);
    mem_data.set(12, mem_data_split[3][0]);
    mem_data.set(13, mem_data_split[3][1]);
    mem_data.set(14, mem_data_split[3][2]);
    mem_data.set(15, mem_data_split[3][3]);
    C[i] = mem_data;
  }
}
/* Module Definition */

void kernel0(tapa::mmap<A_t16> A, tapa::mmap<B_t16> B, tapa::mmap<C_t16> C) {
  /* FIFO Declaration */
  /* A_IO_L3_in_serialize fifo */ tapa::stream<A_t16, 2>
      fifo_A_A_IO_L3_in_serialize;
  /* B_IO_L3_in_serialize fifo */ tapa::stream<B_t16, 2>
      fifo_B_B_IO_L3_in_serialize;
  /* C_drain_IO_L3_out_serialize fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L3_out_serialize;
  /* A_IO_L2_in fifo */ tapa::stream<A_t16, 2> fifo_A_A_IO_L2_in_0(
      "fifo_A_A_IO_L2_in_0");
  /* A_IO_L2_in fifo */ tapa::stream<A_t16, 2> fifo_A_A_IO_L2_in_1(
      "fifo_A_A_IO_L2_in_1");
  /* A_IO_L2_in fifo */ tapa::stream<A_t16, 2> fifo_A_A_IO_L2_in_2(
      "fifo_A_A_IO_L2_in_2");
  /* A_IO_L2_in fifo */ tapa::stream<A_t16, 2> fifo_A_A_IO_L2_in_3(
      "fifo_A_A_IO_L2_in_3");
  /* A_IO_L2_in fifo */ tapa::stream<A_t16, 2> fifo_A_A_IO_L2_in_4(
      "fifo_A_A_IO_L2_in_4");
  /* A_IO_L2_in fifo */ tapa::stream<A_t16, 2> fifo_A_A_IO_L2_in_5(
      "fifo_A_A_IO_L2_in_5");
  /* A_IO_L2_in fifo */ tapa::stream<A_t16, 2> fifo_A_A_IO_L2_in_6(
      "fifo_A_A_IO_L2_in_6");
  /* A_IO_L2_in fifo */ tapa::stream<A_t16, 2> fifo_A_A_IO_L2_in_7(
      "fifo_A_A_IO_L2_in_7");
  /* A_IO_L2_in fifo */ tapa::stream<A_t16, 2> fifo_A_A_IO_L2_in_8(
      "fifo_A_A_IO_L2_in_8");
  /* A_IO_L2_in fifo */ tapa::stream<A_t16, 2> fifo_A_A_IO_L2_in_9(
      "fifo_A_A_IO_L2_in_9");
  /* A_IO_L2_in fifo */ tapa::stream<A_t16, 2> fifo_A_A_IO_L2_in_10(
      "fifo_A_A_IO_L2_in_10");
  /* B_IO_L2_in fifo */ tapa::stream<B_t16, 2> fifo_B_B_IO_L2_in_0(
      "fifo_B_B_IO_L2_in_0");
  /* B_IO_L2_in fifo */ tapa::stream<B_t16, 2> fifo_B_B_IO_L2_in_1(
      "fifo_B_B_IO_L2_in_1");
  /* B_IO_L2_in fifo */ tapa::stream<B_t16, 2> fifo_B_B_IO_L2_in_2(
      "fifo_B_B_IO_L2_in_2");
  /* B_IO_L2_in fifo */ tapa::stream<B_t16, 2> fifo_B_B_IO_L2_in_3(
      "fifo_B_B_IO_L2_in_3");
  /* B_IO_L2_in fifo */ tapa::stream<B_t16, 2> fifo_B_B_IO_L2_in_4(
      "fifo_B_B_IO_L2_in_4");
  /* B_IO_L2_in fifo */ tapa::stream<B_t16, 2> fifo_B_B_IO_L2_in_5(
      "fifo_B_B_IO_L2_in_5");
  /* B_IO_L2_in fifo */ tapa::stream<B_t16, 2> fifo_B_B_IO_L2_in_6(
      "fifo_B_B_IO_L2_in_6");
  /* B_IO_L2_in fifo */ tapa::stream<B_t16, 2> fifo_B_B_IO_L2_in_7(
      "fifo_B_B_IO_L2_in_7");
  /* B_IO_L2_in fifo */ tapa::stream<B_t16, 2> fifo_B_B_IO_L2_in_8(
      "fifo_B_B_IO_L2_in_8");
  /* B_IO_L2_in fifo */ tapa::stream<B_t16, 2> fifo_B_B_IO_L2_in_9(
      "fifo_B_B_IO_L2_in_9");
  /* B_IO_L2_in fifo */ tapa::stream<B_t16, 2> fifo_B_B_IO_L2_in_10(
      "fifo_B_B_IO_L2_in_10");
  /* B_IO_L2_in fifo */ tapa::stream<B_t16, 2> fifo_B_B_IO_L2_in_11(
      "fifo_B_B_IO_L2_in_11");
  /* B_IO_L2_in fifo */ tapa::stream<B_t16, 2> fifo_B_B_IO_L2_in_12(
      "fifo_B_B_IO_L2_in_12");
  /* B_IO_L2_in fifo */ tapa::stream<B_t16, 2> fifo_B_B_IO_L2_in_13(
      "fifo_B_B_IO_L2_in_13");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_0_0("fifo_A_PE_0_0");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_0_1("fifo_A_PE_0_1");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_0_2("fifo_A_PE_0_2");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_0_3("fifo_A_PE_0_3");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_0_4("fifo_A_PE_0_4");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_0_5("fifo_A_PE_0_5");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_0_6("fifo_A_PE_0_6");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_0_7("fifo_A_PE_0_7");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_0_8("fifo_A_PE_0_8");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_0_9("fifo_A_PE_0_9");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_0_10("fifo_A_PE_0_10");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_0_11("fifo_A_PE_0_11");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_0_12("fifo_A_PE_0_12");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_0_13("fifo_A_PE_0_13");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_1_0("fifo_A_PE_1_0");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_1_1("fifo_A_PE_1_1");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_1_2("fifo_A_PE_1_2");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_1_3("fifo_A_PE_1_3");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_1_4("fifo_A_PE_1_4");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_1_5("fifo_A_PE_1_5");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_1_6("fifo_A_PE_1_6");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_1_7("fifo_A_PE_1_7");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_1_8("fifo_A_PE_1_8");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_1_9("fifo_A_PE_1_9");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_1_10("fifo_A_PE_1_10");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_1_11("fifo_A_PE_1_11");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_1_12("fifo_A_PE_1_12");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_1_13("fifo_A_PE_1_13");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_2_0("fifo_A_PE_2_0");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_2_1("fifo_A_PE_2_1");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_2_2("fifo_A_PE_2_2");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_2_3("fifo_A_PE_2_3");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_2_4("fifo_A_PE_2_4");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_2_5("fifo_A_PE_2_5");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_2_6("fifo_A_PE_2_6");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_2_7("fifo_A_PE_2_7");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_2_8("fifo_A_PE_2_8");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_2_9("fifo_A_PE_2_9");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_2_10("fifo_A_PE_2_10");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_2_11("fifo_A_PE_2_11");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_2_12("fifo_A_PE_2_12");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_2_13("fifo_A_PE_2_13");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_3_0("fifo_A_PE_3_0");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_3_1("fifo_A_PE_3_1");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_3_2("fifo_A_PE_3_2");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_3_3("fifo_A_PE_3_3");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_3_4("fifo_A_PE_3_4");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_3_5("fifo_A_PE_3_5");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_3_6("fifo_A_PE_3_6");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_3_7("fifo_A_PE_3_7");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_3_8("fifo_A_PE_3_8");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_3_9("fifo_A_PE_3_9");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_3_10("fifo_A_PE_3_10");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_3_11("fifo_A_PE_3_11");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_3_12("fifo_A_PE_3_12");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_3_13("fifo_A_PE_3_13");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_4_0("fifo_A_PE_4_0");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_4_1("fifo_A_PE_4_1");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_4_2("fifo_A_PE_4_2");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_4_3("fifo_A_PE_4_3");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_4_4("fifo_A_PE_4_4");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_4_5("fifo_A_PE_4_5");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_4_6("fifo_A_PE_4_6");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_4_7("fifo_A_PE_4_7");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_4_8("fifo_A_PE_4_8");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_4_9("fifo_A_PE_4_9");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_4_10("fifo_A_PE_4_10");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_4_11("fifo_A_PE_4_11");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_4_12("fifo_A_PE_4_12");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_4_13("fifo_A_PE_4_13");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_5_0("fifo_A_PE_5_0");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_5_1("fifo_A_PE_5_1");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_5_2("fifo_A_PE_5_2");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_5_3("fifo_A_PE_5_3");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_5_4("fifo_A_PE_5_4");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_5_5("fifo_A_PE_5_5");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_5_6("fifo_A_PE_5_6");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_5_7("fifo_A_PE_5_7");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_5_8("fifo_A_PE_5_8");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_5_9("fifo_A_PE_5_9");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_5_10("fifo_A_PE_5_10");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_5_11("fifo_A_PE_5_11");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_5_12("fifo_A_PE_5_12");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_5_13("fifo_A_PE_5_13");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_6_0("fifo_A_PE_6_0");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_6_1("fifo_A_PE_6_1");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_6_2("fifo_A_PE_6_2");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_6_3("fifo_A_PE_6_3");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_6_4("fifo_A_PE_6_4");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_6_5("fifo_A_PE_6_5");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_6_6("fifo_A_PE_6_6");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_6_7("fifo_A_PE_6_7");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_6_8("fifo_A_PE_6_8");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_6_9("fifo_A_PE_6_9");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_6_10("fifo_A_PE_6_10");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_6_11("fifo_A_PE_6_11");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_6_12("fifo_A_PE_6_12");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_6_13("fifo_A_PE_6_13");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_7_0("fifo_A_PE_7_0");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_7_1("fifo_A_PE_7_1");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_7_2("fifo_A_PE_7_2");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_7_3("fifo_A_PE_7_3");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_7_4("fifo_A_PE_7_4");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_7_5("fifo_A_PE_7_5");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_7_6("fifo_A_PE_7_6");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_7_7("fifo_A_PE_7_7");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_7_8("fifo_A_PE_7_8");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_7_9("fifo_A_PE_7_9");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_7_10("fifo_A_PE_7_10");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_7_11("fifo_A_PE_7_11");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_7_12("fifo_A_PE_7_12");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_7_13("fifo_A_PE_7_13");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_8_0("fifo_A_PE_8_0");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_8_1("fifo_A_PE_8_1");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_8_2("fifo_A_PE_8_2");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_8_3("fifo_A_PE_8_3");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_8_4("fifo_A_PE_8_4");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_8_5("fifo_A_PE_8_5");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_8_6("fifo_A_PE_8_6");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_8_7("fifo_A_PE_8_7");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_8_8("fifo_A_PE_8_8");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_8_9("fifo_A_PE_8_9");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_8_10("fifo_A_PE_8_10");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_8_11("fifo_A_PE_8_11");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_8_12("fifo_A_PE_8_12");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_8_13("fifo_A_PE_8_13");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_9_0("fifo_A_PE_9_0");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_9_1("fifo_A_PE_9_1");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_9_2("fifo_A_PE_9_2");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_9_3("fifo_A_PE_9_3");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_9_4("fifo_A_PE_9_4");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_9_5("fifo_A_PE_9_5");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_9_6("fifo_A_PE_9_6");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_9_7("fifo_A_PE_9_7");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_9_8("fifo_A_PE_9_8");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_9_9("fifo_A_PE_9_9");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_9_10("fifo_A_PE_9_10");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_9_11("fifo_A_PE_9_11");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_9_12("fifo_A_PE_9_12");
  /* PE fifo */ tapa::stream<A_t16, 2> fifo_A_PE_9_13("fifo_A_PE_9_13");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_0_0("fifo_B_PE_0_0");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_1_0("fifo_B_PE_1_0");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_2_0("fifo_B_PE_2_0");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_3_0("fifo_B_PE_3_0");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_4_0("fifo_B_PE_4_0");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_5_0("fifo_B_PE_5_0");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_6_0("fifo_B_PE_6_0");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_7_0("fifo_B_PE_7_0");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_8_0("fifo_B_PE_8_0");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_9_0("fifo_B_PE_9_0");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_10_0("fifo_B_PE_10_0");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_0_1("fifo_B_PE_0_1");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_1_1("fifo_B_PE_1_1");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_2_1("fifo_B_PE_2_1");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_3_1("fifo_B_PE_3_1");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_4_1("fifo_B_PE_4_1");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_5_1("fifo_B_PE_5_1");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_6_1("fifo_B_PE_6_1");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_7_1("fifo_B_PE_7_1");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_8_1("fifo_B_PE_8_1");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_9_1("fifo_B_PE_9_1");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_10_1("fifo_B_PE_10_1");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_0_2("fifo_B_PE_0_2");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_1_2("fifo_B_PE_1_2");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_2_2("fifo_B_PE_2_2");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_3_2("fifo_B_PE_3_2");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_4_2("fifo_B_PE_4_2");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_5_2("fifo_B_PE_5_2");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_6_2("fifo_B_PE_6_2");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_7_2("fifo_B_PE_7_2");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_8_2("fifo_B_PE_8_2");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_9_2("fifo_B_PE_9_2");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_10_2("fifo_B_PE_10_2");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_0_3("fifo_B_PE_0_3");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_1_3("fifo_B_PE_1_3");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_2_3("fifo_B_PE_2_3");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_3_3("fifo_B_PE_3_3");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_4_3("fifo_B_PE_4_3");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_5_3("fifo_B_PE_5_3");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_6_3("fifo_B_PE_6_3");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_7_3("fifo_B_PE_7_3");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_8_3("fifo_B_PE_8_3");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_9_3("fifo_B_PE_9_3");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_10_3("fifo_B_PE_10_3");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_0_4("fifo_B_PE_0_4");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_1_4("fifo_B_PE_1_4");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_2_4("fifo_B_PE_2_4");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_3_4("fifo_B_PE_3_4");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_4_4("fifo_B_PE_4_4");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_5_4("fifo_B_PE_5_4");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_6_4("fifo_B_PE_6_4");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_7_4("fifo_B_PE_7_4");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_8_4("fifo_B_PE_8_4");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_9_4("fifo_B_PE_9_4");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_10_4("fifo_B_PE_10_4");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_0_5("fifo_B_PE_0_5");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_1_5("fifo_B_PE_1_5");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_2_5("fifo_B_PE_2_5");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_3_5("fifo_B_PE_3_5");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_4_5("fifo_B_PE_4_5");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_5_5("fifo_B_PE_5_5");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_6_5("fifo_B_PE_6_5");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_7_5("fifo_B_PE_7_5");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_8_5("fifo_B_PE_8_5");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_9_5("fifo_B_PE_9_5");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_10_5("fifo_B_PE_10_5");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_0_6("fifo_B_PE_0_6");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_1_6("fifo_B_PE_1_6");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_2_6("fifo_B_PE_2_6");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_3_6("fifo_B_PE_3_6");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_4_6("fifo_B_PE_4_6");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_5_6("fifo_B_PE_5_6");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_6_6("fifo_B_PE_6_6");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_7_6("fifo_B_PE_7_6");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_8_6("fifo_B_PE_8_6");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_9_6("fifo_B_PE_9_6");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_10_6("fifo_B_PE_10_6");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_0_7("fifo_B_PE_0_7");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_1_7("fifo_B_PE_1_7");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_2_7("fifo_B_PE_2_7");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_3_7("fifo_B_PE_3_7");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_4_7("fifo_B_PE_4_7");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_5_7("fifo_B_PE_5_7");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_6_7("fifo_B_PE_6_7");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_7_7("fifo_B_PE_7_7");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_8_7("fifo_B_PE_8_7");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_9_7("fifo_B_PE_9_7");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_10_7("fifo_B_PE_10_7");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_0_8("fifo_B_PE_0_8");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_1_8("fifo_B_PE_1_8");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_2_8("fifo_B_PE_2_8");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_3_8("fifo_B_PE_3_8");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_4_8("fifo_B_PE_4_8");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_5_8("fifo_B_PE_5_8");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_6_8("fifo_B_PE_6_8");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_7_8("fifo_B_PE_7_8");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_8_8("fifo_B_PE_8_8");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_9_8("fifo_B_PE_9_8");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_10_8("fifo_B_PE_10_8");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_0_9("fifo_B_PE_0_9");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_1_9("fifo_B_PE_1_9");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_2_9("fifo_B_PE_2_9");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_3_9("fifo_B_PE_3_9");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_4_9("fifo_B_PE_4_9");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_5_9("fifo_B_PE_5_9");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_6_9("fifo_B_PE_6_9");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_7_9("fifo_B_PE_7_9");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_8_9("fifo_B_PE_8_9");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_9_9("fifo_B_PE_9_9");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_10_9("fifo_B_PE_10_9");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_0_10("fifo_B_PE_0_10");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_1_10("fifo_B_PE_1_10");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_2_10("fifo_B_PE_2_10");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_3_10("fifo_B_PE_3_10");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_4_10("fifo_B_PE_4_10");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_5_10("fifo_B_PE_5_10");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_6_10("fifo_B_PE_6_10");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_7_10("fifo_B_PE_7_10");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_8_10("fifo_B_PE_8_10");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_9_10("fifo_B_PE_9_10");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_10_10("fifo_B_PE_10_10");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_0_11("fifo_B_PE_0_11");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_1_11("fifo_B_PE_1_11");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_2_11("fifo_B_PE_2_11");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_3_11("fifo_B_PE_3_11");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_4_11("fifo_B_PE_4_11");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_5_11("fifo_B_PE_5_11");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_6_11("fifo_B_PE_6_11");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_7_11("fifo_B_PE_7_11");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_8_11("fifo_B_PE_8_11");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_9_11("fifo_B_PE_9_11");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_10_11("fifo_B_PE_10_11");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_0_12("fifo_B_PE_0_12");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_1_12("fifo_B_PE_1_12");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_2_12("fifo_B_PE_2_12");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_3_12("fifo_B_PE_3_12");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_4_12("fifo_B_PE_4_12");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_5_12("fifo_B_PE_5_12");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_6_12("fifo_B_PE_6_12");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_7_12("fifo_B_PE_7_12");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_8_12("fifo_B_PE_8_12");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_9_12("fifo_B_PE_9_12");
  /* PE fifo */ tapa::stream<B_t16, 2> fifo_B_PE_10_12("fifo_B_PE_10_12");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_0_0(
      "fifo_C_drain_PE_0_0");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_1_0(
      "fifo_C_drain_PE_1_0");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_2_0(
      "fifo_C_drain_PE_2_0");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_3_0(
      "fifo_C_drain_PE_3_0");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_4_0(
      "fifo_C_drain_PE_4_0");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_5_0(
      "fifo_C_drain_PE_5_0");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_6_0(
      "fifo_C_drain_PE_6_0");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_7_0(
      "fifo_C_drain_PE_7_0");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_8_0(
      "fifo_C_drain_PE_8_0");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_9_0(
      "fifo_C_drain_PE_9_0");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_0_1(
      "fifo_C_drain_PE_0_1");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_1_1(
      "fifo_C_drain_PE_1_1");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_2_1(
      "fifo_C_drain_PE_2_1");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_3_1(
      "fifo_C_drain_PE_3_1");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_4_1(
      "fifo_C_drain_PE_4_1");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_5_1(
      "fifo_C_drain_PE_5_1");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_6_1(
      "fifo_C_drain_PE_6_1");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_7_1(
      "fifo_C_drain_PE_7_1");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_8_1(
      "fifo_C_drain_PE_8_1");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_9_1(
      "fifo_C_drain_PE_9_1");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_0_2(
      "fifo_C_drain_PE_0_2");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_1_2(
      "fifo_C_drain_PE_1_2");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_2_2(
      "fifo_C_drain_PE_2_2");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_3_2(
      "fifo_C_drain_PE_3_2");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_4_2(
      "fifo_C_drain_PE_4_2");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_5_2(
      "fifo_C_drain_PE_5_2");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_6_2(
      "fifo_C_drain_PE_6_2");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_7_2(
      "fifo_C_drain_PE_7_2");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_8_2(
      "fifo_C_drain_PE_8_2");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_9_2(
      "fifo_C_drain_PE_9_2");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_0_3(
      "fifo_C_drain_PE_0_3");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_1_3(
      "fifo_C_drain_PE_1_3");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_2_3(
      "fifo_C_drain_PE_2_3");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_3_3(
      "fifo_C_drain_PE_3_3");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_4_3(
      "fifo_C_drain_PE_4_3");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_5_3(
      "fifo_C_drain_PE_5_3");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_6_3(
      "fifo_C_drain_PE_6_3");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_7_3(
      "fifo_C_drain_PE_7_3");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_8_3(
      "fifo_C_drain_PE_8_3");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_9_3(
      "fifo_C_drain_PE_9_3");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_0_4(
      "fifo_C_drain_PE_0_4");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_1_4(
      "fifo_C_drain_PE_1_4");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_2_4(
      "fifo_C_drain_PE_2_4");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_3_4(
      "fifo_C_drain_PE_3_4");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_4_4(
      "fifo_C_drain_PE_4_4");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_5_4(
      "fifo_C_drain_PE_5_4");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_6_4(
      "fifo_C_drain_PE_6_4");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_7_4(
      "fifo_C_drain_PE_7_4");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_8_4(
      "fifo_C_drain_PE_8_4");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_9_4(
      "fifo_C_drain_PE_9_4");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_0_5(
      "fifo_C_drain_PE_0_5");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_1_5(
      "fifo_C_drain_PE_1_5");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_2_5(
      "fifo_C_drain_PE_2_5");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_3_5(
      "fifo_C_drain_PE_3_5");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_4_5(
      "fifo_C_drain_PE_4_5");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_5_5(
      "fifo_C_drain_PE_5_5");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_6_5(
      "fifo_C_drain_PE_6_5");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_7_5(
      "fifo_C_drain_PE_7_5");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_8_5(
      "fifo_C_drain_PE_8_5");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_9_5(
      "fifo_C_drain_PE_9_5");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_0_6(
      "fifo_C_drain_PE_0_6");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_1_6(
      "fifo_C_drain_PE_1_6");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_2_6(
      "fifo_C_drain_PE_2_6");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_3_6(
      "fifo_C_drain_PE_3_6");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_4_6(
      "fifo_C_drain_PE_4_6");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_5_6(
      "fifo_C_drain_PE_5_6");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_6_6(
      "fifo_C_drain_PE_6_6");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_7_6(
      "fifo_C_drain_PE_7_6");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_8_6(
      "fifo_C_drain_PE_8_6");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_9_6(
      "fifo_C_drain_PE_9_6");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_0_7(
      "fifo_C_drain_PE_0_7");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_1_7(
      "fifo_C_drain_PE_1_7");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_2_7(
      "fifo_C_drain_PE_2_7");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_3_7(
      "fifo_C_drain_PE_3_7");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_4_7(
      "fifo_C_drain_PE_4_7");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_5_7(
      "fifo_C_drain_PE_5_7");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_6_7(
      "fifo_C_drain_PE_6_7");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_7_7(
      "fifo_C_drain_PE_7_7");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_8_7(
      "fifo_C_drain_PE_8_7");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_9_7(
      "fifo_C_drain_PE_9_7");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_0_8(
      "fifo_C_drain_PE_0_8");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_1_8(
      "fifo_C_drain_PE_1_8");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_2_8(
      "fifo_C_drain_PE_2_8");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_3_8(
      "fifo_C_drain_PE_3_8");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_4_8(
      "fifo_C_drain_PE_4_8");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_5_8(
      "fifo_C_drain_PE_5_8");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_6_8(
      "fifo_C_drain_PE_6_8");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_7_8(
      "fifo_C_drain_PE_7_8");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_8_8(
      "fifo_C_drain_PE_8_8");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_9_8(
      "fifo_C_drain_PE_9_8");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_0_9(
      "fifo_C_drain_PE_0_9");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_1_9(
      "fifo_C_drain_PE_1_9");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_2_9(
      "fifo_C_drain_PE_2_9");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_3_9(
      "fifo_C_drain_PE_3_9");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_4_9(
      "fifo_C_drain_PE_4_9");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_5_9(
      "fifo_C_drain_PE_5_9");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_6_9(
      "fifo_C_drain_PE_6_9");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_7_9(
      "fifo_C_drain_PE_7_9");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_8_9(
      "fifo_C_drain_PE_8_9");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_9_9(
      "fifo_C_drain_PE_9_9");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_0_10(
      "fifo_C_drain_PE_0_10");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_1_10(
      "fifo_C_drain_PE_1_10");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_2_10(
      "fifo_C_drain_PE_2_10");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_3_10(
      "fifo_C_drain_PE_3_10");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_4_10(
      "fifo_C_drain_PE_4_10");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_5_10(
      "fifo_C_drain_PE_5_10");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_6_10(
      "fifo_C_drain_PE_6_10");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_7_10(
      "fifo_C_drain_PE_7_10");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_8_10(
      "fifo_C_drain_PE_8_10");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_9_10(
      "fifo_C_drain_PE_9_10");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_0_11(
      "fifo_C_drain_PE_0_11");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_1_11(
      "fifo_C_drain_PE_1_11");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_2_11(
      "fifo_C_drain_PE_2_11");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_3_11(
      "fifo_C_drain_PE_3_11");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_4_11(
      "fifo_C_drain_PE_4_11");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_5_11(
      "fifo_C_drain_PE_5_11");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_6_11(
      "fifo_C_drain_PE_6_11");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_7_11(
      "fifo_C_drain_PE_7_11");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_8_11(
      "fifo_C_drain_PE_8_11");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_9_11(
      "fifo_C_drain_PE_9_11");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_0_12(
      "fifo_C_drain_PE_0_12");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_1_12(
      "fifo_C_drain_PE_1_12");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_2_12(
      "fifo_C_drain_PE_2_12");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_3_12(
      "fifo_C_drain_PE_3_12");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_4_12(
      "fifo_C_drain_PE_4_12");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_5_12(
      "fifo_C_drain_PE_5_12");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_6_12(
      "fifo_C_drain_PE_6_12");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_7_12(
      "fifo_C_drain_PE_7_12");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_8_12(
      "fifo_C_drain_PE_8_12");
  /* PE fifo */ tapa::stream<float, 2> fifo_C_drain_PE_9_12(
      "fifo_C_drain_PE_9_12");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_0_0("fifo_C_drain_C_drain_IO_L1_out_0_0");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_0_1("fifo_C_drain_C_drain_IO_L1_out_0_1");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_0_2("fifo_C_drain_C_drain_IO_L1_out_0_2");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_0_3("fifo_C_drain_C_drain_IO_L1_out_0_3");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_0_4("fifo_C_drain_C_drain_IO_L1_out_0_4");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_0_5("fifo_C_drain_C_drain_IO_L1_out_0_5");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_0_6("fifo_C_drain_C_drain_IO_L1_out_0_6");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_0_7("fifo_C_drain_C_drain_IO_L1_out_0_7");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_0_8("fifo_C_drain_C_drain_IO_L1_out_0_8");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_0_9("fifo_C_drain_C_drain_IO_L1_out_0_9");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_0_10(
          "fifo_C_drain_C_drain_IO_L1_out_0_10");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_1_0("fifo_C_drain_C_drain_IO_L1_out_1_0");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_1_1("fifo_C_drain_C_drain_IO_L1_out_1_1");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_1_2("fifo_C_drain_C_drain_IO_L1_out_1_2");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_1_3("fifo_C_drain_C_drain_IO_L1_out_1_3");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_1_4("fifo_C_drain_C_drain_IO_L1_out_1_4");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_1_5("fifo_C_drain_C_drain_IO_L1_out_1_5");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_1_6("fifo_C_drain_C_drain_IO_L1_out_1_6");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_1_7("fifo_C_drain_C_drain_IO_L1_out_1_7");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_1_8("fifo_C_drain_C_drain_IO_L1_out_1_8");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_1_9("fifo_C_drain_C_drain_IO_L1_out_1_9");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_1_10(
          "fifo_C_drain_C_drain_IO_L1_out_1_10");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_2_0("fifo_C_drain_C_drain_IO_L1_out_2_0");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_2_1("fifo_C_drain_C_drain_IO_L1_out_2_1");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_2_2("fifo_C_drain_C_drain_IO_L1_out_2_2");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_2_3("fifo_C_drain_C_drain_IO_L1_out_2_3");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_2_4("fifo_C_drain_C_drain_IO_L1_out_2_4");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_2_5("fifo_C_drain_C_drain_IO_L1_out_2_5");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_2_6("fifo_C_drain_C_drain_IO_L1_out_2_6");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_2_7("fifo_C_drain_C_drain_IO_L1_out_2_7");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_2_8("fifo_C_drain_C_drain_IO_L1_out_2_8");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_2_9("fifo_C_drain_C_drain_IO_L1_out_2_9");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_2_10(
          "fifo_C_drain_C_drain_IO_L1_out_2_10");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_3_0("fifo_C_drain_C_drain_IO_L1_out_3_0");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_3_1("fifo_C_drain_C_drain_IO_L1_out_3_1");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_3_2("fifo_C_drain_C_drain_IO_L1_out_3_2");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_3_3("fifo_C_drain_C_drain_IO_L1_out_3_3");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_3_4("fifo_C_drain_C_drain_IO_L1_out_3_4");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_3_5("fifo_C_drain_C_drain_IO_L1_out_3_5");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_3_6("fifo_C_drain_C_drain_IO_L1_out_3_6");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_3_7("fifo_C_drain_C_drain_IO_L1_out_3_7");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_3_8("fifo_C_drain_C_drain_IO_L1_out_3_8");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_3_9("fifo_C_drain_C_drain_IO_L1_out_3_9");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_3_10(
          "fifo_C_drain_C_drain_IO_L1_out_3_10");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_4_0("fifo_C_drain_C_drain_IO_L1_out_4_0");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_4_1("fifo_C_drain_C_drain_IO_L1_out_4_1");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_4_2("fifo_C_drain_C_drain_IO_L1_out_4_2");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_4_3("fifo_C_drain_C_drain_IO_L1_out_4_3");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_4_4("fifo_C_drain_C_drain_IO_L1_out_4_4");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_4_5("fifo_C_drain_C_drain_IO_L1_out_4_5");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_4_6("fifo_C_drain_C_drain_IO_L1_out_4_6");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_4_7("fifo_C_drain_C_drain_IO_L1_out_4_7");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_4_8("fifo_C_drain_C_drain_IO_L1_out_4_8");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_4_9("fifo_C_drain_C_drain_IO_L1_out_4_9");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_4_10(
          "fifo_C_drain_C_drain_IO_L1_out_4_10");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_5_0("fifo_C_drain_C_drain_IO_L1_out_5_0");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_5_1("fifo_C_drain_C_drain_IO_L1_out_5_1");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_5_2("fifo_C_drain_C_drain_IO_L1_out_5_2");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_5_3("fifo_C_drain_C_drain_IO_L1_out_5_3");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_5_4("fifo_C_drain_C_drain_IO_L1_out_5_4");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_5_5("fifo_C_drain_C_drain_IO_L1_out_5_5");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_5_6("fifo_C_drain_C_drain_IO_L1_out_5_6");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_5_7("fifo_C_drain_C_drain_IO_L1_out_5_7");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_5_8("fifo_C_drain_C_drain_IO_L1_out_5_8");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_5_9("fifo_C_drain_C_drain_IO_L1_out_5_9");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_5_10(
          "fifo_C_drain_C_drain_IO_L1_out_5_10");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_6_0("fifo_C_drain_C_drain_IO_L1_out_6_0");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_6_1("fifo_C_drain_C_drain_IO_L1_out_6_1");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_6_2("fifo_C_drain_C_drain_IO_L1_out_6_2");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_6_3("fifo_C_drain_C_drain_IO_L1_out_6_3");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_6_4("fifo_C_drain_C_drain_IO_L1_out_6_4");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_6_5("fifo_C_drain_C_drain_IO_L1_out_6_5");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_6_6("fifo_C_drain_C_drain_IO_L1_out_6_6");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_6_7("fifo_C_drain_C_drain_IO_L1_out_6_7");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_6_8("fifo_C_drain_C_drain_IO_L1_out_6_8");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_6_9("fifo_C_drain_C_drain_IO_L1_out_6_9");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_6_10(
          "fifo_C_drain_C_drain_IO_L1_out_6_10");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_7_0("fifo_C_drain_C_drain_IO_L1_out_7_0");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_7_1("fifo_C_drain_C_drain_IO_L1_out_7_1");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_7_2("fifo_C_drain_C_drain_IO_L1_out_7_2");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_7_3("fifo_C_drain_C_drain_IO_L1_out_7_3");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_7_4("fifo_C_drain_C_drain_IO_L1_out_7_4");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_7_5("fifo_C_drain_C_drain_IO_L1_out_7_5");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_7_6("fifo_C_drain_C_drain_IO_L1_out_7_6");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_7_7("fifo_C_drain_C_drain_IO_L1_out_7_7");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_7_8("fifo_C_drain_C_drain_IO_L1_out_7_8");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_7_9("fifo_C_drain_C_drain_IO_L1_out_7_9");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_7_10(
          "fifo_C_drain_C_drain_IO_L1_out_7_10");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_8_0("fifo_C_drain_C_drain_IO_L1_out_8_0");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_8_1("fifo_C_drain_C_drain_IO_L1_out_8_1");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_8_2("fifo_C_drain_C_drain_IO_L1_out_8_2");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_8_3("fifo_C_drain_C_drain_IO_L1_out_8_3");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_8_4("fifo_C_drain_C_drain_IO_L1_out_8_4");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_8_5("fifo_C_drain_C_drain_IO_L1_out_8_5");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_8_6("fifo_C_drain_C_drain_IO_L1_out_8_6");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_8_7("fifo_C_drain_C_drain_IO_L1_out_8_7");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_8_8("fifo_C_drain_C_drain_IO_L1_out_8_8");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_8_9("fifo_C_drain_C_drain_IO_L1_out_8_9");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_8_10(
          "fifo_C_drain_C_drain_IO_L1_out_8_10");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_9_0("fifo_C_drain_C_drain_IO_L1_out_9_0");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_9_1("fifo_C_drain_C_drain_IO_L1_out_9_1");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_9_2("fifo_C_drain_C_drain_IO_L1_out_9_2");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_9_3("fifo_C_drain_C_drain_IO_L1_out_9_3");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_9_4("fifo_C_drain_C_drain_IO_L1_out_9_4");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_9_5("fifo_C_drain_C_drain_IO_L1_out_9_5");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_9_6("fifo_C_drain_C_drain_IO_L1_out_9_6");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_9_7("fifo_C_drain_C_drain_IO_L1_out_9_7");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_9_8("fifo_C_drain_C_drain_IO_L1_out_9_8");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_9_9("fifo_C_drain_C_drain_IO_L1_out_9_9");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_9_10(
          "fifo_C_drain_C_drain_IO_L1_out_9_10");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_10_0(
          "fifo_C_drain_C_drain_IO_L1_out_10_0");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_10_1(
          "fifo_C_drain_C_drain_IO_L1_out_10_1");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_10_2(
          "fifo_C_drain_C_drain_IO_L1_out_10_2");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_10_3(
          "fifo_C_drain_C_drain_IO_L1_out_10_3");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_10_4(
          "fifo_C_drain_C_drain_IO_L1_out_10_4");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_10_5(
          "fifo_C_drain_C_drain_IO_L1_out_10_5");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_10_6(
          "fifo_C_drain_C_drain_IO_L1_out_10_6");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_10_7(
          "fifo_C_drain_C_drain_IO_L1_out_10_7");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_10_8(
          "fifo_C_drain_C_drain_IO_L1_out_10_8");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_10_9(
          "fifo_C_drain_C_drain_IO_L1_out_10_9");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_10_10(
          "fifo_C_drain_C_drain_IO_L1_out_10_10");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_11_0(
          "fifo_C_drain_C_drain_IO_L1_out_11_0");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_11_1(
          "fifo_C_drain_C_drain_IO_L1_out_11_1");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_11_2(
          "fifo_C_drain_C_drain_IO_L1_out_11_2");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_11_3(
          "fifo_C_drain_C_drain_IO_L1_out_11_3");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_11_4(
          "fifo_C_drain_C_drain_IO_L1_out_11_4");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_11_5(
          "fifo_C_drain_C_drain_IO_L1_out_11_5");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_11_6(
          "fifo_C_drain_C_drain_IO_L1_out_11_6");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_11_7(
          "fifo_C_drain_C_drain_IO_L1_out_11_7");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_11_8(
          "fifo_C_drain_C_drain_IO_L1_out_11_8");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_11_9(
          "fifo_C_drain_C_drain_IO_L1_out_11_9");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_11_10(
          "fifo_C_drain_C_drain_IO_L1_out_11_10");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_12_0(
          "fifo_C_drain_C_drain_IO_L1_out_12_0");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_12_1(
          "fifo_C_drain_C_drain_IO_L1_out_12_1");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_12_2(
          "fifo_C_drain_C_drain_IO_L1_out_12_2");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_12_3(
          "fifo_C_drain_C_drain_IO_L1_out_12_3");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_12_4(
          "fifo_C_drain_C_drain_IO_L1_out_12_4");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_12_5(
          "fifo_C_drain_C_drain_IO_L1_out_12_5");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_12_6(
          "fifo_C_drain_C_drain_IO_L1_out_12_6");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_12_7(
          "fifo_C_drain_C_drain_IO_L1_out_12_7");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_12_8(
          "fifo_C_drain_C_drain_IO_L1_out_12_8");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_12_9(
          "fifo_C_drain_C_drain_IO_L1_out_12_9");
  /* C_drain_IO_L1_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L1_out_12_10(
          "fifo_C_drain_C_drain_IO_L1_out_12_10");
  /* C_drain_IO_L2_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L2_out_0("fifo_C_drain_C_drain_IO_L2_out_0");
  /* C_drain_IO_L2_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L2_out_1("fifo_C_drain_C_drain_IO_L2_out_1");
  /* C_drain_IO_L2_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L2_out_2("fifo_C_drain_C_drain_IO_L2_out_2");
  /* C_drain_IO_L2_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L2_out_3("fifo_C_drain_C_drain_IO_L2_out_3");
  /* C_drain_IO_L2_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L2_out_4("fifo_C_drain_C_drain_IO_L2_out_4");
  /* C_drain_IO_L2_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L2_out_5("fifo_C_drain_C_drain_IO_L2_out_5");
  /* C_drain_IO_L2_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L2_out_6("fifo_C_drain_C_drain_IO_L2_out_6");
  /* C_drain_IO_L2_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L2_out_7("fifo_C_drain_C_drain_IO_L2_out_7");
  /* C_drain_IO_L2_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L2_out_8("fifo_C_drain_C_drain_IO_L2_out_8");
  /* C_drain_IO_L2_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L2_out_9("fifo_C_drain_C_drain_IO_L2_out_9");
  /* C_drain_IO_L2_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L2_out_10("fifo_C_drain_C_drain_IO_L2_out_10");
  /* C_drain_IO_L2_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L2_out_11("fifo_C_drain_C_drain_IO_L2_out_11");
  /* C_drain_IO_L2_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L2_out_12("fifo_C_drain_C_drain_IO_L2_out_12");
  /* C_drain_IO_L2_out fifo */ tapa::stream<C_t4, 2>
      fifo_C_drain_C_drain_IO_L2_out_13("fifo_C_drain_C_drain_IO_L2_out_13");
  /* FIFO Declaration */

  tapa::task()
      /* Module Call */
      .invoke(A_IO_L3_in_serialize,
              /* array */ A,
              /* fifo */ fifo_A_A_IO_L3_in_serialize)
      /* Module Call */

      /* Module Call */
      .invoke(A_IO_L3_in,
              /* fifo */ fifo_A_A_IO_L3_in_serialize,
              /* fifo */ fifo_A_A_IO_L2_in_0)
      /* Module Call */

      /* Module Call */
      .invoke(A_IO_L2_in,
              /* module id */ 0,
              /* fifo */ fifo_A_A_IO_L2_in_0,
              /* fifo */ fifo_A_A_IO_L2_in_1,
              /* fifo */ fifo_A_PE_0_0)
      /* Module Call */

      /* Module Call */
      .invoke(A_IO_L2_in,
              /* module id */ 1,
              /* fifo */ fifo_A_A_IO_L2_in_1,
              /* fifo */ fifo_A_A_IO_L2_in_2,
              /* fifo */ fifo_A_PE_1_0)
      /* Module Call */

      /* Module Call */
      .invoke(A_IO_L2_in,
              /* module id */ 2,
              /* fifo */ fifo_A_A_IO_L2_in_2,
              /* fifo */ fifo_A_A_IO_L2_in_3,
              /* fifo */ fifo_A_PE_2_0)
      /* Module Call */

      /* Module Call */
      .invoke(A_IO_L2_in,
              /* module id */ 3,
              /* fifo */ fifo_A_A_IO_L2_in_3,
              /* fifo */ fifo_A_A_IO_L2_in_4,
              /* fifo */ fifo_A_PE_3_0)
      /* Module Call */

      /* Module Call */
      .invoke(A_IO_L2_in,
              /* module id */ 4,
              /* fifo */ fifo_A_A_IO_L2_in_4,
              /* fifo */ fifo_A_A_IO_L2_in_5,
              /* fifo */ fifo_A_PE_4_0)
      /* Module Call */

      /* Module Call */
      .invoke(A_IO_L2_in,
              /* module id */ 5,
              /* fifo */ fifo_A_A_IO_L2_in_5,
              /* fifo */ fifo_A_A_IO_L2_in_6,
              /* fifo */ fifo_A_PE_5_0)
      /* Module Call */

      /* Module Call */
      .invoke(A_IO_L2_in,
              /* module id */ 6,
              /* fifo */ fifo_A_A_IO_L2_in_6,
              /* fifo */ fifo_A_A_IO_L2_in_7,
              /* fifo */ fifo_A_PE_6_0)
      /* Module Call */

      /* Module Call */
      .invoke(A_IO_L2_in,
              /* module id */ 7,
              /* fifo */ fifo_A_A_IO_L2_in_7,
              /* fifo */ fifo_A_A_IO_L2_in_8,
              /* fifo */ fifo_A_PE_7_0)
      /* Module Call */

      /* Module Call */
      .invoke(A_IO_L2_in,
              /* module id */ 8,
              /* fifo */ fifo_A_A_IO_L2_in_8,
              /* fifo */ fifo_A_A_IO_L2_in_9,
              /* fifo */ fifo_A_PE_8_0)
      /* Module Call */

      /* Module Call */
      .invoke(A_IO_L2_in_boundary,
              /* module id */ 9,
              /* fifo */ fifo_A_A_IO_L2_in_9,
              /* fifo */ fifo_A_PE_9_0)
      /* Module Call */

      /* Module Call */
      .invoke(B_IO_L3_in_serialize,
              /* array */ B,
              /* fifo */ fifo_B_B_IO_L3_in_serialize)
      /* Module Call */

      /* Module Call */
      .invoke(B_IO_L3_in,
              /* fifo */ fifo_B_B_IO_L3_in_serialize,
              /* fifo */ fifo_B_B_IO_L2_in_0)
      /* Module Call */

      /* Module Call */
      .invoke(B_IO_L2_in,
              /* module id */ 0,
              /* fifo */ fifo_B_B_IO_L2_in_0,
              /* fifo */ fifo_B_B_IO_L2_in_1,
              /* fifo */ fifo_B_PE_0_0)
      /* Module Call */

      /* Module Call */
      .invoke(B_IO_L2_in,
              /* module id */ 1,
              /* fifo */ fifo_B_B_IO_L2_in_1,
              /* fifo */ fifo_B_B_IO_L2_in_2,
              /* fifo */ fifo_B_PE_0_1)
      /* Module Call */

      /* Module Call */
      .invoke(B_IO_L2_in,
              /* module id */ 2,
              /* fifo */ fifo_B_B_IO_L2_in_2,
              /* fifo */ fifo_B_B_IO_L2_in_3,
              /* fifo */ fifo_B_PE_0_2)
      /* Module Call */

      /* Module Call */
      .invoke(B_IO_L2_in,
              /* module id */ 3,
              /* fifo */ fifo_B_B_IO_L2_in_3,
              /* fifo */ fifo_B_B_IO_L2_in_4,
              /* fifo */ fifo_B_PE_0_3)
      /* Module Call */

      /* Module Call */
      .invoke(B_IO_L2_in,
              /* module id */ 4,
              /* fifo */ fifo_B_B_IO_L2_in_4,
              /* fifo */ fifo_B_B_IO_L2_in_5,
              /* fifo */ fifo_B_PE_0_4)
      /* Module Call */

      /* Module Call */
      .invoke(B_IO_L2_in,
              /* module id */ 5,
              /* fifo */ fifo_B_B_IO_L2_in_5,
              /* fifo */ fifo_B_B_IO_L2_in_6,
              /* fifo */ fifo_B_PE_0_5)
      /* Module Call */

      /* Module Call */
      .invoke(B_IO_L2_in,
              /* module id */ 6,
              /* fifo */ fifo_B_B_IO_L2_in_6,
              /* fifo */ fifo_B_B_IO_L2_in_7,
              /* fifo */ fifo_B_PE_0_6)
      /* Module Call */

      /* Module Call */
      .invoke(B_IO_L2_in,
              /* module id */ 7,
              /* fifo */ fifo_B_B_IO_L2_in_7,
              /* fifo */ fifo_B_B_IO_L2_in_8,
              /* fifo */ fifo_B_PE_0_7)
      /* Module Call */

      /* Module Call */
      .invoke(B_IO_L2_in,
              /* module id */ 8,
              /* fifo */ fifo_B_B_IO_L2_in_8,
              /* fifo */ fifo_B_B_IO_L2_in_9,
              /* fifo */ fifo_B_PE_0_8)
      /* Module Call */

      /* Module Call */
      .invoke(B_IO_L2_in,
              /* module id */ 9,
              /* fifo */ fifo_B_B_IO_L2_in_9,
              /* fifo */ fifo_B_B_IO_L2_in_10,
              /* fifo */ fifo_B_PE_0_9)
      /* Module Call */

      /* Module Call */
      .invoke(B_IO_L2_in,
              /* module id */ 10,
              /* fifo */ fifo_B_B_IO_L2_in_10,
              /* fifo */ fifo_B_B_IO_L2_in_11,
              /* fifo */ fifo_B_PE_0_10)
      /* Module Call */

      /* Module Call */
      .invoke(B_IO_L2_in,
              /* module id */ 11,
              /* fifo */ fifo_B_B_IO_L2_in_11,
              /* fifo */ fifo_B_B_IO_L2_in_12,
              /* fifo */ fifo_B_PE_0_11)
      /* Module Call */

      /* Module Call */
      .invoke(B_IO_L2_in_boundary,
              /* module id */ 12,
              /* fifo */ fifo_B_B_IO_L2_in_12,
              /* fifo */ fifo_B_PE_0_12)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 0,
              /* module id */ 0,
              /* fifo */ fifo_A_PE_0_0,
              /* fifo */ fifo_A_PE_0_1,
              /* fifo */ fifo_B_PE_0_0,
              /* fifo */ fifo_B_PE_1_0,
              /* fifo */ fifo_C_drain_PE_0_0)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 0,
              /* module id */ 1,
              /* fifo */ fifo_A_PE_0_1,
              /* fifo */ fifo_A_PE_0_2,
              /* fifo */ fifo_B_PE_0_1,
              /* fifo */ fifo_B_PE_1_1,
              /* fifo */ fifo_C_drain_PE_0_1)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 0,
              /* module id */ 2,
              /* fifo */ fifo_A_PE_0_2,
              /* fifo */ fifo_A_PE_0_3,
              /* fifo */ fifo_B_PE_0_2,
              /* fifo */ fifo_B_PE_1_2,
              /* fifo */ fifo_C_drain_PE_0_2)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 0,
              /* module id */ 3,
              /* fifo */ fifo_A_PE_0_3,
              /* fifo */ fifo_A_PE_0_4,
              /* fifo */ fifo_B_PE_0_3,
              /* fifo */ fifo_B_PE_1_3,
              /* fifo */ fifo_C_drain_PE_0_3)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 0,
              /* module id */ 4,
              /* fifo */ fifo_A_PE_0_4,
              /* fifo */ fifo_A_PE_0_5,
              /* fifo */ fifo_B_PE_0_4,
              /* fifo */ fifo_B_PE_1_4,
              /* fifo */ fifo_C_drain_PE_0_4)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 0,
              /* module id */ 5,
              /* fifo */ fifo_A_PE_0_5,
              /* fifo */ fifo_A_PE_0_6,
              /* fifo */ fifo_B_PE_0_5,
              /* fifo */ fifo_B_PE_1_5,
              /* fifo */ fifo_C_drain_PE_0_5)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 0,
              /* module id */ 6,
              /* fifo */ fifo_A_PE_0_6,
              /* fifo */ fifo_A_PE_0_7,
              /* fifo */ fifo_B_PE_0_6,
              /* fifo */ fifo_B_PE_1_6,
              /* fifo */ fifo_C_drain_PE_0_6)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 0,
              /* module id */ 7,
              /* fifo */ fifo_A_PE_0_7,
              /* fifo */ fifo_A_PE_0_8,
              /* fifo */ fifo_B_PE_0_7,
              /* fifo */ fifo_B_PE_1_7,
              /* fifo */ fifo_C_drain_PE_0_7)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 0,
              /* module id */ 8,
              /* fifo */ fifo_A_PE_0_8,
              /* fifo */ fifo_A_PE_0_9,
              /* fifo */ fifo_B_PE_0_8,
              /* fifo */ fifo_B_PE_1_8,
              /* fifo */ fifo_C_drain_PE_0_8)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 0,
              /* module id */ 9,
              /* fifo */ fifo_A_PE_0_9,
              /* fifo */ fifo_A_PE_0_10,
              /* fifo */ fifo_B_PE_0_9,
              /* fifo */ fifo_B_PE_1_9,
              /* fifo */ fifo_C_drain_PE_0_9)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 0,
              /* module id */ 10,
              /* fifo */ fifo_A_PE_0_10,
              /* fifo */ fifo_A_PE_0_11,
              /* fifo */ fifo_B_PE_0_10,
              /* fifo */ fifo_B_PE_1_10,
              /* fifo */ fifo_C_drain_PE_0_10)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 0,
              /* module id */ 11,
              /* fifo */ fifo_A_PE_0_11,
              /* fifo */ fifo_A_PE_0_12,
              /* fifo */ fifo_B_PE_0_11,
              /* fifo */ fifo_B_PE_1_11,
              /* fifo */ fifo_C_drain_PE_0_11)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 0,
              /* module id */ 12,
              /* fifo */ fifo_A_PE_0_12,
              /* fifo */ fifo_A_PE_0_13,
              /* fifo */ fifo_B_PE_0_12,
              /* fifo */ fifo_B_PE_1_12,
              /* fifo */ fifo_C_drain_PE_0_12)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 1,
              /* module id */ 0,
              /* fifo */ fifo_A_PE_1_0,
              /* fifo */ fifo_A_PE_1_1,
              /* fifo */ fifo_B_PE_1_0,
              /* fifo */ fifo_B_PE_2_0,
              /* fifo */ fifo_C_drain_PE_1_0)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 1,
              /* module id */ 1,
              /* fifo */ fifo_A_PE_1_1,
              /* fifo */ fifo_A_PE_1_2,
              /* fifo */ fifo_B_PE_1_1,
              /* fifo */ fifo_B_PE_2_1,
              /* fifo */ fifo_C_drain_PE_1_1)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 1,
              /* module id */ 2,
              /* fifo */ fifo_A_PE_1_2,
              /* fifo */ fifo_A_PE_1_3,
              /* fifo */ fifo_B_PE_1_2,
              /* fifo */ fifo_B_PE_2_2,
              /* fifo */ fifo_C_drain_PE_1_2)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 1,
              /* module id */ 3,
              /* fifo */ fifo_A_PE_1_3,
              /* fifo */ fifo_A_PE_1_4,
              /* fifo */ fifo_B_PE_1_3,
              /* fifo */ fifo_B_PE_2_3,
              /* fifo */ fifo_C_drain_PE_1_3)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 1,
              /* module id */ 4,
              /* fifo */ fifo_A_PE_1_4,
              /* fifo */ fifo_A_PE_1_5,
              /* fifo */ fifo_B_PE_1_4,
              /* fifo */ fifo_B_PE_2_4,
              /* fifo */ fifo_C_drain_PE_1_4)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 1,
              /* module id */ 5,
              /* fifo */ fifo_A_PE_1_5,
              /* fifo */ fifo_A_PE_1_6,
              /* fifo */ fifo_B_PE_1_5,
              /* fifo */ fifo_B_PE_2_5,
              /* fifo */ fifo_C_drain_PE_1_5)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 1,
              /* module id */ 6,
              /* fifo */ fifo_A_PE_1_6,
              /* fifo */ fifo_A_PE_1_7,
              /* fifo */ fifo_B_PE_1_6,
              /* fifo */ fifo_B_PE_2_6,
              /* fifo */ fifo_C_drain_PE_1_6)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 1,
              /* module id */ 7,
              /* fifo */ fifo_A_PE_1_7,
              /* fifo */ fifo_A_PE_1_8,
              /* fifo */ fifo_B_PE_1_7,
              /* fifo */ fifo_B_PE_2_7,
              /* fifo */ fifo_C_drain_PE_1_7)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 1,
              /* module id */ 8,
              /* fifo */ fifo_A_PE_1_8,
              /* fifo */ fifo_A_PE_1_9,
              /* fifo */ fifo_B_PE_1_8,
              /* fifo */ fifo_B_PE_2_8,
              /* fifo */ fifo_C_drain_PE_1_8)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 1,
              /* module id */ 9,
              /* fifo */ fifo_A_PE_1_9,
              /* fifo */ fifo_A_PE_1_10,
              /* fifo */ fifo_B_PE_1_9,
              /* fifo */ fifo_B_PE_2_9,
              /* fifo */ fifo_C_drain_PE_1_9)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 1,
              /* module id */ 10,
              /* fifo */ fifo_A_PE_1_10,
              /* fifo */ fifo_A_PE_1_11,
              /* fifo */ fifo_B_PE_1_10,
              /* fifo */ fifo_B_PE_2_10,
              /* fifo */ fifo_C_drain_PE_1_10)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 1,
              /* module id */ 11,
              /* fifo */ fifo_A_PE_1_11,
              /* fifo */ fifo_A_PE_1_12,
              /* fifo */ fifo_B_PE_1_11,
              /* fifo */ fifo_B_PE_2_11,
              /* fifo */ fifo_C_drain_PE_1_11)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 1,
              /* module id */ 12,
              /* fifo */ fifo_A_PE_1_12,
              /* fifo */ fifo_A_PE_1_13,
              /* fifo */ fifo_B_PE_1_12,
              /* fifo */ fifo_B_PE_2_12,
              /* fifo */ fifo_C_drain_PE_1_12)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 2,
              /* module id */ 0,
              /* fifo */ fifo_A_PE_2_0,
              /* fifo */ fifo_A_PE_2_1,
              /* fifo */ fifo_B_PE_2_0,
              /* fifo */ fifo_B_PE_3_0,
              /* fifo */ fifo_C_drain_PE_2_0)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 2,
              /* module id */ 1,
              /* fifo */ fifo_A_PE_2_1,
              /* fifo */ fifo_A_PE_2_2,
              /* fifo */ fifo_B_PE_2_1,
              /* fifo */ fifo_B_PE_3_1,
              /* fifo */ fifo_C_drain_PE_2_1)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 2,
              /* module id */ 2,
              /* fifo */ fifo_A_PE_2_2,
              /* fifo */ fifo_A_PE_2_3,
              /* fifo */ fifo_B_PE_2_2,
              /* fifo */ fifo_B_PE_3_2,
              /* fifo */ fifo_C_drain_PE_2_2)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 2,
              /* module id */ 3,
              /* fifo */ fifo_A_PE_2_3,
              /* fifo */ fifo_A_PE_2_4,
              /* fifo */ fifo_B_PE_2_3,
              /* fifo */ fifo_B_PE_3_3,
              /* fifo */ fifo_C_drain_PE_2_3)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 2,
              /* module id */ 4,
              /* fifo */ fifo_A_PE_2_4,
              /* fifo */ fifo_A_PE_2_5,
              /* fifo */ fifo_B_PE_2_4,
              /* fifo */ fifo_B_PE_3_4,
              /* fifo */ fifo_C_drain_PE_2_4)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 2,
              /* module id */ 5,
              /* fifo */ fifo_A_PE_2_5,
              /* fifo */ fifo_A_PE_2_6,
              /* fifo */ fifo_B_PE_2_5,
              /* fifo */ fifo_B_PE_3_5,
              /* fifo */ fifo_C_drain_PE_2_5)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 2,
              /* module id */ 6,
              /* fifo */ fifo_A_PE_2_6,
              /* fifo */ fifo_A_PE_2_7,
              /* fifo */ fifo_B_PE_2_6,
              /* fifo */ fifo_B_PE_3_6,
              /* fifo */ fifo_C_drain_PE_2_6)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 2,
              /* module id */ 7,
              /* fifo */ fifo_A_PE_2_7,
              /* fifo */ fifo_A_PE_2_8,
              /* fifo */ fifo_B_PE_2_7,
              /* fifo */ fifo_B_PE_3_7,
              /* fifo */ fifo_C_drain_PE_2_7)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 2,
              /* module id */ 8,
              /* fifo */ fifo_A_PE_2_8,
              /* fifo */ fifo_A_PE_2_9,
              /* fifo */ fifo_B_PE_2_8,
              /* fifo */ fifo_B_PE_3_8,
              /* fifo */ fifo_C_drain_PE_2_8)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 2,
              /* module id */ 9,
              /* fifo */ fifo_A_PE_2_9,
              /* fifo */ fifo_A_PE_2_10,
              /* fifo */ fifo_B_PE_2_9,
              /* fifo */ fifo_B_PE_3_9,
              /* fifo */ fifo_C_drain_PE_2_9)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 2,
              /* module id */ 10,
              /* fifo */ fifo_A_PE_2_10,
              /* fifo */ fifo_A_PE_2_11,
              /* fifo */ fifo_B_PE_2_10,
              /* fifo */ fifo_B_PE_3_10,
              /* fifo */ fifo_C_drain_PE_2_10)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 2,
              /* module id */ 11,
              /* fifo */ fifo_A_PE_2_11,
              /* fifo */ fifo_A_PE_2_12,
              /* fifo */ fifo_B_PE_2_11,
              /* fifo */ fifo_B_PE_3_11,
              /* fifo */ fifo_C_drain_PE_2_11)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 2,
              /* module id */ 12,
              /* fifo */ fifo_A_PE_2_12,
              /* fifo */ fifo_A_PE_2_13,
              /* fifo */ fifo_B_PE_2_12,
              /* fifo */ fifo_B_PE_3_12,
              /* fifo */ fifo_C_drain_PE_2_12)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 3,
              /* module id */ 0,
              /* fifo */ fifo_A_PE_3_0,
              /* fifo */ fifo_A_PE_3_1,
              /* fifo */ fifo_B_PE_3_0,
              /* fifo */ fifo_B_PE_4_0,
              /* fifo */ fifo_C_drain_PE_3_0)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 3,
              /* module id */ 1,
              /* fifo */ fifo_A_PE_3_1,
              /* fifo */ fifo_A_PE_3_2,
              /* fifo */ fifo_B_PE_3_1,
              /* fifo */ fifo_B_PE_4_1,
              /* fifo */ fifo_C_drain_PE_3_1)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 3,
              /* module id */ 2,
              /* fifo */ fifo_A_PE_3_2,
              /* fifo */ fifo_A_PE_3_3,
              /* fifo */ fifo_B_PE_3_2,
              /* fifo */ fifo_B_PE_4_2,
              /* fifo */ fifo_C_drain_PE_3_2)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 3,
              /* module id */ 3,
              /* fifo */ fifo_A_PE_3_3,
              /* fifo */ fifo_A_PE_3_4,
              /* fifo */ fifo_B_PE_3_3,
              /* fifo */ fifo_B_PE_4_3,
              /* fifo */ fifo_C_drain_PE_3_3)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 3,
              /* module id */ 4,
              /* fifo */ fifo_A_PE_3_4,
              /* fifo */ fifo_A_PE_3_5,
              /* fifo */ fifo_B_PE_3_4,
              /* fifo */ fifo_B_PE_4_4,
              /* fifo */ fifo_C_drain_PE_3_4)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 3,
              /* module id */ 5,
              /* fifo */ fifo_A_PE_3_5,
              /* fifo */ fifo_A_PE_3_6,
              /* fifo */ fifo_B_PE_3_5,
              /* fifo */ fifo_B_PE_4_5,
              /* fifo */ fifo_C_drain_PE_3_5)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 3,
              /* module id */ 6,
              /* fifo */ fifo_A_PE_3_6,
              /* fifo */ fifo_A_PE_3_7,
              /* fifo */ fifo_B_PE_3_6,
              /* fifo */ fifo_B_PE_4_6,
              /* fifo */ fifo_C_drain_PE_3_6)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 3,
              /* module id */ 7,
              /* fifo */ fifo_A_PE_3_7,
              /* fifo */ fifo_A_PE_3_8,
              /* fifo */ fifo_B_PE_3_7,
              /* fifo */ fifo_B_PE_4_7,
              /* fifo */ fifo_C_drain_PE_3_7)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 3,
              /* module id */ 8,
              /* fifo */ fifo_A_PE_3_8,
              /* fifo */ fifo_A_PE_3_9,
              /* fifo */ fifo_B_PE_3_8,
              /* fifo */ fifo_B_PE_4_8,
              /* fifo */ fifo_C_drain_PE_3_8)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 3,
              /* module id */ 9,
              /* fifo */ fifo_A_PE_3_9,
              /* fifo */ fifo_A_PE_3_10,
              /* fifo */ fifo_B_PE_3_9,
              /* fifo */ fifo_B_PE_4_9,
              /* fifo */ fifo_C_drain_PE_3_9)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 3,
              /* module id */ 10,
              /* fifo */ fifo_A_PE_3_10,
              /* fifo */ fifo_A_PE_3_11,
              /* fifo */ fifo_B_PE_3_10,
              /* fifo */ fifo_B_PE_4_10,
              /* fifo */ fifo_C_drain_PE_3_10)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 3,
              /* module id */ 11,
              /* fifo */ fifo_A_PE_3_11,
              /* fifo */ fifo_A_PE_3_12,
              /* fifo */ fifo_B_PE_3_11,
              /* fifo */ fifo_B_PE_4_11,
              /* fifo */ fifo_C_drain_PE_3_11)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 3,
              /* module id */ 12,
              /* fifo */ fifo_A_PE_3_12,
              /* fifo */ fifo_A_PE_3_13,
              /* fifo */ fifo_B_PE_3_12,
              /* fifo */ fifo_B_PE_4_12,
              /* fifo */ fifo_C_drain_PE_3_12)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 4,
              /* module id */ 0,
              /* fifo */ fifo_A_PE_4_0,
              /* fifo */ fifo_A_PE_4_1,
              /* fifo */ fifo_B_PE_4_0,
              /* fifo */ fifo_B_PE_5_0,
              /* fifo */ fifo_C_drain_PE_4_0)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 4,
              /* module id */ 1,
              /* fifo */ fifo_A_PE_4_1,
              /* fifo */ fifo_A_PE_4_2,
              /* fifo */ fifo_B_PE_4_1,
              /* fifo */ fifo_B_PE_5_1,
              /* fifo */ fifo_C_drain_PE_4_1)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 4,
              /* module id */ 2,
              /* fifo */ fifo_A_PE_4_2,
              /* fifo */ fifo_A_PE_4_3,
              /* fifo */ fifo_B_PE_4_2,
              /* fifo */ fifo_B_PE_5_2,
              /* fifo */ fifo_C_drain_PE_4_2)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 4,
              /* module id */ 3,
              /* fifo */ fifo_A_PE_4_3,
              /* fifo */ fifo_A_PE_4_4,
              /* fifo */ fifo_B_PE_4_3,
              /* fifo */ fifo_B_PE_5_3,
              /* fifo */ fifo_C_drain_PE_4_3)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 4,
              /* module id */ 4,
              /* fifo */ fifo_A_PE_4_4,
              /* fifo */ fifo_A_PE_4_5,
              /* fifo */ fifo_B_PE_4_4,
              /* fifo */ fifo_B_PE_5_4,
              /* fifo */ fifo_C_drain_PE_4_4)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 4,
              /* module id */ 5,
              /* fifo */ fifo_A_PE_4_5,
              /* fifo */ fifo_A_PE_4_6,
              /* fifo */ fifo_B_PE_4_5,
              /* fifo */ fifo_B_PE_5_5,
              /* fifo */ fifo_C_drain_PE_4_5)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 4,
              /* module id */ 6,
              /* fifo */ fifo_A_PE_4_6,
              /* fifo */ fifo_A_PE_4_7,
              /* fifo */ fifo_B_PE_4_6,
              /* fifo */ fifo_B_PE_5_6,
              /* fifo */ fifo_C_drain_PE_4_6)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 4,
              /* module id */ 7,
              /* fifo */ fifo_A_PE_4_7,
              /* fifo */ fifo_A_PE_4_8,
              /* fifo */ fifo_B_PE_4_7,
              /* fifo */ fifo_B_PE_5_7,
              /* fifo */ fifo_C_drain_PE_4_7)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 4,
              /* module id */ 8,
              /* fifo */ fifo_A_PE_4_8,
              /* fifo */ fifo_A_PE_4_9,
              /* fifo */ fifo_B_PE_4_8,
              /* fifo */ fifo_B_PE_5_8,
              /* fifo */ fifo_C_drain_PE_4_8)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 4,
              /* module id */ 9,
              /* fifo */ fifo_A_PE_4_9,
              /* fifo */ fifo_A_PE_4_10,
              /* fifo */ fifo_B_PE_4_9,
              /* fifo */ fifo_B_PE_5_9,
              /* fifo */ fifo_C_drain_PE_4_9)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 4,
              /* module id */ 10,
              /* fifo */ fifo_A_PE_4_10,
              /* fifo */ fifo_A_PE_4_11,
              /* fifo */ fifo_B_PE_4_10,
              /* fifo */ fifo_B_PE_5_10,
              /* fifo */ fifo_C_drain_PE_4_10)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 4,
              /* module id */ 11,
              /* fifo */ fifo_A_PE_4_11,
              /* fifo */ fifo_A_PE_4_12,
              /* fifo */ fifo_B_PE_4_11,
              /* fifo */ fifo_B_PE_5_11,
              /* fifo */ fifo_C_drain_PE_4_11)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 4,
              /* module id */ 12,
              /* fifo */ fifo_A_PE_4_12,
              /* fifo */ fifo_A_PE_4_13,
              /* fifo */ fifo_B_PE_4_12,
              /* fifo */ fifo_B_PE_5_12,
              /* fifo */ fifo_C_drain_PE_4_12)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 5,
              /* module id */ 0,
              /* fifo */ fifo_A_PE_5_0,
              /* fifo */ fifo_A_PE_5_1,
              /* fifo */ fifo_B_PE_5_0,
              /* fifo */ fifo_B_PE_6_0,
              /* fifo */ fifo_C_drain_PE_5_0)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 5,
              /* module id */ 1,
              /* fifo */ fifo_A_PE_5_1,
              /* fifo */ fifo_A_PE_5_2,
              /* fifo */ fifo_B_PE_5_1,
              /* fifo */ fifo_B_PE_6_1,
              /* fifo */ fifo_C_drain_PE_5_1)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 5,
              /* module id */ 2,
              /* fifo */ fifo_A_PE_5_2,
              /* fifo */ fifo_A_PE_5_3,
              /* fifo */ fifo_B_PE_5_2,
              /* fifo */ fifo_B_PE_6_2,
              /* fifo */ fifo_C_drain_PE_5_2)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 5,
              /* module id */ 3,
              /* fifo */ fifo_A_PE_5_3,
              /* fifo */ fifo_A_PE_5_4,
              /* fifo */ fifo_B_PE_5_3,
              /* fifo */ fifo_B_PE_6_3,
              /* fifo */ fifo_C_drain_PE_5_3)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 5,
              /* module id */ 4,
              /* fifo */ fifo_A_PE_5_4,
              /* fifo */ fifo_A_PE_5_5,
              /* fifo */ fifo_B_PE_5_4,
              /* fifo */ fifo_B_PE_6_4,
              /* fifo */ fifo_C_drain_PE_5_4)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 5,
              /* module id */ 5,
              /* fifo */ fifo_A_PE_5_5,
              /* fifo */ fifo_A_PE_5_6,
              /* fifo */ fifo_B_PE_5_5,
              /* fifo */ fifo_B_PE_6_5,
              /* fifo */ fifo_C_drain_PE_5_5)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 5,
              /* module id */ 6,
              /* fifo */ fifo_A_PE_5_6,
              /* fifo */ fifo_A_PE_5_7,
              /* fifo */ fifo_B_PE_5_6,
              /* fifo */ fifo_B_PE_6_6,
              /* fifo */ fifo_C_drain_PE_5_6)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 5,
              /* module id */ 7,
              /* fifo */ fifo_A_PE_5_7,
              /* fifo */ fifo_A_PE_5_8,
              /* fifo */ fifo_B_PE_5_7,
              /* fifo */ fifo_B_PE_6_7,
              /* fifo */ fifo_C_drain_PE_5_7)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 5,
              /* module id */ 8,
              /* fifo */ fifo_A_PE_5_8,
              /* fifo */ fifo_A_PE_5_9,
              /* fifo */ fifo_B_PE_5_8,
              /* fifo */ fifo_B_PE_6_8,
              /* fifo */ fifo_C_drain_PE_5_8)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 5,
              /* module id */ 9,
              /* fifo */ fifo_A_PE_5_9,
              /* fifo */ fifo_A_PE_5_10,
              /* fifo */ fifo_B_PE_5_9,
              /* fifo */ fifo_B_PE_6_9,
              /* fifo */ fifo_C_drain_PE_5_9)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 5,
              /* module id */ 10,
              /* fifo */ fifo_A_PE_5_10,
              /* fifo */ fifo_A_PE_5_11,
              /* fifo */ fifo_B_PE_5_10,
              /* fifo */ fifo_B_PE_6_10,
              /* fifo */ fifo_C_drain_PE_5_10)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 5,
              /* module id */ 11,
              /* fifo */ fifo_A_PE_5_11,
              /* fifo */ fifo_A_PE_5_12,
              /* fifo */ fifo_B_PE_5_11,
              /* fifo */ fifo_B_PE_6_11,
              /* fifo */ fifo_C_drain_PE_5_11)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 5,
              /* module id */ 12,
              /* fifo */ fifo_A_PE_5_12,
              /* fifo */ fifo_A_PE_5_13,
              /* fifo */ fifo_B_PE_5_12,
              /* fifo */ fifo_B_PE_6_12,
              /* fifo */ fifo_C_drain_PE_5_12)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 6,
              /* module id */ 0,
              /* fifo */ fifo_A_PE_6_0,
              /* fifo */ fifo_A_PE_6_1,
              /* fifo */ fifo_B_PE_6_0,
              /* fifo */ fifo_B_PE_7_0,
              /* fifo */ fifo_C_drain_PE_6_0)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 6,
              /* module id */ 1,
              /* fifo */ fifo_A_PE_6_1,
              /* fifo */ fifo_A_PE_6_2,
              /* fifo */ fifo_B_PE_6_1,
              /* fifo */ fifo_B_PE_7_1,
              /* fifo */ fifo_C_drain_PE_6_1)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 6,
              /* module id */ 2,
              /* fifo */ fifo_A_PE_6_2,
              /* fifo */ fifo_A_PE_6_3,
              /* fifo */ fifo_B_PE_6_2,
              /* fifo */ fifo_B_PE_7_2,
              /* fifo */ fifo_C_drain_PE_6_2)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 6,
              /* module id */ 3,
              /* fifo */ fifo_A_PE_6_3,
              /* fifo */ fifo_A_PE_6_4,
              /* fifo */ fifo_B_PE_6_3,
              /* fifo */ fifo_B_PE_7_3,
              /* fifo */ fifo_C_drain_PE_6_3)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 6,
              /* module id */ 4,
              /* fifo */ fifo_A_PE_6_4,
              /* fifo */ fifo_A_PE_6_5,
              /* fifo */ fifo_B_PE_6_4,
              /* fifo */ fifo_B_PE_7_4,
              /* fifo */ fifo_C_drain_PE_6_4)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 6,
              /* module id */ 5,
              /* fifo */ fifo_A_PE_6_5,
              /* fifo */ fifo_A_PE_6_6,
              /* fifo */ fifo_B_PE_6_5,
              /* fifo */ fifo_B_PE_7_5,
              /* fifo */ fifo_C_drain_PE_6_5)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 6,
              /* module id */ 6,
              /* fifo */ fifo_A_PE_6_6,
              /* fifo */ fifo_A_PE_6_7,
              /* fifo */ fifo_B_PE_6_6,
              /* fifo */ fifo_B_PE_7_6,
              /* fifo */ fifo_C_drain_PE_6_6)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 6,
              /* module id */ 7,
              /* fifo */ fifo_A_PE_6_7,
              /* fifo */ fifo_A_PE_6_8,
              /* fifo */ fifo_B_PE_6_7,
              /* fifo */ fifo_B_PE_7_7,
              /* fifo */ fifo_C_drain_PE_6_7)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 6,
              /* module id */ 8,
              /* fifo */ fifo_A_PE_6_8,
              /* fifo */ fifo_A_PE_6_9,
              /* fifo */ fifo_B_PE_6_8,
              /* fifo */ fifo_B_PE_7_8,
              /* fifo */ fifo_C_drain_PE_6_8)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 6,
              /* module id */ 9,
              /* fifo */ fifo_A_PE_6_9,
              /* fifo */ fifo_A_PE_6_10,
              /* fifo */ fifo_B_PE_6_9,
              /* fifo */ fifo_B_PE_7_9,
              /* fifo */ fifo_C_drain_PE_6_9)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 6,
              /* module id */ 10,
              /* fifo */ fifo_A_PE_6_10,
              /* fifo */ fifo_A_PE_6_11,
              /* fifo */ fifo_B_PE_6_10,
              /* fifo */ fifo_B_PE_7_10,
              /* fifo */ fifo_C_drain_PE_6_10)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 6,
              /* module id */ 11,
              /* fifo */ fifo_A_PE_6_11,
              /* fifo */ fifo_A_PE_6_12,
              /* fifo */ fifo_B_PE_6_11,
              /* fifo */ fifo_B_PE_7_11,
              /* fifo */ fifo_C_drain_PE_6_11)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 6,
              /* module id */ 12,
              /* fifo */ fifo_A_PE_6_12,
              /* fifo */ fifo_A_PE_6_13,
              /* fifo */ fifo_B_PE_6_12,
              /* fifo */ fifo_B_PE_7_12,
              /* fifo */ fifo_C_drain_PE_6_12)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 7,
              /* module id */ 0,
              /* fifo */ fifo_A_PE_7_0,
              /* fifo */ fifo_A_PE_7_1,
              /* fifo */ fifo_B_PE_7_0,
              /* fifo */ fifo_B_PE_8_0,
              /* fifo */ fifo_C_drain_PE_7_0)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 7,
              /* module id */ 1,
              /* fifo */ fifo_A_PE_7_1,
              /* fifo */ fifo_A_PE_7_2,
              /* fifo */ fifo_B_PE_7_1,
              /* fifo */ fifo_B_PE_8_1,
              /* fifo */ fifo_C_drain_PE_7_1)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 7,
              /* module id */ 2,
              /* fifo */ fifo_A_PE_7_2,
              /* fifo */ fifo_A_PE_7_3,
              /* fifo */ fifo_B_PE_7_2,
              /* fifo */ fifo_B_PE_8_2,
              /* fifo */ fifo_C_drain_PE_7_2)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 7,
              /* module id */ 3,
              /* fifo */ fifo_A_PE_7_3,
              /* fifo */ fifo_A_PE_7_4,
              /* fifo */ fifo_B_PE_7_3,
              /* fifo */ fifo_B_PE_8_3,
              /* fifo */ fifo_C_drain_PE_7_3)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 7,
              /* module id */ 4,
              /* fifo */ fifo_A_PE_7_4,
              /* fifo */ fifo_A_PE_7_5,
              /* fifo */ fifo_B_PE_7_4,
              /* fifo */ fifo_B_PE_8_4,
              /* fifo */ fifo_C_drain_PE_7_4)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 7,
              /* module id */ 5,
              /* fifo */ fifo_A_PE_7_5,
              /* fifo */ fifo_A_PE_7_6,
              /* fifo */ fifo_B_PE_7_5,
              /* fifo */ fifo_B_PE_8_5,
              /* fifo */ fifo_C_drain_PE_7_5)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 7,
              /* module id */ 6,
              /* fifo */ fifo_A_PE_7_6,
              /* fifo */ fifo_A_PE_7_7,
              /* fifo */ fifo_B_PE_7_6,
              /* fifo */ fifo_B_PE_8_6,
              /* fifo */ fifo_C_drain_PE_7_6)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 7,
              /* module id */ 7,
              /* fifo */ fifo_A_PE_7_7,
              /* fifo */ fifo_A_PE_7_8,
              /* fifo */ fifo_B_PE_7_7,
              /* fifo */ fifo_B_PE_8_7,
              /* fifo */ fifo_C_drain_PE_7_7)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 7,
              /* module id */ 8,
              /* fifo */ fifo_A_PE_7_8,
              /* fifo */ fifo_A_PE_7_9,
              /* fifo */ fifo_B_PE_7_8,
              /* fifo */ fifo_B_PE_8_8,
              /* fifo */ fifo_C_drain_PE_7_8)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 7,
              /* module id */ 9,
              /* fifo */ fifo_A_PE_7_9,
              /* fifo */ fifo_A_PE_7_10,
              /* fifo */ fifo_B_PE_7_9,
              /* fifo */ fifo_B_PE_8_9,
              /* fifo */ fifo_C_drain_PE_7_9)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 7,
              /* module id */ 10,
              /* fifo */ fifo_A_PE_7_10,
              /* fifo */ fifo_A_PE_7_11,
              /* fifo */ fifo_B_PE_7_10,
              /* fifo */ fifo_B_PE_8_10,
              /* fifo */ fifo_C_drain_PE_7_10)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 7,
              /* module id */ 11,
              /* fifo */ fifo_A_PE_7_11,
              /* fifo */ fifo_A_PE_7_12,
              /* fifo */ fifo_B_PE_7_11,
              /* fifo */ fifo_B_PE_8_11,
              /* fifo */ fifo_C_drain_PE_7_11)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 7,
              /* module id */ 12,
              /* fifo */ fifo_A_PE_7_12,
              /* fifo */ fifo_A_PE_7_13,
              /* fifo */ fifo_B_PE_7_12,
              /* fifo */ fifo_B_PE_8_12,
              /* fifo */ fifo_C_drain_PE_7_12)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 8,
              /* module id */ 0,
              /* fifo */ fifo_A_PE_8_0,
              /* fifo */ fifo_A_PE_8_1,
              /* fifo */ fifo_B_PE_8_0,
              /* fifo */ fifo_B_PE_9_0,
              /* fifo */ fifo_C_drain_PE_8_0)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 8,
              /* module id */ 1,
              /* fifo */ fifo_A_PE_8_1,
              /* fifo */ fifo_A_PE_8_2,
              /* fifo */ fifo_B_PE_8_1,
              /* fifo */ fifo_B_PE_9_1,
              /* fifo */ fifo_C_drain_PE_8_1)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 8,
              /* module id */ 2,
              /* fifo */ fifo_A_PE_8_2,
              /* fifo */ fifo_A_PE_8_3,
              /* fifo */ fifo_B_PE_8_2,
              /* fifo */ fifo_B_PE_9_2,
              /* fifo */ fifo_C_drain_PE_8_2)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 8,
              /* module id */ 3,
              /* fifo */ fifo_A_PE_8_3,
              /* fifo */ fifo_A_PE_8_4,
              /* fifo */ fifo_B_PE_8_3,
              /* fifo */ fifo_B_PE_9_3,
              /* fifo */ fifo_C_drain_PE_8_3)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 8,
              /* module id */ 4,
              /* fifo */ fifo_A_PE_8_4,
              /* fifo */ fifo_A_PE_8_5,
              /* fifo */ fifo_B_PE_8_4,
              /* fifo */ fifo_B_PE_9_4,
              /* fifo */ fifo_C_drain_PE_8_4)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 8,
              /* module id */ 5,
              /* fifo */ fifo_A_PE_8_5,
              /* fifo */ fifo_A_PE_8_6,
              /* fifo */ fifo_B_PE_8_5,
              /* fifo */ fifo_B_PE_9_5,
              /* fifo */ fifo_C_drain_PE_8_5)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 8,
              /* module id */ 6,
              /* fifo */ fifo_A_PE_8_6,
              /* fifo */ fifo_A_PE_8_7,
              /* fifo */ fifo_B_PE_8_6,
              /* fifo */ fifo_B_PE_9_6,
              /* fifo */ fifo_C_drain_PE_8_6)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 8,
              /* module id */ 7,
              /* fifo */ fifo_A_PE_8_7,
              /* fifo */ fifo_A_PE_8_8,
              /* fifo */ fifo_B_PE_8_7,
              /* fifo */ fifo_B_PE_9_7,
              /* fifo */ fifo_C_drain_PE_8_7)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 8,
              /* module id */ 8,
              /* fifo */ fifo_A_PE_8_8,
              /* fifo */ fifo_A_PE_8_9,
              /* fifo */ fifo_B_PE_8_8,
              /* fifo */ fifo_B_PE_9_8,
              /* fifo */ fifo_C_drain_PE_8_8)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 8,
              /* module id */ 9,
              /* fifo */ fifo_A_PE_8_9,
              /* fifo */ fifo_A_PE_8_10,
              /* fifo */ fifo_B_PE_8_9,
              /* fifo */ fifo_B_PE_9_9,
              /* fifo */ fifo_C_drain_PE_8_9)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 8,
              /* module id */ 10,
              /* fifo */ fifo_A_PE_8_10,
              /* fifo */ fifo_A_PE_8_11,
              /* fifo */ fifo_B_PE_8_10,
              /* fifo */ fifo_B_PE_9_10,
              /* fifo */ fifo_C_drain_PE_8_10)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 8,
              /* module id */ 11,
              /* fifo */ fifo_A_PE_8_11,
              /* fifo */ fifo_A_PE_8_12,
              /* fifo */ fifo_B_PE_8_11,
              /* fifo */ fifo_B_PE_9_11,
              /* fifo */ fifo_C_drain_PE_8_11)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 8,
              /* module id */ 12,
              /* fifo */ fifo_A_PE_8_12,
              /* fifo */ fifo_A_PE_8_13,
              /* fifo */ fifo_B_PE_8_12,
              /* fifo */ fifo_B_PE_9_12,
              /* fifo */ fifo_C_drain_PE_8_12)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 9,
              /* module id */ 0,
              /* fifo */ fifo_A_PE_9_0,
              /* fifo */ fifo_A_PE_9_1,
              /* fifo */ fifo_B_PE_9_0,
              /* fifo */ fifo_B_PE_10_0,
              /* fifo */ fifo_C_drain_PE_9_0)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 9,
              /* module id */ 1,
              /* fifo */ fifo_A_PE_9_1,
              /* fifo */ fifo_A_PE_9_2,
              /* fifo */ fifo_B_PE_9_1,
              /* fifo */ fifo_B_PE_10_1,
              /* fifo */ fifo_C_drain_PE_9_1)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 9,
              /* module id */ 2,
              /* fifo */ fifo_A_PE_9_2,
              /* fifo */ fifo_A_PE_9_3,
              /* fifo */ fifo_B_PE_9_2,
              /* fifo */ fifo_B_PE_10_2,
              /* fifo */ fifo_C_drain_PE_9_2)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 9,
              /* module id */ 3,
              /* fifo */ fifo_A_PE_9_3,
              /* fifo */ fifo_A_PE_9_4,
              /* fifo */ fifo_B_PE_9_3,
              /* fifo */ fifo_B_PE_10_3,
              /* fifo */ fifo_C_drain_PE_9_3)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 9,
              /* module id */ 4,
              /* fifo */ fifo_A_PE_9_4,
              /* fifo */ fifo_A_PE_9_5,
              /* fifo */ fifo_B_PE_9_4,
              /* fifo */ fifo_B_PE_10_4,
              /* fifo */ fifo_C_drain_PE_9_4)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 9,
              /* module id */ 5,
              /* fifo */ fifo_A_PE_9_5,
              /* fifo */ fifo_A_PE_9_6,
              /* fifo */ fifo_B_PE_9_5,
              /* fifo */ fifo_B_PE_10_5,
              /* fifo */ fifo_C_drain_PE_9_5)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 9,
              /* module id */ 6,
              /* fifo */ fifo_A_PE_9_6,
              /* fifo */ fifo_A_PE_9_7,
              /* fifo */ fifo_B_PE_9_6,
              /* fifo */ fifo_B_PE_10_6,
              /* fifo */ fifo_C_drain_PE_9_6)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 9,
              /* module id */ 7,
              /* fifo */ fifo_A_PE_9_7,
              /* fifo */ fifo_A_PE_9_8,
              /* fifo */ fifo_B_PE_9_7,
              /* fifo */ fifo_B_PE_10_7,
              /* fifo */ fifo_C_drain_PE_9_7)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 9,
              /* module id */ 8,
              /* fifo */ fifo_A_PE_9_8,
              /* fifo */ fifo_A_PE_9_9,
              /* fifo */ fifo_B_PE_9_8,
              /* fifo */ fifo_B_PE_10_8,
              /* fifo */ fifo_C_drain_PE_9_8)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 9,
              /* module id */ 9,
              /* fifo */ fifo_A_PE_9_9,
              /* fifo */ fifo_A_PE_9_10,
              /* fifo */ fifo_B_PE_9_9,
              /* fifo */ fifo_B_PE_10_9,
              /* fifo */ fifo_C_drain_PE_9_9)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 9,
              /* module id */ 10,
              /* fifo */ fifo_A_PE_9_10,
              /* fifo */ fifo_A_PE_9_11,
              /* fifo */ fifo_B_PE_9_10,
              /* fifo */ fifo_B_PE_10_10,
              /* fifo */ fifo_C_drain_PE_9_10)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 9,
              /* module id */ 11,
              /* fifo */ fifo_A_PE_9_11,
              /* fifo */ fifo_A_PE_9_12,
              /* fifo */ fifo_B_PE_9_11,
              /* fifo */ fifo_B_PE_10_11,
              /* fifo */ fifo_C_drain_PE_9_11)
      /* Module Call */

      /* Module Call */
      .invoke(PE_wrapper,
              /* module id */ 9,
              /* module id */ 12,
              /* fifo */ fifo_A_PE_9_12,
              /* fifo */ fifo_A_PE_9_13,
              /* fifo */ fifo_B_PE_9_12,
              /* fifo */ fifo_B_PE_10_12,
              /* fifo */ fifo_C_drain_PE_9_12)
      /* Module Call */

      /* Module Call */
      .invoke(A_PE_dummy_in,
              /* module id */ 0,
              /* module id */ 12,
              /* fifo */ fifo_A_PE_0_13)
      /* Module Call */

      /* Module Call */
      .invoke(A_PE_dummy_in,
              /* module id */ 1,
              /* module id */ 12,
              /* fifo */ fifo_A_PE_1_13)
      /* Module Call */

      /* Module Call */
      .invoke(A_PE_dummy_in,
              /* module id */ 2,
              /* module id */ 12,
              /* fifo */ fifo_A_PE_2_13)
      /* Module Call */

      /* Module Call */
      .invoke(A_PE_dummy_in,
              /* module id */ 3,
              /* module id */ 12,
              /* fifo */ fifo_A_PE_3_13)
      /* Module Call */

      /* Module Call */
      .invoke(A_PE_dummy_in,
              /* module id */ 4,
              /* module id */ 12,
              /* fifo */ fifo_A_PE_4_13)
      /* Module Call */

      /* Module Call */
      .invoke(A_PE_dummy_in,
              /* module id */ 5,
              /* module id */ 12,
              /* fifo */ fifo_A_PE_5_13)
      /* Module Call */

      /* Module Call */
      .invoke(A_PE_dummy_in,
              /* module id */ 6,
              /* module id */ 12,
              /* fifo */ fifo_A_PE_6_13)
      /* Module Call */

      /* Module Call */
      .invoke(A_PE_dummy_in,
              /* module id */ 7,
              /* module id */ 12,
              /* fifo */ fifo_A_PE_7_13)
      /* Module Call */

      /* Module Call */
      .invoke(A_PE_dummy_in,
              /* module id */ 8,
              /* module id */ 12,
              /* fifo */ fifo_A_PE_8_13)
      /* Module Call */

      /* Module Call */
      .invoke(A_PE_dummy_in,
              /* module id */ 9,
              /* module id */ 12,
              /* fifo */ fifo_A_PE_9_13)
      /* Module Call */

      /* Module Call */
      .invoke(B_PE_dummy_in,
              /* module id */ 9,
              /* module id */ 0,
              /* fifo */ fifo_B_PE_10_0)
      /* Module Call */

      /* Module Call */
      .invoke(B_PE_dummy_in,
              /* module id */ 9,
              /* module id */ 1,
              /* fifo */ fifo_B_PE_10_1)
      /* Module Call */

      /* Module Call */
      .invoke(B_PE_dummy_in,
              /* module id */ 9,
              /* module id */ 2,
              /* fifo */ fifo_B_PE_10_2)
      /* Module Call */

      /* Module Call */
      .invoke(B_PE_dummy_in,
              /* module id */ 9,
              /* module id */ 3,
              /* fifo */ fifo_B_PE_10_3)
      /* Module Call */

      /* Module Call */
      .invoke(B_PE_dummy_in,
              /* module id */ 9,
              /* module id */ 4,
              /* fifo */ fifo_B_PE_10_4)
      /* Module Call */

      /* Module Call */
      .invoke(B_PE_dummy_in,
              /* module id */ 9,
              /* module id */ 5,
              /* fifo */ fifo_B_PE_10_5)
      /* Module Call */

      /* Module Call */
      .invoke(B_PE_dummy_in,
              /* module id */ 9,
              /* module id */ 6,
              /* fifo */ fifo_B_PE_10_6)
      /* Module Call */

      /* Module Call */
      .invoke(B_PE_dummy_in,
              /* module id */ 9,
              /* module id */ 7,
              /* fifo */ fifo_B_PE_10_7)
      /* Module Call */

      /* Module Call */
      .invoke(B_PE_dummy_in,
              /* module id */ 9,
              /* module id */ 8,
              /* fifo */ fifo_B_PE_10_8)
      /* Module Call */

      /* Module Call */
      .invoke(B_PE_dummy_in,
              /* module id */ 9,
              /* module id */ 9,
              /* fifo */ fifo_B_PE_10_9)
      /* Module Call */

      /* Module Call */
      .invoke(B_PE_dummy_in,
              /* module id */ 9,
              /* module id */ 10,
              /* fifo */ fifo_B_PE_10_10)
      /* Module Call */

      /* Module Call */
      .invoke(B_PE_dummy_in,
              /* module id */ 9,
              /* module id */ 11,
              /* fifo */ fifo_B_PE_10_11)
      /* Module Call */

      /* Module Call */
      .invoke(B_PE_dummy_in,
              /* module id */ 9,
              /* module id */ 12,
              /* fifo */ fifo_B_PE_10_12)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 0,
              /* module id */ 0,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_0_1,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_0_0,
              /* fifo */ fifo_C_drain_PE_0_0)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 0,
              /* module id */ 1,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_0_2,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_0_1,
              /* fifo */ fifo_C_drain_PE_1_0)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 0,
              /* module id */ 2,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_0_3,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_0_2,
              /* fifo */ fifo_C_drain_PE_2_0)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 0,
              /* module id */ 3,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_0_4,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_0_3,
              /* fifo */ fifo_C_drain_PE_3_0)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 0,
              /* module id */ 4,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_0_5,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_0_4,
              /* fifo */ fifo_C_drain_PE_4_0)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 0,
              /* module id */ 5,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_0_6,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_0_5,
              /* fifo */ fifo_C_drain_PE_5_0)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 0,
              /* module id */ 6,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_0_7,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_0_6,
              /* fifo */ fifo_C_drain_PE_6_0)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 0,
              /* module id */ 7,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_0_8,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_0_7,
              /* fifo */ fifo_C_drain_PE_7_0)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 0,
              /* module id */ 8,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_0_9,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_0_8,
              /* fifo */ fifo_C_drain_PE_8_0)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_boundary_wrapper,
              /* module id */ 0,
              /* module id */ 9,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_0_9,
              /* fifo */ fifo_C_drain_PE_9_0)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 1,
              /* module id */ 0,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_1_1,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_1_0,
              /* fifo */ fifo_C_drain_PE_0_1)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 1,
              /* module id */ 1,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_1_2,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_1_1,
              /* fifo */ fifo_C_drain_PE_1_1)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 1,
              /* module id */ 2,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_1_3,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_1_2,
              /* fifo */ fifo_C_drain_PE_2_1)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 1,
              /* module id */ 3,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_1_4,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_1_3,
              /* fifo */ fifo_C_drain_PE_3_1)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 1,
              /* module id */ 4,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_1_5,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_1_4,
              /* fifo */ fifo_C_drain_PE_4_1)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 1,
              /* module id */ 5,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_1_6,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_1_5,
              /* fifo */ fifo_C_drain_PE_5_1)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 1,
              /* module id */ 6,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_1_7,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_1_6,
              /* fifo */ fifo_C_drain_PE_6_1)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 1,
              /* module id */ 7,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_1_8,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_1_7,
              /* fifo */ fifo_C_drain_PE_7_1)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 1,
              /* module id */ 8,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_1_9,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_1_8,
              /* fifo */ fifo_C_drain_PE_8_1)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_boundary_wrapper,
              /* module id */ 1,
              /* module id */ 9,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_1_9,
              /* fifo */ fifo_C_drain_PE_9_1)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 2,
              /* module id */ 0,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_2_1,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_2_0,
              /* fifo */ fifo_C_drain_PE_0_2)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 2,
              /* module id */ 1,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_2_2,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_2_1,
              /* fifo */ fifo_C_drain_PE_1_2)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 2,
              /* module id */ 2,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_2_3,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_2_2,
              /* fifo */ fifo_C_drain_PE_2_2)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 2,
              /* module id */ 3,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_2_4,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_2_3,
              /* fifo */ fifo_C_drain_PE_3_2)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 2,
              /* module id */ 4,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_2_5,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_2_4,
              /* fifo */ fifo_C_drain_PE_4_2)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 2,
              /* module id */ 5,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_2_6,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_2_5,
              /* fifo */ fifo_C_drain_PE_5_2)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 2,
              /* module id */ 6,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_2_7,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_2_6,
              /* fifo */ fifo_C_drain_PE_6_2)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 2,
              /* module id */ 7,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_2_8,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_2_7,
              /* fifo */ fifo_C_drain_PE_7_2)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 2,
              /* module id */ 8,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_2_9,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_2_8,
              /* fifo */ fifo_C_drain_PE_8_2)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_boundary_wrapper,
              /* module id */ 2,
              /* module id */ 9,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_2_9,
              /* fifo */ fifo_C_drain_PE_9_2)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 3,
              /* module id */ 0,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_3_1,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_3_0,
              /* fifo */ fifo_C_drain_PE_0_3)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 3,
              /* module id */ 1,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_3_2,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_3_1,
              /* fifo */ fifo_C_drain_PE_1_3)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 3,
              /* module id */ 2,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_3_3,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_3_2,
              /* fifo */ fifo_C_drain_PE_2_3)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 3,
              /* module id */ 3,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_3_4,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_3_3,
              /* fifo */ fifo_C_drain_PE_3_3)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 3,
              /* module id */ 4,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_3_5,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_3_4,
              /* fifo */ fifo_C_drain_PE_4_3)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 3,
              /* module id */ 5,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_3_6,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_3_5,
              /* fifo */ fifo_C_drain_PE_5_3)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 3,
              /* module id */ 6,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_3_7,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_3_6,
              /* fifo */ fifo_C_drain_PE_6_3)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 3,
              /* module id */ 7,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_3_8,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_3_7,
              /* fifo */ fifo_C_drain_PE_7_3)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 3,
              /* module id */ 8,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_3_9,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_3_8,
              /* fifo */ fifo_C_drain_PE_8_3)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_boundary_wrapper,
              /* module id */ 3,
              /* module id */ 9,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_3_9,
              /* fifo */ fifo_C_drain_PE_9_3)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 4,
              /* module id */ 0,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_4_1,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_4_0,
              /* fifo */ fifo_C_drain_PE_0_4)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 4,
              /* module id */ 1,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_4_2,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_4_1,
              /* fifo */ fifo_C_drain_PE_1_4)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 4,
              /* module id */ 2,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_4_3,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_4_2,
              /* fifo */ fifo_C_drain_PE_2_4)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 4,
              /* module id */ 3,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_4_4,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_4_3,
              /* fifo */ fifo_C_drain_PE_3_4)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 4,
              /* module id */ 4,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_4_5,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_4_4,
              /* fifo */ fifo_C_drain_PE_4_4)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 4,
              /* module id */ 5,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_4_6,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_4_5,
              /* fifo */ fifo_C_drain_PE_5_4)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 4,
              /* module id */ 6,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_4_7,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_4_6,
              /* fifo */ fifo_C_drain_PE_6_4)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 4,
              /* module id */ 7,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_4_8,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_4_7,
              /* fifo */ fifo_C_drain_PE_7_4)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 4,
              /* module id */ 8,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_4_9,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_4_8,
              /* fifo */ fifo_C_drain_PE_8_4)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_boundary_wrapper,
              /* module id */ 4,
              /* module id */ 9,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_4_9,
              /* fifo */ fifo_C_drain_PE_9_4)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 5,
              /* module id */ 0,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_5_1,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_5_0,
              /* fifo */ fifo_C_drain_PE_0_5)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 5,
              /* module id */ 1,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_5_2,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_5_1,
              /* fifo */ fifo_C_drain_PE_1_5)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 5,
              /* module id */ 2,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_5_3,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_5_2,
              /* fifo */ fifo_C_drain_PE_2_5)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 5,
              /* module id */ 3,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_5_4,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_5_3,
              /* fifo */ fifo_C_drain_PE_3_5)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 5,
              /* module id */ 4,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_5_5,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_5_4,
              /* fifo */ fifo_C_drain_PE_4_5)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 5,
              /* module id */ 5,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_5_6,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_5_5,
              /* fifo */ fifo_C_drain_PE_5_5)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 5,
              /* module id */ 6,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_5_7,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_5_6,
              /* fifo */ fifo_C_drain_PE_6_5)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 5,
              /* module id */ 7,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_5_8,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_5_7,
              /* fifo */ fifo_C_drain_PE_7_5)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 5,
              /* module id */ 8,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_5_9,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_5_8,
              /* fifo */ fifo_C_drain_PE_8_5)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_boundary_wrapper,
              /* module id */ 5,
              /* module id */ 9,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_5_9,
              /* fifo */ fifo_C_drain_PE_9_5)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 6,
              /* module id */ 0,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_6_1,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_6_0,
              /* fifo */ fifo_C_drain_PE_0_6)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 6,
              /* module id */ 1,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_6_2,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_6_1,
              /* fifo */ fifo_C_drain_PE_1_6)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 6,
              /* module id */ 2,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_6_3,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_6_2,
              /* fifo */ fifo_C_drain_PE_2_6)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 6,
              /* module id */ 3,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_6_4,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_6_3,
              /* fifo */ fifo_C_drain_PE_3_6)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 6,
              /* module id */ 4,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_6_5,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_6_4,
              /* fifo */ fifo_C_drain_PE_4_6)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 6,
              /* module id */ 5,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_6_6,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_6_5,
              /* fifo */ fifo_C_drain_PE_5_6)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 6,
              /* module id */ 6,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_6_7,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_6_6,
              /* fifo */ fifo_C_drain_PE_6_6)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 6,
              /* module id */ 7,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_6_8,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_6_7,
              /* fifo */ fifo_C_drain_PE_7_6)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 6,
              /* module id */ 8,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_6_9,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_6_8,
              /* fifo */ fifo_C_drain_PE_8_6)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_boundary_wrapper,
              /* module id */ 6,
              /* module id */ 9,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_6_9,
              /* fifo */ fifo_C_drain_PE_9_6)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 7,
              /* module id */ 0,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_7_1,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_7_0,
              /* fifo */ fifo_C_drain_PE_0_7)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 7,
              /* module id */ 1,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_7_2,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_7_1,
              /* fifo */ fifo_C_drain_PE_1_7)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 7,
              /* module id */ 2,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_7_3,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_7_2,
              /* fifo */ fifo_C_drain_PE_2_7)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 7,
              /* module id */ 3,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_7_4,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_7_3,
              /* fifo */ fifo_C_drain_PE_3_7)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 7,
              /* module id */ 4,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_7_5,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_7_4,
              /* fifo */ fifo_C_drain_PE_4_7)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 7,
              /* module id */ 5,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_7_6,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_7_5,
              /* fifo */ fifo_C_drain_PE_5_7)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 7,
              /* module id */ 6,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_7_7,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_7_6,
              /* fifo */ fifo_C_drain_PE_6_7)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 7,
              /* module id */ 7,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_7_8,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_7_7,
              /* fifo */ fifo_C_drain_PE_7_7)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 7,
              /* module id */ 8,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_7_9,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_7_8,
              /* fifo */ fifo_C_drain_PE_8_7)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_boundary_wrapper,
              /* module id */ 7,
              /* module id */ 9,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_7_9,
              /* fifo */ fifo_C_drain_PE_9_7)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 8,
              /* module id */ 0,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_8_1,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_8_0,
              /* fifo */ fifo_C_drain_PE_0_8)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 8,
              /* module id */ 1,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_8_2,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_8_1,
              /* fifo */ fifo_C_drain_PE_1_8)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 8,
              /* module id */ 2,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_8_3,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_8_2,
              /* fifo */ fifo_C_drain_PE_2_8)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 8,
              /* module id */ 3,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_8_4,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_8_3,
              /* fifo */ fifo_C_drain_PE_3_8)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 8,
              /* module id */ 4,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_8_5,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_8_4,
              /* fifo */ fifo_C_drain_PE_4_8)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 8,
              /* module id */ 5,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_8_6,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_8_5,
              /* fifo */ fifo_C_drain_PE_5_8)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 8,
              /* module id */ 6,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_8_7,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_8_6,
              /* fifo */ fifo_C_drain_PE_6_8)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 8,
              /* module id */ 7,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_8_8,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_8_7,
              /* fifo */ fifo_C_drain_PE_7_8)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 8,
              /* module id */ 8,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_8_9,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_8_8,
              /* fifo */ fifo_C_drain_PE_8_8)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_boundary_wrapper,
              /* module id */ 8,
              /* module id */ 9,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_8_9,
              /* fifo */ fifo_C_drain_PE_9_8)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 9,
              /* module id */ 0,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_9_1,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_9_0,
              /* fifo */ fifo_C_drain_PE_0_9)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 9,
              /* module id */ 1,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_9_2,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_9_1,
              /* fifo */ fifo_C_drain_PE_1_9)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 9,
              /* module id */ 2,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_9_3,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_9_2,
              /* fifo */ fifo_C_drain_PE_2_9)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 9,
              /* module id */ 3,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_9_4,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_9_3,
              /* fifo */ fifo_C_drain_PE_3_9)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 9,
              /* module id */ 4,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_9_5,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_9_4,
              /* fifo */ fifo_C_drain_PE_4_9)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 9,
              /* module id */ 5,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_9_6,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_9_5,
              /* fifo */ fifo_C_drain_PE_5_9)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 9,
              /* module id */ 6,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_9_7,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_9_6,
              /* fifo */ fifo_C_drain_PE_6_9)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 9,
              /* module id */ 7,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_9_8,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_9_7,
              /* fifo */ fifo_C_drain_PE_7_9)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 9,
              /* module id */ 8,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_9_9,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_9_8,
              /* fifo */ fifo_C_drain_PE_8_9)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_boundary_wrapper,
              /* module id */ 9,
              /* module id */ 9,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_9_9,
              /* fifo */ fifo_C_drain_PE_9_9)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 10,
              /* module id */ 0,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_10_1,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_10_0,
              /* fifo */ fifo_C_drain_PE_0_10)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 10,
              /* module id */ 1,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_10_2,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_10_1,
              /* fifo */ fifo_C_drain_PE_1_10)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 10,
              /* module id */ 2,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_10_3,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_10_2,
              /* fifo */ fifo_C_drain_PE_2_10)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 10,
              /* module id */ 3,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_10_4,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_10_3,
              /* fifo */ fifo_C_drain_PE_3_10)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 10,
              /* module id */ 4,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_10_5,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_10_4,
              /* fifo */ fifo_C_drain_PE_4_10)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 10,
              /* module id */ 5,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_10_6,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_10_5,
              /* fifo */ fifo_C_drain_PE_5_10)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 10,
              /* module id */ 6,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_10_7,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_10_6,
              /* fifo */ fifo_C_drain_PE_6_10)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 10,
              /* module id */ 7,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_10_8,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_10_7,
              /* fifo */ fifo_C_drain_PE_7_10)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 10,
              /* module id */ 8,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_10_9,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_10_8,
              /* fifo */ fifo_C_drain_PE_8_10)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_boundary_wrapper,
              /* module id */ 10,
              /* module id */ 9,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_10_9,
              /* fifo */ fifo_C_drain_PE_9_10)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 11,
              /* module id */ 0,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_11_1,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_11_0,
              /* fifo */ fifo_C_drain_PE_0_11)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 11,
              /* module id */ 1,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_11_2,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_11_1,
              /* fifo */ fifo_C_drain_PE_1_11)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 11,
              /* module id */ 2,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_11_3,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_11_2,
              /* fifo */ fifo_C_drain_PE_2_11)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 11,
              /* module id */ 3,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_11_4,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_11_3,
              /* fifo */ fifo_C_drain_PE_3_11)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 11,
              /* module id */ 4,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_11_5,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_11_4,
              /* fifo */ fifo_C_drain_PE_4_11)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 11,
              /* module id */ 5,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_11_6,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_11_5,
              /* fifo */ fifo_C_drain_PE_5_11)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 11,
              /* module id */ 6,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_11_7,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_11_6,
              /* fifo */ fifo_C_drain_PE_6_11)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 11,
              /* module id */ 7,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_11_8,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_11_7,
              /* fifo */ fifo_C_drain_PE_7_11)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 11,
              /* module id */ 8,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_11_9,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_11_8,
              /* fifo */ fifo_C_drain_PE_8_11)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_boundary_wrapper,
              /* module id */ 11,
              /* module id */ 9,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_11_9,
              /* fifo */ fifo_C_drain_PE_9_11)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 12,
              /* module id */ 0,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_12_1,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_12_0,
              /* fifo */ fifo_C_drain_PE_0_12)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 12,
              /* module id */ 1,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_12_2,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_12_1,
              /* fifo */ fifo_C_drain_PE_1_12)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 12,
              /* module id */ 2,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_12_3,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_12_2,
              /* fifo */ fifo_C_drain_PE_2_12)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 12,
              /* module id */ 3,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_12_4,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_12_3,
              /* fifo */ fifo_C_drain_PE_3_12)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 12,
              /* module id */ 4,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_12_5,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_12_4,
              /* fifo */ fifo_C_drain_PE_4_12)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 12,
              /* module id */ 5,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_12_6,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_12_5,
              /* fifo */ fifo_C_drain_PE_5_12)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 12,
              /* module id */ 6,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_12_7,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_12_6,
              /* fifo */ fifo_C_drain_PE_6_12)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 12,
              /* module id */ 7,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_12_8,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_12_7,
              /* fifo */ fifo_C_drain_PE_7_12)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_wrapper,
              /* module id */ 12,
              /* module id */ 8,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_12_9,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_12_8,
              /* fifo */ fifo_C_drain_PE_8_12)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L1_out_boundary_wrapper,
              /* module id */ 12,
              /* module id */ 9,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_12_9,
              /* fifo */ fifo_C_drain_PE_9_12)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L2_out,
              /* module id */ 0,
              /* fifo */ fifo_C_drain_C_drain_IO_L2_out_1,
              /* fifo */ fifo_C_drain_C_drain_IO_L2_out_0,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_0_0)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L2_out,
              /* module id */ 1,
              /* fifo */ fifo_C_drain_C_drain_IO_L2_out_2,
              /* fifo */ fifo_C_drain_C_drain_IO_L2_out_1,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_1_0)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L2_out,
              /* module id */ 2,
              /* fifo */ fifo_C_drain_C_drain_IO_L2_out_3,
              /* fifo */ fifo_C_drain_C_drain_IO_L2_out_2,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_2_0)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L2_out,
              /* module id */ 3,
              /* fifo */ fifo_C_drain_C_drain_IO_L2_out_4,
              /* fifo */ fifo_C_drain_C_drain_IO_L2_out_3,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_3_0)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L2_out,
              /* module id */ 4,
              /* fifo */ fifo_C_drain_C_drain_IO_L2_out_5,
              /* fifo */ fifo_C_drain_C_drain_IO_L2_out_4,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_4_0)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L2_out,
              /* module id */ 5,
              /* fifo */ fifo_C_drain_C_drain_IO_L2_out_6,
              /* fifo */ fifo_C_drain_C_drain_IO_L2_out_5,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_5_0)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L2_out,
              /* module id */ 6,
              /* fifo */ fifo_C_drain_C_drain_IO_L2_out_7,
              /* fifo */ fifo_C_drain_C_drain_IO_L2_out_6,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_6_0)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L2_out,
              /* module id */ 7,
              /* fifo */ fifo_C_drain_C_drain_IO_L2_out_8,
              /* fifo */ fifo_C_drain_C_drain_IO_L2_out_7,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_7_0)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L2_out,
              /* module id */ 8,
              /* fifo */ fifo_C_drain_C_drain_IO_L2_out_9,
              /* fifo */ fifo_C_drain_C_drain_IO_L2_out_8,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_8_0)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L2_out,
              /* module id */ 9,
              /* fifo */ fifo_C_drain_C_drain_IO_L2_out_10,
              /* fifo */ fifo_C_drain_C_drain_IO_L2_out_9,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_9_0)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L2_out,
              /* module id */ 10,
              /* fifo */ fifo_C_drain_C_drain_IO_L2_out_11,
              /* fifo */ fifo_C_drain_C_drain_IO_L2_out_10,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_10_0)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L2_out,
              /* module id */ 11,
              /* fifo */ fifo_C_drain_C_drain_IO_L2_out_12,
              /* fifo */ fifo_C_drain_C_drain_IO_L2_out_11,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_11_0)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L2_out_boundary,
              /* module id */ 12,
              /* fifo */ fifo_C_drain_C_drain_IO_L2_out_12,
              /* fifo */ fifo_C_drain_C_drain_IO_L1_out_12_0)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L3_out,
              /* fifo */ fifo_C_drain_C_drain_IO_L3_out_serialize,
              /* fifo */ fifo_C_drain_C_drain_IO_L2_out_0)
      /* Module Call */

      /* Module Call */
      .invoke(C_drain_IO_L3_out_serialize,
              /* array */ C,
              /* fifo */ fifo_C_drain_C_drain_IO_L3_out_serialize)
      /* Module Call */

      ;
}
