// Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include <tapa.h>

// TEST: Functions with the same name that are not inlined and used in different
// tasks should generate their own report files.

void Impl(tapa::mmap<const float> mmap, uint64_t n,
          tapa::ostream<float>& stream) {
#pragma HLS inline off
  for (uint64_t i = 0; i < n; ++i) {
    stream << mmap[i];
  }
}

void Impl(tapa::istream<float>& stream, tapa::mmap<float> mmap, uint64_t n) {
#pragma HLS inline off
  for (uint64_t i = 0; i < n; ++i) {
    stream >> mmap[i];
  }
}

void Mmap2Stream(tapa::mmap<const float> mmap, uint64_t n,
                 tapa::ostream<float>& stream) {
  Impl(mmap, n, stream);
}

void Stream2Mmap(tapa::istream<float>& stream, tapa::mmap<float> mmap,
                 uint64_t n) {
  Impl(stream, mmap, n);
}

void Add(tapa::istream<float>& a, tapa::istream<float>& b,
         tapa::ostream<float>& c, uint64_t n) {
  for (uint64_t i = 0; i < n; ++i) {
    // TEST: `fmadd` should work with `xcv80-lsva4737-2MHP-e-S` (#258)
    constexpr int kN = 16;
    tapa::vec_t<float, kN> a_vec, b_vec;
    float acc[kN] = {};
#pragma HLS array_partition variable = acc complete
    a_vec.set(a.read());
    b_vec.set(b.read());
    for (int i = 0; i < kN; ++i) {
#pragma HLS unroll
      acc[i] += a_vec[i] * b_vec[i];
    }
    c << acc[0];
  }
}

void VecAdd(tapa::mmap<const float> a, tapa::mmap<const float> b,
            tapa::mmap<float> c, uint64_t n) {
  tapa::stream<float> a_q("a");
  tapa::stream<float> b_q("b");
  tapa::stream<float> c_q("c");

  tapa::task()
      .invoke(Mmap2Stream, a, n, a_q)
      .invoke(Mmap2Stream, b, n, b_q)
      .invoke(Add, a_q, b_q, c_q, n)
      .invoke(Stream2Mmap, c_q, c, n);
}
