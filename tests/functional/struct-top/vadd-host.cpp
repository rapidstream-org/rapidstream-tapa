// Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

// C++ STL
#include <iostream>

// 3rd-party library
#include <gflags/gflags.h>
#include <tapa.h>

#include "vadd.h"

DEFINE_string(bitstream, "", "path to bitstream file, run csim if empty");

int main(int argc, char* argv[]) {
  gflags::ParseCommandLineFlags(&argc, &argv, /*remove_flags=*/true);

  const uint64_t n =
      argc > 1 ? atoll(argv[1]) : (FLAGS_bitstream.empty() ? 65536 : 1024);

  tapa::stream<input_t> a("a");
  tapa::stream<input_t> b("b");
  tapa::stream<float> c("c");

  bool has_error = false;
  tapa::task()
      .invoke(
          [&](tapa::ostream<input_t>& a, tapa::ostream<input_t>& b) {
            for (uint64_t i = 0; i < n; ++i) {
              a.write(input_t{i % 6 == 1, static_cast<float>(i), i % 32,
                              i % 7 == 1});
              b.write(input_t{i % 10 == 1, static_cast<float>(i) * 2, i % 31,
                              i % 9 == 1});
            }
            a.close();
            b.close();
          },
          a, b)
      .invoke(VecAdd, tapa::executable(FLAGS_bitstream), a, b, c, n)
      .invoke(
          [&](tapa::istream<float>& c) {
            for (uint64_t i = 0; i < n; ++i) {
              auto actual = c.read();
              float expected = 0;
              if (i % 6 != 1 && i % 7 != 1) expected += i + (i % 32);
              if (i % 10 != 1 && i % 9 != 1) expected += i * 2 + (i % 31);
              if (actual != expected) {
                std::cerr << "expected: " << expected << ", actual: " << actual
                          << std::endl;
                has_error = true;
              }
            }
            c.open();
          },
          c);

  if (!has_error) std::clog << "PASS!" << std::endl;
  return has_error;
}
