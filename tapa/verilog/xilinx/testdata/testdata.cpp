// Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include <tapa.h>

using Data = uint64_t;

void LowerLevelTask(tapa::istream<Data>& istream,
                    tapa::ostreams<Data, 1>& ostreams, int scalar) {}

void UpperLevelTask(tapa::istream<Data>& istream,
                    tapa::ostreams<Data, 1>& ostreams, int scalar) {
  tapa::task().invoke(LowerLevelTask, istream, ostreams, scalar);
}

void Produce(tapa::mmap<Data> mmap, int scalar, tapa::ostream<Data>& ostream) {
produce:
  [[tapa::pipeline(1)]] for (int i = 0; i < scalar; ++i) {
    ostream.write(mmap[i]);
  }
}

void Consume(tapa::mmap<Data> mmap, int scalar, tapa::istream<Data> istream) {
consume:
  [[tapa::pipeline(1)]] for (int i = 0; i < scalar; ++i) {
    mmap[i] = istream.read();
  }
}

void TopLevelTask(tapa::mmap<Data> mmap_in, tapa::mmap<Data> mmap_out,
                  int scalar) {
  tapa::streams<Data, 1> stream_in;
  tapa::streams<Data, 1> stream_out;
  tapa::task()
      .invoke(Produce, mmap_in, scalar, stream_in)
      .invoke(UpperLevelTask, stream_in, stream_out, scalar)
      .invoke(Consume, mmap_out, scalar, stream_out);
}
