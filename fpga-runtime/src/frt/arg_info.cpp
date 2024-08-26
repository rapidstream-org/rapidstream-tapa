// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include "frt/arg_info.h"

#include <ostream>

namespace fpga {

std::ostream& operator<<(std::ostream& os, const ArgInfo::Cat& cat) {
  switch (cat) {
    case ArgInfo::kScalar:
      return os << "scalar";
    case ArgInfo::kMmap:
      return os << "mmap";
    case ArgInfo::kStream:
      return os << "stream";
  }
  return os;
}

std::ostream& operator<<(std::ostream& os, const ArgInfo& arg) {
  os << "ArgInfo: {index: " << arg.index << ", name: '" << arg.name
     << "', type: '" << arg.type << "', category: " << arg.cat;
  os << "}";
  return os;
}

}  // namespace fpga
