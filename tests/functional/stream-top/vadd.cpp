// Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include <cstdint>

#include <tapa.h>

#include "vadd.h"

void StreamAdd(tapa::istream<input_t>& a, tapa::istream<input_t>& b,
               tapa::ostream<float>& c) {
  // TEST 4: Ensure that the peek function (eot) is supported by the stream
  // in both leaf and non-leaf, FRT and non-FRT kernel invocation.
  TAPA_WHILE_NEITHER_EOT(a, b) {
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

void StreamAdd_XRT(tapa::istream<input_t>& a, tapa::istream<input_t>& b,
                   tapa::ostream<float>& c) {
  // TEST 5: Ensure that both leaf and non-leaf FRT kernel invocation work.
  // In Vitis mode, StreamAdd_XRT is invoked by the XRT runtime, which is
  // a non-leaf FRT kernel invocation. In HLS mode, StreamAdd (not _XRT) is
  // invoked by the tapa-fast-cosim runtime, which is a leaf task.
  tapa::task().invoke(StreamAdd, a, b, c);
}
