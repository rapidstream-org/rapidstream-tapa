// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include "frt/stringify.h"

#include <string_view>

#include <gtest/gtest.h>

namespace {

using namespace std::string_view_literals;

TEST(StringifyTest, FloatToBinaryString) {
  EXPECT_EQ(fpga::ToBinaryString(1.f), "\x00\x00\x80\x3f"sv);
}

TEST(StringifyTest, FloatFromBinaryString) {
  EXPECT_EQ(fpga::FromBinaryString<float>("\x00\x00\x80\x3f"sv), 1.f);
  EXPECT_DEATH(fpga::FromBinaryString<float>("010101"), "size()");
}

}  // namespace
