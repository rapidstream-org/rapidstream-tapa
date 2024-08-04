#include "tapa/host/stream.h"

#include <cstring>
#include <ostream>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include <tapa/host/internal_util.h>

#ifdef __cpp_lib_filesystem
#include <filesystem>
namespace fs = std::filesystem;
#else
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#endif

namespace tapa {
namespace {

using ::tapa::internal::StrCat;

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

}  // namespace
}  // namespace tapa
