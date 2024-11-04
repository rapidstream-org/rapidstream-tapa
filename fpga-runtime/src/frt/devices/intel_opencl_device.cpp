// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include "frt/devices/intel_opencl_device.h"

#include <memory>
#include <stdexcept>
#include <string>

#include <elf.h>

#include <glog/logging.h>
#include <tinyxml2.h>
#include <CL/cl2.hpp>

#include "frt/devices/opencl_device_matcher.h"
#include "frt/devices/opencl_util.h"
#include "frt/stream_arg.h"
#include "frt/tag.h"

namespace fpga {
namespace internal {

namespace {

class DeviceMatcher : public OpenclDeviceMatcher {
 public:
  explicit DeviceMatcher(std::string target_device_name)
      : target_device_name_(std::move(target_device_name)) {}

  std::string GetTargetName() const override { return target_device_name_; }

  std::string Match(cl::Device device) const override {
    const std::string device_name = device.getInfo<CL_DEVICE_NAME>();
    LOG(INFO) << "Found device: " << device_name;

    // Intel devices contain a std::string that is unavailable from the binary.
    const std::string prefix = target_device_name_ + " : ";
    if (device_name == target_device_name_ ||
        device_name.substr(0, prefix.size()) == prefix) {
      return device_name;
    }
    return "";
  }

 private:
  const std::string target_device_name_;
};

}  // namespace

IntelOpenclDevice::IntelOpenclDevice(const cl::Program::Binaries& binaries) {
  std::string target_device_name;
  std::string vendor_name;
  std::vector<std::string> kernel_names;
  std::vector<int> kernel_arg_counts;
  int arg_count = 0;
  auto data = binaries.begin()->data();
  if (data[EI_CLASS] == ELFCLASS32) {
    vendor_name = "Intel(R) FPGA SDK for OpenCL(TM)";
    auto elf_header = reinterpret_cast<const Elf32_Ehdr*>(data);
    auto elf_section_headers = reinterpret_cast<const Elf32_Shdr*>(
        (reinterpret_cast<const char*>(elf_header) + elf_header->e_shoff));
    auto elf_section = [&](int idx) -> const Elf32_Shdr* {
      return &elf_section_headers[idx];
    };
    auto elf_str_table =
        (elf_header->e_shstrndx == SHN_UNDEF)
            ? nullptr
            : reinterpret_cast<const char*>(elf_header) +
                  elf_section(elf_header->e_shstrndx)->sh_offset;
    for (int i = 0; i < elf_header->e_shnum; ++i) {
      auto section_header = elf_section(i);
      auto section_name =
          elf_str_table ? elf_str_table + section_header->sh_name : nullptr;
      if (strcmp(section_name, ".acl.kernel_arg_info.xml") == 0) {
        tinyxml2::XMLDocument doc;
        doc.Parse(reinterpret_cast<const char*>(elf_header) +
                      section_header->sh_offset,
                  0);
        for (auto xml_kernel =
                 doc.FirstChildElement("board")->FirstChildElement("kernel");
             xml_kernel != nullptr;
             xml_kernel = xml_kernel->NextSiblingElement("kernel")) {
          kernel_names.push_back(xml_kernel->Attribute("name"));
          kernel_arg_counts.push_back(arg_count);
          for (auto xml_arg = xml_kernel->FirstChildElement("argument");
               xml_arg != nullptr;
               xml_arg = xml_arg->NextSiblingElement("argument")) {
            auto& arg = arg_table_[arg_count];
            arg.index = arg_count;
            ++arg_count;
            arg.name = xml_arg->Attribute("name");
            arg.type = xml_arg->Attribute("type_name");
            auto cat = atoi(xml_arg->Attribute("opencl_access_type"));
            switch (cat) {
              case 0:
                arg.cat = ArgInfo::kScalar;
                break;
              case 2:
                arg.cat = ArgInfo::kMmap;
                break;
              default:
                LOG(WARNING) << "Unknown argument category: " << cat;
            }
          }
        }
      } else if (strcmp(section_name, ".acl.board") == 0) {
        const std::string board_name(reinterpret_cast<const char*>(elf_header) +
                                         section_header->sh_offset,
                                     section_header->sh_size);
        if (board_name == "EmulatorDevice") {
          setenv("CL_CONTEXT_EMULATOR_DEVICE_INTELFPGA", "1", 0);
          LOG(INFO) << "Running hardware simulation with Intel OpenCL";
        } else if (board_name == "SimulatorDevice") {
          setenv("CL_CONTEXT_MPSIM_DEVICE_INTELFPGA", "1", 0);
          LOG(INFO) << "Running software simulation with Intel OpenCL";
        } else {
          LOG(INFO) << "Running on-board execution with Intel OpenCL";
        }
        target_device_name = board_name;
      }
    }
    if (kernel_names.empty() || target_device_name.empty()) {
      LOG(FATAL) << "Unexpected ELF file";
    }
  } else if (data[EI_CLASS] == ELFCLASS64) {
    vendor_name = "Intel(R) FPGA Emulation Platform for OpenCL(TM)";
    target_device_name = "Intel(R) FPGA Emulation Device";
    LOG(FATAL) << "Fast emulator not supported";
  } else {
    LOG(FATAL) << "Unexpected ELF file";
  }

  Initialize(binaries, vendor_name, DeviceMatcher(target_device_name),
             kernel_names, kernel_arg_counts);
}

std::unique_ptr<Device> IntelOpenclDevice::New(
    const cl::Program::Binaries& binaries) {
  if (binaries.size() != 1 || binaries.begin()->size() < SELFMAG ||
      memcmp(binaries.begin()->data(), ELFMAG, SELFMAG) != 0) {
    return nullptr;
  }
  return std::make_unique<IntelOpenclDevice>(binaries);
}

void IntelOpenclDevice::SetStreamArg(int index, Tag tag, StreamArg& arg) {
  LOG(FATAL) << "Intel OpenCL device does not support streaming";
};

void IntelOpenclDevice::WriteToDevice() {
  load_event_.resize(load_indices_.size());
  int i = 0;
  for (auto index : load_indices_) {
    auto buffer = buffer_table_[index];
    CL_CHECK(cmd_.enqueueWriteBuffer(
        buffer, /* blocking = */ CL_FALSE, /* offset = */ 0,
        buffer.getInfo<CL_MEM_SIZE>(), host_ptr_table_[index],
        /* events = */ nullptr, &load_event_[i]));
    ++i;
  }
}

void IntelOpenclDevice::ReadFromDevice() {
  store_event_.resize(store_indices_.size());
  int i = 0;
  for (auto index : store_indices_) {
    auto buffer = buffer_table_[index];
    cmd_.enqueueReadBuffer(buffer, /* blocking = */ CL_FALSE,
                           /* offset = */ 0, buffer.getInfo<CL_MEM_SIZE>(),
                           host_ptr_table_[index], &compute_event_,
                           &store_event_[i]);
    ++i;
  }
}

cl::Buffer IntelOpenclDevice::CreateBuffer(int index, cl_mem_flags flags,
                                           void* host_ptr, size_t size) {
  flags |= /* CL_MEM_HETEROGENEOUS_INTELFPGA = */ 1 << 19;
  host_ptr_table_[index] = host_ptr;
  host_ptr = nullptr;
  return OpenclDevice::CreateBuffer(index, flags, host_ptr, size);
}

}  // namespace internal
}  // namespace fpga
