// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include "frt/devices/shared_memory_queue.h"

#include <sys/mman.h>

#include <glog/logging.h>
#include <gtest/gtest.h>

namespace fpga::internal {
namespace {

constexpr int kDepth = 2;
constexpr int kWidth = 3;

class SharedMemoryQueueTest : public testing::Test {
 protected:
  ~SharedMemoryQueueTest() override {
    if (fd_ >= 0) {
      PLOG_IF(WARNING, close(fd_) != 0) << "close";
      fd_ = -1;
      PLOG_IF(ERROR, shm_unlink(temp_file_.c_str())) << "shm_unlink";
    }
  }

  const testing::TestInfo* const test_info_ =
      testing::UnitTest::GetInstance()->current_test_info();
  std::string temp_file_ = "/shared_memory_queue.XXXXXX";
  int fd_ = SharedMemoryQueue::CreateFile(temp_file_, kDepth, kWidth);
  SharedMemoryQueue::UniquePtr queue_ = SharedMemoryQueue::New(fd_);
};

TEST_F(SharedMemoryQueueTest, PushAndPopSucceeds) {
  const std::string val = "val";

  queue_->push(val);
  EXPECT_EQ(queue_->pop(), val);
}

TEST_F(SharedMemoryQueueTest, PushFailsWithInvalidInput) {
  EXPECT_DEATH(queue_->push("too long"), "unexpected input");
}

TEST_F(SharedMemoryQueueTest, PushFailsWhenFull) {
  queue_->push("val");
  queue_->push("val");

  EXPECT_TRUE(queue_->full());
  EXPECT_DEATH(queue_->push("val"), "full");
}

TEST_F(SharedMemoryQueueTest, PopFailsWhenEmpty) {
  EXPECT_TRUE(queue_->empty());
  EXPECT_DEATH(queue_->pop(), "empty");
}

TEST_F(SharedMemoryQueueTest, GettersSucceed) {
  EXPECT_EQ(queue_->width(), kWidth);
  EXPECT_EQ(queue_->capacity(), kDepth);
  EXPECT_EQ(queue_->size(), 0);

  queue_->push("val");
  EXPECT_EQ(queue_->size(), 1);
}

}  // namespace
}  // namespace fpga::internal
