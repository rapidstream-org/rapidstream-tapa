// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#ifndef FPGA_RUNTIME_INTEL_OPENCL_DEVICE_H_
#define FPGA_RUNTIME_INTEL_OPENCL_DEVICE_H_

#include <cstddef>

#include <memory>
#include <string>

#include <CL/cl2.hpp>

#include "frt/devices/opencl_device.h"

namespace fpga {
namespace internal {

class IntelOpenclDevice : public OpenclDevice {
 public:
  IntelOpenclDevice(const cl::Program::Binaries& binaries);

  static std::unique_ptr<Device> New(const cl::Program::Binaries& binaries);

  void SetStreamArg(size_t index, Tag tag, StreamArg& arg) override;
  void WriteToDevice() override;
  void ReadFromDevice() override;

 private:
  cl::Buffer CreateBuffer(size_t index, cl_mem_flags flags, void* host_ptr,
                          size_t size) override;

  std::unordered_map<int, void*> host_ptr_table_;
};

}  // namespace internal
}  // namespace fpga

#endif  // FPGA_RUNTIME_INTEL_OPENCL_DEVICE_H_
