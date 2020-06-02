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

#include "tlp/mmap.h"
#include "tlp/stream.h"
#include "tlp/synthesizable/traits.h"
#include "tlp/synthesizable/util.h"

namespace tlp {

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
  ap_uint<bits> data;
  // T& operator[](uint64_t idx) { return *(reinterpret_cast<T*>(&data) + idx);
  // }
  void set(uint64_t idx, T val) {
#pragma HLS inline
    data.range((idx + 1) * widthof<T>() - 1, idx * widthof<T>()) =
        reinterpret_cast<ap_uint<widthof<T>()>&>(val);
  }
  void set(T val) {
#pragma HLS inline
    for (uint64_t i = 0; i < N; ++i) {
#pragma HLS unroll
      set(i, val);
    }
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

// assignment operators
#define DEFINE_OP(op)                                    \
  template <typename T2>                                 \
  vec_t<T, N>& operator op##=(const vec_t<T2, N>& rhs) { \
    _Pragma("HLS inline");                               \
    for (uint64_t i = 0; i < N; ++i) {                   \
      _Pragma("HLS unroll");                             \
      set(i, get(i) op rhs[i]);                          \
    }                                                    \
    return *this;                                        \
  }                                                      \
  template <typename T2>                                 \
  vec_t<T, N>& operator op##=(const T2& rhs) {           \
    _Pragma("HLS inline");                               \
    for (uint64_t i = 0; i < N; ++i) {                   \
      _Pragma("HLS unroll");                             \
      set(i, get(i) op rhs);                             \
    }                                                    \
    return *this;                                        \
  }
  DEFINE_OP(+)
  DEFINE_OP(-)
  DEFINE_OP(*)
  DEFINE_OP(/)
  DEFINE_OP(%)
  DEFINE_OP(&)
  DEFINE_OP(|)
  DEFINE_OP(^)
  DEFINE_OP(<<)
  DEFINE_OP(>>)
#undef DEFINE_OP

// unary arithemetic operators
#define DEFINE_OP(op)                  \
  vec_t<T, N> operator op() {          \
    _Pragma("HLS inline");             \
    for (uint64_t i = 0; i < N; ++i) { \
      _Pragma("HLS unroll");           \
      set(i, op get(i));               \
    }                                  \
    return *this;                      \
  }
  DEFINE_OP(+)
  DEFINE_OP(-)
  DEFINE_OP(~)
#undef DEFINE_OP

// binary arithemetic operators
#define DEFINE_OP(op)                                \
  template <typename T2>                             \
  vec_t<T, N> operator op(const vec_t<T2, N>& rhs) { \
    _Pragma("HLS inline");                           \
    vec_t<T, N> result;                              \
    for (uint64_t i = 0; i < N; ++i) {               \
      _Pragma("HLS unroll");                         \
      result.set(i, get(i) op rhs[i]);               \
    }                                                \
    return result;                                   \
  }                                                  \
  template <typename T2>                             \
  vec_t<T, N> operator op(const T2& rhs) {           \
    _Pragma("HLS inline");                           \
    vec_t<T, N> result;                              \
    for (uint64_t i = 0; i < N; ++i) {               \
      _Pragma("HLS unroll");                         \
      result.set(i, get(i) op rhs);                  \
    }                                                \
    return result;                                   \
  }
  DEFINE_OP(+)
  DEFINE_OP(-)
  DEFINE_OP(*)
  DEFINE_OP(/)
  DEFINE_OP(%)
  DEFINE_OP(&)
  DEFINE_OP(|)
  DEFINE_OP(^)
  DEFINE_OP(<<)
  DEFINE_OP(>>)
#undef DEFINE_OP
};

// binary arithemetic operators, vector on the right-hand side
#define DEFINE_OP(op)                                              \
  template <typename T, uint64_t N, typename T2>                   \
  vec_t<T, N> operator op(const T2& lhs, const vec_t<T, N>& rhs) { \
    _Pragma("HLS inline");                                         \
    vec_t<T, N> result;                                            \
    for (uint64_t i = 0; i < N; ++i) {                             \
      _Pragma("HLS unroll");                                       \
      result.set(i, lhs op rhs[i]);                                \
    }                                                              \
    return result;                                                 \
  }
DEFINE_OP(+)
DEFINE_OP(-)
DEFINE_OP(*)
DEFINE_OP(/)
DEFINE_OP(%)
DEFINE_OP(&)
DEFINE_OP(|)
DEFINE_OP(^)
DEFINE_OP(<<)
DEFINE_OP(>>)
#undef DEFINE_OP

template <uint64_t N, typename T>
vec_t<T, N> make_vec(T val) {
#pragma HLS inline
  vec_t<T, N> result;
  result.set(val);
  return result;
}

}  // namespace tlp

#endif  // TASK_LEVEL_PARALLELIZATION_H_
