// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include "frt/stringify.h"

#include <exception>

#include <gtest/gtest.h>

namespace {

TEST(StringifyTest, FloatToBinaryString) {
  EXPECT_EQ(fpga::ToBinaryString(1.f), "00111111100000000000000000000000");
}

TEST(StringifyTest, FloatFromBinaryString) {
  EXPECT_EQ(fpga::FromBinaryString<float>("00111111100000000000000000000000"),
            1.f);
  EXPECT_DEATH(fpga::FromBinaryString<float>("010101"), "size()");
  EXPECT_THROW(
      fpga::FromBinaryString<float>("12345678901234567890123456789012"),
      std::exception);
}

}  // namespace
