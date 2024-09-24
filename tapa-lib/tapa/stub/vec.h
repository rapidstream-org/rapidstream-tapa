// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#pragma once

#include <climits>
#include <cmath>
#include <cstdint>
#include <cstring>

#include <array>

#include "tapa/stub/util.h"

namespace tapa {

template <typename T, int N>
struct vec_t : protected std::array<T, N> {
 private:
  using base_type = std::array<T, N>;

 public:
  static constexpr int length = N;
  static constexpr int width = widthof<T>() * N;

  using size_type = int;
  using typename base_type::const_iterator;
  using typename base_type::const_pointer;
  using typename base_type::const_reference;
  using typename base_type::const_reverse_iterator;
  using typename base_type::difference_type;
  using typename base_type::iterator;
  using typename base_type::pointer;
  using typename base_type::reference;
  using typename base_type::reverse_iterator;
  using typename base_type::value_type;

  constexpr const_reference operator[](size_type pos) const;
  reference operator[](size_type pos);
  constexpr const_reference get(size_type pos) const;
  void set(size_type pos, const T& value);

  using base_type::base_type;
  using base_type::operator=;

  explicit vec_t(const base_type& other);
  explicit vec_t(base_type&& other);

  template <typename U>
  explicit operator vec_t<U, N>() const;

  void set(T val);
  vec_t& operator=(T val);

#define DEFINE_OP(op)                                   \
  template <typename T2>                                \
  vec_t<T, N>& operator op##=(const vec_t<T2, N>& rhs); \
  template <typename T2>                                \
  vec_t<T, N>& operator op##=(const T2 & rhs);
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
#define DEFINE_OP(op) vec_t<T, N> operator op();
  DEFINE_OP(+)
  DEFINE_OP(-)
  DEFINE_OP(~)
#undef DEFINE_OP

// binary arithemetic operators
#define DEFINE_OP(op)                               \
  template <typename T2>                            \
  vec_t<T, N> operator op(const vec_t<T2, N>& rhs); \
  template <typename T2>                            \
  vec_t<T, N> operator op(const T2 & rhs);
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

  void shift(const T& val);
  bool has(const T& val);
};

template <int begin, int end, typename T, int N>
vec_t<T, end - begin> truncated(const vec_t<T, N>& vec);

template <int length, typename T, int N>
vec_t<T, length> truncated(const vec_t<T, N>& vec);

template <int length, typename T, int N>
vec_t<T, length> truncated(const vec_t<T, N>& vec, int begin);

template <typename T, int N>
vec_t<T, N + 1> cat(const vec_t<T, N>& vec, const T& val);

template <typename T, int N>
vec_t<T, N + 1> cat(const T& val, const vec_t<T, N>& vec);

template <typename T, int N1, int N2>
vec_t<T, N1 + N2> cat(const vec_t<T, N1>& v1, const vec_t<T, N2>& v2);

#if __cplusplus >= 201402L
template <typename T, typename... Args>
auto cat(T arg, Args... args);
#endif  // __cplusplus >= 201402L

// binary arithemetic operators, vector on the right-hand side
#define DEFINE_OP(op)                       \
  template <typename T, int N, typename T2> \
  vec_t<T, N> operator op(const T2 & lhs, const vec_t<T, N>& rhs);
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

template <int N, typename T>
vec_t<T, N> make_vec(T val);

// unary operation functions
#define DEFINE_FUNC(func)      \
  template <typename T, int N> \
  vec_t<T, N> func(vec_t<T, N> vec);
DEFINE_FUNC(exp2)
DEFINE_FUNC(expm1)
DEFINE_FUNC(log)
DEFINE_FUNC(log10)
DEFINE_FUNC(log1p)
DEFINE_FUNC(log2)
#undef DEFINE_FUNC

// binary operation functions
#define DEFINE_FUNC(func)                                           \
  template <typename T, int N>                                      \
  vec_t<T, N> func(const vec_t<T, N>& lhs, const vec_t<T, N>& rhs); \
  template <typename T, int N>                                      \
  vec_t<T, N> func(const T& lhs, const vec_t<T, N>& rhs);           \
  template <typename T, int N>                                      \
  vec_t<T, N> func(const vec_t<T, N>& lhs, const T& rhs);
DEFINE_FUNC(max)
DEFINE_FUNC(min)
#undef DEFINE_FUNC

// reduction operation functions
#define DEFINE_FUNC(func, op)     \
  template <typename T>           \
  T func(const vec_t<T, 1>& vec); \
  template <typename T, int N>    \
  T func(const vec_t<T, N>& vec);
DEFINE_FUNC(sum, +)
DEFINE_FUNC(product, *)
#undef DEFINE_FUNC

template <typename T, int N>
std::ostream& operator<<(std::ostream& os, const vec_t<T, N>& obj);

}  // namespace tapa
