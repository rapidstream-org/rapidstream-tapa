// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#ifndef FPGA_RUNTIME_OPENCL_DEVICE_H_
#define FPGA_RUNTIME_OPENCL_DEVICE_H_

#include <cstddef>
#include <cstdint>

#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <CL/cl2.hpp>

#include "frt/arg_info.h"
#include "frt/device.h"
#include "frt/devices/opencl_device_matcher.h"
#include "frt/tag.h"

namespace fpga {
namespace internal {

class OpenclDevice : public Device {
 public:
  void SetScalarArg(int index, const void* arg, int size) override;
  void SetBufferArg(int index, Tag tag, const BufferArg& arg) override;
  size_t SuspendBuffer(int index) override;

  void Exec() override;
  void Finish() override;

  std::vector<ArgInfo> GetArgsInfo() const override;
  int64_t LoadTimeNanoSeconds() const override;
  int64_t ComputeTimeNanoSeconds() const override;
  int64_t StoreTimeNanoSeconds() const override;
  size_t LoadBytes() const override;
  size_t StoreBytes() const override;

 protected:
  void Initialize(const cl::Program::Binaries& binaries,
                  const std::string& vendor_name,
                  const OpenclDeviceMatcher& device_matcher,
                  const std::vector<std::string>& kernel_names,
                  const std::vector<int>& kernel_arg_counts);
  virtual cl::Buffer CreateBuffer(int index, cl_mem_flags flags, void* host_ptr,
                                  size_t size);

  std::vector<cl::Memory> GetLoadBuffers() const;
  std::vector<cl::Memory> GetStoreBuffers() const;
  std::pair<int, cl::Kernel> GetKernel(int index) const;

  cl::Device device_;
  cl::Context context_;
  cl::CommandQueue cmd_;
  cl::Program program_;
  // Maps prefix sum of arg count to kernels.
  std::map<int, cl::Kernel> kernels_;
  std::unordered_map<int, cl::Buffer> buffer_table_;
  std::unordered_map<int, ArgInfo> arg_table_;
  std::unordered_set<int> load_indices_;
  std::unordered_set<int> store_indices_;
  std::vector<cl::Event> load_event_;
  std::vector<cl::Event> compute_event_;
  std::vector<cl::Event> store_event_;
};

}  // namespace internal
}  // namespace fpga

#endif  // FPGA_RUNTIME_OPENCL_DEVICE_H_
