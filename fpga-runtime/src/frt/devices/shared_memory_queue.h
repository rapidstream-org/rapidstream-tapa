// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#ifndef FPGA_RUNTIME_SHARED_MEMORY_QUEUE_H_
#define FPGA_RUNTIME_SHARED_MEMORY_QUEUE_H_

#include <cstdint>
#include <cstring>

#include <atomic>
#include <memory>
#include <string>

namespace fpga {
namespace internal {

// Shared-memory lock-free SPSC queue with fixed depth and width.
class SharedMemoryQueue {
  struct Deleter {
    void operator()(SharedMemoryQueue* ptr);
  };

 public:
  using UniquePtr = std::unique_ptr<SharedMemoryQueue, Deleter>;

  // Returns `nullptr` on failure with logging.
  static UniquePtr New(int fd);

  // Creates (using `shm_open`) a shared memory object suitable for backing a
  // `SharedMemoryQueue` and returns the corresponding file descriptor, with
  // `path_template` modified to the path of the created shared memory object.
  // Returns a negative fd on failure with the corresponding errno and logging.
  static int CreateFile(std::string& path_template, int32_t depth,
                        int32_t width);

  // Not copyable or movable.
  SharedMemoryQueue(const SharedMemoryQueue&) = delete;
  SharedMemoryQueue* operator=(const SharedMemoryQueue&) = delete;

  size_t size() const;
  size_t capacity() const;
  size_t width() const;
  bool empty() const;
  bool full() const;
  std::string front() const;
  std::string pop();
  void push(const std::string& val);

 private:
  explicit SharedMemoryQueue() = default;

  size_t mmap_len() const;

  char magic_[4] = {};
  int32_t version_ = 0;
  uint32_t depth_ = 0;
  uint32_t width_ = 0;
  std::atomic<uint64_t> tail_{};
  std::atomic<uint64_t> head_{};
  char data_[];
};

static_assert(sizeof(SharedMemoryQueue) == 32);

}  // namespace internal
}  // namespace fpga

#endif  // FPGA_RUNTIME_SHARED_MEMORY_QUEUE_H_
