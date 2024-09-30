// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include <cstdint>

#include <tapa.h>

using tapa::istream;
using tapa::mmap;
using tapa::ostream;
using tapa::stream;
using tapa::task;

void Add(istream<float>& a, istream<float>& b, ostream<float>& c) {
  [[tapa::pipeline(1)]] TAPA_WHILE_NEITHER_EOT(a, b) {
    c.write(a.read(nullptr) + b.read(nullptr));
  }
  c.close();
}

void Mmap2Stream(mmap<float> mmap, int offset, uint64_t n,
                 ostream<float>& stream) {
  for (uint64_t i = 0; i < n; ++i) {
    stream.write(mmap[n * offset + i]);
  }
  stream.close();
}

void Stream2Mmap(istream<float>& stream, mmap<float> mmap, int offset,
                 uint64_t n) {
  for (uint64_t i = 0; i < n; ++i) {
    mmap[n * offset + i] = stream.read();
  }
}

void Load(mmap<float> srcs, uint64_t n, ostream<float>& a, ostream<float>& b) {
  task().invoke(Mmap2Stream, srcs, 0, n, a).invoke(Mmap2Stream, srcs, 1, n, b);
}

void Store(istream<float>& stream, mmap<float> mmap, uint64_t n) {
  task().invoke(Stream2Mmap, stream, mmap, 2, n);
}

void VecAdd(mmap<float> data, uint64_t n) {
  stream<float, 8> a("a");
  stream<float, 8> b("b");
  stream<float, 8> c("c");

  task()
      .invoke(Load, data, n, a, b)
      .invoke(Add, a, b, c)
      .invoke(Store, c, data, n);
}

void VecAddShared(mmap<float> elems, uint64_t n) {
  task().invoke(VecAdd, elems, n);
}
