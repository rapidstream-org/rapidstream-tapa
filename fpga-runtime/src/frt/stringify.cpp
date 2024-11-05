// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include "frt/stringify.h"

#include <string>
#include <string_view>

#include <boost/endian.hpp>

std::string ToBinaryStringImpl(const bool* val) {
  CHECK_NE(val, nullptr);
  return *val ? "1" : "0";
}
void FromBinaryStringImpl(std::string_view str, bool& val) {
  CHECK_EQ(str.size(), 1) << str;
  val = str[0] != '0';
}

namespace fpga {
namespace internal {

bool IsLittleEndian() {
  return boost::endian::order::native == boost::endian::order::little;
}

}  // namespace internal
}  // namespace fpga
