// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#ifndef FPGA_RUNTIME_TAPA_FAST_COSIM_
#define FPGA_RUNTIME_TAPA_FAST_COSIM_

#include <chrono>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include "frt/buffer_arg.h"
#include "frt/device.h"
#include "frt/devices/shared_memory_stream.h"
#include "frt/stream_arg.h"

namespace fpga {
namespace internal {

class TapaFastCosimDevice : public Device {
 public:
  TapaFastCosimDevice(std::string_view bitstream);
  TapaFastCosimDevice(const TapaFastCosimDevice&) = delete;
  TapaFastCosimDevice& operator=(const TapaFastCosimDevice&) = delete;
  TapaFastCosimDevice(TapaFastCosimDevice&&) = delete;
  TapaFastCosimDevice& operator=(TapaFastCosimDevice&&) = delete;

  ~TapaFastCosimDevice() override;

  static std::unique_ptr<Device> New(std::string_view path,
                                     std::string_view content);

  void SetScalarArg(int index, const void* arg, int size) override;
  void SetBufferArg(int index, Tag tag, const BufferArg& arg) override;
  void SetStreamArg(int index, Tag tag, StreamArg& arg) override;
  size_t SuspendBuffer(int index) override;

  void WriteToDevice() override;
  void ReadFromDevice() override;
  void Exec() override;
  void Finish() override;
  bool IsFinished() const override;

  std::vector<ArgInfo> GetArgsInfo() const override;
  int64_t LoadTimeNanoSeconds() const override;
  int64_t ComputeTimeNanoSeconds() const override;
  int64_t StoreTimeNanoSeconds() const override;
  size_t LoadBytes() const override;
  size_t StoreBytes() const override;

  const std::string xo_path;
  const std::string work_dir;

 private:
  void WriteToDeviceImpl();
  void ReadFromDeviceImpl();

  std::unordered_map<int, std::string> scalars_;
  std::unordered_map<int, BufferArg> buffer_table_;
  std::unordered_map<int, std::shared_ptr<SharedMemoryStream>> stream_table_;
  std::vector<ArgInfo> args_;
  std::unordered_set<int> load_indices_;
  std::unordered_set<int> store_indices_;

  bool is_write_to_device_scheduled_ = false;
  bool is_read_from_device_scheduled_ = false;

  std::chrono::nanoseconds load_time_;
  std::chrono::nanoseconds compute_time_;
  std::chrono::nanoseconds store_time_;

  struct Context;
  std::unique_ptr<Context> context_;  // For asynchronous execution.
};

}  // namespace internal
}  // namespace fpga

#endif  // FPGA_RUNTIME_TAPA_FAST_COSIM_
