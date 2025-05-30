// Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include "vadd.h"

// TEST 1: integer template parameters
template <int offset>
void Add(tapa::istream<float>& a, tapa::istream<int>& b,
         tapa::ostream<float>& c, uint64_t n) {
  for (uint64_t i = 0; i < n; ++i) {
    c << (a.read() + b.read() + offset);
  }
}

// TEST 2: type template parameters
template <typename mmap_type, typename stream_type>
void Mmap2Stream(tapa::mmap<mmap_type> mmap, uint64_t n,
                 tapa::ostream<stream_type>& stream) {
  for (uint64_t i = 0; i < n; ++i) {
    stream << mmap[i];
  }
}

// TEST 3: mixed template parameters
template <typename mmap_type, typename stream_type, int offset>
void Stream2Mmap(tapa::istream<stream_type>& stream, tapa::mmap<mmap_type> mmap,
                 uint64_t n) {
  for (uint64_t i = 0; i < n; ++i) {
    stream >> mmap[i];
    mmap[i] += offset;
  }
}

void VecAdd(tapa::mmap<const float> a, tapa::mmap<const float> b,
            tapa::mmap<float> c, uint64_t n) {
  tapa::stream<float> a_q("a");
  tapa::stream<int> b_q("b");
  tapa::stream<float> c_q("c");

  tapa::task()
      .invoke(Mmap2Stream<const float, float>, a, n, a_q)
      .invoke(Mmap2Stream<const float, int>, b, n, b_q)
      .invoke(Add<1>, a_q, b_q, c_q, n)
      .invoke(Stream2Mmap<float, float, -1>, c_q, c, n);
}

// TODO: support non-leaf template tasks
// TODO: support tapa::i/ostream and tapa::mmap as part of the parameter
