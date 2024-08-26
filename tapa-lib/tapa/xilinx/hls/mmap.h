// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#ifndef TAPA_XILINX_HLS_MMAP_H_
#define TAPA_XILINX_HLS_MMAP_H_

#include "tapa/base/mmap.h"

#ifdef __SYNTHESIS__

#include <cstddef>

#include "tapa/xilinx/hls/stream.h"

namespace tapa {
namespace internal {

template <typename addr_t, typename T>
class addr_ostream {
 public:
  bool full() const {
#pragma HLS inline
    return _.full();
  }

  bool try_write(const addr_t& value) {
#pragma HLS inline
    return _.write_nb(offset + value * sizeof(T));
  }

  void write(const addr_t& value) {
#pragma HLS inline
    _.write(offset + value * sizeof(T));
  }

  addr_ostream& operator<<(const addr_t& value) {
#pragma HLS inline
    write(value);
    return *this;
  }

  bool try_close() {}

  void close() {}

  hls::stream<addr_t> _;
  const addr_t offset;
};

}  // namespace internal

template <typename T>
using mmap = T*;

template <typename T>
struct async_mmap {
  using addr_t = int64_t;
  using resp_t = uint8_t;

  tapa::internal::addr_ostream<addr_t, T> read_addr;
  tapa::istream<T> read_data;
  tapa::internal::addr_ostream<addr_t, T> write_addr;
  tapa::ostream<T> write_data;
  tapa::istream<resp_t> write_resp;
};

template <typename T, uint64_t S>
class mmaps;

template <typename T, int chan_count, int64_t chan_size>
class hmap;

}  // namespace tapa

#else  // __SYNTHESIS__

#include "tapa/host/mmap.h"

#endif  // __SYNTHESIS__

#endif  // TAPA_XILINX_HLS_MMAP_H_
