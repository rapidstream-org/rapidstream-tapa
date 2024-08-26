// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#ifndef FPGA_RUNTIME_H_
#define FPGA_RUNTIME_H_

#include <array>
#include <cstddef>
#include <cstdint>

#include <iostream>
#include <memory>
#include <ratio>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "frt/arg_info.h"
#include "frt/buffer.h"
#include "frt/device.h"
#include "frt/stream.h"
#include "frt/stream_wrapper.h"
#include "frt/tag.h"

namespace fpga {

template <typename T>
using ReadOnlyBuffer = internal::Buffer<T, internal::Tag::kReadOnly>;
template <typename T>
using WriteOnlyBuffer = internal::Buffer<T, internal::Tag::kWriteOnly>;
template <typename T>
using ReadWriteBuffer = internal::Buffer<T, internal::Tag::kReadWrite>;
template <typename T>
using PlaceholderBuffer = internal::Buffer<T, internal::Tag::kPlaceHolder>;

template <typename T>
ReadOnlyBuffer<T> ReadOnly(T* ptr, size_t n) {
  return ReadOnlyBuffer<T>(ptr, n);
}
template <typename T>
WriteOnlyBuffer<T> WriteOnly(T* ptr, size_t n) {
  return WriteOnlyBuffer<T>(ptr, n);
}
template <typename T>
ReadWriteBuffer<T> ReadWrite(T* ptr, size_t n) {
  return ReadWriteBuffer<T>(ptr, n);
}
template <typename T>
PlaceholderBuffer<T> Placeholder(T* ptr, size_t n) {
  return PlaceholderBuffer<T>(ptr, n);
}

using ReadStream = internal::Stream<internal::Tag::kReadOnly>;
using WriteStream = internal::Stream<internal::Tag::kWriteOnly>;

class Instance {
 public:
  Instance(const std::string& bitstream);

  // Sets a scalar argument.
  template <typename T>
  void SetArg(int index, T arg) {
    device_->SetScalarArg(index, &arg, sizeof(arg));
  }

  // Sets a buffer argument.
  template <typename T, internal::Tag tag>
  void SetArg(int index, internal::Buffer<T, tag> arg) {
    device_->SetBufferArg(index, tag, arg);
  }

  // Sets a stream argument.
  template <internal::Tag tag>
  void SetArg(int index, internal::Stream<tag>& arg) {
    device_->SetStreamArg(index, tag, arg);
  }

  // Sets all arguments.
  template <typename... Args>
  void SetArgs(Args&&... args) {
    SetArg(0, std::forward<Args>(args)...);
  }

  // Allocates buffer for an argument. This function is now deprecated and its
  // original functionality is now part of `SetArg`.
  template <typename T>
  [[deprecated("'SetArg' is sufficient")]] void AllocBuf(int index, T arg) {}

  // Suspends a buffer from being transferred between host and device and
  // returns the number of transfer operations suspended.
  size_t SuspendBuf(int index);

  // Writes buffers to the device.
  void WriteToDevice();

  // Reads buffers to the device.
  void ReadFromDevice();

  // Executes the program on the device.
  void Exec();

  // Waits for the program to finish.
  void Finish();

  // Invokes the program on the device. This is a shortcut for `SetArgs`,
  // `WriteToDevice`, `Exec`, `ReadFromDevice`, and if there is no stream
  // arguments, `Finish` as well.
  template <typename... Args>
  Instance& Invoke(Args&&... args) {
    SetArgs(std::forward<Args>(args)...);
    WriteToDevice();
    Exec();
    ReadFromDevice();
    bool has_stream = false;
    bool _[sizeof...(Args)] = {(
        has_stream |=
        std::is_base_of<internal::StreamWrapper,
                        typename std::remove_reference<Args>::type>::value)...};
    ConditionallyFinish(has_stream);
    return *this;
  }

  // Returns information of all args as a vector, sorted by the index.
  std::vector<ArgInfo> GetArgsInfo() const;

  // Returns the load time in nanoseconds.
  int64_t LoadTimeNanoSeconds() const;

  // Returns the compute time in nanoseconds.
  int64_t ComputeTimeNanoSeconds() const;

  // Returns the store time in nanoseconds.
  int64_t StoreTimeNanoSeconds() const;

  // Returns the load time in seconds.
  double LoadTimeSeconds() const;

  // Returns the compute time in seconds.
  double ComputeTimeSeconds() const;

  // Returns the store time in seconds.
  double StoreTimeSeconds() const;

  // Returns the load throughput in GB/s.
  double LoadThroughputGbps() const;

  // Returns the store throughput in GB/s.
  double StoreThroughputGbps() const;

 private:
  template <typename T, typename... Args>
  void SetArg(int index, T&& arg, Args&&... other_args) {
    SetArg(index, std::forward<T>(arg));
    SetArg(index + 1, std::forward<Args>(other_args)...);
  }

  void ConditionallyFinish(bool has_stream);

  std::unique_ptr<internal::Device> device_;
};

template <typename Arg, typename... Args>
Instance Invoke(const std::string& bitstream, Arg&& arg, Args&&... args) {
  return std::move(Instance(bitstream).Invoke(std::forward<Arg>(arg),
                                              std::forward<Args>(args)...));
}

}  // namespace fpga

#endif  // FPGA_RUNTIME_H_
