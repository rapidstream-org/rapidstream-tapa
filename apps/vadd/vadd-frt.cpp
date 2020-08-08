#include <stdlib.h>

#include <frt.h>
#include <tapa.h>

void VecAdd(tapa::mmap<const float> a_array, tapa::mmap<const float> b_array,
            tapa::mmap<float> c_array, uint64_t n) {
  fpga::Invoke(getenv("BITSTREAM"),
               fpga::WriteOnly(static_cast<const float*>(a_array), n),
               fpga::WriteOnly(static_cast<const float*>(b_array), n),
               fpga::ReadOnly(static_cast<float*>(c_array), n), n);
}
