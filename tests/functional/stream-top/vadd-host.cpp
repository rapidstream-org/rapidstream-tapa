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

void StreamAdd_HostTask(tapa::istream<input_t>& a, tapa::istream<input_t>& b,
                        tapa::ostream<OUTPUT_TYPE>& c) {
  tapa::stream<tapa::vec_t<OUTPUT_TYPE, 2>> c_vec("c_vec");

  tapa::task()
      // TEST 1: Invoke an FRT kernel with lock-free streams.
      // Here, a and b had already been handshaked as lock-free streams. Passing
      // the streams to the FRT kernel requires a passthrough conversion.
      .invoke(StreamAdd, tapa::executable(FLAGS_bitstream), a, b, c_vec)
      .invoke(
          // Convert the vec_t stream to a stream.
          [&](tapa::istream<tapa::vec_t<OUTPUT_TYPE, 2>>& c_vec,
              tapa::ostream<OUTPUT_TYPE>& c) {
            tapa::vec_t<OUTPUT_TYPE, 2> vec_out;
            TAPA_WHILE_NOT_EOT(c_vec) {
              vec_out = c_vec.read();
              c.write(vec_out[0]);
              c.write(vec_out[1]);
            }
            c_vec.open();
            c.close();
          },
          c_vec, c);
}

int main(int argc, char* argv[]) {
  gflags::ParseCommandLineFlags(&argc, &argv, /*remove_flags=*/true);

  const uint64_t n =
      argc > 1 ? atoll(argv[1]) : (FLAGS_bitstream.empty() ? 65536 : 1024);

  tapa::stream<input_t> a("a");
  tapa::stream<input_t> b("b");
  tapa::stream<OUTPUT_TYPE> c("c");

  bool has_error = false;
  tapa::task()
      .invoke(
          // TEST 2: Use lambda as simulation tasks
          // tapa data types must be argument of lambda instead of capture,
          // otherwise, the stream will not be binded as i/o stream.
          [&](tapa::ostream<input_t>& a, tapa::ostream<input_t>& b) {
            for (uint64_t i = 0; i < n; ++i) {
              // TEST 3: Ensure that a packed struct can be used in a stream.
              a.write(input_t{/*skip_1=*/i % 6 == 1,
                              /*value=*/static_cast<float>(i),
                              /*offset=*/i % 32,
                              /*skip_2=*/i % 7 == 1});
              b.write(input_t{/*skip_1=*/i % 10 == 1,
                              /*value=*/static_cast<float>(i) * 2,
                              /*offset=*/i % 31,
                              /*skip_2=*/i % 9 == 1});
            }
            a.close();
            b.close();
          },
          a, b)
      .invoke(StreamAdd_HostTask, a, b, c)
      .invoke(
          [&](tapa::istream<OUTPUT_TYPE>& c) {
            for (uint64_t i = 0; i < n; ++i) {
              auto actual = c.read();
              OUTPUT_TYPE expected = 0;
              // if a was not skipped, add a.value + a.offset
              if (i % 6 != 1 && i % 7 != 1) expected += i + (i % 32);
              // if b was not skipped, add b.value + b.offset
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
