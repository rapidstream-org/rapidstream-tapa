// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#include "tapa/host/stream.h"

#include <cstring>
#include <ostream>
#include <string>
#include <string_view>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <tapa/host/private_util.h>
#include <tapa/scoped_log_sink_mock.h>
#include <tapa/scoped_set_env.h>

#ifdef __cpp_lib_filesystem
#include <filesystem>
namespace fs = std::filesystem;
#else
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#endif

namespace tapa {
namespace {

using namespace std::string_view_literals;

using ::tapa::internal::StrCat;
using ::tapa_testing::ScopedLogSinkMock;
using ::tapa_testing::ScopedSetEnv;
using ::testing::HasSubstr;
using ::testing::InSequence;
using ::testing::NiceMock;

constexpr char kEnvVarName[] = "TAPA_STREAM_LOG_DIR";

class StreamLogTest : public testing::Test {
 protected:
  void SetUp() override {
    fs::create_directory(temp_dir_);
    ASSERT_TRUE(fs::is_directory(temp_dir_));
    ASSERT_EQ(setenv(kEnvVarName, temp_dir_.c_str(), /*replace=*/1), 0)
        << std::strerror(errno);
  }

  void TearDown() override {
    EXPECT_EQ(unsetenv(kEnvVarName), 0) << std::strerror(errno);
    fs::remove_all(temp_dir_);
  }

  std::string GetLogContent(std::string_view name) {
    std::ifstream ifs(temp_dir_ / StrCat({name, ".txt"}));
    return std::string((std::istreambuf_iterator<char>(ifs)),
                       std::istreambuf_iterator<char>());
  }

  const testing::TestInfo* const test_info_ =
      testing::UnitTest::GetInstance()->current_test_info();
  const fs::path temp_dir_ =
      fs::temp_directory_path() /
      StrCat({test_info_->test_suite_name(), ".", test_info_->name()});
};

// Primitive types are logged in text format.
TEST_F(StreamLogTest, LoggingPrimitiveTypeSucceeds) {
  tapa::stream<int> data_q("data");
  data_q.write(2333);
  data_q.read();

  EXPECT_EQ(GetLogContent(data_q.get_name()), "2333\n");
}

// If a type does not overload `operator<<`, values are logged in hex format.
struct Foo {
  int foo;
};
TEST_F(StreamLogTest, LoggingCustomTypeInHexFormatSucceeds) {
  tapa::stream<Foo> data_q("data");
  data_q.write(Foo{0x2333});
  data_q.read();

  EXPECT_EQ(GetLogContent(data_q.get_name()), "0x33230000\n");  // Little-endian
}

// If the value type overloads `operator<<`, values are logged in text format.
struct Bar {
  int bar;
};
std::ostream& operator<<(std::ostream& os, const Bar& bar) {
  return os << bar.bar;
}
TEST_F(StreamLogTest, LoggingCustomTypeInTextFormatSucceeds) {
  tapa::stream<Bar> data_q("data");
  data_q.write(Bar{2333});
  data_q.read();

  EXPECT_EQ(GetLogContent(data_q.get_name()), "2333\n");
}

TEST_F(StreamLogTest, LoggingEotSucceeds) {
  tapa::stream<int> data_q("data");
  data_q.write(233);
  data_q.read();
  data_q.close();
  data_q.open();
  data_q.write(2333);
  data_q.read();

  EXPECT_EQ(GetLogContent(data_q.get_name()), "233\n\n2333\n");
}

TEST_F(StreamLogTest, LoggingToNonexistentDirFails) {
  ASSERT_EQ(setenv(kEnvVarName, "/nonexistent", /*replace=*/1), 0)
      << std::strerror(errno);

  tapa::stream<int> data_q("data");
  data_q.write(233);
  data_q.read();

  EXPECT_EQ(GetLogContent(data_q.get_name()), "");
}

TEST(StringifyTest, TapaInternalElemToBinaryString) {
  const internal::elem_t<float> val = {.val = 1.f, .eot = true};
  EXPECT_EQ(fpga::internal::ToBinaryString(val), "\0\0\x80?\x1"sv);
}

TEST(StringifyTest, TapaInternalElemFromBinaryString) {
  const internal::elem_t<float> val =
      fpga::internal::FromBinaryString<internal::elem_t<float>>(
          "\0\0\x80?\x1"sv);
  EXPECT_TRUE(val.eot);
  EXPECT_EQ(val.val, 1.f);
}

TEST(StreamNameTest, StreamsHaveIndexedNames) {
  tapa::streams<int, 3> data_q("data");

  EXPECT_EQ(data_q[0].get_name(), "data[0]");
  EXPECT_EQ(data_q[1].get_name(), "data[1]");
  EXPECT_EQ(data_q[2].get_name(), "data[2]");
}

TEST(LeftoverLogTest, SingleLeftoverIsReported) {
  NiceMock<ScopedLogSinkMock> log;

  tapa::stream<int> stream("foo");
  stream.write(2333);

  InSequence seq;
  EXPECT_CALL(log, Warning(HasSubstr("'foo' destructed with leftovers")));
  EXPECT_CALL(log, Warning(HasSubstr("'foo' leftover: 2333")));
}

TEST(LeftoverLogTest, ExcessiveLeftoverIsOmitted) {
  NiceMock<ScopedLogSinkMock> log;
  ScopedSetEnv env("TAPA_STREAM_LEFTOVER_LOG_COUNT", "3");

  tapa::stream<int, 10> stream("foo");
  for (int i = 0; i < 10; ++i) {
    stream.write(i);
  }

  InSequence seq;
  EXPECT_CALL(log, Warning(HasSubstr("'foo' destructed with leftovers")));
  EXPECT_CALL(log, Warning(HasSubstr("'foo' leftover: "))).Times(3);
  EXPECT_CALL(
      log, Warning("Set TAPA_STREAM_LEFTOVER_LOG_COUNT to log more leftovers"));
}

TEST(LeftoverLogTest, InvalidLeftoverLogCountIsReported) {
  NiceMock<ScopedLogSinkMock> log;
  ScopedSetEnv env("TAPA_STREAM_LEFTOVER_LOG_COUNT", "x");

  tapa::stream<int> stream("foo");
  stream.write(2333);

  EXPECT_CALL(log, Error("Invalid TAPA_STREAM_LEFTOVER_LOG_COUNT value: 'x'"));
}

}  // namespace
}  // namespace tapa
