// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#ifndef FPGA_RUNTIME_TAG_H_
#define FPGA_RUNTIME_TAG_H_

namespace fpga {
namespace internal {

enum class Tag {
  kPlaceHolder = 0,
  kReadOnly = 1,
  kWriteOnly = 2,
  kReadWrite = 3,
};

}  // namespace internal
}  // namespace fpga

#endif  // FPGA_RUNTIME_TAG_H_
