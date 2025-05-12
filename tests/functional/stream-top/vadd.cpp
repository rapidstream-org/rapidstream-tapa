// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include <cstdint>

#include <tapa.h>

void StreamAdd(tapa::istream<float>& a, tapa::istream<float>& b,
               tapa::ostream<float>& c) {
  // Ensure that the peek function (eot) is supported by the stream
  TAPA_WHILE_NEITHER_EOT(a, b) { c << (a.read() + b.read()); }
  a.open();
  b.open();
  c.close();
}

void StreamAdd_XRT(tapa::istream<float>& a, tapa::istream<float>& b,
                   tapa::ostream<float>& c) {
  tapa::task().invoke(StreamAdd, a, b, c);
}
