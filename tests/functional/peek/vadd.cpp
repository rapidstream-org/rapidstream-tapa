// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include <tapa.h>
#include <cassert>
#include <cstdint>

void Add(tapa::istream<float>& a, tapa::istream<float>& b,
         tapa::ostream<float>& c, uint64_t n) {
  for (uint64_t i = 0; i < n;) {
    if (a.empty() || b.empty() || c.full()) {
      continue;
    }
    float tmp_a, tmp_b, tmp_peek_a, tmp_peek_b;
    bool valid_a, valid_b;
    tmp_peek_a = a.peek(valid_a);
    tmp_peek_b = b.peek(valid_b);
    assert(valid_a && valid_b);

    a.try_read(tmp_a);
    b.try_read(tmp_b);
    assert(tmp_a == tmp_peek_a);
    assert(tmp_b == tmp_peek_b);
    (void)tmp_peek_a;  // suppress -Wunused-but-set-variable when -DNDEBUG
    (void)tmp_peek_b;  // suppress -Wunused-but-set-variable when -DNDEBUG

    c.try_write(tmp_a + tmp_b);
    ++i;
  }
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
      .invoke(Add, a_q, b_q, c_q, n)
      .invoke(Stream2Mmap, c_q, c, n);
}
