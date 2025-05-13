// Copyright (c) 2024 RapidStream Design Automation, Inc. and contributors.
// All rights reserved. The contributor(s) of this file has/have agreed to the
// RapidStream Contributor License Agreement.

#ifndef TAPA_HOST_TASK_H_
#define TAPA_HOST_TASK_H_

#include "tapa/base/task.h"

#include "tapa/host/coroutine.h"
#include "tapa/host/internal_util.h"
#include "tapa/host/logging.h"

#include <sys/wait.h>
#include <unistd.h>
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <tuple>
#include <type_traits>
#include <utility>

#include <frt.h>

namespace tapa {

namespace internal {

template <typename Param, typename Arg>
struct accessor {
  static Param access(Arg&& arg) { return arg; }
  static void access(fpga::Instance& instance, int& idx, Arg&& arg) {
    instance.SetArg(idx++, static_cast<Param>(arg));
  }
};

template <typename T>
struct accessor<T, seq> {
  static T access(seq&& arg) { return arg.pos++; }
  static void access(fpga::Instance& instance, int& idx, seq&& arg) {
    instance.SetArg(idx++, static_cast<T>(arg.pos++));
  }
};

void* allocate(size_t length);
void deallocate(void* addr, size_t length);

// std::bind wrapper with arguments evaluated from left to right.
struct binder {
  template <typename F, typename... Args>
  binder(F&& f, Args&&... args)
      : result(std::bind(std::forward<F>(f), std::forward<Args>(args)...)) {}
  std::function<void()> result;
};

template <typename F>
struct invoker {
  using FuncType = std::decay_t<F>;
  using Params = typename function_traits<FuncType>::params;

  static_assert(
      std::is_same_v<void, typename function_traits<FuncType>::return_type>,
      "task function must return void");

  template <typename... Args>
  static void invoke(InvokeMode mode, F&& f, Args&&... args) {
    // Create a functor that captures args by value
    auto functor = invoker::functor_with_accessors(
        std::forward<F>(f), std::index_sequence_for<Args...>{},
        std::forward<Args>(args)...);
    if (mode == InvokeMode::kSequential) {  // Sequential scheduling.
      std::move(functor)();
    } else {
      schedule(mode == InvokeMode::kDetach, std::move(functor));
    }
  }

  template <typename... Args>
  static int64_t invoke(bool run_in_new_process, F&& f,
                        const std::string& bitstream, Args&&... args) {
    if (bitstream.empty()) {
      LOG(INFO) << "running software simulation with TAPA library";
      const auto tic = std::chrono::steady_clock::now();
      f(std::forward<Args>(args)...);
      const auto toc = std::chrono::steady_clock::now();
      return std::chrono::duration_cast<std::chrono::nanoseconds>(toc - tic)
          .count();
    } else {
      if (run_in_new_process) {
        auto kernel_time_ns_raw = allocate(sizeof(int64_t));
        auto deleter = [](int64_t* p) { deallocate(p, sizeof(int64_t)); };
        std::unique_ptr<int64_t, decltype(deleter)> kernel_time_ns(
            reinterpret_cast<int64_t*>(kernel_time_ns_raw), deleter);
        if (pid_t pid = fork()) {
          // Parent.
          PCHECK(pid != -1);
          int status = 0;
          CHECK_EQ(wait(&status), pid);
          CHECK(WIFEXITED(status));
          CHECK_EQ(WEXITSTATUS(status), EXIT_SUCCESS);
          return *kernel_time_ns;
        }

        // Child.
        *kernel_time_ns = invoke(f, bitstream, std::forward<Args>(args)...);
        exit(EXIT_SUCCESS);
      } else {
        return invoke(f, bitstream, std::forward<Args>(args)...);
      }
    }
  }

  template <size_t... Is, typename... CapturedArgs>
  static void set_fpga_args(fpga::Instance& instance,
                            std::index_sequence<Is...>,
                            CapturedArgs&&... args) {
    int idx = 0;
    int _[] = {
        (accessor<std::tuple_element_t<Is, Params>, CapturedArgs>::access(
             instance, idx, std::forward<CapturedArgs>(args)),
         0)...};
  }

 private:
  template <typename... Args>
  static int64_t invoke(F&& f, const std::string& bitstream, Args&&... args) {
    auto instance = fpga::Instance(bitstream);
    set_fpga_args(instance, std::index_sequence_for<Args...>{},
                  std::forward<Args>(args)...);
    instance.WriteToDevice();
    instance.Exec();
    instance.ReadFromDevice();
    instance.Finish();
    return instance.ComputeTimeNanoSeconds();
  }

  template <typename Func, size_t... Is, typename... CapturedArgs>
  static auto functor_with_accessors(Func&& func, std::index_sequence<Is...>,
                                     CapturedArgs&&... args) {
    // std::bind creates a copy of args
    // Aggregate initialization evaluates args from left to right.
    return binder{
        func, accessor<std::tuple_element_t<Is, Params>, CapturedArgs>::access(
                  std::forward<CapturedArgs>(args))...}
        .result;
  }
};

}  // namespace internal

/// Enables overriding the executable target for `tapa::task::invoke`.
class executable {
 public:
  explicit executable(std::string path) : path_(std::move(path)) {}

