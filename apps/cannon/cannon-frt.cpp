#include <stdlib.h>

#include <frt.h>

void Cannon(const float* a, const float* b, float* c, uint64_t n) {
  fpga::Invoke(getenv("BITSTREAM"), fpga::WriteOnly(a, n * n),
               fpga::WriteOnly(b, n * n), fpga::ReadOnly(c, n * n), n);
}
