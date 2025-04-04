// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#ifndef TAPA_XILINX_HLS_TASK_H_
#define TAPA_XILINX_HLS_TASK_H_

#include <string>
#include <utility>

#include "tapa/base/task.h"

namespace tapa {

struct executable {
  explicit executable(std::string path);
};

struct task {
  template <typename Func, typename... Args>
  task& invoke(Func&& func, Args&&... args) {
    return invoke<join>(std::forward<Func>(func), "",
                        std::forward<Args>(args)...);
  }

  template <int mode, typename Func, typename... Args>
  task& invoke(Func&& func, Args&&... args) {
    return invoke<mode>(std::forward<Func>(func), "",
                        std::forward<Args>(args)...);
  }

  template <typename Func, typename... Args, size_t name_size>
  task& invoke(Func&& func, const char (&name)[name_size], Args&&... args) {
    return invoke<join>(std::forward<Func>(func), name,
                        std::forward<Args>(args)...);
  }

  template <int mode, typename Func, typename... Args, size_t name_size>
  task& invoke(Func&& func, const char (&name)[name_size], Args&&... args) {
    func(std::forward<Args>(args)...);
    return *this;
  }

  template <int mode, int n, typename Func, typename... Args>
  task& invoke(Func&& func, Args&&... args) {
    return invoke<mode, n>(std::forward<Func>(func), "",
                           std::forward<Args>(args)...);
  }

  template <int mode, int n, typename Func, typename... Args, size_t name_size>
  task& invoke(Func&& func, const char (&name)[name_size], Args&&... args) {
    for (int i = 0; i < n; ++i) {
      invoke<mode>(std::forward<Func>(func), std::forward<Args>(args)...);
    }
    return *this;
  }

  template <typename Func, typename... Args>
  task& invoke(Func&& func, executable exe, Args&&... args) {
    return invoke<join>(std::forward<Func>(func), "",
                        std::forward<Args>(args)...);
  }
};

}  // namespace tapa
#endif  // TAPA_XILINX_HLS_TASK_H
