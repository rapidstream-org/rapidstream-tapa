// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#ifndef FPGA_RUNTIME_STREAM_H_
#define FPGA_RUNTIME_STREAM_H_

#include "frt/devices/shared_memory_queue.h"

#include <memory>

#include <glog/logging.h>

#include "frt/devices/shared_memory_stream.h"
#include "frt/stream_arg.h"
#include "frt/stringify.h"
#include "frt/tag.h"

namespace fpga {
namespace internal {

template <typename T>
class StreamBase : public StreamArg {
 public:
  explicit StreamBase(int64_t depth)
      : StreamArg(
            std::make_shared<SharedMemoryStream>(SharedMemoryStream::Options{
                .depth = depth,
                .width = ToBinaryString(T()).size(),
            })) {}

 protected:
  SharedMemoryQueue& queue() const {
    return *CHECK_NOTNULL(get<std::shared_ptr<SharedMemoryStream>>()->queue());
  }
};

template <typename T, Tag tag>
class Stream;

template <typename T>
class Stream<T, Tag::kReadWrite> : public StreamBase<T> {
 public:
  using StreamBase<T>::StreamBase;

  bool empty() const { return this->queue().empty(); }
  bool full() const { return this->queue().full(); }
  void push(const T& val) { this->queue().push(ToBinaryString(val)); }
  T pop() { return FromBinaryString<T>(this->queue().pop()); }
  T front() const { return FromBinaryString<T>(this->queue().front()); }
};

}  // namespace internal
}  // namespace fpga

#endif  // FPGA_RUNTIME_STREAM_H_
