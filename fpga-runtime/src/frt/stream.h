// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#ifndef FPGA_RUNTIME_STREAM_H_
#define FPGA_RUNTIME_STREAM_H_

#include <memory>
#include <string>

#include "frt/stream_wrapper.h"
#include "frt/tag.h"

namespace fpga {
namespace internal {

template <Tag tag>
class Stream;

template <>
class Stream<Tag::kReadOnly> : public StreamWrapper {
 public:
  Stream(const std::string& name) : StreamWrapper(name) {}

  template <typename T>
  void Read(T* host_ptr, size_t size, bool eot = true) {
    stream_->Read(host_ptr, size * sizeof(T), eot);
  }
};

template <>
class Stream<Tag::kWriteOnly> : public StreamWrapper {
 public:
  Stream(const std::string& name) : StreamWrapper(name) {}

  template <typename T>
  void Write(const T* host_ptr, size_t size, bool eot = true) {
    stream_->Write(host_ptr, size * sizeof(T), eot);
  }
};

}  // namespace internal
}  // namespace fpga

#endif  // FPGA_RUNTIME_STREAM_H_
