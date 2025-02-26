// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

// C++ STL
#include <iostream>

// 3rd-party library
#include <gflags/gflags.h>
#include <tapa.h>

void StreamAdd(tapa::istream<float>& a, tapa::istream<float>& b,
               tapa::ostream<float>& c, uint64_t n);

DEFINE_string(bitstream, "", "path to bitstream file, run csim if empty");

int main(int argc, char* argv[]) {
  gflags::ParseCommandLineFlags(&argc, &argv, /*remove_flags=*/true);

  const uint64_t n =
      argc > 1 ? atoll(argv[1]) : (FLAGS_bitstream.empty() ? 65536 : 1024);

  tapa::stream<float> a("a");
  tapa::stream<float> b("b");
  tapa::stream<float> c("c");

  bool has_error = false;
  tapa::task()
      .invoke([&] {
        for (uint64_t i = 0; i < n; ++i) {
          a.write(static_cast<float>(i));
          b.write(static_cast<float>(i) * 2);
        }
        a.close();
        b.close();
      })
      .invoke(StreamAdd, tapa::executable(FLAGS_bitstream), a, b, c, n)
      .invoke([&] {
        for (uint64_t i = 0; i < n; ++i) {
          auto actual = c.read();
          auto expected = i * 3;
          if (actual != expected) {
            std::cerr << "expected: " << expected << ", actual: " << actual
                      << std::endl;
            has_error = true;
          }
        }
        c.open();
      });

  if (!has_error) std::clog << "PASS!" << std::endl;
  return has_error;
}
