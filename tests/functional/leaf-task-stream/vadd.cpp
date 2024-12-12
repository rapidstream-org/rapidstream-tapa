// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include <cstdint>

#include <tapa.h>

// Leaf task to instantiate a stream as a sliding window.
void Mmap2Stream(tapa::mmap<const float> mmap, uint64_t n,
                 tapa::ostream<float>& stream) {
  // 9 so that line 18 and 19 can be scheduled in one cycle
  tapa::stream<float, 9> intermediate_stream("intermediate_stream");
  for (uint64_t i = 0; i < 8; ++i) {
    intermediate_stream << mmap[i];
  }
  for (uint64_t i = 8; i < n; ++i) {
    stream << intermediate_stream.read();
    intermediate_stream << mmap[i];
  }
  for (uint64_t i = 0; i < 8; ++i) {
    stream << intermediate_stream.read();
  }
}

void Stream2Mmap(tapa::istream<float>& stream, tapa::mmap<float> mmap,
                 uint64_t n) {
  for (uint64_t i = 0; i < n; ++i) {
    stream >> mmap[i];
  }
}

void Add(tapa::istream<float>& a, tapa::istream<float>& b,
         tapa::ostream<float>& c, uint64_t n) {
  for (uint64_t i = 0; i < n; ++i) {
    c << (a.read() + b.read());
  }
}

// Leaf task to instantiate a stream as a communication channel.
void Add2Mmap(tapa::istream<float>& a, tapa::istream<float>& b,
              tapa::mmap<float> c, uint64_t n) {
#pragma HLS dataflow
  tapa::stream<float, 2, tapa::kStreamInfiniteDepth> c_q("c");
  Add(a, b, c_q, n);
  Stream2Mmap(c_q, c, n);
}

void VecAdd(tapa::mmap<const float> a, tapa::mmap<const float> b,
            tapa::mmap<float> c, uint64_t n) {
  tapa::stream<float> a_q("a");
  tapa::stream<float> b_q("b");

  tapa::task()
      .invoke(Mmap2Stream, a, n, a_q)
      .invoke(Mmap2Stream, b, n, b_q)
      .invoke(Add2Mmap, a_q, b_q, c, n);
}
