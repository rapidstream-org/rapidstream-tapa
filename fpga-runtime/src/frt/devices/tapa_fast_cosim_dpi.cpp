// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include <sstream>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

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
    int fd = shm_open(stream_path.c_str(), O_RDWR, 0600);
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
  CHECK_EQ(istream->width(), size_t(svSize(dout, 1)));

  static std::unordered_map<std::string, bool> last_empty_n;

  if (last_empty_n[id] && read == sv_1) {
    // If we provided data in the last cycle, and the downstream consumed it,
    // we need to pop that data in this cycle.
    CHECK(!istream->empty());
    istream->pop();
  }

  if (istream->empty()) {
    // If we are empty in this cycle, we do not provide data.
    StringToOpenArrayHandle(std::string(istream->width(), 'x'), dout);
    empty_n = sv_0;
    last_empty_n[id] = false;

    // If there is no data to be consumed from the DPI queue, we yield to
    // the operating system to allow the TAPA processes or other simulation
    // processes to produce data to write to the queue.
    sleep(0);
  } else {
    // Otherwise, we provide data and tell the downstream we are not empty.
    StringToOpenArrayHandle(istream->front(), dout);
    empty_n = sv_1;
    last_empty_n[id] = true;
  }
}

DPI_DLLESPEC void ostream(
    /* input */ svOpenArrayHandle din,
    /* output */ svLogic& full_n,
    /* input */ svLogic write,
    /* input */ const char* id) {
  SharedMemoryQueue* ostream = GetStream(id);
  CHECK(ostream != nullptr);
  CHECK_EQ(ostream->width(), size_t(svSize(din, 1)));

  static std::unordered_map<std::string, bool> last_full_n;

  if (ostream->full()) {
    // In the previous cycle we should have indicated that we are full, or this
    // is the first cycle of the simulation. Otherwise, we have to consume data
    // in this cycle which is not possible.
    CHECK(last_full_n.find(id) != last_full_n.end() ||
          last_full_n[id] == false);

    // No data can be read in the next cycle because we are full
    full_n = sv_0;
    last_full_n[id] = false;

    // If the DPI queue is full, we yield to the operating system to allow
    // the TAPA processes or other simulation processes to consume data from
    // the queue.
    sleep(0);
  } else {
    // If in the *previous* cycle we have indicated that we are not full, we
    // shall consume data in this cycle if it is available.
    if (last_full_n.find(id) != last_full_n.end() && last_full_n[id] == true &&
        write == sv_1) {
      const std::string bits = OpenArrayHandleToString(din);
      ostream->push(bits);
    }

    // If we are still not full after the consumption, we can accept data in
    // the next cycle.
    bool is_full = ostream->full();
    full_n = is_full ? sv_0 : sv_1;
    last_full_n[id] = is_full ? false : true;
  }
}

}  // extern "C"
