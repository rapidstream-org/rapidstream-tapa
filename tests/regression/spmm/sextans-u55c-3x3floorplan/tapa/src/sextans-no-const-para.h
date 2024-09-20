// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#ifndef SEXTANS_H
#define SEXTANS_H

#include <ap_int.h>
#include <tapa.h>

constexpr int NUM_CH_SPARSE = 8;
constexpr int NUM_CH_B = 4;
constexpr int NUM_CH_C = 8;

const int WINDOW_SIZE = 4096;
const int DEP_DIST_LOAD_STORE = 10;

const int B_PARTITION_FACTOR = 4;
const int URAM_DEPTH = 8192;

using float_v16 = tapa::vec_t<float, 16>;
using float_v8 = tapa::vec_t<float, 8>;

void Sextans(tapa::mmap<int> edge_list_ptr,
             tapa::mmaps<ap_uint<512>, NUM_CH_SPARSE> edge_list_ch,
             tapa::mmaps<float_v16, NUM_CH_B> mat_B_ch,
             tapa::mmaps<float_v16, NUM_CH_C> mat_C_ch_in,
             tapa::mmaps<float_v16, NUM_CH_C> mat_C_ch);

#endif  // SEXTANS_H
