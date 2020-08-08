#ifndef TAPA_VEC_H_
#define TAPA_VEC_H_

#include <climits>
#include <cmath>
#include <cstdint>
#include <cstring>

#include <algorithm>
#include <functional>

#include "./util.h"

namespace tapa {

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
    return reinterpret_cast<T&&>(static_cast<ap_uint<widthof<T>()>>(
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

  // shift all elements by 1, put val at [N-1], and through away [0]
  void shift(const T& val) {
#pragma inline
    for (int i = 1; i < N; ++i) {
#pragma HLS unroll
      set(i - 1, get(i));
    }
    set(N - 1, val);
  }

  // return true if and only if val exists
  bool has(const T& val) {
#pragma inline
    bool result = false;
    for (int i = 0; i < N; ++i) {
#pragma HLS unroll
      if (val == get(i)) result |= true;
    }
    return result;
  }
};

// return vec[begin:end]
template <uint64_t begin, uint64_t end, typename T, uint64_t N>
inline vec_t<T, end - begin> truncated(const vec_t<T, N>& vec) {
  static_assert(begin >= 0, "cannot truncate before 0");
  static_assert(end <= N, "cannot truncate after N");
  vec_t<T, end - begin> result;
#pragma HLS inline
  for (uint64_t i = 0; i < end - begin; ++i) {
#pragma HLS unroll
    result.set(i, vec[begin + i]);
  }
  return result;
}

// return vec[:length]
template <uint64_t length, typename T, uint64_t N>
inline vec_t<T, length> truncated(const vec_t<T, N>& vec) {
#pragma HLS inline
  return truncated<0, length>(vec);
}

// return vec[:] + [val]
template <typename T, uint64_t N>
inline vec_t<T, N + 1> cat(const vec_t<T, N>& vec, const T& val) {
  vec_t<T, N + 1> result;
#pragma HLS inline
  for (uint64_t i = 0; i < N; ++i) {
#pragma HLS unroll
    result.set(i, vec[i]);
  }
  result.set(N, val);
  return result;
}

// return [val] + vec[:]
template <typename T, uint64_t N>
inline vec_t<T, N + 1> cat(const T& val, const vec_t<T, N>& vec) {
  vec_t<T, N + 1> result;
#pragma HLS inline
  result.set(0, val);
  for (uint64_t i = 0; i < N; ++i) {
#pragma HLS unroll
    result.set(i + 1, vec[i]);
  }
  return result;
}

// return v1[:] + v2[:]
template <typename T, uint64_t N1, uint64_t N2>
inline vec_t<T, N1 + N2> cat(const vec_t<T, N1>& v1, const vec_t<T, N2>& v2) {
  vec_t<T, N1 + N2> result;
#pragma HLS inline
  for (uint64_t i = 0; i < N1; ++i) {
#pragma HLS unroll
    result.set(i, v1[i]);
  }
  for (uint64_t i = 0; i < N2; ++i) {
#pragma HLS unroll
    result.set(i + N1, v2[i]);
  }
  return result;
}

#if __cplusplus >= 201402L
template <typename T, typename... Args>
inline auto cat(T arg, Args... args) {
#pragma HLS inline
  return cat(arg, cat(args...));
}
#endif  // __cplusplus >= 201402L

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

// unary operation functions
#define DEFINE_FUNC(func)              \
  template <typename T, uint64_t N>    \
  vec_t<T, N> func(vec_t<T, N> vec) {  \
    _Pragma("HLS inline");             \
    for (uint64_t i = 0; i < N; ++i) { \
      _Pragma("HLS unroll");           \
      vec.set(i, std::func(vec[i]));   \
    }                                  \
    return vec;                        \
  }
DEFINE_FUNC(exp)
DEFINE_FUNC(exp2)
DEFINE_FUNC(expm1)
DEFINE_FUNC(log)
DEFINE_FUNC(log10)
DEFINE_FUNC(log1p)
DEFINE_FUNC(log2)
#undef DEFINE_FUNC

// binary operation functions
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

// reduction operation functions
#define DEFINE_FUNC(func, op)                                             \
  template <typename T>                                                   \
  T func(const vec_t<T, 1>& vec) {                                        \
    _Pragma("HLS inline");                                                \
    return vec[0];                                                        \
  }                                                                       \
  template <typename T, uint64_t N>                                       \
  T func(const vec_t<T, N>& vec) {                                        \
    _Pragma("HLS inline");                                                \
    return func(truncated<N / 2>(vec)) op func(truncated<N / 2, N>(vec)); \
  }
DEFINE_FUNC(sum, +)
DEFINE_FUNC(product, *)
#undef DEFINE_FUNC

}  // namespace tapa

#endif  // TAPA_VEC_H_
