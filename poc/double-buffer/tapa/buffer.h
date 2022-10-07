#ifndef TAPA_BUFFER_H_
#define TAPA_BUFFER_H_

#include <hls_stream.h>

namespace tapa {

using default_addr_t = int;

template <typename T, int part_count, typename addr_t>
class buffer;

template <typename T, int part_count, typename addr_t = default_addr_t>
class partition {
 public:
  operator T&() {
#pragma HLS inline
    last = true;
    return data.data[partition_id];
  }

  operator const T&() const {
#pragma HLS inline
    last = true;
    return data.data[partition_id];
  }

  ~partition() {
#pragma HLS inline
    if (last) {
      data.sink.write(partition_id);
    }
  }

 private:
  using buffer_t = buffer<T, part_count, addr_t>;
  friend buffer_t;

  partition(buffer_t& data) : data(data) {
#pragma HLS inline
    partition_id = data.src.read();
  }

  buffer_t& data;
  int partition_id;
  mutable volatile bool last = true;
};

template <typename T, int part_count, typename addr_t = default_addr_t>
class buffer {
 public:
  partition<T, part_count, addr_t> acquire() {
#pragma HLS inline
    return *this;
  }

  hls::stream<addr_t> src;
  hls::stream<addr_t> sink;
  T data[part_count];
};

}  // namespace tapa

#endif  // TAPA_BUFFER_H_
