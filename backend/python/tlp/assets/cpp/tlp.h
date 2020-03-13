#ifndef TASK_LEVEL_PARALLELIZATION_H_
#define TASK_LEVEL_PARALLELIZATION_H_

#include <climits>
#include <cstdint>

#ifndef __SYNTHESIS__

#include <thread>
#include <vector>

#include <glog/logging.h>

#define HLS_STREAM_THREAD_SAFE

#else  // __SYNTHESIS__

struct dummy {
  template <typename T>
  dummy& operator<<(const T&) {
    return *this;
  }
};

#define LOG(level) dummy()
#define LOG_IF(level, cond) dummy()
#define LOG_EVERY_N(level, n) dummy()
#define LOG_IF_EVERY_N(level, cond, n) dummy()
#define LOG_FIRST_N(level, n) dummy()

#define DLOG(level) dummy()
#define DLOG_IF(level, cond) dummy()
#define DLOG_EVERY_N(level, n) dummy()

#define CHECK(cond) \
  assert(cond);     \
  dummy()
#define CHECK_NE(lhs, rhs) \
  assert((lhs) != (rhs));  \
  dummy()
#define CHECK_EQ(lhs, rhs) \
  assert((lhs) != (rhs));  \
  dummy()
#define CHECK_GE(lhs, rhs) \
  assert((lhs) >= (rhs));  \
  dummy()
#define CHECK_GT(lhs, rhs) \
  assert((lhs) > (rhs));   \
  dummy()
#define CHECK_LE(lhs, rhs) \
  assert((lhs) <= (rhs));  \
  dummy()
#define CHECK_LT(lhs, rhs) \
  assert((lhs) < (rhs));   \
  dummy()
#define CHECK_NOTNULL(ptr) (ptr)
#define CHECK_STREQ(lhs, rhs) dummy()
#define CHECK_STRNE(lhs, rhs) dummy()
#define CHECK_STRCASEEQ(lhs, rhs) dummy()
#define CHECK_STRCASENE(lhs, rhs) dummy()
#define CHECK_DOUBLE_EQ(lhs, rhs) dummy()

#define VLOG_IS_ON(level) false
#define VLOG(level) dummy()
#define VLOG_IF(level, cond) dummy()
#define VLOG_EVERY_N(level, n) dummy()
#define VLOG_IF_EVERY_N(level, cond, n) dummy()
#define VLOG_FIRST_N(level, n) dummy()

#endif  // __SYNTHESIS__

#include <ap_int.h>
#include <ap_utils.h>
#include <hls_stream.h>

namespace tlp {

template <typename T>
T reg(T x) {
#pragma HLS inline self off
#pragma HLS interface ap_none register port = return
  return x;
}

template <typename T>
struct elem_t {
  T val;
  bool eos;
};

template <typename T>
class istream {
 public:
  bool empty() const {
#pragma HLS inline
#pragma HLS protocol
    return fifo.empty();
  }

  bool try_eos(bool& eos) const {
#pragma HLS inline
#pragma HLS protocol
    return empty() ? false : (eos = peek_val.eos, true);
  }

  bool eos(bool& succeeded) const {
#pragma HLS inline
#pragma HLS protocol
    return (succeeded = !empty()) && peek_val.eos;
  }

  bool eos(std::nullptr_t) const {
#pragma HLS inline
#pragma HLS protocol
    return !empty() && peek_val.eos;
  }

  bool try_peek(T& val) const {
#pragma HLS inline
#pragma HLS protocol
    return empty() ? false : (val = peek_val.val, true);
  }

  T peek(bool& succeeded) const {
#pragma HLS inline
#pragma HLS protocol
    elem_t<T> elem;
    (succeeded = !empty()) && (elem = peek_val, true);
    return elem.val;
  }

  T peek(std::nullptr_t) const {
#pragma HLS inline
#pragma HLS protocol
    return peek_val.val;
  }

  T peek(bool& succeeded, bool& is_eos) const {
#pragma HLS inline
#pragma HLS protocol
    succeeded = !empty();
    is_eos = peek_val.eos && succeeded;
    return peek_val.val;
  }

  bool try_read(T& val) {
#pragma HLS inline
#pragma HLS protocol
    elem_t<T> elem;
    return fifo.read_nb(elem) && (val = elem.val, true);
  }

  T read() {
#pragma HLS inline
#pragma HLS latency max = 1
    return fifo.read().val;
  }

  T read(bool& succeeded) {
#pragma HLS inline
#pragma HLS latency max = 1
    elem_t<T> elem;
    succeeded = fifo.read_nb(elem);
    return elem.val;
  }

