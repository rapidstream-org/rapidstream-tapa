#ifndef TASK_LEVEL_PARALLELIZATION_H_
#define TASK_LEVEL_PARALLELIZATION_H_

#include <climits>
#include <cstdarg>
#include <cstdint>
#include <cstdlib>

#ifdef __SYNTHESIS__

namespace tapa {
namespace internal {

struct dummy {
  template <typename T>
  dummy& operator<<(const T&) {
    return *this;
  }
};

}  // namespace internal
}  // namespace tapa

#define LOG(level) ::tapa::internal::dummy()
#define LOG_IF(level, cond) ::tapa::internal::dummy()
#define LOG_EVERY_N(level, n) ::tapa::internal::dummy()
#define LOG_IF_EVERY_N(level, cond, n) ::tapa::internal::dummy()
#define LOG_FIRST_N(level, n) ::tapa::internal::dummy()

#define DLOG(level) ::tapa::internal::dummy()
#define DLOG_IF(level, cond) ::tapa::internal::dummy()
#define DLOG_EVERY_N(level, n) ::tapa::internal::dummy()

#define CHECK(cond) ::tapa::internal::dummy()
#define CHECK_NE(lhs, rhs) ::tapa::internal::dummy()
#define CHECK_EQ(lhs, rhs) ::tapa::internal::dummy()
#define CHECK_GE(lhs, rhs) ::tapa::internal::dummy()
#define CHECK_GT(lhs, rhs) ::tapa::internal::dummy()
#define CHECK_LE(lhs, rhs) ::tapa::internal::dummy()
#define CHECK_LT(lhs, rhs) ::tapa::internal::dummy()
#define CHECK_NOTNULL(ptr) (ptr)
#define CHECK_STREQ(lhs, rhs) ::tapa::internal::dummy()
#define CHECK_STRNE(lhs, rhs) ::tapa::internal::dummy()
#define CHECK_STRCASEEQ(lhs, rhs) ::tapa::internal::dummy()
#define CHECK_STRCASENE(lhs, rhs) ::tapa::internal::dummy()
#define CHECK_DOUBLE_EQ(lhs, rhs) ::tapa::internal::dummy()

#define VLOG_IS_ON(level) false
#define VLOG(level) ::tapa::internal::dummy()
#define VLOG_IF(level, cond) ::tapa::internal::dummy()
#define VLOG_EVERY_N(level, n) ::tapa::internal::dummy()
#define VLOG_IF_EVERY_N(level, cond, n) ::tapa::internal::dummy()
#define VLOG_FIRST_N(level, n) ::tapa::internal::dummy()
#else  // __SYNTHESIS__

#include <chrono>
#include <functional>
#include <iostream>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <sys/types.h>
#include <sys/wait.h>

#include <frt.h>
#include <glog/logging.h>

#endif  // __SYNTHESIS__

#include "tapa/mmap.h"
#include "tapa/stream.h"
#include "tapa/traits.h"
#include "tapa/util.h"
#include "tapa/vec.h"

namespace tapa {

#ifndef __SYNTHESIS__

namespace internal {

void* allocate(size_t length);
void deallocate(void* addr, size_t length);

template <typename T>
struct invoker;

template <typename... Params>
struct invoker<void (&)(Params...)> {
  template <typename... Args>
  static void invoke(bool detach, void (&f)(Params...), Args&&... args) {
    // std::bind creates a copy of args
    internal::schedule(detach, std::bind(f, accessor<Params, Args>::access(
                                                std::forward<Args>(args))...));
  }

