// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include <cstdint>
#include <iomanip>

#include <tapa.h>

#include "bandwidth.h"

void Copy(tapa::async_mmap<Elem>& mem, uint64_t n, uint64_t flags) {
  const bool read = flags & kRead;
  const bool write = flags & kWrite;

  if (!read && !write) return;

  Elem elem;

  for (uint64_t i_rd_req = 0, i_rd_resp = 0, i_wr_req = 0, i_wr_resp = 0;
       write ? (i_wr_resp < n) : (i_rd_resp < n);) {
#pragma HLS pipeline II = 1

    bool can_read = !mem.read_data.empty();
    bool can_write = !mem.write_addr.full() && !mem.write_data.full();
    int64_t read_addr = i_rd_req;
    int64_t write_addr = i_wr_req;

    // issue read requests to read_addr channel
    if (read && i_rd_req < n && mem.read_addr.try_write(read_addr)) {
      ++i_rd_req;
      VLOG(3) << "RD REQ [" << std::setw(5) << read_addr << "]";
    }

    // receive read responses when ready to write
    // we have a read-after-write dependency, so we need to check the write
    // conditions as well
    if (read && can_read && (!write || can_write)) {
      mem.read_data.try_read(elem);
      ++i_rd_resp;
      VLOG(3) << "RD RSP #" << std::setw(5) << i_rd_resp - 1;
    }

    // issue write requests, write out the element we just receive from the
    // read_data channel
    if (((read && can_read && write) || (!read && i_wr_req < n)) && can_write) {
      mem.write_addr.write(write_addr);
      mem.write_data.write(elem);
      ++i_wr_req;
      VLOG(3) << "WR REQ [" << std::setw(5) << write_addr << "]";
    }

    // receive write responses
    if (write && !mem.write_resp.empty()) {
      i_wr_resp += mem.write_resp.read(nullptr) + 1;
      VLOG(3) << "WR RSP #" << std::setw(5) << i_wr_resp - 1;
    }
  }
}

void Bandwidth(tapa::mmaps<Elem, kBankCount> chan, uint64_t n, uint64_t flags) {
  tapa::task().invoke<tapa::join, kBankCount>(Copy, chan, n, flags);
}
