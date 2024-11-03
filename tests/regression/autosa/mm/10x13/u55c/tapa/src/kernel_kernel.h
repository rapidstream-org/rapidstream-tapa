// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include <ap_int.h>
#include <tapa.h>

/* Data Type */
typedef float A_t1;
typedef float B_t1;
typedef float C_t1;
typedef tapa::vec_t<float, 16> A_t16;
typedef tapa::vec_t<float, 16> B_t16;
typedef tapa::vec_t<float, 16> C_t16;
typedef tapa::vec_t<float, 4> C_t4;
/* Data Type */

void kernel0(tapa::mmap<A_t16> A, tapa::mmap<B_t16> B, tapa::mmap<C_t16> C);
void A_IO_L2_in_intra_trans(int idx, A_t16 local_A[8][4],
                            tapa::ostream<A_t16>& fifo_A_local_out,
                            bool intra_trans_en);
void A_IO_L2_in_inter_trans(int idx, A_t16 local_A[8][4],
                            tapa::istream<A_t16>& fifo_A_in,
                            tapa::ostream<A_t16>& fifo_A_out,
                            bool inter_trans_en);
void A_IO_L2_in_inter_trans_boundary(int idx, A_t16 local_A[8][4],
                                     tapa::istream<A_t16>& fifo_A_in,
                                     bool inter_trans_en);
void B_IO_L2_in_intra_trans(int idx, B_t16 local_B[8][4],
                            tapa::ostream<B_t16>& fifo_B_local_out,
                            bool intra_trans_en);
void B_IO_L2_in_inter_trans(int idx, B_t16 local_B[8][4],
                            tapa::istream<B_t16>& fifo_B_in,
                            tapa::ostream<B_t16>& fifo_B_out,
                            bool inter_trans_en);
void B_IO_L2_in_inter_trans_boundary(int idx, B_t16 local_B[8][4],
                                     tapa::istream<B_t16>& fifo_B_in,
                                     bool inter_trans_en);
void PE_wrapper(int idx, int idy, tapa::istream<A_t16>& fifo_A_in,
                tapa::ostream<A_t16>& fifo_A_out,
                tapa::istream<B_t16>& fifo_B_in,
                tapa::ostream<B_t16>& fifo_B_out,
                tapa::ostream<float>& fifo_C_drain_out);
void C_drain_IO_L1_out_intra_trans(int idx, int idy, C_t4 local_C[8][2],
                                   tapa::istream<float>& fifo_C_drain_local_in);
void C_drain_IO_L1_out_inter_trans(int idx, int idy, C_t4 local_C[8][2],
                                   tapa::istream<C_t4>& fifo_C_drain_in,
                                   tapa::ostream<C_t4>& fifo_C_drain_out);
void C_drain_IO_L1_out_inter_trans_boundary(
    int idx, int idy, C_t4 local_C[8][2],
    tapa::ostream<C_t4>& fifo_C_drain_out);
void C_drain_IO_L1_out_wrapper(int idx, int idy,
                               tapa::istream<C_t4>& fifo_C_drain_in,
                               tapa::ostream<C_t4>& fifo_C_drain_out,
                               tapa::istream<float>& fifo_C_drain_local_in);
void C_drain_IO_L1_out_boundary_wrapper(
    int idx, int idy, tapa::ostream<C_t4>& fifo_C_drain_out,
    tapa::istream<float>& fifo_C_drain_local_in);
