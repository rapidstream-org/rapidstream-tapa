// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include <tapa.h>

void Add(tapa::istream<float>& a, tapa::istream<float>& b,
         tapa::ostream<float>& c) {
  [[tapa::pipeline(1)]] TAPA_WHILE_NEITHER_EOT(a, b) {
    c << (a.read() + b.read());
  }
}

void VecAdd(tapa::istream<float>& a, tapa::istream<float>& b,
            tapa::ostream<float>& c) {
  tapa::task().invoke(Add, a, b, c);
}
