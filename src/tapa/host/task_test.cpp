#include <thread>

#include <gtest/gtest.h>

#include "tapa/host/stream.h"

namespace tapa {
namespace {

void DataSource(tapa::ostream<int> data_out_q, int n) {
  for (int i = 0; i < n; ++i) {
    data_out_q.write(i);
  }
}

void DataSink(tapa::istream<int> data_in_q, int n) {
  for (int i = 0; i < n; ++i) {
    data_in_q.read();
  }
}

TEST(TaskTest, YieldingWithoutCoroutineWorks) {
  constexpr int kN = 5000;
  tapa::stream<int, 2> data_q;
  std::thread t1(DataSource, data_q, kN);
  std::thread t2(DataSink, data_q, kN);
  t1.join();
  t2.join();
}

}  // namespace
}  // namespace tapa