  template <typename... Args>
  static int64_t invoke(bool run_in_new_process, void (&f)(Params...),
                        const std::string& bitstream, Args&&... args) {
    if (bitstream.empty()) {
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

 private:
  template <typename... Args>
  static int64_t invoke(void (&f)(Params...), const std::string& bitstream,
                        Args&&... args) {
    auto instance = fpga::Instance(bitstream);
    int idx = 0;
    int _[] = {(
        accessor<Params, Args>::access(instance, idx, std::forward<Args>(args)),
        0)...};
    instance.WriteToDevice();
    instance.Exec();
    instance.ReadFromDevice();
    instance.Finish();
    return instance.ComputeTimeNanoSeconds();
  }
};

}  // namespace internal

#endif  // __SYNTHESIS__

struct task {
#ifndef __SYNTHESIS__

  task();
  task(task&&) = delete;
  task(const task&) = delete;
  ~task();

  task& operator=(task&&) = delete;
  task& operator=(const task&) = delete;

#endif  // __SYNTHESIS__

  template <typename Func, typename... Args>
  task& invoke(Func&& f, Args&&... args) {
    return invoke<join>(std::forward<Func>(f), "", std::forward<Args>(args)...);
  }

  template <int step, typename Func, typename... Args>
  task& invoke(Func&& f, Args&&... args) {
    return invoke<step>(std::forward<Func>(f), "", std::forward<Args>(args)...);
  }

  template <typename Func, typename... Args, size_t name_size>
  task& invoke(Func&& f, const char (&name)[name_size], Args&&... args) {
    return invoke<join>(std::forward<Func>(f), name,
                        std::forward<Args>(args)...);
  }

  template <int step, typename Func, typename... Args, size_t name_size>
  task& invoke(Func&& f, const char (&name)[name_size], Args&&... args) {
#ifdef __SYNTHESIS__
    f(std::forward<Args>(args)...);
#else   // __SYNTHESIS__
    internal::invoker<Func>::template invoke<Args...>(
        /* detach= */ step < 0, std::forward<Func>(f),
        std::forward<Args>(args)...);
#endif  // __SYNTHESIS__
    return *this;
  }

  // invoke task vector without a name
  template <int step, uint64_t length, typename Func, typename... Args>
  task& invoke(Func&& f, Args&&... args) {
    return invoke<step, length>(std::forward<Func>(f), "",
                                std::forward<Args>(args)...);
  }

  // invoke task vector with a name
  template <int step, uint64_t length, typename Func, typename... Args,
            size_t name_size>
  task& invoke(Func&& f, const char (&name)[name_size], Args&&... args) {
    for (int i = 0; i < length; ++i) {
      invoke<step>(std::forward<Func>(f), std::forward<Args>(args)...);
    }
    return *this;
  }
};

struct seq {
  int pos = 0;
};

#ifndef __SYNTHESIS__

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

}  // namespace internal

// Host-only invoke that takes path to a bistream file as an argument. Returns
// the kernel time in nanoseconds.
template <typename Func, typename... Args>
inline int64_t invoke(Func&& f, const std::string& bitstream, Args&&... args) {
  return internal::invoker<Func>::template invoke<Args...>(
      /*run_in_new_process*/ false, std::forward<Func>(f), bitstream,
      std::forward<Args>(args)...);
}

// Workaround for the fact that Xilinx's cosim cannot run for more than once in
// each process. The mmap pointers MUST be allocated via mmap, or the updates
// won't be seen by the caller process!
template <typename Func, typename... Args>
inline int64_t invoke_in_new_process(Func&& f, const std::string& bitstream,
                                     Args&&... args) {
  return internal::invoker<Func>::template invoke<Args...>(
      /*run_in_new_process*/ true, std::forward<Func>(f), bitstream,
      std::forward<Args>(args)...);
}

template <typename T>
struct aligned_allocator {
  using value_type = T;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  T* allocate(size_t count) {
    return reinterpret_cast<T*>(internal::allocate(count * sizeof(T)));
  }
  void deallocate(T* ptr, std::size_t count) {
    internal::deallocate(ptr, count * sizeof(T));
  }
};

#endif  // __SYNTHESIS__

}  // namespace tapa

#endif  // TASK_LEVEL_PARALLELIZATION_H_
