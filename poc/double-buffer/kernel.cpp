#include "tapa/buffer.h"

using Data = double;

constexpr int N = 1000;

void Top(tapa::buffer<Data, N, 2>& foo_buf, tapa::buffer<Data, N, 3>& bar_buf) {
#pragma HLS disaggregate variable = foo_buf
#pragma HLS interface ap_fifo port = foo_buf.src
#pragma HLS aggregate variable = foo_buf.src
#pragma HLS interface ap_fifo port = foo_buf.sink
#pragma HLS aggregate variable = foo_buf.sink
#pragma HLS interface ap_memory port = foo_buf.data
#pragma HLS aggregate variable = foo_buf.data
#pragma HLS array_partition variable = foo_buf.data complete dim = 1

#pragma HLS disaggregate variable = bar_buf
#pragma HLS interface ap_fifo port = bar_buf.src
#pragma HLS aggregate variable = bar_buf.src
#pragma HLS interface ap_fifo port = bar_buf.sink
#pragma HLS aggregate variable = bar_buf.sink
#pragma HLS interface ap_memory port = bar_buf.data
#pragma HLS aggregate variable = bar_buf.data
#pragma HLS array_partition variable = bar_buf.data complete dim = 1

outer:
  for (int j = 0; j < 50; ++j) {
    auto foo = foo_buf.acquire();
    tapa::partition<Data, N, 3> bar = bar_buf.acquire();
  inner:
    for (int i = 0; i < N; ++i) {
#pragma HLS pipeline II = 1
      bar[N - 1 - i] = foo[i] * foo[i];
    }
  }
}
