// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include <tapa.h>
#include <iostream>

void StreamAdd(tapa::istream<float>& a, tapa::istream<float>& b,
               tapa::ostream<float>& c, uint64_t n);

int main(int argc, char* argv[]) {
  const uint64_t n = argc > 1 ? atoll(argv[1]) : 1024 * 1024;

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
      })
      .invoke(StreamAdd, a, b, c, n)
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
      });

  if (!has_error) std::clog << "PASS!" << std::endl;
  return has_error;
}
