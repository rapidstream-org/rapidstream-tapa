// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#ifndef FPGA_RUNTIME_DEVICE_H_
#define FPGA_RUNTIME_DEVICE_H_

#include <cstddef>
#include <cstdint>

#include <vector>

#include "frt/arg_info.h"
#include "frt/buffer_arg.h"
#include "frt/stream_arg.h"
#include "frt/tag.h"

namespace fpga {
namespace internal {

class Device {
 public:
  virtual ~Device() = default;

  virtual void SetScalarArg(size_t index, const void* arg, int size) = 0;
  virtual void SetBufferArg(size_t index, Tag tag, const BufferArg& arg) = 0;
  virtual void SetStreamArg(size_t index, Tag tag, StreamArg& arg) = 0;
  virtual size_t SuspendBuffer(size_t index) = 0;

  virtual void WriteToDevice() = 0;
  virtual void ReadFromDevice() = 0;
  virtual void Exec() = 0;
  virtual void Finish() = 0;
  virtual void Kill() = 0;
  virtual bool IsFinished() const = 0;

  virtual std::vector<ArgInfo> GetArgsInfo() const = 0;
  virtual int64_t LoadTimeNanoSeconds() const = 0;
  virtual int64_t ComputeTimeNanoSeconds() const = 0;
  virtual int64_t StoreTimeNanoSeconds() const = 0;
  virtual size_t LoadBytes() const = 0;
  virtual size_t StoreBytes() const = 0;
};

}  // namespace internal
}  // namespace fpga

#endif  // FPGA_RUNTIME_DEVICE_H_
