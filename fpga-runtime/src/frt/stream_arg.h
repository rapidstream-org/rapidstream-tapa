// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#ifndef FPGA_RUNTIME_STREAM_ARG_H_
#define FPGA_RUNTIME_STREAM_ARG_H_

#include <memory>
#include <string>

#include "frt/stream_interface.h"

namespace fpga {
namespace internal {

class StreamArg {
 public:
  void Attach(std::unique_ptr<StreamInterface>&& stream) {
    stream_ = std::move(stream);
  }
  const std::string name;

 protected:
  StreamArg(const std::string& name) : name(name) {}
  std::unique_ptr<StreamInterface> stream_;
};

}  // namespace internal
}  // namespace fpga

#endif  // FPGA_RUNTIME_STREAM_ARG_H_
