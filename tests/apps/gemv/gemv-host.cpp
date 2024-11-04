// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include <random>
#include <vector>

#include <gflags/gflags.h>
#include <tapa.h>

#include "gemv.h"

using std::vector;

void Gemv(tapa::hmap<bits<DataVec>, kPcCount, kPcSize> mat_a,
          tapa::mmap<bits<DataVec>> vec_x, tapa::mmap<bits<DataVec>> vec_y);

DEFINE_string(bitstream, "", "path to bitstream file, run csim if empty");

int main(int argc, char* argv[]) {
  gflags::ParseCommandLineFlags(&argc, &argv, /*remove_flags=*/true);

  LOG(INFO) << "Matrix size: " << kMatrixSize << " x " << kMatrixSize;

  std::vector<Data, tapa::aligned_allocator<Data>> a(kMatrixSize * kMatrixSize);
  std::vector<Data, tapa::aligned_allocator<Data>> x(kMatrixSize);
  std::vector<Data, tapa::aligned_allocator<Data>> y(kMatrixSize);
  std::vector<Data, tapa::aligned_allocator<Data>> host_y(kMatrixSize);

  std::mt19937 gen;
  std::uniform_int_distribution<Data> dist;
  for (int i = 0; i < kMatrixSize; i++) {
    for (int j = 0; j < kMatrixSize; j++) {
      a[i * kMatrixSize + j] = dist(gen);
    }
    x[i] = dist(gen);
  }

  for (int j = 0; j < kMatrixSize; j++) {
    y[j] = 0;
    host_y[j] = 0;
    for (int i = 0; i < kMatrixSize; i++) {
      host_y[j] += a[i * kMatrixSize + j] * x[i];
    }
  }

  int64_t kernel_time_ns =
      tapa::invoke(Gemv, FLAGS_bitstream,
                   tapa::read_only_mmap<Data>(a).reinterpret<bits<DataVec>>(),
                   tapa::read_only_mmap<Data>(x).reinterpret<bits<DataVec>>(),
                   tapa::write_only_mmap<Data>(y).reinterpret<bits<DataVec>>());

  LOG(INFO) << "kernel time: " << kernel_time_ns * 1e-9 << " s";
  float gops = 2.0 * kMatrixSize * kMatrixSize / kernel_time_ns;
  LOG(INFO) << "GOPS: " << gops << " (" << gops / 2.0 * sizeof(Data)
            << " GB/s)";

  int64_t num_errors = 0;
  const int64_t threshold = 10;  // only report up to these errors
  for (int i = 0; i < kMatrixSize; i++) {
    Data expected = host_y[i];
    Data actual = y[i];

    if (expected != actual) {
      if (num_errors < threshold) {
        LOG(ERROR) << "expected: " << expected << ", actual: " << actual;
      } else if (num_errors == threshold) {
        LOG(ERROR) << "...";
      }
      ++num_errors;
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
