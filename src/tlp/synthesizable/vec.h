#ifndef TLP_VEC_H_
#define TLP_VEC_H_

#include <climits>
#include <cstdint>
#include <cstring>

#include <algorithm>

#include "./util.h"

namespace tlp {

template <typename T, uint64_t N>
struct vec_t {
  // static constexpr metadata
  static constexpr uint64_t length = N;
  static constexpr uint64_t bytes = length * sizeof(T);
  static constexpr uint64_t bits = bytes * CHAR_BIT;

  // part that differs on the kernel and the host
#ifdef __SYNTHESIS__
  ap_uint<bits> data;
  vec_t() {}
  T get(uint64_t idx) const {
#pragma HLS inline
    return reinterpret_cast<T&&>(static_cast<ap_uint<widthof<T>()> >(
        data.range((idx + 1) * widthof<T>() - 1, idx * widthof<T>())));
  }
  void set(uint64_t idx, T val) {
#pragma HLS inline
    data.range((idx + 1) * widthof<T>() - 1, idx * widthof<T>()) =
        reinterpret_cast<ap_uint<widthof<T>()>&>(val);
  }
#else   // __SYNTHESIS__
  T data[N];
  vec_t() { memset(data, 0xcc, sizeof(data)); }
  T get(uint64_t idx) const { return data[idx]; }
  void set(uint64_t idx, T val) { data[idx] = val; }
#endif  // __SYNTHESIS__

  // constructors and operators
  vec_t(const vec_t&) = default;
  vec_t(vec_t&&) = default;
  vec_t& operator=(const vec_t&) = default;
  vec_t& operator=(vec_t&&) = default;
  template <typename U>
  operator vec_t<U, N>() {
#pragma HLS inline
    vec_t<U, N> result;
    for (uint64_t i = 0; i < N; ++i) {
      result.set(i, static_cast<U>(get(i)));
    }
    return result;
  }
  T operator[](uint64_t idx) const {
#pragma HLS inline
    return get(idx);
  }
  void set(T val) {
#pragma HLS inline
    for (uint64_t i = 0; i < N; ++i) {
#pragma HLS unroll
      set(i, val);
    }
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

#define DEFINE_FUNC(func)                                            \
  template <typename T, uint64_t N>                                  \
  vec_t<T, N> func(const vec_t<T, N>& lhs, const vec_t<T, N>& rhs) { \
    _Pragma("HLS inline");                                           \
    vec_t<T, N> result;                                              \
    for (uint64_t i = 0; i < N; ++i) {                               \
      _Pragma("HLS unroll");                                         \
      result.set(i, std::func(lhs[i], rhs[i]));                      \
    }                                                                \
    return result;                                                   \
  }                                                                  \
  template <typename T, uint64_t N>                                  \
  vec_t<T, N> func(const T& lhs, const vec_t<T, N>& rhs) {           \
    _Pragma("HLS inline") return func(make_vec<N>(lhs), rhs);        \
  }                                                                  \
  template <typename T, uint64_t N>                                  \
  vec_t<T, N> func(const vec_t<T, N>& lhs, const T& rhs) {           \
    _Pragma("HLS inline") return func(lhs, make_vec<N>(rhs));        \
  }
DEFINE_FUNC(max)
DEFINE_FUNC(min)
#undef DEFINE_FUNC

}  // namespace tlp

#endif  // TLP_VEC_H_
