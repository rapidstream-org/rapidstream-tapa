// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include <tapa.h>
#include <cassert>
#include <cstdint>
#include <iostream>

void Add(tapa::istream<float>& a, tapa::istream<float>& b,
         tapa::ostream<float>& c, uint64_t n) {
  bool eot_a;
  bool eot_b;
  bool valid_a, valid_b;
  while (1) {
    if (a.empty() || b.empty() || c.full()) {
      continue;
    }
    a.peek(valid_a, eot_a);
    b.peek(valid_b, eot_b);
    if (eot_a && eot_b) {
      break;
    }
    assert(!eot_a && !eot_b);
    assert(valid_a && valid_b);
    c << (a.read() + b.read());
  }
  c.close();
}

void Mmap2Stream(tapa::mmap<const float> mmap, uint64_t n,
                 tapa::ostream<float>& stream) {
  for (uint64_t i = 0; i < n; ++i) {
    stream << mmap[i];
    std::cout << i << std::endl;
  }
  stream.close();
}

void Stream2Mmap(tapa::istream<float>& stream, tapa::mmap<float> mmap,
                 uint64_t n) {
  int i = 0;
  bool eot;
  while (1) {
    if (stream.try_eot(eot)) {
      if (eot) {
        break;
      }
      mmap[i] = stream.read();
      i++;
    }
  }
  assert(i == n);
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
