#include <stdlib.h>

#include <frt.h>
#include <tlp.h>

void Jacobi(tlp::mmap<float> bank_0_t0, tlp::mmap<const float> bank_0_t1,
            uint64_t coalesced_data_num) {
  fpga::Invoke(
      getenv("BITSTREAM"),
      fpga::ReadOnly(static_cast<float*>(bank_0_t0), coalesced_data_num * 2),
      fpga::WriteOnly(static_cast<const float*>(bank_0_t1),
                      coalesced_data_num * 2),
      coalesced_data_num);
}
