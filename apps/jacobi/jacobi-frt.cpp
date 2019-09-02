#include <stdlib.h>

#include <frt.h>

void Jacobi(float* bank_0_t0, float* bank_0_t1, uint64_t coalesced_data_num) {
  fpga::Invoke(
      getenv("BITSTREAM"), fpga::ReadOnly(bank_0_t0, coalesced_data_num * 2),
      fpga::WriteOnly(bank_0_t1, coalesced_data_num * 2), coalesced_data_num);
}
