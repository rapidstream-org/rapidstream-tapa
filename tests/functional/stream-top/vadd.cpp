// Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include <cstdint>

#include <tapa.h>

#include "vadd.h"

// TEST 7: Ensure that inline will not break Vitis HLS (issues/136).
inline void StreamAdd(tapa::istream<input_t>& a, tapa::istream<input_t>& b,
                      tapa::ostream<tapa::vec_t<OUTPUT_TYPE, 2>>& c) {
  // TEST 6: Ensure that tapa::vec_t can be used as a stream type.
  tapa::vec_t<OUTPUT_TYPE, 2> vec_out;
  int vec_idx = 0;

  // TEST 4: Ensure that the peek function (eot) is supported by the stream
  // in both leaf and non-leaf, FRT and non-FRT kernel invocation.
  TAPA_WHILE_NEITHER_EOT(a, b) {
    input_t da = a.read(), db = b.read();
    OUTPUT_TYPE data = 0;
    data += (da.skip_1 || da.skip_2) ? OUTPUT_TYPE(0)
                                     : OUTPUT_TYPE(da.value + da.offset);
    data += (db.skip_1 || db.skip_2) ? OUTPUT_TYPE(0)
                                     : OUTPUT_TYPE(db.value + db.offset);

    vec_out[vec_idx++] = data;
    if (vec_idx == 2) {
      c << vec_out;
      vec_idx = 0;
    }
  }
  a.open();
  b.open();
  c.close();
}

void StreamAdd_XRT(tapa::istream<input_t>& a, tapa::istream<input_t>& b,
                   tapa::ostream<tapa::vec_t<OUTPUT_TYPE, 2>>& c) {
  // TEST 5: Ensure that both leaf and non-leaf FRT kernel invocation work.
  // In Vitis mode, StreamAdd_XRT is invoked by the XRT runtime, which is
  // a non-leaf FRT kernel invocation. In HLS mode, StreamAdd (not _XRT) is
  // invoked by the tapa-fast-cosim runtime, which is a leaf task.
  tapa::task().invoke(StreamAdd, a, b, c);
}
