#include <stdlib.h>

#include <frt.h>
#include <tlp.h>

#include "bandwidth.h"

void Bandwidth(tlp::async_mmap<Elem> chan0, tlp::async_mmap<Elem> chan1,
               tlp::async_mmap<Elem> chan2, tlp::async_mmap<Elem> chan3,
               uint64_t n) {
  auto instance = fpga::Invoke(getenv("BITSTREAM"),
                               fpga::ReadWrite(chan0.get(), chan0.size()),
                               fpga::ReadWrite(chan1.get(), chan1.size()),
                               fpga::ReadWrite(chan2.get(), chan2.size()),
                               fpga::ReadWrite(chan3.get(), chan3.size()), n);
  LOG(INFO) << "kernel time: " << instance.ComputeTimeSeconds() * 1e3 << " ms";
  LOG(INFO) << "throughput: "
            << double(chan0.size() + chan1.size() + chan2.size() +
                      chan3.size()) *
                   2 * 64 / instance.ComputeTimeNanoSeconds()
            << " GB/s";
}
