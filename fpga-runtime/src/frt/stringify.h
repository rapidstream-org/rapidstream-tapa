
// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#ifndef FPGA_RUNTIME_STRINGIFY_H_
#define FPGA_RUNTIME_STRINGIFY_H_

// IWYU pragma: private, include "frt.h"

#include <climits>
#include <cstring>

#include <algorithm>
#include <array>
#include <bitset>
#include <string>
#include <string_view>

#include <glog/logging.h>

std::string ToBinaryStringImpl(const bool* val);
void FromBinaryStringImpl(std::string_view str, bool& val);

namespace fpga {
namespace internal {

bool IsLittleEndian();  // TODO: remove when std::endian is available.

}  // namespace internal

// `HasToBinaryString<T>::value` is `true` if and only if an overload of
// `ToBinaryStringImpl<T>` is defined.
template <typename T, typename = void>
struct HasToBinaryString : std::false_type {};
template <typename T>
struct HasToBinaryString<
    T,
    std::enable_if_t<std::is_same_v<
        std::string, decltype(ToBinaryStringImpl(std::declval<const T*>()))>>>
    : std::true_type {};

// `HasFromBinaryString<T>::value` is `true` if and only if an overload of
// `FromBinaryStringImpl<T>` is defined.
template <typename T, typename = void>
struct HasFromBinaryString : std::false_type {};
template <typename T>
struct HasFromBinaryString<
    T, std::enable_if_t<std::is_void_v<decltype(FromBinaryStringImpl(
           std::declval<std::string_view>(), std::declval<T&>()))>>>
    : std::true_type {};

// `ToBinaryStringImpl<T>` is used if it is defined.
template <typename T,
          typename std::enable_if_t<HasToBinaryString<T>::value, int> = 0>
std::string ToBinaryString(const T& val) {
  return ToBinaryStringImpl(&val);
}

// Default implementation of if `ToBinaryStringImpl<T>` is not defined.
template <typename T,
          typename std::enable_if_t<!HasToBinaryString<T>::value, int> = 0>
std::string ToBinaryString(const T& val) {
  std::array<char, sizeof(val)> bytes;
  memcpy(bytes.data(), &val, sizeof(val));
  if (internal::IsLittleEndian()) {
    std::reverse(bytes.begin(), bytes.end());
  }

  const int expected_size = sizeof(val) * CHAR_BIT;
  std::string str;
  str.reserve(expected_size);
  for (std::bitset<CHAR_BIT> bits : bytes) {
    str += bits.to_string();
  }
  CHECK_EQ(str.size(), expected_size) << str;
  return str;
}

// `FromBinaryStringImpl<T>` is used if it is defined.
template <typename T,
          typename std::enable_if_t<HasFromBinaryString<T>::value, int> = 0>
T FromBinaryString(std::string_view str) {
  T val;
  FromBinaryStringImpl(str, val);
  return val;
}

// Default implementation of if `FromBinaryStringImpl<T>` is not defined.
template <typename T,
          typename std::enable_if_t<!HasFromBinaryString<T>::value, int> = 0>
T FromBinaryString(std::string_view str) {
  T val;
  std::array<char, sizeof(val)> bytes;
  CHECK_EQ(str.size(), bytes.size() * CHAR_BIT) << str;
  for (char& byte : bytes) {
    std::bitset<CHAR_BIT> bits(str.data(), CHAR_BIT);
    str.remove_prefix(CHAR_BIT);
    byte = bits.to_ulong();
  }
  if (internal::IsLittleEndian()) {
    std::reverse(bytes.begin(), bytes.end());
  }
  memcpy(&val, bytes.data(), sizeof(val));
  return val;
}

}  // namespace fpga

#endif  // FPGA_RUNTIME_STRINGIFY_H_
