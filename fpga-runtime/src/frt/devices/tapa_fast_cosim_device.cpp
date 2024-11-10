// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include "frt/devices/tapa_fast_cosim_device.h"

#include <cstdlib>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <ios>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <tinyxml2.h>
#include <unistd.h>

#include <gflags/gflags.h>
#include <glog/logging.h>
#include <nlohmann/json.hpp>

#include "frt/arg_info.h"
#include "frt/devices/filesystem.h"
#include "frt/devices/shared_memory_stream.h"
#include "frt/devices/xilinx_environ.h"
#include "frt/stream_arg.h"
#include "frt/subprocess.h"
#include "frt/zip_file.h"

DEFINE_bool(xosim_start_gui, false, "start Vivado GUI for simulation");
DEFINE_bool(xosim_save_waveform, false, "save waveform in the work directory");
DEFINE_string(xosim_work_dir, "",
              "if not empty, use the specified work directory instead of a "
              "temporary one");
DEFINE_string(xosim_executable, "",
              "if not empty, use the specified executable instead of "
              "`tapa-fast-cosim`");
DEFINE_string(xosim_part_num, "",
              "if not empty, use the specified part number for Vivado");
DEFINE_bool(xosim_setup_only, false, "only setup the simulation");
DEFINE_bool(xosim_resume_from_post_sim, false,
            "skip simulation and do post-sim checking");

