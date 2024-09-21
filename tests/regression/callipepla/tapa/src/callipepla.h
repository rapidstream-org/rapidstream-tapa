// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#ifndef CALLIPEPLA_H
#define CALLIPEPLA_H

#include <ap_int.h>
#include <tapa.h>

constexpr int NUM_CH_SPARSE = 16;

constexpr int X_PARTITION_FACTOR =
    4;  // BRAMs = 512 / 16 / 2 = 16 -> factor = 16 / (64 / 16)
constexpr int WINDOW_SIZE = X_PARTITION_FACTOR * 1024;
constexpr int DEP_DIST_LOAD_STORE = 7;
constexpr int URAM_DEPTH = 3 * 4096;
// ch16: 3 * 4096 * 16 * 8 = 1,572,864

using double_v8 = tapa::vec_t<double, 8>;

void Callipepla(tapa::mmap<int> edge_list_ptr,
                tapa::mmaps<ap_uint<512>, NUM_CH_SPARSE> edge_list_ch,
                tapa::mmaps<double_v8, 2> vec_x,
                tapa::mmaps<double_v8, 2> vec_p, tapa::mmap<double_v8> vec_Ap,
                tapa::mmaps<double_v8, 2> vec_r, tapa::mmap<double_v8> vec_digA,
                tapa::mmap<double> vec_res, const int NUM_ITE,
                const int NUM_A_LEN, const int M, const int rp_time,
                const double th_termination);

#endif  // CALLIPEPLA_H
