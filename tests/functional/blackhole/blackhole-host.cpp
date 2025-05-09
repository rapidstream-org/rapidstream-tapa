// Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include <gflags/gflags.h>
#include <tapa.h>
#include <cstdint>

DEFINE_string(bitstream, "", "path to bitstream file, run csim if empty");

void Blackhole(tapa::istream<uint64_t>& data_stream);

int main(int argc, char* argv[]) {
  gflags::ParseCommandLineFlags(&argc, &argv, /*remove_flags=*/true);
  const uint64_t n = argc > 1 ? atoll(argv[1]) : 1024 * 1024;

  tapa::stream<uint64_t> data_stream("data_stream");
  tapa::task()
      .invoke(
          [n](tapa::ostream<uint64_t>& data_stream) {
            for (uint64_t i = 0; i < n; ++i) data_stream.write(i);
            data_stream.close();
          },
          data_stream)
      .invoke(Blackhole, tapa::executable(FLAGS_bitstream), data_stream);

  return 0;
}