namespace fpga {
namespace internal {

namespace {

using clock = std::chrono::steady_clock;

std::string GetWorkDirectory() {
  if (!FLAGS_xosim_work_dir.empty()) {
    LOG_IF(INFO, fs::create_directories(FLAGS_xosim_work_dir))
        << "created work directory '" << FLAGS_xosim_work_dir << "'";
    return fs::absolute(FLAGS_xosim_work_dir).string();
  }
  std::string dir =
      (fs::temp_directory_path() / "tapa-fast-cosim.XXXXXX").string();
  LOG_IF(FATAL, ::mkdtemp(&dir[0]) == nullptr)
      << "failed to create work directory";
  return dir;
}

std::string GetInputDataPath(const std::string& work_dir, int index) {
  return work_dir + "/" + std::to_string(index) + ".bin";
}

std::string GetOutputDataPath(const std::string& work_dir, int index) {
  return work_dir + "/" + std::to_string(index) + "_out.bin";
}

std::string GetConfigPath(const std::string& work_dir) {
  return work_dir + "/config.json";
}

}  // namespace

struct TapaFastCosimDevice::Context {
  std::chrono::time_point<std::chrono::steady_clock> start_timestamp;
  subprocess::Popen proc;
};

TapaFastCosimDevice::TapaFastCosimDevice(std::string_view xo_path)
    : xo_path(fs::absolute(xo_path)), work_dir(GetWorkDirectory()) {
  miniz_cpp::zip_file xo_file = this->xo_path;
  std::string kernel_xml;
  for (auto& info : xo_file.infolist()) {
    constexpr std::string_view kSuffix = "/kernel.xml";
    if (info.filename.size() >= kSuffix.size() &&
        std::equal(kSuffix.rbegin(), kSuffix.rend(), info.filename.rbegin())) {
      kernel_xml = xo_file.read(info);
      break;
    }
  }
  LOG_IF(FATAL, kernel_xml.empty())
      << "Missing 'kernel.xml' in '" << xo_path << "'";

  tinyxml2::XMLDocument doc;
  doc.Parse(kernel_xml.data());
  for (const tinyxml2::XMLElement* xml_arg = doc.FirstChildElement("root")
                                                 ->FirstChildElement("kernel")
                                                 ->FirstChildElement("args")
                                                 ->FirstChildElement("arg");
       xml_arg != nullptr; xml_arg = xml_arg->NextSiblingElement("arg")) {
    ArgInfo arg;
    arg.index = atoi(xml_arg->Attribute("id"));
    LOG_IF(FATAL, arg.index != args_.size())
        << "Expecting argument #" << args_.size() << ", got argument #"
        << arg.index << " in the metadata";
    arg.name = xml_arg->Attribute("name");
    arg.type = xml_arg->Attribute("type");
    switch (int cat = atoi(xml_arg->Attribute("addressQualifier")); cat) {
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
    args_.push_back(arg);
  }

  LOG(INFO) << "Running hardware simulation with TAPA fast cosim";
}

TapaFastCosimDevice::~TapaFastCosimDevice() {
  if (FLAGS_xosim_work_dir.empty()) {
    fs::remove_all(work_dir);
  }
}

std::unique_ptr<Device> TapaFastCosimDevice::New(std::string_view path,
                                                 std::string_view content) {
  constexpr std::string_view kZipMagic("PK\3\4", 4);
  if (content.size() < kZipMagic.size() ||
      memcmp(content.data(), kZipMagic.data(), kZipMagic.size()) != 0) {
    return nullptr;
  }
  return std::make_unique<TapaFastCosimDevice>(path);
}

void TapaFastCosimDevice::SetScalarArg(int index, const void* arg, int size) {
  LOG_IF(FATAL, index >= args_.size())
      << "Cannot set argument #" << index << "; there are only " << args_.size()
      << " arguments";
  LOG_IF(FATAL, args_[index].cat != ArgInfo::kScalar)
      << "Cannot set argument '" << args_[index].name
      << "' as a scalar; it is a " << args_[index].cat;
  std::basic_string_view<unsigned char> arg_str(
      reinterpret_cast<const unsigned char*>(arg), size);
  std::stringstream ss;
  ss << "'h";
  // Assuming litten-endian.
  for (auto it = arg_str.crbegin(); it < arg_str.crend(); ++it) {
    ss << std::setfill('0') << std::setw(2) << std::hex << int(*it);
  }
  scalars_[index] = ss.str();
}

void TapaFastCosimDevice::SetBufferArg(int index, Tag tag,
                                       const BufferArg& arg) {
  LOG_IF(FATAL, index >= args_.size())
      << "Cannot set argument #" << index << "; there are only " << args_.size()
      << " arguments";
  LOG_IF(FATAL, args_[index].cat != ArgInfo::kMmap)
      << "Cannot set argument '" << args_[index].name
      << "' as an mmap; it is a " << args_[index].cat;
  buffer_table_.insert({index, arg});
  if (tag == Tag::kReadOnly || tag == Tag::kReadWrite) {
    store_indices_.insert(index);
  }
  if (tag == Tag::kWriteOnly || tag == Tag::kReadWrite) {
    load_indices_.insert(index);
  }
}

void TapaFastCosimDevice::SetStreamArg(int index, Tag tag, StreamArg& arg) {
  stream_table_[index] = arg.get<std::shared_ptr<SharedMemoryStream>>();
}

size_t TapaFastCosimDevice::SuspendBuffer(int index) {
  return load_indices_.erase(index) + store_indices_.erase(index);
}

void TapaFastCosimDevice::WriteToDevice() {
  is_write_to_device_scheduled_ = true;
}

void TapaFastCosimDevice::WriteToDeviceImpl() {
  // All buffers must have a data file.
  auto tic = clock::now();
  for (const auto& [index, buffer_arg] : buffer_table_) {
    std::ofstream(GetInputDataPath(work_dir, index),
                  std::ios::out | std::ios::binary)
        .write(buffer_arg.Get(), buffer_arg.SizeInBytes());
  }
  load_time_ = clock::now() - tic;
}

void TapaFastCosimDevice::ReadFromDevice() {
  is_read_from_device_scheduled_ = true;
}

void TapaFastCosimDevice::ReadFromDeviceImpl() {
  auto tic = clock::now();
  for (int index : store_indices_) {
    auto buffer_arg = buffer_table_.at(index);
    std::ifstream(GetOutputDataPath(work_dir, index),
                  std::ios::in | std::ios::binary)
        .read(buffer_arg.Get(), buffer_arg.SizeInBytes());
  }
  store_time_ = clock::now() - tic;
}

void TapaFastCosimDevice::Exec() {
  if (is_write_to_device_scheduled_) {
    WriteToDeviceImpl();
  }

  auto tic = clock::now();

  nlohmann::json json;
  json["xo_path"] = xo_path;

  nlohmann::json scalar_to_val = nlohmann::json::object();
  for (const auto& [index, scalar] : scalars_) {
    scalar_to_val[std::to_string(index)] = scalar;
  }
  json["scalar_to_val"] = std::move(scalar_to_val);

  nlohmann::json axi_to_c_array_size = nlohmann::json::object();
  nlohmann::json axi_to_data_file = nlohmann::json::object();
  for (const auto& [index, content] : buffer_table_) {
    axi_to_c_array_size[std::to_string(index)] = content.SizeInCount();
    axi_to_data_file[std::to_string(index)] = GetInputDataPath(work_dir, index);
  }
  json["axi_to_c_array_size"] = std::move(axi_to_c_array_size);
  json["axi_to_data_file"] = std::move(axi_to_data_file);

  nlohmann::json axis_to_data_file = nlohmann::json::object();
  for (const auto& [index, stream] : stream_table_) {
    VLOG(1) << "arg[" << index << "] is a stream backed by " << stream->path();
    axis_to_data_file[std::to_string(index)] = stream->path();
  }
  json["axis_to_data_file"] = std::move(axis_to_data_file);

  std::ofstream(GetConfigPath(work_dir)) << json.dump(2);

  std::vector<std::string> argv;
  if (FLAGS_xosim_executable.empty()) {
    argv = {"tapa-fast-cosim"};
  } else {
    argv = {FLAGS_xosim_executable};
  }
  argv.insert(argv.end(), {
                              "--config_path=" + GetConfigPath(work_dir),
                              "--tb_output_dir=" + work_dir + "/output",
                              "--launch_simulation",
                          });
  if (FLAGS_xosim_start_gui) {
    argv.push_back("--start_gui");
  }
  if (FLAGS_xosim_save_waveform) {
    argv.push_back("--save_waveform");
  }
  if (FLAGS_xosim_setup_only) {
    argv.push_back("--setup_only");
  }
  if (!FLAGS_xosim_part_num.empty()) {
    argv.push_back("--part_num=" + FLAGS_xosim_part_num);
  }

  // skip launch the simulation if resume from post sim
  if (FLAGS_xosim_resume_from_post_sim) {
    return;
  }

  context_ = std::make_unique<Context>(Context{
      .start_timestamp = tic,
      .proc = subprocess::Popen(argv,
                                subprocess::environment(xilinx::GetEnviron())),
  });
}

void TapaFastCosimDevice::Finish() {
  LOG_IF(FATAL, context_ == nullptr) << "Exec() must be called before Finish()";
  int rc = context_->proc.wait();

  LOG_IF(FATAL, rc != 0) << "TAPA fast cosim failed";

  // skip the rest of the function if only setup is needed
  if (FLAGS_xosim_setup_only) {
    exit(0);
  }

  compute_time_ = clock::now() - context_->start_timestamp;

  if (is_read_from_device_scheduled_) {
    ReadFromDeviceImpl();
  }
}

bool TapaFastCosimDevice::IsFinished() const {
  return context_ != nullptr && context_->proc.poll() >= 0;
}

std::vector<ArgInfo> TapaFastCosimDevice::GetArgsInfo() const { return args_; }

int64_t TapaFastCosimDevice::LoadTimeNanoSeconds() const {
  return load_time_.count();
}

int64_t TapaFastCosimDevice::ComputeTimeNanoSeconds() const {
  return compute_time_.count();
}

int64_t TapaFastCosimDevice::StoreTimeNanoSeconds() const {
  return store_time_.count();
}

size_t TapaFastCosimDevice::LoadBytes() const {
  size_t total_size = 0;
  for (auto& [index, buffer_arg] : buffer_table_) {
    total_size += buffer_arg.SizeInBytes();
  }
  return total_size;
}

size_t TapaFastCosimDevice::StoreBytes() const {
  size_t total_size = 0;
  for (int index : store_indices_) {
    auto buffer_arg = buffer_table_.at(index);
    total_size += buffer_arg.SizeInBytes();
  }
  return total_size;
}

}  // namespace internal
}  // namespace fpga
