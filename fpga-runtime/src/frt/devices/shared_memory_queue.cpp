// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include "frt/devices/shared_memory_queue.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <string>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#include <glog/logging.h>

namespace fpga {
namespace internal {

namespace {

constexpr char kMagic[] = "tapa";
constexpr int32_t kVersion = 1;

}  // namespace

void SharedMemoryQueue::Deleter::operator()(SharedMemoryQueue* ptr) {
  if (munmap(ptr, ptr->mmap_len()) != 0) {
    PLOG(ERROR) << "munmap";
  }
}

SharedMemoryQueue::UniquePtr SharedMemoryQueue::New(int fd) {
  auto* ptr = static_cast<SharedMemoryQueue*>(
      mmap(nullptr, sizeof(SharedMemoryQueue), PROT_READ | PROT_WRITE,
           MAP_SHARED, fd, /*offset=*/0));
  if (ptr == MAP_FAILED) {
    PLOG(ERROR) << "mmap";
    return nullptr;
  }

  const std::string magic(ptr->magic_, sizeof(ptr->magic_));
  if (magic != kMagic) {
    LOG(ERROR) << "unexpected magic '" << magic << "'; want '" << kMagic << "'"
               << "; size: " << magic.size();
    return nullptr;
  }

  if (ptr->version_ != kVersion) {
    LOG(ERROR) << "unexpected version " << ptr->version_ << "; want "
               << kVersion;
    return nullptr;
  }

  if (ptr->depth_ <= 0) {
    LOG(ERROR) << "unexpected non-positive depth " << ptr->depth_;
    return nullptr;
  }

  if (ptr->width_ <= 0) {
    LOG(ERROR) << "unexpected non-positive width " << ptr->width_;
    return nullptr;
  }

  ptr = static_cast<SharedMemoryQueue*>(
      mremap(ptr, sizeof(SharedMemoryQueue), ptr->mmap_len(), MREMAP_MAYMOVE));
  if (ptr == MAP_FAILED) {
    PLOG(ERROR) << "mremap";
    PLOG_IF(ERROR, munmap(ptr, sizeof(SharedMemoryQueue)) != 0) << "munmap";
    return nullptr;
  }

  return UniquePtr(ptr);
}

int SharedMemoryQueue::CreateFile(std::string& path, int32_t depth,
                                  int32_t width) {
  int fd = mkostemp(&path[0], O_CLOEXEC);
  if (fd < 0) {
    PLOG(ERROR) << "mkostemp";
    return fd;
  }

  SharedMemoryQueue queue;
  static_assert(sizeof(queue.magic_) + 1 == sizeof(kMagic));  // +1 for '\0'
  memcpy(queue.magic_, kMagic, sizeof(queue.magic_));
  queue.version_ = kVersion;
  queue.depth_ = depth;
  queue.width_ = width;
  int rc = ftruncate(fd, sizeof(queue) + depth * width);
  if (rc == 0) {
    rc = write(fd, &queue, sizeof(queue));
    if (rc == sizeof(queue)) {
      return fd;
    }
    if (rc >= 0) {
      LOG(ERROR) << "partial write: wrote " << rc << " bytes, want "
                 << sizeof(queue);
    } else {
      PLOG(ERROR) << "write";
    }
  } else {
    PLOG(ERROR) << "ftruncate";
  }
  PLOG_IF(ERROR, close(fd)) << "close";
  PLOG_IF(ERROR, unlink(path.c_str())) << "unlink";
  return rc;
}

int64_t SharedMemoryQueue::size() const { return head_ - tail_; }

int64_t SharedMemoryQueue::capacity() const { return depth_; }

int64_t SharedMemoryQueue::width() const { return width_; }

bool SharedMemoryQueue::empty() const { return size() <= 0; }

bool SharedMemoryQueue::full() const { return size() >= capacity(); }

std::string SharedMemoryQueue::front() const {
  return std::string(&data_[(tail_ % depth_) * width_], width_);
}

std::string SharedMemoryQueue::pop() {
  CHECK_GT(size(), 0) << "pop called on an empty queue";
  std::string val = front();
  ++tail_;
  return val;
}

void SharedMemoryQueue::push(const std::string& val) {
  CHECK_LT(size(), capacity()) << "push called on a full queue";
  CHECK_EQ(val.size(), width_) << "unexpected input: " << val;
  memcpy(&data_[(head_ % depth_) * width_], val.data(), val.size());
  ++head_;
}

size_t SharedMemoryQueue::mmap_len() const {
  return sizeof(*this) + depth_ * width_;
}

}  // namespace internal
}  // namespace fpga
