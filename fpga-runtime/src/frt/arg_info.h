// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#ifndef FPGA_RUNTIME_ARG_INFO_H_
#define FPGA_RUNTIME_ARG_INFO_H_

#include <ostream>
#include <string>

namespace fpga {

struct ArgInfo {
  enum Cat {
    kScalar = 0,
    kMmap = 1,
    kStream = 2,
  };
  int index;
  std::string name;
  std::string type;
  Cat cat;
};

std::ostream& operator<<(std::ostream& os, const ArgInfo::Cat& cat);
std::ostream& operator<<(std::ostream& os, const ArgInfo& arg);

}  // namespace fpga

#endif  // FPGA_RUNTIME_ARG_INFO_H_
