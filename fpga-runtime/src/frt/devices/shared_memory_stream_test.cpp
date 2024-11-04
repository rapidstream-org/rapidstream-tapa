// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include "frt/devices/shared_memory_stream.h"

#include <string>

#include <gtest/gtest.h>

#include "frt/devices/shared_memory_queue.h"

namespace fpga::internal {
namespace {

constexpr int kDepth = 2;
constexpr int kWidth = 3;

TEST(SharedMemoryStreamTest, CreateStreamSucceeds) {
  SharedMemoryStream stream({
      .depth = kDepth,
      .width = kWidth,
  });
  const std::string val = "val";

  SharedMemoryQueue* queue = stream.queue();
  ASSERT_NE(queue, nullptr);

  queue->push(val);
  EXPECT_EQ(queue->pop(), val);
}

TEST(SharedMemoryStreamTest, CreateStreamFailsWithInvalidPathTemplate) {
  SharedMemoryStream stream({
      .depth = kDepth,
      .width = kWidth,
      .path_template = "/invalid path",
  });

  SharedMemoryQueue* queue = stream.queue();

  EXPECT_EQ(queue, nullptr);
}

}  // namespace
}  // namespace fpga::internal
