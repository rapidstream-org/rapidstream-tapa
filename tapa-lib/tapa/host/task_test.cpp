// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include "tapa/host/task.h"

#include <thread>

#include <gtest/gtest.h>

#include "tapa.h"

namespace tapa {
namespace {

constexpr int kN = 5000;

void DataSource(tapa::ostream<int>& data_out_q, int n) {
  for (int i = 0; i < n; ++i) {
    data_out_q.write(i);
  }
}

template <typename T>
void DataSinkTemplated(tapa::istream<T>& data_in_q, int n) {
  for (int i = 0; i < n; ++i) {
    data_in_q.read();
  }
}

void DataSink(tapa::istream<int>& data_in_q, int n) {
  DataSinkTemplated(data_in_q, n);
}

// WARNING: This is not synthesizable.
TEST(TaskTest, YieldingWithoutCoroutineWorks) {
  tapa::stream<int, 2> data_q;
  std::thread t1(
      DataSource,
      std::ref(tapa::internal::template accessor<
               tapa::ostream<int>&, tapa::stream<int, 2>&>::access(data_q)),
      kN);
  std::thread t2(
      DataSink,
      std::ref(tapa::internal::template accessor<
               tapa::istream<int>&, tapa::stream<int, 2>&>::access(data_q)),
      kN);
  t1.join();
  t2.join();
}

// WARNING: This is not synthesizable.
TEST(TaskTest, DirectInvocationMixedWithTapaInvocationWorks) {
  tapa::stream<int, kN> data_q;
  DataSource(data_q, kN);
  tapa::task().invoke(DataSink, data_q, kN);
}

// WARNING: This is not synthesizable (yet).
TEST(TaskTest, InvokingTemplatedTaskWorks) {
  tapa::stream<int, kN> data_q;
  tapa::task()
      .invoke(DataSinkTemplated<int>, data_q, kN)
      .invoke(DataSource, data_q, kN);
}

}  // namespace
}  // namespace tapa
