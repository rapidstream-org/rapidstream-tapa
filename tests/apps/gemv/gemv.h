// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#pragma once

#include <cstdint>

#include <ap_int.h>
#include <tapa.h>

using Data = int;

constexpr int kMaxMatrixSize = 2048;  // 8192; // 16384;
constexpr int kVecLen = 64 / sizeof(Data);

using DataVec = tapa::vec_t<Data, kVecLen>;

constexpr int kMatrixSize = 2048;  // 4096; // 16384;

constexpr int kPcCount = 2;
constexpr int kPeCount = 2;

constexpr int64_t kPcSize = 128 * 1024;

template <typename T>
using bits = ap_uint<tapa::widthof<T>()>;

void Gemv(tapa::hmap<bits<DataVec>, kPcCount, kPcSize> mat_a,
          tapa::mmap<bits<DataVec>> vec_x, tapa::mmap<bits<DataVec>> vec_y);
