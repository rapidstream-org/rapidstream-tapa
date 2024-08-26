// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include <chrono>
#include <fstream>
#include <iostream>
#include <vector>

#include <gflags/gflags.h>
#include <tapa.h>
#include "SEIDEL2D.h"

using std::clog;
using std::endl;
using std::vector;
using std::chrono::duration;
using std::chrono::high_resolution_clock;

// void VecAdd(tapa::mmap<const float> a_array, tapa::mmap<const float> b_array,
//             tapa::mmap<float> c_array, uint64_t n);

void unikernel(
    tapa::mmap<INTERFACE_WIDTH> in_0,
    tapa::mmap<INTERFACE_WIDTH> out_0,  // HBM 0 1
    tapa::mmap<INTERFACE_WIDTH> in_1, tapa::mmap<INTERFACE_WIDTH> out_1,
    tapa::mmap<INTERFACE_WIDTH> in_2, tapa::mmap<INTERFACE_WIDTH> out_2,
    tapa::mmap<INTERFACE_WIDTH> in_3, tapa::mmap<INTERFACE_WIDTH> out_3,
    tapa::mmap<INTERFACE_WIDTH> in_4, tapa::mmap<INTERFACE_WIDTH> out_4,
    tapa::mmap<INTERFACE_WIDTH> in_5, tapa::mmap<INTERFACE_WIDTH> out_5,
    tapa::mmap<INTERFACE_WIDTH> in_6, tapa::mmap<INTERFACE_WIDTH> out_6,
    tapa::mmap<INTERFACE_WIDTH> in_7, tapa::mmap<INTERFACE_WIDTH> out_7,
    tapa::mmap<INTERFACE_WIDTH> in_8, tapa::mmap<INTERFACE_WIDTH> out_8,
    tapa::mmap<INTERFACE_WIDTH> in_9, tapa::mmap<INTERFACE_WIDTH> out_9,
    tapa::mmap<INTERFACE_WIDTH> in_10, tapa::mmap<INTERFACE_WIDTH> out_10,
    tapa::mmap<INTERFACE_WIDTH> in_11, tapa::mmap<INTERFACE_WIDTH> out_11,
    tapa::mmap<INTERFACE_WIDTH> in_12, tapa::mmap<INTERFACE_WIDTH> out_12,
    tapa::mmap<INTERFACE_WIDTH> in_13, tapa::mmap<INTERFACE_WIDTH> out_13,
    tapa::mmap<INTERFACE_WIDTH> in_14, tapa::mmap<INTERFACE_WIDTH> out_14,
    uint32_t iters);

DEFINE_string(bitstream, "", "path to bitstream file, run csim if empty");

