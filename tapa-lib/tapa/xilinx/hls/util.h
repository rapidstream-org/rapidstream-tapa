// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#ifndef TAPA_XILINX_HLS_UTIL_H_
#define TAPA_XILINX_HLS_UTIL_H_

#include "tapa/base/util.h"

#if __has_include("ap_utils.h")
#include "ap_utils.h"
#else
#include "etc/ap_utils.h"
#endif

#include "ap_int.h"

#include <cstring>
#include <type_traits>

namespace tapa {

// ap_data_bits is a utility function to get the bit width of an ap_int or
// ap_uint type.
namespace internal {
template <typename T>
inline constexpr int ap_data_bits(T) {
  return 0;
}
template <int width>
inline constexpr int ap_data_bits(const ap_int<width>&&) {
  return width;
}
template <int width>
inline constexpr int ap_data_bits(const ap_uint<width>&&) {
  return width;
}
}  // namespace internal

template <typename To, typename From>
inline typename std::enable_if<sizeof(To) == sizeof(From), To>::type  //
bit_cast(From from) noexcept {
#pragma HLS inline
  To to;
  std::memcpy(&to, &from, sizeof(To));
  return to;
}

template <size_t Depth>
struct DepthTag {};

template <typename T, size_t Depth>
T __attribute__((noinline)) reg_impl(T x, DepthTag<Depth>) {
#pragma HLS inline off
#pragma HLS pipeline II = 1
  volatile T data = reg_impl<T>(x, DepthTag<Depth - 1>{});
  {
#pragma HLS protocol float
    ap_wait();
    return *((T*)&data);
  }
}

template <typename T>
inline T reg_impl(T x, DepthTag<0>) {
#pragma HLS inline
  return x;
}

template <typename T, size_t Depth = 1>
inline T reg(T x) {
#pragma HLS inline
  return reg_impl(x, DepthTag<Depth>{});
}

}  // namespace tapa

#endif  // TAPA_XILINX_HLS_UTIL_H_
