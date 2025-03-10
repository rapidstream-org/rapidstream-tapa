// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#pragma once

#include "tapa/base/task.h"

namespace tapa {

struct executable {
  explicit executable(std::string path);
};

struct task {
  template <typename Func, typename... Args>
  task& invoke(Func&& func, Args&&... args);
  template <int mode, typename Func, typename... Args>
  task& invoke(Func&& func, Args&&... args);
  template <typename Func, typename... Args, size_t name_size>
  task& invoke(Func&& func, const char (&name)[name_size], Args&&... args);
  template <int mode, typename Func, typename... Args, size_t name_size>
  task& invoke(Func&& func, const char (&name)[name_size], Args&&... args);
  template <int mode, int n, typename Func, typename... Args>
  task& invoke(Func&& func, Args&&... args);
  template <int mode, int n, typename Func, typename... Args, size_t name_size>
  task& invoke(Func&& func, const char (&name)[name_size], Args&&... args);
  template <typename Func, typename... Args>
  task& invoke(Func&& func, executable exe, Args&&... args);
};

}  // namespace tapa
