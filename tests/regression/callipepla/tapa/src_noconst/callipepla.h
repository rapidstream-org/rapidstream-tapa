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

#endif  // CALLIPEPLA_H
