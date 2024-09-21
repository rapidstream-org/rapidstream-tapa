// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#ifndef SEEPENS_H
#define SEEPENS_H

#include <ap_int.h>
#include <tapa.h>

// constexpr int NUM_CH_SPARSE = 16;
constexpr int NUM_CH_SPARSE = 24;  // or, 32, 40, 48, 56

constexpr int WINDOW_SIZE = 8192;
constexpr int DEP_DIST_LOAD_STORE = 10;
constexpr int X_PARTITION_FACTOR = 8;
constexpr int URAM_DEPTH =
    ((NUM_CH_SPARSE == 16) ? 3 : 2) * 4096;  // 16 -> 12,288, others -> 8,192

using float_v16 = tapa::vec_t<float, 16>;

void Serpens(tapa::mmap<int> edge_list_ptr,
             tapa::mmaps<ap_uint<512>, NUM_CH_SPARSE> edge_list_ch,
             tapa::mmap<float_v16> vec_X, tapa::mmap<float_v16> vec_Y,
             tapa::mmap<float_v16> vec_Y_out);

#endif  // SEEPENS_H
