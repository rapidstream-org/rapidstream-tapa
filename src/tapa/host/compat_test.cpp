
#include "tapa/host/compat.h"

#include <gtest/gtest.h>

#include "tapa/host/task.h"

// WARNING: These test cases are not synthesizable.

namespace {

constexpr int kN = 5000;

void DataSink(tapa::hls_compat::stream<int>& data_in_q, int n) {
  for (int i = 0; i < n; ++i) {
    data_in_q.read();
  }
}

void DataSource(tapa::hls_compat::stream<int>& data_out_q, int n) {
  for (int i = 0; i < n; ++i) {
    data_out_q.write(i);
  }
}

TEST(CompatTest, CompatHlsStreamHasInfiniteDepth) {
  tapa::hls_compat::stream<int> data_q;
  DataSource(data_q, 10'000'000);
}

TEST(CompatTest, CompatHlsStreamCanBeNamed) {
  tapa::hls_compat::stream<int> data_q("data");
  DataSource(data_q, kN);
  DataSink(data_q, kN);
}

TEST(CompatTest, CompatHlsStreamCanBeInvoked) {
  tapa::hls_compat::stream<int> data_q("data");
  tapa::task().invoke(DataSource, data_q, kN).invoke(DataSink, data_q, kN);
}

}  // namespace
