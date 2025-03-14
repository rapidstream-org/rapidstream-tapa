// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include "vadd.h"

void Add(tapa::istream<float>& a, tapa::istream<float>& b,
         tapa::ostream<float>& c, uint64_t n) {
  for (uint64_t i = 0; i < n; ++i) {
    c << (a.read() + b.read());
  }
}

void Pass(tapa::istream<float>& a, tapa::ostream<float>& b, uint64_t n) {
  for (uint64_t i = 0; i < n; ++i) {
    b << a.read();
  }
}

void AddUpper(tapa::istream<float>& a, tapa::istream<float>& b,
              tapa::ostream<float>& c, uint64_t n) {
  tapa::stream<float> pass_strm("pass");
  tapa::stream<float> pass_strm2("pass2");
  tapa::task()
      .invoke(Add, a, b, pass_strm, n)
      .invoke(Pass, pass_strm, pass_strm2, n)
      .invoke(Pass, pass_strm2, c, n);
}

void Mmap2Stream(tapa::mmap<const float> mmap, uint64_t n,
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

void VecAdd(tapa::mmap<const float> a, tapa::mmap<const float> b,
            tapa::mmap<float> c, uint64_t n) {
  tapa::stream<float> a_q("a");
  tapa::stream<float> b_q("b");
  tapa::stream<float> c_q("c");

  tapa::task()
      .invoke(Mmap2Stream, a, n, a_q)
      .invoke(Mmap2Stream, b, n, b_q)
      .invoke(AddUpper, a_q, b_q, c_q, n)
      .invoke(Stream2Mmap, c_q, c, n);
}
