// Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#ifndef TAPA_SCOPED_LOG_SINK_MOCK_H_
#define TAPA_SCOPED_LOG_SINK_MOCK_H_

#include <cstddef>
#include <ctime>

#include <string_view>

#include <glog/logging.h>
#include <gmock/gmock.h>

namespace tapa_testing {

// NOT thread-safe.
class ScopedLogSinkMock : public google::LogSink {
 public:
  ScopedLogSinkMock() { AddLogSink(this); }

  // Not copyable or movable.
  ScopedLogSinkMock(const ScopedLogSinkMock&) = delete;
  ScopedLogSinkMock& operator=(const ScopedLogSinkMock&) = delete;

  ~ScopedLogSinkMock() override { RemoveLogSink(this); }

  MOCK_METHOD(void, Info, (std::string_view message));
  MOCK_METHOD(void, Warning, (std::string_view message));
  MOCK_METHOD(void, Error, (std::string_view message));
  MOCK_METHOD(void, Fatal, (std::string_view message));

 private:
  void send(google::LogSeverity severity, const char* full_filename,
            const char* base_filename, int line, const tm* tm_time,
            const char* message, size_t message_len) override {
    const std::string_view message_view(message, message_len);
    switch (severity) {
      case google::INFO:
        return Info(message_view);
      case google::WARNING:
        return Warning(message_view);
      case google::ERROR:
        return Error(message_view);
      case google::FATAL:
        return Fatal(message_view);
      default:
        FAIL() << "Unexpected glog severity: " << severity;
    }
  }
};

}  // namespace tapa_testing

#endif  // TAPA_SCOPED_LOG_SINK_MOCK_H_
