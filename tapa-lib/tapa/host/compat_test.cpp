// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include "tapa/host/compat.h"

#include <gtest/gtest.h>

#include "tapa/host/task.h"

// WARNING: These test cases are not synthesizable.

namespace {

constexpr int kN = 5000;
constexpr int kDepth = 5;

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

void DataSourceNonBlocking(
    tapa::hls_compat::stream_interface<float>& data_out_q,
    tapa::hls_compat::stream_interface<int>& count_out_q, int n) {
  int write_count = 0;
  for (int i = 0; i < n; ++i) {
    if (data_out_q.try_write(i)) {
      ++write_count;
    }
  }
  count_out_q.write(write_count);
}

void DataSinkNonBlocking(tapa::hls_compat::stream_interface<float>& data_in_q,
                         tapa::hls_compat::stream_interface<int>& count_in_q,
                         int n) {
  int read_count = 0;
  for (int i = 0; i < n; ++i) {
    float value_unused;
    if (data_in_q.try_read(value_unused)) {
      ++read_count;
    }
  }

  // Since `tapa::hls_compat:task()` schedules tasks sequentially, we should
  // only be able to read `kDepth` elements.
  EXPECT_EQ(read_count, kDepth);

  // `DataSourceNonBlocking` should have finished here so we expect to read the
  // count successfully.
  int write_count = -1;
  EXPECT_TRUE(count_in_q.try_read(write_count));

  // The `write_count` should be the same as `read_count`.
  EXPECT_EQ(write_count, read_count);
}

TEST(CompatTest, CompatHlsTasksAreScheduledSequentially) {
  tapa::stream<float, kDepth> data_q;
  tapa::hls_compat::stream<int> count_q;
  tapa::hls_compat::task()
      .invoke(DataSourceNonBlocking, data_q, count_q, kN)
      .invoke(DataSinkNonBlocking, data_q, count_q, kN);
}

}  // namespace
