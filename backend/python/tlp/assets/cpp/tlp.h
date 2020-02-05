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
struct data_t {
  T val;
  bool eos;
};

// tlp::stream<T>::try_eos(bool&)
template <typename T>
inline bool try_eos(hls::stream<data_t<T>>& fifo, data_t<T> peek_val,
                    bool& eos) {
#pragma HLS inline
#pragma HLS protocol
  return fifo.empty() ? false : (eos = peek_val.eos, true);
}

// tlp::stream<T>::eos(bool&)
template <typename T>
inline bool eos(hls::stream<data_t<T>>& fifo, data_t<T> peek_val, bool& valid) {
#pragma HLS inline
#pragma HLS protocol
  return (valid = !fifo.empty()) && peek_val.eos;
}

// tlp::stream<T>::eos(std::nullptr_t)
template <typename T>
inline bool eos(hls::stream<data_t<T>>& fifo, data_t<T> peek_val,
                std::nullptr_t) {
#pragma HLS inline
#pragma HLS protocol
  return !fifo.empty() && peek_val.eos;
}

// tlp::stream<T>::try_peek(T&)
template <typename T>
inline bool try_peek(hls::stream<data_t<T>>& fifo, data_t<T> peek_val, T& val) {
#pragma HLS inline
#pragma HLS protocol
  return fifo.empty() ? false : (val = peek_val.val, true);
}

// tlp::stream<T>::peek(bool&)
template <typename T>
inline T peek(hls::stream<T>& fifo, T peek_val, bool& succeeded) {
#pragma HLS inline
#pragma HLS protocol
  T tmp;
  (succeeded = !fifo.empty()) && (tmp = peek_val, true);
  return tmp;
}
template <typename T>
inline T peek(hls::stream<data_t<T>>& fifo, data_t<T> peek_val,
              bool& succeeded) {
#pragma HLS inline
#pragma HLS protocol
  data_t<T> tmp;
  (succeeded = !fifo.empty()) && (tmp = peek_val, true);
  return tmp.val;
}

// tlp::stream<T>::peek(std::nullptr_t)
template <typename T>
inline T peek(hls::stream<T>& fifo, T peek_val, std::nullptr_t) {
#pragma HLS inline
#pragma HLS protocol
  return peek_val;
}
template <typename T>
inline T peek(hls::stream<data_t<T>>& fifo, data_t<T> peek_val,
              std::nullptr_t) {
#pragma HLS inline
#pragma HLS protocol
  return peek_val.val;
}

// tlp::stream<T>::peek(bool&, bool&)
template <typename T>
inline T peek(hls::stream<data_t<T>>& fifo, data_t<T> peek_val, bool& succeeded,
              bool& is_eos) {
#pragma HLS inline
#pragma HLS protocol
  succeeded = !fifo.empty();
  is_eos = peek_val.eos && succeeded;
  return peek_val.val;
}

// tlp::stream<T>::try_read(T&)
template <typename T>
inline bool try_read(hls::stream<T>& fifo, T& value) {
#pragma HLS inline
#pragma HLS protocol
  return fifo.read_nb(value);
}
template <typename T>
inline bool try_read(hls::stream<data_t<T>>& fifo, T& value) {
#pragma HLS inline
#pragma HLS protocol
  data_t<T> tmp;
  return fifo.read_nb(tmp) && (value = tmp.val, true);
}

// tlp::stream<T>::read()
template <typename T>
inline T read(hls::stream<T>& fifo) {
#pragma HLS inline
#pragma HLS latency max = 1
  return fifo.read();
}
template <typename T>
inline T read(hls::stream<data_t<T>>& fifo) {
#pragma HLS inline
#pragma HLS latency max = 1
  return fifo.read().val;
}

// tlp::stream<T>::read(bool&)
template <typename T>
inline T read(hls::stream<T>& fifo, bool& succeeded) {
#pragma HLS inline
#pragma HLS latency max = 1
  T value;
  succeeded = fifo.read_nb(value);
  return value;
}
template <typename T>
inline T read(hls::stream<data_t<T>>& fifo, bool& succeeded) {
#pragma HLS inline
#pragma HLS latency max = 1
  data_t<T> value;
  succeeded = fifo.read_nb(value);
  return value.val;
}

