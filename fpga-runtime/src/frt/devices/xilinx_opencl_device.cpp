// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include "frt/devices/xilinx_opencl_device.h"

#include <cstdlib>

#include <fstream>
#include <initializer_list>
#include <memory>
#include <string>
#include <string_view>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <CL/cl.h>
#include <gflags/gflags.h>
#include <glog/logging.h>
#include <tinyxml2.h>
#include <xclbin.h>
#include <CL/cl2.hpp>
#include <nlohmann/json.hpp>

#include "frt/devices/filesystem.h"
#include "frt/devices/opencl_device_matcher.h"
#include "frt/devices/opencl_util.h"
#include "frt/devices/xilinx_environ.h"
#include "frt/stream_wrapper.h"
#include "frt/subprocess.h"
#include "frt/tag.h"

DEFINE_string(xocl_bdf, "",
              "if not empty, use the specified PCIe Bus:Device:Function "
              "instead of trying to match device name");

namespace fpga {
namespace internal {

namespace {

std::vector<std::string_view> Split(std::string_view text, char delimiter,
                                    size_t maxsplit = -1) {
  std::vector<std::string_view> pieces;
  size_t piece_len = 0;
  for (char character : text) {
    if (pieces.size() == maxsplit) break;
    if (character == delimiter) {
      pieces.push_back(text.substr(0, piece_len));
      text.remove_prefix(piece_len + 1);
      piece_len = 0;
    } else {
      ++piece_len;
    }
  }
  pieces.push_back(text);
  return pieces;
}

bool StartsWith(std::string_view text, std::string_view prefix) {
  return text.size() >= prefix.size() &&
         text.substr(0, prefix.size()) == prefix;
}

std::string Concat(std::initializer_list<std::string_view> pieces) {
  size_t total_length = 0;
  for (auto piece : pieces) total_length += piece.size();
  std::string text;
  text.reserve(total_length);
  for (auto piece : pieces) text += piece;
  return text;
}

class DeviceMatcher : public OpenclDeviceMatcher {
 public:
  explicit DeviceMatcher(std::string target_device_name)
      : target_device_name_(std::move(target_device_name)),
        target_device_name_pieces_(
            Split(target_device_name_, /*delimiter=*/'_', /*maxsplit=*/4)) {}

  // Not copyable nor movable because the `string_view`s won't be valid.
  DeviceMatcher(const DeviceMatcher&) = delete;
  DeviceMatcher(DeviceMatcher&&) = delete;
  DeviceMatcher& operator=(const DeviceMatcher&) = delete;
  DeviceMatcher& operator=(DeviceMatcher&&) = delete;

  std::string GetTargetName() const override { return target_device_name_; }

  std::string Match(cl::Device device) const override {
    const std::string device_name = device.getInfo<CL_DEVICE_NAME>();
    char bdf[32];
    size_t bdf_size = 0;
    cl_int rc = clGetDeviceInfo(device.get(), CL_DEVICE_PCIE_BDF, sizeof(bdf),
                                bdf, &bdf_size);
    if (rc != CL_SUCCESS) {
      return "";
    }
    const std::string device_name_and_bdf =
        Concat({device_name, " (bdf=", bdf, ")"});
    LOG(INFO) << "Found device: " << device_name_and_bdf;

    if (const std::string target_bdf = FLAGS_xocl_bdf; !target_bdf.empty()) {
      if (target_bdf == bdf) {
        return device_name_and_bdf;
      }
      return "";
    }

    if (device_name == target_device_name_) return device_name_and_bdf;

    // Xilinx devices might have unrelated names in the binary:
    //   1) target_device_name == "xilinx_u250_gen3x16_xdma_3_1_202020_1"
    //      device_name == "xilinx_u250_gen3x16_xdma_shell_3_1"
    //   2) target_device_name == "xilinx_u200_gen3x16_xdma_1_202110_1"
    //      device_name == "xilinx_u200_gen3x16_xdma_base_1"

    // For 1), this is {"xilinx", "u250", "gen3x16", "xdma", "3_1_202020_1"}.
    // For 2), this is {"xilinx", "u200", "gen3x16", "xdma", "1_202110_1"}.
    if (target_device_name_pieces_.size() < 5) return "";

    // For 1), this is {"xilinx", "u250", "gen3x16", "xdma", "shell", "3_1"}.
    // For 2), this is {"xilinx", "u200", "gen3x16", "xdma", "base", "1"}.
    std::vector<std::string_view> device_name_pieces =
        Split(device_name, /*delimiter=*/'_', /*maxsplit=*/5);
    if (device_name_pieces.size() < 6) return "";

    for (int i = 0; i < 4; ++i) {
      if (device_name_pieces[i] != target_device_name_pieces_[i]) return "";
    }

    if (StartsWith(target_device_name_pieces_[4], device_name_pieces[5])) {
      return device_name_and_bdf;
    }

    return "";
  }