  T read(std::nullptr_t) {
#pragma HLS inline
#pragma HLS latency max = 1
    elem_t<T> elem;
    fifo.read_nb(elem);
    return elem.val;
  }

  T read(bool* succeeded_ret, const T& default_val) {
#pragma HLS inline
#pragma HLS latency max = 1
    elem_t<T> elem;
    bool succeeded = fifo.read_nb(elem);
    if (succeeded_ret != nullptr) *succeeded_ret = succeeded;
    return succeeded ? elem.val : default_val;
  }

  void try_open() {
#pragma HLS inline
#pragma HLS latency max = 1
    elem_t<T> elem;
    assert(!fifo.read_nb(elem) || elem.eos);
  }

  void open() {
#pragma HLS inline
#pragma HLS latency max = 1
    assert(fifo.read().eos);
  }

  hls::stream<elem_t<T>> fifo;
  elem_t<T> peek_val;
};

template <typename T>
class ostream {
 public:
  bool full() const {
#pragma HLS inline
#pragma HLS protocol
    return fifo.full();
  }

  bool try_write(const T& val) {
#pragma HLS inline
#pragma HLS protocol
    return fifo.write_nb({val, false});
  }

  void write(const T& val) {
#pragma HLS inline
#pragma HLS protocol
    fifo.write({val, false});
  }

  bool try_close() {
#pragma HLS inline
#pragma HLS protocol
    elem_t<T> elem = {};
    elem.eos = true;
    return fifo.write_nb(elem);
  }

  void close() {
#pragma HLS inline
#pragma HLS protocol
    elem_t<T> elem = {};
    elem.eos = true;
    fifo.write(elem);
  }

  hls::stream<elem_t<T>> fifo;
};

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

template <typename T>
inline constexpr uint64_t widthof() {
  return sizeof(T) * CHAR_BIT;
}
template <typename T, uint64_t N>
struct vec_t {
  template <typename U>
  operator vec_t<U, N>() {
#pragma HLS inline
    vec_t<U, N> result;
    for (uint64_t i = 0; i < N; ++i) {
      result.set(i, static_cast<U>(get(i)));
    }
    return result;
  }
  static constexpr uint64_t length = N;
  static constexpr uint64_t bytes = length * sizeof(T);
  static constexpr uint64_t bits = bytes * CHAR_BIT;
  ap_uint<bits> data = 0;
  // T& operator[](uint64_t idx) { return *(reinterpret_cast<T*>(&data) + idx);
  // }
  void set(uint64_t idx, T val) {
#pragma HLS inline
    data.range((idx + 1) * widthof<T>() - 1, idx * widthof<T>()) =
        reinterpret_cast<ap_uint<widthof<T>()>&>(val);
  }
  T get(uint64_t idx) const {
#pragma HLS inline
    return reinterpret_cast<T&&>(static_cast<ap_uint<widthof<T>()>>(
        data.range((idx + 1) * widthof<T>() - 1, idx * widthof<T>())));
  }
  T operator[](uint64_t idx) const {
#pragma HLS inline
    return get(idx);
  }
};

}  // namespace tlp

#define TLP_WHILE_NOT_EOS(fifo)                               \
  for (bool tlp_##fifo##_valid;                               \
       !fifo.eos(tlp_##fifo##_valid) || !tlp_##fifo##_valid;) \
    if (tlp_##fifo##_valid)

#define TLP_WHILE_NEITHER_EOS(fifo1, fifo2)                         \
  for (bool tlp_##fifo1##_valid, tlp_##fifo2##_valid;               \
       (!fifo1.eos(tlp_##fifo1##_valid) || !tlp_##fifo1##_valid) && \
       (!fifo2.eos(tlp_##fifo2##_valid) || !tlp_##fifo2##_valid);)  \
    if (tlp_##fifo1##_valid && tlp_##fifo2##_valid)

#define TLP_WHILE_NONE_EOS(fifo1, fifo2, fifo3)                            \
  for (bool tlp_##fifo1##_valid, tlp_##fifo2##_valid, tlp_##fifo3##_valid; \
       (!fifo1.eos(tlp_##fifo1##_valid) || !tlp_##fifo1##_valid) &&        \
       (!fifo2.eos(tlp_##fifo2##_valid) || !tlp_##fifo2##_valid) &&        \
       (!fifo3.eos(tlp_##fifo3##_valid) || !tlp_##fifo3##_valid);)         \
    if (tlp_##fifo1##_valid && tlp_##fifo2##_valid && tlp_##fifo3##_valid)

#endif  // TASK_LEVEL_PARALLELIZATION_H_