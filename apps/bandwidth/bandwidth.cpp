#include <cstdint>
#include <iomanip>

#include <tapa.h>

#include "bandwidth.h"
#include "lfsr.h"

void Copy(tapa::async_mmap<Elem>& mem, uint64_t n, uint64_t flags) {
  const bool random = flags & kRandom;
  const bool read = flags & kRead;
  const bool write = flags & kWrite;

  if (!read && !write) return;

  uint16_t mask = 0xffffu;
  [[tapa::unroll]]  //
  for (int i = 16; i > 0; --i) {
    if (n < (1ULL << i)) {
      mask >>= 1;
    }
  }

  Lfsr<16> lfsr_rd = 0xbeefu;
  Lfsr<16> lfsr_wr = 0xbeefu;
  Elem elem;

  [[tapa::pipeline(1)]]  //
  for (uint64_t i_rd_req = 0, i_rd_resp = 0, i_wr_req = 0, i_wr_resp = 0;
       write ? (i_wr_resp < n) : (i_rd_resp < n);) {
    bool can_read = !mem.read_data.empty();
    bool can_write = !mem.write_addr.full() && !mem.write_data.full();
    int64_t read_addr = random ? uint64_t(lfsr_rd & mask) : i_rd_req;
    int64_t write_addr = random ? uint64_t(lfsr_wr & mask) : i_wr_req;

    if (read && i_rd_req < n && mem.read_addr.try_write(read_addr)) {
      ++i_rd_req;
      ++lfsr_rd;
      VLOG(3) << "RD REQ [" << std::setw(5) << read_addr << "]";
    }

    if (read && can_read && (!write || can_write)) {
      mem.read_data.try_read(elem);
      ++i_rd_resp;
      VLOG(3) << "RD RSP #" << std::setw(5) << i_rd_resp - 1;
    }

    if (((read && can_read && write) || (!read && i_wr_req < n)) && can_write) {
      mem.write_addr.write(write_addr);
      mem.write_data.write(elem);
      ++i_wr_req;
      ++lfsr_wr;
      VLOG(3) << "WR REQ [" << std::setw(5) << write_addr << "]";
    }

    if (write && !mem.write_resp.empty()) {
      i_wr_resp += mem.write_resp.read(nullptr) + 1;
      VLOG(3) << "WR RSP #" << std::setw(5) << i_wr_resp - 1;
    }
  }
}

void Bandwidth(tapa::mmaps<Elem, kBankCount> chan, uint64_t n, uint64_t flags) {
  tapa::task().invoke<tapa::join, kBankCount>(Copy, chan, n, flags);
}
