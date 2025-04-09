// Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include <tapa.h>

#include "vadd.h"

void StreamAdd(tapa::istream<input_t>& a, tapa::istream<input_t>& b,
               tapa::ostream<float>& c, uint64_t n) {
  for (uint64_t i = 0; i < n; ++i) {
    input_t da = a.read(), db = b.read();
    float data = 0.;
    data += (da.skip_1 || da.skip_2) ? 0. : (da.value + da.offset);
    data += (db.skip_1 || db.skip_2) ? 0. : (db.value + db.offset);
    c << data;
  }
  a.open();
  b.open();
  c.close();
}

void VecAdd(tapa::istream<input_t>& a, tapa::istream<input_t>& b,
            tapa::ostream<float>& c, uint64_t n) {
  tapa::task().invoke(StreamAdd, a, b, c, n);
}
