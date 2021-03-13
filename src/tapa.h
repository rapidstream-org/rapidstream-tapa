#ifndef TASK_LEVEL_PARALLELIZATION_H_
#define TASK_LEVEL_PARALLELIZATION_H_

#ifdef __SYNTHESIS__
#error this header is not synthesizable
#endif  // __SYNTHESIS__

#include <climits>
#include <cstdarg>
#include <cstdint>
#include <cstdlib>

#include <functional>
#include <iostream>
#include <iterator>
#include <list>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include <frt.h>
#include <glog/logging.h>

#include "tapa/mmap.h"
#include "tapa/stream.h"
#include "tapa/synthesizable/traits.h"
#include "tapa/synthesizable/util.h"
#include "tapa/synthesizable/vec.h"

namespace tapa {

struct seq {};

struct task {
  task();
  task(task&&) = delete;
  task(const task&) = delete;
  ~task();

  task& operator=(task&&) = delete;
  task& operator=(const task&) = delete;

  template <int step, typename Function, typename... Args>
  task& invoke(Function&& f, Args&&... args) {
    return invoke<step>(f, "", args...);
  }

  template <int step, typename Function, typename... Args, size_t S>
  task& invoke(Function&& f, const char (&name)[S], Args&&... args) {
    schedule(/* detach= */ step < 0, std::bind(f, std::forward<Args>(args)...));
    return *this;
  }

  // invoke task vector without a name
  template <int step, uint64_t length, typename Function, typename... Args>
  task& invoke(Function&& f, Args&&... args) {
    return invoke<step, length>(f, "", args...);
  }

  // invoke task vector with a name
  template <int step, uint64_t length, typename Function, typename... Args,
            size_t S>
  task& invoke(Function&& f, const char (&name)[S], Args&&... args) {
    for (uint64_t i = 0; i < length; ++i) {
      this->invoke<step>(f, access(args, i)...);
    }
    return *this;
  }

 private:
  // scalar
  template <typename T>
  static T& access(T& arg, uint64_t idx) {
    return arg;
  }

  // sequence
  static int access(seq, uint64_t idx) { return idx; }

  // access streams in vector invoke
  template <typename T, uint64_t length, uint64_t depth>
  static stream<T, depth>& access(streams<T, length, depth>& arg,
                                  uint64_t idx) {
    LOG_IF(INFO, idx >= length) << "invocation #" << idx << " accesses "
                                << "stream #" << idx % length;
    return arg[idx];
  }

  // access async_mmaps in vector invoke
  template <typename T, uint64_t length>
  static async_mmap<T>& access(async_mmaps<T, length>& arg, uint64_t idx) {
    LOG_IF(INFO, idx >= length) << "invocation #" << idx << " accesses "
                                << "async_mmap #" << idx % length;
    return arg[idx % length];
  }

  void schedule(bool detach, const std::function<void()>&);
};

template <typename T, uint64_t N>
inline std::ostream& operator<<(std::ostream& os, const vec_t<T, N>& obj) {
  os << "{";
  for (uint64_t i = 0; i < N; ++i) {
    if (i > 0) os << ", ";
    os << "[" << i << "]: " << obj[i];
  }
  return os << "}";
}

namespace internal {

void* allocate(size_t length);
void deallocate(void* addr, size_t length);

// functions cannot be specialized so use classes
template <typename T>
struct dispatcher {
  static T&& forward(typename std::remove_reference<T>::type& arg) {
    return static_cast<T&&>(arg);
  }
};
template <typename T>
struct dispatcher<placeholder_mmap<T>> {
  static fpga::PlaceholderBuffer<T> forward(placeholder_mmap<T> arg) {
    return fpga::Placeholder(arg.get(), arg.size());
  }
};
// read/write are with respect to the kernel in tapa but host in frt
template <typename T>
struct dispatcher<read_only_mmap<T>> {
  static fpga::WriteOnlyBuffer<T> forward(read_only_mmap<T> arg) {
    return fpga::WriteOnly(arg.get(), arg.size());
  }
};
template <typename T>
struct dispatcher<write_only_mmap<T>> {
  static fpga::ReadOnlyBuffer<T> forward(write_only_mmap<T> arg) {
    return fpga::ReadOnly(arg.get(), arg.size());
  }
};
template <typename T>
struct dispatcher<read_write_mmap<T>> {
  static fpga::ReadWriteBuffer<T> forward(read_write_mmap<T> arg) {
    return fpga::ReadWrite(arg.get(), arg.size());
  }
};
// TODO: dispatch stream correctly

}  // namespace internal

// host-only invoke that takes path to a bistream file as an argument
template <typename Func, typename... Args>
inline void invoke(Func&& f, const std::string& bitstream, Args&&... args) {
  if (bitstream.empty()) {
    f(std::forward<Args>(args)...);
  } else {
    fpga::Invoke(bitstream, internal::dispatcher<Args>::forward(args)...);
  }
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

}  // namespace tapa

#endif  // TASK_LEVEL_PARALLELIZATION_H_
