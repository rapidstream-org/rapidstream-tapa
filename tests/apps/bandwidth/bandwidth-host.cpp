// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include <chrono>
#include <iostream>
#include <vector>

#include <gflags/gflags.h>

#include "bandwidth.h"

template <typename T>
using vector = std::vector<T, tapa::aligned_allocator<T>>;

DEFINE_string(bitstream, "", "path to bitstream file, run csim if empty");

int main(int argc, char* argv[]) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  const uint64_t n = argc > 1 ? atoll(argv[1]) : 1024 * 1024;
  const uint64_t flags = argc > 2 ? atoll(argv[2]) : 6LL;

  vector<float> chan[kBankCount];
  for (uint64_t i = 0; i < kBankCount; ++i) {
    chan[i].resize(n * Elem::length);
    for (uint64_t j = 0; j < n * Elem::length; ++j) {
      chan[i][j] = i ^ j;
    }
  }

  tapa::invoke(Bandwidth, FLAGS_bitstream,
               tapa::read_write_mmaps<float, kBankCount>(chan)
                   .vectorized<Elem::length>(),
               n, flags);

  if (!((flags & kRead) && (flags & kWrite))) return 0;

  int64_t num_errors = 0;
  const int64_t threshold = 10;  // only report up to these errors
  for (uint64_t i = 0; i < kBankCount; ++i) {
    for (uint64_t j = 0; j < n * Elem::length; ++j) {
      int64_t expected = i ^ j;
      int64_t actual = chan[i][j];
      if (actual != expected) {
        if (num_errors < threshold) {
          LOG(ERROR) << "expected: " << expected << ", actual: " << actual;
        } else if (num_errors == threshold) {
          LOG(ERROR) << "...";
        }
        ++num_errors;
      }
    }
  }
  if (num_errors == 0) {
    LOG(INFO) << "PASS!";
  } else {
    if (num_errors > threshold) {
      LOG(WARNING) << " (+" << (num_errors - threshold) << " more errors)";
    }
    LOG(INFO) << "FAIL!";
  }
  return num_errors > 0 ? 1 : 0;
}
