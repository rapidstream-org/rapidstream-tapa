// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include <sstream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include <fcntl.h>

#include <glog/logging.h>
#include <svdpi.h>

#include "frt/devices/shared_memory_queue.h"

namespace {

using ::fpga::internal::SharedMemoryQueue;

std::unordered_map<std::string, SharedMemoryQueue::UniquePtr> InitStreamMap() {
  const char* env = std::getenv("TAPA_FAST_COSIM_DPI_ARGS");
  CHECK(env != nullptr) << "Please set `TAPA_FAST_COSIM_DPI_ARGS`";
  VLOG(1) << "TAPA_FAST_COSIM_DPI_ARGS: " << env;

  std::vector<std::tuple<std::string, std::string>> stream_id_and_path;
  {
    std::istringstream ss(env);
    std::string stream_entry;  // `id:path`
    while (std::getline(ss, stream_entry, /*delimiter=*/',')) {
      const std::string::size_type pos = stream_entry.find(':');
      CHECK_NE(pos, std::string::npos) << stream_entry;
      stream_id_and_path.push_back({
          stream_entry.substr(0, pos),
          stream_entry.substr(pos + 1),
      });
    }
  }

  std::unordered_map<std::string, SharedMemoryQueue::UniquePtr> streams;
  for (const std::tuple<std::string, std::string>& entry : stream_id_and_path) {
    const std::string& stream_id = std::get<0>(entry);
    const std::string& stream_path = std::get<1>(entry);
    int fd = open(stream_path.c_str(), O_RDWR | O_CLOEXEC);
    VLOG(2) << "fd: " << fd << " <=> arg: " << stream_id;
    streams[stream_id] = SharedMemoryQueue::New(fd);
  }
  return streams;
}

SharedMemoryQueue* GetStream(const char* id) {
  if (id == nullptr) {
    LOG(ERROR) << "stream id is nullptr";
    return nullptr;
  }
  static std::unordered_map<std::string, SharedMemoryQueue::UniquePtr> streams =
      InitStreamMap();
  auto it = streams.find(id);
  if (it == streams.end()) {
    return nullptr;
  }
  return it->second.get();
}

void StringToOpenArrayHandle(const std::string& bits,
                             svOpenArrayHandle handle) {
  const int increment = svIncrement(handle, 1);
  int index = svLeft(handle, 1);
  for (const char c : bits) {
    svLogic bit = sv_x;
    switch (c) {
      case '0':
        bit = sv_0;
        break;
      case '1':
        bit = sv_1;
        break;
      case 'z':
      case 'Z':
        bit = sv_z;
        break;
      case 'x':
      case 'X':
        bit = sv_x;
        break;
      case '\'':
      case '_':
        continue;
      default:
        LOG(FATAL) << "unexpected bit character: " << c << " (" << int(c)
                   << ")";
    }
    svPutLogicArrElem(handle, bit, index);
    index -= increment;
  }
  CHECK_EQ(index, svRight(handle, 1) - increment);
}

std::string OpenArrayHandleToString(svOpenArrayHandle handle) {
  std::string bits;
  bits.reserve(svSize(handle, 1));
  const int increment = svIncrement(handle, 1);
  for (int index = svLeft(handle, 1); index != svRight(handle, 1) - increment;
       index -= increment) {
    bits.push_back([&] {
      const svLogic bit = svGetLogicArrElem(handle, index);
      switch (bit) {
        case sv_0:
          return '0';
        case sv_1:
          return '1';
        case sv_x:
          return 'x';
        case sv_z:
          return 'z';
        default:
          LOG(FATAL) << "unexpected bit enum: " << int(bit);
          return 'x';
      }
    }());
  }
  return bits;
}

}  // namespace

extern "C" {

DPI_DLLESPEC void istream(
    /* output */ svOpenArrayHandle dout,
    /* output */ svLogic& empty_n,
    /* input */ svLogic read,
    /* input */ const char* id) {
  SharedMemoryQueue* istream = GetStream(id);
  CHECK(istream != nullptr);
  CHECK_EQ(istream->width(), svSize(dout, 1));
  empty_n = istream->empty() ? sv_0 : sv_1;
  std::string bits(istream->width(), 'x');
  if (!istream->empty()) {
    bits = istream->front();
  }
  StringToOpenArrayHandle(bits, dout);
  if (read == sv_1 && !istream->empty()) {
    istream->pop();
  }
}

DPI_DLLESPEC void ostream(
    /* input */ svOpenArrayHandle din,
    /* output */ svLogic& full_n,
    /* input */ svLogic write,
    /* input */ const char* id) {
  SharedMemoryQueue* ostream = GetStream(id);
  CHECK(ostream != nullptr);
  CHECK_EQ(ostream->width(), svSize(din, 1));
  if (write == sv_1) {
    CHECK(!ostream->full());
    const std::string bits = OpenArrayHandleToString(din);
    ostream->push(bits);
  }
  full_n = (ostream->size() < ostream->capacity()) ? sv_1 : sv_0;
}

}  // extern "C"
