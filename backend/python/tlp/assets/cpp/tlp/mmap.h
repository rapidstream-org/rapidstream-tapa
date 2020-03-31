#ifndef TLP_MMAP_H_
#define TLP_MMAP_H_

#include <cstdint>

#include <ap_utils.h>
#include <hls_stream.h>

namespace tlp {

template <typename T>
using mmap = T*;

template <typename T>
struct async_mmap {
  using addr_t = uint64_t;

  // Read operations.
  void read_addr_write(addr_t addr) {
#pragma HLS inline
#pragma HLS protocol
    read_addr.write(addr);
  }
  bool read_addr_try_write(addr_t addr) {
#pragma HLS inline
#pragma HLS protocol
    return read_addr.write_nb(addr);
  }
  bool read_data_empty() {
#pragma HLS inline
#pragma HLS protocol
    return read_data.empty();
  }
  bool read_data_try_read(T& resp) {
#pragma HLS inline
#pragma HLS latency max = 1
    return read_data.read_nb(resp);
  };

  // Write operations.
  void write_addr_write(addr_t addr) {
#pragma HLS inline
#pragma HLS protocol
    write_addr.write(addr);
  }
  bool write_addr_try_write(addr_t addr) {
#pragma HLS inline
#pragma HLS protocol
    return write_addr.write_nb(addr);
  }
  void write_data_write(const T& data) {
#pragma HLS inline
#pragma HLS protocol
    write_data.write(data);
  }
  bool write_data_try_write(const T& data) {
#pragma HLS inline
#pragma HLS protocol
    return write_data.write_nb(data);
  }

  // Waits until no operations are pending or on-going.
  void fence() {
#pragma HLS inline
    ap_wait_n(80);
  }

  hls::stream<addr_t> read_addr;  // write-only
  hls::stream<T> read_data;       // read-only
  T read_peek;
  hls::stream<addr_t> write_addr;  // write-only
  hls::stream<T> write_data;       // write-only
};

}  // namespace tlp

#endif  // TLP_MMAP_H_