 private:
  const std::string target_device_name_;
  const std::vector<std::string_view> target_device_name_pieces_;
};

}  // namespace

XilinxOpenclDevice::XilinxOpenclDevice(const cl::Program::Binaries& binaries) {
  std::string target_device_name;
  std::vector<std::string> kernel_names;
  std::vector<int> kernel_arg_counts;
  int arg_count = 0;
  const auto axlf_top = reinterpret_cast<const axlf*>(binaries.begin()->data());
  switch (axlf_top->m_header.m_mode) {
    case XCLBIN_FLAT:
    case XCLBIN_PR:
    case XCLBIN_TANDEM_STAGE2:
    case XCLBIN_TANDEM_STAGE2_WITH_PR:
      break;
    case XCLBIN_HW_EMU:
      setenv("XCL_EMULATION_MODE", "hw_emu", 0);
      break;
    case XCLBIN_SW_EMU:
      setenv("XCL_EMULATION_MODE", "sw_emu", 0);
      break;
    default:
      LOG(FATAL) << "Unknown xclbin mode";
  }
  target_device_name =
      reinterpret_cast<const char*>(axlf_top->m_header.m_platformVBNV);
  LOG_IF(FATAL, target_device_name.empty())
      << "Cannot determine target device name from binary";
  if (auto metadata = xclbin::get_axlf_section(axlf_top, EMBEDDED_METADATA)) {
    std::string xml_string(
        reinterpret_cast<const char*>(axlf_top) + metadata->m_sectionOffset,
        metadata->m_sectionSize);
    tinyxml2::XMLDocument doc;
    doc.Parse(xml_string.c_str());
    auto xml_core = doc.FirstChildElement("project")
                        ->FirstChildElement("platform")
                        ->FirstChildElement("device")
                        ->FirstChildElement("core");
    std::string target_meta = xml_core->Attribute("target");
    for (auto xml_kernel = xml_core->FirstChildElement("kernel");
         xml_kernel != nullptr;
         xml_kernel = xml_kernel->NextSiblingElement("kernel")) {
      kernel_names.push_back(xml_kernel->Attribute("name"));
      kernel_arg_counts.push_back(arg_count);
      for (auto xml_arg = xml_kernel->FirstChildElement("arg");
           xml_arg != nullptr; xml_arg = xml_arg->NextSiblingElement("arg")) {
        auto& arg = arg_table_[arg_count];
        arg.index = arg_count;
        ++arg_count;
        arg.name = xml_arg->Attribute("name");
        arg.type = xml_arg->Attribute("type");
        auto cat = atoi(xml_arg->Attribute("addressQualifier"));
        switch (cat) {
          case 0:
            arg.cat = ArgInfo::kScalar;
            break;
          case 1:
            arg.cat = ArgInfo::kMmap;
            break;
          case 4:
            arg.cat = ArgInfo::kStream;
            break;
          default:
            LOG(WARNING) << "Unknown argument category: " << cat;
        }
      }
    }
    // m_mode doesn't always work
    if (target_meta == "hw_em") {
      setenv("XCL_EMULATION_MODE", "hw_emu", 0);
    } else if (target_meta == "csim") {
      setenv("XCL_EMULATION_MODE", "sw_emu", 0);
    }
  } else {
    LOG(FATAL) << "Cannot determine kernel name from binary";
  }

  if (const char* xcl_emulation_mode = getenv("XCL_EMULATION_MODE")) {
    for (const auto& [name, value] : xilinx::GetEnviron()) {
      setenv(name.c_str(), value.c_str(), /* __replace = */ 1);
    }

    const auto uid = std::to_string(geteuid());

    // Vitis software simulation stucks without $USER.
    setenv("USER", uid.c_str(), /* __replace = */ 0);

    const char* tmpdir_or_null = getenv("TMPDIR");
    std::string tmpdir = tmpdir_or_null ? tmpdir_or_null : "/tmp";
    tmpdir += "/.frt." + uid;
    if (mkdir(tmpdir.c_str(), S_IRUSR | S_IWUSR | S_IXUSR) && errno != EEXIST) {
      LOG(FATAL) << "Cannot create FRT tmpdir '" << tmpdir
                 << "': " << strerror(errno);
    }

    // If SDACCEL_EM_RUN_DIR is not set, use a per-use tmpdir for `.run`.
    setenv("SDACCEL_EM_RUN_DIR", tmpdir.c_str(), 0);

    // If EMCONFIG_PATH is not set, use a per-user and per-device tmpdir to
    // cache `emconfig.json`.
    fs::path emconfig_dir;
    if (const char* emconfig_dir_or_null = getenv("EMCONFIG_PATH")) {
      emconfig_dir = emconfig_dir_or_null;
    } else {
      emconfig_dir = tmpdir;
      emconfig_dir /= "emconfig." + target_device_name;
      setenv("EMCONFIG_PATH", emconfig_dir.c_str(), 0);
    }

    // Detect if emconfig already exists.
    bool is_emconfig_ready = false;
    if (fs::path emconfig_path = emconfig_dir / "emconfig.json";
        fs::is_regular_file(emconfig_path)) {
      nlohmann::json json = nlohmann::json::parse(std::ifstream(emconfig_path));
      try {
        for (const auto& board : json.at("Platform").at("Boards")) {
          for (const auto& device : board.at("Devices")) {
            if (device.at("Name") == target_device_name) {
              is_emconfig_ready = true;
            }
          }
        }
      } catch (const nlohmann::json::out_of_range&) {
      }
    }

    // Generate `emconfig.json` when necessary.
    if (!is_emconfig_ready) {
      fs::path emconfig_dir_per_pid = emconfig_dir;
      emconfig_dir_per_pid += "." + std::to_string(getpid());
      try {
        int return_code = subprocess::call({
            "emconfigutil",
            "--platform",
            target_device_name,
            "--od",
            emconfig_dir_per_pid.native(),
        });
        LOG_IF(FATAL, return_code != 0) << "emconfigutil failed";
      } catch (const std::exception& e) {
        LOG(FATAL) << "emconfigutil failed: " << e.what();
      }

      // Use `rename` to create the emconfig directory atomically.
      fs::path emconfig_dir_per_pid_tmp = emconfig_dir_per_pid;
      emconfig_dir_per_pid_tmp += ".tmp";
      fs::create_directory_symlink(
          emconfig_dir_per_pid.filename(),  // Use relative path for symlink.
          emconfig_dir_per_pid_tmp);
      fs::rename(emconfig_dir_per_pid_tmp, emconfig_dir);
    }
    if (xcl_emulation_mode == std::string_view("sw_emu")) {
      LOG(INFO) << "Running software simulation with Xilinx OpenCL";
    } else if (xcl_emulation_mode == std::string_view("hw_emu")) {
      LOG(INFO) << "Running hardware simulation with Xilinx OpenCL";
    } else {
      LOG(FATAL) << "Unexpected XCL_EMULATION_MODE: " << xcl_emulation_mode;
    }
  } else {
    LOG(INFO) << "Running on-board execution with Xilinx OpenCL";
  }

  Initialize(binaries, /*vendor_name=*/"Xilinx",
             DeviceMatcher(target_device_name), kernel_names,
             kernel_arg_counts);
}

std::unique_ptr<Device> XilinxOpenclDevice::New(
    const cl::Program::Binaries& binaries) {
  if (binaries.size() != 1 || binaries.begin()->size() < 8 ||
      memcmp(binaries.begin()->data(), "xclbin2", 8) != 0) {
    return nullptr;
  }
  return std::make_unique<XilinxOpenclDevice>(binaries);
}

void XilinxOpenclDevice::SetStreamArg(int index, Tag tag, StreamWrapper& arg) {
  LOG(FATAL) << "Xilinx OpenCL streaming is disabled";
}

void XilinxOpenclDevice::WriteToDevice() {
  if (!load_indices_.empty()) {
    load_event_.resize(1);
    CL_CHECK(cmd_.enqueueMigrateMemObjects(GetLoadBuffers(), /* flags = */ 0,
                                           /* events = */ nullptr,
                                           load_event_.data()));
  } else {
    load_event_.clear();
  }
}

void XilinxOpenclDevice::ReadFromDevice() {
  if (!store_indices_.empty()) {
    store_event_.resize(1);
    CL_CHECK(cmd_.enqueueMigrateMemObjects(
        GetStoreBuffers(), CL_MIGRATE_MEM_OBJECT_HOST, &compute_event_,
        store_event_.data()));
  } else {
    store_event_.clear();
  }
}

cl::Buffer XilinxOpenclDevice::CreateBuffer(int index, cl_mem_flags flags,
                                            void* host_ptr, size_t size) {
  flags |= CL_MEM_USE_HOST_PTR;
  return OpenclDevice::CreateBuffer(index, flags, host_ptr, size);
}

}  // namespace internal
}  // namespace fpga
