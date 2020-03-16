#include <cstdint>

#include <tlp.h>

#include "bandwidth.h"

void Copy(tlp::async_mmap<Elem> mem, uint64_t n) {
  for (uint64_t i_rd = 0, i_wr = 0; i_wr < n;) {
#pragma HLS pipeline II = 1
    if (i_rd < i_wr + kEstimatedLatency && i_rd < n) {
      mem.read_addr_try_write(i_rd);
      ++i_rd;
    }
    Elem vec;
    if (mem.read_data_try_read(vec)) {
      mem.write_addr_write(i_wr);
      mem.write_data_write(vec);
      ++i_wr;
    }
  }
}

void Bandwidth(tlp::async_mmaps<Elem, kBankCount> chan, uint64_t n) {
  tlp::task()
      .invoke<0>(Copy, chan[0], n)
      .invoke<0>(Copy, chan[1], n)
      .invoke<0>(Copy, chan[2], n)
      .invoke<0>(Copy, chan[3], n);
}