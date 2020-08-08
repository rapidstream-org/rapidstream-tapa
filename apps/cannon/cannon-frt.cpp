#include <stdlib.h>

#include <frt.h>
#include <tapa.h>

void Cannon(tapa::mmap<const float> a, tapa::mmap<const float> b,
            tapa::mmap<float> c, uint64_t n) {
  fpga::Invoke(getenv("BITSTREAM"),
               fpga::WriteOnly(static_cast<const float*>(a), n * n),
               fpga::WriteOnly(static_cast<const float*>(b), n * n),
               fpga::ReadOnly(static_cast<float*>(c), n * n), n);
}
