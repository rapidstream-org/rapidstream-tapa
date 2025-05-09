// Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include <tapa.h>
#include <cstdint>

void Consumer(tapa::istream<uint64_t>& data_stream) {
  TAPA_WHILE_NOT_EOT(data_stream) data_stream.read();
  data_stream.open();
}

void Blackhole(tapa::istream<uint64_t>& data_stream) {
  // The blackhole function consumes the data stream without processing it.
  //
  // It was known in a previous version that (1) when the top-level argument
  // is a stream directly feeding into the blackhole; and (2) when the blackhole
  // function has `_stream` in its name as a suffix, TAPA failed to connect the
  // top-level stream's data to the blackhole function with a peek port.
  //
  // This test exists to verify that the issue is resolved, and as a TODO,
  // it would be good to have a general blackhole task implemented in TAPA
  // as it is frequently used.
  tapa::task().invoke(Consumer, data_stream);
}