  // Not copyable or movable.
  executable(const executable& other) = delete;
  executable& operator=(const executable& other) = delete;

 private:
  friend struct task;
  const std::string path_;
};

/// Defines a parent task instantiating children task instances.
///
/// Canonical usage:
/// @code{.cpp}
///  tapa::task()
///    .invoke(...)
///    .invoke(...)
///    ...
///    ;
/// @endcode
///
/// A parent task itself does not do any computation. By default, a parent task
/// will not finish until all its children task instances finish. Such children
/// task instances are @a joined to their parent. The alternative is to @a
/// detach a child from the parent. If a child task instance is instantiated and
/// detached, the parent will no longer wait for the child task to finish.
/// Detached tasks are very useful when infinite loops can be used.
struct task {
  /// Constructs a @c tapa::task.
  explicit task();

  ~task();

  // Not copyable or movable.
  task(const task&) = delete;
  task& operator=(const task&) = delete;

  /// Invokes a task and instantiates a child task instance.
  ///
  /// @param func Task function definition of the instantiated child.
  /// @param args Arguments passed to @c func.
  /// @return     Reference to the caller @c tapa::task.
  template <typename Func, typename... Args>
  task& invoke(Func&& func, Args&&... args) {
    return invoke<join>(std::forward<Func>(func), "",
                        std::forward<Args>(args)...);
  }

  /// Invokes a task and instantiates a child task instance with the given
  /// instatiation mode.
  ///
  /// @tparam mode Instatiation mode (@c join or @c detach).
  /// @param func  Task function definition of the instantiated child.
  /// @param args  Arguments passed to @c func.
  /// @return      Reference to the caller @c tapa::task.
  template <internal::InvokeMode mode, typename Func, typename... Args>
  task& invoke(Func&& func, Args&&... args) {
    return invoke<mode>(std::forward<Func>(func), "",
                        std::forward<Args>(args)...);
  }

  template <typename Func, typename... Args, size_t name_size>
  task& invoke(Func&& func, const char (&name)[name_size], Args&&... args) {
    return invoke<join>(std::forward<Func>(func), name,
                        std::forward<Args>(args)...);
  }

  template <internal::InvokeMode mode, typename Func, typename... Args,
            size_t name_size>
  task& invoke(Func&& func, const char (&name)[name_size], Args&&... args) {
    static_assert(
        internal::is_callable_v<typename std::remove_reference_t<Func>>,
        "the first argument for tapa::task::invoke() must be callable");
    internal::invoker<Func>::template invoke<Args...>(
        mode_override.value_or(mode), std::forward<Func>(func),
        std::forward<Args>(args)...);
    return *this;
  }

  /// Host-only invoke that takes an @c executable as an argument.
  ///
  /// @param func Task function definition of the instantiated child.
  /// @param exe  Optionally overrides the execution target.
  /// @param args Arguments passed to @c func.
  /// @return     Reference to the caller @c tapa::task.
  template <typename Func, typename... Args>
  task& invoke(Func&& func, executable exe, Args&&... args) {
    if (exe.path_.empty()) {
      return invoke(std::forward<Func>(func), std::forward<Args>(args)...);
    }

    auto instance = std::make_shared<fpga::Instance>(exe.path_);
    internal::schedule_cleanup([instance]() { instance->Kill(); });
    internal::schedule(
        /*detach=*/false, [=]() mutable {
          // captured FPGA arguments by value
          internal::invoker<Func>::set_fpga_args(
              *instance, std::index_sequence_for<Args...>{}, args...);
          invoke_frt(std::move(instance));
        });

    return *this;
  }

  /// Invokes a task @c n times and instantiates @c n child task
  /// instances with the given instatiation mode.
  ///
  /// @tparam mode Instatiation mode (@c join or @c detach).
  /// @tparam n    Instatiation count.
  /// @param func  Task function definition of the instantiated child.
  /// @param args  Arguments passed to @c func.
  /// @return      Reference to the caller @c tapa::task.
  template <internal::InvokeMode mode, int n, typename Func, typename... Args>
  task& invoke(Func&& func, Args&&... args) {
    return invoke<mode, n>(std::forward<Func>(func), "",
                           std::forward<Args>(args)...);
  }

  template <internal::InvokeMode mode, int n, typename Func, typename... Args,
            size_t name_size>
  task& invoke(Func&& func, const char (&name)[name_size], Args&&... args) {
    for (int i = 0; i < n; ++i) {
      invoke<mode>(std::forward<Func>(func), std::forward<Args>(args)...);
    }
    return *this;
  }

 protected:
  std::optional<internal::InvokeMode> mode_override;

 private:
  void invoke_frt(std::shared_ptr<fpga::Instance> instance);
};

}  // namespace tapa

#endif  // TAPA_HOST_TASK_H_
