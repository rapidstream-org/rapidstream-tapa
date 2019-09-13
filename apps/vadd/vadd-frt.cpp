#include <stdlib.h>

#include <frt.h>
#include <tlp.h>

void VecAdd(tlp::mmap<const float> a_array, tlp::mmap<const float> b_array,
            tlp::mmap<float> c_array, uint64_t n) {
  fpga::Invoke(getenv("BITSTREAM"),
               fpga::WriteOnly(static_cast<const float*>(a_array), n),
               fpga::WriteOnly(static_cast<const float*>(b_array), n),
               fpga::ReadOnly(static_cast<float*>(c_array), n), n);
}
