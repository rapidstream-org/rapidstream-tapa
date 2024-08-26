// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#ifndef FPGA_RUNTIME_XILINX_OPENCL_DEVICE_H_
#define FPGA_RUNTIME_XILINX_OPENCL_DEVICE_H_

#include <cstddef>

#include <memory>
#include <string>
#include <unordered_map>

#include <CL/cl2.hpp>

#include "frt/devices/opencl_device.h"

namespace fpga {
namespace internal {

class XilinxOpenclDevice : public OpenclDevice {
 public:
  XilinxOpenclDevice(const cl::Program::Binaries& binaries);

  static std::unique_ptr<Device> New(const cl::Program::Binaries& binaries);

  void SetStreamArg(int index, Tag tag, StreamWrapper& arg) override;
  void WriteToDevice() override;
  void ReadFromDevice() override;

 private:
  cl::Buffer CreateBuffer(int index, cl_mem_flags flags, void* host_ptr,
                          size_t size) override;
};

}  // namespace internal
}  // namespace fpga

#endif  // FPGA_RUNTIME_XILINX_OPENCL_DEVICE_H_