// tlp::stream<T>::read(std::nullptr_t)
template <typename T>
inline T read(hls::stream<T>& fifo, std::nullptr_t) {
#pragma HLS inline
#pragma HLS latency max = 1
  T value;
  fifo.read_nb(value);
  return value;
}
template <typename T>
inline T read(hls::stream<data_t<T>>& fifo, std::nullptr_t) {
#pragma HLS inline
#pragma HLS latency max = 1
  data_t<T> value;
  fifo.read_nb(value);
  return value.val;
}

// tlp::stream<T>::try_open()
template <typename T>
inline void try_open(hls::stream<data_t<T>>& fifo) {
#pragma HLS inline
#pragma HLS latency max = 1
  data_t<T> tmp;
  assert(!fifo.read_nb(tmp) || tmp.eos);
}

// tlp::stream<T>::open()
template <typename T>
inline void open(hls::stream<data_t<T>>& fifo) {
#pragma HLS inline
#pragma HLS latency max = 1
  assert(fifo.read().eos);
}

// tlp::stream<T>::write(const T&)
template <typename T>
inline void write(hls::stream<data_t<T>>& fifo, const T& value) {
#pragma HLS inline
//#pragma HLS latency max = 1
#pragma HLS protocol
  fifo.write({value, false});
}

// tlp::stream<T>::try_write(const T&)
template <typename T>
inline bool try_write(hls::stream<data_t<T>>& fifo, const T& value) {
#pragma HLS inline
//#pragma HLS latency max = 1
#pragma HLS protocol
  return fifo.write_nb({value, false});
}

// tlp::stream<T>::close()
template <typename T>
inline void close(hls::stream<data_t<T>>& fifo) {
#pragma HLS inline
//#pragma HLS latency max = 1
#pragma HLS protocol
  data_t<T> tmp = {};
  tmp.eos = true;
  fifo.write(tmp);
}

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
  void set(uint64_t idx, const T& val) {
#pragma HLS inline
    data.range((idx + 1) * widthof<T>() - 1, idx * widthof<T>()) =
        reinterpret_cast<const ap_uint<widthof<T>()>&>(val);
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

#define TLP_WHILE_NOT_EOS(fifo)                                  \
  for (bool tlp_##fifo##_valid;                                  \
       !tlp::eos(fifo, tlp_##fifo##_peek, tlp_##fifo##_valid) || \
       !tlp_##fifo##_valid;)                                     \
    if (tlp_##fifo##_valid)

#define TLP_WHILE_NEITHER_EOS(fifo1, fifo2)                          \
  for (bool tlp_##fifo1##_valid, tlp_##fifo2##_valid;                \
       (!tlp::eos(fifo1, tlp_##fifo1##_peek, tlp_##fifo1##_valid) || \
        !tlp_##fifo1##_valid) &&                                     \
       (!tlp::eos(fifo2, tlp_##fifo2##_peek, tlp_##fifo2##_valid) || \
        !tlp_##fifo2##_valid);)                                      \
    if (tlp_##fifo1##_valid && tlp_##fifo2##_valid)

#define TLP_WHILE_NONE_EOS(fifo1, fifo2, fifo3)                            \
  for (bool tlp_##fifo1##_valid, tlp_##fifo2##_valid, tlp_##fifo3##_valid; \
       (!tlp::eos(fifo1, tlp_##fifo1##_peek, tlp_##fifo1##_valid) ||       \
        !tlp_##fifo1##_valid) &&                                           \
       (!tlp::eos(fifo2, tlp_##fifo2##_peek, tlp_##fifo2##_valid) ||       \
        !tlp_##fifo2##_valid) &&                                           \
       (!tlp::eos(fifo3, tlp_##fifo3##_peek, tlp_##fifo3##_valid) ||       \
        !tlp_##fifo3##_valid);)                                            \
    if (tlp_##fifo1##_valid && tlp_##fifo2##_valid && tlp_##fifo3##_valid)

#endif  // TASK_LEVEL_PARALLELIZATION_H_