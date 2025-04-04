// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#ifndef TAPA_BASE_TASK_H_
#define TAPA_BASE_TASK_H_

namespace tapa {

namespace internal {
enum class InvokeMode {
  // Forks new task in background, and joins it before current task finishes.
  kJoin = 0,

  // Forks new task in background without joining.
  kDetach = -1,

  // Runs new task inline, before current invocation completes.
  kSequential = 1,
};
}  // namespace internal

inline constexpr auto join = internal::InvokeMode::kJoin;
inline constexpr auto detach = internal::InvokeMode::kDetach;

/// Class that generates a sequence of integers as task arguments.
///
/// Canonical usage:
/// @code{.cpp}
///  void TaskFoo(int i, ...) {
///    ...
///  }
///  tapa::task()
///    .invoke<3>(TaskFoo, tapa::seq(), ...)
///    ...
///    ;
/// @endcode
///
/// @c TaskFoo will be invoked three times, receiving @c 0, @c 1, and @c 2 as
/// the first argument, respectively.
struct seq {
  /// Constructs a @c tapa::seq. This is the only public API.
  seq() = default;

  seq(const seq&) = delete;
  seq(seq&&) = delete;
  seq& operator=(const seq&) = delete;
  seq& operator=(seq&&) = delete;
  int pos = 0;
};

}  // namespace tapa

#endif  // TAPA_BASE_TASK_H_
