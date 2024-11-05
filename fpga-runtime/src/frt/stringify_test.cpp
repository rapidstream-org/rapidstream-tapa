// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include "frt/stringify.h"

#include <exception>

#include <gtest/gtest.h>

namespace custom {

struct Struct {
  bool foo;
  float bar;
};

// Customized `ToBinaryStringImpl` and `FromBinaryStringImpl` must be defined in
// the same namespace as the type.
std::string ToBinaryStringImpl(const Struct* val) {
  return fpga::ToBinaryString(val->foo) + fpga::ToBinaryString(val->bar);
}
void FromBinaryStringImpl(std::string_view str, Struct& val) {
  val.foo = fpga::FromBinaryString<bool>(str.substr(0, 1));
  val.bar = fpga::FromBinaryString<float>(str.substr(1));
}

}  // namespace custom

namespace {

TEST(StringifyTest, BoolToBinaryString) {
  static_assert(fpga::HasToBinaryString<bool>::value);
  EXPECT_EQ(fpga::ToBinaryString(true), "1");
}

TEST(StringifyTest, BoolFromBinaryString) {
  static_assert(fpga::HasFromBinaryString<bool>::value);
  EXPECT_TRUE(fpga::FromBinaryString<bool>("1"));
  EXPECT_DEATH(fpga::FromBinaryString<bool>(""), "size()");
}

TEST(StringifyTest, FloatToBinaryString) {
  static_assert(!fpga::HasToBinaryString<float>::value);
  EXPECT_EQ(fpga::ToBinaryString(1.f), "00111111100000000000000000000000");
}

TEST(StringifyTest, FloatFromBinaryString) {
  static_assert(!fpga::HasFromBinaryString<float>::value);
  EXPECT_EQ(fpga::FromBinaryString<float>("00111111100000000000000000000000"),
            1.f);
  EXPECT_DEATH(fpga::FromBinaryString<float>("010101"), "size()");
  EXPECT_THROW(
      fpga::FromBinaryString<float>("12345678901234567890123456789012"),
      std::exception);
}

TEST(StringifyTest, CustomStructToBinaryString) {
  static_assert(fpga::HasToBinaryString<custom::Struct>::value);
  const custom::Struct val = {.foo = true, .bar = 1.f};
  EXPECT_EQ(fpga::ToBinaryString(val), "100111111100000000000000000000000");
}

TEST(StringifyTest, CustomStructFromBinaryString) {
  static_assert(fpga::HasFromBinaryString<custom::Struct>::value);
  const custom::Struct val = fpga::FromBinaryString<custom::Struct>(
      "100111111100000000000000000000000");
  EXPECT_TRUE(val.foo);
  EXPECT_EQ(val.bar, 1.f);
}

}  // namespace
