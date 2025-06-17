// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include "frt/buffer.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <vector>

#include <gtest/gtest-death-test.h>
#include <gtest/gtest.h>

#include "frt.h"
#include "frt/tag.h"

namespace fpga::internal {
namespace {

class BufferTest : public testing::Test {
 protected:
  void SetUp() override { elements_.resize(10); }

  std::vector<float> elements_;
};

TEST_F(BufferTest, SizeReturnsElementCount) {
  auto buf = fpga::Placeholder(elements_.data(), elements_.size());

  EXPECT_EQ(buf.Size(), elements_.size());
}

TEST_F(BufferTest, SizeInBytesReturnsByteCount) {
  auto buf = fpga::ReadOnly(elements_.data(), elements_.size());

  EXPECT_EQ(buf.SizeInBytes(), elements_.size() * sizeof(elements_[0]));
}

TEST_F(BufferTest, ReinterpretAsWiderTypeSucceeds) {
  float* ptr = elements_.data();
  int64_t size = elements_.size();
  if (reinterpret_cast<uintptr_t>(ptr) % 2 != 0) {
    // Force alignment if necessary.
    ++ptr;
    size -= 2;
    ASSERT_GT(size, 0);
    ASSERT_EQ(size % 2, 0);
  }

  auto buf = fpga::WriteOnly(ptr, size).Reinterpret<double>();

  EXPECT_EQ(reinterpret_cast<uintptr_t>(buf.Get()),
            reinterpret_cast<uintptr_t>(ptr));
  EXPECT_EQ(buf.Size(), size / 2);
}

TEST_F(BufferTest, ReinterpretAsNarrowerTypeSucceeds) {
  auto buf1 = fpga::ReadWrite(elements_.data(), elements_.size());

  auto buf2 = buf1.Reinterpret<uint16_t>();

  EXPECT_EQ(reinterpret_cast<uintptr_t>(buf1.Get()),
            reinterpret_cast<uintptr_t>(buf2.Get()));
  EXPECT_EQ(buf1.SizeInBytes(), buf2.SizeInBytes());
}

TEST_F(BufferTest, ReinterpretAsSameSizeTypeSucceeds) {
  auto buf1 = fpga::Placeholder(elements_.data(), elements_.size());

  auto buf2 = buf1.Reinterpret<int>();

  EXPECT_EQ(reinterpret_cast<uintptr_t>(buf1.Get()),
            reinterpret_cast<uintptr_t>(buf2.Get()));
  EXPECT_EQ(buf1.SizeInBytes(), buf2.SizeInBytes());
}

TEST_F(BufferTest, ReinterpretAsWiderTypeWithUnalignedTypeFails) {
  auto buf = fpga::ReadOnly(elements_.data(), elements_.size());

  EXPECT_DEATH((buf.Reinterpret<std::array<char, 5>>()),
               "sizeof\\(U\\) must be a multiple of sizeof\\(T\\)");
}

TEST_F(BufferTest, ReinterpretAsWiderTypeWithUnalignedSizeFails) {
  auto buf = fpga::WriteOnly(elements_.data(), elements_.size() - 1);

  EXPECT_DEATH(buf.Reinterpret<double>(),
               "size of Buffer<T> must be a multiple of N "
               "\\(= sizeof\\(U\\)/sizeof\\(T\\)\\)");
}

TEST_F(BufferTest, ReinterpretAsNarrowerTypeWithUnalignedTypeFails) {
  auto buf = fpga::ReadWrite(elements_.data(), elements_.size());

  EXPECT_DEATH((buf.Reinterpret<std::array<char, 3>>()),
               "sizeof\\(T\\) must be a multiple of sizeof\\(U\\)");
}

TEST_F(BufferTest, ReinterpretAsWiderTypeWithUnalignedDataFails) {
  float* ptr = elements_.data();
  int64_t size = elements_.size();
  if (reinterpret_cast<uintptr_t>(ptr) % 2 == 0) {
    // Force alignment if necessary.
    ++ptr;
    size -= 2;
    ASSERT_GT(size, 0);
    ASSERT_EQ(size % 2, 0);
  }

  EXPECT_DEATH(fpga::Placeholder(ptr, size).Reinterpret<double>(),
               "data of Buffer<T> must be 8-byte aligned");
}

}  // namespace
}  // namespace fpga::internal
