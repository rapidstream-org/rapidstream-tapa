// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include <cstdint>

#include <tapa.h>

void StreamAdd(tapa::istream<float>& a, tapa::istream<float>& b,
               tapa::ostream<float>& c, uint64_t n) {
  for (uint64_t i = 0; i < n; ++i) c << (a.read() + b.read());
  a.open();
  b.open();
  c.close();
}

void VecAdd(tapa::istream<float>& a, tapa::istream<float>& b,
            tapa::ostream<float>& c, uint64_t n) {
  tapa::task().invoke(StreamAdd, a, b, c, n);
}
