#include <stdlib.h>

#include <frt.h>

void VecAdd(const float* a_array, const float* b_array, float* c_array,
            uint64_t n) {
  fpga::Invoke(getenv("BITSTREAM"), fpga::WriteOnly(a_array, n),
               fpga::WriteOnly(b_array, n), fpga::ReadOnly(c_array, n), n);
}
