// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include <thread>

#include <gflags/gflags.h>

#include <tapa.h>
#include <tapa/host/dpi/mmap_queue.h>

void VecAdd(tapa::istream<float>& a, tapa::istream<float>& b,
            tapa::ostream<float>& c);

DEFINE_string(bitstream, "", "path to bitstream file, run csim if empty");

int main(int argc, char* argv[]) {
  gflags::ParseCommandLineFlags(&argc, &argv, /*remove_flags=*/true);

  const uint64_t n = argc > 1 ? atoll(argv[1])
                              : (FLAGS_bitstream.empty() ? 1024 * 1024 : 1024);
  tapa::dpi::stream<float> a(n / 2);
  tapa::dpi::stream<float> b(n / 2);
  tapa::dpi::stream<float> c(n / 2);
  std::thread writer([&] {
    for (uint64_t i = 0; i < n; ++i) {
      a.write(static_cast<float>(i));
      b.write(static_cast<float>(i) * 2);
    }
    a.close();
    b.close();
  });
  uint64_t num_errors = 0;
  std::thread reader([&] {
    const uint64_t threshold = 10;  // only report up to these errors
    for (uint64_t i = 0; i < n; ++i) {
      auto expected = i * 3;
      auto actual = static_cast<uint64_t>(c.read());
      if (actual != expected) {
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
  });
  tapa::invoke(VecAdd, FLAGS_bitstream, a, b, c);
  reader.join();
  writer.join();

  return num_errors > 0 ? 1 : 0;
}
