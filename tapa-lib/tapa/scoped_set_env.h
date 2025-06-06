// Copyright (c) 2025 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#ifndef TAPA_SCOPED_SET_ENV_H_
#define TAPA_SCOPED_SET_ENV_H_

#include <cstdlib>

#include <string>
#include <string_view>

#include <string.h>

#include <glog/logging.h>

namespace tapa_testing {

// NOT thread-safe.
class ScopedSetEnv {
 public:
  explicit ScopedSetEnv(std::string_view name, const char* value)
      : name_(name) {
    if (const char* old_value = getenv(name_.c_str()); old_value != nullptr) {
      old_value_ = strdup(old_value);
    }
    SetValue(value);
  }

  // Not copyable or movable.
  ScopedSetEnv(const ScopedSetEnv&) = delete;
  ScopedSetEnv& operator=(const ScopedSetEnv&) = delete;

  ~ScopedSetEnv() {
    SetValue(old_value_);
    free(old_value_);
  }

 private:
  void SetValue(const char* value) {
    if (value == nullptr) {
      PLOG_IF(ERROR, unsetenv(name_.c_str())) << "unsetenv";
    } else {
      PLOG_IF(ERROR, setenv(name_.c_str(), value, /*replace=*/1)) << "setenv";
    }
  }
  const std::string name_;
  char* old_value_ = nullptr;
};

}  // namespace tapa_testing

#endif  // TAPA_SCOPED_SET_ENV_H_
