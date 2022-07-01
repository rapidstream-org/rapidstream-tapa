#ifndef TAPA_BUFFER_H_
#define TAPA_BUFFER_H_

#include <hls_stream.h>

namespace tapa {

using default_addr_t = int;

template <typename T, int elem_count, int part_count, typename addr_t>
class buffer;

template <typename T, int elem_count, int part_count,
          typename addr_t = default_addr_t>
class partition {
 public:
  T& operator[](addr_t i) {
#pragma HLS inline
    T* ptr = &data.data[partition_id][i];
    last = ptr;
    return *ptr;
  }

  const T& operator[](addr_t i) const {
#pragma HLS inline
    last = &data.data[partition_id][i];
    return *last;
  }

  ~partition() {
#pragma HLS inline
    data.sink.write({partition_id, *last});
  }

 private:
  using buffer_t = buffer<T, elem_count, part_count, addr_t>;
  friend buffer_t;

  partition(buffer_t& data) : data(data) {
#pragma HLS inline
    partition_id = data.src.read();
  }

  buffer_t& data;
  int partition_id;
  mutable const T* last = nullptr;
};

template <typename T, int elem_count, int part_count,
          typename addr_t = default_addr_t>
class buffer {
 public:
  struct sink_elem_t {
    int addr;
    T val;
  };

  partition<T, elem_count, part_count, addr_t> acquire() {
#pragma HLS inline
    return *this;
  }

  hls::stream<int> src;
  hls::stream<sink_elem_t> sink;
  T data[part_count][elem_count];
};

}  // namespace tapa

#endif  // TAPA_BUFFER_H_
