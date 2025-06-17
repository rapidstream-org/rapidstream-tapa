
// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#ifndef FPGA_RUNTIME_STRINGIFY_H_
#define FPGA_RUNTIME_STRINGIFY_H_

// IWYU pragma: private, include "frt.h"

#include <cstring>

#include <string>
#include <string_view>

#include <glog/logging.h>

namespace fpga {
namespace internal {

template <typename T>
std::string ToBinaryString(const T& val) {
  std::string bytes(sizeof(val), '\0');
  memcpy(bytes.data(), &val, sizeof(val));
  return bytes;
}

template <typename T>
T FromBinaryString(std::string_view bytes) {
  T val;
  CHECK_EQ(bytes.size(), sizeof(val)) << bytes;
  memcpy(&val, bytes.data(), sizeof(val));
  return val;
}

}  // namespace internal
}  // namespace fpga

#endif  // FPGA_RUNTIME_STRINGIFY_H_
