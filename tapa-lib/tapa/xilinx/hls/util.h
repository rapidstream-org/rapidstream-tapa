// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#ifndef TAPA_XILINX_HLS_UTIL_H_
#define TAPA_XILINX_HLS_UTIL_H_

#include "tapa/base/util.h"

#include <cstring>

namespace tapa {

template <typename To, typename From>
inline typename std::enable_if<sizeof(To) == sizeof(From), To>::type  //
bit_cast(From from) noexcept {
#pragma HLS inline
  To to;
  std::memcpy(&to, &from, sizeof(To));
  return to;
}

template <typename T>
T reg(T x) {
#pragma HLS inline off
#pragma HLS pipeline II = 1
#pragma HLS latency min = 1 max = 1
  return x;
}

}  // namespace tapa

#endif  // TAPA_XILINX_HLS_UTIL_H_
