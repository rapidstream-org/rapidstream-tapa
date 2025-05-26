// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include "frt/stringify.h"

#include <boost/endian.hpp>

namespace fpga {
namespace internal {

bool IsLittleEndian() {
  return boost::endian::order::native == boost::endian::order::little;
}

}  // namespace internal
}  // namespace fpga
