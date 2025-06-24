// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include <cstdint>

#include <tapa.h>

#include "vadd.h"

void AddSingle(tapa::istream<float>& a, tapa::istream<float>& b,
               tapa::ostream<float>& c, uint64_t n) {
  for (uint64_t i = 0; i < n; ++i) {
    c << a.read() + b.read();
  }
}

void Add(tapa::istreams<float, M>& a, tapa::istreams<float, M>& b,
         tapa::ostreams<float, M>& c, uint64_t n) {
  tapa::task()
      // Test 1: Tapacc will automatically access the streams automatically
      .invoke<tapa::join>(AddSingle, a, b, c, n)
      .invoke<tapa::join>(AddSingle, a, b, c, n)
      // Test 2: With the ability to access streams by index
      .invoke<tapa::join>(AddSingle, a[3], b[3], c[3], n)
      .invoke<tapa::join>(AddSingle, a[2], b[2], c[2], n);
}

void Mmap2Stream(tapa::mmap<float> mmap, uint64_t n,
                 tapa::ostream<float>& stream) {
  for (uint64_t i = 0; i < n; ++i) {
    stream << mmap[i];
  }
}

void Stream2Mmap(tapa::istream<float>& stream, tapa::mmap<float> mmap,
                 uint64_t n) {
  for (uint64_t i = 0; i < n; ++i) {
    stream >> mmap[i];
  }
}

void VecAdd(tapa::mmaps<float, M> a, tapa::mmaps<float, M> b,
            tapa::mmaps<float, M> c, uint64_t n) {
  tapa::streams<float, M> a_q("a");
  tapa::streams<float, M> b_q("b");
  tapa::streams<float, M> c_q("c");

  tapa::task()
      // Test 3: Invoking multiple task instances with one invoke
      .invoke<tapa::join, M>(Mmap2Stream, a, n, a_q)
      .invoke<tapa::join, M>(Mmap2Stream, b, n, b_q)
      .invoke(Add, a_q, b_q, c_q, n)
      .invoke<tapa::join, M>(Stream2Mmap, c_q, c, n);
}
