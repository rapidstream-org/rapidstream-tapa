// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include <string>
#include <unordered_map>

namespace fpga::xilinx {

using Environ = std::unordered_map<std::string, std::string>;

Environ GetEnviron();

}  // namespace fpga::xilinx
