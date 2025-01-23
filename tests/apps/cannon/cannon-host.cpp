// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include <cmath>

#include <iostream>
#include <limits>
#include <vector>

#include <gflags/gflags.h>

#include "cannon.h"

using std::abs;
using std::clog;
using std::endl;
using std::vector;

DEFINE_string(bitstream, "", "path to bitstream file, run csim if empty");

int main(int argc, char* argv[]) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  const uint64_t n = 32;  // Hardcoded for efficient hardware generation.
  vector<float> a_vec(n * n);
  vector<float> b_vec(n * n);
  vector<float> c_vec(n * n);
  auto a = reinterpret_cast<float(*)[n]>(a_vec.data());
  auto b = reinterpret_cast<float(*)[n]>(b_vec.data());
  auto c = reinterpret_cast<float(*)[n]>(c_vec.data());
  for (uint64_t i = 0; i < n; ++i) {
    for (uint64_t j = 0; j < n; ++j) {
      auto shuffle = [](uint64_t x, uint64_t n) -> float {
        return static_cast<float>((n / 2 - x) * (n / 2 - x));
      };
      a[i][j] = pow(shuffle(i, n), 1.8f) + shuffle(j, n);
      b[i][j] = pow(shuffle(j, n), 1.2f) + shuffle(i, n);
      c[i][j] = 0.f;
    }
  }

  // reshape the matrices into 2x2 blocks (each has size n/2 x n/2)
  const int p = 2;
  vector<float> a_buf(n * n);
  vector<float> b_buf(n * n);
  vector<float> c_buf(n * n);
  auto a_block = reinterpret_cast<float(*)[p][n / p][n / p]>(a_buf.data());
  auto b_block = reinterpret_cast<float(*)[p][n / p][n / p]>(b_buf.data());
  auto c_block = reinterpret_cast<float(*)[p][n / p][n / p]>(c_buf.data());

  for (uint64_t i = 0; i < p; ++i) {
    for (uint64_t j = 0; j < p; ++j) {
      for (uint64_t ii = 0; ii < n / p; ++ii) {
        for (uint64_t jj = 0; jj < n / p; ++jj) {
          a_block[i][(i + j) % p][ii][jj] = a[i * n / p + ii][j * n / p + jj];
          b_block[(i + j) % p][j][ii][jj] = b[i * n / p + ii][j * n / p + jj];
        }
      }
    }
  }

  int64_t kernel_time_ns = tapa::invoke(
      Cannon, FLAGS_bitstream, tapa::read_only_mmap<const float>(a_buf),
      tapa::read_only_mmap<const float>(b_buf),
      tapa::write_only_mmap<float>(c_buf), n);

  for (uint64_t i = 0; i < p; ++i) {
    for (uint64_t j = 0; j < p; ++j) {
      for (uint64_t ii = 0; ii < n / p; ++ii) {
        for (uint64_t jj = 0; jj < n / p; ++jj) {
          c[i * n / p + ii][j * n / p + jj] = c_block[i][j][ii][jj];
        }
      }
    }
  }
  clog << "kernel time: " << kernel_time_ns * 1e-9 << " s" << endl;

  uint64_t num_errors = 0;
  const uint64_t threshold = 10;  // only report up to these errors
  for (uint64_t i = 0; i < n; ++i) {
    for (uint64_t j = 0; j < n; ++j) {
      auto expected = 0.f;
      for (uint64_t k = 0; k < n; ++k) {
        expected += a[i][k] * b[k][j];
      }
      auto actual = c[i][j];
      if (abs(actual - expected) > 1e-4 * abs(expected)) {
        if (num_errors < threshold) {
          clog << "expected: " << expected << ", actual: " << actual << endl;
        } else if (num_errors == threshold) {
          clog << "...";
        }
        ++num_errors;
      }
    }
  }
  if (num_errors == 0) {
    clog << "PASS!" << endl;
  } else {
    if (num_errors > threshold) {
      clog << " (+" << (num_errors - threshold) << " more errors)" << endl;
    }
    clog << "FAIL!" << endl;
  }
  return num_errors > 0 ? 1 : 0;
}
