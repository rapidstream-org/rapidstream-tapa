// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#ifndef FPGA_RUNTIME_SHARED_MEMORY_STREAM_H_
#define FPGA_RUNTIME_SHARED_MEMORY_STREAM_H_

#include <cstdint>

#include <string>

#include "frt/devices/shared_memory_queue.h"

namespace fpga {
namespace internal {

// Wrapper of `SharedMemoryQueue` that manages the backing file's path and fd.
class SharedMemoryStream {
 public:
  struct Options {
    uint64_t depth = 0;
    uint64_t width = 0;

    // This will be consumed by `mkstemp` to create a unique path for the shared
    // memory object. See `SharedMemoryQueue::CreateFile`.
    std::string filename_template = "shared_memory_queue.XXXXXX";
  };

  explicit SharedMemoryStream(Options options);

  // Not copyable or movable.
  SharedMemoryStream(const SharedMemoryStream&) = delete;
  SharedMemoryStream& operator=(const SharedMemoryStream&) = delete;

  ~SharedMemoryStream();

  const std::string& path() const;
  SharedMemoryQueue* queue() const;

 private:
  std::string path_;
  int fd_ = -1;
  SharedMemoryQueue::UniquePtr queue_;
};

}  // namespace internal
}  // namespace fpga

#endif  // FPGA_RUNTIME_SHARED_MEMORY_STREAM_H_
