// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

// C++ STL
#include <iostream>

// 3rd-party library
#include <gflags/gflags.h>
#include <tapa.h>

DEFINE_string(bitstream, "", "path to bitstream file, run csim if empty");

void StreamAdd(tapa::istream<float>& a, tapa::istream<float>& b,
               tapa::ostream<float>& c);

int main(int argc, char* argv[]) {
  gflags::ParseCommandLineFlags(&argc, &argv, /*remove_flags=*/true);

  const uint64_t n =
      argc > 1 ? atoll(argv[1]) : (FLAGS_bitstream.empty() ? 65536 : 1024);

  tapa::stream<float> a("a");
  tapa::stream<float> b("b");
  tapa::stream<float> c("c");

  bool has_error = false;
  tapa::task()
      .invoke(
          // tapa data types must be argument of lambda instead of capture,
          // otherwise, the stream will not be binded as i/o stream.
          [&](tapa::ostream<float>& a, tapa::ostream<float>& b) {
            for (uint64_t i = 0; i < n; ++i) {
              a.write(static_cast<float>(i));
              b.write(static_cast<float>(i) * 2);
            }
            a.close();
            b.close();
          },
          a, b)
      .invoke(StreamAdd, tapa::executable(FLAGS_bitstream), a, b, c)
      .invoke(
          [&](tapa::istream<float>& c) {
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
          },
          c);

  if (!has_error) std::clog << "PASS!" << std::endl;
  return has_error;
}
