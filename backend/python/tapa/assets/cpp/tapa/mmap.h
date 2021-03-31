#ifndef TAPA_MMAP_H_
#define TAPA_MMAP_H_

#include <cstdint>

#include <tapa/stream.h>

namespace tapa {

template <typename T>
using mmap = T*;

namespace internal {

template <typename T>
struct async_mmap {
  using addr_t = int64_t;
  using resp_t = uint8_t;

  tapa::ostream<addr_t> read_addr;
  tapa::istream<T> read_data;
  tapa::ostream<addr_t> write_addr;
  tapa::ostream<T> write_data;
  tapa::istream<resp_t> write_resp;
};

}  // namespace internal

// HLS doesn't like non-reference instance of hls::stream in the arguments.
template <typename T>
using async_mmap = internal::async_mmap<T>&;

}  // namespace tapa

#endif  // TAPA_MMAP_H_