int main(int argc, char* argv[]) {
  gflags::ParseCommandLineFlags(&argc, &argv, /*remove_flags=*/true);

  srand(time(NULL));

  // Data initialization
  //  const uint64_t n = argc > 1 ? atoll(argv[1]) : 1024 * 1024;
  printf("midle_region = %d\n", MIDLE_REGION);
  vector<INTERFACE_WIDTH> in_0(MIDLE_REGION);
  vector<INTERFACE_WIDTH> out_0(MIDLE_REGION);
  // vector<INTERFACE_WIDTH> in_1(MIDLE_REGION); vector<INTERFACE_WIDTH>
  // out_1(MIDLE_REGION); vector<INTERFACE_WIDTH> in_2(MIDLE_REGION);
  // vector<INTERFACE_WIDTH> out_2(MIDLE_REGION); vector<INTERFACE_WIDTH>
  // in_3(MIDLE_REGION); vector<INTERFACE_WIDTH> out_3(MIDLE_REGION);
  // vector<INTERFACE_WIDTH> in_4(MIDLE_REGION); vector<INTERFACE_WIDTH>
  // out_4(MIDLE_REGION); vector<INTERFACE_WIDTH> in_5(MIDLE_REGION);
  // vector<INTERFACE_WIDTH> out_5(MIDLE_REGION); vector<INTERFACE_WIDTH>
  // in_6(MIDLE_REGION); vector<INTERFACE_WIDTH> out_6(MIDLE_REGION);
  // vector<INTERFACE_WIDTH> in_7(MIDLE_REGION); vector<INTERFACE_WIDTH>
  // out_7(MIDLE_REGION); vector<INTERFACE_WIDTH> in_8(MIDLE_REGION);
  // vector<INTERFACE_WIDTH> out_8(MIDLE_REGION); vector<INTERFACE_WIDTH>
  // in_9(MIDLE_REGION); vector<INTERFACE_WIDTH> out_9(MIDLE_REGION);
  // vector<INTERFACE_WIDTH> in_10(MIDLE_REGION); vector<INTERFACE_WIDTH>
  // out_10(MIDLE_REGION); vector<INTERFACE_WIDTH> in_11(MIDLE_REGION);
  // vector<INTERFACE_WIDTH> out_11(MIDLE_REGION); for (uint64_t i = 0; i < n;
  // ++i) {
  //   a[i] = static_cast<float>(i);
  //   b[i] = static_cast<float>(i) * 2;
  //   c[i] = 0.f;
  // }
  // Software emulation vector
  float sw_in[MIDLE_REGION * WIDTH_FACTOR];
  float sw_out[MIDLE_REGION * WIDTH_FACTOR];

  // input test
  for (int i = 0; i < MIDLE_REGION; i++) {
    INTERFACE_WIDTH a;
    float temp = rand() % 100 + 1;
    // float temp = (i * i) % 100;
    for (int k = 0; k < WIDTH_FACTOR; k++) {
      unsigned int idx_k = k << 5;
      // float temp = (i * WIDTH_FACTOR + k);

      a.range(idx_k + 31, idx_k) = *((uint32_t*)(&temp));
      sw_in[i * WIDTH_FACTOR + k] = temp;
    }
    in_0[i] = a;
    out_0[i] = a;
    in_1[i] = a;
    out_1[i] = a;
    in_2[i] = a;
    out_2[i] = a;
    in_3[i] = a;
    out_3[i] = a;
    in_4[i] = a;
    out_4[i] = a;
    in_5[i] = a;
    out_5[i] = a;
    in_6[i] = a;
    out_6[i] = a;
    in_7[i] = a;
    out_7[i] = a;
    in_8[i] = a;
    out_8[i] = a;
    in_9[i] = a;
    out_9[i] = a;
    in_10[i] = a;
    out_10[i] = a;
    in_11[i] = a;
    out_11[i] = a;
    in_12[i] = a;
    out_12[i] = a;
    in_13[i] = a;
    out_13[i] = a;
    in_14[i] = a;
    out_14[i] = a;
  }
  const uint32_t iter = 1;

  // Software result

  // for(int i = 0; i < MIDLE_REGION * WIDTH_FACTOR; i++){
  //   sw_in[i] = i;
  // }

  for (int n = 0; n < iter; n++) {
    for (int i = 1025; i < MIDLE_REGION * WIDTH_FACTOR - 1025; i++) {
      sw_out[i] = (sw_in[i - 1024] + sw_in[i - 1025] + sw_in[i - 1023] +
                   sw_in[i - 1] + sw_in[i] + sw_in[i + 1] + sw_in[i + 1023] +
                   sw_in[i + 1] + sw_in[i + 1025]) /
                  (float)9;
    }
  }
  // std::cout << (GRID_COLS/WIDTH_FACTOR*PART_ROWS +
  // (TOP_APPEND+BOTTOM_APPEND)*(1-1)) << endl; std::cout << MIDLE_REGION <<
  // endl;

  std::cout << "kernel start" << endl;
  // Kernel execution
  auto start = high_resolution_clock::now();
  // tapa::invoke(VecAdd, FLAGS_bitstream, tapa::read_only_mmap<const float>(a),
  //              tapa::read_only_mmap<const float>(b),
  //              tapa::write_only_mmap<float>(c), n);
  tapa::invoke(unikernel, FLAGS_bitstream,
               tapa::read_write_mmap<INTERFACE_WIDTH>(in_0),
               tapa::read_write_mmap<INTERFACE_WIDTH>(out_0),
               tapa::read_write_mmap<INTERFACE_WIDTH>(in_1),
               tapa::read_write_mmap<INTERFACE_WIDTH>(out_1),
               tapa::read_write_mmap<INTERFACE_WIDTH>(in_2),
               tapa::read_write_mmap<INTERFACE_WIDTH>(out_2),
               tapa::read_write_mmap<INTERFACE_WIDTH>(in_3),
               tapa::read_write_mmap<INTERFACE_WIDTH>(out_3),
               tapa::read_write_mmap<INTERFACE_WIDTH>(in_4),
               tapa::read_write_mmap<INTERFACE_WIDTH>(out_4),
               tapa::read_write_mmap<INTERFACE_WIDTH>(in_5),
               tapa::read_write_mmap<INTERFACE_WIDTH>(out_5),
               tapa::read_write_mmap<INTERFACE_WIDTH>(in_6),
               tapa::read_write_mmap<INTERFACE_WIDTH>(out_6),
               tapa::read_write_mmap<INTERFACE_WIDTH>(in_7),
               tapa::read_write_mmap<INTERFACE_WIDTH>(out_7),
               tapa::read_write_mmap<INTERFACE_WIDTH>(in_8),
               tapa::read_write_mmap<INTERFACE_WIDTH>(out_8),
               tapa::read_write_mmap<INTERFACE_WIDTH>(in_9),
               tapa::read_write_mmap<INTERFACE_WIDTH>(out_9),
               tapa::read_write_mmap<INTERFACE_WIDTH>(in_10),
               tapa::read_write_mmap<INTERFACE_WIDTH>(out_10),
               tapa::read_write_mmap<INTERFACE_WIDTH>(in_11),
               tapa::read_write_mmap<INTERFACE_WIDTH>(out_11),
               tapa::read_write_mmap<INTERFACE_WIDTH>(in_12),
               tapa::read_write_mmap<INTERFACE_WIDTH>(out_12),
               tapa::read_write_mmap<INTERFACE_WIDTH>(in_13),
               tapa::read_write_mmap<INTERFACE_WIDTH>(out_13),
               tapa::read_write_mmap<INTERFACE_WIDTH>(in_14),
               tapa::read_write_mmap<INTERFACE_WIDTH>(out_14), iter);
  auto stop = high_resolution_clock::now();
  duration<double> elapsed = stop - start;
  clog << "elapsed time: " << elapsed.count() << "s" << endl;

  // Verification
  uint64_t num_errors = 0;
  const uint64_t threshold = 10;  // only report up to these errors
  for (int i = 66; i < 128; i++) {
    for (int k = 0; k < WIDTH_FACTOR; k++) {
      unsigned int idx_k = k << 5;
      uint32_t temp = out_0[i + 66].range(idx_k + 31, idx_k);
      float hw_result = (*((float*)(&temp)));
      if (sw_out[i * WIDTH_FACTOR + k] != hw_result) {
        ++num_errors;
        std::cout << (i * WIDTH_FACTOR + k) << " " << hw_result << " "
                  << sw_out[i * WIDTH_FACTOR + k] << endl;
      }
      // sstd::cout << (i * WIDTH_FACTOR + k) << " " << hw_result << " " <<
      // sw_out[i * WIDTH_FACTOR + k] << endl;
    }
  }
  // for (uint64_t i = 0; i < n; ++i) {
  //   auto expected = i * 3;
  //   auto actual = static_cast<uint64_t>(c[i]);
  //   if (actual != expected) {
  //     if (num_errors < threshold) {
  //       clog << "expected: " << expected << ", actual: " << actual << endl;
  //     } else if (num_errors == threshold) {
  //       clog << "...";
  //     }
  //     ++num_errors;
  //   }
  // }

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
