// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include "frt/stringify.h"

#include <string_view>

#include <gtest/gtest.h>

namespace fpga::internal {
namespace {

using namespace std::string_view_literals;

TEST(StringifyTest, FloatToBinaryString) {
  EXPECT_EQ(ToBinaryString(1.f), "\x00\x00\x80\x3f"sv);
}

TEST(StringifyTest, FloatFromBinaryString) {
  EXPECT_EQ(FromBinaryString<float>("\x00\x00\x80\x3f"sv), 1.f);
  EXPECT_DEATH(FromBinaryString<float>("010101"), "size()");
}

}  // namespace
}  // namespace fpga::internal
