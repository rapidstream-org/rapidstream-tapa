#include <cstdint>

#include <tapa.h>

#include "bandwidth.h"

void Copy(tapa::async_mmap<Elem> mem, uint64_t n, uint64_t flags) {
  const bool random = flags & kRandom;
  const bool read = flags & kRead;
  const bool write = flags & kWrite;

  if (!read && !write) return;

  uint16_t mask = 0xffffu;
  for (int i = 16; i > 0; --i) {
#pragma HLS unroll
    if (n < (1ULL << i)) {
      mask >>= 1;
    }
  }

  uint16_t lfsr_rd = 0xbeefu;
  uint16_t lfsr_wr = 0xbeefu;
  bool valid = false;
  bool addr_ready = false;
  bool data_ready = false;
  Elem elem;

  [[tapa::pipeline(1)]]
  for (uint64_t i_rd_req = 0, i_rd_resp = 0, i_wr_req = 0, i_wr_resp = 0;
       write ? (i_wr_resp < n) : (i_rd_resp < n);) {

    if (read && i_rd_req < i_rd_resp + kEstimatedLatency && i_rd_req < n) {
      auto addr = random ? uint64_t(lfsr_rd & mask) : i_rd_req;
      mem.read_addr.try_write(addr);

      ++i_rd_req;
      uint16_t bit =
          (lfsr_rd >> 0) ^ (lfsr_rd >> 2) ^ (lfsr_rd >> 3) ^ (lfsr_rd >> 5);
      lfsr_rd = (lfsr_rd >> 1) | (bit << 15);
    }

    if ((!write || !valid) && mem.read_data.try_read(elem)) {
      valid = true;
      ++i_rd_resp;
    }

    if (write && !addr_ready && i_wr_req < n) {
      auto addr = random ? uint64_t(lfsr_wr & mask) : i_wr_req;
      addr_ready = mem.write_addr.try_write(addr);
    }

    if (write && (!read || valid) && !data_ready && i_wr_req < n) {
      data_ready = mem.write_data.try_write(elem);
    }

    if (write && !mem.write_resp.empty()) {
      i_wr_resp += mem.write_resp.read(nullptr) + 1;
    }

    if (addr_ready && data_ready) {
      valid = false;
      addr_ready = false;
      data_ready = false;
      ++i_wr_req;
      uint16_t bit =
          (lfsr_wr >> 0) ^ (lfsr_wr >> 2) ^ (lfsr_wr >> 3) ^ (lfsr_wr >> 5);
      lfsr_wr = (lfsr_wr >> 1) | (bit << 15);
    }
  }
}

void Bandwidth(tapa::mmaps<Elem, kBankCount> chan, uint64_t n, uint64_t flags) {
  tapa::task().invoke<tapa::join, kBankCount>(Copy, chan, n, flags);
}
