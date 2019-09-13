#include <stdlib.h>

#include <frt.h>
#include <tlp.h>

void Cannon(tlp::mmap<const float> a, tlp::mmap<const float> b,
            tlp::mmap<float> c, uint64_t n) {
  fpga::Invoke(getenv("BITSTREAM"),
               fpga::WriteOnly(static_cast<const float*>(a), n * n),
               fpga::WriteOnly(static_cast<const float*>(b), n * n),
               fpga::ReadOnly(static_cast<float*>(c), n * n), n);
}
