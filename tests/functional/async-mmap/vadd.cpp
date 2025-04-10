// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include <cstdint>

#include <tapa.h>

template <typename T, typename R>
inline void async_read(tapa::async_mmap<T>& mem, tapa::ostream<T>& strm, R n) {
  for (R i_req = 0, i_resp = 0; i_resp < n;) {
#pragma HLS loop_tripcount min = 1 max = 800
#pragma HLS pipeline II = 1
    if ((i_req < n) && !mem.read_addr.full()) {
      mem.read_addr.try_write(i_req);
      ++i_req;
    }
    if (!strm.full() && !mem.read_data.empty()) {
      T tmp;
      mem.read_data.try_read(tmp);
      strm.try_write(tmp);
      ++i_resp;
    }
  }
}

template <typename T, typename R>
inline void async_write(tapa::async_mmap<T>& mem, tapa::istream<T>& strm, R n) {
  for (R i_req = 0, i_resp = 0; i_resp < n;) {
#pragma HLS loop_tripcount min = 1 max = 800
#pragma HLS pipeline II = 1
    if ((i_req < n) && !strm.empty() && !mem.write_addr.full() &&
        !mem.write_data.full()) {
      mem.write_addr.try_write(i_req);
      T tmp;
      strm.try_read(tmp);
      mem.write_data.try_write(tmp);
      ++i_req;
    }
    uint8_t n_resp;
    if (mem.write_resp.try_read(n_resp)) {
      i_resp += int(n_resp) + 1;
    }
  }
}

void Add(tapa::istream<float>& a, tapa::istream<float>& b,
         tapa::ostream<float>& c, uint64_t n) {
  for (uint64_t i = 0; i < n; ++i) {
    c << (a.read() + b.read());
  }
}

void Mmap2Stream(tapa::async_mmap<float>& mmap, uint64_t n,
                 tapa::ostream<float>& stream) {
  async_read(mmap, stream, n);
}

void Stream2Mmap(tapa::istream<float>& stream, tapa::async_mmap<float>& mmap,
                 uint64_t n) {
  async_write(mmap, stream, n);
}

void VecAdd(tapa::mmap<float> a, tapa::mmap<float> b, tapa::mmap<float> c,
            uint64_t n) {
  tapa::stream<float> a_q("a");
  tapa::stream<float> b_q("b");
  tapa::stream<float> c_q("c");

  tapa::task()
      .invoke(Mmap2Stream, a, n, a_q)
      .invoke(Mmap2Stream, b, n, b_q)
      .invoke(Add, a_q, b_q, c_q, n)
      .invoke(Stream2Mmap, c_q, c, n);
}
