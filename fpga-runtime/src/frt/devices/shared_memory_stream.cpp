// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include "frt/devices/shared_memory_stream.h"

#include <utility>

#include <sys/mman.h>
#include <unistd.h>

#include <glog/logging.h>

#include "frt/devices/shared_memory_queue.h"

namespace fpga {
namespace internal {

SharedMemoryStream::SharedMemoryStream(Options options)
    : path_(std::move(options.path_template)),
      fd_(SharedMemoryQueue::CreateFile(path_, options.depth, options.width)),
      queue_(SharedMemoryQueue::New(fd_)) {}

SharedMemoryStream::~SharedMemoryStream() {
  if (fd_ >= 0) {
    PLOG_IF(WARNING, shm_unlink(path_.c_str())) << __func__ << ": shm_unlink";
    PLOG_IF(WARNING, close(fd_)) << __func__ << ": close";
  }
}

const std::string& SharedMemoryStream::path() const { return path_; }
SharedMemoryQueue* SharedMemoryStream::queue() const { return queue_.get(); }

}  // namespace internal
}  // namespace fpga
