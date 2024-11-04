// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#ifndef FPGA_RUNTIME_STREAM_ARG_H_
#define FPGA_RUNTIME_STREAM_ARG_H_

#include <any>
#include <utility>

namespace fpga {
namespace internal {

// Type-erased streaming interface. Used to pass stream arguments to devices
// with arbitrary type-safe context.
class StreamArg {
 public:
  explicit StreamArg(std::any context) : context_(std::move(context)) {}

  // Not copyable or movable.
  StreamArg(const StreamArg&) = delete;
  StreamArg& operator=(const StreamArg&) = delete;

  template <typename Context>
  Context get() const {
    return std::any_cast<Context>(context_);
  }

  template <typename Context>
  void set(Context context) {
    context_ = context;
  }

 private:
  std::any context_;
};

}  // namespace internal
}  // namespace fpga

#endif  // FPGA_RUNTIME_STREAM_ARG_H_
