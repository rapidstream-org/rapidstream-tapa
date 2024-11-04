// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#ifndef FPGA_RUNTIME_STREAM_H_
#define FPGA_RUNTIME_STREAM_H_

#include <memory>
#include <string>

#include "frt/stream_arg.h"
#include "frt/tag.h"

namespace fpga {
namespace internal {

template <Tag tag>
class Stream;

template <>
class Stream<Tag::kReadOnly> : public StreamArg {
 public:
  Stream(const std::string& name) : StreamArg(name) {}
};

template <>
class Stream<Tag::kWriteOnly> : public StreamArg {
 public:
  Stream(const std::string& name) : StreamArg(name) {}
};

}  // namespace internal
}  // namespace fpga

#endif  // FPGA_RUNTIME_STREAM_H_
